#include "Widgets.h"

namespace ml
{
//==============================================================================
HistoryGraph::HistoryGraph()
{
    setOpaque (true);
    setMouseCursor (juce::MouseCursor::CrosshairCursor);
    curvePath.preallocateSpace (4096);
    fillPath.preallocateSpace (4096);
}

void HistoryGraph::setData (const std::vector<float>& curve, float targetLufs)
{
    data.assign (curve.begin(), curve.end());
    target = targetLufs;
    repaint (plotArea());
}

void HistoryGraph::setPlayhead (double seconds, double total)
{
    const float newHead = (float) juce::jmax (0.0, seconds);
    const float newTotal = (float) juce::jmax (0.001, total);

    if (std::abs (newHead - playhead) < 0.001f && std::abs (newTotal - totalSeconds) < 0.001f)
        return;

    playhead = newHead;
    totalSeconds = newTotal;
    repaint (plotArea());
}

juce::Rectangle<int> HistoryGraph::plotArea() const
{
    auto b = getLocalBounds().withTrimmedTop (36).withTrimmedBottom (24);

    if (b.getWidth() < 160)
        return b;

    return b.withTrimmedLeft (48).withTrimmedRight (64);
}

int HistoryGraph::pointsForRange() const
{
    switch (range)
    {
        case Range::oneSecond:  return 10;
        case Range::tenSeconds: return 100;
        case Range::oneMinute:  return 600;
        case Range::whole:      break;
    }

    return juce::jmax (2, (int) data.size());
}

std::array<juce::Rectangle<int>, 4> HistoryGraph::chipBounds()
{
    if (! chipsDirty)
        return chipCache;

    const char* zh[4] { "1 秒", "10 秒", "60 秒", "整曲" };
    const char* en[4] { "1 s", "10 s", "60 s", "Song" };

    int x = getLocalBounds().getX();
    const int y = getLocalBounds().getY() + 2;

    for (int i = 0; i < 4; ++i)
    {
        const auto text = T (zh[i], en[i]);
        const int w = juce::roundToInt (juce::GlyphArrangement::getStringWidth (getLabelFont(), text)) + 20;

        chipCache[(size_t) i] = juce::Rectangle<int> (x, y, w, 20);
        x += w + 6;
    }

    chipsDirty = false;
    return chipCache;
}

void HistoryGraph::mouseDown (const juce::MouseEvent& e)
{
    const auto chips = chipBounds();

    for (int i = 0; i < 4; ++i)
    {
        if (chips[(size_t) i].contains (e.getPosition()))
        {
            range = (Range) i;
            repaint();
            return;
        }
    }

    const auto plot = plotArea();

    if (plot.contains (e.getPosition()))
    {
        const int span = juce::jmax (2, pointsForRange());
        const float stepX = (float) plot.getWidth() / (float) (span - 1);
        const int indexFromRight = juce::jlimit (0, span - 1,
            juce::roundToInt ((float) (plot.getRight() - e.x) / juce::jmax (1.0f, stepX)));

        dragging = true;
        dragStartAge = dragEndAge = (float) indexFromRight * 0.1f;
        repaint (plotArea());
    }
}

void HistoryGraph::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragging)
        return;

    const auto plot = plotArea();

    if (plot.getWidth() <= 0)
        return;

    const int span = juce::jmax (2, pointsForRange());
    const float stepX = (float) plot.getWidth() / (float) (span - 1);
    const int indexFromRight = juce::jlimit (0, span - 1,
        juce::roundToInt ((float) (plot.getRight() - e.x) / juce::jmax (1.0f, stepX)));

    dragEndAge = (float) indexFromRight * 0.1f;
    repaint (plotArea());
}

void HistoryGraph::mouseUp (const juce::MouseEvent&)
{
    if (! dragging)
        return;

    dragging = false;

    if (dragStartAge >= 0.0f && dragEndAge >= 0.0f
        && std::abs (dragEndAge - dragStartAge) >= 0.15f && onSelection != nullptr)
    {
        onSelection ((double) juce::jmin (dragStartAge, dragEndAge),
                     (double) juce::jmax (dragStartAge, dragEndAge));
    }
    else if (onSeek != nullptr)
    {
        onSeek ((double) dragStartAge);
    }

    dragStartAge = dragEndAge = -1.0f;
    repaint (plotArea());
}

