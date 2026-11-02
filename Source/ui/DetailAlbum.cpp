#include "Modules.h"

#include <map>

namespace ml
{
void DetailArea::paintAlbum (juce::Graphics& g)
{
    auto area = contentArea().withTrimmedTop (32);
    area.removeFromBottom (72);

    const auto& a = state.album;
    auto summary = area.removeFromTop (40);
    area.removeFromTop (8);

    g.setColour (theme::cardAlt);
    g.fillRoundedRectangle (summary.toFloat(), tokens::radiusControl);
    g.setColour (theme::border);
    g.drawRoundedRectangle (summary.toFloat().reduced (0.5f), tokens::radiusControl, 1.0f);

    auto s = summary.reduced (10, 0);

    g.setColour (a.hasMedian ? theme::good : theme::warn);
    g.setFont (getLabelFont());
    g.drawText (a.hasMedian ? T ("专辑基准已建立", "Album baseline ready")
                            : T ("至少采集 2 首生成中位基准", "Capture at least 2 tracks for a median"),
                s.removeFromLeft (juce::jmin (260, s.getWidth() / 2)), juce::Justification::centredLeft);

    const auto& gate = state.albumGate;

    g.setColour (gate.blocked ? theme::bad : (gate.incomplete ? theme::warn : theme::good));
    g.setFont (getValueFont());
    g.drawText (gate.blocked ? T ("阻断 RELEASE BLOCK", "RELEASE BLOCK")
                             : (gate.incomplete ? T ("未采集 INCOMPLETE", "INCOMPLETE")
                                                : T ("放行 RELEASE PASS", "RELEASE PASS")),
                s.removeFromRight (192), juce::Justification::centredRight);

    g.setColour (theme::textFaint);
    g.setFont (getLabelFont());
    g.drawText ("rev " + juce::String (a.revision) + ml::sep()
                    + juce::String ((int) a.tracks.size()) + T (" 首", " tracks") + ml::sep()
                    + T ("新增 ", "new ") + juce::String (state.albumDiff.added)
                    + " / " + T ("恶化 ", "worse ") + juce::String (state.albumDiff.worsened),
                s, juce::Justification::centredRight);

    const int tableW = juce::roundToInt ((float) area.getWidth() * 0.62f);
    auto table = area.removeFromLeft (tableW);
    area.removeFromLeft (tokens::gap);
    auto side = area;

    if (a.tracks.empty())
    {
        g.setColour (theme::textDim);
        g.setFont (getLabelFont());
        g.drawFittedText (T ("在专辑页输入曲目名，点「采集本曲」；每首歌曲检查完成后采集一次。",
                             "Type a track name, then Capture track after checking each song."),
                          area, juce::Justification::centred, 3);
        return;
    }

    auto header = table.removeFromTop (24);
    const int wTrack = juce::roundToInt ((float) table.getWidth() * 0.27f);
    const int wLufs  = juce::roundToInt ((float) table.getWidth() * 0.11f);
    const int wDelta = juce::roundToInt ((float) table.getWidth() * 0.12f);
    const int wLra   = juce::roundToInt ((float) table.getWidth() * 0.11f);
    const int wPlr   = juce::roundToInt ((float) table.getWidth() * 0.11f);
    const int wTp    = juce::roundToInt ((float) table.getWidth() * 0.13f);

    g.setColour (theme::textFaint);
    g.setFont (getLabelFont());

    auto h = header;
    g.drawText (T ("曲目", "Track"), h.removeFromLeft (wTrack), juce::Justification::centredLeft);
    g.drawText ("LUFS", h.removeFromLeft (wLufs), juce::Justification::centredRight);
    g.drawText (T ("Δ中位", "Δ median"), h.removeFromLeft (wDelta), juce::Justification::centredRight);
    g.drawText ("LRA", h.removeFromLeft (wLra), juce::Justification::centredRight);
    g.drawText ("PLR", h.removeFromLeft (wPlr), juce::Justification::centredRight);
    g.drawText ("TP", h.removeFromLeft (wTp), juce::Justification::centredRight);
    g.drawText (T ("状态", "Status"), h, juce::Justification::centredRight);

    std::map<juce::String, int> trackSeverity;

    for (const auto& r : gate.risks)
    {
        auto it = trackSeverity.find (r.track);
        trackSeverity[r.track] = it == trackSeverity.end() ? r.severity
                                                           : juce::jmax (it->second, r.severity);
    }

    const int rowH = juce::jlimit (24, 31, (table.getHeight() - 4)
                                             / juce::jmax (1, (int) a.tracks.size()));

    for (const auto& t : a.tracks)
    {
        if (table.getHeight() < rowH)
            break;

        auto row = table.removeFromTop (rowH);
        const int sev = trackSeverity.count (t.label) > 0 ? trackSeverity[t.label] : 0;
        const auto colour = sev >= 2 ? theme::bad : (sev == 1 ? theme::warn : theme::text);

        if (sev >= 2)
        {
            g.setColour (theme::bad.withAlpha (0.06f));
            g.fillRect (row);
            g.setColour (theme::bad);
            g.fillRect (row.getX(), row.getY(), 2, row.getHeight());
        }
        else
        {
            g.setColour (theme::borderSoft.withAlpha (0.25f));
            g.drawHorizontalLine (row.getBottom(), (float) row.getX(), (float) row.getRight());
        }

        auto r = row;
        g.setColour (colour);
        g.setFont (getLabelFont());
        g.drawFittedText (t.label, r.removeFromLeft (wTrack).withTrimmedRight (4),
                          juce::Justification::centredLeft, 1);

        g.setFont (getValueFont());
        g.drawText (juce::String (t.stats.integrated, 1), r.removeFromLeft (wLufs),
                    juce::Justification::centredRight);

        if (a.hasMedian)
        {
            const float d = t.stats.integrated - a.medianStats.integrated;
            g.setColour (std::abs (d) > a.rules.lufsSpread ? theme::warn : theme::textDim);
            g.drawText ((d >= 0.0f ? "+" : "") + juce::String (d, 1),
                        r.removeFromLeft (wDelta), juce::Justification::centredRight);
        }
        else
        {
            g.setColour (theme::textFaint);
            g.drawText ("—", r.removeFromLeft (wDelta), juce::Justification::centredRight);
        }

        g.setColour (theme::textDim);
        g.drawText (juce::String (t.stats.lra, 1), r.removeFromLeft (wLra), juce::Justification::centredRight);
        g.drawText (juce::String (t.stats.getPlr(), 1), r.removeFromLeft (wPlr), juce::Justification::centredRight);

        g.setColour (t.stats.truePeakDb > t.tpCeiling + 0.05f ? theme::bad : theme::textDim);
        g.drawText (juce::String (t.stats.truePeakDb, 1), r.removeFromLeft (wTp), juce::Justification::centredRight);

        g.setColour (sev >= 2 ? theme::bad : (sev == 1 ? theme::warn : theme::good));
        g.setFont (getLabelFont());
        g.drawText (sev >= 2 ? T ("阻断", "Block") : (sev == 1 ? T ("注意", "Warn") : T ("正常", "OK")),
                    r, juce::Justification::centredRight);
    }

    auto gateBox = side.removeFromTop (68);

    g.setColour (gate.blocked ? theme::bad.withAlpha (0.08f)
                             : (gate.incomplete ? theme::warn.withAlpha (0.07f) : theme::good.withAlpha (0.07f)));
    g.fillRoundedRectangle (gateBox.toFloat(), tokens::radiusControl);
    g.setColour (gate.blocked ? theme::bad : (gate.incomplete ? theme::warn : theme::good));
    g.drawRoundedRectangle (gateBox.toFloat().reduced (0.5f), tokens::radiusControl, 1.0f);

    auto gateText = gateBox.reduced (10, 6);
    g.setFont (getLabelFont());
    g.drawText (gate.blocked ? T ("交付门禁：阻断", "Gate: blocked")
                             : (gate.incomplete ? T ("交付门禁：未采集", "Gate: incomplete")
                                                : T ("交付门禁：放行", "Gate: pass")),
                gateText.removeFromTop (20), juce::Justification::centredLeft);

    g.setColour (theme::textDim);
    g.setFont (getLabelFont());
    g.drawFittedText (gate.blockers > 0
                          ? juce::String (gate.blockers) + T (" 个阻断项；先处理排序前 3 项。",
                                                              " blockers; fix the top 3 risks first.")
                          : (gate.incomplete ? T ("采集至少 2 首后生成专辑中位与门禁结论。",
                                                  "Capture at least 2 tracks to build the median and gate.")
                                             : T ("当前规则集未发现阻断项；警告仍需人工试听。",
                                                  "No blocking item under the current rules; audition warnings manually.")),
                      gateText, juce::Justification::centredLeft, 2);

    side.removeFromTop (8);
    auto risks = side.removeFromTop (juce::jmax (72, side.getHeight() / 2));

    g.setColour (theme::textDim);
    g.setFont (getLabelFont());
    g.drawText (T ("整张专辑风险排序", "Album risk ranking"), risks.removeFromTop (20),
                juce::Justification::centredLeft);

    int shown = 0;

    for (const auto& r : gate.risks)
    {
        if (shown >= 4 || risks.getHeight() < 26)
            break;

        auto row = risks.removeFromTop (28);
        g.setColour (r.severity >= 2 ? theme::bad : theme::warn);
        g.fillRoundedRectangle (row.removeFromLeft (4).toFloat(), 1.5f);
        row.removeFromLeft (8);

        g.setFont (getLabelFont());
        g.drawFittedText (r.track + " · " + r.dimension, row.removeFromTop (16),
                          juce::Justification::centredLeft, 1);
        g.setColour (theme::textFaint);
        g.setFont (getValueFont());
        g.drawFittedText (r.detail + "  @" + r.time, row, juce::Justification::centredLeft, 1);
        ++shown;
    }

    side.removeFromTop (4);

    g.setColour (theme::textFaint);
    g.setFont (getLabelFont());
    g.drawFittedText (T ("规则：", "Rules: ")
                          + juce::String (a.rules.lufsSpread, 1) + " LU" + ml::sep()
                          + juce::String (a.rules.bandSpread, 1) + " dB" + ml::sep()
                          + T ("信号 ", "signal ") + juce::String (juce::roundToInt (a.rules.maxSignalRisk))
                          + ml::sep() + T ("单声道 ", "mono ") + juce::String (a.rules.maxMonoLoss, 1) + " dB",
                      side.removeFromTop (28), juce::Justification::topLeft, 2);

    g.setColour (theme::textDim);
    g.drawFittedText (state.albumDiff.lines.joinIntoString ("  ·  "), side,
                      juce::Justification::topLeft, 2);
}
} // namespace ml
