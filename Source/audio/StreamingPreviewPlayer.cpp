#include "StreamingPreviewPlayer.h"

#include <cmath>

namespace ml
{
StreamingPreviewPlayer::StreamingPreviewPlayer()
    : juce::Thread ("MasterLens streaming preview")
{
}

StreamingPreviewPlayer::~StreamingPreviewPlayer()
{
    signalThreadShouldExit();
    notify();
    stopThread (4000);
}

void StreamingPreviewPlayer::prepare (double targetSampleRate, double ringSeconds)
{
    signalThreadShouldExit();
    notify();
    stopThread (4000);

    targetRate.store (targetSampleRate > 0.0 ? targetSampleRate : 48000.0,
                      std::memory_order_relaxed);

    // Reserve for the worst supported source rate so the ring never reallocates
    // while the audio thread is rendering.
    const int wanted = (int) std::ceil (192000.0 * juce::jmax (1.0, ringSeconds));
    int frames = 1;

    while (frames < wanted)
        frames <<= 1;

    ringFrames = frames;
    ringMask = frames - 1;
    ringL.assign ((size_t) frames, 0.0f);
    ringR.assign ((size_t) frames, 0.0f);
    chunk.setSize (2, chunkFrames, false, true, false);

    writeFrame.store (0, std::memory_order_relaxed);
    playFrame.store (0, std::memory_order_relaxed);
    ringGeneration.store (0, std::memory_order_relaxed);
    playEpoch.store (0, std::memory_order_relaxed);
    openEpoch.store (0, std::memory_order_relaxed);
    audioEpoch = 0;
    cubicPosition = 0.0;
    mixLevel = 0.0f;
    loopFrames = loopStartFrame = 0;
    active.store (false, std::memory_order_relaxed);
    buffering.store (false, std::memory_order_relaxed);
    ready.store (false, std::memory_order_relaxed);
    failed.store (false, std::memory_order_relaxed);
    finished.store (false, std::memory_order_relaxed);
    displaySeconds.store (0.0, std::memory_order_relaxed);

    {
        const juce::SpinLock::ScopedLockType sl (lock);
        status = "empty";
    }
}

void StreamingPreviewPlayer::open (const juce::File& file, bool shouldLoop)
{
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        pendingFile = file;
        pendingReader.reset();
        status = "loading";
    }

    pendingLoop.store (shouldLoop, std::memory_order_relaxed);
    loop.store (shouldLoop, std::memory_order_relaxed);
    ready.store (false, std::memory_order_release);
    failed.store (false, std::memory_order_relaxed);
    finished.store (false, std::memory_order_relaxed);
    active.store (false, std::memory_order_release);
    openEpoch.fetch_add (1, std::memory_order_acq_rel);

    if (! isThreadRunning())
        startThread();
    else
        notify();
}

void StreamingPreviewPlayer::openReader (std::unique_ptr<juce::AudioFormatReader> reader,
                                         bool shouldLoop)
{
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        pendingFile = juce::File();
        pendingReader = std::move (reader);
        status = "loading";
    }

    pendingLoop.store (shouldLoop, std::memory_order_relaxed);
    loop.store (shouldLoop, std::memory_order_relaxed);
    ready.store (false, std::memory_order_release);
    failed.store (false, std::memory_order_relaxed);
    finished.store (false, std::memory_order_relaxed);
    active.store (false, std::memory_order_release);
    openEpoch.fetch_add (1, std::memory_order_acq_rel);

    if (! isThreadRunning())
        startThread();
    else
        notify();
}

void StreamingPreviewPlayer::start (double fromSeconds)
{
    startRequest.store (juce::jmax (0.0, fromSeconds), std::memory_order_relaxed);
    startFrameForUi.store ((juce::int64) (juce::jmax (0.0, fromSeconds)
                                          * sourceRate.load (std::memory_order_relaxed)),
                           std::memory_order_relaxed);
    playEpoch.fetch_add (1, std::memory_order_acq_rel);
    active.store (true, std::memory_order_release);
    buffering.store (true, std::memory_order_relaxed);

    {
        const juce::SpinLock::ScopedLockType sl (lock);
        status = "buffering";
    }

    if (! isThreadRunning())
        startThread();
    else
        notify();
}

void StreamingPreviewPlayer::seek (double seconds)
{
    start (seconds);
}

void StreamingPreviewPlayer::stop()
{
    active.store (false, std::memory_order_release);
    buffering.store (false, std::memory_order_relaxed);

    {
        const juce::SpinLock::ScopedLockType sl (lock);
        status = "idle";
    }
}

double StreamingPreviewPlayer::getBufferedSeconds() const noexcept
{
    const auto rate = sourceRate.load (std::memory_order_relaxed);
    const auto ahead = writeFrame.load (std::memory_order_acquire)
                       - playFrame.load (std::memory_order_relaxed);
    return rate > 0.0 ? (double) juce::jmax ((juce::int64) 0, ahead) / rate : 0.0;
}

