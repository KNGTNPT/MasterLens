#include "Modules.h"

namespace ml
{
//==============================================================================
ModuleRail::ModuleRail()
{
    setOpaque (true);
    setWantsKeyboardFocus (false);
}

juce::Rectangle<int> ModuleRail::rowBounds (int index) const
{
    const int top = 68;
    const int rowH = juce::jlimit (24, 36, (getHeight() - top - 8) / juce::jmax (1, numPages));
    return { 0, top + index * rowH, getWidth(), rowH };
}

void ModuleRail::setModule (int index, juce::String title, juce::String value,
                            juce::String status, juce::Colour colour)
{
    if (index < 0 || index >= numPages)
        return;

    auto& item = items[(size_t) index];
    item.title = std::move (title);
    item.value = std::move (value);
    item.status = std::move (status);
    item.colour = colour;
    repaint();
}

void ModuleRail::setSelected (int index)
{
    selected = juce::jlimit (0, numPages - 1, index);
    repaint();
}

void ModuleRail::setVerdict (juce::String headline, juce::String subline, juce::Colour colour)
{
    verdictHeadline = std::move (headline);
    verdictSubline = std::move (subline);
    verdictColour = colour;
    repaint();
}

void ModuleRail::paint (juce::Graphics& g)
{
    g.fillAll (theme::card);

    // ---- overall verdict -----------------------------------------------------
    {
        auto card = juce::Rectangle<int> (10, 10, getWidth() - 20, 48);

        g.setColour (theme::cardAlt);
        g.fillRoundedRectangle (card.toFloat(), tokens::radiusCard);
        g.setColour (theme::border);
        g.drawRoundedRectangle (card.toFloat().reduced (0.5f), tokens::radiusCard, 1.0f);

        g.setColour (verdictColour);
        g.fillRoundedRectangle ((float) card.getX() + 10.0f, (float) card.getY() + 13.0f,
                                3.0f, 26.0f, 1.5f);

        g.setFont (uiFont (tokens::section, true));
        g.drawText (verdictHeadline, card.getX() + 20, card.getY() + 8, card.getWidth() - 30, 22,
                    juce::Justification::centredLeft);

        g.setColour (theme::textFaint);
        g.setFont (uiFont (tokens::unit));
        g.drawText (verdictSubline, card.getX() + 20, card.getY() + 30, card.getWidth() - 30, 16,
                    juce::Justification::centredLeft);
    }

    // ---- module rows ---------------------------------------------------------
    for (int i = 0; i < numPages; ++i)
    {
        auto row = rowBounds (i);
        const bool sel = i == selected;
        const auto& item = items[(size_t) i];

        if (sel)
        {
            g.setColour (theme::accentSoft);
            g.fillRoundedRectangle (row.toFloat().reduced (6, 2), 6.0f);
            g.setColour (theme::accent);
            g.fillRoundedRectangle ((float) row.getX() + 6.0f, (float) row.getY() + 6.0f,
                                    2.0f, (float) row.getHeight() - 12.0f, 1.0f);
        }
        else if (i == hoverRow)
        {
            g.setColour (theme::cardAlt.withAlpha (0.6f));
            g.fillRoundedRectangle (row.toFloat().reduced (6, 2), 6.0f);
        }

        g.setColour (item.colour);
        g.fillEllipse (16.0f, (float) row.getY() + (row.getHeight() < 34 ? 9.0f : 13.0f), 6.0f, 6.0f);

        const bool compact = row.getHeight() < 34;

        g.setColour (sel ? theme::text : theme::textDim);
        g.setFont (uiFont (tokens::label, sel));
        g.drawText (item.title, 28, row.getY() + (compact ? 3 : 5), row.getWidth() - 110,
                    compact ? 14 : 17, juce::Justification::centredLeft);

        g.setColour (item.colour);
        g.setFont (uiFont (tokens::unit));
        g.drawText (item.status, 28, row.getY() + (compact ? 16 : 22), 90, compact ? 12 : 14,
                    juce::Justification::centredLeft);

        g.setColour (theme::textFaint);
        g.setFont (monoFont (tokens::unit));
        g.drawText (item.value, row.getWidth() - 106, row.getY() + (compact ? 7 : 8), 92, 14,
                    juce::Justification::centredRight);
    }
}

static const char* moduleHintZh (int index)
{
    switch (index)
    {
        case 0: return "响度、动态与响度范围（ITU-R BS.1770-4 / EBU Tech 3342）";
        case 1: return "4× 过采样真峰值与跨采样过载事件";
        case 2: return "10 段能量相对粉噪 / 参考谱的偏差";
        case 3: return "相关性、宽度与三段分频侧向能量";
        case 4: return "8 个平台归一化后的实际后果：增益、预测峰值与服务限幅";
        case 5: return "专辑中位快照、跨曲目偏离、风险聚合与可复现交付门禁";
        case 6: return "失真、混叠、超低堆积、瞬态、单声道折叠、中频掩蔽与联动归因";
        case 7: return "全部静态阈值检查清单，可逐条核对";
        case 8: return "超限与谐振事件的时间位置；双击底部时间线可加标记";
        case 9: return "曲风标准响度 + 响度战争参与度滑块；各平台交付规范与尾部静音检查";
        default: return "拖入 WAV / AIFF / FLAC 或专辑文件夹，后台秒级分析并写入专辑门禁";
    }
}

static const char* moduleHintEn (int index)
{
    switch (index)
    {
        case 0: return "Loudness, dynamics and LRA (ITU-R BS.1770-4 / EBU Tech 3342)";
        case 1: return "4x oversampled true peak and inter-sample overs";
        case 2: return "10 band energy deviation against pink noise / reference";
        case 3: return "Correlation, width and three-band side energy";
        case 4: return "Normalisation consequences for eight platforms: gain, predicted peak, service limiting";
        case 5: return "Album median snapshot, cross-track deviations, risk aggregation and reproducible release gate";
        case 6: return "THD/IMD, aliasing, sub build-up, transients, mono fold, mid masking and attribution";
        case 7: return "Full checklist of every static threshold";
        case 8: return "Over-limit and resonance positions; double-click the strip to add a marker";
        case 9: return "Genre reference loudness + loudness-war participation; platform delivery specs and tail-silence check";
        default: return "Drop WAV / AIFF / FLAC files or an album folder for fast offline analysis and gate fill";
    }
}

juce::String ModuleRail::getTooltip()
{
    if (hoverRow < 0 || hoverRow >= numPages)
        return {};

    const auto& item = items[(size_t) hoverRow];
    const juce::String state = item.status.isNotEmpty() ? item.status : T ("状态", "Status");

    return T (moduleHintZh (hoverRow), moduleHintEn (hoverRow)) + "  ·  " + state;
}

void ModuleRail::mouseDown (const juce::MouseEvent& e)
{
    for (int i = 0; i < numPages; ++i)
    {
        if (rowBounds (i).contains (e.getPosition()))
        {
            if (i != selected)
            {
                selected = i;
                repaint();

                if (onSelect != nullptr)
                    onSelect (i);
            }

            return;
        }
    }
}

void ModuleRail::mouseMove (const juce::MouseEvent& e)
{
    int row = -1;

    for (int i = 0; i < numPages; ++i)
        if (rowBounds (i).contains (e.getPosition()))
            row = i;

    if (row != hoverRow)
    {
        hoverRow = row;
        repaint();
    }
}

void ModuleRail::mouseExit (const juce::MouseEvent&)
{
    if (hoverRow >= 0)
    {
        hoverRow = -1;
        repaint();
    }
}

} // namespace ml
