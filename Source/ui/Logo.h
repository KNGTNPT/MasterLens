#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace ml
{
/** The MasterLens x BerryStudio mark: a five-segment aperture around a centre dot.

    Neutral ink and size-adjusted gaps keep the mark subordinate to the data.
    Everything is vector drawn, so the same routine is used for the header (32 px),
    the About / Settings cards (48-64 px), the app icon (1024 px) and the SVG
    published in docs/logo.svg.
*/
void paintLogoMark (juce::Graphics&, juce::Rectangle<float> area, bool monochrome = false, float angle = 0.0f);

/** "BERRYSTUDIO" wordmark with optical tracking. */
void paintBerryWordmark (juce::Graphics&, juce::Rectangle<int> area, juce::Colour,
                         float height = 10.0f, juce::Justification = juce::Justification::centredLeft);

/** Mark + wordmark, used by About / Settings / splash cards. */
void paintBerryLockup (juce::Graphics&, juce::Rectangle<int> area, juce::Colour,
                       float markSize = 48.0f, float textHeight = 12.0f);
} // namespace ml
