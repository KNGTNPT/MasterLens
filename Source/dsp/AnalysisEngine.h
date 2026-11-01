#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <cmath>
#include <vector>

namespace ml
{
inline constexpr float kSilentDb = -100.0f;

inline float energyToLufs (double energy) noexcept
{
    if (energy <= 1.0e-12)
        return kSilentDb;

    return (float) (-0.691 + 10.0 * std::log10 (energy));
}

inline double lufsToEnergy (double lufs) noexcept
{
    return std::pow (10.0, (lufs + 0.691) / 10.0);
}

//==============================================================================
/** Live measurement values shared between the audio thread and the UI.

    Every field is a lock-free atomic so the editor can poll at frame rate without
    ever blocking the audio callback.
*/
struct Stats
{
    std::atomic<float> integrated   { kSilentDb };
    std::atomic<float> momentary    { kSilentDb };
    std::atomic<float> shortTerm    { kSilentDb };
    std::atomic<float> maxMomentary { kSilentDb };
    std::atomic<float> maxShortTerm { kSilentDb };
    std::atomic<float> lra          { 0.0f };
    std::atomic<float> truePeakDb   { kSilentDb };
    std::atomic<float> samplePeakDb { kSilentDb };
    std::atomic<float> correlation  { 0.0f };
    std::atomic<float> widthPct     { 0.0f };
    std::atomic<float> lowSidePct   { 0.0f };
    std::atomic<float> dcOffsetDb   { kSilentDb };
    std::atomic<int>   clipEvents   { 0 };
    std::atomic<int>   clickEvents  { 0 };
    std::atomic<int>   oversCount   { 0 };
    std::atomic<double> elapsed     { 0.0 };
    std::atomic<float> tailSilenceSeconds { 0.0f }; // trailing digital silence at -60 dBFS
    std::atomic<bool>  stereo       { false };
    std::atomic<bool>  hasSignal    { false };

    // --- v2.0 host timeline / monitoring --------------------------------
    std::atomic<double> timeOrigin   { 0.0 };  // host playhead at reset
    std::atomic<double> hostPosition { 0.0 };
    std::atomic<double> hostLength   { 0.0 };
    std::atomic<float>  coveragePct  { 100.0f };
    std::atomic<float>  monitorTrimDb { 0.0f };
    std::atomic<float>  truePeakLeft  { kSilentDb };
    std::atomic<float>  truePeakRight { kSilentDb };
    std::atomic<float>  sideTruePeak  { kSilentDb };

    // --- v1.2 -----------------------------------------------------------
    std::atomic<float>  psr            { kSilentDb };   // peak-to-short-term ratio
    std::atomic<float>  psrMin         { kSilentDb };
    std::atomic<double> psrMinTime     { 0.0 };
    std::atomic<float>  truePeakMaxTime{ 0.0f };        // seconds
    std::atomic<int>    spikeCount     { 0 };
    std::atomic<double> lastSpikeTime  { 0.0 };
    std::atomic<float>  bandCorrLow    { 0.0f };
    std::atomic<float>  bandCorrMid    { 0.0f };
    std::atomic<float>  bandCorrHigh   { 0.0f };
    std::atomic<float>  bandSideLow    { 0.0f };        // side/mid, %
    std::atomic<float>  bandSideMid    { 0.0f };
    std::atomic<float>  bandSideHigh   { 0.0f };

    // --- v1.4.2 signal integrity ----------------------------------------
    std::atomic<float>  monoFoldLossDb   { 0.0f };     // level lost in a mono fold
    std::atomic<float>  monoRetainPct    { 100.0f };    // amplitude kept after fold
    std::atomic<float>  monoLowRetainPct { 100.0f };    // same, below 150 Hz
    std::atomic<float>  subSharePct      { 0.0f };      // < 40 Hz share of total energy
    std::atomic<float>  flatTopPct       { 0.0f };      // % of samples on a flat top