void HistoryGraph::mouseMove (const juce::MouseEvent& e)
{
    if (! plotArea().contains (e.getPosition()))
    {
        if (hoverX >= 0)
        {
            hoverX = -1;
            repaint();
        }

        return;
    }

    hoverX = e.x;
    repaint();
}

void HistoryGraph::mouseExit (const juce::MouseEvent&)
{
    if (hoverX >= 0)
    {
        hoverX = -1;
        repaint();
    }
}

void HistoryGraph::ensureBackground()
{
    if (! backgroundDirty && ! background.isNull()
        && cachedThemeRevision == theme::revision()
        && background.getWidth() == juce::roundToInt (getWidth() * juce::jmax (1.0f, getDesktopScaleFactor()))
        && background.getHeight() == juce::roundToInt (getHeight() * juce::jmax (1.0f, getDesktopScaleFactor())))
        return;

    if (getWidth() <= 0 || getHeight() <= 0)
        return;

    const int scale = juce::roundToInt (juce::jmax (1.0f, getDesktopScaleFactor()));
    background = juce::Image (juce::Image::ARGB, getWidth() * scale, getHeight() * scale, true);
    juce::Graphics bg (background);
    bg.addTransform (juce::AffineTransform::scale ((float) scale));
    surface::graph (bg, getLocalBounds().toFloat());

    backgroundDirty = false;
    cachedThemeRevision = theme::revision();
}

void HistoryGraph::paint (juce::Graphics& g)
{
    ensureBackground();
    g.drawImage (background, getLocalBounds().toFloat());
    paintPlot (g);
    paintOverlay (g);
}

void HistoryGraph::paintPlot (juce::Graphics& g)
{
    const auto plot = plotArea();

    if (plot.getWidth() < 40 || plot.getHeight() < 30)
        return;

    const auto yFor = [plot] (float db)
    {
        const float t = juce::jlimit (0.0f, 1.0f, (db + 36.0f) / 36.0f);
        return (float) plot.getBottom() - t * (float) plot.getHeight();
    };

    // ---- grid and y axis -----------------------------------------------------
    const float step = plot.getHeight() < 150 ? 12.0f : 6.0f;

    g.setFont (getValueFont());

    for (float db = 0.0f; db >= -36.0f; db -= step)
    {
        const float y = yFor (db);

        g.setColour (db == 0.0f || db == -36.0f ? theme::border : theme::borderSoft);
        g.drawHorizontalLine (juce::roundToInt (y), (float) plot.getX(), (float) plot.getRight());

        g.setColour (theme::textFaint);
        g.drawText (num (db, 0),
                    juce::Rectangle<float> ((float) plot.getX() - 42.0f, y - 6.0f, 36.0f, 12.0f),
                    juce::Justification::centredRight);
    }

    // The unit lives with the current value; the curve itself stays clean.

    // ---- LRA allowance band (8 % fill) --------------------------------------
    if (bandLra > 0.05f)
    {
        const float top = yFor (juce::jmin (0.0f, target + bandLra * 0.5f));
        const float bottom = yFor (juce::jmax (-36.0f, target - bandLra * 0.5f));

        g.setColour (theme::good.withAlpha (0.08f));
        g.fillRect (juce::Rectangle<float> ((float) plot.getX(), top, (float) plot.getWidth(),
                                            juce::jmax (1.0f, bottom - top)));
    }

    // ---- dashed target line --------------------------------------------------
    {
        const float y = yFor (juce::jlimit (-36.0f, 0.0f, target));
        g.setColour (theme::good.withAlpha (0.85f));

        for (float x = (float) plot.getX(); x < (float) plot.getRight(); x += 9.0f)
            g.fillRect (x, y - 0.5f, 5.0f, 1.0f);
    }

    // ---- curve ---------------------------------------------------------------
    const int span = juce::jmax (2, pointsForRange());
    const int count = juce::jmin ((int) data.size(), span);
    const float stepX = (float) plot.getWidth() / (float) (span - 1);

    if (count >= 2)
    {
        curvePath.clear();
        bool started = false;
        float firstX = (float) plot.getRight(), lastX = (float) plot.getRight();

        for (int i = 0; i < count; ++i)
        {
            const float value = data[(size_t) ((int) data.size() - count + i)];

            if (value <= -99.0f)
            {
                started = false;
                continue;
            }

            const int fromRight = count - 1 - i;
            const float x = (float) plot.getRight() - (float) fromRight * stepX;
            const float y = yFor (value);

            if (! started)
            {
                curvePath.startNewSubPath (x, y);
                started = true;
                firstX = juce::jmin (firstX, x);
            }
            else
            {
                curvePath.lineTo (x, y);
            }

            lastX = x;
        }

        if (started)
        {
            fillPath.clear();
            fillPath.addPath (curvePath);
            fillPath.lineTo (lastX, (float) plot.getBottom());
            fillPath.lineTo (firstX, (float) plot.getBottom());
            fillPath.closeSubPath();

            juce::ColourGradient fillGrad (theme::accent.withAlpha (0.16f), 0.0f, (float) plot.getY(),
                                           theme::accent.withAlpha (0.0f), 0.0f, (float) plot.getBottom(), false);
            g.setGradientFill (fillGrad);
            g.fillPath (fillPath);

            g.setColour (theme::accent);
            g.strokePath (curvePath, juce::PathStrokeType (1.4f));

            // Events live on the curve, not on a separate 40 px strip.
            for (const auto& ev : events)
            {
                if (ev.ageSeconds < 0.0f)
                    continue;

                const float mx = (float) plot.getRight() - (ev.ageSeconds / 0.1f) * stepX;

                if (mx < (float) plot.getX() || mx > (float) plot.getRight())
                    continue;

                const auto c = ev.severity >= 2 ? theme::bad
                             : (ev.severity == 1 ? theme::warn : theme::accent);
                g.setColour (c.withAlpha (0.7f));
                g.fillRoundedRectangle (mx - 1.0f, (float) plot.getY(), 2.0f,
                                        (float) plot.getHeight(), 1.0f);
            }
        }
    }
}


