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

    headerArea = b.removeFromTop (52);
    paramArea = b.removeFromTop (64);
    footerArea = b.removeFromBottom (36);

    // ---- header widgets -----------------------------------------------------
    {
        auto h = headerArea.reduced (14, 10);

        languageButton.setBounds (h.removeFromRight (46).withSizeKeepingCentre (46, 28));
        h.removeFromRight (8);
        settingsButton.setBounds (h.removeFromRight (64).withSizeKeepingCentre (64, 28));
        h.removeFromRight (8);
        aboutButton.setBounds (h.removeFromRight (58).withSizeKeepingCentre (58, 28));
        h.removeFromRight (8);
        themeButton.setBounds (h.removeFromRight (72).withSizeKeepingCentre (72, 28));
        h.removeFromRight (12);

        const int presetW = juce::jlimit (150, 240, h.getWidth() - 360);
        presetBox.setBounds (h.removeFromRight (presetW).withSizeKeepingCentre (presetW, 28));
    }

    // ---- parameter bar: 12 column grid --------------------------------------
    {
        auto bar = paramArea.reduced (14, 8);
        auto actions = bar.removeFromRight (juce::jlimit (160, 186, bar.getWidth() / 4));

        holdButton.setBounds (actions.removeFromRight (74).withSizeKeepingCentre (74, 30));
        actions.removeFromRight (6);
        clearButton.setBounds (actions.removeFromRight (90).withSizeKeepingCentre (90, 30));

        bar.removeFromRight (12);

        const int colW = juce::jmax (40, bar.getWidth() / 12);

        auto left = bar.withWidth (colW * 5);
        auto right = bar.withX (bar.getX() + colW * 6).withWidth (colW * 5);

        targetCaption.setBounds (left.removeFromTop (15));
        targetSlider.setBounds (left.removeFromTop (30));
        ceilingCaption.setBounds (right.removeFromTop (15));
        ceilingSlider.setBounds (right.removeFromTop (30));
    }

    // ---- body ---------------------------------------------------------------
    auto body = b.reduced (12, 10);

    rail.setBounds (body.removeFromLeft (186));
    body.removeFromLeft (10);

    timeline.setBounds (body.removeFromBottom (40));
    body.removeFromBottom (6);
    detail.setBounds (body);

    // ---- footer widgets -----------------------------------------------------
    {
        auto f = footerArea.reduced (14, 4);

        exportButton.setBounds (f.removeFromRight (112).withSizeKeepingCentre (112, 26));
        f.removeFromRight (6);
        snapshotB.setBounds (f.removeFromRight (32).withSizeKeepingCentre (32, 26));
        f.removeFromRight (4);
        snapshotA.setBounds (f.removeFromRight (32).withSizeKeepingCentre (32, 26));
        f.removeFromRight (6);
        monitorButton.setBounds (f.removeFromRight (86).withSizeKeepingCentre (86, 26));
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
