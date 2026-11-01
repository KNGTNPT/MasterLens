#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>

#include "Advisor.h"

namespace ml
{
/** Design tokens shared by every module.

    Sizes follow the 8 pt grid; there are exactly two corner radii and six text
    sizes. uiFont() applies the JUCE line-height compensation so the rendered
    glyph height matches the token.
*/
namespace tokens
{
inline constexpr float display = 32.0f;   // hero reading (true peak page)
inline constexpr float value   = 28.0f;   // metric card reading
inline constexpr float section = 14.0f;   // section title
inline constexpr float label   = 13.0f;   // control label
inline constexpr float aux     = 12.0f;   // secondary copy
inline constexpr float unit    = 11.0f;   // units, axis labels, time codes

inline constexpr float radiusCard    = 9.0f;
inline constexpr float radiusControl = 5.0f;

// 2.0 density pass: the old 100 % layout had generous 40 / 104 px rows and
// 12 px gaps around 10-11 px type. At native scale that wasted vertical space
// and made the interface feel spread out even though the glyphs were small.
inline constexpr int rowHeight     = 34;
inline constexpr int controlHeight = 30;
inline constexpr int cardHeight    = 96;
inline constexpr int gap           = 8;
inline constexpr int margin        = 12;
} // namespace tokens

/** Runtime interface zoom (1.0 - 2.0).

    The editor lays out in base points and applies one AffineTransform to every
    child, so changing the scale enlarges glyphs, row heights, cards and the
    window together. Nothing re-flows, so no label can be clipped by a text size
    change; the whole surface simply becomes bigger.
*/
namespace ui
{
inline constexpr float minScale = 1.0f;
inline constexpr float maxScale = 2.0f;

inline constexpr std::array<float, 5> scaleSteps { 1.0f, 1.25f, 1.5f, 1.75f, 2.0f };

inline float scale = 1.0f;

inline void setScale (float s) noexcept
{
    scale = juce::jlimit (minScale, maxScale, s);
}

inline int scaled (int v) noexcept { return juce::roundToInt ((float) v * scale); }
inline float scaled (float v) noexcept { return v * scale; }

/** Nearest step index for a stored scale factor. */
inline int stepIndexFor (float s) noexcept
{
    int best = 0;

    for (int i = 0; i < (int) scaleSteps.size(); ++i)
        if (std::abs (scaleSteps[(size_t) i] - s) < std::abs (scaleSteps[(size_t) best] - s))
            best = i;

    return best;
}

inline juce::String scaleText (float s) { return juce::String (juce::roundToInt (s * 100.0f)) + "%"; }
} // namespace ui

/** Runtime palette.

    The colours are mutable inline variables: switching theme reassigns them in
    one place and every paint routine picks the new values up on its next frame.
    Anything that caches an offscreen image must compare theme::revision()
    before reusing it.
*/
namespace theme
{
inline juce::Colour background { 0xff0b0e13 };
inline juce::Colour card       { 0xff141922 };
inline juce::Colour cardAlt    { 0xff1b222d };
inline juce::Colour raised     { 0xff1b222d };
inline juce::Colour border     { 0xff232b38 };
inline juce::Colour borderSoft { 0xff1e2632 };
inline juce::Colour divider    { 0xff1e2632 };
inline juce::Colour text       { 0xffe6eaf2 };
inline juce::Colour textDim    { 0xff9aa6b8 };
inline juce::Colour textFaint  { 0xff7e8a9c };
inline juce::Colour accent     { 0xff4c8dff };
inline juce::Colour accentSoft { 0xff1e2c46 };
inline juce::Colour cyan       { 0xff46c8d8 };
inline juce::Colour good       { 0xff35c48a };
inline juce::Colour info       { 0xff4c8dff };
inline juce::Colour warn       { 0xfff5a623 };
inline juce::Colour bad        { 0xffff5c5c };
inline juce::Colour violet     { 0xff8b7bff };

inline bool dark { true };
inline int revisionCounter { 0 };

inline int revision() noexcept { return revisionCounter; }

/** Applies the dark (true) or light palette and bumps the revision counter. */
void apply (bool useDark);
} // namespace theme

juce::String typefaceName();
juce::Font uiFont (float height, bool bold = false);
juce::Font monoFont (float height, bool bold = false);
juce::Colour severityColour (Severity s);

//==============================================================================
// Number / text formatting. Every reading goes through these so the decimals,
// the U+2212 minus and the en dash stay consistent across modules.
//==============================================================================

/** U+2212 minus sign (same advance width as the plus sign). */
inline juce::String minusSign() { return juce::String::charToString (0x2212); }

/** U+2013 en dash: the only character allowed between range bounds. */
inline juce::String enDash() { return juce::String::charToString (0x2013); }

/** U+2014 em dash: placeholder for "no reading yet". */
inline juce::String noValue() { return juce::String::charToString (0x2014); }

/** U+00B7 middle dot, used as the inline separator. */
inline juce::String midDot() { return juce::String::charToString (0x00B7); }

/** U+00D7 multiplication sign (4x oversampling). */
inline juce::String times() { return juce::String::charToString (0x00D7); }

/** U+00B1 plus-minus sign. */
/** ASCII "+/-": U+00B1 renders as "Â±" in some font/codepage combinations. */
inline juce::String plusMinus() { return juce::String ("+/-"); }

/** Inline separator. ASCII only: some UI fonts in the wild map U+00B7 through a
    Latin-1 codepage and render it as "Â·". */
inline juce::String sep() { return "  |  "; }

/** Formats a value with fixed decimals and a typographic minus. */
inline juce::String num (float v, int decimals = 1)
{
    auto s = juce::String ((double) v, decimals);

    if (s.startsWithChar ('-'))
        s = minusSign() + s.substring (1);

    return s;
}

/** Formats a signed delta: "+2.3" / "−2.3". */
inline juce::String sgn (float v, int decimals = 1)
{
    const auto s = num (v, decimals);
    return v > -1.0e-6f ? "+" + s : s;
}

/** "5 – 12" with an en dash. */
inline juce::String fmtRange (float a, float b, int decimals = 1)
{
    return num (a, decimals) + " " + enDash() + " " + num (b, decimals);
}

/** Value at or below -99 means "no measurement", never a real reading. */
inline juce::String fmt1 (float v) { return v <= -99.0f ? noValue() : num (v, 1); }
inline juce::String fmt2 (float v) { return v <= -99.0f ? noValue() : num (v, 2); }
inline juce::String fmtDb (float v) { return v <= -99.0f ? noValue() : num (v, 1); }

inline juce::String fmtTime (double seconds)
{
    const int total = juce::jmax (0, (int) std::floor (seconds));
    const int mins = total / 60;
    const int secs = total % 60;
    return juce::String (mins).paddedLeft ('0', 2) + ":" + juce::String (secs).paddedLeft ('0', 2);
}

//==============================================================================
/** Dark professional LookAndFeel; the light variant is driven by theme::apply. */
class MasterLensLookAndFeel : public juce::LookAndFeel_V4
{
public:
    MasterLensLookAndFeel();

    /** Re-reads the active palette (call after theme::apply). */
    void refreshColours();

    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&,
                               bool isMouseOver, bool isButtonDown) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH,
                       juce::ComboBox&) override;

    void drawLabel (juce::Graphics&, juce::Label&) override;
    void drawTooltip (juce::Graphics&, const juce::String&, int width, int height) override;
    juce::Rectangle<int> getTooltipBounds (const juce::String&, juce::Point<int> screenPos,
                                           juce::Rectangle<int> parentArea) override;

    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getLabelFont (juce::Label&) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;
};

/** Legacy name kept so older call sites still compile. */
using LightLookAndFeel = MasterLensLookAndFeel;
} // namespace ml
