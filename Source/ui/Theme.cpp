#include "Theme.h"

#include <map>

namespace ml
{
//==============================================================================
void theme::apply (bool useDark)
{
    theme::dark = useDark;

    if (useDark)
    {
        theme::background = juce::Colour (0xff171a1b);
        theme::card       = juce::Colour (0xff202425);
        theme::cardAlt    = juce::Colour (0xff2a2f30);
        theme::raised     = juce::Colour (0xff2a2f30);
        theme::border     = juce::Colour (0xff3b4242);
        theme::borderSoft = juce::Colour (0xff303637);
        theme::divider    = juce::Colour (0xff303637);
        theme::text       = juce::Colour (0xffe5e9e3);
        theme::textDim    = juce::Colour (0xffa4afaa);
        theme::textFaint  = juce::Colour (0xff89958f);
        theme::accent     = juce::Colour (0xff99c9b4);
        theme::accentSoft = juce::Colour (0xff30463e);
        theme::cyan       = juce::Colour (0xff91b9c5);
        theme::good       = juce::Colour (0xff99c9b4);
        theme::info       = juce::Colour (0xff99c9b4);
        theme::warn       = juce::Colour (0xffdbb57a);
        theme::bad        = juce::Colour (0xffe1897c);
        theme::violet     = juce::Colour (0xffb7abd0);
    }
    else
    {
        theme::background = juce::Colour (0xffe5e6df);
        theme::card       = juce::Colour (0xffffffff);
        theme::cardAlt    = juce::Colour (0xffe9ece5);
        theme::raised     = juce::Colour (0xffffffff);
        theme::border     = juce::Colour (0xffc4cdc5);
        theme::borderSoft = juce::Colour (0xffd0d7cf);
        theme::divider    = juce::Colour (0xffd0d7cf);
        theme::text       = juce::Colour (0xff26352e);
        theme::textDim    = juce::Colour (0xff53665b);
        theme::textFaint  = juce::Colour (0xff62776a);
        theme::accent     = juce::Colour (0xff356d56);
        theme::accentSoft = juce::Colour (0xffd6e5d9);
        theme::cyan       = juce::Colour (0xff426b7a);
        theme::good       = juce::Colour (0xff14835a);
        theme::info       = juce::Colour (0xff356d56);
        theme::warn       = juce::Colour (0xffb46a10);
        theme::bad        = juce::Colour (0xffc9302a);
        theme::violet     = juce::Colour (0xff5f4fe0);
    }

    ++theme::revisionCounter;
}

//==============================================================================
namespace surface
{
static float desktopScale()
{
    if (auto* d = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
        return juce::jmax (1.0f, (float) d->scale);

    return 1.0f;
}

static juce::Colour lift (juce::Colour c, float amount)
{
    return c.overlaidWith (juce::Colours::white.withAlpha (amount));
}

static juce::Colour sink (juce::Colour c, float amount)
{
    return c.overlaidWith (juce::Colours::black.withAlpha (amount));
}

void raised (juce::Graphics& g, juce::Rectangle<float> area, float radius)
{
    // One light from above-left: top is a hair lighter, bottom a hair darker.
    g.setGradientFill (juce::ColourGradient (lift (theme::raised, 0.035f),
                                             area.getX(), area.getY(),
                                             sink (theme::raised, 0.035f),
                                             area.getX(), area.getBottom(), false));
    g.fillRoundedRectangle (area, radius);

    g.setColour (juce::Colours::white.withAlpha (theme::dark ? 0.06f : 0.30f));
    g.drawHorizontalLine (juce::roundToInt (area.getY() + 0.5f),
                          area.getX() + radius, area.getRight() - radius);

    // Fixed raised shadow; this is two solid translucent strips, not a blur.
    g.setColour (juce::Colours::black.withAlpha (0.20f));
    g.fillRoundedRectangle (area.translated (0.0f, 1.0f).withTrimmedTop (area.getHeight() - 1.0f),
                            radius * 0.5f);
    g.setColour (juce::Colours::black.withAlpha (0.10f));
    g.fillRoundedRectangle (area.translated (0.0f, 2.0f).withTrimmedTop (area.getHeight() - 1.0f),
                            radius * 0.5f);
}

void recessed (juce::Graphics& g, juce::Rectangle<float> area, float radius)
{
    g.setColour (theme::card);
    g.fillRoundedRectangle (area, radius);

    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.drawHorizontalLine (juce::roundToInt (area.getY() + 0.5f),
                          area.getX() + radius, area.getRight() - radius);

    g.setColour (juce::Colours::white.withAlpha (0.04f));
    g.drawHorizontalLine (juce::roundToInt (area.getBottom() - 0.5f),
                          area.getX() + radius, area.getRight() - radius);
}

void hairlineH (juce::Graphics& g, float x1, float x2, float y, juce::Colour colour)
{
    g.setColour (colour);
    g.fillRect (x1, y, x2 - x1, 1.0f / desktopScale());
}

void hairlineV (juce::Graphics& g, float x, float y1, float y2, juce::Colour colour)
{
    g.setColour (colour);
    g.fillRect (x, y1, 1.0f / desktopScale(), y2 - y1);
}

void cornerTicks (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour)
{
    if (area.getWidth() < 24.0f || area.getHeight() < 24.0f)
        return;

    g.setColour (colour.withAlpha (0.75f));
    const float len = 3.0f;

    g.fillRect (area.getX(), area.getY(), len, 1.0f);
    g.fillRect (area.getX(), area.getY(), 1.0f, len);
    g.fillRect (area.getRight() - len, area.getY(), len, 1.0f);
    g.fillRect (area.getRight() - 1.0f, area.getY(), 1.0f, len);
    g.fillRect (area.getX(), area.getBottom() - 1.0f, len, 1.0f);
    g.fillRect (area.getX(), area.getBottom() - len, 1.0f, len);
    g.fillRect (area.getRight() - len, area.getBottom() - 1.0f, len, 1.0f);
    g.fillRect (area.getRight() - 1.0f, area.getBottom() - len, 1.0f, len);
}

void glow (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour, float radius)
{
    g.setColour (colour.withAlpha (0.12f));
    g.drawRoundedRectangle (area.expanded (8.0f), radius + 8.0f, 1.0f);
    g.setColour (colour.withAlpha (0.18f));
    g.drawRoundedRectangle (area.expanded (4.0f), radius + 4.0f, 1.0f);
    g.setColour (colour);
    g.drawRoundedRectangle (area, radius, 1.0f);
}

void panel (juce::Graphics& g, juce::Rectangle<float> r, bool isRaised)
{
    if (isRaised)
    {
        raised (g, r, tokens::radiusCard);
    }
    else
    {
        g.setColour (theme::card);
        g.fillRoundedRectangle (r, tokens::radiusCard);

        // A 1 px inner top highlight keeps the panel from reading as a hole.
        g.setColour (juce::Colours::white.withAlpha (theme::dark ? 0.045f : 0.30f));
        g.drawHorizontalLine (juce::roundToInt (r.getY() + 0.5f),
                              r.getX() + tokens::radiusCard, r.getRight() - tokens::radiusCard);
    }
}

void graph (juce::Graphics& g, juce::Rectangle<float> area)
{
    panel (g, area, false);
    cornerTicks (g, area.reduced (8.0f));
}

void noise (juce::Graphics&, juce::Rectangle<float>) {}
void vignette (juce::Graphics&, juce::Rectangle<float>) {}
void shadow (juce::Graphics& g, juce::Rectangle<float> area, float radius)
{
    g.setColour (juce::Colours::black.withAlpha (0.18f));
    g.fillRoundedRectangle (area.translated (0.0f, 1.0f), radius);
}
} // namespace surface

//==============================================================================
juce::String typefaceName()
{
    static const juce::String name = []
    {
        const juce::StringArray preferred { "PingFang SC", "Hiragino Sans GB", "Heiti SC",
                                            "Noto Sans CJK SC", "Helvetica Neue", "DejaVu Sans" };
        const auto all = juce::Font::findAllTypefaceNames();

        for (const auto& p : preferred)
            if (all.contains (p))
                return p;

        return juce::Font::getDefaultSansSerifFontName();
    }();

    return name;
}

static const juce::StringArray& allTypefaceNames()
{
    static const juce::StringArray names = juce::Font::findAllTypefaceNames();
    return names;
}

static std::map<int, juce::Font>& fontCache (bool bold)
{
    // Deliberately leaked: destroying juce::Font objects after JUCE's own
    // static font machinery has been torn down crashes on exit / plug-in unload.
    static auto* caches = new std::map<int, juce::Font>[2];
    return caches[bold ? 1 : 0];
}

juce::Font uiFont (float height, bool bold)
{
    auto& cache = fontCache (bold);

    const int key = juce::roundToInt (height * 10.0f);
    const auto it = cache.find (key);

    if (it != cache.end())
        return it->second;

    juce::Font font (juce::FontOptions (typefaceName(), height * 1.22f,
                                        bold ? juce::Font::bold : juce::Font::plain)
                         .withFallbacks ({ "PingFang SC", "Hiragino Sans GB", "Noto Sans CJK SC", "Arial Unicode MS" }));

    cache.emplace (key, font);
    return font;
}

juce::Font monoFont (float height, bool bold)
{
    auto& cache = fontCache (bold);

    const int key = juce::roundToInt (height * 10.0f) + 100000;
    const auto it = cache.find (key);

    if (it != cache.end())
        return it->second;

    static const juce::String name = []
    {
        const auto& all = allTypefaceNames();

        for (const auto* candidate : { "SF Mono", "Menlo", "Monaco", "DejaVu Sans Mono" })
            if (all.contains (candidate))
                return juce::String (candidate);

        return juce::Font::getDefaultMonospacedFontName();
    }();

    juce::Font font (juce::FontOptions (name, height * 1.1f,
                                        bold ? juce::Font::bold : juce::Font::plain)
                         .withFallbacks ({ "PingFang SC", "Menlo", "Arial Unicode MS" }));

    cache.emplace (key, font);
    return font;
}

juce::Colour severityColour (Severity s)
{
    switch (s)
    {
        case Severity::critical: return theme::bad;
        case Severity::warn:     return theme::warn;
        case Severity::info:     return theme::info;
        default:                 return theme::good;
    }
}
} // namespace ml


namespace ml
{
//==============================================================================
MasterLensLookAndFeel::MasterLensLookAndFeel()
{
    refreshColours();
}

void MasterLensLookAndFeel::refreshColours()
{
    setColour (juce::ResizableWindow::backgroundColourId, theme::background);
    setColour (juce::DocumentWindow::textColourId, theme::text);

    setColour (juce::Label::textColourId, theme::text);
    setColour (juce::TextEditor::backgroundColourId, theme::cardAlt);
    setColour (juce::TextEditor::textColourId, theme::text);
    setColour (juce::TextEditor::outlineColourId, theme::border);
    setColour (juce::TextEditor::focusedOutlineColourId, theme::accent);
    setColour (juce::TextEditor::highlightColourId, theme::accent.withAlpha (0.25f));

    setColour (juce::TextButton::buttonColourId, theme::cardAlt);
    setColour (juce::TextButton::buttonOnColourId, theme::accentSoft);
    setColour (juce::TextButton::textColourOffId, theme::text);
    setColour (juce::TextButton::textColourOnId, theme::accent);

    setColour (juce::ComboBox::backgroundColourId, theme::cardAlt);
    setColour (juce::ComboBox::textColourId, theme::text);
    setColour (juce::ComboBox::outlineColourId, theme::border);
    setColour (juce::ComboBox::arrowColourId, theme::textDim);

    setColour (juce::PopupMenu::backgroundColourId, theme::card);
    setColour (juce::PopupMenu::textColourId, theme::text);
    setColour (juce::PopupMenu::headerTextColourId, theme::textDim);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, theme::accentSoft);
    setColour (juce::PopupMenu::highlightedTextColourId, theme::accent);

