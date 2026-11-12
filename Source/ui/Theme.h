#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>

#include "Advisor.h"

namespace ml
{
/** Design tokens shared by every module.

    Base sizes shared by the graphite instrument surface. Fonts are supplied by the four semantic accessors below: label, value, title, critical.
*/
namespace tokens
{
inline constexpr float display = 30.0f;   // main readout
inline constexpr float title   = 20.0f;   // page title
inline constexpr float value   = 18.0f;   // secondary readings
inline constexpr float section = 14.0f;   // group title / control label
inline constexpr float label   = 14.0f;   // control label
inline constexpr float aux     = 14.0f;   // secondary copy
inline constexpr float unit    = 14.0f;   // units, axis labels, time codes

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
inline juce::Colour accentInk  { 0xff9fc0b0 };   // accent as text on accentSoft
inline juce::Colour accentSoft { 0xff30463e };
inline juce::Colour cyan       { 0xff80989f };
inline juce::Colour good       { 0xff819e90 };
inline juce::Colour info       { 0xff819e90 };
inline juce::Colour warn       { 0xffb19870 };
inline juce::Colour bad        { 0xffc08278 };
inline juce::Colour violet     { 0xff94949c };
inline juce::Colour track      { 0xff343a3b };   // empty meter track / progress rail

// 3.1: the measurement window is dark in both themes, so plot ink cannot
// follow the page palette. These tokens are fixed across light and dark.
inline juce::Colour well      { 0xff0b0e0f };
inline juce::Colour wellEdge  { 0xff2b3133 };
inline juce::Colour plotInk   { 0xffe7e4d9 };
inline juce::Colour plotFaint { 0xff8d948f };
inline juce::Colour plotGrid  { 0xff232a2b };
inline juce::Colour plotAccent{ 0xff9fbcab };
inline juce::Colour plotWarn  { 0xffd8b48a };
inline juce::Colour plotBad   { 0xffe0a39a };

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
juce::Font getMicroFont();
juce::Font getPageTitleFont();
juce::Font getMetaFont();
juce::Font getMonoFont (float height);
juce::Font getLabelTrackedFont();
juce::Font getValueFont();
juce::Font getTitleFont();
juce::Font getCriticalFont();

/** Label font for a specific run: uppercase Latin keeps the 0.04em tracking,
    anything containing CJK or lowercase body copy drops it. */
juce::Font labelFontFor (const juce::String& text);
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

/** Inline separator for meta / list lines. One style only: `|` and `·` mixed
    on the same screen made the reader re-parse every line. The middle dot is
    built from a code point rather than a literal to stay codepage-safe. */
inline juce::String sep() { return "  " + midDot() + "  "; }

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
/** Baseline-correct text runs.

    JUCE vertically centres a glyph run inside a rectangle using the font's own
    ascent and descent, so a 30 px reading and its 14 px unit placed in the same
    rectangle share a box, not a baseline: the unit floats inside the value's
    line box and drifts as soon as either size changes. Every value / unit pair
    is drawn through these helpers instead, which anchor both runs to one
    baseline measured from the box's optical centre.
*/
namespace text
{
/** Baseline that vertically centres the font's line box in `box`. */
inline float centredBaseline (const juce::Font& font, juce::Rectangle<int> box) noexcept
{
    const float lineHeight = font.getAscent() + font.getDescent();
    return (float) box.getCentreY() - lineHeight * 0.5f + font.getAscent();
}

/** Draws one run on the given baseline; returns its advance width. */
inline int drawRun (juce::Graphics& g, const juce::Font& font, juce::Colour colour,
                    const juce::String& run, int x, float baseline)
{
    g.setColour (colour);
    g.setFont (font);
    g.drawSingleLineText (run, x, juce::roundToInt (baseline));

    return juce::roundToInt (juce::GlyphArrangement::getStringWidth (font, run));
}

/** Draws `value` and its `unit` right-aligned on one baseline.

    Used by table cells where the unit should stay a step smaller than the
    number instead of sharing the value run.
*/
inline int drawValueUnitRight (juce::Graphics& g, juce::Rectangle<int> box,
                               const juce::String& value, const juce::String& unit,
                               const juce::Font& valueFont, const juce::Font& unitFont,
                               juce::Colour valueColour, juce::Colour unitColour,
                               int gap = 6)
{
    const float baseline = centredBaseline (valueFont, box);
    const int valueWidth = juce::roundToInt (juce::GlyphArrangement::getStringWidth (valueFont, value));
    const int unitWidth = unit.isEmpty() ? 0
        : juce::roundToInt (juce::GlyphArrangement::getStringWidth (unitFont, unit));
    const int total = valueWidth + (unit.isEmpty() ? 0 : gap + unitWidth);

    drawRun (g, valueFont, valueColour, value, box.getRight() - total, baseline);

    if (unit.isNotEmpty())
        drawRun (g, unitFont, unitColour, unit, box.getRight() - unitWidth, baseline);

    return total;
}

/** Draws already-split lines at a fixed pitch.

    JUCE's drawFittedText uses one font height as the line box, which is too
    tight for multi-line CJK copy. Paragraph blocks pass an explicit pitch
    (1.4-1.5x the font height) instead.
*/
inline void drawLines (juce::Graphics& g, const juce::Font& font, juce::Colour colour,
                       const juce::StringArray& lines, juce::Rectangle<int> area, int pitch)
{
    g.setColour (colour);
    g.setFont (font);

    auto y = area.getY();

    for (const auto& line : lines)
    {
        if (y + pitch > area.getBottom())
            break;

        g.drawText (line, area.getX(), y, area.getWidth(), pitch,
                    juce::Justification::centredLeft, false);
        y += pitch;
    }
}

/** Draws `value` and `unit` on one shared baseline inside `box`.

    Returns the total advance, so the caller can continue the row (a pill, a
    delta) without re-measuring.
*/
inline int drawValueUnit (juce::Graphics& g, juce::Rectangle<int> box,
                          const juce::String& value, const juce::String& unit,
                          const juce::Font& valueFont, const juce::Font& unitFont,
                          juce::Colour valueColour, juce::Colour unitColour,
                          int gap = 8)
{
    const float baseline = centredBaseline (valueFont, box);
    const int valueWidth = drawRun (g, valueFont, valueColour, value, box.getX(), baseline);

    if (unit.isEmpty())
        return valueWidth;

    const int unitWidth = drawRun (g, unitFont, unitColour, unit,
                                   box.getX() + valueWidth + gap, baseline);

    return valueWidth + gap + unitWidth;
}
} // namespace text

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
class MasterLensLookAndFeel : public InstrumentTypography, private juce::Timer
{
public:
    MasterLensLookAndFeel();
    void refreshColours();
    void bindButtons (juce::Component& root);
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float position, float startAngle, float endAngle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&,
                               bool isMouseOver, bool isButtonDown) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;
private:
    void timerCallback() override;
    struct ButtonMotion { juce::Component::SafePointer<juce::Button> button; float depth = 0.0f, velocity = 0.0f; };
    std::array<ButtonMotion, 128> buttonMotions;
    juce::Path rotaryBackground, rotaryValue, rotaryHighlight;
};

using LightLookAndFeel = MasterLensLookAndFeel;
} // namespace ml
