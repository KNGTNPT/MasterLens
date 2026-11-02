#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include "dsp/AnalysisEngine.h"

// Test-only headless probe: bypasses file-QC FFT/report heuristics, exercises
// exactly the real-time measurement engine with deterministic random buffers.
inline int runMeasurementProbe (const char* path, int seed)
{
    juce::AudioFormatManager formats; formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (juce::File (path)));
    if (! reader || reader->numChannels < 1 || reader->numChannels > 2) return 1;
    ml::AnalysisEngine engine; ml::Stats stats; ml::resetStats (stats);
    engine.prepare (reader->sampleRate, (int) reader->numChannels);
    juce::AudioBuffer<float> buffer ((int) reader->numChannels, 8192);
    juce::Random random (seed);
    for (juce::int64 offset = 0; offset < reader->lengthInSamples;)
    {
        const int n = (int) juce::jmin ((juce::int64) (seed == 0 ? 256 : 1 + random.nextInt (8192)), reader->lengthInSamples - offset);
        if (! reader->read (&buffer, 0, n, offset, true, true)) return 1;
        engine.process (buffer, stats, n); offset += n;
    }
    engine.finish (stats);
    const auto s = ml::snapshot (stats);
    juce::var result (new juce::DynamicObject()); auto* o = result.getDynamicObject();
    o->setProperty ("I", s.integrated); o->setProperty ("M", s.maxMomentary);
    o->setProperty ("S", s.maxShortTerm); o->setProperty ("LRA", s.lra);
    o->setProperty ("TP", s.truePeakDb); o->setProperty ("duration", s.elapsed);
    o->setProperty ("peakTime", s.truePeakMaxTime);
    std::printf ("%s\n", juce::JSON::toString (result, true).toRawUTF8());
    return 0;
}
