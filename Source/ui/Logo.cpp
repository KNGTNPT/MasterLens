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

void paintLogoMark (juce::Graphics& g, juce::Rectangle<float> area, bool /*monochrome*/, float angle)
{
    if (area.getWidth() < 4.0f || area.getHeight() < 4.0f)
        return;

    const float s = juce::jmin (area.getWidth(), area.getHeight());
    area = area.withSizeKeepingCentre (s, s);
    g.setColour (theme::textDim);
    const auto transform = juce::AffineTransform::rotation (angle, 0.5f, 0.5f)
        .scaled (s).translated (area.getPosition());
    g.strokePath (aperture.paths[s < 32.0f ? 0 : 1],
                  juce::PathStrokeType (juce::jmax (1.0f, s * 0.060f), juce::PathStrokeType::curved,
                                        juce::PathStrokeType::rounded), transform);
    const auto centre = area.getCentre();
    g.fillEllipse (centre.x - s * 0.060f, centre.y - s * 0.060f, s * 0.120f, s * 0.120f);
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
