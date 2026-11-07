#pragma once

#include <atomic>
#include <functional>

#include <juce_core/juce_core.h>

namespace ml
{
struct CodecCheckResult
{
    bool ok = false;
    juce::String codec;
    juce::String error;
    juce::String decodedPath; // legacy field; temporary files are not retained
    juce::String encodedContainer, encodedSha256, decodedSha256, sourceSha256;
    juce::int64 encodedBytes = 0;
    float integratedLufs = -100.0f;
    float truePeakDbtp = -100.0f;

    // Backend evidence (guide §五.7): which runtime produced the result.
    juce::String backend;        // bundled | override | system | none
    juce::String ffmpegVersion;
};

/** Local codec check: the shared FFmpeg runtime encodes to AAC/MP3, decodes the
    result back to WAV, then the normal file analyser measures it. */
CodecCheckResult runCodecCheck (const juce::File& input, const juce::String& codec,
                                const juce::File& outputDirectory);

/** Encoded/decoded files kept on disk for A/B playback.

    Same runtime contract as runCodecCheck (AAC 256k / MP3 320k), but the caller
    owns workDirectory and must delete it. The decoded file is a float WAV at the
    source sample rate, so StreamingPreviewPlayer can stream it.
*/
struct CodecPreview
{
    bool ok = false;
    juce::String error;
    juce::String codec;
    juce::String container;
    juce::File workDirectory;
    juce::File decodedFile;
    juce::int64 encodedBytes = 0;
    juce::String backend;
    juce::String ffmpegVersion;
};

CodecPreview createCodecPreview (const juce::File& input, const juce::String& codec,
                                 const juce::File& parentDirectory,
                                 const std::atomic<bool>* cancelFlag = nullptr,
                                 double durationSeconds = 0.0,
                                 const std::function<void (double)>& onProgress = {});
} // namespace ml