    // --- v2.0 beta2: independent mid / side loudness ---------------------
    std::atomic<float>  midLufs          { kSilentDb };
    std::atomic<float>  sideLufs         { kSilentDb };
    std::atomic<float>  midShortTerm     { kSilentDb };
    std::atomic<float>  sideShortTerm    { kSilentDb };
};


/** One true-peak over-shoot, recorded on the audio thread without allocating. */
struct TpEvent
{
    double time = 0.0;
    float  overshootDb = 0.0f;
};

/** Fixed-size ring for true-peak events (single producer, no allocation). */
class TpEventRing
{
public:
    static constexpr int capacity = 64;

    void push (const TpEvent& e) noexcept
    {
        data[write] = e;
        write = (write + 1) % capacity;

        if (count < capacity)
            ++count;

        version.fetch_add (1, std::memory_order_release);
    }

    void reset() noexcept
    {
        write = 0;
        count = 0;
        version.fetch_add (1, std::memory_order_release);
    }

    int size() const noexcept { return count; }

    /** Oldest first. Safe enough for display: the reader validates the version. */
    TpEvent get (int i) const noexcept
    {
        const int index = (write - count + i + capacity * 4) % capacity;
        return data[index];
    }

    unsigned int getVersion() const noexcept { return version.load (std::memory_order_acquire); }

private:
    TpEvent data[capacity] {};
    int write = 0;
    int count = 0;
    std::atomic<unsigned int> version { 0 };
};

struct StatsSnapshot
{
    float integrated   = kSilentDb;
    float momentary    = kSilentDb;
    float shortTerm    = kSilentDb;
    float maxMomentary = kSilentDb;
    float maxShortTerm = kSilentDb;
    float lra          = 0.0f;
    float truePeakDb   = kSilentDb;
    float samplePeakDb = kSilentDb;
    float correlation  = 0.0f;
    float widthPct     = 0.0f;
    float lowSidePct   = 0.0f;
    float dcOffsetDb   = kSilentDb;
    int   clipEvents   = 0;
    int   clickEvents  = 0;
    int   oversCount   = 0;
    double elapsed     = 0.0;
    float tailSilenceMs = 0.0f;
    bool  stereo       = false;
    bool  hasSignal    = false;

    float  psr = kSilentDb;
    float  psrMin = kSilentDb;
    double psrMinTime = 0.0;
    float  truePeakMaxTime = 0.0f;
    int    spikeCount = 0;
    double lastSpikeTime = 0.0;
    float  bandCorrLow = 0.0f, bandCorrMid = 0.0f, bandCorrHigh = 0.0f;
    float  bandSideLow = 0.0f, bandSideMid = 0.0f, bandSideHigh = 0.0f;

    float  monoFoldLossDb = 0.0f;
    float  monoRetainPct = 100.0f;
    float  monoLowRetainPct = 100.0f;
    float  subSharePct = 0.0f;
    float  flatTopPct = 0.0f;
    float  midLufs = kSilentDb;
    float  sideLufs = kSilentDb;
    float  midShortTerm = kSilentDb;
    float  sideShortTerm = kSilentDb;

    // Host timeline / monitoring (v2.0)
    double timeOrigin = 0.0;
    double hostPosition = 0.0;
    double hostLength = 0.0;
    float  coveragePct = 100.0f;
    float  monitorTrimDb = 0.0f;
    float  truePeakLeft = kSilentDb;
    float  truePeakRight = kSilentDb;
    float  sideTruePeak = kSilentDb;

