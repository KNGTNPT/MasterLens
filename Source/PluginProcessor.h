#pragma once

#include <array>
#include <atomic>
#include <cstdint>

#include <juce_audio_processors/juce_audio_processors.h>

#include "dsp/AnalysisEngine.h"
#include "dsp/SpectrumAnalyzer.h"
#include "dsp/SpectrumFifo.h"
#include "audio/AuditionEngine.h"
#include "audio/ReferencePlayer.h"

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

    // Coverage is measured as the union of host-time intervals actually seen,
    // not as elapsed processing time. Looping the same eight bars therefore
    // cannot make a four-minute track look 100 % measured.
    static constexpr double coverageBucketSeconds = 0.1;
    static constexpr int coverageBucketCount = 144000; // 4 hours at 10 Hz
    static constexpr int coverageWordCount = (coverageBucketCount + 63) / 64;

    void resetHostCoverage() noexcept;
    void markHostCoverage (double startSeconds, double endSeconds) noexcept;

    std::array<std::atomic<std::uint64_t>, coverageWordCount> hostCoverage {};
    std::atomic<int> coveredBuckets { 0 };

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
