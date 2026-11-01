#include "Logo.h"

#include "Theme.h"

namespace ml
{
namespace
{
    float glyphWidth (const juce::Font& font, const juce::String& text)
    {
        return juce::GlyphArrangement::getStringWidth (font, text);
    }

    void drawTracked (juce::Graphics& g, const juce::String& text, const juce::Font& font,
                      juce::Rectangle<int> area, float tracking, juce::Colour colour,
                      juce::Justification justification)
    {
        float total = 0.0f;

        for (int i = 0; i < text.length(); ++i)
            total += glyphWidth (font, juce::String::charToString (text[i])) + tracking;

        total = juce::jmax (0.0f, total - tracking);

        const float start = justification.testFlags (juce::Justification::horizontallyCentred)
                                ? (float) area.getX() + ((float) area.getWidth() - total) * 0.5f
                                : (justification.testFlags (juce::Justification::right)
                                       ? (float) area.getRight() - total
                                       : (float) area.getX());

        float x = start;

        g.setColour (colour);
        g.setFont (font);

        for (int i = 0; i < text.length(); ++i)
        {
            const auto ch = juce::String::charToString (text[i]);
            const float w = glyphWidth (font, ch);

            g.drawText (ch, juce::Rectangle<float> (x, (float) area.getY(), w + 2.0f,
                                                    (float) area.getHeight()),
                        juce::Justification::centredLeft, false);

            x += w + tracking;
        }
    }
} // namespace

//==============================================================================
void paintLogoMark (juce::Graphics& g, juce::Rectangle<float> area, bool monochrome)
{
    if (area.getWidth() < 4.0f || area.getHeight() < 4.0f)
        return;

    const float s = juce::jmin (area.getWidth(), area.getHeight());
    area = area.withSizeKeepingCentre (s, s);

    const auto centre = area.getCentre();
    const float r = s * 0.365f;
    const float stroke = juce::jmax (1.0f, s * 0.067f);

    // Five unequal arcs. The lower-left arc is the only cyan element; at 16 px
    // everything collapses to the single ink colour so the gaps stay legible.
    const float arcs[5][2]
    {
        { -1.40f, -0.14f },   // top
        {  0.04f,  1.01f },   // upper right
        {  1.33f,  2.42f },   // lower right
        {  2.76f,  3.72f },   // lower left (cyan at >= 32 px)
        {  4.10f,  5.02f }    // upper left
    };

    const bool colourLogo = ! monochrome && s >= 32.0f;
    const auto blue = theme::accent;
    const auto monoInk = theme::text;

    for (int i = 0; i < 5; ++i)
    {
        juce::Path arc;
        arc.addCentredArc (centre.x, centre.y, r, r, 0.0f, arcs[i][0], arcs[i][1], true);

        g.setColour (colourLogo && i == 3 ? theme::cyan
                                          : (monochrome ? monoInk : blue));
        g.strokePath (arc, juce::PathStrokeType (stroke,
                                                 juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
    }

    g.setColour (colourLogo ? theme::cyan : (monochrome ? monoInk : blue));
    g.fillEllipse (centre.x - s * 0.072f, centre.y - s * 0.072f,
                   s * 0.144f, s * 0.144f);
}

void paintBerryWordmark (juce::Graphics& g, juce::Rectangle<int> area, juce::Colour colour,
                         float height, juce::Justification justification)
{
    const auto font = uiFont (height, true);
    drawTracked (g, "BERRYSTUDIO", font, area, height * 0.04f, colour, justification);
}

void paintBerryLockup (juce::Graphics& g, juce::Rectangle<int> area, juce::Colour colour,
                       float markSize, float textHeight)
{
    auto mark = area.removeFromLeft (juce::roundToInt (markSize)).toFloat()
                    .withSizeKeepingCentre (markSize, markSize);

    paintLogoMark (g, mark);

    // Lockup gap is exactly half the mark height.
    area.removeFromLeft (juce::roundToInt (markSize * 0.5f));

    auto textArea = area;
    paintBerryWordmark (g, textArea.removeFromTop (juce::roundToInt (textHeight * 1.7f)),
                        colour, textHeight, juce::Justification::centredLeft);
}
} // namespace ml