void HistoryGraph::paintOverlay (juce::Graphics& g)
{
    const auto plot = plotArea();

    if (plot.getWidth() < 40 || plot.getHeight() < 30)
        return;

    const auto yFor = [plot] (float db)
    {
        const float t = juce::jlimit (0.0f, 1.0f, (db + 36.0f) / 36.0f);
        return (float) plot.getBottom() - t * (float) plot.getHeight();
    };

    const int span = juce::jmax (2, pointsForRange());
    const float stepX = (float) plot.getWidth() / (float) (span - 1);

    // ---- current value (top right, next to the unit) ------------------------
    const float latest = data.empty() ? -100.0f : data.back();

    if (latest > -99.0f)
    {
        auto valueArea = plot.withHeight (20).withTrimmedTop (4).withTrimmedRight (4)
                             .removeFromRight (92);
        auto unitArea = valueArea.removeFromRight (34).withTrimmedLeft (6);
        g.setColour (theme::text);
        g.setFont (getValueFont());
        g.drawText (num (latest), valueArea, juce::Justification::centredRight);
        g.setColour (theme::textFaint);
        g.setFont (getLabelFont());
        g.drawText ("LUFS", unitArea, juce::Justification::centredLeft);
    }

    // ---- drag selection ------------------------------------------------------
    if (dragStartAge >= 0.0f && dragEndAge >= 0.0f
        && std::abs (dragEndAge - dragStartAge) > 0.01f)
    {
        const float a = (float) plot.getRight() - (juce::jmax (dragStartAge, dragEndAge) / 0.1f) * stepX;
        const float b = (float) plot.getRight() - (juce::jmin (dragStartAge, dragEndAge) / 0.1f) * stepX;
        const float x1 = juce::jlimit ((float) plot.getX(), (float) plot.getRight(), a);
        const float x2 = juce::jlimit ((float) plot.getX(), (float) plot.getRight(), b);

        g.setColour (theme::accent.withAlpha (0.10f));
        g.fillRect (juce::Rectangle<float> (x1, (float) plot.getY(), x2 - x1,
                                            (float) plot.getHeight()));
        g.setColour (theme::accent);
        g.fillRect (x1, (float) plot.getY(), 1.0f, (float) plot.getHeight());
        g.fillRect (x2, (float) plot.getY(), 1.0f, (float) plot.getHeight());
    }

    // ---- playhead: the old bottom strip now lives in the graph --------------
    if (totalSeconds > 0.001f && playhead > 0.0f)
    {
        const float age = juce::jmax (0.0f, totalSeconds - playhead);
        const float hx = (float) plot.getRight() - (age / 0.1f) * stepX;

        if (hx >= (float) plot.getX() && hx <= (float) plot.getRight())
        {
            g.setColour (theme::accent.withAlpha (0.85f));
            g.fillRect (hx - 0.5f, (float) plot.getY(), 1.0f, (float) plot.getHeight());
            g.fillEllipse (hx - 3.0f, (float) plot.getY() - 3.0f, 6.0f, 6.0f);
        }
    }

    // ---- one relative time axis ---------------------------------------------
    g.setFont (getValueFont());
    g.setColour (theme::textFaint);

    const float midSeconds = (float) (span - 1) * 0.05f;
    const float leftSeconds = (float) (span - 1) * 0.1f;

    const auto secondsText = [] (float seconds)
    {
        return minusSign()
             + (seconds >= 9.95f ? juce::String (juce::roundToInt (seconds))
                                 : juce::String (seconds, 1))
             + " s";
    };

    const auto leftLabel = span <= 2 ? juce::String() : secondsText (leftSeconds);
    const auto midLabel = secondsText (midSeconds);

    g.drawText (leftLabel,
                juce::Rectangle<int> (plot.getX(), plot.getBottom() + 4, 64, 12),
                juce::Justification::centredLeft);
    g.drawText (midLabel,
                juce::Rectangle<int> (plot.getCentreX() - 30, plot.getBottom() + 4, 60, 12),
                juce::Justification::centred);
    g.drawText (T ("现在", "now"),
                juce::Rectangle<int> (plot.getRight() - 44, plot.getBottom() + 4, 44, 12),
                juce::Justification::centredRight);

    // ---- window chips --------------------------------------------------------
    const auto chips = chipBounds();

    const char* zh[4] { "1 秒", "10 秒", "60 秒", "整曲" };
    const char* en[4] { "1 s", "10 s", "60 s", "Song" };

    for (int i = 0; i < 4; ++i)
    {
        const bool active = (int) range == i;

        g.setColour (active ? theme::accentSoft : theme::raised);
        g.fillRoundedRectangle (chips[(size_t) i].toFloat(), tokens::radiusControl);

        g.setColour (active ? theme::accent : theme::textDim);
        g.setFont (getLabelFont());
        g.drawText (T (zh[i], en[i]), chips[(size_t) i], juce::Justification::centred);
    }

    // ---- hover crosshair + readout ------------------------------------------
    if (hoverX >= plot.getX() && hoverX <= plot.getRight() && ! data.empty())
    {
        const int indexFromRight = juce::jlimit (0, span - 1,
            juce::roundToInt ((float) (plot.getRight() - hoverX) / juce::jmax (1.0f, stepX)));

        if (indexFromRight < (int) data.size())
        {
            const float value = data[(size_t) ((int) data.size() - 1 - indexFromRight)];
            const float x = (float) plot.getRight() - (float) indexFromRight * stepX;

            g.setColour (theme::textDim.withAlpha (0.55f));
            g.drawVerticalLine (juce::roundToInt (x), (float) plot.getY(), (float) plot.getBottom());

            if (value > -99.0f)
            {
                g.setColour (theme::accent);
                g.fillEllipse (x - 3.0f, yFor (value) - 3.0f, 6.0f, 6.0f);

                const float seconds = (float) indexFromRight * 0.1f;
                const auto text = (seconds <= 0.05f ? T ("现在", "now")
                                                    : minusSign() + juce::String (seconds, 1) + " s")
                                  + "   " + num (value) + " LUFS";

                const auto font = getValueFont();
                const int tw = juce::roundToInt (juce::GlyphArrangement::getStringWidth (font, text)) + 16;

                auto tip = juce::Rectangle<int> (juce::roundToInt (x) + 10, plot.getY() + 6, tw, 22);
                tip.setX (juce::jlimit (plot.getX(), plot.getRight() - tw, tip.getX()));

                g.setColour (theme::raised);
                g.fillRoundedRectangle (tip.toFloat(), tokens::radiusControl);

                g.setColour (theme::text);
                g.setFont (font);
                g.drawText (text, tip, juce::Justification::centred);
            }
        }
    }
}

} // namespace ml
