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
    stopThread (4000);
}

void ReferencePlayer::load (const juce::StringArray& files, double sampleRate)
{
    stopThread (4000);

    pendingPath.clear();

    for (const auto& f : files)
        if (juce::File (f).existsAsFile())
        {
            pendingPath = f;
            break;
        }

    targetRate.store (sampleRate > 0.0 ? sampleRate : 48000.0, std::memory_order_relaxed);

    {
        const juce::SpinLock::ScopedLockType sl (lock);
        status = pendingPath.isEmpty() ? "empty" : "loading";
        fileName.clear();

        if (pendingPath.isEmpty())
            data.reset();
    }

    ready.store (false, std::memory_order_release);
    busy.store (! pendingPath.isEmpty(), std::memory_order_release);

    if (pendingPath.isNotEmpty())
        startThread();
}

void ReferencePlayer::clear()
{
    stopThread (4000);

    {
        const juce::SpinLock::ScopedLockType sl (lock);
        data.reset();
        status = "empty";
        fileName.clear();
    }

    ready.store (false, std::memory_order_release);
    busy.store (false, std::memory_order_release);
    enabled.store (false, std::memory_order_release);
    duration.store (0.0, std::memory_order_relaxed);
    offsetSeconds.store (0.0, std::memory_order_relaxed);
    loudnessLufs.store (kSilentDb, std::memory_order_relaxed);
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

    std::shared_ptr<const Data> d;

    {
        const juce::SpinLock::ScopedLockType sl (lock);
        d = data;
    }

    if (d == nullptr || ! hostPlaying || output.getNumSamples() <= 0)
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
            output.setSample (0, i, juce::jlimit (-1.0f, 1.0f, 0.5f * (l + r) * gain));
        else
        {
            output.setSample (0, i, juce::jlimit (-1.0f, 1.0f, l * gain));
            output.setSample (1, i, juce::jlimit (-1.0f, 1.0f, r * gain));
        }

        pos += ratio;
    }

    for (int ch = 2; ch < output.getNumChannels(); ++ch)
        output.clear (ch, 0, output.getNumSamples());

    return true;
}

void ReferencePlayer::run()
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    const juce::File file (pendingPath);
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));

    if (reader == nullptr || reader->lengthInSamples <= 1)
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        status = "failed";
        busy.store (false, std::memory_order_release);
        return;
    }

    const int channels = juce::jlimit (1, 2, (int) reader->numChannels);
    const int length = (int) juce::jmin ((juce::int64) juce::jmax (1, 48000 * 60 * 60),
                                          reader->lengthInSamples);

    auto loaded = std::make_shared<Data>();
    loaded->sourceRate = reader->sampleRate > 0.0 ? reader->sampleRate : 48000.0;
    loaded->length = length;
    loaded->audio.setSize (channels, length);
    loaded->audio.clear();

    reader->read (&loaded->audio, 0, length, 0, true, true);

    if (threadShouldExit())
        return;

    // Measure the reference on the worker thread so the monitor can match its
    // loudness instead of applying the input's trim to an already-mastered file.
    ml::AnalysisEngine measure;
    ml::Stats referenceStats;
    ml::resetStats (referenceStats);
    measure.prepare (loaded->sourceRate, channels);
    measure.process (loaded->audio, referenceStats, length);
    measure.finish (referenceStats);
    const auto measuredLoudness = ml::snapshot (referenceStats).integrated;

    {
        const juce::SpinLock::ScopedLockType sl (lock);
        data = std::move (loaded);
        status = "ready";
        fileName = file.getFileName();
    }

    loudnessLufs.store (measuredLoudness, std::memory_order_relaxed);
    duration.store ((double) length / juce::jmax (1.0, reader->sampleRate),
                    std::memory_order_relaxed);
    ready.store (true, std::memory_order_release);
    busy.store (false, std::memory_order_release);
}
} // namespace ml
