#include "PluginEditor.h"

using ml::T;
#include "Delivery.h"
#include "GenreWar.h"
#include "Version.h"

//==============================================================================
juce::String MasterLensAudioProcessorEditor::buildReport() const
{
    juce::String r;
    const float ceiling = processor.getCeilingDb();
    const auto& s = stats;
    const auto basis = currentBasis();
    const auto platform = ml::platformSummary (s);

    r << "MasterLens " << ml::kVersion << " by " << ml::kOrganisation
      << T (" · 母带交付检查报告\n", " - mastering QC report\n");
    r << T ("生成时间：", "Generated: ") << juce::Time::getCurrentTime().toISO8601 (true) << "\n";
    r << T ("采样率：", "Sample rate: ") << (int) processor.getSampleRateValue() << " Hz   "
      << T ("声道：", "Channels: ") << (s.stereo ? T ("立体声", "stereo") : T ("单声道", "mono"))
      << T ("   覆盖：", "   coverage: ") << ml::fmt1 (s.coveragePct) << "%\n\n";

    r << T ("【响度与动态】\n", "[Loudness & dynamics]\n");
    r << "  I            : " << ml::fmtDb (s.integrated) << " LUFS";

    if (basis.hasComparison() && basis.mode != ml::BasisMode::manual)
        r << T ("（基准 ", " (basis ") << basis.label() << "  "
             << ml::sgn (s.integrated - basis.comparisonLufs(), 1) << " LU)\n";
    else
        r << T ("（平台归一化：最坏衰减 ", " (platform: worst cut ")
             << ml::fmt1 (platform.maxCutDb) << T (" dB，丢弃 ", " dB, wasted ")
             << ml::fmt1 (platform.maxWastedDb) << " LU)\n";

    r << "  S / M        : " << ml::fmtDb (s.shortTerm) << " / " << ml::fmtDb (s.momentary) << " LUFS\n";
    r << "  LRA / PLR    : " << ml::fmt1 (s.lra) << " LU / " << ml::fmt1 (s.getPlr()) << " dB\n";
    r << "  PSR (min)    : " << ml::fmtDb (s.psr) << " / " << ml::fmtDb (s.psrMin) << " dB"
      << " @ " << ml::fmtTime (s.psrMinTime) << "\n\n";

    r << T ("【真峰值】\n", "[True peak]\n");
    r << "  TP max       : " << ml::fmtDb (s.truePeakDb) << " dBTP"
      << T ("（上限 ", " (ceiling ") << ml::num (ceiling) << " dBTP)\n";
    r << T ("  最大值位置   : ", "  Maximum at   : ") << ml::fmtTime (s.truePeakMaxTime) << "\n";
    r << T ("  跨采样过载   : ", "  Over windows : ") << s.oversCount << "\n";
    r << "  L / R / Side : " << ml::fmtDb (s.truePeakLeft) << " / "
      << ml::fmtDb (s.truePeakRight) << " / " << ml::fmtDb (s.sideTruePeak) << " dBTP\n";

    for (const auto& e : tpEvents)
        r << "    " << ml::fmtTime (e.time) << "  +" << ml::fmt1 (e.overshootDb) << " dB\n";

    r << "\n" << T ("【频谱 / 立体声】\n", "[Spectrum / stereo]\n");
    r << "  20-250 / 250-2k / 2k-8k / 8k-20k : "
      << ml::fmt1 (bandRatios[0]) << "% / " << ml::fmt1 (bandRatios[1]) << "% / "
      << ml::fmt1 (bandRatios[2]) << "% / " << ml::fmt1 (bandRatios[3]) << "%\n";
    r << "  corr / width / low-side : " << ml::sgn (s.correlation, 2) << " / "
      << ml::fmt1 (s.widthPct) << "% / " << ml::fmt1 (s.lowSidePct) << "%\n";

    r << "\n" << T ("【平台交付预测】\n", "[Delivery prediction]\n");

    for (int i = 0; i < ml::numPlatforms; ++i)
    {
        const auto& p = ml::platform (i);
        const auto res = ml::evaluate (s, p);

        r << "  " << T (p.nameZh, p.nameEn) << " : "
          << T ("增益 ", "gain ") << ml::sgn (res.gainDb) << " dB, "
          << T ("预测 TP ", "predicted TP ") << ml::fmt1 (res.predictedTp) << " dBTP, "
          << T ("服务限幅 ", "limited ") << ml::fmt1 (res.limitedDb) << " dB, "
          << T ("被丢弃响度 ", "wasted loudness ") << ml::fmt1 (res.wastedLoudnessDb) << " LU, "
          << (res.state == ml::DeliveryState::over ? T ("超限", "OVER")
            : res.state == ml::DeliveryState::warning ? T ("临界", "WARNING") : T ("可行", "OK")) << "\n";
    }

    r << "\n" << T ("【曲风响度目标 / 交付要求】\n", "[Genre loudness target / delivery requirements]\n");
    {
        const int gi = juce::jlimit (0, ml::presets::customIndex - 1, targetGenreIndex);
        const auto& g = ml::presets::entries[gi];
        r << T ("  曲风：", "  Genre: ") << T (g.zh, g.en)
          << T ("  参与度 ", "  participation ") << juce::String (juce::roundToInt (targetWarPct)) << "%\n";
        r << T ("  目标 / LRA / TP：", "  Target / LRA / TP: ")
          << ml::fmt1 (ml::presets::war::warTargetLufs (gi, targetWarPct)) << " LUFS / "
          << ml::fmt1 (ml::presets::war::warTargetLra (gi, targetWarPct)) << " LU / "
          << ml::fmt1 (g.ceiling) << " dBTP\n";
        r << T ("  尾部数字静音：", "  Tail digital silence: ") << ml::fmt1 (s.tailSilenceMs / 1000.0f) << " s\n";

        for (int i = 0; i < ml::numPlatforms; ++i)
        {
            const auto& p = ml::platform (i);
            r << "    " << T (p.nameZh, p.nameEn) << " : " << ml::num (p.targetLufs) << " LUFS / "
              << ml::num (p.tpCeiling) << " dBTP / " << T (p.codecZh, p.codecEn);
            if (p.minTailSilenceMs > 0)
                r << T (" / 曲尾至少 ", " / tail >= ") << p.minTailSilenceMs << " ms";
            r << "\n";
        }
    }

    r << "\n" << T ("【信号完整性】\n", "[Signal integrity]\n");
    r << "  " << T ("谐波堆积 / 互调边带 / 平顶 ", "harmonics / IMD / flat tops ") << " : "
      << ml::fmt1 (signalDefects.harmonicExcessDb) << " dB / +"
      << ml::fmt1 (signalDefects.imdSidebandDb) << " dB / "
      << ml::fmt2 (signalDefects.flatTopPct) << "%\n";
    r << "  " << T ("混叠风险 / 高频斜率        ", "alias risk / HF tilt          ") << " : "
      << juce::String (juce::roundToInt (signalDefects.aliasRisk)) << " / "
      << ml::sgn (signalDefects.hfTiltDb) << " dB\n";
    r << "  " << T ("超低占比 / 20-40 斜率       ", "sub share / 20-40 tilt          ") << " : "
      << ml::fmt1 (signalDefects.subSharePct) << "% / "
      << ml::sgn (signalDefects.subTiltDb) << " dB\n";
    r << "  " << T ("瞬态得分 / 峰响损失         ", "transient / crest loss          ") << " : "
      << juce::String (juce::roundToInt (signalDefects.transientScore)) << "/100 / "
      << ml::fmt1 (signalDefects.crestLossDb) << " dB\n";
    r << "  " << T ("单声道折叠损失 / 保留       ", "mono fold loss / retained       ") << " : "
      << ml::fmt1 (signalDefects.monoFoldLossDb) << " dB / "
      << ml::fmt1 (signalDefects.monoRetainPct) << "% ("
      << T ("低频 ", "low ") << ml::fmt1 (signalDefects.monoLowRetainPct) << "%)\n";
    r << "  " << T ("中频掩蔽                    ", "mid masking                    ") << " : "
      << ml::sgn (signalDefects.maskingDb) << " dB\n";
    r << "  " << T ("联动层                      ", "correlated layers              ") << " : "
      << signalDefects.attributionCount << "\n";

    if (reference.hasStats())
    {
        const auto rs = reference.getStats();
        r << "\n" << T ("【参考作品对比】\n", "[Reference comparison]\n");
        r << "  " << T ("综合响度 ", "integrated ") << ml::fmt1 (rs.integrated) << " LUFS"
          << T ("（差 ", " (delta ") << ml::sgn (s.integrated - rs.integrated) << " LU)\n";
        r << "  LRA      " << ml::fmt1 (rs.lra) << " LU"
          << T ("（差 ", " (delta ") << ml::sgn (s.lra - rs.lra) << " LU)\n";
        r << "  PLR      " << ml::fmt1 (rs.getPlr()) << " dB"
          << T ("（差 ", " (delta ") << ml::sgn (s.getPlr() - rs.getPlr()) << " dB)\n";
        r << "  " << T ("超低占比 ", "sub share ") << ml::fmt1 (rs.subSharePct) << "%"
          << T ("（差 ", " (delta ") << ml::sgn (s.subSharePct - rs.subSharePct) << "%)\n";
    }

    r << "\n" << T ("【时间线标记】\n", "[Timeline markers]\n");

    if (markers.empty())
        r << T ("  无\n", "  none\n");

    for (const auto& m : markers)
        r << "  " << ml::fmtTime (m.time) << "  " << m.text << "\n";

    r << "\n" << T ("【诊断】\n", "[Diagnostics]\n");

    if (issueCache.empty())
        r << T ("  未发现需要处理的项目\n", "  No action items\n");

    for (const auto& i : issueCache)
    {
        const juce::String tag = i.severity == ml::Severity::critical ? T ("需要处理", "Action")
                               : i.severity == ml::Severity::warn ? T ("建议检查", "Review")
                               : i.severity == ml::Severity::info ? T ("可优化", "Improve")
                                                                  : T ("良好", "Good");
        r << "  [" << tag << "] " << i.title << "\n    " << i.detail << "\n";
    }

    r << "\n" << T ("【专辑统一 / 交付门禁】\n", "[Album consistency / release gate]\n");
    r << T ("  专辑：", "  Album: ") << albumSnapshot.albumName
      << T ("  rev ", "  rev ") << albumSnapshot.revision
      << T ("  曲目 ", "  tracks ") << (int) albumSnapshot.tracks.size() << "\n";
    r << T ("  门禁：", "  Gate: ")
      << (albumGate.blocked ? T ("阻断", "BLOCKED")
                            : (albumGate.incomplete ? T ("未采集", "INCOMPLETE") : T ("放行", "PASS")))
      << T ("  阻断项 ", "  blockers ") << albumGate.blockers << "\n";

    for (const auto& reason : albumGate.reasons)
        r << "    - " << reason << "\n";

    if (! albumSnapshot.tracks.empty())
    {
        for (const auto& t : albumSnapshot.tracks)
            r << "    " << t.label << "  " << ml::fmtDb (t.stats.integrated) << " LUFS  "
              << ml::fmt1 (t.stats.lra) << " LU  " << ml::fmt1 (t.stats.getPlr()) << " PLR  "
              << ml::fmtDb (t.stats.truePeakDb) << " dBTP\n";

        r << T ("  与上版：新增 ", "  vs previous: new ") << albumDiff.added
          << T ("，恶化 ", ", worse ") << albumDiff.worsened
          << T ("，改善 ", ", better ") << albumDiff.improved << "\n";
    }

    r << "\n" << T ("开发者 ", "Developer ") << ml::kDeveloper
      << T (" · 机构 ", " · Organisation ") << ml::kOrganisation << " · v" << ml::kVersion << "\n";

    return r;
}
