#pragma once

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
};

/** Optional local codec check: ffmpeg encodes to AAC/MP3 and decodes the result
    back to WAV, then the normal file analyser measures the decoded true peak. */
CodecCheckResult runCodecCheck (const juce::File& input, const juce::String& codec,
                                const juce::File& outputDirectory);

/** Encoded/decoded files kept on disk for A/B playback.

    Same ffmpeg contract as runCodecCheck (local ffmpeg, AAC 256k / MP3 320k),
    but the caller owns workDirectory and must delete it. The decoded file is a
    float WAV at the source sample rate, so it can be streamed by
    StreamingPreviewPlayer.
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
};

CodecPreview createCodecPreview (const juce::File& input, const juce::String& codec,
                                 const juce::File& parentDirectory);
} // namespace ml
