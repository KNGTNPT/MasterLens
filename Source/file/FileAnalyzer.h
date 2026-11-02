#pragma once

#include <array>
#include <atomic>
#include <vector>

#include <juce_audio_formats/juce_audio_formats.h>

#include "../dsp/AnalysisEngine.h"
#include "../dsp/DefectAnalyzer.h"
#include "FileForensics.h"

namespace ml
{
/** One offline file / album-folder analysis result.

    The engine is deliberately independent of the audio callback: a file is read
    on a worker thread, processed at its own sample rate, then returned as the
    same StatsSnapshot used by the live meter. That is what makes a four-minute
    track finish in seconds instead of waiting for real time.
*/
struct FileAnalysisResult
{
    juce::String path;
    juce::String fileName;
    juce::String error;

    double sampleRate = 0.0;
    double durationSeconds = 0.0;
    juce::int64 numSamples = 0;
    int bitDepth = 0;
    int numChannels = 0;

    StatsSnapshot stats;
    std::array<float, 10> bandDb {};
    SignalDefects defects;
    FileForensics forensic;

    bool ok = false;
};

// Shared by the GUI sidecar and CLI; one schema for the same measurements.
juce::var fileResultToVar (const FileAnalysisResult&, float target = -14.0f, float ceiling = -1.0f);
juce::String fileResultsCsv (const std::vector<FileAnalysisResult>&, float target = -14.0f, float ceiling = -1.0f);

/** Background WAV / AIFF / FLAC analyser with a tiny thread-safe result queue.

    Results are taken by the editor and can be added to the shared album session
    without touching the audio thread. Only one batch runs at a time.
*/
class FileAnalyzer : private juce::Thread
{
public:
    FileAnalyzer();
    ~FileAnalyzer() override;

    /** Expands directories recursively and starts a batch. Returns false when a
        batch is already running or nothing playable was found. */
    bool analyse (const juce::StringArray& paths);

    void cancel();
    bool isBusy() const noexcept { return busy.load (std::memory_order_acquire); }
    int getTotal() const noexcept { return total.load (std::memory_order_acquire); }
    int getDone() const noexcept { return done.load (std::memory_order_acquire); }
    juce::String getStatus() const;
    juce::String getCurrentFile() const;

    std::vector<FileAnalysisResult> takeResults();

    static bool isSupportedFile (const juce::File& file);
    static juce::StringArray expandPaths (const juce::StringArray& paths);

private:
    void run() override;

    FileAnalysisResult analyseFile (const juce::File& file);

    mutable juce::SpinLock lock;
    juce::StringArray pending;
    juce::String status { "idle" };
    juce::String current;
    std::vector<FileAnalysisResult> results;

    std::atomic<bool> busy { false };
    std::atomic<int> total { 0 };
    std::atomic<int> done { 0 };
};
} // namespace ml
