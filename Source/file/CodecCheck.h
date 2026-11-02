#pragma once

#include <juce_core/juce_core.h>

namespace ml
{
struct CodecCheckResult
{
    bool ok = false;
    juce::String codec;
    juce::String error;
    juce::String decodedPath;
    float integratedLufs = -100.0f;
    float truePeakDbtp = -100.0f;
};

/** Optional local codec check: ffmpeg encodes to AAC/MP3 and decodes the result
    back to WAV, then the normal file analyser measures the decoded true peak. */
CodecCheckResult runCodecCheck (const juce::File& input, const juce::String& codec,
                                const juce::File& outputDirectory);
} // namespace ml