    float getPlr() const noexcept { return integrated > -70.0f ? juce::jmax (0.0f, truePeakDb - integrated) : 0.0f; }
};

inline StatsSnapshot snapshot (const Stats& s) noexcept
{
    StatsSnapshot r;
    r.integrated   = s.integrated.load();
    r.momentary    = s.momentary.load();
    r.shortTerm    = s.shortTerm.load();
    r.maxMomentary = s.maxMomentary.load();
    r.maxShortTerm = s.maxShortTerm.load();
    r.lra          = s.lra.load();
    r.truePeakDb   = s.truePeakDb.load();
    r.samplePeakDb = s.samplePeakDb.load();
    r.correlation  = s.correlation.load();
    r.widthPct     = s.widthPct.load();
    r.lowSidePct   = s.lowSidePct.load();
    r.dcOffsetDb   = s.dcOffsetDb.load();
    r.clipEvents   = s.clipEvents.load();
    r.clickEvents  = s.clickEvents.load();
    r.oversCount   = s.oversCount.load();
    r.elapsed      = s.elapsed.load();
    r.tailSilenceMs = s.tailSilenceSeconds.load() * 1000.0f;
    r.stereo       = s.stereo.load();
    r.hasSignal    = s.hasSignal.load();
    r.timeOrigin   = s.timeOrigin.load();
    r.hostPosition = s.hostPosition.load();
    r.hostLength   = s.hostLength.load();
    r.coveragePct  = s.coveragePct.load();
    r.monitorTrimDb = s.monitorTrimDb.load();
    r.truePeakLeft = s.truePeakLeft.load();
    r.truePeakRight = s.truePeakRight.load();
    r.sideTruePeak = s.sideTruePeak.load();
    r.midLufs = s.midLufs.load();
    r.sideLufs = s.sideLufs.load();
    r.midShortTerm = s.midShortTerm.load();
    r.sideShortTerm = s.sideShortTerm.load();
    r.psr = s.psr.load();
    r.psrMin = s.psrMin.load();
    r.psrMinTime = s.psrMinTime.load();
    r.truePeakMaxTime = s.truePeakMaxTime.load();
    r.spikeCount = s.spikeCount.load();
    r.lastSpikeTime = s.lastSpikeTime.load();
    r.bandCorrLow = s.bandCorrLow.load();
    r.bandCorrMid = s.bandCorrMid.load();
    r.bandCorrHigh = s.bandCorrHigh.load();
    r.bandSideLow = s.bandSideLow.load();
    r.bandSideMid = s.bandSideMid.load();
    r.bandSideHigh = s.bandSideHigh.load();
    r.monoFoldLossDb = s.monoFoldLossDb.load();
    r.monoRetainPct = s.monoRetainPct.load();
    r.monoLowRetainPct = s.monoLowRetainPct.load();
    r.subSharePct = s.subSharePct.load();
    r.flatTopPct = s.flatTopPct.load();
    return r;
}

inline void resetStats (Stats& s) noexcept
{
    s.integrated = kSilentDb;  s.momentary = kSilentDb;  s.shortTerm = kSilentDb;
    s.maxMomentary = kSilentDb; s.maxShortTerm = kSilentDb; s.lra = 0.0f;
    s.truePeakDb = kSilentDb;  s.samplePeakDb = kSilentDb;
    s.correlation = 0.0f;      s.widthPct = 0.0f;        s.lowSidePct = 0.0f;
    s.dcOffsetDb = kSilentDb;  s.clipEvents = 0;         s.clickEvents = 0;
    s.oversCount = 0;          s.elapsed = 0.0;          s.hasSignal = false;
    s.tailSilenceSeconds = 0.0f;
    s.timeOrigin = 0.0;        s.hostPosition = 0.0;     s.hostLength = 0.0;
    s.coveragePct = 100.0f;    s.monitorTrimDb = 0.0f;
    s.truePeakLeft = kSilentDb; s.truePeakRight = kSilentDb; s.sideTruePeak = kSilentDb;
    s.midLufs = kSilentDb;      s.sideLufs = kSilentDb;
    s.midShortTerm = kSilentDb; s.sideShortTerm = kSilentDb;
    s.psr = kSilentDb;         s.psrMin = kSilentDb;     s.psrMinTime = 0.0;
    s.truePeakMaxTime = 0.0f;  s.spikeCount = 0;         s.lastSpikeTime = 0.0;
    s.bandCorrLow = 0.0f;      s.bandCorrMid = 0.0f;     s.bandCorrHigh = 0.0f;
    s.bandSideLow = 0.0f;      s.bandSideMid = 0.0f;     s.bandSideHigh = 0.0f;
    s.monoFoldLossDb = 0.0f;   s.monoRetainPct = 100.0f; s.monoLowRetainPct = 100.0f;
    s.subSharePct = 0.0f;      s.flatTopPct = 0.0f;
}

//==============================================================================
/** Direct-form-I biquad, double precision state (used at 44.1/48/96 kHz alike). */
struct Biquad
{
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
    double z1 = 0.0, z2 = 0.0;

