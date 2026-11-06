#include "CodecCheck.h"

#include <juce_cryptography/juce_cryptography.h>

namespace ml
{
namespace
{
juce::String findFfmpegExecutable()
{
    for (const auto& candidate : { "/opt/homebrew/bin/ffmpeg", "/usr/local/bin/ffmpeg" })
        if (juce::File (candidate).existsAsFile())
            return candidate;

    return "ffmpeg";
}

bool runFfmpeg (const juce::StringArray& args, const juce::String& stage, juce::String& error)
{
    juce::ChildProcess process;

    if (! process.start (args, 0))
    {
        error = stage + ": ffmpeg unavailable";
        return false;
    }

    const int timeout = juce::jlimit (100, 120000, juce::SystemStats::getEnvironmentVariable (
        "MASTERLENS_CODEC_TIMEOUT_MS", "120000").getIntValue());
    const double deadline = juce::Time::getMillisecondCounterHiRes() + timeout;

    while (process.isRunning())
    {
        if (juce::Time::getMillisecondCounterHiRes() > deadline)
        {
            process.kill();
            process.waitForProcessToFinish (1000);
            error = stage + ": timeout";
            return false;
        }

        juce::Thread::sleep (5);
    }

    if (process.getExitCode() != 0)
    {
        error = stage + ": failed, exit " + juce::String ((int) process.getExitCode());
        return false;
    }

    return true;
}
} // namespace

CodecPreview createCodecPreview (const juce::File& input, const juce::String& codec,
                                 const juce::File& parentDirectory)
{
    CodecPreview preview;
    preview.codec = codec.toLowerCase();

    if (! input.existsAsFile())
    {
        preview.error = "input missing";
        return preview;
    }

    if (preview.codec != "aac" && preview.codec != "mp3")
    {
        preview.error = "codec must be aac or mp3";
        return preview;
    }

    parentDirectory.createDirectory();
    preview.workDirectory = parentDirectory.getNonexistentChildFile ("masterlens-codec", "", false);

    if (! preview.workDirectory.createDirectory())
    {
        preview.error = "temporary directory unavailable";
        return preview;
    }

    const auto isAac = preview.codec == "aac";
    const auto encoded = preview.workDirectory.getChildFile (isAac ? "encoded.m4a" : "encoded.mp3");
    const auto decoded = preview.workDirectory.getChildFile ("decoded.wav");
    const auto executable = juce::SystemStats::getEnvironmentVariable ("MASTERLENS_FFMPEG",
                                                                      findFfmpegExecutable());

    juce::StringArray encode;
    encode.addArray ({ executable, "-nostdin", "-hide_banner", "-loglevel", "quiet", "-y",
                      "-i", input.getFullPathName(), "-vn", "-c:a", isAac ? "aac" : "libmp3lame",
                      "-b:a", isAac ? "256k" : "320k", encoded.getFullPathName() });

    if (! runFfmpeg (encode, "encode", preview.error))
        return preview;

    if (! encoded.existsAsFile())
    {
        preview.error = "encode: no output";
        return preview;
    }

    preview.encodedBytes = encoded.getSize();
    preview.container = isAac ? "m4a" : "mp3";

    juce::StringArray decode;
    decode.addArray ({ executable, "-nostdin", "-hide_banner", "-loglevel", "quiet", "-y",
                      "-i", encoded.getFullPathName(), "-vn", "-c:a", "pcm_f32le",
                      decoded.getFullPathName() });

    if (! runFfmpeg (decode, "decode", preview.error))
        return preview;

    if (! decoded.existsAsFile())
    {
        preview.error = "decode: no output";
        return preview;
    }

    preview.decodedFile = decoded;
    preview.ok = true;
    return preview;
}
} // namespace ml
