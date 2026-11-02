#pragma once

#include <juce_core/juce_core.h>
#include "dsp/AnalysisEngine.h"
#include "Basis.h"

namespace ml
{
enum class DeliveryState { ok = 0, warning = 1, over = 2 };

struct PlatformPreset
{
    const char* id;
    const char* nameZh;
    const char* nameEn;
    float targetLufs;
    float tpCeiling;
    bool  normalises;
    int   minTailSilenceMs;      // 0 = no tail-silence requirement
    const char* codecZh;
    const char* codecEn;
    const char* noteZh;
    const char* noteEn;
};

/** Static, hand-written delivery targets. Nothing here is learned or fetched. */
inline const PlatformPreset& platform (int index)
{
    static const PlatformPreset table[] =
    {
        { "spotify",   "Spotify",        "Spotify",        -14.0f, -1.0f, true, 0,
          "Ogg Vorbis / AAC", "Ogg Vorbis / AAC",
          "以 -14 LUFS 归一化；母带高于 -14 时建议 TP 低于 -2 dBTP。",
          "Normalises to -14 LUFS; if the master is louder, keep TP below -2 dBTP." },
        { "apple",     "Apple Music",    "Apple Music",    -16.0f, -1.0f, true, 0,
          "ALAC / AAC 256k", "ALAC / AAC 256k",
          "-16 LUFS / -1 dBTP 为本机比较情景；ADM 使用高质量 24-bit 源并检查编码后的削波。",
          "-16 LUFS / -1 dBTP is a local comparison scenario; use high-quality 24-bit sources and check encoded clipping for ADM." },
        { "youtube",   "YouTube",        "YouTube",        -14.0f, -1.0f, true, 0,
          "AAC / Opus", "AAC / Opus",
          "按 -14 LUFS 归一化；过响母带会被整体衰减。",
          "Normalised to -14 LUFS; louder masters are turned down as a whole." },
        { "tidal",     "Tidal",          "Tidal",          -14.0f, -1.0f, true, 0,
          "FLAC / AAC", "FLAC / AAC",
          "约 -14 LUFS 归一化，真峰值上限 -1 dBTP。",
          "About -14 LUFS normalisation with a -1 dBTP ceiling." },
        { "amazon",    "Amazon Music",   "Amazon Music",   -14.0f, -1.0f, true, 0,
          "MP3 / FLAC", "MP3 / FLAC",
          "以 -14 LUFS 为播放基准，编码前保留 TP 余量。",
          "Plays around -14 LUFS; keep true-peak headroom before encoding." },
        { "soundcloud","SoundCloud",     "SoundCloud",     -14.0f, -1.0f, true, 0,
          "AAC / MP3", "AAC / MP3",
          "上传转码前保持 -1 dBTP，避免有损编码削波。",
          "Keep -1 dBTP before lossy transcoding to avoid codec clipping." },
        { "podcast",   "播客 / 广播",     "Podcast / R128", -16.0f, -1.0f, true, 0,
          "AAC / MP3", "AAC / MP3",
          "播客常用 -16 LUFS；广播 EBU R128 以 -23 LUFS 为准。",
          "Podcasts commonly use -16 LUFS; EBU R128 broadcast uses -23 LUFS." },
        { "club",      "俱乐部 / 舞曲",   "Club / dance",    -8.0f, -0.8f, false, 0,
          "WAV / AIFF", "WAV / AIFF",
          "不做平台归一化；为 PA 留 -0.8 dBTP，重点检查低频与瞬态。",
          "No platform normalisation; keep -0.8 dBTP for a PA and check low end and transients." }
    };

    return table[juce::jlimit (0, 7, index)];
}

inline constexpr int numPlatforms = 8;

struct DeliveryResult
{
    float gainDb = 0.0f;             // headroom-constrained constant-gain scenario
    float requestedGainDb = 0.0f;    // gain required to reach the scenario target
    float predictedTpRaw = 0.0f;     // peak if the requested gain were applied
    float predictedTp = 0.0f;        // peak at the constrained gain; no limiter model
    float limitedDb = 0.0f;          // legacy field: target shortfall / required peak reduction
    float wastedLoudnessDb = 0.0f;   // loudness bought that normalisation throws away
    DeliveryState state = DeliveryState::ok;
};

/** Local comparison scenarios, not a simulation of streaming-service DSP.
    Positive gain is limited by peak headroom. A shortfall does not imply that a
    platform adds a limiter (Spotify Normal explicitly limits the gain instead).
    Loudness after constant gain is an estimate because gating may change. */
inline DeliveryResult evaluate (const StatsSnapshot& s, const PlatformPreset& p)
{
    DeliveryResult r;

    if (s.integrated <= -70.0f || s.truePeakDb <= -70.0f)
        return r;

    if (! p.normalises)
    {
        r.predictedTpRaw = s.truePeakDb;
        r.predictedTp = s.truePeakDb;

        r.state = r.predictedTp > p.tpCeiling
                    ? DeliveryState::over
                    : (r.predictedTp > p.tpCeiling - 0.5f ? DeliveryState::warning
                                                          : DeliveryState::ok);
        return r;
    }

    r.requestedGainDb = p.targetLufs - s.integrated;
    r.predictedTpRaw = s.truePeakDb + r.requestedGainDb;
    r.gainDb = juce::jmin (r.requestedGainDb, p.tpCeiling - s.truePeakDb);
    r.predictedTp = s.truePeakDb + r.gainDb;
    r.limitedDb = juce::jmax (0.0f, r.requestedGainDb - r.gainDb);

    r.wastedLoudnessDb = juce::jmax (0.0f, s.integrated - p.targetLufs);

    r.state = r.limitedDb > 0.05f ? DeliveryState::over
            : (r.predictedTp > p.tpCeiling - 0.5f ? DeliveryState::warning
                                                  : DeliveryState::ok);
    return r;
}

/** Aggregated worst case over all platforms (used by the main view). */
inline PlatformSummary platformSummary (const StatsSnapshot& s)
{
    PlatformSummary out;

    for (int i = 0; i < numPlatforms; ++i)
    {
        const auto& p = platform (i);
        const auto r = evaluate (s, p);
        const float gain = r.gainDb;

        if (gain < 0.0f)
            out.maxCutDb = juce::jmax (out.maxCutDb, -gain);
        else
            out.maxBoostDb = juce::jmax (out.maxBoostDb, gain);

        out.maxLimitedDb = juce::jmax (out.maxLimitedDb, r.limitedDb);
        out.maxWastedDb = juce::jmax (out.maxWastedDb, r.wastedLoudnessDb);

        if (r.state == DeliveryState::over)         ++out.overCount;
        else if (r.state == DeliveryState::warning) ++out.warningCount;
    }

    return out;
}
} // namespace ml
