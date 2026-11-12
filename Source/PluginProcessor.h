#pragma once
#include "album/AlbumSession.h"
#include "dsp/CoverageTracker.h"

#include <array>
#include <atomic>
#include <cstdint>

#include <juce_audio_processors/juce_audio_processors.h>

#include "dsp/AnalysisEngine.h"
#include "dsp/SpectrumAnalyzer.h"
#include "dsp/SpectrumFifo.h"
#include "audio/AuditionEngine.h"
#include "audio/FilePreviewPlayer.h"
#include "audio/ReferencePlayer.h"
#include "audio/StreamingPreviewPlayer.h"

class MasterLensAudioProcessor : public juce::AudioProcessor
{
public:
    MasterLensAudioProcessor();
    ~MasterLensAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "MasterLens"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "MasterLens"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    ml::Stats stats;
    ml::AnalysisEngine engine;
    ml::SpectrumRing spectrumRing;
    ml::SpectrumFifo outputSpectrumFifo;
    ml::AuditionEngine auditionEngine;
    ml::ReferencePlayer referencePlayer;
    ml::StreamingPreviewPlayer streamingPreview;
    ml::StreamingPreviewPlayer codecPreview;
    std::atomic<int> codecSide { 0 };
    std::atomic<double> codecDelaySeconds { 0.0 };
    std::atomic<bool> codecDiffMode { false };
    std::atomic<float> codecDiffMatchDb { 0.0f };
    ml::album::Session albumSession;
    ml::FilePreviewPlayer filePreviewPlayer;
    std::atomic<bool> hostTransportPlaying { false };

    ml::StatsSnapshot getStatsSnapshot() const { return ml::snapshot (stats); }
    void requestReset() { resetRequested.store (true); }
    void copyShortTermCurve (std::vector<float>& dest, int maxPoints) const
    {
        engine.copyRecentShortTerm (dest, maxPoints);
    }

    float getTargetLufs() const;
    float getCeilingDb() const;
    float getSmoothing() const;
    double getSampleRateValue() const noexcept { return currentSampleRate; }

    /** Known project/track length for coverage. Zero means "not specified". */
    void setTrackLengthSeconds (double seconds) noexcept
    {
        trackLengthSeconds.store (juce::jmax (0.0, seconds), std::memory_order_relaxed);
    }

    double getTrackLengthSeconds() const noexcept
    {
        return trackLengthSeconds.load (std::memory_order_relaxed);
    }

    bool isMonitorMatch() const { return getBool ("monitorMatch"); }
    float getMonitorTrimDb() const { return stats.monitorTrimDb.load(); }
    void setMonitorTargetLufs (float lufs) noexcept
    {
        monitorTargetLufs.store (juce::jlimit (-36.0f, 0.0f, lufs), std::memory_order_relaxed);
    }

    void startAudition (double startSeconds, double endSeconds, float lowHz, float highHz, bool monoFold)
    {
        auditionEngine.start (startSeconds, endSeconds, lowHz, highHz, monoFold);
    }

    void startLiveAudition (float lowHz, float highHz, bool monoFold)
    {
        auditionEngine.startLive (lowHz, highHz, monoFold);
    }

    /** 4th-order band solo: real band level, no makeup gain. */
    void startBandSolo (float lowHz, float highHz, bool monoFold = false)
    {
        auditionEngine.startLiveBand (lowHz, highHz, monoFold, 4, false);
    }

    void startFilePreview (const juce::File& file, double start, double end,
                           bool loop, bool feedAnalysis = false)
    {
        filePreviewPlayer.start (file, start, end, loop, feedAnalysis);
    }

    void stopFilePreview() { filePreviewPlayer.stop(); }
    void seekFilePreview (double seconds) { filePreviewPlayer.seek (seconds); }
    bool isFilePreviewActive() const noexcept { return filePreviewPlayer.isActive(); }
    bool filePreviewFeedsAnalysis() const noexcept { return filePreviewPlayer.feedsAnalysis(); }
    double getFilePreviewPosition() const noexcept { return filePreviewPlayer.getPositionSeconds(); }
    double getFilePreviewDuration() const noexcept { return filePreviewPlayer.getDurationSeconds(); }
    juce::String getFilePreviewStatus() const { return filePreviewPlayer.getStatus(); }

