#include "FilePreviewPlayer.h"

#include <cmath>

namespace ml
{
FilePreviewPlayer::FilePreviewPlayer()
    : juce::Thread ("MasterLens file preview")
{
}

FilePreviewPlayer::~FilePreviewPlayer()
{
    signalThreadShouldExit();
    notify();
    stopThread (-1);
}

void FilePreviewPlayer::prepare (double targetSampleRate)
{
    stop();
    targetRate = targetSampleRate > 0.0 ? targetSampleRate : 48000.0;
}

void FilePreviewPlayer::start (const juce::File& file, double startSeconds,
                               double endSeconds, bool shouldLoop)
{
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        request.fetch_add (1);
        pendingFile = file; pendingStart = juce::jmax (0.0, startSeconds);
        pendingEnd = juce::jmax (pendingStart + 0.05, endSeconds);
        loop.store (shouldLoop); ready.store (false); active.store (true); busy.store (true);
        status = "loading";
    }

    if (! isThreadRunning()) startThread();
    notify();
}

void FilePreviewPlayer::stop()
{
    active.store (false, std::memory_order_release);
    ready.store (false, std::memory_order_release);
    request.fetch_add (1);
    const juce::SpinLock::ScopedLockType sl (lock);
    pendingFile = juce::File(); status = "idle";
    notify();
}

juce::String FilePreviewPlayer::getStatus() const
{
    const juce::SpinLock::ScopedLockType sl (lock);
    return status;
}

void FilePreviewPlayer::run()
{
    unsigned handled = 0;
    while (! threadShouldExit())
    {
        data.collect();
        const auto serial = request.load();
        if (serial == handled) { wait (25); continue; }
        handled = serial;
        juce::File file; double from = 0, to = 0;
        {
            const juce::SpinLock::ScopedLockType sl (lock);
            if (! active.load()) { data.publish ({}); busy.store (false); continue; }
            file = pendingFile; from = pendingStart; to = pendingEnd;
        }
        const auto fail = [&] (const char* text)
        {
            const juce::SpinLock::ScopedLockType sl (lock);
            if (request.load() == serial)
            { status = text; active.store (false); ready.store (false); busy.store (false); }
        };
        juce::AudioFormatManager formats; formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
        if (! reader || reader->lengthInSamples <= 1 || reader->numChannels < 1 || reader->numChannels > 2
            || ! std::isfinite(reader->sampleRate) || reader->sampleRate < 1000 || reader->sampleRate > 768000)
        { fail ("failed"); continue; }
        const auto first = (juce::int64) juce::jlimit (0.0, (double) reader->lengthInSamples - 1, from * reader->sampleRate);
        const auto last = (juce::int64) juce::jlimit ((double) first+1, (double) reader->lengthInSamples, to * reader->sampleRate);
        // Evidence previews request four seconds; standalone transport requests
        // the whole selected file. Keep a bounded cache for unusually long files.
        const auto length = (int) juce::jmin ((juce::int64) (reader->sampleRate * 900.0), last-first);
        if (length <= 1) { fail ("empty"); continue; }
        auto loaded = std::make_unique<Data>();
        loaded->sourceRate = reader->sampleRate; loaded->startSample = first;
        loaded->length = length; loaded->generation = serial;
        loaded->audio.setSize ((int) reader->numChannels, length);
        bool ok = true;
        for (int offset = 0; offset < length; offset += 4096)
        {
            if (threadShouldExit() || request.load() != serial) { ok = false; break; }
            const int n = juce::jmin (4096, length-offset);
            if (! reader->read (&loaded->audio, offset, n, first+offset, true, true)) { ok = false; break; }
            for (int ch = 0; ch < loaded->audio.getNumChannels(); ++ch)
                for (int i = 0; i < n; ++i) if (! std::isfinite(loaded->audio.getSample(ch,offset+i))) ok = false;
            if (! ok) break;
        }
        if (! ok) { fail ("decode failed"); continue; }
        const juce::SpinLock::ScopedLockType sl (lock);
        if (request.load() != serial || ! active.load() || threadShouldExit()) continue;
        data.publish (std::move (loaded)); status = "playing";
        ready.store (true); busy.store (false);
    }
    data.publish ({});
}

bool FilePreviewPlayer::render (juce::AudioBuffer<float>& output)
{
    if (! active.load (std::memory_order_acquire))
        return false;

    output.clear();

    const auto d = data.acquire();

    if (! d || ! ready.load (std::memory_order_acquire) || d->length <= 1)
        return true;

    if (audioGeneration != d->generation) { audioGeneration = d->generation; readPosition = 0.0; }
    const double ratio = d->sourceRate / juce::jmax (1.0, targetRate.load());
    const int outChannels = output.getNumChannels();

    for (int i = 0; i < output.getNumSamples(); ++i)
    {
        if (readPosition >= (double) d->length)
        {
            if (loop.load (std::memory_order_relaxed))
                readPosition = 0.0;
            else
            {
                active.store (false, std::memory_order_release);
                break;
            }
        }

        const int idx = juce::jlimit (0, (int) d->length - 1, (int) std::floor (readPosition));
        const int next = juce::jmin (idx + 1, (int) d->length - 1);
        const float frac = (float) (readPosition - (double) idx);
        const float l = d->audio.getSample (0, idx) * (1.0f - frac)
                      + d->audio.getSample (0, next) * frac;
        const float r = d->audio.getNumChannels() > 1
                            ? d->audio.getSample (1, idx) * (1.0f - frac)
                                + d->audio.getSample (1, next) * frac
                            : l;

        if (outChannels == 1)
            output.setSample (0, i, 0.5f * (l + r));
        else
        {
            output.setSample (0, i, l);
            output.setSample (1, i, r);
        }

        readPosition += ratio;
    }

    for (int ch = 2; ch < outChannels; ++ch)
        output.clear (ch, 0, output.getNumSamples());

    return true;
}
} // namespace ml
