#include "RegionLoudness.h"

#include <cmath>

namespace ml
{
float gatedLufsFromMomentary (const std::vector<float>& momentaryLufs)
{
    std::vector<double> energy;
    energy.reserve (momentaryLufs.size());

    for (float lufs : momentaryLufs)
        if (lufs > -99.0f)
            energy.push_back (lufsToEnergy (lufs));

    if (energy.empty())
        return kSilentDb;

    // Absolute gate: every 400 ms window below -70 LUFS is discarded.
    double absoluteSum = 0.0;
    int absoluteCount = 0;

    for (double e : energy)
        if (energyToLufs (e) > -70.0f)
        {
            absoluteSum += e;
            ++absoluteCount;
        }

    if (absoluteCount == 0)
        return kSilentDb;

    // Relative gate: -10 LU below the absolute-gated mean.
    const double relativeThreshold = energyToLufs (absoluteSum / (double) absoluteCount) - 10.0;
    double gatedSum = 0.0;
    int gatedCount = 0;

    for (double e : energy)
    {
        const double l = energyToLufs (e);

        if (l > -70.0 && l > relativeThreshold)
        {
            gatedSum += e;
            ++gatedCount;
        }
    }

    if (gatedCount == 0)
        return kSilentDb;

    return energyToLufs (gatedSum / (double) gatedCount);
}

RangeLoudness measureFileRangeLufs (const juce::File& file,
                                    double fromSeconds, double toSeconds)
{
    RangeLoudness result;
    result.fromSeconds = juce::jmax (0.0, juce::jmin (fromSeconds, toSeconds));
    result.toSeconds = juce::jmax (result.fromSeconds, juce::jmax (fromSeconds, toSeconds));

    if (! file.existsAsFile())
    {
        result.error = "missing";
        return result;
    }

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));

    if (reader == nullptr || reader->lengthInSamples <= 1 || reader->numChannels < 1
        || reader->numChannels > 2 || reader->sampleRate < 1000.0 || reader->sampleRate > 768000.0)
    {
        result.error = "unsupported";
        return result;
    }

    const double rate = reader->sampleRate;
    const double duration = (double) reader->lengthInSamples / rate;
    result.fromSeconds = juce::jlimit (0.0, juce::jmax (0.0, duration - 0.1), result.fromSeconds);
    result.toSeconds = juce::jlimit (result.fromSeconds + 0.1, duration, result.toSeconds);

    if (result.toSeconds - result.fromSeconds < 0.4)
    {
        result.error = "range too short";
        return result;
    }

    const int channels = juce::jlimit (1, 2, (int) reader->numChannels);
    const int hop = juce::jmax (64, (int) std::round (rate * 0.1));
    const juce::int64 first = (juce::int64) std::round (result.fromSeconds * rate);
    const juce::int64 last = juce::jmin (reader->lengthInSamples,
                                        (juce::int64) std::round (result.toSeconds * rate));

    AnalysisEngine engine;
    Stats stats;
    resetStats (stats);
    engine.prepare (rate, channels);

    juce::AudioBuffer<float> block (channels, hop);
    std::vector<float> windows;

    for (juce::int64 position = first; position < last; position += hop)
    {
        const int n = (int) juce::jmin ((juce::int64) hop, last - position);
        block.clear();

        if (! reader->read (&block, 0, n, position, true, true))
        {
            result.error = "decode failed";
            return result;
        }

        engine.process (block, stats, n);
        const float momentary = snapshot (stats).momentary;

        if (momentary > -99.0f)
            windows.push_back (momentary);
    }

    engine.finish (stats);
    result.windows = (int) windows.size();
    result.lufs = gatedLufsFromMomentary (windows);
    result.ok = result.lufs > -70.0f;
    return result;
}

ReferenceRangeMeter::ReferenceRangeMeter()
    : juce::Thread ("MasterLens reference range meter")
{
}

ReferenceRangeMeter::~ReferenceRangeMeter()
{
    signalThreadShouldExit();
    notify();
    stopThread (4000);
}

void ReferenceRangeMeter::start (const juce::File& file, double fromSeconds, double toSeconds)
{
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        pendingFile = file;
        pendingFrom = fromSeconds;
        pendingTo = toSeconds;
        hasResult = false;
    }

    busy.store (true, std::memory_order_release);
    request.fetch_add (1);

    if (! isThreadRunning())
        startThread();
    else
        notify();
}

void ReferenceRangeMeter::cancel()
{
    request.fetch_add (1);
    busy.store (false, std::memory_order_release);

    const juce::SpinLock::ScopedLockType sl (lock);
    hasResult = false;
}

bool ReferenceRangeMeter::takeResult (RangeLoudness& out)
{
    const juce::SpinLock::ScopedLockType sl (lock);

    if (! hasResult)
        return false;

    out = result;
    hasResult = false;
    return true;
}

void ReferenceRangeMeter::run()
{
    unsigned handled = 0;

    while (! threadShouldExit())
    {
        const auto serial = request.load();

        if (serial == handled)
        {
            wait (40);
            continue;
        }

        handled = serial;
        juce::File file;
        double from = 0.0, to = 0.0;

        {
            const juce::SpinLock::ScopedLockType sl (lock);
            file = pendingFile;
            from = pendingFrom;
            to = pendingTo;
        }

        auto measured = measureFileRangeLufs (file, from, to);

        if (request.load() != serial)
            continue;

        {
            const juce::SpinLock::ScopedLockType sl (lock);
            result = measured;
            hasResult = true;
        }

        busy.store (false, std::memory_order_release);
    }
}
} // namespace ml
