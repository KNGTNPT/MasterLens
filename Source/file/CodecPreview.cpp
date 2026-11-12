#include "CodecCheck.h"

#include <cmath>

#include <juce_audio_formats/juce_audio_formats.h>

#include "FFmpegRuntime.h"

namespace ml
{
namespace
{
double codecTimeoutSeconds (double durationSeconds)
{
    const int envMs = juce::SystemStats::getEnvironmentVariable (
        "MASTERLENS_CODEC_TIMEOUT_MS", {}).getIntValue();

    if (envMs > 0)
        return juce::jlimit (0.1, 3600.0, envMs / 1000.0);

    // Long files get more room, but never an unbounded wait.
    return juce::jlimit (120.0, 3600.0, 120.0 + juce::jmax (0.0, durationSeconds) * 3.0);
}

bool looksLikeValidWav (const juce::File& file)
{
    if (! file.existsAsFile() || file.getSize() <= 44)
        return false;

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));

    return reader != nullptr && reader->lengthInSamples > 0
           && reader->numChannels >= 1 && reader->numChannels <= 2
           && std::isfinite (reader->sampleRate) && reader->sampleRate >= 1000.0;
}

juce::String unavailableMessage()
{
    const auto status = FFmpegRuntime::get().getStatusText();

    if (FFmpegRuntime::runtimeRequiredForRelease())
        return "encode: required bundled ffmpeg unavailable (" + status + ")";

    return "encode: ffmpeg unavailable (" + status + ")";
}
} // namespace

CodecPreview createCodecPreview (const juce::File& input, const juce::String& codec,
                                 const juce::File& parentDirectory,
                                 const std::atomic<bool>* cancelFlag,
                                 double durationSeconds,
                                 const std::function<void (double)>& onProgress)
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

    auto& runtime = FFmpegRuntime::get();
    runtime.resolve();
    preview.backend = runtime.getSource();
    preview.ffmpegVersion = runtime.getCapabilities().version;

    if (! runtime.isAvailable())
    {
        preview.error = unavailableMessage();
        return preview;
    }

    if (! runtime.supports (preview.codec))
    {
        preview.error = "encode: ffmpeg has no " + preview.codec + " encoder ("
                        + runtime.getStatusText() + ")";
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
    const double timeout = codecTimeoutSeconds (durationSeconds);

    const auto progressFor = [durationSeconds, &onProgress] (juce::int64 micros)
    {
        if (onProgress && durationSeconds > 0.0)
            onProgress (juce::jlimit (0.0, 1.0, (double) micros / 1.0e6 / durationSeconds));
    };

    juce::StringArray encode;
    encode.addArray ({ "-y", "-nostdin", "-hide_banner", "-loglevel", "error",
                       "-nostats", "-progress", "pipe:1",
                       "-i", input.getFullPathName(), "-vn",
                       "-c:a", isAac ? "aac" : "libmp3lame",
                       "-b:a", isAac ? "256k" : "320k",
                       encoded.getFullPathName() });

    const auto encodeRun = runtime.run (encode, timeout, cancelFlag, progressFor);

    if (! encodeRun.ok)
    {
        preview.error = "encode: " + encodeRun.error
                        + (encodeRun.error == "failed" ? "" : ": " + encodeRun.message);
        return preview;
    }

    if (! encoded.existsAsFile() || encoded.getSize() <= 0)
    {
        preview.error = "encode: no output";
        return preview;
    }

    preview.encodedBytes = encoded.getSize();
    preview.container = isAac ? "m4a" : "mp3";

    juce::StringArray decode;
    decode.addArray ({ "-y", "-nostdin", "-hide_banner", "-loglevel", "error",
                       "-nostats", "-progress", "pipe:1",
                       "-i", encoded.getFullPathName(), "-vn",
                       "-c:a", "pcm_f32le",
                       decoded.getFullPathName() });

    const auto decodeRun = runtime.run (decode, timeout, cancelFlag, progressFor);

    if (! decodeRun.ok)
    {
        preview.error = "decode: " + decodeRun.error
                        + (decodeRun.error == "failed" ? "" : ": " + decodeRun.message);
        return preview;
    }

    if (! looksLikeValidWav (decoded))
    {
        preview.error = "decode: invalid output";
        return preview;
    }

    preview.decodedFile = decoded;

    // 3.3: encoder delay against the source, measured on the first 8k samples.
    // The A/B switch uses this to align the decoded side; the residual is
    // reported so a suspiciously large offset can be surfaced instead of hidden.
    {
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();

        const auto readWindow = [&fm] (const juce::File& f)
        {
            juce::AudioBuffer<float> buf;
            std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (f));

            if (r == nullptr || r->lengthInSamples <= 0 || r->numChannels <= 0)
                return buf;

            const juce::int64 length = r->lengthInSamples;
            const int n = (int) (length < 8192 ? length : 8192);
            buf.setSize (juce::jlimit (1, 2, (int) r->numChannels), n);
            r->read (&buf, 0, n, 0, true, true);
            return buf;
        };

        const auto src = readWindow (input);
        const auto dec = readWindow (decoded);
        const auto al = measureCodecAlignment (src, dec);
        preview.delaySamples = al.delaySamples;
        preview.residual = al.residual;

        std::unique_ptr<juce::AudioFormatReader> sr (fm.createReaderFor (input));
        std::unique_ptr<juce::AudioFormatReader> dr (fm.createReaderFor (decoded));

        if (sr != nullptr && dr != nullptr)
            preview.tailSamples = (int) juce::jmax ((juce::int64) 0,
                dr->lengthInSamples - sr->lengthInSamples - (juce::int64) al.delaySamples);
    }

    preview.ok = true;
    return preview;
}
} // namespace ml
