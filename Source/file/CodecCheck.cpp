#include "CodecCheck.h"

#include <cmath>
#include <vector>

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_cryptography/juce_cryptography.h>

#include "FFmpegRuntime.h"
#include "FileAnalyzer.h"

namespace ml
{
namespace
{
double sourceDuration (const juce::File& file)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));

    if (reader == nullptr || reader->sampleRate <= 0.0)
        return 0.0;

    return (double) reader->lengthInSamples / reader->sampleRate;
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

    auto& runtime = FFmpegRuntime::get();
    runtime.resolve();
    r.backend = runtime.getSource();
    r.ffmpegVersion = runtime.getCapabilities().version;

    if (! runtime.isAvailable())
    {
        r.error = "encode: ffmpeg unavailable (" + runtime.getStatusText() + ")";
        return r;
    }

    if (! runtime.supports (r.codec))
    {
        r.error = "encode: ffmpeg has no " + r.codec + " encoder ("
                  + runtime.getStatusText() + ")";
        return r;
    }

    r.sourceSha256 = juce::SHA256 (input).toHexString();

    auto preview = createCodecPreview (input, r.codec, outputDirectory, nullptr,
                                       sourceDuration (input));
    r.delaySamples = preview.delaySamples;
    r.tailSamples = preview.tailSamples;
    r.residual = preview.residual;

    struct Cleanup
    {
        juce::File path;
        ~Cleanup() { if (path != juce::File()) path.deleteRecursively(); }
    } cleanup { preview.workDirectory };

    if (! preview.ok)
    {
        r.error = preview.error;
        return r;
    }

    const auto encoded = preview.workDirectory.getChildFile (r.codec == "aac" ? "encoded.m4a"
                                                                             : "encoded.mp3");
    r.encodedBytes = preview.encodedBytes;
    r.encodedContainer = preview.container;
    r.encodedSha256 = juce::SHA256 (encoded).toHexString();

    FileAnalyzer analyzer;
    analyzer.analyse ({ preview.decodedFile.getFullPathName() });

    for (int wait = 0; wait < 1200 && analyzer.isBusy(); ++wait)
        juce::Thread::sleep (10);

    const auto results = analyzer.takeResults();

    if (results.size() != 1 || ! results[0].ok)
    {
        r.error = "decoded file analysis failed";
        return r;
    }

    r.decodedSha256 = results[0].forensic.sha256;

    if (! input.existsAsFile() || juce::SHA256 (input).toHexString() != r.sourceSha256)
    {
        r.error = "source changed during codec check";
        return r;
    }

    r.ok = true;
    r.integratedLufs = results[0].stats.integrated;
    r.truePeakDbtp = results[0].stats.truePeakDb;
    return r;
}
} // namespace ml