    setColour (juce::Slider::textBoxTextColourId, theme::text);
    setColour (juce::Slider::textBoxBackgroundColourId, theme::cardAlt);
    setColour (juce::Slider::textBoxOutlineColourId, theme::border);
    setColour (juce::Slider::thumbColourId, theme::accent);

    setColour (juce::ScrollBar::thumbColourId, theme::border);
    setColour (juce::ScrollBar::trackColourId, juce::Colours::transparentBlack);

    setColour (juce::TooltipWindow::backgroundColourId, theme::cardAlt);
    setColour (juce::TooltipWindow::textColourId, theme::text);
    setColour (juce::TooltipWindow::outlineColourId, theme::border);
}

void MasterLensLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                              float sliderPos, float minSliderPos, float maxSliderPos,
                                              juce::Slider::SliderStyle style, juce::Slider& slider)
{
    if (style != juce::Slider::LinearHorizontal)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, minSliderPos,
                                          maxSliderPos, style, slider);
        return;
    }

    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height);
    auto track = juce::Rectangle<float> (bounds.getX(), bounds.getCentreY() - 1.0f,
                                         bounds.getWidth(), 2.0f);

    g.setColour (theme::borderSoft);
    g.fillRect (track);
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillRect (track.withHeight (1.0f));
    g.setColour (juce::Colours::white.withAlpha (0.04f));
    g.fillRect (track.withTop (track.getBottom() - 1.0f));

    const float pos = juce::jlimit (track.getX(), track.getRight(), sliderPos);
    g.setColour (slider.isEnabled() ? theme::accent : theme::textFaint.withAlpha (0.4f));
    g.fillRect (track.withRight (pos));

    const bool dragging = slider.isMouseButtonDown() && slider.isMouseOverOrDragging();
    const float thumbH = dragging ? 18.0f : 14.0f;
    auto thumb = juce::Rectangle<float> (pos - 5.0f, bounds.getCentreY() - thumbH * 0.5f,
                                         10.0f, thumbH);

    g.setColour (theme::raised);
    g.fillRoundedRectangle (thumb, 3.0f);
    g.setColour (theme::border);
    g.drawRoundedRectangle (thumb, 3.0f, 1.0f);
    g.setColour (theme::accent);
    g.fillRoundedRectangle (thumb.reduced (3.0f, 2.0f), 1.5f);

    if (slider.hasKeyboardFocus (true))
    {
        g.setColour (theme::accent);
        g.drawRoundedRectangle (thumb.expanded (2.0f), 5.0f, 1.0f);
    }

    if (dragging)
    {
        const auto valueText = slider.getTextFromValue (slider.getValue());
        const auto font = monoFont (11.0f);
        const int tw = juce::roundToInt (juce::GlyphArrangement::getStringWidth (font, valueText)) + 16;
        auto bubble = juce::Rectangle<int> (juce::roundToInt (pos) - tw / 2,
                                            juce::roundToInt (thumb.getY()) - 26, tw, 20);
        bubble.setX (juce::jlimit (x, x + width - tw, bubble.getX()));

        g.setColour (theme::cardAlt);
        g.fillRoundedRectangle (bubble.toFloat(), 4.0f);
        g.setColour (theme::border);
        g.drawRoundedRectangle (bubble.toFloat(), 4.0f, 1.0f);
        g.setColour (theme::text);
        g.setFont (font);
        g.drawText (valueText, bubble, juce::Justification::centred);
    }
}

void MasterLensLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                                  const juce::Colour& backgroundColour,
                                                  bool isMouseOver, bool isButtonDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    constexpr float corner = tokens::radiusControl;
    const bool primary = (bool) button.getProperties()["mlPrimary"];
    const bool segment = (bool) button.getProperties()["mlSegment"];

    g.saveState();
    g.setOpacity (button.isEnabled() ? 1.0f : 0.4f);

    if (segment)
    {
        if (button.getToggleState())
        {
            g.setColour (theme::accent);
            g.fillRect (bounds.getX() + 4.0f, bounds.getBottom() - 2.0f,
                        juce::jmax (12.0f, bounds.getWidth() - 8.0f), 2.0f);
        }
        else
        {
            g.setColour (theme::borderSoft);
            g.fillRect (bounds.getX(), bounds.getBottom() - 1.0f, bounds.getWidth(), 1.0f);
        }

        g.restoreState();
        return;
    }

    if (isButtonDown)
    {
        surface::recessed (g, bounds, corner);
    }
    else if (primary)
    {
        g.setColour (theme::accent.withAlpha (isMouseOver ? 0.20f : 0.14f));
        g.fillRoundedRectangle (bounds, corner);
    }
    else
    {
        auto base = button.getToggleState() ? theme::accentSoft : backgroundColour;

        if (isMouseOver)
            base = base.overlaidWith (juce::Colours::white.withAlpha (0.04f));

        g.setGradientFill (juce::ColourGradient (base.overlaidWith (juce::Colours::white.withAlpha (0.025f)),
                                                 bounds.getX(), bounds.getY(),
                                                 base.overlaidWith (juce::Colours::black.withAlpha (0.025f)),
                                                 bounds.getX(), bounds.getBottom(), false));
        g.fillRoundedRectangle (bounds, corner);

        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.drawHorizontalLine (juce::roundToInt (bounds.getY() + 0.5f),
                              bounds.getX() + corner, bounds.getRight() - corner);
    }

    if (button.hasKeyboardFocus (true))
    {
        g.setColour (theme::accent);
        g.drawRoundedRectangle (bounds.reduced (2.0f), corner - 1.0f, 1.0f);
    }

    g.restoreState();
}

void MasterLensLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button,
                                            bool isMouseOver, bool isButtonDown)
{
    juce::ignoreUnused (isMouseOver, isButtonDown);

    const bool primary = (bool) button.getProperties()["mlPrimary"];
    const bool segment = (bool) button.getProperties()["mlSegment"];
    auto colour = (primary || (segment && button.getToggleState())) ? theme::accent
                          : (button.getToggleState() ? theme::accent : theme::text);
    colour = colour.withMultipliedAlpha (button.isEnabled() ? 1.0f : 0.4f);

    g.setColour (colour);
    g.setFont (uiFont (tokens::label, primary));
    g.drawText (button.getButtonText(), button.getLocalBounds().reduced (6, 0),
                juce::Justification::centred, true);
}


void MasterLensLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                              bool isMouseOver, bool isButtonDown)
{
    juce::ignoreUnused (isButtonDown);

    auto area = button.getLocalBounds();
    auto track = area.removeFromLeft (28).withSizeKeepingCentre (28, 16).toFloat();

    g.setColour (button.getToggleState() ? theme::accent : theme::borderSoft);
    g.fillRoundedRectangle (track, 8.0f);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.drawRoundedRectangle (track, 8.0f, 1.0f);

    const float knobX = button.getToggleState() ? track.getRight() - 14.0f : track.getX() + 2.0f;
    g.setColour (button.getToggleState() ? theme::text : theme::textDim);
    g.fillEllipse (knobX, track.getY() + 2.0f, 12.0f, 12.0f);

    if (isMouseOver)
    {
        g.setColour (theme::text.withAlpha (0.16f));
        g.drawRoundedRectangle (track.expanded (1.0f), 9.0f, 1.0f);
    }

    if (button.hasKeyboardFocus (true))
    {
        g.setColour (theme::accent);
        g.drawRoundedRectangle (track.expanded (2.0f), 10.0f, 1.0f);
    }

    g.setColour (theme::textDim);
    g.setFont (uiFont (tokens::label));
    g.drawText (button.getButtonText(), area.withTrimmedLeft (8), juce::Justification::centredLeft, true);
}

void MasterLensLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool isButtonDown,
                                          int buttonX, int buttonY, int buttonW, int buttonH,
                                          juce::ComboBox& box)
{
    juce::ignoreUnused (buttonX, buttonY, buttonW, buttonH);

    const auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height);

    g.setColour (theme::card);
    g.fillRoundedRectangle (bounds, tokens::radiusControl);
    g.setColour (theme::borderSoft);
    g.fillRect (bounds.getX(), bounds.getBottom() - 1.0f, bounds.getWidth(), 1.0f);

    const float cx = bounds.getRight() - 14.0f;
    const float cy = bounds.getCentreY();

    juce::Path arrow;
    arrow.startNewSubPath (cx - 4.0f, cy - 2.0f);
    arrow.lineTo (cx, cy + 2.5f);
    arrow.lineTo (cx + 4.0f, cy - 2.0f);

    g.setColour (box.hasKeyboardFocus (true) ? theme::accent : theme::textDim);
    g.strokePath (arrow, juce::PathStrokeType (1.5f, juce::PathStrokeType::mitered,
                                               juce::PathStrokeType::rounded));

    if (box.hasKeyboardFocus (true))
    {
        g.setColour (theme::accent);
        g.drawRoundedRectangle (bounds.reduced (0.5f), tokens::radiusControl, 1.0f);
    }
}

