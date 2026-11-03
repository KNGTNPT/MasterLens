#include "CodecCheck.h"
#include "FileAnalyzer.h"
#include <juce_cryptography/juce_cryptography.h>

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

    outputDirectory.createDirectory();
    const auto work = outputDirectory.getNonexistentChildFile ("masterlens-codec", "", false);
    if (! work.createDirectory()) { r.error = "temporary directory unavailable"; return r; }
    struct Cleanup { juce::File path; ~Cleanup() { path.deleteRecursively(); } } cleanup { work };
    const auto encoded = work.getChildFile (r.codec == "aac" ? "encoded.m4a" : "encoded.mp3");
    const auto out = work.getChildFile ("decoded.wav");
    const auto executable = juce::SystemStats::getEnvironmentVariable ("MASTERLENS_FFMPEG", findFfmpeg());
    const auto execute = [&] (juce::StringArray args, const char* stage)
    {
        juce::ChildProcess process;
        if (! process.start (args, 0)) { r.error = juce::String (stage) + ": ffmpeg unavailable"; return false; }
        const int timeout = juce::jlimit (100, 120000, juce::SystemStats::getEnvironmentVariable (
            "MASTERLENS_CODEC_TIMEOUT_MS", "120000").getIntValue());
        const double deadline = juce::Time::getMillisecondCounterHiRes() + timeout;
        while (process.isRunning())
        {
            if (juce::Time::getMillisecondCounterHiRes() > deadline)
            { process.kill(); process.waitForProcessToFinish (1000); r.error = juce::String (stage) + ": timeout"; return false; }
            juce::Thread::sleep (5);
        }
        if (process.getExitCode() != 0)
        { r.error = juce::String (stage) + ": failed, exit " + juce::String ((int) process.getExitCode()); return false; }
        return true;
    };
    juce::StringArray encode;
    encode.addArray ({ executable, "-nostdin", "-hide_banner", "-loglevel", "quiet", "-y",
                      "-i", input.getFullPathName(), "-vn", "-c:a", r.codec == "aac" ? "aac" : "libmp3lame",
                      "-b:a", r.codec == "aac" ? "256k" : "320k", encoded.getFullPathName() });
    if (! execute (encode, "encode") || ! encoded.existsAsFile()) return r;
    r.encodedBytes = encoded.getSize();
    r.encodedSha256 = juce::SHA256 (encoded).toHexString();
    r.encodedContainer = r.codec == "aac" ? "m4a" : "mp3";
    juce::StringArray decode;
    decode.addArray ({ executable, "-nostdin", "-hide_banner", "-loglevel", "quiet", "-y",
                      "-i", encoded.getFullPathName(), "-vn", "-c:a", "pcm_f32le", out.getFullPathName() });
    if (! execute (decode, "decode") || ! out.existsAsFile()) return r;
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

    r.decodedSha256 = results[0].forensic.sha256;
    r.ok = true;
    r.integratedLufs = results[0].stats.integrated;
    r.truePeakDbtp = results[0].stats.truePeakDb;
    return r;
}
} // namespace ml
