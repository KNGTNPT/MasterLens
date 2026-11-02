#pragma once

#include <juce_dsp/juce_dsp.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>

namespace ml
{
/** Output-only SPSC spectrum transport. One audio-thread producer, one UI reader.

    Storage is fixed at construction. The producer publishes complete, overlapping
    mono frames; it never waits or overwrites a slot owned by the reader. A full
    queue drops a display frame, not audio. The consumer drains to the newest
    published frame, so missed UI ticks do not become a playback backlog.
*/
class SpectrumFifo
{
public:
    static constexpr int fftOrder = 13;
    static constexpr int fftSize = 1 << fftOrder;
    static constexpr int hopSize = fftSize / 4;
    static constexpr int capacity = 4;

    SpectrumFifo() = default;

    struct Frame
    {
        std::array<float, fftSize> samples {};
        std::uint64_t endSample = 0; // exclusive, relative to prepare()
        std::uint64_t generation = 0;
        double sampleRate = 48000.0;
    };

    /** Producer lifecycle only: call while processBlock is stopped.

        Do not reset AbstractFifo while the editor may be reading. Old queued
        frames retain their generation and are rejected by the consumer instead.
    */
    void prepare (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
        historyWrite = samplesSinceFrame = 0;
        sampleCount = 0;
        producedSamples.store (0, std::memory_order_release);
        producerGeneration = generation.fetch_add (1, std::memory_order_acq_rel) + 1;
    }

    /** Audio thread only. Tap the final main output, after audition/reference/trim.
        Stereo is (L + R) / 2; mono is copied without attenuation. Never modifies
        output, allocates, windows, performs FFT, or calls into the UI.
    */
    void pushMainOutput (const juce::AudioBuffer<float>& output) noexcept
    {
        if (output.getNumChannels() == 0 || output.getNumSamples() == 0)
            return;

        const auto* left = output.getReadPointer (0);
        const auto* right = output.getNumChannels() > 1 ? output.getReadPointer (1) : nullptr;

        for (int i = 0; i < output.getNumSamples(); ++i)
        {
            history[(size_t) historyWrite] = right != nullptr ? 0.5f * (left[i] + right[i]) : left[i];
            historyWrite = (historyWrite + 1) & (fftSize - 1);
            ++sampleCount;
            ++samplesSinceFrame;

            if (sampleCount >= fftSize && samplesSinceFrame >= hopSize)
            {
                publishFrame();
                samplesSinceFrame = 0;
            }
        }

        producedSamples.store (sampleCount, std::memory_order_release);
    }

    /** UI thread only. Copies the newest complete frame and releases all older
        queued frames. The audio producer cannot touch these slots until read()
        releases them. Do not attach a second consumer (including a debug probe).
    */
    bool readLatest (Frame& destination) noexcept
    {
        const auto scope = fifo.read (fifo.getNumReady());

        if (scope.blockSize1 + scope.blockSize2 == 0)
            return false;

        const int latest = scope.blockSize2 > 0 ? scope.startIndex2 + scope.blockSize2 - 1
                                                : scope.startIndex1 + scope.blockSize1 - 1;
        destination = frames[(size_t) latest];
        return destination.generation == getGeneration();
    }

    std::uint64_t getGeneration() const noexcept { return generation.load (std::memory_order_acquire); }
    std::uint64_t getProducedSamples() const noexcept { return producedSamples.load (std::memory_order_acquire); }
    std::uint64_t getDroppedFrames() const noexcept { return droppedFrames.load (std::memory_order_relaxed); }

    /** UI-side cached raw and Blackman-windowed frame. Construct on the message
        thread, update from its timer, and only read these caches from the UI.
        Blackman coefficients are prepared once, outside paint/processBlock.
    */
    class Reader
    {
    public:
        Reader()
        {
            using Window = juce::dsp::WindowingFunction<float>;
            Window::fillWindowingTables (window.data(), fftSize, Window::blackman, false);

            double sum = 0.0;
            for (const auto coefficient : window)
                sum += coefficient;

            // For a future unnormalised FFT: bin-centred sine amplitude = bin * scale.
            magnitudeScale = (float) (2.0 / sum);
        }

        bool update (SpectrumFifo& source) noexcept
        {
            if (raw.generation != source.getGeneration())
                valid = false;

            if (! source.readLatest (raw))
            {
                if (raw.generation != source.getGeneration())
                    valid = false;
                return false;
            }

            for (size_t i = 0; i < windowed.size(); ++i)
                windowed[i] = raw.samples[i] * window[i];

            valid = true;
            return true;
        }

        bool hasFrame() const noexcept { return valid; }
        const Frame& getFrame() const noexcept { return raw; }
        const std::array<float, fftSize>& getWindowedSamples() const noexcept { return windowed; }
        float getFftMagnitudeScale() const noexcept { return magnitudeScale; }

    private:
        Frame raw;
        std::array<float, fftSize> window {}, windowed {};
        float magnitudeScale = 0.0f;
        bool valid = false;
    };

private:
    void publishFrame() noexcept
    {
        const auto scope = fifo.write (1);

        if (scope.blockSize1 + scope.blockSize2 == 0)
        {
            droppedFrames.fetch_add (1, std::memory_order_relaxed);
            return;
        }

        auto& frame = frames[(size_t) (scope.blockSize1 > 0 ? scope.startIndex1 : scope.startIndex2)];
        const int tail = fftSize - historyWrite;
        std::copy_n (history.data() + historyWrite, tail, frame.samples.data());
        std::copy_n (history.data(), historyWrite, frame.samples.data() + tail);
        frame.endSample = sampleCount;
        frame.generation = producerGeneration;
        frame.sampleRate = sampleRate;
        producedSamples.store (sampleCount, std::memory_order_release);
    }

    // AbstractFifo reserves one slot; capacity + 1 gives exactly capacity frames.
    juce::AbstractFifo fifo { capacity + 1 };
    std::array<Frame, capacity + 1> frames {};
    std::array<float, fftSize> history {};
    int historyWrite = 0, samplesSinceFrame = 0;
    std::uint64_t sampleCount = 0, producerGeneration = 0;
    double sampleRate = 48000.0;
    std::atomic<std::uint64_t> generation { 0 }, producedSamples { 0 }, droppedFrames { 0 };

    static_assert (std::atomic<std::uint64_t>::is_always_lock_free,
                   "Spectrum transport counters must be lock free");
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumFifo)
};
} // namespace ml
