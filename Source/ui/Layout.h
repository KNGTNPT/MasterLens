#pragma once

#include "Theme.h"

namespace ml::grid
{
inline int down (int value) noexcept { return juce::jmax (0, value / 4 * 4); }
inline juce::String controlText (const juce::String& text)
{
    return text.toUpperCase().replace (" DBTP", " dBTP").replace (" DBFS", " dBFS")
        .replace (" DB", " dB").replace (" KHZ", " kHz").replace (" HZ", " Hz");
}

inline juce::Rectangle<int> bounds (juce::Rectangle<int> r) noexcept
{
    return { down (r.getX()), down (r.getY()), down (r.getWidth()), down (r.getHeight()) };
}

/** Explicit pixel tracks avoid fractional Grid distribution at arbitrary sizes. */
inline void row (juce::Rectangle<int> area, std::initializer_list<juce::Component*> components,
                 std::initializer_list<int> widths)
{
    jassert (components.size() == widths.size());
    juce::Grid grid;
    grid.setGap (juce::Grid::Px (8));
    grid.templateRows = { juce::Grid::TrackInfo (juce::Grid::Px (down (area.getHeight()))) };
    for (const auto width : widths)
        grid.templateColumns.add (juce::Grid::TrackInfo (juce::Grid::Px (down (width))));
    for (auto* c : components) grid.items.add (juce::GridItem (*c));
    grid.performLayout (bounds (area));
}

/** Normalise project-owned child boundaries after each component's subdivision.
    JUCE's internal text caret, viewport plumbing and resize handle are not a
    second layout system. Inspector displays the component's logical bounds.
*/
inline void alignChildren (juce::Component& parent)
{
    for (auto* child : parent.getChildren())
    {
        if (dynamic_cast<juce::ResizableCornerComponent*> (child) != nullptr) continue;
        child->setBounds (bounds (child->getBounds()));
        child->getProperties().set ("mlGrid", 4);
        if (auto* label = dynamic_cast<juce::Label*> (child))
            label->setFont (labelFontFor (label->getText()));
        if (auto* text = dynamic_cast<juce::TextEditor*> (child)) text->setFont (getValueFont());
    }
}
} // namespace ml::grid
