#include "ReferencePlayer.h"

#include <cmath>

namespace ml
{
ReferencePlayer::ReferencePlayer()
    : juce::Thread ("MasterLens reference player")
{
}

ReferencePlayer::~ReferencePlayer()
{
    signalThreadShouldExit();
    notify();
    stopThread (-1);
}

void ReferencePlayer::load (const juce::StringArray& files, double sampleRate)
{
    const juce::SpinLock::ScopedLockType command (lock);
    pendingPath.clear();

    for (const auto& f : files)
        if (juce::File (f).existsAsFile())
        {
            pendingPath = f;
            break;
        }

    targetRate.store (sampleRate > 0.0 ? sampleRate : 48000.0, std::memory_order_relaxed);

    {
        status = pendingPath.isEmpty() ? "empty" : "loading";
        fileName.clear();


    }

    ready.store (false, std::memory_order_release);
    busy.store (! pendingPath.isEmpty(), std::memory_order_release);

    enabled.store (false);
    request.fetch_add (1);
    if (! isThreadRunning()) startThread();
    notify();
}

void ReferencePlayer::clear()
{
    load ({}, targetRate.load());
    enabled.store (false);
    duration.store (0.0);
    offsetSeconds.store (0.0);
    loudnessLufs.store (kSilentDb);
}

juce::String ReferencePlayer::getStatus() const
{
    const juce::SpinLock::ScopedLockType sl (lock);
    return status;
}

juce::String ReferencePlayer::getFileName() const
{
    const juce::SpinLock::ScopedLockType sl (lock);
    return fileName;
}

bool ReferencePlayer::render (juce::AudioBuffer<float>& output, double hostSeconds,
                              bool hostPlaying, float extraGainDb)
{
    if (! enabled.load (std::memory_order_acquire))
        return false;

    output.clear();

    const auto d = data.acquire();

    if (! d || ! hostPlaying || output.getNumSamples() <= 0)
        return true;

    const auto& src = d->audio;
    const int sourceChannels = src.getNumChannels();

    if (sourceChannels <= 0 || d->length <= 1)
        return true;

    const double ratio = d->sourceRate / juce::jmax (1.0, targetRate.load (std::memory_order_relaxed));
    double pos = (hostSeconds + offsetSeconds.load (std::memory_order_relaxed)) * d->sourceRate;
    const float gain = juce::Decibels::decibelsToGain (extraGainDb, -60.0f);
    const auto length = (int) d->length;

    // Catmull-Rom (4-point cubic) keeps 44.1 / 48 / 88.2 / 96 kHz references
    // musical; linear interpolation was audible on highs at 2:1 ratios.
    const auto sampleCubic = [&src, length] (int ch, double p)
    {
        const int i1 = juce::jlimit (0, length - 1, (int) std::floor (p));
        const int i0 = juce::jmax (0, i1 - 1);
        const int i2 = juce::jmin (length - 1, i1 + 1);
        const int i3 = juce::jmin (length - 1, i1 + 2);
        const float t = (float) (p - (double) i1);
        const float p0 = src.getSample (ch, i0);
        const float p1 = src.getSample (ch, i1);
        const float p2 = src.getSample (ch, i2);
        const float p3 = src.getSample (ch, i3);
        const float a = -0.5f * p0 + 1.5f * p1 - 1.5f * p2 + 0.5f * p3;
        const float b = p0 - 2.5f * p1 + 2.0f * p2 - 0.5f * p3;
        const float c = -0.5f * p0 + 0.5f * p2;
        return ((a * t + b) * t + c) * t + p1;
    };

    for (int i = 0; i < output.getNumSamples(); ++i)
    {
        const auto index = (juce::int64) std::floor (pos);

        if (index < 0 || index >= d->length)
        {
            pos += ratio;
            continue;
        }

        const float l = sampleCubic (0, pos);
        const float r = sourceChannels > 1 ? sampleCubic (1, pos) : l;

        if (output.getNumChannels() == 1)
            output.setSample (0, i, 0.5f * (l + r) * gain);
        else
        {
            output.setSample (0, i, l * gain);
            output.setSample (1, i, r * gain);
        }

        pos += ratio;
    }

    for (int ch = 2; ch < output.getNumChannels(); ++ch)
        output.clear (ch, 0, output.getNumSamples());

    return true;
}

void ReferencePlayer::run()
{
    unsigned handled = 0;
    while (! threadShouldExit())
    {
        data.collect();
        const auto serial = request.load();
        if (serial == handled) { wait (25); continue; }
        handled = serial;
        juce::String path;
        { const juce::SpinLock::ScopedLockType sl (lock); path = pendingPath; }
        if (path.isEmpty()) { data.publish ({}); continue; }
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        const juce::File file (path);
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
        const auto fail = [&] ()
        {
            const juce::SpinLock::ScopedLockType sl (lock);
            if (request.load() == serial) { status = "failed"; ready.store (false); busy.store (false); }
        };
        if (! reader || reader->lengthInSamples <= 1 || reader->numChannels > 2
            || reader->lengthInSamples > (juce::int64) (reader->sampleRate * 3600.0))
        { fail(); continue; }
        auto loaded = std::make_unique<Data>();
        loaded->sourceRate = reader->sampleRate;
        loaded->length = reader->lengthInSamples;
        loaded->audio.setSize ((int) reader->numChannels, (int) loaded->length);
        AnalysisEngine measure; Stats measured; resetStats (measured);
        measure.prepare (loaded->sourceRate, loaded->audio.getNumChannels());
        bool ok = true;
        for (int offset = 0; offset < (int) loaded->length; offset += 4096)
        {
            if (threadShouldExit() || request.load() != serial) { ok = false; break; }
            const int count = juce::jmin (4096, (int) loaded->length - offset);
            if (! reader->read (&loaded->audio, offset, count, offset, true, true)) { ok = false; break; }
            juce::AudioBuffer<float> view (loaded->audio.getArrayOfWritePointers(),
                loaded->audio.getNumChannels(), offset, count);
            measure.process (view, measured, count);
        }
        if (! ok) { if (request.load() == serial) fail(); continue; }
        measure.finish (measured);
        const juce::SpinLock::ScopedLockType sl (lock);
        if (request.load() != serial || threadShouldExit()) continue;
        const auto length = loaded->length;
        const auto rate = loaded->sourceRate;
        data.publish (std::move (loaded));
        status = "ready"; fileName = file.getFileName();
        loudnessLufs.store (snapshot (measured).integrated);
        duration.store ((double) length / rate);
        ready.store (true); busy.store (false);
    }
    data.publish ({});
}
} // namespace ml
