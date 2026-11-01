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
        theme::background = juce::Colour (0xff141517);
        theme::card       = juce::Colour (0xff1a1b1e);
        theme::cardAlt    = juce::Colour (0xff202226);
        theme::raised     = juce::Colour (0xff202226);
        theme::border     = juce::Colour (0x12ffffff);
        theme::borderSoft = juce::Colour (0x0affffff);
        theme::divider    = juce::Colour (0x12ffffff);
        theme::text       = juce::Colour (0xffe9e9e7);
        theme::textDim    = juce::Colour (0xff9b9da2);
        theme::textFaint  = juce::Colour (0xff6c6f75);
        theme::accent     = juce::Colour (0xffc2703c);
        theme::accentHover= juce::Colour (0xffd07c45);
        theme::accentSoft = juce::Colour (0x24c2703c);
        theme::cyan       = juce::Colour (0xffc2703c);
        theme::good       = juce::Colour (0xff6fa36f);
        theme::info       = juce::Colour (0xffc2703c);
        theme::warn       = juce::Colour (0xffc99a3f);
        theme::bad        = juce::Colour (0xffc25b5b);
        theme::violet     = juce::Colour (0xff8a5a3d);
    }
    else
    {
        theme::background = juce::Colour (0xffefebe5);
        theme::card       = juce::Colour (0xfff8f5f0);
        theme::cardAlt    = juce::Colour (0xffe9e3da);
        theme::raised     = juce::Colour (0xfffffdf9);
        theme::border     = juce::Colour (0x18000000);
        theme::borderSoft = juce::Colour (0x0d000000);
        theme::divider    = juce::Colour (0x18000000);
        theme::text       = juce::Colour (0xff252422);
        theme::textDim    = juce::Colour (0xff6b6862);
        theme::textFaint  = juce::Colour (0xff8d877d);
        theme::accent     = juce::Colour (0xffa9582d);
        theme::accentHover= juce::Colour (0xffbc6537);
        theme::accentSoft = juce::Colour (0x24a9582d);
        theme::cyan       = juce::Colour (0xffa9582d);
        theme::good       = juce::Colour (0xff4f7f50);
        theme::info       = juce::Colour (0xffa9582d);
        theme::warn       = juce::Colour (0xff9a6a1f);
        theme::bad        = juce::Colour (0xffa84444);
        theme::violet     = juce::Colour (0xff7b4e37);
    }

    ++theme::revisionCounter;
}

//==============================================================================
namespace surface
{
static const juce::Image& noiseImage()
{
    static const juce::Image image = []
    {
        juce::Image img (juce::Image::ARGB, 256, 256, true);
        juce::Random rng (0x4d4c4e);
        juce::Image::BitmapData data (img, juce::Image::BitmapData::writeOnly);

        for (int y = 0; y < img.getHeight(); ++y)
            for (int x = 0; x < img.getWidth(); ++x)
            {
                const auto v = (juce::uint8) (rng.nextInt (256));
                data.setPixelColour (x, y, juce::Colour::fromRGB (v, v, v));
            }

        return img;
    }();

    return image;
}

void noise (juce::Graphics& g, juce::Rectangle<float> area)
{
    g.setOpacity (theme::dark ? 0.025f : 0.018f);
    g.drawImage (noiseImage(), area);
    g.setOpacity (1.0f);
}

void vignette (juce::Graphics& g, juce::Rectangle<float> area)
{
    if (area.getWidth() < 8.0f || area.getHeight() < 8.0f)
        return;

    juce::ColourGradient grad (juce::Colour (0x00000000), area.getCentreX(), area.getCentreY(),
                               juce::Colour (0x0f000000), area.getX(), area.getY(), true);
    g.setGradientFill (grad);
    g.fillRect (area);
}

void panel (juce::Graphics& g, juce::Rectangle<float> area, bool raised)
{
    const auto r = area.reduced (0.5f);
    const auto base = raised ? theme::raised : theme::card;

    g.setColour (base);
    g.fillRoundedRectangle (r, tokens::radiusCard);

    // One-pixel inner top highlight. It reads as a soft edge without a border.
    g.setColour (juce::Colours::white.withAlpha (theme::dark ? 0.04f : 0.30f));
    g.drawHorizontalLine (juce::roundToInt (r.getY() + 1.0f),
                          r.getX() + tokens::radiusCard, r.getRight() - tokens::radiusCard);

    if (raised)
        shadow (g, r, tokens::radiusCard);
}

void graph (juce::Graphics& g, juce::Rectangle<float> area)
{
    panel (g, area, false);
    vignette (g, area);
    noise (g, area);
}

void shadow (juce::Graphics& g, juce::Rectangle<float> area, float radius)
{
    for (int i = 3; i >= 1; --i)
    {
        g.setColour (juce::Colours::black.withAlpha (0.035f * (float) (4 - i)));
        g.fillRoundedRectangle (area.expanded ((float) i * 2.0f).translated (0.0f, 2.0f), radius + (float) i);
    }
}
} // namespace surface

