#pragma once

#include <atomic>
#include <vector>

#include <juce_audio_formats/juce_audio_formats.h>

#include "AnalysisEngine.h"

namespace ml
{
/** BS.1770-4 gated integrated loudness from momentary windows.

    Each entry is a 400 ms momentary loudness sampled every 100 ms - exactly the
    window/hop pair BS.1770 uses for gating. Absolute gate -70 LUFS, relative
    gate -10 LU below the level of the absolute-gated material.
*/
float gatedLufsFromMomentary (const std::vector<float>& momentaryLufs);

struct RangeLoudness
{
    bool ok = false;
    juce::String error;
    double lufs = kSilentDb;
    int windows = 0;
    double fromSeconds = 0.0, toSeconds = 0.0;
};

/** Decodes only [from,to] of a file and measures it with the normal engine.

    Runs on the calling thread; callers should use ReferenceRangeMeter when the
    range can be long. Returns a silent / not-ok result instead of guessing when
    the file, the range or the decoder is unusable.
*/
RangeLoudness measureFileRangeLufs (const juce::File& file,
                                    double fromSeconds, double toSeconds);

/** One-shot background measurement for the selection match.

    The UI starts it and polls takeResult() from its timer, so a long selection
    never blocks the message thread. Only one measurement runs at a time; a new
    request replaces the pending one.
*/
class ReferenceRangeMeter : private juce::Thread
{
public:
    ReferenceRangeMeter();
    ~ReferenceRangeMeter() override;

    void start (const juce::File& file, double fromSeconds, double toSeconds);
    void cancel();

    bool isBusy() const noexcept { return busy.load (std::memory_order_acquire); }
    bool takeResult (RangeLoudness& result);

private:
    void run() override;

    mutable juce::SpinLock lock;
    juce::File pendingFile;
    double pendingFrom = 0.0, pendingTo = 0.0;
    std::atomic<unsigned> request { 0 };
    std::atomic<bool> busy { false };
    bool hasResult = false;
    RangeLoudness result;
};
} // namespace ml
