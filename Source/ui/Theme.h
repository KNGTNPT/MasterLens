#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>

#include "Advisor.h"

namespace ml
{
/** Design tokens shared by every module.

    Base sizes shared by the graphite instrument surface. Fonts are supplied by the three semantic accessors below.
*/
namespace tokens
{
inline constexpr float display = 20.0f;   // main readout
inline constexpr float value   = 13.0f;   // secondary readings
inline constexpr float section = 11.0f;   // page / group title
inline constexpr float label   = 11.0f;   // control label
inline constexpr float aux     = 11.0f;   // secondary copy
inline constexpr float unit    = 11.0f;   // units, axis labels, time codes

inline constexpr float radiusCard    = 6.0f;
inline constexpr float radiusControl = 5.0f;

inline constexpr int rowHeight     = 32;
inline constexpr int controlHeight = 28;
inline constexpr int cardHeight    = 76;   // legacy name; now the readout band
inline constexpr int gap           = 8;
inline constexpr int groupGap      = 24;
inline constexpr int margin        = 16;
inline constexpr int spectrumAnchor = 32;
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
inline juce::Colour background { 0xff171a1b };
inline juce::Colour card       { 0xff202425 };
inline juce::Colour cardAlt    { 0xff2a2f30 };
inline juce::Colour raised     { 0xff2a2f30 };
inline juce::Colour border     { 0xff3b4242 };
inline juce::Colour borderSoft { 0xff303637 };
inline juce::Colour divider    { 0xff303637 };
inline juce::Colour text       { 0xff969c98 };
inline juce::Colour textDim    { 0xff909994 };
inline juce::Colour textFaint  { 0xff858f89 };
inline juce::Colour accent     { 0xff819e90 };
inline juce::Colour accentSoft { 0xff30463e };
inline juce::Colour cyan       { 0xff80989f };
inline juce::Colour good       { 0xff819e90 };
inline juce::Colour info       { 0xff819e90 };
inline juce::Colour warn       { 0xffb19870 };
inline juce::Colour bad        { 0xffc08278 };
inline juce::Colour violet     { 0xff94949c };

inline bool dark { true };
inline int revisionCounter { 0 };

inline int revision() noexcept { return revisionCounter; }

/** Applies the dark (true) or light palette and bumps the revision counter. */
void apply (bool useDark);
} // namespace theme

/** Instrument surfaces: exactly three materials.

    Flat     data / curves / type - no texture, no gradient.
    Raised   panels and buttons - broad shallow light, a fine top highlight,
             and three short contact-shadow strips. No live blur.
    Recessed tracks, fields and table bodies - top inner shadow, bottom highlight.

    Decoration is limited to hairlines, 3 px chart corner ticks and 01/02 group
    numbers. There is no noise, vignette, glass, neon or random dot field.
*/
namespace surface
{
void panel (juce::Graphics&, juce::Rectangle<float> area, bool raised = false);
void graph (juce::Graphics&, juce::Rectangle<float> area);
void raised (juce::Graphics&, juce::Rectangle<float> area, float radius = tokens::radiusControl);
void recessed (juce::Graphics&, juce::Rectangle<float> area, float radius = tokens::radiusControl);
void hairlineH (juce::Graphics&, float x1, float x2, float y, juce::Colour = theme::divider);
void hairlineV (juce::Graphics&, float x, float y1, float y2, juce::Colour = theme::divider);
void cornerTicks (juce::Graphics&, juce::Rectangle<float> area, juce::Colour = theme::border);
void glow (juce::Graphics&, juce::Rectangle<float> area, juce::Colour, float radius = tokens::radiusControl);
} // namespace surface

juce::String typefaceName();
juce::Font getLabelFont();
juce::Font getValueFont();
juce::Font getCriticalFont();
const juce::GlyphArrangement& getMasterWordmark();
const juce::GlyphArrangement& getBrandByline (bool english);
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
/** Font callbacks are shared independently of control painting. */
class InstrumentTypography : public juce::LookAndFeel_V4
{
public:
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getLabelFont (juce::Label&) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override;
    juce::Slider::SliderLayout getSliderLayout (juce::Slider&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
};

/** Only the two instrument control painters are customised. */
class MasterLensLookAndFeel : public InstrumentTypography
{
public:
    MasterLensLookAndFeel();
    void refreshColours();
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float position, float startAngle, float endAngle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&,
                               bool isMouseOver, bool isButtonDown) override;
private:
    juce::Path rotaryBackground, rotaryValue, rotaryHighlight;
};

using LightLookAndFeel = MasterLensLookAndFeel;
} // namespace ml