void MasterLensLookAndFeel::drawLabel (juce::Graphics& g, juce::Label& label)
{
    // Sliders use their text box label as the numeric input: draw it as a real
    // 32 px control with a monospaced, right aligned reading.
    if (dynamic_cast<juce::Slider*> (label.getParentComponent()) != nullptr)
    {
        auto r = label.getLocalBounds().toFloat();

        // Numeric fields are bottom-rule only: no box, no fill.
        g.setColour (label.hasKeyboardFocus (true) ? theme::accent : theme::borderSoft);
        g.fillRect (r.getX(), r.getBottom() - 1.0f, r.getWidth(), 1.0f);

        if (label.isBeingEdited())
            return; // the TextEditor paints itself

        g.setColour (theme::text);
        g.setFont (monoFont (12.0f, true));
        g.drawText (label.getText(), label.getLocalBounds().reduced (2, 0),
                    juce::Justification::centredRight, false);
        return;
    }

    LookAndFeel_V4::drawLabel (g, label);
}

void MasterLensLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text,
                                         int width, int height)
{
    const auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.5f);

    g.setColour (theme::card);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (theme::border);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);

    g.setColour (theme::textDim);
    g.setFont (uiFont (tokens::unit * ui::scale));
    g.drawFittedText (text, 8, 0, width - 16, height, juce::Justification::centred,
                      juce::jmax (2, juce::roundToInt (3.0f * ui::scale)), 1.0f);
}

juce::Rectangle<int> MasterLensLookAndFeel::getTooltipBounds (const juce::String& tipText,
                                                              juce::Point<int> screenPos,
                                                              juce::Rectangle<int> parentArea)
{
    const auto font = uiFont (tokens::aux * ui::scale);
    const int maxW = juce::roundToInt (360.0f * ui::scale);
    const int pad = juce::roundToInt (16.0f * ui::scale);

    juce::AttributedString s;
    s.setJustification (juce::Justification::centred);
    s.append (tipText, font, theme::text);

    juce::TextLayout layout;
    layout.createLayout (s, (float) maxW);

    int w = juce::jlimit (80, maxW, juce::roundToInt (layout.getWidth()) + pad);
    int h = juce::jmax (juce::roundToInt (22.0f * ui::scale),
                        juce::roundToInt (layout.getHeight()) + juce::roundToInt (8.0f * ui::scale));

    return juce::Rectangle<int> (screenPos.x > parentArea.getCentreX() ? screenPos.x - w - 12
                                                                      : screenPos.x + 24,
                                 screenPos.y > parentArea.getCentreY() ? screenPos.y - h - 6
                                                                      : screenPos.y + 18,
                                 w, h)
             .constrainedWithin (parentArea);
}

juce::Font MasterLensLookAndFeel::getComboBoxFont (juce::ComboBox&) { return uiFont (tokens::label); }

juce::Font MasterLensLookAndFeel::getLabelFont (juce::Label& label)
{
    if (dynamic_cast<juce::Slider*> (label.getParentComponent()) != nullptr)
        return monoFont (13.0f);

    return uiFont (tokens::aux);
}

