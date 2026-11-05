#include "PluginEditor.h"
#include "ui/Layout.h"

using ml::T;

namespace
{
    juce::Rectangle<int> screenWorkArea (juce::Component& c)
    {
        const auto& displays = juce::Desktop::getInstance().getDisplays();

        if (auto* d = displays.getDisplayForRect (c.getScreenBounds()))
            return d->userBounds.toNearestInt();

        if (auto* d = displays.getPrimaryDisplay())
            return d->userBounds.toNearestInt();

        return { 0, 0, 1440, 900 };
    }
} // namespace

//==============================================================================
juce::Rectangle<int> MasterLensAudioProcessorEditor::baseBounds() const
{
    return { 0, 0, 1120, 700 };
}

//==============================================================================
float MasterLensAudioProcessorEditor::computeInitialUiScale() const
{
    const auto area = screenWorkArea (*const_cast<MasterLensAudioProcessorEditor*> (this));
    const float fit = juce::jmin ((float) area.getWidth() * 0.96f / 1120.0f,
        (float) juce::jmax (320, area.getHeight() - 36) * 0.96f / 700.0f);
    // Migrate old sessions once: their tiny font scale was not a readable preference.
    const bool migrated = (bool) processor.apvts.state.getProperty ("responsive231", false);
    const float preferred = migrated ? (float) processor.apvts.state.getProperty ("uiScale", 1.25f) : 1.25f;
    return juce::jlimit (ml::ui::minScale, ml::ui::maxScale, juce::jmin (preferred, fit));
}

//==============================================================================
void MasterLensAudioProcessorEditor::resized()
{
    uiScale = juce::jmax (0.1f, juce::jmin (getWidth() / 1120.0f, getHeight() / 700.0f));
    ml::ui::setScale (uiScale);
    const auto base = baseBounds();
    settingsPanel.setScale (uiScale);
    auto remaining = base;
    headerArea = remaining.removeFromTop (64);
    footerArea = remaining.removeFromBottom (32);
    paramArea = remaining.removeFromBottom (96);
    rail.setBounds (remaining.removeFromTop (48).reduced (16, 0));
    auto body = remaining.reduced (16);

    if (selectedPage == (int) ml::PageId::spectrum)
    {
        const int spectrumHeight = ml::grid::down ((body.getHeight() - 8) * 56 / 100);
        spectrumArea = body.removeFromTop (spectrumHeight);
        body.removeFromTop (8);
        readingsArea = body;
        const int bandReadings = readingsArea.getHeight() >= 180 ? 64 : 0;
        detail.setSpectrumSplit (spectrumHeight, bandReadings);
        detail.setBounds (remaining.reduced (16));
        readout.setBounds (readingsArea.withTrimmedTop (32).withTrimmedBottom (bandReadings));
        readout.toFront (false);
    }
    else
    {
        spectrumArea = {};
        readingsArea = body.removeFromLeft (base.getWidth() < 1000 ? 176 : 208);
        readout.setBounds (readingsArea);
        body.removeFromLeft (16);
        detail.setBounds (body);
    }

    const bool english = ml::Localisation::isEnglish();
    auto headerControls = headerArea.reduced (24, 16);
    const int settingsWidth = english ? 72 : 48;
    const int aboutWidth = english ? 56 : 40;
    const int controlsWidth = 48 + aboutWidth + settingsWidth + 40 + 3 * 8;
    ml::grid::row (headerControls.removeFromRight (controlsWidth),
                  { &themeButton, &aboutButton, &settingsButton, &languageButton },
                  { 48, aboutWidth, settingsWidth, 40 });

    auto desk = paramArea.reduced (24, 8);
    auto basis = desk.removeFromLeft (base.getWidth() < 1000 ? 136 : 192);
    presetBox.setBounds (basis.withTrimmedTop (32).withHeight (32));
    desk.removeFromLeft (16);
    auto targets = desk.removeFromLeft (176);
    auto captions = targets.removeFromTop (16);
    ml::grid::row (captions, { &targetCaption, &ceilingCaption }, { 80, 80 });
    ml::grid::row (targets.withHeight (64), { &targetSlider, &ceilingSlider }, { 80, 80 });
    infoButton.setVisible (false);
    targetCaption.setVisible (true);
    ceilingCaption.setVisible (true);
    desk.removeFromLeft (16);
    const int holdWidth = english ? 48 : 40;
    const int clearWidth = english ? 48 : 40;
    const int monitorWidth = base.getWidth() < 1000 ? 64 : 88;
    const int snapshotCaptionWidth = english ? 72 : 60;
    const int actionWidth = holdWidth + clearWidth + monitorWidth + snapshotCaptionWidth
                            + 32 + 32 + 88 + 6 * 8;
    auto actions = desk.withTrimmedTop (24).withHeight (32).removeFromRight (actionWidth);
    ml::grid::row (actions, { &holdButton, &clearButton, &monitorButton, &snapshotCaption,
                              &snapshotA, &snapshotB, &exportButton },
                  { holdWidth, clearWidth, monitorWidth, snapshotCaptionWidth, 32, 32, 88 });

    aboutPanel.setBounds (base);
    genrePanel.setBounds (base);
    settingsPanel.setBounds (base);

    ml::grid::alignChildren (*this);
    for (auto* overlay : { static_cast<juce::Component*> (&aboutPanel),
                          static_cast<juce::Component*> (&genrePanel),
                          static_cast<juce::Component*> (&settingsPanel) })
        if (overlay->isVisible()) overlay->toFront (false);

    // One transform for the whole surface: layout stays in base points, so a
    // 200 % zoom cannot clip a label.
    for (auto* child : getChildren())
        if (child != nullptr)
            child->setTransform (dynamic_cast<juce::ResizableCornerComponent*> (child) != nullptr
                ? juce::AffineTransform() : juce::AffineTransform::scale (uiScale));

    processor.apvts.state.setProperty ("uiScale", uiScale, nullptr);
    processor.apvts.state.setProperty ("responsive231", true, nullptr);
    processor.apvts.state.setProperty ("editorWidth", getWidth(), nullptr);
    processor.apvts.state.setProperty ("editorHeight", getHeight(), nullptr);
}

