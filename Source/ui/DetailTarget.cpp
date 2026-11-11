#include "Modules.h"
#include "../Delivery.h"
#include "../GenreWar.h"

namespace ml
{
void DetailArea::paintTarget (juce::Graphics& g)
{
    auto area = contentArea().withTrimmedTop (32);
    area.removeFromTop (60);   // child combo / slider / button
    area.removeFromTop (8);

    const auto& s = state.stats;
    const int genreIndex = juce::jlimit (0, presets::customIndex - 1, state.targetGenreIndex);
    const auto& e = presets::entries[genreIndex];
    const auto& n = presets::war::notes[genreIndex];
    const bool hasSignal = s.hasSignal;

    const int leftW = juce::roundToInt ((float) area.getWidth() * 0.43f);
    auto left = area.removeFromLeft (leftW);
    area.removeFromLeft (tokens::gap);
    auto right = area;

    // ---- genre / loudness-war plan -----------------------------------------
    {
        g.setColour (theme::cardAlt);
        g.fillRoundedRectangle (left.toFloat(), tokens::radiusCard);
        g.setColour (theme::border);
        g.drawRoundedRectangle (left.toFloat().reduced (0.5f), tokens::radiusCard, 1.0f);

        auto inner = left.reduced (14, 11);
        const bool compact = left.getHeight() < 320;
        const bool tiny = left.getHeight() < 220;

        g.setColour (theme::accent);
        g.setFont (getLabelFont());
        g.drawText (T ("曲风参照", "GENRE REFERENCE"),
                    inner.removeFromTop (16), juce::Justification::centredLeft, false);

        g.setColour (theme::text);
        g.setFont (getLabelFont());
        g.drawFittedText (T (e.zh, e.en), inner.removeFromTop (tiny ? 16 : (compact ? 20 : 22)),
                          juce::Justification::centredLeft, 1);

        auto currentRow = inner.removeFromTop (tiny ? 22 : (compact ? 30 : 38));
        const auto valueText = hasSignal ? num (s.integrated) : noValue();
        g.setColour (hasSignal ? theme::good : theme::textFaint);
        g.setFont (getValueFont());
        g.drawText (valueText, currentRow, juce::Justification::centredLeft, false);

        const auto valueWidth = juce::roundToInt (
            juce::GlyphArrangement::getStringWidth (getValueFont(), valueText));
        g.setColour (theme::textFaint);
        g.setFont (getLabelFont());
        g.drawText ("LUFS", currentRow.withTrimmedLeft (valueWidth + 7).withTrimmedTop (4),
                    juce::Justification::centredLeft, false);

        g.setColour (theme::textFaint);
        g.setFont (getLabelFont());
        g.drawText (hasSignal ? T ("当前综合响度", "current integrated")
                              : T ("等待信号", "waiting for signal"),
                    inner.removeFromTop (tiny ? 10 : (compact ? 13 : 15)),
                    juce::Justification::centredLeft, false);

        inner.removeFromTop (tiny ? 2 : (compact ? 5 : 8));

        const int rowCount = tiny ? 5 : 7;
        const int reservedNote = tiny ? 0 : (compact ? 34 : 52);
        const int rowH = juce::jlimit (15, 42,
            (inner.getHeight() - reservedNote) / juce::jmax (1, rowCount));
        const auto row = [&] (const juce::String& key, const juce::String& value)
        {
            auto r = inner.removeFromTop (rowH);

            g.setColour (theme::textFaint);
            g.setFont (getLabelFont());
            g.drawText (key, r.removeFromLeft (112), juce::Justification::centredLeft, false);

            g.setColour (theme::text);
            g.setFont (getValueFont());
            g.drawText (value, r, juce::Justification::centredRight, false);
        };

        row (T ("标准曲风参照", "Genre reference"), num (e.target) + " LUFS");

        if (! tiny)
            row (T ("发行观测区间", "Released range"),
                 num (n.lufsLo) + " " + enDash() + " " + num (n.lufsHi) + " LUFS");

        row (T ("目标（滑块）", "Target (slider)"), num (state.targetPlanLufs) + " LUFS");
        row (T ("建议 LRA", "Suggested LRA"), num (state.targetPlanLra) + " LU");
        row (T ("真峰值上限", "True peak"), num (state.targetPlanCeiling) + " dBTP");
        row (T ("参与度", "War participation"), juce::String (juce::roundToInt (state.targetWarPct)) + "%");

        if (! tiny)
            row (T ("相对目标 Δ", "Δ vs target"),
                 hasSignal ? sgn (s.integrated - state.targetPlanLufs) + " LU" : noValue());

        if (! tiny)
        {
            inner.removeFromTop (compact ? 3 : 6);

            g.setColour (theme::textFaint);
            g.setFont (getLabelFont());
            g.drawFittedText (T ("50% = 曲风标准；0% = 该曲风最安静的发行观测值，100% = 最响的发行观测值。",
                                 "50% = genre reference; 0% = quietest observed release, 100% = loudest observed release."),
                              inner.removeFromTop (juce::jmin (compact ? 30 : 42, inner.getHeight())),
                              juce::Justification::topLeft, 2);

            if (! compact && inner.getHeight() >= 28)
            {
                g.setColour (theme::warn);
                g.setFont (getLabelFont());
                g.drawFittedText (T ("应用后切换到「手动参考」，目标只作检查参照，不是自动合格线。",
                                     "Apply switches to Manual reference; the target is a reference, not an automatic pass line."),
                                  inner.removeFromTop (juce::jmin (30, inner.getHeight())),
                                  juce::Justification::topLeft, 2);
            }
        }
    }

    // ---- platform delivery requirements ------------------------------------
    {
        g.setColour (theme::cardAlt);
        g.fillRoundedRectangle (right.toFloat(), tokens::radiusCard);
        g.setColour (theme::border);
        g.drawRoundedRectangle (right.toFloat().reduced (0.5f), tokens::radiusCard, 1.0f);

        auto inner = right.reduced (12, 9);
        const bool compact = right.getHeight() < 320;

        g.setColour (theme::accent);
        g.setFont (getLabelFont());
        g.drawText (T ("平台交付要求", "PLATFORM DELIVERY REQUIREMENTS"),
                    inner.removeFromTop (16), juce::Justification::centredLeft, false);

        auto header = inner.removeFromTop (20);
        const int wName = juce::jmax (86, juce::roundToInt ((float) inner.getWidth() * 0.30f));
        const int wLufs = 46, wTp = 48;

        g.setColour (theme::textFaint);
        g.setFont (getLabelFont());
        g.drawText (T ("平台", "Platform"), header.removeFromLeft (wName), juce::Justification::centredLeft);
        g.drawText ("LUFS", header.removeFromLeft (wLufs), juce::Justification::centredRight);
        g.drawText ("dBTP", header.removeFromLeft (wTp), juce::Justification::centredRight);
        g.drawText (T ("状态 / 尾部", "state / tail"), header, juce::Justification::centredRight);

        const auto statusColour = [this] (const DeliveryResult& r, const PlatformPreset& p)
        {
            if (! state.stats.hasSignal)
                return theme::textFaint;

            const bool clipOk = state.stats.clipEvents == 0;
            const bool tailMeasured = state.stats.tailSilenceMs > 0.0f;
            const bool tailOk = p.minTailSilenceMs <= 0 || ! tailMeasured
                                || state.stats.tailSilenceMs >= (float) p.minTailSilenceMs;

            if (r.state == DeliveryState::over || ! clipOk || ! tailOk)
                return theme::bad;

            if (p.minTailSilenceMs > 0 && ! tailMeasured)
                return theme::textFaint;

            if (r.state == DeliveryState::warning)
                return theme::warn;

            return theme::good;
        };

        const int rowH = juce::jlimit (18, 38,
            (inner.getHeight() - (compact ? 46 : 62)) / numPlatforms);

        for (int i = 0; i < numPlatforms; ++i)
        {
            if (inner.getHeight() < rowH)
                break;

            const auto& p = platform (i);
            const auto result = evaluate (s, p);
            auto row = inner.removeFromTop (rowH);

            g.setColour (theme::textDim);
            g.setFont (getLabelFont());
            g.drawFittedText (T (p.nameZh, p.nameEn), row.removeFromLeft (wName),
                              juce::Justification::centredLeft, 1);

            g.setColour (theme::text);
            g.setFont (getValueFont());
            g.drawText (num (p.targetLufs), row.removeFromLeft (wLufs),
                        juce::Justification::centredRight, false);
            g.drawText (num (p.tpCeiling), row.removeFromLeft (wTp),
                        juce::Justification::centredRight, false);

            const auto colour = statusColour (result, p);
            g.setColour (colour);

            const bool tailMeasured = s.tailSilenceMs > 0.0f;
            juce::String stateText;

            if (! s.hasSignal)
                stateText = "-";
            else if (result.state == DeliveryState::over)
                stateText = T ("超限", "OVER");
            else if (p.minTailSilenceMs > 0 && tailMeasured
                     && s.tailSilenceMs < (float) p.minTailSilenceMs)
                stateText = T ("尾静音不足", "TAIL");
            else if (p.minTailSilenceMs > 0 && ! tailMeasured)
                stateText = T ("尾部待测", "TAIL?");
            else if (result.state == DeliveryState::warning)
                stateText = T ("临界", "WARN");
            else
                stateText = T ("通过", "PASS");

            g.setFont (getLabelFont());
            g.drawText (stateText, row, juce::Justification::centredRight, false);
        }


        inner.removeFromTop (compact ? 3 : 5);

        g.setColour (theme::textFaint);
        g.setFont (getLabelFont());
        juce::StringArray note;
        note.add (T ("Apple Digital Masters：优先高质量 24-bit 源，使用 Apple 工具检查编码后削波；",
                     "ADM: use high-quality 24-bit sources and Apple tools to check encoded clipping;"));
        note.add (T ("尾部按发行商规范。Spotify：−14 LUFS；若母带更响，TP 建议低于 −2 dBTP。",
                     "follow distributor specs for tail silence. Spotify: −14 LUFS; if louder, keep TP below −2 dBTP."));
        text::drawLines (g, getLabelFont(), theme::textFaint, note,
                         inner.removeFromTop (juce::jmin (40, inner.getHeight())), 20);

        if (inner.getHeight() >= 18)
        {
            g.setColour (theme::textDim);
            g.setFont (getValueFont());
            g.drawText (T ("尾部静音 ", "Tail silence ") + num (s.tailSilenceMs / 1000.0f, 1) + " s"
                            + ml::sep() + T ("削波 ", "clips ") + juce::String (s.clipEvents)
                            + ml::sep() + T ("测量至曲尾时有效", "valid at the end of the song"),
                        inner.removeFromTop (20), juce::Justification::centredLeft, false);
        }
    }
}
} // namespace ml

