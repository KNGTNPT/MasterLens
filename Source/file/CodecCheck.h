#pragma once

#include <atomic>
#include <functional>

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>

#include <cmath>
#include <vector>

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
    int delaySamples = 0;
    int tailSamples = 0;
    float residual = 1.0f;

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

    // 3.3: encoder delay / tail measured against the source, so A/B playback
    // and the differential audition can align the decoded side.
    int delaySamples = 0;
    int tailSamples = 0;
    float residual = 1.0f;
};

/** 3.3: 4x windowed-sinc interpolation for locating the encoded true peak. */
inline double sincKernel (double x)
{
    if (std::abs (x) < 1.0e-9)
        return 1.0;

    const double px = juce::MathConstants<double>::pi * x;
    return std::sin (px) / px;
}

inline float interpolateSample (const float* window, int centre, double frac, int taps)
{
    double sum = 0.0, norm = 0.0;

    for (int k = -taps; k <= taps; ++k)
    {
        const double w = 0.5 + 0.5 * std::cos (juce::MathConstants<double>::pi * k / (taps + 1));
        const double s = sincKernel (frac - k) * w;
        sum += window[centre + k] * s;
        norm += s;
    }

    return (float) (sum / (std::abs (norm) > 1.0e-9 ? norm : 1.0));
}

/** 3.3: gain that matches the decoded loudness to the source before A-B. */
inline float loudnessMatchGainDb (float sourceLufs, float decodedLufs)
{
    if (! std::isfinite (sourceLufs) || ! std::isfinite (decodedLufs)
        || sourceLufs <= -70.0f || decodedLufs <= -70.0f)
        return 0.0f;

    return juce::jlimit (-12.0f, 12.0f, sourceLufs - decodedLufs);
}

/** Encoder alignment between two equal-rate buffers (mono sum). */
struct CodecAlignment
{
    bool ok = false;
    int delaySamples = 0;   // decoded starts this many samples after the source
    float residual = 1.0f;  // normalised error at the best offset (0 = identical)
};

inline CodecAlignment measureCodecAlignment (const juce::AudioBuffer<float>& source,
                                      const juce::AudioBuffer<float>& decoded)
{
    CodecAlignment r;

    const int n = juce::jmin (8192, juce::jmax (0, source.getNumSamples()),
                              juce::jmax (0, decoded.getNumSamples()));

    if (n < 512 || source.getNumChannels() <= 0 || decoded.getNumChannels() <= 0)
        return r;

    std::vector<float> a ((size_t) n), b ((size_t) n);

    for (int i = 0; i < n; ++i)
    {
        float s = 0.0f, d = 0.0f;

        for (int ch = 0; ch < source.getNumChannels(); ++ch)
            s += source.getSample (ch, i);
        for (int ch = 0; ch < decoded.getNumChannels(); ++ch)
            d += decoded.getSample (ch, i);

        a[(size_t) i] = s / (float) source.getNumChannels();
        b[(size_t) i] = d / (float) decoded.getNumChannels();
    }

    const int search = 4096;
    const int coarse = 4;
    int bestShift = 0;
    double best = -1.0;

    const auto correlate = [&] (int shift)
    {
        const int lo = juce::jmax (0, -shift);
        const int hi = juce::jmin (n, n - shift);
        if (hi - lo < n / 2)
            return -1.0;

        double num = 0.0, denA = 0.0, denB = 0.0;

        for (int i = lo; i < hi; ++i)
        {
            const double x = a[(size_t) i];
            const double y = b[(size_t) (i + shift)];
            num += x * y;
            denA += x * x;
            denB += y * y;
        }

        const double den = std::sqrt (denA * denB);
        return den > 0.0 ? num / den : -1.0;
    };

    for (int shift = -search; shift <= search; shift += coarse)
    {
        const double c = correlate (shift);

        if (c > best)
        {
            best = c;
            bestShift = shift;
        }
    }

    for (int shift = bestShift - coarse; shift <= bestShift + coarse; ++shift)
    {
        if (std::abs (shift) > search)
            continue;

        const double c = correlate (shift);

        if (c > best)
        {
            best = c;
            bestShift = shift;
        }
    }

    r.ok = best > 0.2;
    r.delaySamples = bestShift;
    r.residual = (float) juce::jlimit (0.0, 1.0, 1.0 - best);
    return r;
}


CodecPreview createCodecPreview (const juce::File& input, const juce::String& codec,
                                 const juce::File& parentDirectory,
                                 const std::atomic<bool>* cancelFlag = nullptr,
                                 double durationSeconds = 0.0,
                                 const std::function<void (double)>& onProgress = {});
} // namespace ml
