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
    const int x0 = grid::down (getWidth() * index / numPages);
    const int x1 = grid::down (getWidth() * (index + 1) / numPages);
    return { x0, 0, x1 - x0, getHeight() };

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

    if (batchDepth == 0)
        repaint();
}

void ModuleRail::endBatch()
{
    if (batchDepth > 0)
        --batchDepth;

    if (batchDepth == 0)
        repaint();
}

void ModuleRail::setSelected (int index)
{
    const int next = juce::jlimit (0, numPages - 1, index);

    if (next == selected)
        return;

    selected = next;

    // Screenshot / headless renders should show the settled state; only the
    // live editor spends 140 ms sliding the indicator.
    if (! isShowing())
    {
        visualSelected = (float) selected;
        animationActive = false;
        stopTimer();
        repaint();
        return;
    }

    animationFrom = visualSelected;
    animationStarted = juce::Time::getMillisecondCounterHiRes() * 0.001;
    animationActive = true;
    startTimerHz (60);
    repaint();
}

void ModuleRail::timerCallback()
{
    if (! animationActive)
    {
        stopTimer();
        return;
    }

    constexpr double duration = 0.14;
    const double elapsed = juce::Time::getMillisecondCounterHiRes() * 0.001 - animationStarted;
    const float t = (float) juce::jlimit (0.0, 1.0, elapsed / duration);
    const float eased = 1.0f - std::pow (1.0f - t, 3.0f);
    visualSelected = animationFrom + ((float) selected - animationFrom) * eased;

    if (t >= 1.0f)
    {
        visualSelected = (float) selected;
        animationActive = false;
        stopTimer();
    }

    repaint();
}

void ModuleRail::setVerdict (juce::String headline, juce::String subline, juce::Colour colour)
{
    verdictHeadline = std::move (headline);
    verdictSubline = std::move (subline);
    verdictColour = colour;

    if (batchDepth == 0)
        repaint();
}

void ModuleRail::paint (juce::Graphics& g)
{
    g.fillAll (theme::background);
    const char* zh[] { "响度", "峰值", "频谱", "声场", "平台", "专辑", "信号", "诊断", "时间", "目标", "文件" };
    const char* en[] { "Loudness", "Peak", "Spectrum", "Stereo", "Delivery", "Album", "Signal", "Checks", "Timeline", "Target", "Files" };
    for (int i = 0; i < numPages; ++i)
    {
        auto r = rowBounds (i);
        const bool sel = i == selected;
        const auto colour = sel ? theme::accent : (i == hoverRow ? theme::text : theme::textDim);
        const float cx = (float) r.getCentreX(), cy = (float) r.getY() + 14.0f;
        g.setColour (colour);
        // oil-ui tab icons: the exact 16x16 stroke paths from the design
        // mockup, parsed once and recoloured per row.
        static const char* iconPaths[] = {
            "M3 13V6M7.5 13V3.5M12 13V8.5",
            "M2 12l3.5-5 2.5 3L12 4",
            "M2 11c3 0 3-7 5.5-7S10 12 14 11",
            "M6 8a3 3 0 1 0 0-.1M11 8a3 3 0 1 0 0-.1",
            "M3 5h10M3 8h7M3 11h10",
            "M3 4h10v8H3zM6 4v8",
            "M3 8h2l1.5-4 3 8L11 8h3",
            "M3 8l3 3 7-6",
            "M8 2a6 6 0 1 0 0 12A6 6 0 0 0 8 2zM8 5v3l2 1",
            "M8 2a6 6 0 1 0 0 12A6 6 0 0 0 8 2zM8 5.5a2.5 2.5 0 1 0 0 5 2.5 2.5 0 0 0 0-5z",
            "M3 4h4l1.5 2H13v8H3z"
        };

        static juce::OwnedArray<juce::Drawable> iconCache;

        if (iconCache.isEmpty())
        {
            for (const auto* d : iconPaths)
            {
                const juce::String svg = juce::String ("<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 16 16\" fill=\"none\" stroke=\"#ffffff\" stroke-width=\"1.3\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"") + d + "\"/></svg>";
                auto xml = juce::XmlDocument::parse (svg);

                if (xml != nullptr)
                    iconCache.add (juce::Drawable::createFromSVG (*xml).release());
                else
                    iconCache.add (nullptr);
            }
        }

        if (auto* iconDrawable = i < iconCache.size() ? iconCache[i] : nullptr)
        {
            if (auto* comp = dynamic_cast<juce::DrawableComposite*> (iconDrawable))
                for (int child = 0; child < comp->getNumChildComponents(); ++child)
                    if (auto* path = dynamic_cast<juce::DrawablePath*> (comp->getChildComponent (child)))
                        path->setStrokeFill (juce::FillType (colour));

            const float size = 20.0f;
            juce::Graphics::ScopedSaveState save (g);
            g.addTransform (juce::AffineTransform::scale (size / 16.0f)
                                .translated (cx - size * 0.5f, cy - size * 0.5f));
            iconDrawable->draw (g, 1.0f);
        }
        g.setFont (labelFontFor (T (zh[i], en[i]).toUpperCase()));
        g.drawText (T (zh[i], en[i]).toUpperCase(), r.withTrimmedTop (28).withTrimmedBottom (4),
                    juce::Justification::centred, false);
    }

    // One sliding indicator, not a hard rectangle jump. The text colours still
    // switch immediately so the destination is readable during the 140 ms move.
    const float tabW = (float) getWidth() / (float) numPages;
    const float x = juce::jlimit (0.0f, (float) getWidth() - tabW,
                                  (float) getWidth() * visualSelected / (float) numPages);
    juce::Rectangle<float> indicator (x, 0.0f, tabW, (float) getHeight());
    g.setColour (theme::accent.withAlpha (0.65f));
    g.fillRect (indicator.getX() + 14.0f, indicator.getBottom() - 2.0f,
                juce::jmax (8.0f, indicator.getWidth() - 28.0f), 2.0f);
}

static const char* moduleHintZh (int index)
{
    switch (index)
    {
        case 0: return "响度、动态与响度范围（ITU-R BS.1770-4 / EBU Tech 3342）";
        case 1: return "192 kHz 基准真峰值与跨采样过载事件";
        case 2: return "10 段能量相对粉噪 / 参考谱的偏差";
        case 3: return "相关性、宽度与三段分频侧向能量";
        case 4: return "8 个本机交付情景：安全增益、预测峰值与目标差额";
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
        case 1: return "192 kHz target true peak and inter-sample overs";
        case 2: return "10 band energy deviation against pink noise / reference";
        case 3: return "Correlation, width and three-band side energy";
        case 4: return "Eight local delivery scenarios: safe gain, predicted peak and target shortfall";
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
                // Go through setSelected so the visual indicator animates from
                // the previous page. Setting `selected` directly left
                // `visualSelected` behind, so the underline did not follow.
                setSelected (i);

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