//==============================================================================
void MasterLensAudioProcessorEditor::setUiScale (float newScale, bool store)
{
    newScale = juce::jlimit (ml::ui::minScale, ml::ui::maxScale, newScale);
    if (store)
    {
        const auto area = screenWorkArea (*this);
        const float fit = juce::jmin (area.getWidth() * 0.96f / 1120.0f,
            juce::jmax (320, area.getHeight() - 36) * 0.96f / 700.0f);
        newScale = juce::jmax (ml::ui::minScale, juce::jmin (newScale, fit));
    }
    setSize (juce::roundToInt (1120.0f * newScale), juce::roundToInt (700.0f * newScale));
    settingsPanel.setScale (uiScale);
    detail.invalidateDisplays();
    repaint();
}

//==============================================================================
void MasterLensAudioProcessorEditor::toggleTheme()
{
    ml::theme::apply (! ml::theme::dark);
    processor.apvts.state.setProperty ("appearance", ml::theme::dark ? 1 : 0, nullptr);
    applyTheme();
}

void MasterLensAudioProcessorEditor::applyTheme()
{
    lnf.refreshColours();

    targetCaption.setColour (juce::Label::textColourId, ml::theme::textDim);
    ceilingCaption.setColour (juce::Label::textColourId, ml::theme::textDim);

    detail.invalidateDisplays();
    readout.repaint();
    rail.repaint();
    timeline.repaint();
    settingsPanel.repaint();

    updateTexts();
    repaint();
}

void MasterLensAudioProcessorEditor::setThemeForSnapshot (bool useDark)
{
    if (useDark == ml::theme::dark)
        return;

    toggleTheme();
}
