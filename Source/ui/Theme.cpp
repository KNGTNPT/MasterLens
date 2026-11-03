#include "Theme.h"

#include "MasterLensFonts.h"
#include "Layout.h"
#include "../Version.h"

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
        theme::text       = juce::Colour (0xff969c98);
        theme::textDim    = juce::Colour (0xff909994);
        theme::textFaint  = juce::Colour (0xff858f89);
        theme::accent     = juce::Colour (0xff819e90);
        theme::accentSoft = juce::Colour (0xff30463e);
        theme::cyan       = juce::Colour (0xff80989f);
        theme::good       = juce::Colour (0xff819e90);
        theme::info       = juce::Colour (0xff819e90);
        theme::warn       = juce::Colour (0xffb19870);
        theme::bad        = juce::Colour (0xffc08278);
        theme::violet     = juce::Colour (0xff94949c);
    }
    else
    {
        theme::background = juce::Colour (0xffe5e6df);
        theme::card       = juce::Colour (0xffdce0d9);
        theme::cardAlt    = juce::Colour (0xffe9ece5);
        theme::raised     = juce::Colour (0xffdce0d9);
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
    // Shallow, broad light from above-left. Analytic gradients, no per-frame blur.
    for (int i = 3; i > 0; --i)
    {
        g.setColour (juce::Colours::black.withAlpha ((theme::dark ? 0.045f : 0.025f) * (4 - i)));
        g.fillRoundedRectangle (area.reduced (0.6f).translated (0.0f, (float) i), radius);
    }
    g.setGradientFill (juce::ColourGradient (lift (theme::raised, theme::dark ? 0.055f : 0.30f),
                                             area.getX(), area.getY(),
                                             sink (theme::raised, theme::dark ? 0.065f : 0.045f),
                                             area.getRight() * 0.35f + area.getX() * 0.65f,
                                             area.getBottom(), false));
    g.fillRoundedRectangle (area, radius);

    g.setColour (juce::Colours::white.withAlpha (theme::dark ? 0.13f : 0.70f));
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
    g.setGradientFill (juce::ColourGradient (sink (theme::card, theme::dark ? 0.22f : 0.055f),
                                             area.getX(), area.getY(), theme::card,
                                             area.getX(), area.getY() + juce::jmin (22.0f, area.getHeight()), false));
    g.fillRoundedRectangle (area, radius);

    g.setColour (juce::Colours::black.withAlpha (theme::dark ? 0.34f : 0.10f));
    g.drawHorizontalLine (juce::roundToInt (area.getY() + 0.5f),
                          area.getX() + radius, area.getRight() - radius);

    g.setColour (juce::Colours::white.withAlpha (theme::dark ? 0.08f : 0.65f));
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
    // A state boundary needs one line; concentric halos add no information.
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
        g.setGradientFill (juce::ColourGradient (lift (theme::card, theme::dark ? 0.023f : 0.0f),
                                                 r.getX(), r.getY(), theme::card,
                                                 r.getX() + r.getWidth() * 0.30f,
                                                 r.getY() + juce::jmin (160.0f, r.getHeight()), false));
        g.fillRoundedRectangle (r, tokens::radiusCard);

        // A 1 px inner top highlight keeps the panel from reading as a hole.
        g.setColour (juce::Colours::white.withAlpha (theme::dark ? 0.085f : 0.65f));
        g.drawHorizontalLine (juce::roundToInt (r.getY() + 0.5f),
                              r.getX() + tokens::radiusCard, r.getRight() - tokens::radiusCard);
    }
}

