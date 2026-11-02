#include "Widgets.h"

#include <cmath>

namespace ml
{
/** Builds the pass / fail list shown on the diagnostics page.

    Every threshold below is a hard-coded, documented number (or the BS.1770
    gating result) - nothing is learned, fetched or guessed.
*/
std::vector<CheckResult> buildChecklist (const StatsSnapshot& s,
                                         float ceilingDb,
                                         const std::vector<float>& bandDevs,
                                         const SignalDefects& defects,
                                         const BasisContext& basis,
                                         const PlatformSummary& platform,
                                         bool hasTonalBasis)
{
    std::vector<CheckResult> rows;
    rows.reserve (15);

    const bool live = s.hasSignal;

    const auto add = [&rows] (juce::String label, juce::String value,
                              Severity severity, juce::String hint)
    {
        CheckResult r;
        r.label = std::move (label);
        r.value = std::move (value);
        r.severity = severity;
        r.hint = std::move (hint);
        rows.push_back (std::move (r));
    };

    // 1) basis comparison or platform-normalisation consequence (never a genre target)
    {
        const bool valid = live && s.integrated > -70.0f;

        if (valid && basis.hasComparison() && basis.mode != BasisMode::manual)
        {
            const float diff = s.integrated - basis.comparisonLufs();
            const auto sev = std::abs (diff) <= 1.5f ? Severity::good : Severity::info;

            add (T ("响度 / 基准差", "Loudness / basis delta"),
                 num (s.integrated) + " LUFS  (" + sgn (diff) + " LU)", sev,
                 basis.label() + ml::sep() + num (basis.comparisonLufs()) + " LUFS"
                     + T ("；仅显示差异，不套用固定曲风目标", "; difference only, no fixed genre target"));
        }
        else if (valid && basis.mode == BasisMode::manual)
        {
            const float diff = s.integrated - basis.manualLufs;
            add (T ("响度 / 手动参考", "Loudness / manual"),
                 num (s.integrated) + " LUFS  (" + sgn (diff) + " LU)", Severity::info,
                 T ("手动参考 ", "Manual ") + num (basis.manualLufs)
                     + T (" LUFS；此参考不作为固定合格线", " LUFS; not a fixed pass line"));
        }
        else
        {
            const auto sev = ! valid ? Severity::info
                           : (platform.maxLimitedDb > 0.05f ? Severity::warn
                           : (platform.maxWastedDb > 3.0f ? Severity::info : Severity::good));

            add (T ("响度 / 平台归一化", "Loudness / platform"),
                 valid ? num (s.integrated) + " LUFS" : noValue(), sev,
                 valid ? T ("归一化后感知不变；最坏衰减 ", "Level unchanged; worst cut ")
                             + num (platform.maxCutDb) + T (" dB，丢弃 ", " dB, wasted ")
                             + num (platform.maxWastedDb) + " LU" + T ("，目标差额 ", ", target shortfall ")
                             + num (platform.maxLimitedDb) + " LU"
                       : T ("等待信号", "Waiting for signal"));
        }
    }

    // 2) true peak vs ceiling (hard limit)
    {
        const bool valid = live && s.truePeakDb > -70.0f;
        const float headroom = ceilingDb - s.truePeakDb;
        const auto sev = ! valid ? Severity::info
                       : (headroom < 0.0f ? Severity::critical
                       : (headroom < 1.0f ? Severity::warn : Severity::good));

        add (T ("真峰值 / 上限", "True peak / ceiling"),
             valid ? num (s.truePeakDb) + " dBTP  (" + T ("余量 ", "headroom ") + num (headroom) + ")"
                   : noValue(), sev,
             T ("上限 ", "Ceiling ") + num (ceilingDb) + " dBTP · 4× oversampling");
    }

    // 3) loudness range 3 - 16 LU
    {
        const bool valid = live && s.lra > 0.05f;
        const auto sev = ! valid ? Severity::info
                       : (s.lra < 3.0f ? Severity::warn
                       : (s.lra > 16.0f ? Severity::info : Severity::good));

        add (T ("响度范围 / LRA", "Loudness range"),
             valid ? num (s.lra) + " LU" : noValue(), sev,
             T ("参考 ", "Typical ") + fmtRange (3.0f, 16.0f) + " LU");
    }

    // 4) peak to loudness ratio >= 10 dB
    {
        const bool valid = live && s.integrated > -70.0f && s.truePeakDb > -70.0f;
        const float plr = s.getPlr();
        const auto sev = ! valid ? Severity::info
                       : (plr < 8.0f ? Severity::warn
                       : (plr < 10.0f ? Severity::info : Severity::good));

        add (T ("峰响比 / PLR", "Peak to loudness"),
             valid ? num (plr) + " dB" : noValue(), sev,
             T ("参考 ", "Typical ") + fmtRange (10.0f, 16.0f) + " dB");
    }

    // 5) phase correlation
    {
        const bool valid = live && s.stereo;
        const auto sev = ! valid ? Severity::info
                       : (s.correlation < 0.0f ? Severity::critical
                       : (s.correlation < 0.15f ? Severity::warn : Severity::good));

        add (T ("声道相关性", "Phase correlation"),
             valid ? sgn (s.correlation, 2) : noValue(), sev,
             T ("≥ 0.15 为健康，< 0 表示反相", ">= 0.15 is healthy, < 0 means out of phase"));
    }

    // 6) low-end side energy
    {
        const bool valid = live && s.stereo;
        const auto sev = ! valid ? Severity::info
                       : (s.lowSidePct > 50.0f ? Severity::critical
                       : (s.lowSidePct > 35.0f ? Severity::warn : Severity::good));

        add (T ("低频侧向能量", "Low-end side energy"),
             valid ? num (s.lowSidePct) + "%" : noValue(), sev,
             T ("低于 35%，单声道折叠不损失低频", "Below 35% so mono folding keeps the low end"));
    }

    // 7) DC offset
    {
        const bool valid = live && s.dcOffsetDb > -99.0f;
        const auto sev = ! valid ? Severity::info
                       : (s.dcOffsetDb > -46.0f ? Severity::warn : Severity::good);

        add (T ("直流偏移 / DC", "DC offset"),
             valid ? num (s.dcOffsetDb) + " dB" : noValue(), sev,
             T ("低于 −46 dB 视为正常", "Below -46 dB is normal"));
    }

    // 8) distortion events
    {
        const int events = s.clipEvents + s.clickEvents;
        const auto sev = ! live ? Severity::info
                       : (s.clipEvents > 0 ? Severity::critical
                       : (s.clickEvents > 0 ? Severity::warn : Severity::good));

        add (T ("削波 / 爆音事件", "Clip / click events"),
             live ? juce::String (events) + T (" 次", " events") : noValue(), sev,
             T ("削波 0 次、爆音 0 次为合格", "0 clips and 0 clicks is the target"));
    }

    // 9) tonal balance (reference deviation)
    {
        float worst = 0.0f;

        for (float d : bandDevs)
            worst = juce::jmax (worst, std::abs (d));

        const bool valid = live && hasTonalBasis
                           && bandDevs.size() == (size_t) SpectrumAnalyzer::numBands;
        const auto sev = ! valid ? Severity::info
                       : (worst < 4.5f ? Severity::good
                       : (worst < 6.0f ? Severity::info : Severity::warn));

        add (T ("频段平衡 / 参考", "Tonal balance / reference"),
             valid ? T ("最大偏差 ", "Max deviation ") + num (worst) + " dB" : noValue(), sev,
             valid ? T ("各频段相对参考 ≤ ±4.5 dB", "Every band within +/- 4.5 dB of the reference")
                   : T ("未载入参考轨且专辑中位不足；频谱不设绝对目标曲线",
                        "No reference and no album median; no absolute target curve is used"));
    }

    // 10-15) signal integrity (only failing layers, plus one pass summary)
    if (defects.ready)
    {
        int layersOver = 0;

        if (defects.distortionRisk >= 55.0f)
        {
            ++layersOver;
            add (T ("谐波 / 互调失真", "Harmonic / IMD"),
                 T ("谐波 +", "harm +") + num (defects.harmonicExcessDb) + " dB",
                 defects.flatTopPct > 0.5f ? Severity::critical : Severity::warn,
                 T ("2–5 次谐波高于局部谱底 5 dB 以上；平顶样本 ",
                    "2nd-5th harmonics above the local floor; flat tops ")
                     + num (defects.flatTopPct, 2) + "%");
        }

        if (defects.aliasRisk >= 55.0f)
        {
            ++layersOver;
            add (T ("混叠 / 高频完整性", "Aliasing / HF"),
                 T ("风险 ", "risk ") + juce::String (juce::roundToInt (defects.aliasRisk)),
                 Severity::warn,
                 T ("16–20 kHz 高于自然滚降 ", "16-20 kHz above natural roll-off ")
                     + sgn (defects.aliasExcessDb) + " dB");
        }

        if (defects.subRisk >= 55.0f)
        {
            ++layersOver;
            add (T ("超低频堆积", "Sub build-up"),
                 num (defects.subSharePct) + "% < 40 Hz", Severity::info,
                 T ("40 Hz 以下能量占比过高；收紧 20–40 Hz",
                    "Too much energy below 40 Hz; tighten 20-40 Hz"));
        }

        if (defects.transientScore < 45.0f)
        {
            ++layersOver;
            add (T ("瞬态完整性", "Transient integrity"),
                 juce::String (juce::roundToInt (defects.transientScore)) + "/100",
                 Severity::warn,
                 T ("PLR ", "PLR ") + num (s.getPlr()) + T (" dB，攻击被压平", " dB - attacks flattened"));
        }

        if (defects.monoRisk >= 40.0f)
        {
            ++layersOver;
            add (T ("单声道折叠损失", "Mono fold loss"),
                 num (defects.monoFoldLossDb) + " dB",
                 defects.monoRisk >= 70.0f ? Severity::critical : Severity::warn,
                 T ("折叠后保留 ", "Retained ") + num (defects.monoRetainPct) + "%");
        }

        if (defects.maskingRisk >= 55.0f)
        {
            ++layersOver;
            add (T ("中频掩蔽风险", "Mid masking"),
                 sgn (defects.maskingDb) + " dB", Severity::info,
                 T ("200–800 Hz 相对 1–4 kHz 偏高", "200-800 Hz over 1-4 kHz"));
        }

        if (layersOver == 0)
            add (T ("信号完整性", "Signal integrity"),
                 T ("6 层正常", "6 layers OK"), Severity::good,
                 T ("失真、混叠、超低、瞬态、单声道与掩蔽均未越线",
                    "Distortion, aliasing, sub, transients, mono and masking are all in range"));
    }

    return rows;
}
} // namespace ml