/** Popup menus live in their own desktop window and are not affected by the
    editor transform, so their type is scaled here instead. */
juce::Font MasterLensLookAndFeel::getPopupMenuFont() { return uiFont (tokens::label * ui::scale); }
juce::Font MasterLensLookAndFeel::getTextButtonFont (juce::TextButton&, int) { return uiFont (tokens::label); }

void MasterLensLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    g.fillAll (juce::Colours::transparentBlack);
    g.setColour (theme::cardAlt);
    g.fillRoundedRectangle (0.0f, 0.0f, (float) width, (float) height, tokens::radiusCard);
    g.setColour (theme::border);
    g.drawRoundedRectangle (0.5f, 0.5f, (float) width - 1.0f, (float) height - 1.0f,
                            tokens::radiusCard, 1.0f);
}

void MasterLensLookAndFeel::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area,
                                               bool isSeparator, bool isActive, bool isHighlighted,
                                               bool isTicked, bool hasSubMenu, const juce::String& text,
                                               const juce::String& shortcutKeyText,
                                               const juce::Drawable* icon, const juce::Colour* textColour)
{
    juce::ignoreUnused (hasSubMenu, icon);

    if (isSeparator)
    {
        surface::hairlineH (g, (float) area.getX() + 10.0f, (float) area.getRight() - 10.0f,
                            (float) area.getCentreY());
        return;
    }

    if (isHighlighted)
    {
        g.setColour (theme::raised);
        g.fillRect (area.reduced (4, 1));
    }

    if (isTicked)
    {
        g.setColour (theme::accent);
        g.fillRect (area.getX() + 4, area.getY() + 5, 2, area.getHeight() - 10);
    }

    g.setColour (isActive ? theme::text : theme::textFaint);
    g.setFont (uiFont (tokens::label * ui::scale));
    g.drawText (text, area.reduced (14, 0), juce::Justification::centredLeft, true);

    if (shortcutKeyText.isNotEmpty())
    {
        g.setColour (theme::textFaint);
        g.setFont (monoFont (tokens::unit * ui::scale));
        g.drawText (shortcutKeyText, area.reduced (14, 0), juce::Justification::centredRight, true);
    }
}

void MasterLensLookAndFeel::drawScrollbar (juce::Graphics& g, juce::ScrollBar&, int x, int y,
                                           int width, int height, bool isScrollbarVertical,
                                           int thumbStartPosition, int thumbSize,
                                           bool isMouseOver, bool isMouseDown)
{
    juce::ignoreUnused (isScrollbarVertical);

    const float alpha = isMouseDown ? 0.85f : (isMouseOver ? 0.6f : 0.35f);
    juce::Rectangle<float> thumb;

    if (isScrollbarVertical)
        thumb = { (float) x + (float) width * 0.5f - 1.5f, (float) thumbStartPosition,
                  3.0f, (float) thumbSize };
    else
        thumb = { (float) thumbStartPosition, (float) y + (float) height * 0.5f - 1.5f,
                  (float) thumbSize, 3.0f };

    g.setColour (theme::textDim.withAlpha (alpha));
    g.fillRoundedRectangle (thumb, 1.5f);
}

void MasterLensLookAndFeel::drawScrollbarButton (juce::Graphics&, juce::ScrollBar&, int, int,
                                                 int, bool, bool, bool)
{
    // No arrow buttons: the 3 px thumb is the whole scrollbar.
}

void MasterLensLookAndFeel::fillTextEditorBackground (juce::Graphics& g, int width, int height,
                                                      juce::TextEditor&)
{
    g.setColour (theme::card);
    g.fillRect (0, 0, width, height);
}

void MasterLensLookAndFeel::drawTextEditorOutline (juce::Graphics& g, int width, int height,
                                                    juce::TextEditor& editor)
{
    g.setColour (editor.hasKeyboardFocus (true) ? theme::accent : theme::borderSoft);
    g.fillRect (0, height - 1, width, 1);
}
} // namespace ml