void graph (juce::Graphics& g, juce::Rectangle<float> area)
{
    panel (g, area, false);
    // Plot boundaries and scales already identify the measurement window.
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
namespace
{
    class InstrumentFonts final : public juce::DeletedAtShutdown
    {
    public:
        static InstrumentFonts& get()
        {
            if (instance == nullptr) instance = new InstrumentFonts();
            return *instance;
        }
        ~InstrumentFonts() override { instance = nullptr; }

        juce::Typeface::Ptr regular = juce::Typeface::createSystemTypefaceFor (
            MasterLensFonts::GeistRegular_otf, MasterLensFonts::GeistRegular_otfSize);
        juce::Typeface::Ptr medium = juce::Typeface::createSystemTypefaceFor (
            MasterLensFonts::GeistMedium_otf, MasterLensFonts::GeistMedium_otfSize);
        juce::Typeface::Ptr bold = juce::Typeface::createSystemTypefaceFor (
            MasterLensFonts::GeistBold_otf, MasterLensFonts::GeistBold_otfSize);

        juce::Typeface::Ptr brandMono = juce::Typeface::createSystemTypefaceFor (
            MasterLensFonts::GeistMonoRegular_otf, MasterLensFonts::GeistMonoRegular_otfSize);

        juce::Font label { options (regular, 14.0f).withKerningFactor (0.04f) };
        juce::Font value { options (medium, 16.0f).withFeatureEnabled ("tnum") };
        juce::Font critical { options (bold, 26.0f).withFeatureEnabled ("tnum") };
        juce::GlyphArrangement masterWordmark, brandBylineZh, brandBylineEn;

        InstrumentFonts()
        {
            // User-requested 130% brand geometry; measurement font roles stay unchanged.
            masterWordmark.addLineOfText (value.withExtraKerningFactor (0.208f),
                                         "MASTERLENS", 0.0f, 0.0f);
            const juce::Font byline { options (brandMono, 12.0f).withKerningFactor (0.12f) };
            brandBylineZh.addLineOfText (byline, juce::String ("BERRYSTUDIO / ")
                + kVersion, 0.0f, 0.0f);
            brandBylineEn.addLineOfText (byline, juce::String ("BERRYSTUDIO / ")
                + kVersion, 0.0f, 0.0f);
        }

    private:
        static juce::FontOptions options (const juce::Typeface::Ptr& face, float height)
        {
            return juce::FontOptions (face).withHeight (height)
                .withFallbacks ({ "PingFang SC", "Microsoft YaHei UI", "Noto Sans CJK SC" });
        }
        static inline InstrumentFonts* instance = nullptr;
    };
}

juce::String typefaceName() { return InstrumentFonts::get().regular->getName(); }
juce::Font getLabelFont() { return InstrumentFonts::get().label; }
juce::Font getValueFont() { return InstrumentFonts::get().value; }
juce::Font getCriticalFont() { return InstrumentFonts::get().critical; }
const juce::GlyphArrangement& getMasterWordmark() { return InstrumentFonts::get().masterWordmark; }
const juce::GlyphArrangement& getBrandByline (bool english)
{
    return english ? InstrumentFonts::get().brandBylineEn : InstrumentFonts::get().brandBylineZh;
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
    setDefaultSansSerifTypeface (InstrumentFonts::get().regular);
    rotaryBackground.preallocateSpace (2048);
    rotaryValue.preallocateSpace (2048);
    rotaryHighlight.preallocateSpace (2048);
    refreshColours();
    startTimerHz (90);
}

void MasterLensLookAndFeel::bindButtons (juce::Component& root)
{
    if (auto* button = dynamic_cast<juce::Button*> (&root))
    {
        bool found = false;
        for (const auto& motion : buttonMotions) if (motion.button.getComponent() == button) found = true;
        if (! found) for (auto& motion : buttonMotions) if (motion.button == nullptr)
        { motion.button = button; break; }
    }
    for (auto* child : root.getChildren()) bindButtons (*child);
}

void MasterLensLookAndFeel::timerCallback()
{
    for (auto& motion : buttonMotions)
        if (auto* button = motion.button.getComponent())
        {
            if (! button->isShowing()) continue;
            const float target = button->isDown() ? 1.0f : 0.0f;
            if (std::abs (motion.depth - target) < 0.001f && std::abs (motion.velocity) < 0.001f) continue;
            // Small damped rebound, entirely inside the existing hit area.
            motion.velocity = (motion.velocity + (target - motion.depth) * 0.24f) * 0.60f;
            motion.depth += motion.velocity;
            button->repaint();
        }
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
    setColour (juce::TextButton::buttonOnColourId, theme::raised);
    setColour (juce::TextButton::textColourOffId, theme::text);
    setColour (juce::TextButton::textColourOnId, theme::text);

    setColour (juce::ComboBox::backgroundColourId, theme::cardAlt);
    setColour (juce::ComboBox::textColourId, theme::text);
    setColour (juce::ComboBox::outlineColourId, theme::border);
    setColour (juce::ComboBox::arrowColourId, theme::textDim);

    setColour (juce::PopupMenu::backgroundColourId, theme::card);
    setColour (juce::PopupMenu::textColourId, theme::text);
    setColour (juce::PopupMenu::headerTextColourId, theme::textDim);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, theme::raised);
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

juce::Font InstrumentTypography::getComboBoxFont (juce::ComboBox&) { return ml::getValueFont(); }
juce::Font InstrumentTypography::getLabelFont (juce::Label& label)
{
    return dynamic_cast<juce::Slider*> (label.getParentComponent()) != nullptr
        ? ml::getValueFont() : label.getFont();
}
juce::Font InstrumentTypography::getPopupMenuFont() { return ml::getValueFont().withHeight (16.0f * ui::scale); }
juce::Font InstrumentTypography::getTextButtonFont (juce::TextButton&, int) { return ml::getLabelFont(); }

juce::Slider::SliderLayout InstrumentTypography::getSliderLayout (juce::Slider& slider)
{
    auto result = juce::LookAndFeel_V4::getSliderLayout (slider);
    result.sliderBounds = grid::bounds (result.sliderBounds);
    result.textBoxBounds = grid::bounds (result.textBoxBounds);
    for (auto* c : slider.getChildren())
        if (auto* label = dynamic_cast<juce::Label*> (c))
        {
            label->setFont (ml::getValueFont());
            // Native slider labels can retain colours captured before our LNF was attached.
            label->setColour (juce::Label::textColourId, theme::text);
            label->setColour (juce::Label::textWhenEditingColourId, theme::text);
            label->setColour (juce::Label::backgroundColourId, theme::cardAlt);
            label->setColour (juce::Label::outlineColourId, theme::border);
            label->setColour (juce::Label::outlineWhenEditingColourId, theme::accent);
            label->setMinimumHorizontalScale (1.0f);
            label->getProperties().set ("mlGrid", 4);
        }
    return result;
}

void InstrumentTypography::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    auto area = box.getLocalBounds();
    area.removeFromLeft (4);
    area.removeFromRight (24);
    label.setBounds (grid::bounds (area));
    label.setFont (ml::getValueFont());
    label.setMinimumHorizontalScale (1.0f);
    label.getProperties().set ("mlGrid", 4);
}

void MasterLensLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
    float position, float startAngle, float endAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (4.0f);
    const auto centre = bounds.getCentre();
    const float radius = juce::jmax (1.0f, juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f);
    const float bodyRadius = juce::jmax (1.0f, radius - 5.0f);
    const float angle = startAngle + juce::jlimit (0.0f, 1.0f, position) * (endAngle - startAngle);
    const bool down = slider.isMouseButtonDown();
    g.saveState();
    g.setOpacity (slider.isEnabled() ? 1.0f : 0.4f);
    rotaryBackground.clear();
    rotaryBackground.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, startAngle, endAngle, true);
    g.setColour (juce::Colours::darkgrey.withAlpha (0.5f));
    g.strokePath (rotaryBackground, juce::PathStrokeType (2.0f));
    rotaryValue.clear();
    rotaryValue.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, startAngle, angle, true);
    g.setColour (theme::accent.withAlpha (0.6f));
    g.strokePath (rotaryValue, juce::PathStrokeType (2.0f));
    const auto body = juce::Rectangle<float> (bodyRadius * 2, bodyRadius * 2).withCentre (centre);
    g.setColour (down ? theme::raised.darker (0.15f) : theme::raised);
    g.fillEllipse (body);
    if (! down && slider.isMouseOver())
    {
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.fillEllipse (body);
    }
    rotaryHighlight.clear();
    rotaryHighlight.addCentredArc (centre.x, centre.y, bodyRadius, bodyRadius, 0.0f,
                                  -juce::MathConstants<float>::halfPi, 0.35f, true);
    g.setColour (juce::Colours::white.withAlpha (0.16f));
    g.strokePath (rotaryHighlight, juce::PathStrokeType (1.0f));
    const auto pointer = centre + juce::Point<float> (std::sin (angle), -std::cos (angle)) * (bodyRadius - 3.0f);
    g.setColour (theme::textDim);
    g.drawLine ({ centre, pointer }, 1.0f);
    if (slider.hasKeyboardFocus (true))
    {
        g.setColour (theme::accent);
        g.drawEllipse (body.reduced (2.0f), 1.0f);
    }
    g.restoreState();
}

void MasterLensLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
    const juce::Colour&, bool hover, bool down)
{
    float depth = down ? 1.0f : 0.0f;
    for (const auto& motion : buttonMotions)
        if (motion.button.getComponent() == &button) { depth = motion.depth; break; }
    auto bounds = button.getLocalBounds().toFloat().reduced (0.5f)
        .reduced (juce::jmax (0.0f, depth) * 0.5f).translated (0.0f, depth * 2.0f);
    auto base = button.findColour (juce::TextButton::buttonColourId);
    if (button.getToggleState()) base = base.brighter (0.07f);
    if (down) base = base.darker (0.18f);
    g.saveState();
    g.setOpacity (button.isEnabled() ? 1.0f : 0.4f);
    g.setColour (base);
    g.fillRoundedRectangle (bounds, tokens::radiusControl);
    if (hover && ! down)
    {
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.fillRoundedRectangle (bounds, tokens::radiusControl);
    }
    // The release ripple stays clipped to the button and never moves the hit area.
    const float pulse = juce::jlimit (0.0f, 1.0f, std::abs (depth));
    if (pulse > 0.01f)
    {
        g.setColour (theme::accent.withAlpha (pulse * 0.22f));
        g.drawRoundedRectangle (bounds.reduced (2.0f + (1.0f - pulse) * 5.0f), tokens::radiusControl, 1.0f);
    }
    // A top-left glint and lower contact shade explain the press depth.
    g.setColour (juce::Colours::white.withAlpha (0.06f + 0.04f * juce::jmax (0.0f, depth)));
    g.drawHorizontalLine (juce::roundToInt (bounds.getY() + 1), bounds.getX() + 5, bounds.getRight() - 5);
    g.setColour (juce::Colours::black.withAlpha (0.18f));
    g.drawHorizontalLine (juce::roundToInt (bounds.getBottom() - 1), bounds.getX() + 5, bounds.getRight() - 5);
    if (button.getToggleState())
    {
        g.setColour (theme::textDim);
        g.fillRect (bounds.reduced (8.0f).withHeight (1.0f).withY (bounds.getBottom() - 4.0f));
    }
    if (button.hasKeyboardFocus (true))
    {
        g.setColour (theme::accent);
        g.drawRoundedRectangle (bounds.reduced (2.0f), tokens::radiusControl - 1.0f, 1.0f);
    }
    g.restoreState();
}
} // namespace ml
