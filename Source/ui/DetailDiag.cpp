#include "Modules.h"

namespace ml
{
void DetailArea::paintDiagnostics (juce::Graphics& g)
{
    auto area = contentArea().withTrimmedTop (32);

    const auto checks = buildChecklist (state.stats, state.ceiling,
                                        state.bandDevs, state.defects,
                                        state.basis, state.platform, state.hasTonalBasis);

    int goodCount = 0, infoCount = 0, actionCount = 0;

    for (const auto& c : checks)
    {
        if (c.severity == Severity::critical || c.severity == Severity::warn)
            ++actionCount;
        else if (c.severity == Severity::info)
            ++infoCount;
        else
            ++goodCount;
    }

    auto summary = area.removeFromTop (32);

    const auto chip = [&g] (juce::Rectangle<int>& r, const juce::String& text, juce::Colour colour)
    {
        const auto font = getLabelFont();
        const int w = juce::roundToInt (juce::GlyphArrangement::getStringWidth (font, text)) + 20;
        auto c = r.removeFromLeft (w);

        g.setColour (colour.withAlpha (0.13f));
        g.fillRoundedRectangle (c.toFloat(), 9.0f);
        g.setColour (colour);
        g.setFont (font);
        g.drawText (text, c, juce::Justification::centred);

        r.removeFromLeft (8);
    };

    auto chipRow = summary;
    chip (chipRow, T ("通过 ", "Pass ") + juce::String (goodCount), theme::good);
    chip (chipRow, T ("提示 ", "Info ") + juce::String (infoCount), theme::info);
    chip (chipRow, T ("需处理 ", "Action ") + juce::String (actionCount),
          actionCount > 0 ? theme::warn : theme::good);

    g.setColour (theme::textFaint);
    g.setFont (getLabelFont());
    g.drawText (T ("点击左侧条目展开说明", "Click a row on the left for details"),
                summary, juce::Justification::centredRight);

    auto left = area.removeFromLeft (juce::roundToInt ((float) area.getWidth() * 0.56f));
    juce::ignoreUnused (left);

    area.removeFromLeft (tokens::gap);

    g.setColour (theme::textDim);
    g.setFont (getLabelFont());
    g.drawText (T ("检查项清单", "Checklist"), area.removeFromTop (24),
                juce::Justification::centredLeft);

    for (const auto& c : checks)
    {
        if (area.getHeight() < 26)
            break;

        auto row = area.removeFromTop (28);
        const auto colour = severityColour (c.severity);

        g.setColour (colour);
        g.fillEllipse ((float) row.getX(), (float) row.getCentreY() - 3.0f, 6.0f, 6.0f);

        g.setColour (theme::textDim);
        g.setFont (getLabelFont());
        g.drawText (c.label, row.withTrimmedLeft (16).withWidth (juce::jmax (80, row.getWidth() - 130)),
                    juce::Justification::centredLeft);

        g.setColour (colour);
        g.setFont (getValueFont());
        g.drawText (c.value, row, juce::Justification::centredRight);
    }
}
} // namespace ml
