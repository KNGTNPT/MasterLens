#include "Modules.h"

namespace ml
{
static constexpr int kTimelineStripHeight = 96;
static constexpr int kTimelineButtonRow  = 34;

static double timeNow (const StatsSnapshot& s) noexcept
{
    return s.hostPosition > 0.0 ? s.hostPosition : s.elapsed;
}

static double timeTotal (const StatsSnapshot& s) noexcept
{
    if (s.hostLength > 0.0)
        return juce::jmax (s.hostLength, timeNow (s));

    return juce::jmax (1.0, timeNow (s));
}

/** Strip rect in local coordinates: pinned to the bottom of the content area. */
static juce::Rectangle<int> timelineStripArea (juce::Rectangle<int> content)
{
    return { content.getX(), content.getBottom() - kTimelineButtonRow - kTimelineStripHeight,
             content.getWidth(), kTimelineStripHeight };
}

void DetailArea::paintTimeline (juce::Graphics& g)
{
    const auto content = contentArea();
    const auto strip = timelineStripArea (content);

    const int listTop = content.getY() + 30;
    auto list = juce::Rectangle<int> (content.getX(), listTop, content.getWidth(),
                                      juce::jmax (0, strip.getY() - 12 - listTop));

    const double total = timeTotal (state.stats);

    if (region.valid && list.getHeight() > 80)
    {
        auto card = list.removeFromTop (juce::jmin (66, list.getHeight() / 2));
        list.removeFromTop (8);

        g.setColour (theme::accent.withAlpha (0.08f));
        g.fillRoundedRectangle (card.toFloat(), tokens::radiusControl);
        g.setColour (theme::accent.withAlpha (0.45f));
        g.drawRoundedRectangle (card.toFloat().reduced (0.5f), tokens::radiusControl, 1.0f);

        auto inner = card.reduced (10, 6);
        g.setColour (theme::accent);
        g.setFont (uiFont (tokens::label, true));
        g.drawText (region.name, inner.removeFromTop (18), juce::Justification::centredLeft, false);

        g.setColour (theme::text);
        g.setFont (monoFont (juce::jmin (14.0f, (float) inner.getHeight() * 0.55f), true));
        g.drawFittedText (num (region.lufs) + " LUFS   "
                              + T ("M ", "M ") + num (region.midLufs) + " / S "
                              + num (region.sideLufs) + "   "
                              + T ("TP ", "TP ") + num (region.truePeak) + " dBTP   "
                              + "PLR " + num (region.plr) + "   "
                              + T ("相关 ", "corr ") + sgn (region.correlation, 2),
                          inner.removeFromTop (18), juce::Justification::centredLeft, 1, 0.7f);

        g.setColour (theme::textFaint);
        g.setFont (uiFont (tokens::unit));
        g.drawText (T ("100 ms 包络估算 · 拖动底部时间线选择区域",
                       "100 ms envelope estimate - drag the bottom timeline to select"),
                    inner, juce::Justification::centredLeft, false);
    }

    // ---- correlated events (same moment, one attribution) -------------------
    {
        const auto merged = mergeMarkerEvents (state.markers);

        if (! merged.empty() && list.getHeight() > 80)
        {
            const int cardH = juce::jmin (110, juce::jmax (54, list.getHeight() / 2));
            auto card = list.removeFromTop (cardH);
            list.removeFromTop (8);

            g.setColour (theme::cardAlt);
            g.fillRoundedRectangle (card.toFloat(), tokens::radiusControl);
            g.setColour (theme::border);
            g.drawRoundedRectangle (card.toFloat().reduced (0.5f), tokens::radiusControl, 1.0f);

            auto inner = card.reduced (10, 7);

            g.setColour (theme::accent);
            g.setFont (uiFont (tokens::unit, true));
            g.drawText (T ("联动事件（同一时间点合并归因）",
                           "LINKED EVENTS (same moment, one cause)"),
                        inner.removeFromTop (15), juce::Justification::centredLeft, false);

            int shown = 0;

            for (const auto& e : merged)
            {
                if (inner.getHeight() < 17 || shown >= 3)
                    break;

                auto row = inner.removeFromTop (17);
                const auto colour = e.severity >= 2 ? theme::bad
                                  : (e.severity == 1 ? theme::warn : theme::info);

                g.setColour (colour);
                g.fillEllipse ((float) row.getX(), (float) row.getCentreY() - 3.0f, 6.0f, 6.0f);

                g.setColour (theme::textDim);
                g.setFont (uiFont (tokens::unit));
                g.drawText (e.text, row.withTrimmedLeft (12), juce::Justification::centredLeft, true);
                ++shown;
            }
        }
    }

    // ---- event list (above the strip) ---------------------------------------
    g.setColour (theme::textDim);
    g.setFont (uiFont (tokens::aux));
    g.drawText (T ("事件列表", "Event list"), list.removeFromTop (20),
                juce::Justification::centredLeft);

    if (state.markers.empty())
    {
        g.setColour (theme::textFaint);
        g.setFont (uiFont (tokens::aux));
        g.drawFittedText (T ("暂无标记；播放中检出的超限与谐振会自动记录，也可双击时间条手动添加",
                             "No markers yet. Overs and resonances are recorded automatically; "
                             "double-click the strip to add one"),
                          list.removeFromTop (24), juce::Justification::centredLeft, 2);
    }
    else
    {
        int shown = 0;

        for (auto it = state.markers.rbegin(); it != state.markers.rend() && shown < 14; ++it, ++shown)
        {
            if (list.getHeight() < 22)
                break;

            auto row = list.removeFromTop (22);
            const auto colour = it->severity >= 2 ? theme::bad
                              : (it->severity == 1 ? theme::warn : theme::info);

            g.setColour (colour);
            g.fillEllipse ((float) row.getX(), (float) row.getCentreY() - 3.0f, 6.0f, 6.0f);

            g.setColour (theme::text);
            g.setFont (monoFont (11.0f, true));
            g.drawText (fmtTime (it->time), row.withTrimmedLeft (14).withWidth (56),
                        juce::Justification::centredLeft);

            g.setColour (theme::textDim);
            g.setFont (uiFont (tokens::aux));
            g.drawText (it->text, row.withTrimmedLeft (76), juce::Justification::centredLeft);
        }
    }

    // ---- strip (pinned to the bottom) ---------------------------------------
    g.setColour (theme::cardAlt);
    g.fillRoundedRectangle (strip.toFloat(), tokens::radiusCard);
    g.setColour (theme::border);
    g.drawRoundedRectangle (strip.toFloat().reduced (0.5f), tokens::radiusCard, 1.0f);

    auto inner = strip.reduced (14, 10);
    const float y = (float) inner.getCentreY();

    g.setColour (theme::borderSoft);
    g.fillRoundedRectangle ((float) inner.getX(), y - 1.5f, (float) inner.getWidth(), 3.0f, 1.5f);

    const auto xFor = [inner, total] (double t)
    {
        return (float) inner.getX()
             + (float) juce::jlimit (0.0, 1.0, t / total) * (float) inner.getWidth();
    };

    for (const auto& m : state.markers)
    {
        const auto c = m.severity >= 2 ? theme::bad : (m.severity == 1 ? theme::warn : theme::info);

        g.setColour (c);
        g.fillRoundedRectangle (xFor (m.time) - 1.5f, y - 14.0f, 3.0f, 28.0f, 1.5f);
    }

    g.setColour (theme::accent);
    g.fillRoundedRectangle (xFor (timeNow (state.stats)) - 1.0f, y - 18.0f, 2.0f, 36.0f, 1.0f);

    g.setColour (theme::textFaint);
    g.setFont (monoFont (tokens::unit));
    g.drawText ("00:00", inner.getX(), (int) strip.getBottom() - 18, 46, 12,
                juce::Justification::centredLeft);
    g.drawText (fmtTime (total), inner.getRight() - 46, (int) strip.getBottom() - 18, 46, 12,
                juce::Justification::centredRight);
}

void DetailArea::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (page != PageId::timeline)
        return;

    const auto strip = timelineStripArea (contentArea());

    if (! strip.contains (e.getPosition()))
        return;

    const double total = timeTotal (state.stats);
    const auto inner = strip.reduced (14, 10);
    const double t = (double) juce::jlimit (0.0f, 1.0f,
        (float) (e.x - inner.getX()) / juce::jmax (1.0f, (float) inner.getWidth())) * total;

    if (onAddMarkerAt != nullptr)
        onAddMarkerAt (t);
}
} // namespace ml
