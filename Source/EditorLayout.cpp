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
    const float s = uiScale > 0.01f ? uiScale : 1.0f;

    return { 0, 0,
             ml::grid::down (juce::roundToInt ((float) getWidth() / s)),
             ml::grid::down (juce::roundToInt ((float) getHeight() / s)) };
}

//==============================================================================
float MasterLensAudioProcessorEditor::computeInitialUiScale() const
{
    const auto area = screenWorkArea (const_cast<MasterLensAudioProcessorEditor&> (*this));
    const float availW = (float) area.getWidth() * 0.96f;
    const float availH = (float) juce::jmax (320, area.getHeight() - 36) * 0.96f;

    // The largest scale whose *minimum* layout still fits on this display.
    float fitScale = ml::ui::minScale;

    for (float step : ml::ui::scaleSteps)
        if (880.0f * step <= availW && 560.0f * step <= availH)
            fitScale = step;

    const auto stored = processor.apvts.state.getProperty ("uiScale");

    if (! stored.isVoid())
    {
        // Older sessions may have saved 150 / 200 % on a laptop. Clamp the
        // restored scale to what the current display can actually show instead
        // of opening a window that cannot fit under the menu bar / Dock.
        const float requested = juce::jlimit (ml::ui::minScale, ml::ui::maxScale, (float) stored);
        const float fitted = juce::jmin (requested, fitScale);

        if (std::abs (fitted - requested) > 0.001f)
            processor.apvts.state.setProperty ("uiScale", fitted, nullptr);

        return fitted;
    }

    // First launch: pick the largest step whose minimum layout still fits.
    processor.apvts.state.setProperty ("uiScale", fitScale, nullptr);
    return fitScale;
}

//==============================================================================
void MasterLensAudioProcessorEditor::resized()
{
    const auto base = baseBounds();
    const int alignedWidth = ml::ui::scaled (base.getWidth());
    const int alignedHeight = ml::ui::scaled (base.getHeight());
    if (getWidth() != alignedWidth || getHeight() != alignedHeight)
    {
        setSize (alignedWidth, alignedHeight);
        return;
    }
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
    const int actionWidth = holdWidth + clearWidth + monitorWidth + 32 + 32 + 88 + 5 * 8;
    auto actions = desk.withTrimmedTop (24).withHeight (32).removeFromRight (actionWidth);
    ml::grid::row (actions, { &holdButton, &clearButton, &monitorButton, &snapshotA, &snapshotB, &exportButton },
                  { holdWidth, clearWidth, monitorWidth, 32, 32, 88 });

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

    processor.apvts.state.setProperty ("editorWidth", base.getWidth(), nullptr);
    processor.apvts.state.setProperty ("editorHeight", base.getHeight(), nullptr);
}

//==============================================================================
void MasterLensAudioProcessorEditor::setUiScale (float newScale, bool store)
{
    newScale = juce::jlimit (ml::ui::minScale, ml::ui::maxScale, newScale);

    if (store)
    {
        // Never let a zoom step open a window that cannot fit the current
        // display's work area (MacBook Air default scaling is the common case).
        const auto area = screenWorkArea (*this);
        float fitScale = ml::ui::minScale;

        for (float step : ml::ui::scaleSteps)
            if (880.0f * step <= (float) area.getWidth() * 0.96f
                && 560.0f * step <= (float) juce::jmax (320, area.getHeight() - 36) * 0.96f)
                fitScale = step;

        newScale = juce::jmin (newScale, fitScale);
    }

    if (std::abs (newScale - uiScale) < 0.001f)
        return;

    const int baseW = juce::jlimit (880, 1900, juce::roundToInt ((float) getWidth() / uiScale));
    const int baseH = juce::jlimit (560, 1400, juce::roundToInt ((float) getHeight() / uiScale));

    uiScale = newScale;
    ml::ui::setScale (uiScale);

    if (store)
        processor.apvts.state.setProperty ("uiScale", uiScale, nullptr);

    setResizeLimits (ml::ui::scaled (880), ml::ui::scaled (560),
                     ml::ui::scaled (1900), ml::ui::scaled (1400));
    setSize (ml::ui::scaled (baseW), ml::ui::scaled (baseH));

    settingsPanel.setScale (uiScale);
    detail.invalidateDisplays();
    readout.repaint();
    rail.repaint();
    timeline.repaint();
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