    inline float process (float x) noexcept
    {
        const double in  = (double) x;
        const double out = b0 * in + z1;
        z1 = b1 * in - a1 * out + z2;
        z2 = b2 * in - a2 * out;
        return (float) out;
    }

    void reset() noexcept { z1 = z2 = 0.0; }

    void setCoefficients (double nb0, double nb1, double nb2, double na1, double na2) noexcept
    {
        b0 = nb0; b1 = nb1; b2 = nb2; a1 = na1; a2 = na2;
    }

    /** RBJ low pass, used for the low-frequency stereo image check. */
    void designLowPass (double sampleRate, double freq, double q) noexcept;

    /** RBJ high pass. */
    void designHighPass (double sampleRate, double freq, double q) noexcept;
};

//==============================================================================
/** 4x oversampling true-peak detector (windowed-sinc polyphase interpolator).

    Peak magnitude is independent of the group delay, so the plugin stays
    latency-free while still catching inter-sample overs.
*/
class TruePeakDetector
{
public:
    void prepare (double sampleRate);
    void reset() noexcept;

    /** Returns the maximum absolute interpolated value for this input sample. */
    inline float process (float x) noexcept
    {
        history[(size_t) head] = x;
        head = (head + 1) & (historySize - 1);

        float maxAbs = std::abs (x);
        int idx = head - 1;

        for (int p = 0; p < phases; ++p)
        {
            float acc = 0.0f;
            int i = idx;
            const float* c = coeffs[p];

            for (int j = 0; j < tapsPerPhase; ++j)
            {
                acc += c[j] * history[(size_t) (i & (historySize - 1))];
                --i;
            }

            maxAbs = juce::jmax (maxAbs, std::abs (acc));
        }

        return maxAbs;
    }

private:
    static constexpr int phases = 4;
    static constexpr int tapsPerPhase = 24;
    static constexpr int historySize = 32;   // power of two, >= tapsPerPhase

    float coeffs[phases][tapsPerPhase] {};
    float history[historySize] {};
    int head = 0;
};

//==============================================================================
/** BS.1770-4 loudness (I / M / S / LRA) plus mix diagnostics.

    All heavy lifting happens on the audio thread but only involves a handful of
    multiplies per sample. Results are published through Stats, which the editor
    polls at 60 fps.
*/
class AnalysisEngine
{
public:
    static constexpr int   numBands = 10;
    static constexpr int   curveSize = 4096;      // 100 ms points ≈ 6.8 minutes
    static constexpr float histMinDb = -70.0f;
    static constexpr float histStepDb = 0.1f;
    static constexpr int   histBins = 780;

    void prepare (double sampleRate, int numChannels);
    void reset();
    void setTimeOrigin (double seconds) noexcept { timeOrigin = seconds; }

    /** Analyses a block in place. Audio is never modified. */
    void process (const juce::AudioBuffer<float>& buffer, Stats& stats, int numSamples);

    /** Flushes the final partial 100 ms block after an offline file pass. */
    void finish (Stats& stats);

