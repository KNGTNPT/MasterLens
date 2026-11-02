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
    stopThread (4000);
}

void FilePreviewPlayer::prepare (double targetSampleRate)
{
    stop();
    targetRate = targetSampleRate > 0.0 ? targetSampleRate : 48000.0;
}

void FilePreviewPlayer::start (const juce::File& file, double startSeconds,
                               double endSeconds, bool shouldLoop)
{
    stopThread (4000);

    {
        const juce::SpinLock::ScopedLockType sl (lock);
        data.reset();
        pendingFile = file;
        pendingStart = juce::jmax (0.0, startSeconds);
        pendingEnd = juce::jmax (pendingStart + 0.05, endSeconds);
        status = "loading";
    }

    loop.store (shouldLoop, std::memory_order_relaxed);
    ready.store (false, std::memory_order_release);
    active.store (true, std::memory_order_release);
    busy.store (true, std::memory_order_release);
    startThread();
}

void FilePreviewPlayer::stop()
{
    active.store (false, std::memory_order_release);
    ready.store (false, std::memory_order_release);
    signalThreadShouldExit();

    const juce::SpinLock::ScopedLockType sl (lock);
    data.reset();
    status = "idle";
}

juce::String FilePreviewPlayer::getStatus() const
{
    const juce::SpinLock::ScopedLockType sl (lock);
    return status;
}

void FilePreviewPlayer::run()
{
    juce::File file;
    double startSeconds = 0.0, endSeconds = 0.0;

    {
        const juce::SpinLock::ScopedLockType sl (lock);
        file = pendingFile;
        startSeconds = pendingStart;
        endSeconds = pendingEnd;
    }

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));

    if (reader == nullptr || reader->lengthInSamples <= 0)
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        status = "failed";
        active.store (false, std::memory_order_release);
        busy.store (false, std::memory_order_release);
        return;
    }

    const int channels = juce::jlimit (1, 2, (int) reader->numChannels);
    const double sourceRate = reader->sampleRate > 0.0 ? reader->sampleRate : targetRate;
    const auto first = (juce::int64) juce::jlimit (0.0, (double) reader->lengthInSamples - 1.0,
                                                   startSeconds * sourceRate);
    const auto last = (juce::int64) juce::jlimit ((double) first + 1.0,
                                                  (double) reader->lengthInSamples,
                                                  endSeconds * sourceRate);
    const auto length = juce::jmin ((juce::int64) (sourceRate * 12.0), last - first);

    if (length <= 1)
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        status = "empty";
        active.store (false, std::memory_order_release);
        busy.store (false, std::memory_order_release);
        return;
    }

    auto loaded = std::make_shared<Data>();
    loaded->sourceRate = sourceRate;
    loaded->startSample = first;
    loaded->length = length;
    loaded->audio.setSize (channels, (int) length);
    loaded->audio.clear();
    reader->read (&loaded->audio, 0, (int) length, first, true, true);

    if (threadShouldExit())
    {
        busy.store (false, std::memory_order_release);
        return;
    }

    {
        const juce::SpinLock::ScopedLockType sl (lock);
        data = std::move (loaded);
        status = "playing";
    }

    readPosition = 0.0;
    ready.store (true, std::memory_order_release);
    busy.store (false, std::memory_order_release);
}

bool FilePreviewPlayer::render (juce::AudioBuffer<float>& output)
{
    if (! active.load (std::memory_order_acquire))
        return false;

    output.clear();

    std::shared_ptr<const Data> d;
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        d = data;
    }

    if (d == nullptr || ! ready.load (std::memory_order_acquire) || d->length <= 1)
        return true;

    const double ratio = d->sourceRate / juce::jmax (1.0, targetRate);
    const int outChannels = output.getNumChannels();

    for (int i = 0; i < output.getNumSamples(); ++i)
    {
        if (readPosition >= (double) d->length - 1.0)
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
            output.setSample (0, i, juce::jlimit (-1.0f, 1.0f, 0.5f * (l + r)));
        else
        {
            output.setSample (0, i, juce::jlimit (-1.0f, 1.0f, l));
            output.setSample (1, i, juce::jlimit (-1.0f, 1.0f, r));
        }

        readPosition += ratio;
    }

    for (int ch = 2; ch < outChannels; ++ch)
        output.clear (ch, 0, output.getNumSamples());

    return true;
}
} // namespace ml
