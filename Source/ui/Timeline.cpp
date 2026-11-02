#include "Modules.h"

namespace ml
{
//==============================================================================
TimelineStrip::TimelineStrip()
{
    setOpaque (false);
}

void TimelineStrip::setMarkers (const std::vector<Marker>& m, double totalSeconds)
{
    markers = m;
    total = juce::jmax (1.0, totalSeconds);
    repaint();
}

void TimelineStrip::setPlayhead (double seconds)
{
    playhead = juce::jmax (0.0, seconds);
    repaint();
}

juce::Rectangle<int> TimelineStrip::trackArea() const
{
    return getLocalBounds().reduced (14, 0).withTrimmedTop (20).withHeight (12);
}

double TimelineStrip::timeAtX (int x) const
{
    const auto track = trackArea();
    const float t = juce::jlimit (0.0f, 1.0f,
                                  (float) (x - track.getX()) / juce::jmax (1.0f, (float) track.getWidth()));
    return (double) t * total;
}

void TimelineStrip::paint (juce::Graphics& g)
{
    g.fillAll (theme::cardAlt.withAlpha (0.55f));

    const auto b = getLocalBounds().toFloat();
    const auto track = trackArea();
    const float y = (float) track.getCentreY();

    g.setColour (theme::textFaint);
    g.setFont (getLabelFont());
    g.drawText (T ("整曲时间线", "Song timeline"), 14, 2, 120, 14, juce::Justification::centredLeft);

    g.setColour (theme::textFaint);
    g.setFont (getLabelFont());
    g.drawText (T ("双击添加标记", "double-click to add"), b.getWidth() - 128.0f, 2.0f, 114.0f, 14.0f,
                juce::Justification::centredRight);

    g.setColour (theme::borderSoft);
    g.fillRoundedRectangle ((float) track.getX(), y - 1.5f, (float) track.getWidth(), 3.0f, 1.5f);

    if (selectionEnd > selectionStart + 0.01)
    {
        const float x1 = (float) track.getX()
                         + (float) juce::jlimit (0.0, 1.0, selectionStart / total) * (float) track.getWidth();
        const float x2 = (float) track.getX()
                         + (float) juce::jlimit (0.0, 1.0, selectionEnd / total) * (float) track.getWidth();

        g.setColour (theme::accent.withAlpha (0.18f));
        g.fillRect (juce::Rectangle<float> (x1, (float) track.getY() - 16.0f,
                                            juce::jmax (2.0f, x2 - x1), (float) track.getHeight() + 32.0f));
        g.setColour (theme::accent);
        g.drawVerticalLine (juce::roundToInt (x1), (float) track.getY() - 16.0f,
                            (float) track.getY() + 16.0f);
        g.drawVerticalLine (juce::roundToInt (x2), (float) track.getY() - 16.0f,
                            (float) track.getY() + 16.0f);
    }

    for (const auto& m : markers)
    {
        const float t = (float) juce::jlimit (0.0, 1.0, m.time / total);
        const float x = (float) track.getX() + t * (float) track.getWidth();
        const auto c = m.severity >= 2 ? theme::bad : (m.severity == 1 ? theme::warn : theme::info);

        g.setColour (c);
        g.fillRoundedRectangle (x - 1.5f, y - 10.0f, 3.0f, 20.0f, 1.5f);
    }

    if (playhead > 0.0)
    {
        const float t = (float) juce::jlimit (0.0, 1.0, playhead / total);
        const float x = (float) track.getX() + t * (float) track.getWidth();

        g.setColour (theme::accent);
        g.fillRoundedRectangle (x - 1.0f, y - 14.0f, 2.0f, 28.0f, 1.0f);
    }

    g.setColour (theme::textFaint);
    g.setFont (getValueFont());
    g.drawText ("00:00", track.getX(), (int) b.getBottom() - 14, 46, 12,
                juce::Justification::centredLeft);
    g.drawText (fmtTime (total), track.getRight() - 46, (int) b.getBottom() - 14, 46, 12,
                juce::Justification::centredRight);
}

void TimelineStrip::mouseDown (const juce::MouseEvent& e)
{
    if (! trackArea().expanded (0, 10).contains (e.getPosition()))
        return;

    draggingSelection = true;
    selectionStart = selectionEnd = timeAtX (e.x);
    repaint();
}

void TimelineStrip::mouseDrag (const juce::MouseEvent& e)
{
    if (! draggingSelection)
        return;

    selectionEnd = timeAtX (e.x);

    if (selectionEnd < selectionStart)
        std::swap (selectionStart, selectionEnd);

    repaint();
}

void TimelineStrip::mouseUp (const juce::MouseEvent&)
{
    if (! draggingSelection)
        return;

    draggingSelection = false;

    if (selectionEnd - selectionStart >= 0.15 && onSelectionChanged != nullptr)
        onSelectionChanged (selectionStart, selectionEnd);

    repaint();
}

void TimelineStrip::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (onDoubleClick != nullptr)
        onDoubleClick (timeAtX (e.x));
}
} // namespace ml
