#include "PluginEditor.h"

using ml::T;
#include "Delivery.h"
#include "GenreWar.h"
#include "ui/StatusText.h"

using ml::statusLabel;
using ml::statusColour;

//==============================================================================
void MasterLensAudioProcessorEditor::updateRail()
{
    rail.beginBatch();

    const float target = processor.getTargetLufs();
    const float ceiling = processor.getCeilingDb();
    const bool live = stats.hasSignal;
    const auto basis = currentBasis();
    const auto platform = ml::platformSummary (stats);

    int worst = 0;

    const auto note = [&worst] (int level)
    {
        if (level > worst)
            worst = level;
    };

    // 0 loudness / basis
    {
        const bool valid = live && stats.integrated > -70.0f;

        if (valid && basis.hasComparison() && basis.mode != ml::BasisMode::manual)
        {
            const float diff = stats.integrated - basis.comparisonLufs();
            const int level = std::abs (diff) <= 1.0f ? 0 : (std::abs (diff) <= 2.0f ? 1 : 2);
            note (level);

            rail.setModule (0, T ("响度基准", "Loudness"),
                            T ("Δ ", "Δ ") + ml::sgn (diff, 1),
                            basis.label() + ml::sep() + ml::statusLabel (level),
                            ml::statusColour (level));
        }
        else if (valid && basis.mode == ml::BasisMode::manual)
        {
            const float diff = stats.integrated - target;
            const int level = std::abs (diff) <= 1.5f ? 0 : 1;
            note (level);

            rail.setModule (0, T ("响度基准", "Loudness"),
                            T ("Δ ", "Δ ") + ml::sgn (diff, 1),
                            T ("手动参考", "manual reference"), ml::statusColour (level));
        }
        else
        {
            const int level = ! valid ? -1
                            : (platform.maxLimitedDb > 0.1f ? 2
                            : (platform.maxWastedDb > 3.0f ? 1 : 0));

            if (valid) note (level);
            rail.setModule (0, T ("响度基准", "Loudness"),
                            valid ? T ("衰减 ", "cut ") + ml::num (platform.maxCutDb) + " dB" : ml::noValue(),
                            valid ? T ("归一化后感知不变", "level unchanged after norm")
                                  : ml::statusLabel (-1),
                            ml::statusColour (level));
        }
    }

    // 1 true peak
    {
        const bool valid = live && stats.truePeakDb > -70.0f;
        const float headroom = ceiling - stats.truePeakDb;
        const int level = ! valid ? -1 : (headroom < 0.0f ? 3 : (headroom < 1.0f ? 2 : 0));

        if (valid) note (level);
        rail.setModule (1, T ("真峰值", "True peak"),
                        valid ? ml::num (stats.truePeakDb) + " dBTP" : ml::noValue(),
                        statusLabel (level), statusColour (level));
    }

    // 2 spectrum
    {
        const auto devs = currentBandDevs();
        float worstDev = 0.0f;

        for (float d : devs)
            worstDev = juce::jmax (worstDev, std::abs (d));

        const bool valid = live && basis.hasComparison() && basis.mode != ml::BasisMode::manual
                           && ! devs.empty() && worstDev > 0.0f;
        const int level = ! valid ? -1 : (worstDev < 4.5f ? 0 : (worstDev < 6.0f ? 1 : 2));

        if (valid) note (level);
        rail.setModule (2, T ("频谱平衡", "Spectrum"),
                        valid ? ml::plusMinus() + ml::num (worstDev) + " dB"
                              : (basis.hasComparison() ? ml::noValue() : T ("无基准", "no basis")),
                        valid ? statusLabel (level) : T ("仅对照参考", "reference only"),
                        valid ? ml::statusColour (level) : ml::theme::textFaint);
    }

    // 3 stereo
    {
        const bool valid = live && stats.stereo;
        const int level = ! valid ? -1 : (stats.correlation < 0.0f ? 3
                        : (stats.correlation < 0.15f ? 2 : 0));

        if (valid) note (level);
        rail.setModule (3, T ("立体声 / 相位", "Stereo / phase"),
                        valid ? T ("相关 ", "corr ") + ml::sgn (stats.correlation, 2) : ml::noValue(),
                        statusLabel (level), statusColour (level));
    }

    // 4 delivery
    {
        const bool valid = live && stats.integrated > -70.0f;
        const int level = ! valid ? -1 : (platform.overCount > 0 ? 3
                        : (platform.warningCount > 0 || platform.maxLimitedDb > 0.05f ? 2 : 0));

        if (valid) note (level);
        rail.setModule (4, T ("平台归一化", "Normalisation"),
                        valid ? juce::String (ml::numPlatforms - platform.overCount - platform.warningCount)
                                    + "/" + juce::String (ml::numPlatforms)
                              : ml::noValue(),
                        statusLabel (level), statusColour (level));
    }

    // 5 album
    {
        const bool hasTracks = ! albumSnapshot.tracks.empty();
        const int warnings = (int) albumGate.risks.size() - albumGate.blockers;
        const int level = ! hasTracks ? -1 : (albumGate.blocked ? 3 : (warnings > 0 ? 2 : 0));

        if (level >= 0) note (level);
        rail.setModule (5, T ("专辑门禁", "Album gate"),
                        juce::String ((int) albumSnapshot.tracks.size()) + T (" 首", " tracks"),
                        hasTracks ? (albumGate.blocked ? T ("阻断", "BLOCK") : T ("放行", "PASS"))
                                  : T ("未采集", "No capture"),
                        hasTracks ? (albumGate.blocked ? ml::theme::bad
                                                       : (warnings > 0 ? ml::theme::warn : ml::theme::good))
                                  : ml::theme::textFaint);
    }

    // 6 signal health
    {
        const float worstRisk = signalDefects.worstRisk();
        const int level = ! live ? -1 : (worstRisk >= 65.0f ? 3
                                          : (worstRisk >= 35.0f ? 2 : 0));

        if (level >= 0) note (level);
        rail.setModule (6, T ("信号健康", "Signal health"),
                        live ? juce::String (juce::roundToInt (worstRisk)) + "%" : ml::noValue(),
                        statusLabel (level), statusColour (level));
    }

    // 7 diagnostics
    {
        const int critical = advisor.count (ml::Severity::critical);
        const int warned = advisor.count (ml::Severity::warn);
        const int level = ! live ? -1 : (critical > 0 ? 3 : (warned > 0 ? 2 : 0));

        if (level >= 0) note (level);
        rail.setModule (7, T ("混音诊断", "Diagnostics"),
                        juce::String ((int) issueCache.size()) + T (" 项", " items"),
                        statusLabel (level), statusColour (level));
    }

    // 8 timeline
    rail.setModule (8, T ("时间线", "Timeline"),
                    juce::String ((int) markers.size()) + T (" 标记", " markers"),
                    markers.empty() ? statusLabel (-1) : statusLabel (1),
                    markers.empty() ? statusColour (-1) : statusColour (1));

    // 10 offline file QC
    {
        const int done = fileResults.size();
        const bool busy = fileAnalyzer.isBusy();
        rail.setModule (10, T ("文件 QC", "File QC"),
                        done > 0 ? juce::String (done) + T (" 首", " files") : ml::noValue(),
                        busy ? T ("分析中", "analysing") : (done > 0 ? T ("已入专辑", "in album")
                                                                     : T ("拖入文件", "drop files")),
                        busy ? ml::theme::cyan : (done > 0 ? ml::theme::good : ml::theme::textFaint));
    }

    // 9 genre loudness target / platform delivery requirements
    {
        const auto target = ml::presets::war::warTargetLufs (targetGenreIndex, targetWarPct);
        rail.setModule (9, T ("曲风交付", "Genre / delivery"),
                        ml::num (target) + " LUFS",
                        juce::String (juce::roundToInt (targetWarPct)) + T ("% 参与度", "% war"),
                        ml::theme::accent);
    }

    if (! live && albumSnapshot.tracks.empty())
    {
        footerStatusText.clear();
        rail.setVerdict (T ("等待信号", "Waiting for signal"),
                         T ("播放音频后开始测量", "Play audio to start"),
                         ml::theme::textFaint);
    }
    else if (albumGate.blocked)
    {
        footerStatusText.clear();
        rail.setVerdict (T ("专辑门禁阻断", "Album gate blocked"),
                         juce::String (albumGate.blockers) + T (" 个阻断项；查看专辑页排序",
                                                              " blockers; see album ranking"),
                         ml::theme::bad);
    }
    else if (worst >= 3)
    {
        footerStatusText.clear();
        rail.setVerdict (T ("需要处理", "Action needed"),
                         T ("硬性超限，请先处理", "Hard limit exceeded"), ml::theme::bad);
    }
    else if (worst == 2)
    {
        footerStatusText.clear();
        rail.setVerdict (T ("建议检查", "Review suggested"),
                         T ("存在临界项", "Borderline readings"), ml::theme::warn);
    }
    else
    {
        footerStatusText.clear();
        rail.setVerdict (T ("未发现阻断项", "No blockers"),
                         T ("基准差异与动态风险已显示，不代表固定目标合格",
                            "Basis differences and dynamic risk are shown; no fixed target is applied"),
                         ml::theme::good);
    }

    if (processor.isAuditionActive())
    {
        footerStatusText = T ("试听中 · Esc 停止", "Auditioning · Esc to stop");
        footerStatusColour = ml::theme::accent;
    }

    rail.endBatch();
}
