#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <cerrno>
#include <juce_core/juce_core.h>
#include "Version.h"
#include "file/FileAnalyzer.h"
#include "file/DeliverySpec.h"
#include "file/CodecCheck.h"

namespace
{
bool number (const char* text, float& out)
{
    char* end = nullptr;
    errno = 0;
    const double v = std::strtod (text, &end);
    if (errno != 0 || end == text || *end != '\0' || ! std::isfinite (v)) return false;
    out = (float) v;
    return std::isfinite (out);
}
}

int main (int argc, char** argv)
{
    juce::StringArray paths;
    juce::String jsonPath, failOn, specPath, codec;
    bool csv = false, options = true;
    float target = -14.0f, ceiling = -1.0f;
    ml::DeliverySpec deliverySpec;
    for (int i = 1; i < argc; ++i)
    {
        const juce::String arg (argv[i]);
        if (options && (arg == "--help" || arg == "-h"))
        {
            std::printf ("MasterLens CLI %s\n"
                         "Usage: MasterLensCli analyze <file-or-folder> [more...] [options]\n"
                         "  --json out.json     Full measurements, sample-addressed QC, bands, SHA256\n"
                         "  --csv               Quoted summary to stdout\n"
                         "  --target LUFS       Gain-budget target [-36, -5], default -14\n"
                         "  --ceiling dBTP      Gain-budget ceiling [-12, 0], default -1\n"
                         "  --fail-on error     Exit 2 for QC blockers / peaks over ceiling\n"
                         "  --fail-on warning   Also fail on review findings / mono loss >= 6 dB\n"
                         "  --spec spec.json    Client sample-rate / bit-depth / loudness / naming spec\n"
                         "  --codec aac|mp3     Optional ffmpeg round-trip true-peak recheck\n"
                         "  --                  End options (for paths starting with -)\n"
                         "Exit 0=analysed, 1=input / decode / report error, 2=QC gate failed.\n"
                         "Gain budgets estimate constant gain only; no audio is written.\n", ml::kVersion);
            return 0;
        }
        if (options && arg == "--") { options = false; continue; }
        if (options && arg == "analyze") continue;
        if (options && arg == "--csv") { csv = true; continue; }
        if (options && (arg == "--json" || arg == "--target" || arg == "--ceiling"
                        || arg == "--fail-on" || arg == "--spec" || arg == "--codec"))
        {
            if (++i >= argc) { std::fprintf (stderr, "Missing value for %s\n", arg.toRawUTF8()); return 1; }
            if (arg == "--json") jsonPath = argv[i];
            else if (arg == "--fail-on") failOn = argv[i];
            else if (arg == "--spec") specPath = argv[i];
            else if (arg == "--codec") codec = argv[i];
            else if (! number (argv[i], arg == "--target" ? target : ceiling))
            { std::fprintf (stderr, "Invalid number for %s\n", arg.toRawUTF8()); return 1; }
            continue;
        }
        if (options && arg.startsWithChar ('-'))
        { std::fprintf (stderr, "Unknown option: %s\n", arg.toRawUTF8()); return 1; }
        paths.add (arg);
    }
    if (paths.isEmpty() || target < -36.0f || target > -5.0f || ceiling < -12.0f || ceiling > 0.0f
        || (failOn.isNotEmpty() && failOn != "error" && failOn != "warning"))
    { std::fprintf (stderr, "Invalid input / option range. Use --help.\n"); return 1; }
    for (const auto& path : paths)
        if (ml::FileAnalyzer::expandPaths ({ path }).isEmpty())
        { std::fprintf (stderr, "No supported files at: %s\n", path.toRawUTF8()); return 1; }

    if (codec.isNotEmpty() && codec != "aac" && codec != "mp3")
    {
        std::fprintf (stderr, "--codec must be aac or mp3\n");
        return 1;
    }

    if (specPath.isNotEmpty())
    {
        const juce::File specFile (specPath);
        if (! specFile.existsAsFile())
        { std::fprintf (stderr, "Spec file not found: %s\n", specPath.toRawUTF8()); return 1; }
        const auto parsed = juce::JSON::parse (specFile.loadFileAsString());
        if (auto* root = parsed.getDynamicObject())
        {
            juce::String error;
            if (! ml::DeliverySpec::parse (parsed, deliverySpec, error))
            { std::fprintf (stderr, "%s\n", error.toRawUTF8()); return 1; }
            deliverySpec.enabled = true;
        }
        else
        { std::fprintf (stderr, "Invalid spec JSON\n"); return 1; }
    }
    if (jsonPath.isNotEmpty())
        for (const auto& path : ml::FileAnalyzer::expandPaths (paths))
            if (juce::File (jsonPath) == juce::File (path))
            { std::fprintf (stderr, "Report cannot overwrite an audio source.\n"); return 1; }

    if (jsonPath.isNotEmpty() && specPath.isNotEmpty() && juce::File (jsonPath) == juce::File (specPath))
    { std::fprintf (stderr, "Report cannot overwrite a delivery spec\n"); return 1; }

    ml::FileAnalyzer analyzer;
    if (! analyzer.analyse (paths)) return 1;
    while (analyzer.isBusy()) juce::Thread::sleep (50);
    const auto results = analyzer.takeResults();
    bool failed = false, blocked = false;
    for (const auto& r : results)
    {
        failed |= ! r.ok;
        bool warning = ! r.forensic.findings.empty();
        for (size_t b = 0; b < 10; ++b)
            warning |= r.forensic.bandAvailable[b] && r.forensic.monoLossDb[b] >= 6.0f;
        const auto specCheck = ml::evaluateSpec (r, deliverySpec);
        blocked |= r.forensic.blockers() > 0 || r.stats.truePeakDb > (deliverySpec.enabled ? deliverySpec.ceilingDbtp : ceiling) + 0.05f
                   || (deliverySpec.enabled && ! specCheck.ok)
                   || (failOn == "warning" && warning);
    }
    juce::Array<juce::var> codecChecks;

    if (codec.isNotEmpty())
    {
        const auto tempDir = juce::File::getSpecialLocation (juce::File::tempDirectory);
        for (const auto& r : results)
        {
            if (! r.ok)
                continue;

            auto cc = ml::runCodecCheck (juce::File (r.path), codec, tempDir);
            if (cc.ok && cc.sourceSha256 != r.forensic.sha256)
            { cc.ok = false; cc.error = "source changed since analysis"; }
            failed |= ! cc.ok;
            blocked |= cc.ok && cc.truePeakDbtp > (deliverySpec.enabled ? deliverySpec.ceilingDbtp : ceiling) + 0.05f;
            if (! cc.ok) std::fprintf (stderr, "Codec check failed: %s: %s\n", r.fileName.toRawUTF8(), cc.error.toRawUTF8());
            auto* o = new juce::DynamicObject();
            o->setProperty ("sourceSha256", cc.sourceSha256);
            o->setProperty ("encodedContainer", cc.encodedContainer);
            o->setProperty ("encodedBytes", cc.encodedBytes);
            o->setProperty ("encodedSha256", cc.encodedSha256);
            o->setProperty ("decodedSha256", cc.decodedSha256);
            o->setProperty ("file", r.fileName);
            o->setProperty ("codec", codec);
            o->setProperty ("ok", cc.ok);
            o->setProperty ("error", cc.error);
            o->setProperty ("decodedPath", cc.decodedPath);
            o->setProperty ("integratedLufs", cc.integratedLufs);
            o->setProperty ("truePeakDbtp", cc.truePeakDbtp);
            codecChecks.add (juce::var (o));
        }
    }

    if (csv)
        std::printf ("%s", ml::fileResultsCsv (results, target, ceiling).toRawUTF8());
    else
        for (const auto& r : results)
        {
            const auto p = ml::gainBudget (r.stats.integrated, r.stats.truePeakDb, target, ceiling);
            std::printf ("%s  %s  %.1f LUFS  %.1f dBTP  QC %d  gain %+.1f dB  shortfall %.1f LU\n",
                         r.fileName.toRawUTF8(), r.ok ? "OK" : r.error.toRawUTF8(),
                         r.stats.integrated, r.stats.truePeakDb, r.forensic.blockers(), p.safeGainDb, p.shortfallLu);
        }
    if (jsonPath.isNotEmpty())
    {
        auto* root = new juce::DynamicObject();
        root->setProperty ("product", "MasterLens"); root->setProperty ("version", ml::kVersion);
        root->setProperty ("schemaVersion", 3);
        root->setProperty ("generatedAt", juce::Time::getCurrentTime().toISO8601 (true));
        root->setProperty ("gatePolicy", failOn.isEmpty() ? "none" : failOn);
        root->setProperty ("analysisOk", ! failed && ! results.empty());
        root->setProperty ("gateFailed", failed || ((failOn.isNotEmpty() || deliverySpec.enabled) && blocked));
        juce::Array<juce::var> files;
        for (const auto& r : results)
            files.add (ml::fileResultToVar (r, target, ceiling, &deliverySpec));
        root->setProperty ("files", files);
        root->setProperty ("deliverySpec", deliverySpec.toVar());
        root->setProperty ("codecChecks", codecChecks);
        juce::TemporaryFile reportTemp { juce::File (jsonPath) };
        if (! reportTemp.getFile().replaceWithText (juce::JSON::toString (juce::var (root), true))
            || ! reportTemp.overwriteTargetFileWithTemporary())
        { std::fprintf (stderr, "Could not write JSON: %s\n", jsonPath.toRawUTF8()); return 1; }
        std::fprintf (stderr, "JSON: %s\n", jsonPath.toRawUTF8());
    }
    if (failed || results.empty()) { std::fprintf (stderr, "Analysis FAILED; no delivery pass established\n"); return 1; }
    return (failOn.isNotEmpty() || deliverySpec.enabled) && blocked ? 2 : 0;
}