juce::String StreamingPreviewPlayer::getStatus() const
{
    const juce::SpinLock::ScopedLockType sl (lock);
    return status;
}
bool StreamingPreviewPlayer::render (juce::AudioBuffer<float>& output)
{
    const int n = output.getNumSamples();

    if (n <= 0)
        return false;

    const bool on = active.load (std::memory_order_acquire);

    if (! on && mixLevel <= 0.0f)
        return false;

    const unsigned wantEpoch = playEpoch.load (std::memory_order_acquire);

    if (audioEpoch != wantEpoch)
    {
        if (ringGeneration.load (std::memory_order_acquire) != wantEpoch)
        {
            // The reader has not restarted the ring yet: silence, never stale data.
            output.clear();
            buffering.store (true, std::memory_order_relaxed);
            playFrame.store (0, std::memory_order_relaxed);
            return true;
        }

        audioEpoch = wantEpoch;
        cubicPosition = 0.0;
        playFrame.store (0, std::memory_order_relaxed);
        loopStartFrame = startFrameForUi.load (std::memory_order_relaxed);
        const auto total = totalFrames.load (std::memory_order_relaxed);
        loopFrames = (loop.load (std::memory_order_relaxed) && total > loopStartFrame)
                         ? total - loopStartFrame : 0;
    }

    auto* outL = output.getWritePointer (0);
    auto* outR = output.getNumChannels() > 1 ? output.getWritePointer (1) : nullptr;

    const double rate = juce::jmax (1000.0, sourceRate.load (std::memory_order_relaxed));
    const double step = rate / juce::jmax (1.0, targetRate.load (std::memory_order_relaxed));
    const float fadeStep = 1.0f / (float) juce::jmax (1.0, targetRate.load() * 0.005);
    const auto written = writeFrame.load (std::memory_order_acquire);
    bool underrun = false;

    for (int i = 0; i < n; ++i)
    {
        const float targetMix = on ? 1.0f : 0.0f;
        mixLevel += juce::jlimit (-fadeStep, fadeStep, targetMix - mixLevel);

        if (mixLevel <= 0.0f && ! on)
        {
            outL[i] = 0.0f;

            if (outR != nullptr)
                outR[i] = 0.0f;

            continue;
        }

        const auto frame = (juce::int64) std::floor (cubicPosition);

        if (written < frame + 2)
        {
            underrun = true;
            outL[i] = 0.0f;

            if (outR != nullptr)
                outR[i] = 0.0f;

            continue;
        }

        const auto at = [this] (juce::int64 index) -> float
        {
            return ringL[(size_t) (index & ringMask)];
        };
        const auto atR = [this] (juce::int64 index) -> float
        {
            return ringR[(size_t) (index & ringMask)];
        };

        const auto i0 = juce::jmax ((juce::int64) 0, frame - 1);
        const auto i2 = frame + 1;
        const auto i3 = frame + 2;
        const float t = (float) (cubicPosition - (double) frame);

        const auto cubic = [] (float p0, float p1, float p2, float p3, float x)
        {
            const float a = -0.5f * p0 + 1.5f * p1 - 1.5f * p2 + 0.5f * p3;
            const float b = p0 - 2.5f * p1 + 2.0f * p2 - 0.5f * p3;
            const float c = -0.5f * p0 + 0.5f * p2;
            return ((a * x + b) * x + c) * x + p1;
        };

        const float l = cubic (at (i0), at (frame), at (i2), at (i3), t);
        const float r = cubic (atR (i0), atR (frame), atR (i2), atR (i3), t);

        outL[i] = l * mixLevel;

        if (outR != nullptr)
            outR[i] = r * mixLevel;

        cubicPosition += step;
    }

    const auto consumed = (juce::int64) std::floor (cubicPosition);
    playFrame.store (consumed, std::memory_order_relaxed);
    buffering.store (underrun, std::memory_order_relaxed);

    const double position = loopFrames > 0
        ? (double) (loopStartFrame + (consumed % loopFrames)) / rate
        : (double) (loopStartFrame + consumed) / rate;
    displaySeconds.store (position, std::memory_order_relaxed);

    if (! loop.load (std::memory_order_relaxed)
        && finished.load (std::memory_order_relaxed)
        && consumed + 2 >= totalFrames.load (std::memory_order_relaxed) - loopStartFrame)
        active.store (false, std::memory_order_relaxed);

    for (int ch = 2; ch < output.getNumChannels(); ++ch)
        output.clear (ch, 0, n);

    return true;
}
void StreamingPreviewPlayer::run()
{
    unsigned handledOpen = 0;
    unsigned handledPlay = 0;
    std::unique_ptr<juce::AudioFormatReader> reader;
    double rate = 48000.0;
    juce::int64 total = 0;
    juce::int64 filePos = 0;
    juce::int64 loopStart = 0;
    bool haveSource = false;

    while (! threadShouldExit())
    {
        const auto openRequest = openEpoch.load (std::memory_order_acquire);

        if (openRequest != handledOpen)
        {
            handledOpen = openRequest;
            juce::File file;
            std::unique_ptr<juce::AudioFormatReader> adopted;

            {
                const juce::SpinLock::ScopedLockType sl (lock);
                file = pendingFile;
                adopted = std::move (pendingReader);
            }

            reader.reset();
            ready.store (false, std::memory_order_release);
            failed.store (false, std::memory_order_relaxed);
            finished.store (false, std::memory_order_relaxed);
            haveSource = false;
            handledPlay = playEpoch.load() - 1;   // force a ring reset below

            if (adopted == nullptr && file != juce::File())
            {
                juce::AudioFormatManager formats;
                formats.registerBasicFormats();
                adopted.reset (formats.createReaderFor (file));
            }

            if (adopted == nullptr
                || adopted->lengthInSamples <= 1
                || adopted->numChannels < 1 || adopted->numChannels > 2
                || ! std::isfinite (adopted->sampleRate)
                || adopted->sampleRate < 1000.0 || adopted->sampleRate > 768000.0)
            {
                failed.store (true, std::memory_order_release);

                const juce::SpinLock::ScopedLockType sl (lock);
                status = "failed";
                continue;
            }

            rate = adopted->sampleRate;
            total = adopted->lengthInSamples;
            sourceRate.store (rate, std::memory_order_relaxed);
            totalFrames.store (total, std::memory_order_relaxed);
            duration.store ((double) total / rate, std::memory_order_relaxed);
            chunk.setSize (juce::jmin (2, (int) adopted->numChannels), chunkFrames,
                           false, true, false);
            reader = std::move (adopted);
            haveSource = true;
            ready.store (true, std::memory_order_release);

            {
                const juce::SpinLock::ScopedLockType sl (lock);
                status = "ready";
            }
        }

        if (! haveSource)
        {
            wait (25);
            continue;
        }

        const auto playRequest = playEpoch.load (std::memory_order_acquire);

        if (playRequest != handledPlay)
        {
            handledPlay = playRequest;
            writeFrame.store (0, std::memory_order_release);
            filePos = juce::jlimit ((juce::int64) 0, juce::jmax ((juce::int64) 0, total - 1),
                                    (juce::int64) (startRequest.load (std::memory_order_relaxed)
                                                   * rate));
            loopStart = filePos;
            finished.store (false, std::memory_order_relaxed);
            ringGeneration.store (playRequest, std::memory_order_release);
        }

        if (failed.load (std::memory_order_relaxed))
        {
            wait (40);
            continue;
        }

        const auto written = writeFrame.load (std::memory_order_relaxed);
        const auto played = playFrame.load (std::memory_order_relaxed);
        const auto freeFrames = (juce::int64) ringFrames - (written - played);

        if (freeFrames < chunkFrames)
        {
            wait (8);
            continue;
        }

        const auto available = total - filePos;
        const int count = (int) juce::jmin ((juce::int64) chunkFrames, available);

        if (count <= 0)
        {
            if (loop.load (std::memory_order_relaxed) && loopStart < total)
                filePos = loopStart;
            else
            {
                finished.store (true, std::memory_order_release);

                const juce::SpinLock::ScopedLockType sl (lock);
                status = "finished";
            }

            wait (20);
            continue;
        }

        chunk.clear();

        if (! reader->read (&chunk, 0, count, filePos, true, true))
        {
            failed.store (true, std::memory_order_release);

            const juce::SpinLock::ScopedLockType sl (lock);
            status = "decode failed";
            continue;
        }

        const int channels = chunk.getNumChannels();

        for (int i = 0; i < count; ++i)
        {
            const auto index = (size_t) ((written + i) & ringMask);
            ringL[index] = chunk.getSample (0, i);
            ringR[index] = chunk.getSample (juce::jmin (1, channels - 1), i);
        }

        writeFrame.store (written + count, std::memory_order_release);
        filePos += count;

        if (filePos >= total)
        {
            if (loop.load (std::memory_order_relaxed) && loopStart < total)
                filePos = loopStart;
            else
            {
                finished.store (true, std::memory_order_release);

                const juce::SpinLock::ScopedLockType sl (lock);
                status = "finished";
            }
        }
        else
        {
            const juce::SpinLock::ScopedLockType sl (lock);

            if (active.load (std::memory_order_relaxed))
                status = "playing";
        }
    }
}
} // namespace ml
