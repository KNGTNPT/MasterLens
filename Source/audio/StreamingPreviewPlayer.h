#pragma once

#include <atomic>
#include <memory>
#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>

namespace ml
{
/** Constant-memory preview player for long files.

    A reader thread decodes 8192-frame chunks into a preallocated power-of-two
    ring at the file's own sample rate; the audio thread only reads that ring and
    resamples with the same 4-point Catmull-Rom as ReferencePlayer. Underruns
    render silence and raise a buffering flag - the callback never blocks, locks
    or allocates.

    Generation handshake: the UI bumps playEpoch, the reader resets the ring and
    publishes ringGeneration once the ring starts at frame 0 again. The audio
    thread stays silent until it sees ringGeneration == playEpoch, so a seek can
    never play stale ring content.
*/
class StreamingPreviewPlayer : private juce::Thread
{
public:
    StreamingPreviewPlayer();
    ~StreamingPreviewPlayer() override;

    /** Allocates the ring (max 4 s at 192 kHz) and sets the host rate. */
    void prepare (double targetSampleRate, double ringSeconds = 4.0);

    /** Opens a file on the reader thread; does not start playback. */
    void open (const juce::File& file, bool shouldLoop);

    /** Test / advanced: adopts an already-open reader. */
    void openReader (std::unique_ptr<juce::AudioFormatReader> reader, bool shouldLoop);

    /** Starts at fromSeconds; the reader restarts the ring from there. */
    void start (double fromSeconds);

    /** Restarts the ring at seconds while keeping the active state. */
    void seek (double seconds);

    void stop();

    bool isActive() const noexcept { return active.load (std::memory_order_acquire); }
    bool isBuffering() const noexcept { return buffering.load (std::memory_order_relaxed); }
    bool isReady() const noexcept { return ready.load (std::memory_order_acquire); }
    bool isFinished() const noexcept { return finished.load (std::memory_order_relaxed); }
    double getPositionSeconds() const noexcept { return displaySeconds.load (std::memory_order_relaxed); }
    double getDurationSeconds() const noexcept { return duration.load (std::memory_order_relaxed); }
    double getBufferedSeconds() const noexcept;

    juce::String getStatus() const;

    /** Replaces the output while active (or while the stop fade is running).
        @returns true when this player owns the buffer. */
    bool render (juce::AudioBuffer<float>& output);

private:
    void run() override;
    void resetForEpoch();

    // ---- message thread -> reader thread -----------------------------------
    mutable juce::SpinLock lock;
    juce::File pendingFile;
    std::unique_ptr<juce::AudioFormatReader> pendingReader;
    std::atomic<unsigned> openEpoch { 0 };
    std::atomic<bool> pendingLoop { false };

    // ---- reader thread -> audio thread -------------------------------------
    std::vector<float> ringL, ringR;
    int ringFrames = 0, ringMask = 0;
    int chunkFrames = 8192;
    juce::AudioBuffer<float> chunk;
    std::atomic<juce::int64> writeFrame { 0 };
    std::atomic<unsigned> ringGeneration { 0 };
    std::atomic<double> sourceRate { 48000.0 };
    std::atomic<double> duration { 0.0 };
    std::atomic<juce::int64> totalFrames { 0 };
    std::atomic<bool> ready { false };
    std::atomic<bool> loop { false };
    std::atomic<bool> failed { false };
    std::atomic<bool> finished { false };

    // ---- audio thread ------------------------------------------------------
    std::atomic<unsigned> playEpoch { 0 };
    std::atomic<double> startRequest { 0.0 };
    std::atomic<juce::int64> startFrameForUi { 0 };
    std::atomic<double> displaySeconds { 0.0 };
    std::atomic<juce::int64> playFrame { 0 };
    std::atomic<bool> active { false };
    std::atomic<bool> buffering { false };
    std::atomic<double> targetRate { 48000.0 };

    unsigned audioEpoch = 0;
    double cubicPosition = 0.0;
    float mixLevel = 0.0f;
    juce::int64 loopFrames = 0, loopStartFrame = 0;

    juce::String status { "empty" };
};
} // namespace ml
