#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <cerrno>
#include <juce_core/juce_core.h>
#include "Version.h"
#include "file/FileAnalyzer.h"
#include "file/DeliverySpec.h"
#include "file/CodecCheck.h"
#include "file/FFmpegRuntime.h"

#if JUCE_WINDOWS
 #include <windows.h>
 #include <shellapi.h>
#endif

namespace
{
/** UTF-8 safe argv access: Windows main() hands over the ANSI code page, so a
    Chinese or space-containing path would otherwise arrive as mojibake. */
juce::StringArray commandLineArguments (int argc, char** argv)
{
    juce::StringArray out;

#if JUCE_WINDOWS
    int wideCount = 0;

    if (auto** wide = CommandLineToArgvW (GetCommandLineW(), &wideCount))
    {
        for (int i = 0; i < wideCount; ++i)
            out.add (juce::String (wide[i]));

        LocalFree (wide);
        return out;
    }
#endif

    for (int i = 0; i < argc; ++i)
        out.add (juce::String::fromUTF8 (argv[i]));

    return out;
}

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
    const auto arguments = commandLineArguments (argc, argv);
    const int argumentCount = arguments.size();

    juce::StringArray paths;
    juce::String jsonPath, failOn, specPath, codec, statePath, summaryPath;
    int retries = 0;
    int watchSeconds = 0, watchIterations = 0;
    bool csv = false, options = true;
    float target = -14.0f, ceiling = -1.0f;
    ml::DeliverySpec deliverySpec;
    for (int i = 1; i < argumentCount; ++i)
    {
        const juce::String arg = arguments[i];
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
                         "  --codec aac|mp3     Round-trip through the bundled FFmpeg runtime\n"
                         "  --state q.json     Incremental queue: skip unchanged files that passed\n"
                         "  --summary out.json Batch summary: changed, skipped, pass, block, review\n"
                         "  --retry N          Re-analysis attempts for failed files (0-5)\n"
                         "  --watch SECONDS    Keep scanning the folder every SECONDS (state/summary reused)\n"
                         "                      (dev builds: set MASTERLENS_FFMPEG or ALLOW_SYSTEM_FFMPEG=1)\n"
                         "  --                  End options (for paths starting with -)\n"
                         "Exit 0=analysed, 1=input / decode / report error, 2=QC gate failed.\n"
                         "Gain budgets estimate constant gain only; no audio is written.\n", ml::kVersion);
            return 0;
        }
        if (options && arg == "--") { options = false; continue; }
        if (options && arg == "analyze") continue;
        if (options && arg == "--csv") { csv = true; continue; }
        if (options && (arg == "--json" || arg == "--target" || arg == "--ceiling"
                        || arg == "--fail-on" || arg == "--spec" || arg == "--codec"
                        || arg == "--state" || arg == "--summary" || arg == "--retry"
                        || arg == "--watch" || arg == "--watch-iterations"))
        {
            if (++i >= argumentCount) { std::fprintf (stderr, "Missing value for %s\n", arg.toRawUTF8()); return 1; }
            if (arg == "--json") jsonPath = arguments[i];
            else if (arg == "--fail-on") failOn = arguments[i];
            else if (arg == "--spec") specPath = arguments[i];
            else if (arg == "--codec") codec = arguments[i];
            else if (arg == "--state") statePath = arguments[i];
            else if (arg == "--summary") summaryPath = arguments[i];
            else if (arg == "--retry") retries = juce::jlimit (0, 5, arguments[i].getIntValue());
            else if (arg == "--watch") watchSeconds = juce::jlimit (1, 3600, arguments[i].getIntValue());
            else if (arg == "--watch-iterations") watchIterations = juce::jmax (0, arguments[i].getIntValue());
            else if (! number (arguments[i].toRawUTF8(), arg == "--target" ? target : ceiling))
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

    // 3.4: continuous watch. The parent only supervises; each pass runs the
    // normal single-scan path as a child so state/summary stay crash-safe.
    if (watchSeconds > 0)
    {
        juce::StringArray childArgs;
        childArgs.add (juce::File::getSpecialLocation (juce::File::currentExecutableFile).getFullPathName());

        for (int i = 1; i < argc; ++i)
        {
            const juce::String a = arguments[i];

            if (a == "--watch" || a == "--watch-iterations")
            {
                ++i;
                continue;
            }

            childArgs.add (a);
        }

        const int limit = watchIterations > 0 ? watchIterations : -1;
        int pass = 0, lastExit = 0;

        while (limit < 0 || pass < limit)
        {
            juce::ChildProcess child;

            if (! child.start (childArgs))
            {
                std::fprintf (stderr, "watch: cannot start child process\n");
                return 1;
            }

            child.waitForProcessToFinish (-1);
            lastExit = child.getExitCode();
            ++pass;

            if (limit >= 0 && pass >= limit)
                break;

            juce::Thread::sleep (watchSeconds * 1000);
        }

        return lastExit;
    }

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
            // The client spec is the effective target for every output below
            // (blocking, codec, CSV, JSON), not just for the spec check.
            target = deliverySpec.targetLufs;
            ceiling = deliverySpec.ceilingDbtp;
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

    // 3.4: incremental queue. A file whose mtime/size is unchanged and whose
    // last status was "passed" is skipped; everything else is re-analysed.
    juce::StringArray skippedPaths;
    juce::StringArray toAnalyze;
    juce::var stateRoot;

    if (statePath.isNotEmpty())
    {
        const juce::File stateFile (statePath);
        if (stateFile.existsAsFile())
            stateRoot = juce::JSON::parse (stateFile.loadFileAsString());
    }

    for (const auto& path : ml::FileAnalyzer::expandPaths (paths))
    {
        const juce::File f (path);
        bool unchanged = false;

        if (auto* state = stateRoot.getDynamicObject())
            if (auto* e = state->getProperty (path).getDynamicObject())
                unchanged = (juce::int64) e->getProperty ("mtimeMs") == f.getLastModificationTime().toMilliseconds()
                         && (juce::int64) e->getProperty ("size") == f.getSize()
                         && e->getProperty ("status").toString() != "error";

        if (unchanged)
            skippedPaths.add (path);
        else
            toAnalyze.add (path);
    }

    std::vector<ml::FileAnalysisResult> results;

    if (! toAnalyze.isEmpty())
    {
        ml::FileAnalyzer analyzer;
        if (! analyzer.analyse (toAnalyze)) return 1;
        while (analyzer.isBusy()) juce::Thread::sleep (50);
        results = analyzer.takeResults();
    }

    // 3.4: bounded retry for failed files only. Successful files are untouched.
    for (auto& r : results)
    {
        for (int attempt = 0; attempt < retries && ! r.ok; ++attempt)
        {
            ml::FileAnalyzer retryAnalyzer;
            if (! retryAnalyzer.analyse ({ r.path }))
                break;

            while (retryAnalyzer.isBusy())
                juce::Thread::sleep (50);

            auto again = retryAnalyzer.takeResults();

            if (! again.empty())
            {
                r = again.front();

                if (r.ok)
                    break;
            }
        }
    }
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
    // 3.4: state + batch summary. "passed" files are skipped next run; blocked /
    // review / error files stay in the queue until they change or pass.
    if (statePath.isNotEmpty() || summaryPath.isNotEmpty())
    {
        const auto statusOf = [&] (const ml::FileAnalysisResult& r) -> juce::String
        {
            if (! r.ok)
                return "error";

            const auto specCheck = ml::evaluateSpec (r, deliverySpec);
            const bool over = r.stats.truePeakDb > (deliverySpec.enabled ? deliverySpec.ceilingDbtp : ceiling) + 0.05f;

            if (r.forensic.blockers() > 0 || over || (deliverySpec.enabled && ! specCheck.ok))
                return "blocked";
            if (! r.forensic.findings.empty())
                return "review";
            return "passed";
        };

        auto* state = new juce::DynamicObject();
        juce::Array<juce::var> files;
        int passed = 0, reviewed = 0, blockedCount = 0, errors = 0;

        for (const auto& r : results)
        {
            const auto st = statusOf (r);

            if (st == "passed") ++passed;
            else if (st == "review") ++reviewed;
            else if (st == "blocked") ++blockedCount;
            else ++errors;

            const juce::File f (r.path);
            auto* entry = new juce::DynamicObject();
            entry->setProperty ("mtimeMs", f.getLastModificationTime().toMilliseconds());
            entry->setProperty ("size", f.getSize());
            entry->setProperty ("status", st);
            entry->setProperty ("sha256", r.forensic.sha256);
            state->setProperty (r.path, juce::var (entry));

            auto* prev = stateRoot.getDynamicObject() != nullptr
                             ? stateRoot.getDynamicObject()->getProperty (r.path).getDynamicObject()
                             : nullptr;
            const auto prevSha = prev != nullptr ? prev->getProperty ("sha256").toString() : juce::String();
            const auto prevStatus = prev != nullptr ? prev->getProperty ("status").toString() : juce::String();

            auto* file = new juce::DynamicObject();
            file->setProperty ("path", r.path);
            file->setProperty ("status", st);
            file->setProperty ("changed", true);
            file->setProperty ("previousStatus", prevStatus);
            file->setProperty ("versionChanged", prevSha.isNotEmpty() && prevSha != r.forensic.sha256);
            files.add (juce::var (file));
        }

        for (const auto& p : skippedPaths)
        {
            if (auto* old = stateRoot.getDynamicObject())
                state->setProperty (p, old->getProperty (p));

            const auto* prev = stateRoot.getDynamicObject() != nullptr
                                   ? stateRoot.getDynamicObject()->getProperty (p).getDynamicObject()
                                   : nullptr;

            auto* file = new juce::DynamicObject();
            file->setProperty ("path", p);
            file->setProperty ("status", "skipped");
            file->setProperty ("changed", false);
            file->setProperty ("previousStatus", prev != nullptr ? prev->getProperty ("status").toString()
                                                                 : juce::String());
            file->setProperty ("versionChanged", false);
            files.add (juce::var (file));
        }

        if (statePath.isNotEmpty())
            juce::File (statePath).replaceWithText (juce::JSON::toString (juce::var (state), true));

        if (summaryPath.isNotEmpty())
        {
            auto* sum = new juce::DynamicObject();
            sum->setProperty ("product", "MasterLens");
            sum->setProperty ("version", ml::kVersion);
            sum->setProperty ("generatedAt", juce::Time::getCurrentTime().toISO8601 (true));
            sum->setProperty ("analyzed", (int) results.size());
            sum->setProperty ("skipped", skippedPaths.size());
            sum->setProperty ("passed", passed);
            sum->setProperty ("review", reviewed);
            sum->setProperty ("blocked", blockedCount);
            sum->setProperty ("errors", errors);
            sum->setProperty ("files", files);
            juce::File (summaryPath).replaceWithText (juce::JSON::toString (juce::var (sum), true));
        }
    }

    juce::Array<juce::var> codecChecks;

    if (codec.isNotEmpty())
    {
        auto& runtime = ml::FFmpegRuntime::get();
        runtime.resolve();
        std::fprintf (stderr, "Codec backend: %s\n", runtime.getStatusText().toRawUTF8());

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
            o->setProperty ("delaySamples", cc.delaySamples);
            o->setProperty ("tailSamples", cc.tailSamples);
            o->setProperty ("residual", cc.residual);
            o->setProperty ("backend", cc.backend);
            o->setProperty ("ffmpegVersion", cc.ffmpegVersion);
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
    if (failed || (results.empty() && skippedPaths.isEmpty())) { std::fprintf (stderr, "Analysis FAILED; no delivery pass established\n"); return 1; }
    return (failOn.isNotEmpty() || deliverySpec.enabled) && blocked ? 2 : 0;
}
