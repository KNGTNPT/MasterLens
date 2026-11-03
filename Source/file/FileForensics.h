#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <vector>

namespace ml
{
struct FileFinding
{
    juce::String code;
    int severity = 1; // 1 = review, 2 = blocker; never a diagnosis of musical intent
    juce::int64 startSample = 0, lengthSamples = 0;
    int channel = -1; // -1 = all, otherwise zero-based
    double value = 0.0;
    bool reviewed = false; // UI review state; exported with the evidence
};

struct FileForensics
{
    juce::String evidenceKey (const FileFinding&) const;
    bool measured = false;
    juce::String sha256;
    std::array<double, 2> dcOffset {};
    std::array<juce::int64, 2> fullScaleSamples {}, longestFullScaleRun {};
    juce::int64 firstActiveSample = -1, lastActiveSample = -1;
    double leadingSilenceSeconds = 0.0, trailingSilenceSeconds = 0.0;
    juce::int64 nonFiniteSamples = 0;
    int internalSilenceCount = 0, clippedRunCount = 0, omittedFindings = 0;
    std::array<float, 10> monoLossDb {}, sideEnergyPct {}, energySharePct {};
    std::array<bool, 10> bandAvailable {};
    int worstMonoBand = -1;
    std::vector<FileFinding> findings;

    void add (const FileFinding&);
    int blockers() const;
    juce::var toVar (double sampleRate) const;
};

/** Bounded-memory, sample-addressed QC. Runs only in the offline worker / tests. */
class FileForensicsScanner
{
public:
    FileForensicsScanner (double rate, int channels, int bitDepth);
    bool process (const juce::AudioBuffer<float>&, int count);
    FileForensics finish();

private:
    void closeClip (int channel);
    double rate;
    int channels;
    double fullScaleThreshold;
    juce::int64 position = 0, zeroStart = -1;
    std::array<double, 2> sums {};
    std::array<juce::int64, 2> clipStart { -1, -1 };
    std::array<float, 2> first {}, last {};
    FileForensics result;
};

struct GainBudget
{
    bool valid = false;
    float targetLufs = -14.0f, ceilingDbtp = -1.0f;
    float requestedGainDb = 0.0f, safeGainDb = 0.0f;
    float achievedLufsEstimate = 0.0f, predictedTpDbtp = 0.0f, shortfallLu = 0.0f;
    juce::var toVar() const;
};

GainBudget gainBudget (float integrated, float truePeak, float target, float ceiling);
juce::String findingText (const FileFinding&, bool english);
} // namespace ml