//==============================================================================
juce::String typefaceName()
{
    static const juce::String name = []
    {
        const juce::StringArray preferred { "PingFang SC", "Hiragino Sans GB", "Heiti SC",
                                            "Songti SC", "Helvetica Neue" };
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
                         .withFallbacks ({ "PingFang SC", "Hiragino Sans GB", "Arial Unicode MS" }));

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

        for (const auto* candidate : { "SF Mono", "Menlo", "Monaco" })
            if (all.contains (candidate))
                return juce::String (candidate);

        return juce::String ("Menlo");
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
    constexpr float trackH = 6.0f;

    auto track = juce::Rectangle<float> (bounds.getX(), bounds.getCentreY() - trackH * 0.5f,
                                         bounds.getWidth(), trackH);

    g.setColour (theme::borderSoft);
    g.fillRoundedRectangle (track, 3.0f);

    const float pos = juce::jlimit (track.getX(), track.getRight(), sliderPos);
    g.setColour (theme::accent);
    g.fillRoundedRectangle (track.withRight (pos), 3.0f);

    const float r = 7.0f;
    auto thumb = juce::Rectangle<float> (pos - r, bounds.getCentreY() - r, r * 2.0f, r * 2.0f);

    g.setColour (theme::cardAlt);
    g.fillEllipse (thumb);
    g.setColour (slider.hasKeyboardFocus (true) ? theme::text : theme::accent);
    g.drawEllipse (thumb.reduced (1.5f), 2.0f);
}

void MasterLensLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                                  const juce::Colour& backgroundColour,
                                                  bool isMouseOver, bool isButtonDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    constexpr float corner = tokens::radiusControl;

    auto base = button.getToggleState() ? theme::accentSoft : backgroundColour;

    if (isButtonDown)
        base = base.brighter (0.10f);
    else if (isMouseOver)
        base = base.brighter (0.06f);

    g.setColour (base);
    g.fillRoundedRectangle (bounds, corner);

    g.setColour (button.getToggleState() ? theme::accent.withAlpha (0.70f) : theme::border);
    g.drawRoundedRectangle (bounds, corner, 1.0f);

    if (button.hasKeyboardFocus (true))
    {
        g.setColour (theme::accent);
        g.drawRoundedRectangle (bounds.reduced (2.0f), corner - 1.0f, 2.0f);
    }
}


void MasterLensLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool isButtonDown,
                                          int buttonX, int buttonY, int buttonW, int buttonH,
                                          juce::ComboBox& box)
{
    juce::ignoreUnused (buttonX, buttonY, buttonW, buttonH);

    const auto bounds = juce::Rectangle<float> (0.5f, 0.5f, (float) width - 1.0f, (float) height - 1.0f);

    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (bounds, tokens::radiusControl);

    g.setColour (box.hasKeyboardFocus (true) ? theme::accent
                                             : box.findColour (juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle (bounds, tokens::radiusControl, 1.0f);

    const float cx = bounds.getRight() - 16.0f;
    const float cy = bounds.getCentreY();

    juce::Path arrow;
    arrow.startNewSubPath (cx - 4.0f, cy - 2.0f);
    arrow.lineTo (cx, cy + 2.5f);
    arrow.lineTo (cx + 4.0f, cy - 2.0f);

    g.setColour (box.findColour (juce::ComboBox::arrowColourId));
    g.strokePath (arrow, juce::PathStrokeType (1.6f, juce::PathStrokeType::mitered,
                                               juce::PathStrokeType::rounded));

    if (box.hasKeyboardFocus (true))
    {
        g.setColour (theme::accent);
        g.drawRoundedRectangle (bounds.reduced (2.0f), tokens::radiusControl - 1.0f, 2.0f);
    }
}

void MasterLensLookAndFeel::drawLabel (juce::Graphics& g, juce::Label& label)
{
    // Sliders use their text box label as the numeric input: draw it as a real
    // 32 px control with a monospaced, right aligned reading.
    if (dynamic_cast<juce::Slider*> (label.getParentComponent()) != nullptr)
    {
        auto r = label.getLocalBounds().toFloat().reduced (0.5f);

        g.setColour (theme::cardAlt);
        g.fillRoundedRectangle (r, tokens::radiusControl);

        g.setColour (label.hasKeyboardFocus (true) ? theme::accent : theme::border);
        g.drawRoundedRectangle (r, tokens::radiusControl, 1.0f);

        if (label.isBeingEdited())
            return; // the TextEditor paints itself

        g.setColour (theme::text);
        g.setFont (monoFont (13.0f));
        g.drawText (label.getText(), label.getLocalBounds().reduced (8, 0),
                    juce::Justification::centredRight, false);
        return;
    }

    LookAndFeel_V4::drawLabel (g, label);
}

void MasterLensLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text,
                                         int width, int height)
{
    const auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.5f);

    g.setColour (theme::cardAlt);
    g.fillRoundedRectangle (r, tokens::radiusControl);
    g.setColour (theme::border);
    g.drawRoundedRectangle (r, tokens::radiusControl, 1.0f);

    g.setColour (theme::text);
    g.setFont (uiFont (tokens::aux * ui::scale));
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
    g.setColour (theme::card);
    g.fillRoundedRectangle (0.0f, 0.0f, (float) width, (float) height, tokens::radiusCard);
    g.setColour (theme::border);
    g.drawRoundedRectangle (0.5f, 0.5f, (float) width - 1.0f, (float) height - 1.0f,
                            tokens::radiusCard, 1.0f);
}
} // namespace ml
