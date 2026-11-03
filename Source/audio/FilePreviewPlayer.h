#pragma once

#include <atomic>
#include <memory>
#include "RealtimeAsset.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>

namespace ml
{
/** Small, allocation-free-at-render file cue player used by the File QC evidence
    inspector. It loads only the requested region, loops it, and replaces the
    output only while the user has explicitly asked for a preview. */
class FilePreviewPlayer : private juce::Thread
{
public:
    FilePreviewPlayer();
    ~FilePreviewPlayer() override;

    void prepare (double targetSampleRate);
    void start (const juce::File& file, double startSeconds, double endSeconds,
                bool shouldLoop, bool feedAnalysis = false);
    void stop();
    void seek (double fileSeconds);

    bool isActive() const noexcept { return active.load (std::memory_order_acquire); }
    bool isReady() const noexcept { return ready.load (std::memory_order_acquire); }
    bool feedsAnalysis() const noexcept { return feedAnalysis.load (std::memory_order_acquire); }
    double getPositionSeconds() const noexcept { return positionSeconds.load (std::memory_order_relaxed); }
    double getDurationSeconds() const noexcept { return durationSeconds.load (std::memory_order_relaxed); }
    juce::String getStatus() const;

    /** Returns true when the preview owns the output buffer. */
    bool render (juce::AudioBuffer<float>& output);

private:
    struct Data
    {
        juce::AudioBuffer<float> audio;
        double sourceRate = 48000.0;
        juce::int64 startSample = 0;
        juce::int64 length = 0;
        unsigned generation = 0;
    };

    void run() override;

    mutable juce::SpinLock lock;
    RealtimeAsset<Data> data;
    std::atomic<unsigned> request { 0 };
    unsigned audioGeneration = 0;
    juce::File pendingFile;
    double pendingStart = 0.0, pendingEnd = 0.0;
    std::atomic<double> targetRate { 48000.0 };
    std::atomic<double> positionSeconds { 0.0 };
    std::atomic<double> durationSeconds { 0.0 };
    std::atomic<double> seekRequest { -1.0 };
    double readPosition = 0.0;
    bool pendingLoop = true;
    juce::String status { "idle" };
    std::atomic<bool> active { false };
    std::atomic<bool> ready { false };
    std::atomic<bool> busy { false };
    std::atomic<bool> loop { true };
    std::atomic<bool> feedAnalysis { false };
};
} // namespace ml
