#include "Widgets.h"
#include "../Version.h"
#include "Logo.h"
#include "MasterLensFonts.h"

namespace ml
{
//==============================================================================
SnapshotChip::SnapshotChip (juce::String letterText)
    : letter (std::move (letterText))
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setWantsKeyboardFocus (true);
}

void SnapshotChip::setState (bool hasData, bool isActive)
{
    if (filled == hasData && active == isActive)
        return;

    filled = hasData;
    active = isActive;
    repaint();
}

void SnapshotChip::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);

    const auto background = active ? theme::accentSoft
                                   : (isMouseOver() ? theme::cardAlt.brighter (0.06f) : theme::cardAlt);

    g.setColour (background);
    g.fillRoundedRectangle (r, tokens::radiusControl);

    g.setColour (active ? theme::accent.withAlpha (0.75f) : theme::border);
    g.drawRoundedRectangle (r, tokens::radiusControl, 1.0f);

    g.setColour (active ? theme::accentInk : (filled ? theme::text : theme::textDim));
    g.setFont (getValueFont());
    g.drawText (letter, getLocalBounds(), juce::Justification::centred);

    if (filled)
    {
        g.setColour (active ? theme::accent : theme::good);
        g.fillEllipse (r.getRight() - 6.0f, r.getBottom() - 6.0f, 4.0f, 4.0f);
    }

    if (hasKeyboardFocus (true))
    {
        g.setColour (theme::accent);
        g.drawRoundedRectangle (r.reduced (2.0f), tokens::radiusControl - 1.0f, 2.0f);
    }
}

void SnapshotChip::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isRightButtonDown() || e.mods.isPopupMenu())
    {
        if (onClear != nullptr && filled)
            onClear();

        return;
    }

    if (onClick != nullptr)
        onClick();
}

juce::String SnapshotChip::getTooltip()
{
    if (filled)
        return T ("点击切回实时读数；右键清除该快照",
                  "Click to return to live readings; right-click to clear this snapshot");

    return T ("点击把当前读数保存到该快照，用于改动前后对比",
              "Click to store the current readings for before / after comparison");
}

//==============================================================================
static juce::Rectangle<int> aboutCardBounds (juce::Rectangle<int> area)
{
    const int w = juce::jmin (640, area.getWidth() - 64);
    const int h = juce::jmin (350, area.getHeight() - 64);

    return area.withSizeKeepingCentre (w, h);
}

AboutPanel::AboutPanel()
{
    // Keep the supplied vector bytes and geometry. The light theme uses the
    // original black artwork; the dark theme only promotes its black ink to
    // white so it remains legible. Red is untouched.
    studioLight = juce::Drawable::createFromImageData (MasterLensFonts::Berrystudio_svg,
                                                        MasterLensFonts::Berrystudio_svgSize);
    studioDark = juce::Drawable::createFromImageData (MasterLensFonts::Berrystudio_svg,
                                                       MasterLensFonts::Berrystudio_svgSize);

    if (studioDark != nullptr)
        studioDark->replaceColour (juce::Colours::black, juce::Colours::white);

    setOpaque (false);

    closeButton.setButtonText (ml::grid::controlText (T ("关闭", "Close")));
    closeButton.onClick = [this] { if (onClose != nullptr) onClose(); };
    addAndMakeVisible (closeButton);
}

void AboutPanel::setVisibleAndFocus (bool shouldBeVisible)
{
    setVisible (shouldBeVisible);

    if (shouldBeVisible)
    {
        toFront (true);
        closeButton.setButtonText (ml::grid::controlText (T ("关闭", "Close")));
        closeButton.grabKeyboardFocus();
    }
}

void AboutPanel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (theme::dark ? 0.55f : 0.28f));

    const auto card = aboutCardBounds (getLocalBounds()).toFloat();

    surface::raised (g, card, tokens::radiusCard);

    auto inner = card.reduced (28.0f, 24.0f).toNearestInt();

    auto logo = inner.removeFromTop (64).removeFromLeft (64).toFloat();

    paintLogoMark (g, logo);

    g.setColour (theme::text);
    g.setFont (getLabelFont());
    g.drawText (kProductName, (int) logo.getRight() + 16, (int) logo.getY() - 2, 300, 30,
                juce::Justification::centredLeft, false);

    g.setColour (theme::cyan);
    g.setFont (getLabelFont());
    g.drawText (T ("一个 BerryStudio 产品", "A BerryStudio product"),
                (int) logo.getRight() + 16, (int) logo.getY() + 26, 330, 18,
                juce::Justification::centredLeft, false);

    g.setColour (theme::textDim);
    g.setFont (getLabelFont());
    g.drawText (T ("母带响度 · 平台归一化 · 信号健康", "Loudness · normalisation · signal health"),
                (int) logo.getRight() + 16, (int) logo.getY() + 44, 360, 18,
                juce::Justification::centredLeft, false);

    inner.removeFromTop (10);

    g.setColour (theme::divider);
    g.drawHorizontalLine (inner.getY(), (float) inner.getX(), (float) inner.getRight());
    inner.removeFromTop (12);

    const auto drawRow = [&g, &inner] (const juce::String& label, const juce::String& value,
                                       juce::Colour colour)
    {
        auto r = inner.removeFromTop (28);
        g.setColour (theme::textFaint);
        g.setFont (getLabelFont());
        g.drawText (label, r.removeFromLeft (128), juce::Justification::centredLeft);
        g.setColour (colour);
        g.setFont (getLabelFont());
        g.drawText (value, r, juce::Justification::centredLeft);
    };

    drawRow (T ("版本", "Version"), kVersion, theme::text);

    inner.removeFromTop (8);

    juce::StringArray standards;
    standards.add (T ("测量标准：ITU-R BS.1770-4 · EBU R128 · EBU Tech 3342",
                      "Standards: ITU-R BS.1770-4 · EBU R128 · EBU Tech 3342"));
    standards.add (T ("真峰值 192 kHz 插值基准 · 零延迟 · 不修改音频",
                      "True peak at a 192 kHz target · zero latency · never alters audio"));
    standards.add (T ("选区等响需先播放该段 · 编码 A/B 使用随包 FFmpeg",
                      "Selection match needs the section played first · codec A/B uses FFmpeg"));
    standards.add (T ("构建于 JUCE 8", "Built with JUCE 8"));
    text::drawLines (g, getLabelFont(), theme::textFaint, standards,
                     inner.removeFromTop (80), 20);

    inner.removeFromTop (20);
    auto studioArea = inner.removeFromTop (40).removeFromLeft (220);
    if (auto* studio = theme::dark ? studioDark.get() : studioLight.get())
        studio->drawWithin (g, studioArea.toFloat(), juce::RectanglePlacement::xLeft, 1.0f);
}

void AboutPanel::resized()
{
    const auto card = aboutCardBounds (getLocalBounds());
    closeButton.setBounds (card.getRight() - 124, card.getBottom() - 46, 96, 30);
    grid::alignChildren (*this);
}

void AboutPanel::mouseDown (const juce::MouseEvent& e)
{
    if (! aboutCardBounds (getLocalBounds()).contains (e.getPosition()))
        if (onClose != nullptr)
            onClose();
}

} // namespace ml