    // Long-file transport: constant memory, lock-free render.
    void openStreamingPreview (const juce::File& file, bool shouldLoop)
    {
        streamingPreview.open (file, shouldLoop);
    }

    void startStreamingPreview (double fromSeconds) { streamingPreview.start (fromSeconds); }
    void stopStreamingPreview() { streamingPreview.stop(); }
    void seekStreamingPreview (double seconds) { streamingPreview.seek (seconds); }
    bool isStreamingPreviewActive() const noexcept { return streamingPreview.isActive(); }
    bool isStreamingPreviewBuffering() const noexcept { return streamingPreview.isBuffering(); }
    double getStreamingPosition() const noexcept { return streamingPreview.getPositionSeconds(); }
    double getStreamingDuration() const noexcept { return streamingPreview.getDurationSeconds(); }
    juce::String getStreamingStatus() const { return streamingPreview.getStatus(); }

    // Codec A/B: side 0 is the source file, side 1 the decoded preview.
    void openCodecPreview (const juce::File& file) { codecPreview.open (file, false); }
    void startCodecPreview (double fromSeconds) { codecPreview.start (fromSeconds); }
    /** 3.3 differential audition: A and B play together, output = (A-B). */
    void startCodecDiff();
    void stopCodecDiff();
    bool isCodecDiffActive() const noexcept { return codecDiffMode.load (std::memory_order_relaxed); }

    void setCodecDiffMatchDb (float db) noexcept { codecDiffMatchDb.store (juce::jlimit (-12.0f, 12.0f, db), std::memory_order_relaxed); }

    void setCodecDelaySeconds (double seconds) noexcept { codecDelaySeconds.store (juce::jmax (0.0, seconds), std::memory_order_relaxed); }
    void stopCodecPreview() { codecPreview.stop(); codecSide.store (0); }
    bool isCodecPreviewActive() const noexcept { return codecPreview.isActive(); }
    bool isCodecPreviewBuffering() const noexcept { return codecPreview.isBuffering(); }
    double getCodecPosition() const noexcept { return codecPreview.getPositionSeconds(); }
    double getCodecDuration() const noexcept { return codecPreview.getDurationSeconds(); }
    int getCodecSide() const noexcept { return codecSide.load (std::memory_order_relaxed); }
    void setCodecSide (int side);

    void stopAudition() { auditionEngine.stop(); }
    bool isAuditionActive() const noexcept { return auditionEngine.isActive(); }
    bool isAuditionLive() const noexcept { return auditionEngine.isLive(); }
    float getAuditionLowHz() const noexcept { return auditionEngine.lowHz(); }
    float getAuditionHighHz() const noexcept { return auditionEngine.highHz(); }
    bool isAuditionMono() const noexcept { return auditionEngine.mono(); }

    int consumePendingUiCommand() { return pendingUiCommand.exchange (0); }
    bool autoResetOnStart() const { return getBool ("autoResetOnStart"); }
    bool autoScanOnStop() const { return getBool ("autoScanOnStop"); }
    bool autoReportOnStop() const { return getBool ("autoReportOnStop"); }

private:
    bool getBool (const char* id) const
    {
        if (auto* p = apvts.getRawParameterValue (id))
            return p->load() > 0.5f;

        return false;
    }

    void resetHostCoverage() noexcept;
    void markHostCoverage (double startSeconds, double endSeconds) noexcept;
    ml::CoverageTracker coverageTracker;
    /** Unique regions of the standalone file transport that reached analysis. */
    ml::CoverageTracker transportCoverage;
    bool wasTransportActive = false;

    std::atomic<int> pendingUiCommand { 0 };
    std::atomic<float> monitorTargetLufs { -14.0f };
    int lastTransportPlaying = -1;
    int lastCommandValue = 0;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    std::atomic<bool> resetRequested { false };
    std::atomic<double> trackLengthSeconds { 0.0 };
    double currentSampleRate = 48000.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MasterLensAudioProcessor)
};
