#include "Modules.h"

namespace ml
{
void DetailArea::paintTruePeak (juce::Graphics& g)
{
    auto area = contentArea().withTrimmedTop (32);
    const auto& s = state.stats;

    auto top = area.removeFromTop (juce::jmin (136, area.getHeight() / 2));

    g.setColour (theme::cardAlt);
    g.fillRoundedRectangle (top.toFloat(), tokens::radiusCard);
    g.setColour (theme::border);
    g.drawRoundedRectangle (top.toFloat().reduced (0.5f), tokens::radiusCard, 1.0f);

    auto inner = top.reduced (16, 12);
    auto left = inner.removeFromLeft (juce::jmin (330, inner.getWidth() / 2));

    const bool valid = s.truePeakDb > -70.0f;
    const auto colour = ! valid ? theme::textFaint
                      : (s.truePeakDb > state.ceiling ? theme::bad
                      : (s.truePeakDb > state.ceiling - 0.6f ? theme::warn : theme::good));

    g.setColour (theme::textDim);
    g.setFont (getLabelFont());
    g.drawText (T ("最大值", "MAXIMUM"), left.removeFromTop (16),
                juce::Justification::centredLeft);

    auto valueRow = left.removeFromTop (48);
    const auto critical = getCriticalFont();
    const auto valueText = valid ? num (s.truePeakDb) : noValue();
    g.setColour (colour);
    g.setFont (critical);
    g.drawText (valueText, valueRow, juce::Justification::centredLeft);

    const auto valueWidth = juce::roundToInt (
        juce::GlyphArrangement::getStringWidth (critical, valueText));
    g.setColour (theme::textFaint);
    g.setFont (getLabelFont());
    g.drawText ("dBTP", valueRow.withTrimmedLeft (valueWidth + 8).withTrimmedTop (18),
                juce::Justification::centredLeft);

    const auto row = [&] (const juce::String& key, const juce::String& value)
    {
        auto r = inner.removeFromTop (28);

        g.setColour (theme::textFaint);
        g.setFont (getLabelFont());
        g.drawText (key, r.removeFromLeft (152), juce::Justification::centredLeft);

        g.setColour (theme::text);
        g.setFont (getValueFont());
        g.drawText (value, r, juce::Justification::centredLeft);
    };

    row (T ("上限", "Ceiling"), num (state.ceiling) + " dBTP");
    row (T ("最大值位置", "Maximum at"),
         s.truePeakMaxTime > 0.01f ? fmtTime (s.truePeakMaxTime) : noValue());
    row (T ("跨采样过载", "Inter-sample overs"), "OVERS " + juce::String (s.oversCount));
    row (T ("过采样倍数", "Oversampling"), juce::String ("4") + times());

    area.removeFromTop (tokens::gap);

    g.setColour (theme::textDim);
    g.setFont (getLabelFont());
    g.drawText (T ("超限事件（时间戳 / 超出量）", "Over-limit events (time / overshoot)"),
                area.removeFromTop (20), juce::Justification::centredLeft);

    if (state.tpEvents.empty())
    {
        g.setColour (theme::good);
        g.setFont (getLabelFont());
        g.drawText (T ("未检出真峰值超限", "No true-peak overs detected"),
                    area.removeFromTop (24), juce::Justification::centredLeft);
    }
    else
    {
        int shown = 0;

        for (auto it = state.tpEvents.rbegin(); it != state.tpEvents.rend() && shown < 10; ++it, ++shown)
        {
            auto r = area.removeFromTop (24);

            g.setColour (theme::bad);
            g.setFont (getValueFont());
            g.drawText (fmtTime (it->time), r.removeFromLeft (72), juce::Justification::centredLeft);

            g.setColour (theme::text);
            g.setFont (getValueFont());
            g.drawText (num (it->overshootDb) + " dB", r, juce::Justification::centredLeft);
        }
    }

    // ---- method note ---------------------------------------------------------
    if (area.getHeight() >= 92)
    {
        auto card = area.removeFromTop (juce::jmin (92, area.getHeight()));
        card = card.withTrimmedTop (tokens::gap);

        g.setColour (theme::cardAlt);
        g.fillRoundedRectangle (card.toFloat(), tokens::radiusCard);
        g.setColour (theme::border);
        g.drawRoundedRectangle (card.toFloat().reduced (0.5f), tokens::radiusCard, 1.0f);

        auto inner = card.reduced (14, 10);

        g.setColour (theme::textDim);
        g.setFont (getLabelFont());
        g.drawText (T ("检测方式与规则", "Method and rules"), inner.removeFromTop (16),
                    juce::Justification::centredLeft);

        g.setColour (theme::textFaint);
        g.setFont (getLabelFont());
        g.drawFittedText (T ("真峰值以 192 kHz 为插值基准（48 kHz 4× / 96 kHz 2× / 192 kHz 1×），阈值 = 顶部设定的真峰值上限；"
                             "超限事件写入时间线并出现在导出报告中，判断规则全部可追溯到实际读数。",
                             "True peak uses a 192 kHz interpolation target (4x at 48 kHz / 2x at 96 kHz / 1x at 192 kHz); "
                             "the threshold is the true-peak ceiling above. Overs are written to the timeline and the exported "
                             "report - every rule traces back to a measured value."),
                          inner, juce::Justification::topLeft, 3);
    }
}
} // namespace ml
