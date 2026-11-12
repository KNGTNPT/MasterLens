#pragma once

#include <atomic>
#include <memory>
#include "RealtimeAsset.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>

#include "../dsp/AnalysisEngine.h"

namespace ml
{
/** In-plugin reference A/B player.

    The first imported reference file is decoded into memory on a worker thread
    and rendered synchronously with the host playhead. It is only audible while
    the user explicitly enables reference playback; otherwise the plug-in stays
    bit-transparent. Playback uses 4-point Catmull-Rom interpolation when the
    reference file rate differs from the host rate, so 44.1 / 48 / 96 kHz
    references stay in sync without the dull top end of linear interpolation.
*/
class ReferencePlayer : private juce::Thread
{
public:
    ReferencePlayer();
    ~ReferencePlayer() override;

    void prepare (double sampleRate) noexcept { targetRate.store (sampleRate); }
    void load (const juce::StringArray& files, double targetSampleRate);
    void clear();

    bool isBusy() const noexcept { return busy.load (std::memory_order_acquire); }
    bool isReady() const noexcept { return ready.load (std::memory_order_acquire); }
    juce::String getStatus() const;
    juce::String getFileName() const;
    double getDuration() const noexcept { return duration.load (std::memory_order_relaxed); }
    float getLoudness() const noexcept { return loudnessLufs.load (std::memory_order_relaxed); }

    void setEnabled (bool shouldPlay) noexcept { enabled.store (shouldPlay, std::memory_order_release); }
    bool isEnabled() const noexcept { return enabled.load (std::memory_order_acquire); }

    void setOffsetSeconds (double seconds) noexcept
    {
        offsetSeconds.store (seconds, std::memory_order_relaxed);
    }

    double getOffsetSeconds() const noexcept
    {
        return offsetSeconds.load (std::memory_order_relaxed);
    }

    void setGainDb (float db) noexcept
    {
        gainDb.store (juce::jlimit (-24.0f, 12.0f, db), std::memory_order_relaxed);
    }

    /** Cue loop over the reference (3.2): start + length in seconds. */
    void setLoop (bool on, double startSeconds, double lengthSeconds) noexcept
    {
        loopOn.store (on, std::memory_order_release);
        loopStartSeconds.store (juce::jmax (0.0, startSeconds), std::memory_order_relaxed);
        loopLengthSeconds.store (juce::jmax (0.0, lengthSeconds), std::memory_order_relaxed);

        if (on && streaming.load (std::memory_order_acquire))
            preloadStreamLoop();
        else if (! on)
        {
            const juce::SpinLock::ScopedLockType sl (loopLock);
            streamLoopReady.store (false, std::memory_order_release);
        }
    }

    bool isLooping() const noexcept { return loopOn.load (std::memory_order_acquire); }

    /** 3.2: long references play from a bounded read-ahead buffer instead of
        being decoded into one big AudioBuffer. Returns the buffered bytes. */
    bool isStreaming() const noexcept { return streaming.load (std::memory_order_acquire); }
    juce::int64 getMemoryBytes() const noexcept { return memoryBytes.load (std::memory_order_relaxed); }
    /** Highest reference memory seen since construction (streamed or in-memory). */
    juce::int64 getPeakMemoryBytes() const noexcept { return peakMemoryBytes.load (std::memory_order_relaxed); }

    float getGainDb() const noexcept
    {
        return gainDb.load (std::memory_order_relaxed);
    }

    /** Replaces the output with the reference at the host position.
        @returns true when reference audio was written. */
    bool render (juce::AudioBuffer<float>& output, double hostSeconds,
                 bool hostPlaying, float extraGainDb, float inputGainDb = 0.0f);

    bool renderStreaming (juce::BufferingAudioReader& reader, juce::AudioBuffer<float>& output,
                          double hostSeconds, bool hostPlaying,
                          float extraGainDb, float inputGainDb);

    void preloadStreamLoop();
    bool renderStreamLoop (juce::AudioBuffer<float>& output, bool hostPlaying,
                           float extraGainDb, float inputGainDb);

    juce::SpinLock loopLock;
    juce::AudioBuffer<float> streamLoop;
    std::atomic<bool> streamLoopReady { false };
    double streamLoopPos = 0.0;
    juce::String streamPath;

private:
    struct Data
    {
        juce::AudioBuffer<float> audio;
        double sourceRate = 48000.0;
        juce::int64 length = 0;
    };

    void run() override;

    mutable juce::SpinLock lock;
    RealtimeAsset<Data> data;
    std::atomic<unsigned> request { 0 };
    float audioMix = 0.0f;
    double audioPosition = 0.0;
    std::atomic<float> liveMix { 0.0f };
    juce::String pendingPath;
    std::atomic<double> targetRate { 48000.0 };
    juce::String status { "empty" };
    juce::String fileName;
    std::atomic<bool> busy { false };
    std::atomic<bool> ready { false };
    std::atomic<bool> enabled { false };
    std::atomic<double> duration { 0.0 };
    std::atomic<double> offsetSeconds { 0.0 };
    std::atomic<bool> loopOn { false };
    std::atomic<double> loopStartSeconds { 0.0 }, loopLengthSeconds { 0.0 };
    double loopPosition = -1.0;

    std::atomic<bool> streaming { false };
    std::atomic<juce::int64> memoryBytes { 0 };
    std::atomic<juce::int64> peakMemoryBytes { 0 };

    // Long-file path: a bounded read-ahead reader serviced by a time-slice
    // thread. Shared so the audio thread can keep one alive while a new file
    // is being swapped in.
    juce::TimeSliceThread streamThread { "MasterLens reference stream" };
    juce::SpinLock streamLock;
    std::shared_ptr<juce::BufferingAudioReader> streamReader;
    std::vector<std::shared_ptr<juce::BufferingAudioReader>> retiredReaders;
    juce::AudioBuffer<float> streamChunk;
    juce::int64 streamLength = 0;
    double streamRate = 48000.0;
    std::atomic<float> gainDb { 0.0f };
    std::atomic<float> loudnessLufs { kSilentDb };
};
/** Wrap a time into [start, start + length); used by the reference cue loop. */
inline double wrapLoopPosition (double seconds, double start, double length)
{
    if (length <= 0.0)
        return seconds;

    const double offset = std::fmod (seconds - start, length);
    return start + (offset < 0.0 ? offset + length : offset);
}

} // namespace ml
