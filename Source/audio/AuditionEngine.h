#pragma once

#include <atomic>
#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>

namespace ml
{
/** Problem audition.

    Two monitoring modes are supported:

      - Live: the current input is band-limited and follows the host playhead.
        Nothing is read back from history, so DAW scrubbing / stop is mirrored
        exactly. This is the default for the signal-health cards.
      - Loop: a short slice from the recent 12 s history is looped. This is used
        by timestamped problem regions and album risk rows.

    Bypassed (bit-transparent) unless the user explicitly starts an audition.
*/
class AuditionEngine
{
public:
    enum class Mode { off = 0, live = 1, loop = 2 };

    void prepare (double sampleRate, int numChannels);
    void reset();

    /** Message-thread request: loop a slice from the recent history. */
    void start (double startSeconds, double endSeconds, float lowHz, float highHz, bool monoFold);

    /** Message-thread request: filter the live input and follow the host. */
    void startLive (float lowHz, float highHz, bool monoFold);

    void stop();

    bool isActive() const noexcept { return active.load (std::memory_order_acquire); }

    /** True when the last timestamp request fell outside the history ring. The
        caller is expected to tell the user instead of silently playing a
        different (more recent) slice. */
    bool consumeRangeRejected (double& requestedStart) noexcept
    {
        if (rangeRejected.exchange (0, std::memory_order_acq_rel) == 0)
            return false;

        requestedStart = rejectedStart.load (std::memory_order_relaxed);
        return true;
    }

    double getRingSeconds() const noexcept
    {
        return sampleRate > 0.0 ? (double) ringLength / sampleRate : 12.0;
    }
    bool isLive() const noexcept { return mode.load (std::memory_order_acquire) == (int) Mode::live; }
    bool isLoop() const noexcept { return mode.load (std::memory_order_acquire) == (int) Mode::loop; }
    Mode getMode() const noexcept { return (Mode) mode.load (std::memory_order_acquire); }
    float lowHz() const noexcept { return requestedLo.load (std::memory_order_relaxed); }
    float highHz() const noexcept { return requestedHi.load (std::memory_order_relaxed); }
    bool mono() const noexcept { return requestedMono.load (std::memory_order_relaxed); }

    /** Audio-thread processing. History is always captured; output is replaced
        only while an audition is active. */
    void process (juce::AudioBuffer<float>& buffer);

private:
    struct Biquad
    {
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
        float z1 = 0.0f, z2 = 0.0f;

        void reset() noexcept { z1 = z2 = 0.0f; }
        void setHighPass (double sr, double hz, double q) noexcept;
        void setLowPass (double sr, double hz, double q) noexcept;
        float process (float x) noexcept
        {
            const float y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
    };

    std::vector<float> left, right;
    int ringLength = 0;
    int writePos = 0;
    int64_t totalSamples = 0;
    double sampleRate = 48000.0;

    std::atomic<bool> pendingStart { false };
    std::atomic<bool> active { false };
    std::atomic<int> mode { (int) Mode::off };
    std::atomic<double> requestedStart { 0.0 }, requestedEnd { 0.0 };
    std::atomic<float> requestedLo { 20.0f }, requestedHi { 20000.0f };
    std::atomic<bool> requestedMono { false };
    std::atomic<int> rangeRejected { 0 };
    std::atomic<double> rejectedStart { 0.0 };

    int loopStart = 0, loopLength = 0, loopPos = 0;
    int liveFade = 0;
    float gain = 1.0f;
    int fadeSamples = 1;

    Biquad hpL, hpR, lpL, lpR;
};
} // namespace ml
