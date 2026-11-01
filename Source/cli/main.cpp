#include <cstdio>
#include <vector>

#include <juce_core/juce_core.h>

#include "Version.h"
#include "file/FileAnalyzer.h"

namespace
{
juce::var resultToVar (const ml::FileAnalysisResult& r)
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("file", r.fileName);
    o->setProperty ("path", r.path);
    o->setProperty ("ok", r.ok);
    o->setProperty ("error", r.error);
    o->setProperty ("sampleRate", r.sampleRate);
    o->setProperty ("bitDepth", r.bitDepth);
    o->setProperty ("channels", r.numChannels);
    o->setProperty ("durationSeconds", r.durationSeconds);
    o->setProperty ("coveragePct", r.stats.coveragePct);

    auto* s = new juce::DynamicObject();
    s->setProperty ("integratedLufs", r.stats.integrated);
    s->setProperty ("momentaryLufs", r.stats.momentary);
    s->setProperty ("shortTermLufs", r.stats.shortTerm);
    s->setProperty ("lraLu", r.stats.lra);
    s->setProperty ("plrDb", r.stats.getPlr());
    s->setProperty ("truePeakDbtp", r.stats.truePeakDb);
    s->setProperty ("truePeakLeftDbtp", r.stats.truePeakLeft);
    s->setProperty ("truePeakRightDbtp", r.stats.truePeakRight);
    s->setProperty ("sideTruePeakDbtp", r.stats.sideTruePeak);
    s->setProperty ("midIntegratedLufs", r.stats.midLufs);
    s->setProperty ("sideIntegratedLufs", r.stats.sideLufs);
    s->setProperty ("midShortTermLufs", r.stats.midShortTerm);
    s->setProperty ("sideShortTermLufs", r.stats.sideShortTerm);
    s->setProperty ("correlation", r.stats.correlation);
    s->setProperty ("widthPct", r.stats.widthPct);
    s->setProperty ("lowSidePct", r.stats.lowSidePct);
    s->setProperty ("clipEvents", r.stats.clipEvents);
    s->setProperty ("clickEvents", r.stats.clickEvents);
    s->setProperty ("oversCount", r.stats.oversCount);
    o->setProperty ("stats", juce::var (s));

    juce::Array<juce::var> bands;

    for (float b : r.bandDb)
        bands.add (b);

    o->setProperty ("bandsDb", bands);

    auto* d = new juce::DynamicObject();
    d->setProperty ("distortionRisk", r.defects.distortionRisk);
    d->setProperty ("aliasRisk", r.defects.aliasRisk);
    d->setProperty ("subRisk", r.defects.subRisk);
    d->setProperty ("transientScore", r.defects.transientScore);
    d->setProperty ("monoRisk", r.defects.monoRisk);
    d->setProperty ("maskingRisk", r.defects.maskingRisk);
    d->setProperty ("worstRisk", r.defects.worstRisk());
    o->setProperty ("defects", juce::var (d));

    return juce::var (o);
}

void printCsv (const ml::FileAnalysisResult& r)
{
    std::printf ("%s,%s,%.6f,%d,%d,%.3f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f\n",
                 r.fileName.toRawUTF8(), r.ok ? "ok" : "error",
                 r.sampleRate, r.bitDepth, r.numChannels, r.durationSeconds,
                 r.stats.integrated, r.stats.truePeakDb, r.stats.lra,
                 r.stats.getPlr(), r.stats.midLufs, r.stats.sideLufs);
}
} // namespace

int main (int argc, char** argv)
{
    juce::StringArray paths;
    juce::String jsonPath;
    bool csv = false;

    for (int i = 1; i < argc; ++i)
    {
        const juce::String arg (argv[i]);

        if (arg == "--json" && i + 1 < argc)
            jsonPath = argv[++i];
        else if (arg == "--csv")
            csv = true;
        else if (arg == "--help" || arg == "-h")
        {
            std::printf ("MasterLens CLI %s\n"
                         "Usage: MasterLensCli analyze <file-or-folder> [more...] [--json out.json] [--csv]\n",
                         ml::kVersion);
            return 0;
        }
        else if (arg != "analyze")
            paths.add (arg);
    }

    if (paths.isEmpty())
    {
        std::printf ("Nothing to analyse. Use --help for usage.\n");
        return 1;
    }

    ml::FileAnalyzer analyzer;

    if (! analyzer.analyse (paths))
    {
        std::printf ("No supported WAV / AIFF / FLAC files found.\n");
        return 1;
    }

    while (analyzer.isBusy())
        juce::Thread::sleep (50);

    auto results = analyzer.takeResults();

    if (csv)
    {
        std::printf ("file,status,sampleRate,bitDepth,channels,duration,integratedLufs,truePeakDbtp,lraLu,plrDb,midLufs,sideLufs\n");

        for (const auto& r : results)
            printCsv (r);
    }
    else
    {
        for (const auto& r : results)
            std::printf ("%s  %s  %.1f LUFS  %.1f dBTP  %.1f LU  %.0f%% risk\n",
                         r.fileName.toRawUTF8(), r.ok ? "OK" : r.error.toRawUTF8(),
                         r.stats.integrated, r.stats.truePeakDb, r.stats.lra,
                         r.defects.worstRisk());
    }

    if (jsonPath.isNotEmpty())
    {
        auto* root = new juce::DynamicObject();
        root->setProperty ("product", "MasterLens");
        root->setProperty ("version", ml::kVersion);
        root->setProperty ("schemaVersion", 2);
        root->setProperty ("generatedAt", juce::Time::getCurrentTime().toISO8601 (true));

        juce::Array<juce::var> files;

        for (const auto& r : results)
            files.add (resultToVar (r));

        root->setProperty ("files", files);

        juce::File (jsonPath).replaceWithText (juce::JSON::toString (juce::var (root), true));
        std::printf ("JSON: %s\n", jsonPath.toRawUTF8());
    }

    return results.empty() ? 1 : 0;
}
