#include "CodecCheck.h"
#include "FileAnalyzer.h"

namespace ml
{
namespace
{
juce::String findFfmpeg()
{
    for (const auto& candidate : { "/opt/homebrew/bin/ffmpeg", "/usr/local/bin/ffmpeg" })
        if (juce::File (candidate).existsAsFile())
            return candidate;

    return "ffmpeg";
}

} // namespace

CodecCheckResult runCodecCheck (const juce::File& input, const juce::String& codec,
                                const juce::File& outputDirectory)
{
    CodecCheckResult r;
    r.codec = codec.toLowerCase();

    if (! input.existsAsFile())
    {
        r.error = "input missing";
        return r;
    }

    if (r.codec != "aac" && r.codec != "mp3")
    {
        r.error = "codec must be aac or mp3";
        return r;
    }

    const auto out = outputDirectory.getNonexistentChildFile (
        "masterlens-" + r.codec + "-check", ".wav", false);
    r.decodedPath = out.getFullPathName();

    juce::StringArray args;
    args.add (findFfmpeg());
    args.addArray ({ "-hide_banner", "-loglevel", "error", "-y", "-i", input.getFullPathName() });
    if (r.codec == "aac")
        args.addArray ({ "-c:a", "aac", "-b:a", "256k" });
    else
        args.addArray ({ "-c:a", "libmp3lame", "-b:a", "320k" });
    args.addArray ({ "-f", "wav", out.getFullPathName() });

    juce::ChildProcess process;
    const bool started = process.start (args);
    const bool finished = started && process.waitForProcessToFinish (120000);

    if (! finished || process.getExitCode() != 0 || ! out.existsAsFile())
    {
        r.error = "ffmpeg unavailable or encode failed (" + juce::String ((int) process.getExitCode()) + ")";
        out.deleteFile();
        return r;
    }

    FileAnalyzer analyzer;
    analyzer.analyse ({ out.getFullPathName() });

    for (int wait = 0; wait < 1200 && analyzer.isBusy(); ++wait)
        juce::Thread::sleep (10);

    const auto results = analyzer.takeResults();

    if (results.size() != 1 || ! results[0].ok)
    {
        r.error = "decoded file analysis failed";
        return r;
    }

    r.ok = true;
    r.integratedLufs = results[0].stats.integrated;
    r.truePeakDbtp = results[0].stats.truePeakDb;
    return r;
}
} // namespace ml
