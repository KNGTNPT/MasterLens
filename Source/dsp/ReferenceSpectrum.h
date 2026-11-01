#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <vector>

#include "AnalysisEngine.h"

namespace ml
{
/** Offline Welch-averaged spectrum of 1-4 reference tracks.

    Runs on a background thread (never on the audio thread) and hands the
    finished spectrum to the UI through a spin-locked snapshot.
*/
class ReferenceSpectrum : private juce::Thread
{
public:
    ReferenceSpectrum();
    ~ReferenceSpectrum() override;

    void load (const juce::StringArray& files, double sampleRate, int fftOrder = 13);
    void clear();

    bool isReady() const noexcept { return ready.load(); }
    bool isBusy() const noexcept { return busy.load(); }
    juce::String getStatus() const;

    /** Averaged spectrum in dB per bin; empty until ready. */
    std::vector<float> getSpectrumDb() const;

    /** Aggregate loudness / dynamics of the reference set (energy weighted). */
    StatsSnapshot getStats() const;
    bool hasStats() const noexcept { return statsReady.load(); }

    juce::StringArray getFiles() const;

    /** 10-20 Hz loudness envelope of the reference set, for coarse alignment. */
    std::vector<float> getEnvelopeDb() const;
    double getEnvelopeHopSeconds() const noexcept { return envelopeHop.load(); }

private:
    void run() override;

    juce::StringArray pendingFiles;
    double pendingSampleRate = 48000.0;
    int pendingOrder = 13;

    mutable juce::SpinLock lock;
    std::vector<float> spectrumDb;
    std::vector<float> envelopeDb;
    std::atomic<double> envelopeHop { 0.1 };
    StatsSnapshot refStats;
    std::atomic<bool> ready { false };
    std::atomic<bool> statsReady { false };
    std::atomic<bool> busy { false };
    juce::String status;
};
} // namespace ml
