#include "Widgets.h"

namespace ml
{
//==============================================================================
HistoryGraph::HistoryGraph()
{
    setOpaque (true);
    setMouseCursor (juce::MouseCursor::CrosshairCursor);
}

void HistoryGraph::setData (std::vector<float> curve, float targetLufs)
{
    data = std::move (curve);
    target = targetLufs;
    repaint();
}

juce::Rectangle<int> HistoryGraph::plotArea() const
{
    auto b = getLocalBounds().withTrimmedTop (34).withTrimmedBottom (22);

    if (b.getWidth() < 160)
        return b;

    return b.withTrimmedLeft (46).withTrimmedRight (64);
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

std::array<juce::Rectangle<int>, 4> HistoryGraph::chipBounds() const
{
    std::array<juce::Rectangle<int>, 4> out {};

    const char* zh[4] { "1 秒", "10 秒", "60 秒", "整曲" };
    const char* en[4] { "1 s", "10 s", "60 s", "Song" };

    int x = getLocalBounds().getX();
    const int y = getLocalBounds().getY() + 2;

    for (int i = 0; i < 4; ++i)
    {
        const auto text = T (zh[i], en[i]);
        const int w = juce::roundToInt (juce::GlyphArrangement::getStringWidth (uiFont (tokens::unit), text)) + 20;

        out[(size_t) i] = juce::Rectangle<int> (x, y, w, 20);
        x += w + 6;
    }

    return out;
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

    if (plot.contains (e.getPosition()) && onSeek != nullptr)
    {
        const int span = juce::jmax (2, pointsForRange());
        const float stepX = (float) plot.getWidth() / (float) (span - 1);
        const int indexFromRight = juce::jlimit (0, span - 1,
            juce::roundToInt ((float) (plot.getRight() - e.x) / juce::jmax (1.0f, stepX)));
        onSeek ((double) indexFromRight * 0.1);
    }
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

    g.setFont (monoFont (tokens::unit));

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

    g.setFont (uiFont (tokens::unit));
    g.setColour (theme::textFaint);
    g.drawText ("LUFS", juce::Rectangle<int> (plot.getRight() + 8, plot.getY() - 14, 50, 12),
                juce::Justification::centredLeft);

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
        juce::Path path;
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
                path.startNewSubPath (x, y);
                started = true;
                firstX = juce::jmin (firstX, x);
            }
            else
            {
                path.lineTo (x, y);
            }

            lastX = x;
        }

        if (started)
        {
            auto filled = path;
            filled.lineTo (lastX, (float) plot.getBottom());
            filled.lineTo (firstX, (float) plot.getBottom());
            filled.closeSubPath();

            juce::ColourGradient fillGrad (theme::accent.withAlpha (0.16f), 0.0f, (float) plot.getY(),
                                           theme::accent.withAlpha (0.0f), 0.0f, (float) plot.getBottom(), false);
            g.setGradientFill (fillGrad);
            g.fillPath (filled);

            g.setColour (theme::accent);
            g.strokePath (path, juce::PathStrokeType (1.4f));

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
        g.setColour (theme::text);
        g.setFont (monoFont (18.0f, true));
        g.drawText (num (latest),
                    juce::Rectangle<int> (plot.getRight() + 6, plot.getY() + 4, 58, 22),
                    juce::Justification::centredLeft);
    }

    // ---- one relative time axis ---------------------------------------------
    g.setFont (monoFont (tokens::unit));
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
        g.setFont (uiFont (tokens::unit));
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

                const auto font = monoFont (tokens::unit);
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
