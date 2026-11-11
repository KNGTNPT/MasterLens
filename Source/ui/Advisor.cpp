#include "Advisor.h"
#include "Theme.h"
#include "../Delivery.h"
#include "../Basis.h"
#include "../Text.h"
#include "../dsp/SpectrumAnalyzer.h"
#include "Localisation.h"

#include <algorithm>

namespace ml
{
static juce::String one (double v) { return ml::num ((float) v, 1); }
static juce::String oneSigned (double v) { return ml::sgn ((float) v, 1); }
static juce::String twoSigned (double v) { return ml::sgn ((float) v, 2); }

void Advisor::reset()
{
    conditions.clear();
    visible.clear();
    overall = Severity::good;
    frame = 0;
}

juce::String Advisor::getHeadline() const
{
    switch (overall)
    {
        case Severity::critical: return T ("需要处理", "Action needed");
        case Severity::warn:     return T ("建议检查", "Review suggested");
        case Severity::info:     return T ("可优化", "Could improve");
        default:                 return T ("状态良好", "Healthy");
    }
}

//==============================================================================
void Advisor::update (const StatsSnapshot& stats,
                      const std::vector<float>& bandDevs,
                      float targetLufs,
                      float ceilingDb,
                      const SignalDefects& defects,
                      const BasisContext& basis,
                      bool hasTonalBasis)
{
    ++frame;

    std::vector<Issue> found;
    const bool signal = stats.hasSignal && stats.shortTerm > -65.0f;

    const auto add = [&found] (juce::String id, juce::String title, const juce::String& detail,
                               Severity sev, bool timed = false)
    {
        Issue i;
        i.id = std::move (id);
        i.title = std::move (title);
        i.detail = detail;
        i.severity = sev;
        i.timed = timed;
        found.push_back (std::move (i));
    };

    if (stats.invalidSamples > 0)
        add ("nonfinite_input", T ("输入含无效采样", "Non-finite input samples"),
             T ("测量已标为无效，请检查上游并清零重测", "Measurement invalid; check upstream processing and reset"), Severity::critical);

    // ---------------------------------------------------------------- loudness
    if (stats.integrated > -70.0f)
    {
        if (basis.hasComparison() && basis.mode != BasisMode::manual)
        {
            const double diff = (double) stats.integrated - (double) basis.comparisonLufs();
            const double plrDiff = (double) stats.getPlr() - (double) basis.comparisonPlr();
            const auto basisName = basis.label();

            if (std::abs (diff) > 1.5 || std::abs (plrDiff) > 1.5)
            {
                const bool louder = diff > 0.0;
                add ("basis-diff", T ("与基准的响度 / 动态差", "Loudness / dynamics vs basis"),
                     T ("综合响度 ", "Integrated ") + one (stats.integrated) + " LUFS，"
                         + T ("比", "vs ") + basisName + " " + oneSigned (-diff) + " LU；"
                         + "PLR " + oneSigned (-plrDiff) + " LU。"
                         + (louder ? T ("归一化后感知响度不变，但动态余量更少。",
                                        "Normalisation keeps perceived level but leaves less dynamic headroom.")
                                   : T ("保留更多动态；以平台归一化为目标时通常无需提升。",
                                        "More dynamics retained; no need to raise it for normalised platforms.")),
                     Severity::info);
            }
            else
            {
                add ("basis-ok", T ("与基准接近", "Close to basis"),
                     T ("综合响度与 PLR 均接近", "Integrated loudness and PLR are close to ")
                         + basisName + T ("；没有固定曲风目标被套用。", "; no fixed genre target is applied."),
                     Severity::good);
            }
        }
        else
        {
            const auto p = platformSummary (stats);

            if (p.maxWastedDb > 3.0f || p.maxLimitedDb > 0.05f)
            {
                add ("platform-norm", T ("平台归一化后果", "Platform normalisation consequence"),
                     T ("最坏平台需衰减 ", "Worst platform attenuates ")
                         + one (p.maxCutDb) + T (" dB；被丢弃 ", " dB; wasted ")
                         + one (p.maxWastedDb) + T (" LU，目标尚差 ", " LU, target shortfall ")
                         + one (p.maxLimitedDb) + " dB。"
                         + T ("归一化后感知响度不变，损失的是动态余量。",
                              "Perceived level after normalisation is unchanged; the loss is dynamic headroom."),
                     Severity::info);
            }
            else
            {
                add ("platform-ok", T ("平台归一化无需额外处理", "No extra platform normalisation"),
                     T ("最坏平台衰减 ", "Worst attenuation ") + one (p.maxCutDb)
                         + T (" dB，目标尚差 ", " dB, target shortfall ") + one (p.maxLimitedDb) + " dB。",
                     Severity::good);
            }
        }
    }

    if (stats.truePeakDb > (double) ceilingDb + 0.05)
        add ("tp-over", T ("真峰值过载", "True peak over ceiling"),
             TF (T ("真峰值 {tp} dBTP，超出上限 {ceiling} dBTP 共 {d} dB。转为 AAC / MP3 后会出现削波失真，请在限制器预留余量。",
                    "True peak {tp} dBTP is {d} dB above the {ceiling} dBTP ceiling. Lossy encoding "
                    "will clip - leave headroom on the limiter."),
                 { { "tp", one (stats.truePeakDb) }, { "ceiling", one (ceilingDb) },
                   { "d", one (stats.truePeakDb - ceilingDb) } }),
             Severity::critical);
    else if (stats.truePeakDb > -70.0f && stats.truePeakDb > ceilingDb - 0.1f)
        add ("tp-edge", T ("真峰值贴近上限", "True peak close to ceiling"),
             TF (T ("真峰值 {tp} dBTP，与上限仅差 {d} dB，编码后可能越过 0 dBTP。",
                    "True peak {tp} dBTP is only {d} dB below the ceiling; encoding may push it past 0 dBTP."),
                 { { "tp", one (stats.truePeakDb) }, { "d", one (ceilingDb - stats.truePeakDb) } }),
             Severity::info);

    // ---------------------------------------------------------------- delivery
    if (stats.integrated > -70.0f && stats.truePeakDb > -70.0f)
    {
        int over = 0, warned = 0;
        const char* worstZh = "Spotify";
        const char* worstEn = "Spotify";
        float worstTp = -100.0f;
        float worstGain = 0.0f;

        for (int i = 0; i < numPlatforms; ++i)
        {
            const auto& p = platform (i);
            const auto r = evaluate (stats, p);

            if (r.state == DeliveryState::over)
            {
                ++over;

                if (r.predictedTpRaw > worstTp)
                {
                    worstTp = r.predictedTpRaw;
                    worstGain = r.gainDb;
                    worstZh = p.nameZh;
                    worstEn = p.nameEn;
                }
            }
            else if (r.state == DeliveryState::warning)
            {
                ++warned;
            }
        }

        if (over > 0)
            add ("delivery-over", T ("按平台归一化后真峰值超限", "Predicted true peak over ceiling"),
                 TF (T ("{n} 个平台在归一化后预测真峰值超过上限，最高出现在 {p}（{tp} dBTP，需增益 {g} dB）。这是本机目标情景；请检查增益预算并实际试听编码结果。",
                        "{n} platforms would exceed the true-peak ceiling after normalisation - worst is "
                        "{p} at {tp} dBTP (gain {g} dB). This is a local target scenario; review the gain budget and audition encoded output."),
                     { { "n", juce::String (over) }, { "p", T (worstZh, worstEn) },
                       { "tp", one (worstTp) }, { "g", oneSigned (worstGain) } }),
                 Severity::critical);
        else if (warned > 0)
            add ("delivery-warn", T ("交付余量偏紧", "Delivery headroom is tight"),
                 T ("至少一个平台的预测真峰值贴近上限，建议再留 0.3 dB 余量。",
                    "At least one platform sits right at the ceiling - leave another 0.3 dB of headroom."),
                 Severity::warn);
        else
            add ("delivery-ok", T ("各平台归一化后均在限内", "Every platform stays within the ceiling"),
                 T ("当前情景的增益与真峰值均在所选上限内。",
                    "The local scenarios stay within their peak ceilings."),
                 Severity::good);
    }

    // ---------------------------------------------------------------- dynamics
    if (stats.lra > 0.0f)
    {
        if (stats.lra < 3.0f)
            add ("lra-flat", T ("动态被过度压缩", "Dynamics over-compressed"),
                 TF (T ("响度范围仅 {v} LU（参考 5–12 LU）。持续压缩会让音乐失去起伏与呼吸感。",
                        "Loudness range is only {v} LU (5-12 LU is typical). Constant compression "
                        "flattens the rise and fall of the music."),
                     { { "v", one (stats.lra) } }),
                 Severity::warn);
        else if (stats.lra > 16.0f)
            add ("lra-wide", T ("响度范围偏大", "Loudness range very wide"),
                 TF (T ("响度范围 {v} LU，段落音量差异明显，检查是否有段落未做电平对齐。",
                        "Loudness range is {v} LU. Sections differ a lot in level - check whether "
                        "one of them was left unaligned."),
                     { { "v", one (stats.lra) } }),
                 Severity::info);
    }

    if (stats.integrated > -70.0f && stats.truePeakDb > -70.0f)
    {
        const float plr = stats.getPlr();

        if (plr < 8.0f)
            add ("plr-low", T ("瞬态被压扁", "Transients squashed"),
                 TF (T ("峰响比仅 {v} dB（参考 10–16 dB）。鼓点与拨弦的冲击力被限制器吃掉，声音趋于扁平。",
                        "Peak-to-loudness ratio is only {v} dB (10-16 dB is typical). The limiter "
                        "is eating the punch of kicks and plucks."),
                     { { "v", one (plr) } }),
                 Severity::warn);
    }

    // ------------------------------------------------------------------ stereo
    if (stats.stereo)
    {
        if (stats.correlation < 0.0f)
            add ("corr-neg", T ("存在相位冲突", "Phase conflict"),
                 TF (T ("左右声道相关性 {v}，低于 0。折叠为单声道时中低频会被抵消，手机 / 蓝牙音箱上会明显变薄。",
                        "Channel correlation is {v}, below zero. Folding to mono cancels the "
                        "low-mids and the mix will sound thin on phones and Bluetooth speakers."),
                     { { "v", twoSigned (stats.correlation) } }),
                 Severity::critical);
        else if (stats.correlation < 0.15f && stats.shortTerm > -50.0f)
            add ("corr-low", T ("单声道兼容性偏低", "Mono compatibility weak"),
                 TF (T ("声道相关性仅 {v}（理想 0.3–0.8）。折叠单声道后侧向内容会减少，检查过宽的立体声效果器与 Haas 延迟。",
                        "Correlation is only {v} (0.3-0.8 is typical). Mono folding will lose side "
                        "content - check very wide stereo effects and Haas delays."),
                     { { "v", twoSigned (stats.correlation) } }),
                 Severity::warn);

        if (stats.shortTerm > -50.0f)
        {
            if (stats.widthPct < 15.0f)
                add ("width-narrow", T ("立体声宽度偏窄", "Stereo image very narrow"),
                     TF (T ("侧向能量仅 {v}%，接近单声道。检查是否有声道重复、双单声道录音或过度中置。",
                            "Side energy is only {v}% - almost mono. Check for doubled channels, "
                            "dual-mono recordings or heavy mono-ing."),
                         { { "v", one (stats.widthPct) } }),
                     Severity::info);
            else if (stats.widthPct > 150.0f)
                add ("width-wide", T ("立体声宽度过度", "Stereo image too wide"),
                     TF (T ("侧向能量 {v}%，远超中置。单声道折叠损失大，注意相位与兼容性。",
                            "Side energy is {v}%, far above the mid. Mono folding loses a lot - "
                            "watch phase and compatibility."),
                         { { "v", one (stats.widthPct) } }),
                     Severity::warn);

            if (stats.lowSidePct > 35.0f)
                add ("low-side", T ("低频不是单声道", "Low end is not mono"),
                     TF (T ("120 Hz 以下侧向能量 {v}%。俱乐部 / 黑胶系统上低频会不稳定，甚至相位抵消；建议对 120 Hz 以下做单声道处理。",
                            "Side energy below 120 Hz is {v}%. Club and vinyl systems make the low "
                            "end unstable or even cancel it - mono the band below 120 Hz."),
                         { { "v", one (stats.lowSidePct) } }),
                     Severity::warn);
        }
    }

    // --------------------------------------------------------------- artifacts
    if (stats.clipEvents > 0)
        add ("clip", T ("检出削波失真", "Clipping detected"),
             TF (T ("共 {n} 次采样级削波（波形顶部被削平）。降低增益或限制器阈值 1–2 dB 后重新检查。",
                    "{n} sample-level clipping events (the waveform tops are flattened). Lower the "
                    "gain or the limiter threshold by 1-2 dB and check again."),
                 { { "n", juce::String (stats.clipEvents) } }),
             Severity::critical, true);

    if (stats.oversCount > 0)
        add ("overs", T ("检出跨采样过载", "Inter-sample overs detected"),
             TF (T ("{n} 个 100 ms 区间内重建峰值超过 0 dBTP。编码为有损格式后会被削波，建议上限设为 -1 dBTP。",
                    "Reconstructed peaks exceed 0 dBTP in {n} of the 100 ms windows. Lossy encoders "
                    "will clip them - set the ceiling to -1 dBTP."),
                 { { "n", juce::String (stats.oversCount) } }),
             Severity::warn, true);

    if (stats.clickEvents > 0)
        add ("clicks", T ("检出瞬态异常", "Clicks / edit points detected"),
             TF (T ("检测到 {n} 次采样跳变（可能是剪辑点、爆音或脉冲噪声）。放大波形检查接缝处。",
                    "{n} sudden sample jumps (edit points, pops or impulsive noise). Zoom into the "
                    "waveform and inspect the joins."),
                 { { "n", juce::String (stats.clickEvents) } }),
             Severity::info, true);

    if (stats.dcOffsetDb > -46.0f)
        add ("dc", T ("存在直流偏移", "DC offset present"),
             TF (T ("直流电平 {v} dBFS（4 秒均值）。会侵占动态余量、导致不对称削波，建议加 30 Hz 高通。",
                    "DC level is {v} dBFS (4 s mean). It eats headroom and causes asymmetric "
                    "clipping - add a 30 Hz high-pass."),
                 { { "v", one (stats.dcOffsetDb) } }),
             Severity::warn);

    // ----------------------------------------------------------------- spectral
    if (signal && hasTonalBasis && bandDevs.size() == (size_t) SpectrumAnalyzer::numBands)
    {
        const auto dev = [&bandDevs] (int i) { return (double) bandDevs[(size_t) i]; };
        const auto label = [] (int i)
        {
            const auto& info = SpectrumAnalyzer::getBandInfo (i);
            return T (info.label, info.labelEn);
        };

        if (dev (3) > 4.5)
            add ("mud", T ("中低频堆积（浑浊）", "Low-mids muddy"),
                 TF (T ("250–500 Hz 能量比参考高 {v} dB。鼓组与贝斯容易糊在一起；尝试衰减 300–400 Hz 或给非低频乐器加高通。",
                        "250-500 Hz sits {v} dB above the reference. Kicks and bass blur together - "
                        "try cutting 300-400 Hz or high-passing the non-bass instruments."),
                     { { "v", oneSigned (dev (3)) } }),
                 Severity::warn);

        if (dev (6) > 4.5)
            add ("harsh", T ("中高频过硬（刺耳）", "Harsh upper mids"),
                 TF (T ("2–5 kHz 比参考高 {v} dB。长时间聆听容易听觉疲劳；检查失真吉他、人声 3 kHz 附近与过载的激励器。",
                        "2-5 kHz sits {v} dB above the reference. Long listening sessions become "
                        "fatiguing - check distorted guitars, the 3 kHz vocal area and driven exciters."),
                     { { "v", oneSigned (dev (6)) } }),
                 Severity::warn);

        if (dev (7) > 4.5)
            add ("sib", T ("齿音过重", "Sibilance too strong"),
                 TF (T ("5–9 kHz 比参考高 {v} dB（齿音区）。对人声使用动态均衡或齿音消除器。",
                        "5-9 kHz sits {v} dB above the reference (the sibilance band). Use a dynamic "
                        "EQ or a de-esser on the vocals."),
                     { { "v", oneSigned (dev (7)) } }),
                 Severity::warn);

        if (dev (8) < -5.0)
            add ("air-low", T ("高频空气感不足", "Lacks air"),
                 TF (T ("9–16 kHz 比参考低 {v} dB，整体听感偏闷。可适度提升高频搁架，注意不要制造齿音。",
                        "9-16 kHz sits {v} dB below the reference and the mix feels dull. A gentle "
                        "high shelf helps - but do not create sibilance."),
                     { { "v", oneSigned (dev (8)) } }),
                 Severity::info);

        if (dev (0) > 5.5)
            add ("sub", T ("超低频过重", "Too much sub"),
                 TF (T ("20–60 Hz 比参考高 {v} dB。小尺寸设备上会浪费功率并提前触发限制器，建议收紧 40 Hz 以下。",
                        "20-60 Hz sits {v} dB above the reference. It wastes power on small speakers "
                        "and trips the limiter early - tighten everything below 40 Hz."),
                     { { "v", oneSigned (dev (0)) } }),
                 Severity::info);

        if (dev (2) < -5.0)
            add ("lowmid-hole", T ("低中频塌陷", "Low-mid hole"),
                 TF (T ("120–250 Hz 比参考低 {v} dB，声音容易发空、缺乏厚度；检查高通频率与相位抵消。",
                        "120-250 Hz sits {v} dB below the reference, so the mix sounds hollow. Check "
                        "high-pass frequencies and phase cancellation."),
                     { { "v", oneSigned (dev (2)) } }),
                 Severity::info);

        const double brightTilt = (dev (6) + dev (7) + dev (8)) / 3.0
                                - (dev (1) + dev (2) + dev (3)) / 3.0;

        if (brightTilt > 6.0)
            add ("tilt-bright", T ("整体音色偏亮", "Overall tone is bright"),
                 TF (T ("高频段平均比中低频高 {v} dB，长时间聆听偏刺；适当降低高频搁架或中高频动态。",
                        "The top end averages {v} dB above the low-mids, which gets fatiguing. Ease "
                        "off the high shelf or tame the upper-mid dynamics."),
                     { { "v", oneSigned (brightTilt) } }),
                 Severity::warn);
        else if (brightTilt < -6.0)
            add ("tilt-dark", T ("整体音色偏暗", "Overall tone is dark"),
                 TF (T ("高频段平均比中低频低 {v} dB，声音偏闷、缺乏细节；检查高频衰减与母带均衡。",
                        "The top end averages {v} dB below the low-mids, so the mix sounds dull. Check "
                        "high-frequency attenuation and mastering EQ."),
                     { { "v", oneSigned (brightTilt) } }),
                 Severity::warn);

        for (int b = 1; b <= 8; ++b)
        {
            if (dev (b) < -8.0)
            {
                add ("hole-" + juce::String (b), T ("频段空缺", "Spectral hole"),
                     TF (T ("{band} 频段比参考低 {v} dB，可能是滤波过度、梳状滤波或相位抵消。",
                            "The {band} band sits {v} dB below the reference - likely over-filtering, "
                            "comb filtering or phase cancellation."),
                         { { "band", label (b) }, { "v", oneSigned (dev (b)) } }),
                     Severity::info);
                break;
            }
        }
    }
    else if (signal && ! hasTonalBasis)
    {
        add ("basis-missing", T ("频谱不设绝对目标", "No absolute spectral target"),
             T ("未载入参考轨，且专辑中位不足两条；频谱平衡只会在有参考 / 专辑基准时给出判断。",
                "No reference tracks and fewer than two album tracks: spectral balance is only judged "
                "against a reference or album median."),
             Severity::info);
    }

    bool anyBad = false;

    // ---- signal integrity (v1.4.2) -----------------------------------------
    if (signal && defects.ready)
    {
        if (defects.distortionRisk >= 55.0f)
            add ("sig-dist", T ("谐波 / 互调失真", "Harmonic / IMD distortion"),
                 TF (T ("2–5 次谐波高于局部谱底 {h} dB，互调边带 +{i} dB。信号已被非线性级塑形；"
                        "先降 0.5–1 dB 输出，再检查饱和与限幅的过采样。",
                        "2nd-5th harmonics sit {h} dB above the local floor and IMD sidebands are "
                        "+{i} dB. A nonlinear stage is shaping the signal - drop 0.5-1 dB, then check "
                        "saturation and limiter oversampling."),
                     { { "h", one (defects.harmonicExcessDb) }, { "i", one (defects.imdSidebandDb) } }),
                 Severity::warn);

        if (defects.flatTopPct > 0.05f)
            add ("sig-flat", T ("波形平顶（硬削波）", "Flat-topped waveform"),
                 TF (T ("{p}% 的样本停在平顶上，属于硬削波而不是软限幅；高频会出现混叠并伴随瞬态损失。",
                        "{p}% of samples sit on a flat top - that is hard clipping, not soft limiting, "
                        "with aliasing and transient loss to match."),
                     { { "p", juce::String (defects.flatTopPct, 2) } }),
                 defects.flatTopPct > 0.5f ? Severity::critical : Severity::warn);

        if (defects.aliasRisk >= 55.0f)
            add ("sig-alias", T ("高频混叠风险", "Aliasing risk"),
                 TF (T ("16–20 kHz 比自然滚降高 {e} dB（高频斜率 {t} dB）。常见于未过采样的削波 / 限幅器。",
                        "16-20 kHz sits {e} dB above the natural roll-off (HF tilt {t} dB). Usually a "
                        "clipping or limiting stage without oversampling."),
                     { { "e", one (defects.aliasExcessDb) }, { "t", one (defects.hfTiltDb) } }),
                 Severity::warn);

        if (defects.subRisk >= 55.0f)
            add ("sig-sub", T ("超低频堆积", "Sub-bass build-up"),
                 TF (T ("40 Hz 以下占总能量 {s}%，20–40 Hz 比 40–80 Hz 高 {t} dB。小扬声器会浪费功率并提前触发限幅。",
                        "Below 40 Hz takes {s}% of the total energy and 20-40 Hz sits {t} dB above "
                        "40-80 Hz. Small speakers waste power and hit the limiter early."),
                     { { "s", one (defects.subSharePct) }, { "t", one (defects.subTiltDb) } }),
                 Severity::info);

        if (defects.transientScore < 45.0f)
            add ("sig-transient", T ("瞬态完整性下降", "Transient integrity lost"),
                 TF (T ("瞬态得分 {s}/100，PLR {p} dB（峰响损失 {c} dB）。攻击被限幅压平，鼓点会失去冲击力。",
                        "Transient score {s}/100 with PLR {p} dB ({c} dB crest loss). Attacks are "
                        "flattened, so drum hits lose their impact."),
                     { { "s", juce::String (juce::roundToInt (defects.transientScore)) },
                       { "p", one (stats.getPlr()) }, { "c", one (defects.crestLossDb) } }),
                 Severity::warn);

        if (defects.monoRisk >= 40.0f)
            add ("sig-mono", T ("单声道折叠内容损失", "Mono fold loss"),
                 TF (T ("折叠到 M=(L+R)/2 后损失 {l} dB（保留 {r}%，低频保留 {rl}%）。手机、Club 单声道 PA 会直接听到这个损失。",
                        "Folding to M=(L+R)/2 loses {l} dB (retains {r}%, low end {rl}%). Phone speakers "
                        "and mono club PAs hear exactly that loss."),
                     { { "l", one (defects.monoFoldLossDb) }, { "r", one (defects.monoRetainPct) },
                       { "rl", one (defects.monoLowRetainPct) } }),
                 defects.monoRisk >= 70.0f ? Severity::critical : Severity::warn);

        if (defects.maskingRisk >= 55.0f)
            add ("sig-masking", T ("中频掩蔽风险", "Mid masking risk"),
                 TF (T ("200–800 Hz 比 1–4 kHz 存在感高 {v} dB，人声与军鼓的清晰度会被低中能量掩蔽。",
                        "200-800 Hz sits {v} dB above the 1-4 kHz presence region, so low-mid energy "
                        "masks vocals and snare clarity."),
                     { { "v", one (defects.maskingDb) } }),
                 Severity::info);

        if (defects.attributionCount >= 2)
            add ("sig-link", T ("多层联动异常（已归因）", "Correlated multi-layer issue"),
                 T ("多个信号层同时越线：检查末级限幅 / 削波是否同时造成真峰值、瞬态与高频失真；"
                    "若低频层也越线，则先处理 20–40 Hz 侧向内容，再复查单声道折叠。",
                    "Several signal layers exceed their thresholds together. Check whether the final "
                    "limiter/clipper is causing peak, transient and HF distortion at once; if the "
                    "low-end layer is also over, fix 20-40 Hz side content before re-checking mono fold."),
                 Severity::critical);
    }

    for (const auto& f : found)
        if (f.severity == Severity::warn || f.severity == Severity::critical)
            anyBad = true;

    // ---- positive confirmations (kept after the warnings in the list) ------
    if (signal)
    {
        if (stats.clipEvents == 0 && stats.oversCount == 0 && stats.clickEvents == 0)
            add ("no-clip", T ("未检出削波或爆音", "No clipping or clicks"),
                 T ("采样级削波、跨采样过载与瞬态跳变检查均为 0，波形完整。",
                    "Sample clipping, inter-sample overs and click checks all read zero - the "
                    "waveform is intact."),
                 Severity::good);

        if (stats.stereo && stats.correlation >= 0.15f && stats.lowSidePct <= 35.0f
            && stats.shortTerm > -50.0f)
            add ("mono-ok", T ("单声道兼容性良好", "Mono compatible"),
                 TF (T ("声道相关性 {c}，120 Hz 以下侧向能量仅 {s}%，折叠单声道不会明显抵消。",
                        "Correlation is {c} and side energy below 120 Hz is only {s}%, so mono "
                        "folding will not cancel noticeably."),
                     { { "c", twoSigned (stats.correlation) }, { "s", one (stats.lowSidePct) } }),
                 Severity::good);

        if (stats.integrated > -70.0f && stats.lra >= 3.0f && stats.lra <= 16.0f
            && stats.getPlr() >= 10.0f)
            add ("dyn-ok", T ("动态范围健康", "Dynamics healthy"),
                 TF (T ("响度范围 {lra} LU、峰响比 {plr} dB，瞬态与段落起伏保留良好。",
                        "Loudness range {lra} LU and peak-to-loudness {plr} dB - transients and "
                        "section dynamics are preserved."),
                     { { "lra", one (stats.lra) }, { "plr", one (stats.getPlr()) } }),
                 Severity::good);
    }

    if (signal && hasTonalBasis && bandDevs.size() == (size_t) SpectrumAnalyzer::numBands)
    {
        float worst = 0.0f;

        for (int b = 0; b < SpectrumAnalyzer::numBands; ++b)
            worst = juce::jmax (worst, std::abs (bandDevs[(size_t) b]));

        if (worst < 4.5f)
            add ("tonal-ok", T ("频段平衡正常", "Tonal balance is even"),
                 T ("各频段相对参考的偏差都在 ±4.5 dB 内，未发现浑浊、刺耳或齿音过重。",
                    "Every band stays within ±4.5 dB of the reference - no mud, harshness or "
                    "sibilance issues."),
                 Severity::good);
    }

    if (! anyBad && signal)
        add ("summary", T ("混音质检通过", "Mix check passed"),
             T ("响度、动态、声场、频段平衡与失真检查均处于合理范围，可按 -1 dBTP 上限输出。",
                "Loudness, dynamics, stereo image, tonal balance and distortion checks are all in "
                "range - the mix is ready at a -1 dBTP ceiling."),
             Severity::good);

    apply (found, stats);
}

//==============================================================================
void Advisor::apply (const std::vector<Issue>& found, const StatsSnapshot& stats)
{
    std::map<juce::String, bool> present;

    for (const auto& f : found)
        present[f.id] = true;

    for (const auto& f : found)
    {
        auto& c = conditions[f.id];

        if (c.onTicks == 0)
            c.firstSeen = stats.elapsed;

        c.onTicks = juce::jmin (100000, c.onTicks + 1);
        c.offTicks = 0;
        c.issue = f;
        c.issue.detectedAt = c.firstSeen;
    }

    for (auto& kv : conditions)
    {
        if (present.find (kv.first) == present.end())
        {
            kv.second.onTicks = 0;
            ++kv.second.offTicks;
        }
    }

    visible.clear();

    for (auto& kv : conditions)
    {
        const auto& c = kv.second;
        const int required = c.issue.timed ? 1 : 3;

        if (c.onTicks >= required)
            visible.push_back (c.issue);
    }

    std::sort (visible.begin(), visible.end(), [] (const Issue& a, const Issue& b)
    {
        if (a.severity != b.severity)
            return (int) a.severity > (int) b.severity;

        return a.detectedAt < b.detectedAt;
    });

    if (visible.size() > 32)
        visible.resize (32);

    overall = Severity::good;

    for (const auto& i : visible)
        overall = (Severity) juce::jmax ((int) overall, (int) i.severity);

    for (auto it = conditions.begin(); it != conditions.end();)
    {
        if (it->second.onTicks == 0 && it->second.offTicks > 60)
            it = conditions.erase (it);
        else
            ++it;
    }
}
} // namespace ml





