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

namespace
{
    struct ApertureGeometry
    {
        std::array<juce::Path, 2> paths;
        ApertureGeometry()
        {
            for (size_t size = 0; size < paths.size(); ++size)
            {
                const float gap = size == 0 ? 0.40f : 0.30f;
                const float step = juce::MathConstants<float>::twoPi / 5.0f;
                for (int i = 0; i < 5; ++i)
                    paths[size].addCentredArc (0.5f, 0.5f, 0.365f, 0.365f, 0.0f,
                        step * (float) i + gap * 0.5f,
                        step * (float) (i + 1) - gap * 0.5f, true);
            }
        }
    };
    const ApertureGeometry aperture; // prepare once, outside painting
}

void paintLogoMark (juce::Graphics& g, juce::Rectangle<float> area, bool /*monochrome*/, float /*angle*/)
{
    // oil-ui mark: 32-unit ring, two 10.5-unit arcs and a 3.2-unit core.
    if (area.getWidth() < 4.0f || area.getHeight() < 4.0f)
        return;

    const float s = juce::jmin (area.getWidth(), area.getHeight());
    const auto square = area.withSizeKeepingCentre (s, s);
    const auto centre = square.getCentre();
    const float stroke = juce::jmax (1.0f, s * (1.4f / 32.0f));

    g.setColour (theme::text.withAlpha (0.55f));
    const float ringR = s * (12.5f / 32.0f);
    g.drawEllipse (centre.x - ringR, centre.y - ringR, ringR * 2.0f, ringR * 2.0f, stroke);

    juce::Path arcs;
    const float arcR = s * (10.5f / 32.0f);
    arcs.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f,
                        juce::MathConstants<float>::pi * -0.5f,
                        juce::MathConstants<float>::pi * (-1.0f / 6.0f), true);
    arcs.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f,
                        juce::MathConstants<float>::pi * 0.5f,
                        juce::MathConstants<float>::pi * (5.0f / 6.0f), true);

    g.setColour (theme::text.withAlpha (0.9f));
    g.strokePath (arcs, juce::PathStrokeType (stroke, juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));

    const float dotR = s * (3.2f / 32.0f);
    g.fillEllipse (centre.x - dotR, centre.y - dotR, dotR * 2.0f, dotR * 2.0f);
}

void paintBerryWordmark (juce::Graphics& g, juce::Rectangle<int> area, juce::Colour colour,
                         float height, juce::Justification justification)
{
    const auto font = getLabelFont();
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

    // The wordmark keeps the full row height so that its line box is centred on
    // the same axis as the mark and as any sibling label drawn next to it. The
    // old top strip (textHeight * 1.7) sat about 7 px high at footer sizes.
    paintBerryWordmark (g, area, colour, textHeight, juce::Justification::centredLeft);
}
} // namespace ml
