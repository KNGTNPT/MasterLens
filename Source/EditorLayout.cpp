#include "PluginEditor.h"

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
             juce::jmax (1, juce::roundToInt ((float) getWidth() / s)),
             juce::jmax (1, juce::roundToInt ((float) getHeight() / s)) };
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
    auto base = baseBounds();
    auto b = base;

    headerArea = b.removeFromTop (48);
    readout.setBounds (b.removeFromTop (base.getWidth() < 960 ? 64 : 76));
    footerArea = b.removeFromBottom (32);
    paramArea = {};

    // ---- header widgets -----------------------------------------------------
    {
        auto h = headerArea.reduced (20, 10);

        languageButton.setBounds (h.removeFromRight (34).withSizeKeepingCentre (34, 26));
        h.removeFromRight (6);
        settingsButton.setBounds (h.removeFromRight (44).withSizeKeepingCentre (44, 26));
        h.removeFromRight (6);
        aboutButton.setBounds (h.removeFromRight (36).withSizeKeepingCentre (36, 26));
        h.removeFromRight (6);
        themeButton.setBounds (h.removeFromRight (42).withSizeKeepingCentre (42, 26));
        h.removeFromRight (14);

        const int presetW = juce::jlimit (126, 190, h.getWidth() - 470);
        presetBox.setBounds (h.removeFromRight (presetW).withSizeKeepingCentre (presetW, 26));
        h.removeFromRight (10);

        auto ceilingGroup = h.removeFromRight (136);
        ceilingCaption.setBounds (ceilingGroup.removeFromLeft (30));
        ceilingSlider.setBounds (ceilingGroup.withSizeKeepingCentre (106, 26));
        h.removeFromRight (8);

        auto targetGroup = h.removeFromRight (136);
        targetCaption.setBounds (targetGroup.removeFromLeft (30));
        targetSlider.setBounds (targetGroup.withSizeKeepingCentre (106, 26));

        targetCaption.setVisible (true);
        ceilingCaption.setVisible (true);
    }

    // ---- body ---------------------------------------------------------------
    auto body = b.reduced (20, 8);

    const int railW = base.getWidth() < 960 ? 168 : 176;
    rail.setBounds (body.removeFromLeft (railW));
    body.removeFromLeft (20);
    detail.setBounds (body);

    // ---- footer widgets -----------------------------------------------------
    {
        auto f = footerArea.reduced (16, 4);

        exportButton.setBounds (f.removeFromRight (84).withSizeKeepingCentre (84, 24));
        f.removeFromRight (6);
        snapshotB.setBounds (f.removeFromRight (24).withSizeKeepingCentre (24, 24));
        f.removeFromRight (4);
        snapshotA.setBounds (f.removeFromRight (24).withSizeKeepingCentre (24, 24));
        f.removeFromRight (6);
        monitorButton.setBounds (f.removeFromRight (72).withSizeKeepingCentre (72, 24));
        f.removeFromRight (6);
        clearButton.setBounds (f.removeFromRight (44).withSizeKeepingCentre (44, 24));
        f.removeFromRight (4);
        holdButton.setBounds (f.removeFromRight (34).withSizeKeepingCentre (34, 24));
    }

    aboutPanel.setBounds (base);
    genrePanel.setBounds (base);
    settingsPanel.setBounds (base);

    // One transform for the whole surface: layout stays in base points, so a
    // 200 % zoom cannot clip a label.
    for (auto* child : getChildren())
        if (child != nullptr)
            child->setTransform (juce::AffineTransform::scale (uiScale));

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
