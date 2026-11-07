#pragma once

#include <atomic>

#include <juce_audio_formats/juce_audio_formats.h>

#include "FileAnalyzer.h"

namespace ml
{
struct CodecABResult
{
    bool ok = false;
    juce::String error;
    juce::String codec;       // "aac" / "mp3"
    juce::String label;       // "AAC 256k"
    juce::File decodedFile;
    juce::int64 encodedBytes = 0;
    float decodedLufs = kSilentDb;
    float decodedTp = kSilentDb;
    juce::String backend;
    juce::String ffmpegVersion;
};

/** Background encode -> decode -> measure for the file QC A/B.

    The decoded WAV stays on disk so StreamingPreviewPlayer can play it; the
    caller deletes it through clear(). Missing ffmpeg is reported as an error,
    never replaced by a fake result.
*/
class CodecABJob : private juce::Thread
{
public:
    CodecABJob();
    ~CodecABJob() override;

    void start (const juce::File& source, const juce::String& codec);
    void cancel();

    /** Stops the worker and deletes the temporary encoded/decoded files. */
    void clear();

    bool isBusy() const noexcept { return busy.load (std::memory_order_acquire); }
    double getProgress() const noexcept { return progress.load (std::memory_order_relaxed); }
    juce::String getStatus() const;
    bool takeResult (CodecABResult& out);
    juce::File getDecodedFile() const;

private:
    void run() override;

    mutable juce::SpinLock lock;
    juce::File pendingSource;
    juce::String pendingCodec;
    juce::File workDirectory;
    juce::String statusText { "idle" };
    std::atomic<bool> busy { false };
    std::atomic<bool> pending { false };
    std::atomic<bool> cancelFlag { false };
    std::atomic<double> progress { 0.0 };
    std::atomic<unsigned> request { 0 };
    bool hasResult = false;
    CodecABResult result;
};
} // namespace ml