    /** Copies the most recent short-term (3 s) loudness points, oldest first. */
    void copyRecentShortTerm (std::vector<float>& dest, int maxPoints) const;

    const TpEventRing& getTpEvents() const noexcept { return tpEventRing; }

private:
    void finishPartialBlock (Stats& stats);
    float computeIntegrated() const;
    float computeIntegratedHist (const std::atomic<unsigned int>* histogram) const;
    float computeLra() const;

    double fs = 48000.0;
    int channels = 2;

    Biquad kShelf[2], kHighPass[2];
    Biquad lowPassA[2], lowPassB[2];      // low band, LR4 low pass @ 150 Hz
    Biquad hp150A[2], hp150B[2];          // LR4 high pass @ 150 Hz
    Biquad lp2kA[2], lp2kB[2];            // LR4 low pass @ 2 kHz
    Biquad hp2kA[2], hp2kB[2];            // LR4 high pass @ 2 kHz
    Biquad subLpA[2], subLpB[2];          // LR4 low pass @ 40 Hz (sub-bass share)
    TruePeakDetector truePeak[2];
    TruePeakDetector sideTruePeakDetector;

    // per-band accumulators: 0 = <150 Hz, 1 = 150 Hz..2 kHz, 2 = >2 kHz
    double bandL2[3] {}, bandR2[3] {}, bandLR[3] {}, bandMid[3] {}, bandSide[3] {};

    double peakRing[30] {};
    int peakRingPos = 0;
    int peakRingCount = 0;
    double lastSpikeSecond = -100.0;
    double lastTpEventTime = -100.0;

    TpEventRing tpEventRing;

    int partialLength = 4800;
    int partialSamples = 0;

    double sumWeightedK = 0.0;
    double sumLR = 0.0, sumL2 = 0.0, sumR2 = 0.0;
    double sumL = 0.0, sumR = 0.0;
    double sumMid = 0.0, sumSide = 0.0;
    double sumMidK = 0.0, sumSideK = 0.0;
    double sumLowMid = 0.0, sumLowSide = 0.0;
    double sumSubL2 = 0.0, sumSubR2 = 0.0;

    double partialFlatTop = 0.0;
    double partialProcessed = 0.0;
    float  flatTopSmooth = 0.0f;
    float  subShareSmooth = 0.0f;
    float  monoRetainSmooth = 100.0f;
    float  monoLowRetainSmooth = 100.0f;
    float  prevAbsSp = 0.0f;

    double partialMaxTruePeak = 0.0;
    double partialMaxTruePeakL = 0.0;
    double partialMaxTruePeakR = 0.0;
    double partialMaxSideTruePeak = 0.0;
    double partialMaxSamplePeak = 0.0;
    float  prevL = 0.0f, prevR = 0.0f;
    float  clickAverage = 0.0f;

    // true DC offset: mean of the 100 ms block means over the last 4 seconds
    double dcRingL[40] {};
    double dcRingR[40] {};
    int    dcRingPos = 0;
    int    dcRingCount = 0;

    bool inClip = false;

    // trailing partial-block energies (100 ms each) for M / S windows
    double partialRing[64] {};
    int    ringWrite = 0;
    int    ringCount = 0;
    int    partialCounter = 0;

    std::atomic<unsigned int> blockHist[histBins] {};
    std::atomic<unsigned int> shortHist[histBins] {};
    std::atomic<unsigned int> shortHistCount { 0 };
    std::atomic<unsigned int> midBlockHist[histBins] {};
    std::atomic<unsigned int> sideBlockHist[histBins] {};
    double midRing[64] {};
    double sideRing[64] {};
    int midRingWrite = 0;
    int sideRingWrite = 0;

    float shortCurve[curveSize] {};
    std::atomic<int> curveWrite { 0 };

    juce::uint32 elapsedSamples = 0;
    double timeOrigin = 0.0;
};
} // namespace ml
