#include "PluginEditor.h"

using ml::T;

//==============================================================================
void MasterLensAudioProcessorEditor::buildControls()
{
    presetBox.onChange = [this] { applyPreset (presetBox.getSelectedId() - 1); };
    addAndMakeVisible (presetBox);

    infoButton.setButtonText ("?");
    infoButton.setTooltip (T ("曲风参考：这个曲风的实际发行响度与响度战争背景",
                              "Genre reference: what released masters measure and why"));
    infoButton.onClick = [this] { showGenreInfo(); };
    addAndMakeVisible (infoButton);

    const auto captionFont = ml::uiFont (ml::tokens::label);

    targetCaption.setFont (captionFont);
    targetCaption.setColour (juce::Label::textColourId, ml::theme::textDim);
    addAndMakeVisible (targetCaption);

    ceilingCaption.setFont (captionFont);
    ceilingCaption.setColour (juce::Label::textColourId, ml::theme::textDim);
    addAndMakeVisible (ceilingCaption);

    targetSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    targetSlider.setTextBoxStyle (juce::Slider::TextBoxRight, true, 72, 32);
    targetSlider.setDoubleClickReturnValue (true, -14.0);
    addAndMakeVisible (targetSlider);

    ceilingSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    ceilingSlider.setTextBoxStyle (juce::Slider::TextBoxRight, true, 72, 32);
    ceilingSlider.setDoubleClickReturnValue (true, -1.0);
    addAndMakeVisible (ceilingSlider);

    targetAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "targetLufs", targetSlider);

    ceilingAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "tpCeiling", ceilingSlider);

    monitorButton.setClickingTogglesState (true);
    monitorButton.setColour (juce::TextButton::buttonOnColourId, ml::theme::accentSoft);
    monitorAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, "monitorMatch", monitorButton);
    addAndMakeVisible (monitorButton);

    targetSlider.onValueChange = [this] { syncPresetToParameters(); };
    ceilingSlider.onValueChange = [this] { syncPresetToParameters(); };

    holdButton.setClickingTogglesState (true);
    holdButton.onClick = [this] { setHold (holdButton.getToggleState(), false); };
    addAndMakeVisible (holdButton);

    clearButton.onClick = [this] { resetAnalysis(); };
    addAndMakeVisible (clearButton);

    languageButton.onClick = [this]
    {
        const auto next = ml::Localisation::isEnglish() ? ml::Language::chinese : ml::Language::english;
        ml::Localisation::set (next);
        processor.apvts.state.setProperty ("language", next == ml::Language::english ? 1 : 0, nullptr);
        updateTexts();
        settingsPanel.refreshTexts();
        detail.refreshTexts();
        detail.invalidateDisplays();
        repaint();
    };
    addAndMakeVisible (languageButton);

    aboutButton.onClick = [this]
    {
        settingsPanel.setVisibleAndFocus (false);
        aboutPanel.setVisibleAndFocus (! aboutPanel.isVisible());
    };
    addAndMakeVisible (aboutButton);

    settingsButton.onClick = [this]
    {
        aboutPanel.setVisibleAndFocus (false);
        genrePanel.setVisibleAndFocus (false);
        settingsPanel.setVisibleAndFocus (! settingsPanel.isVisible());
    };
    addAndMakeVisible (settingsButton);

    themeButton.onClick = [this] { toggleTheme(); };
    addAndMakeVisible (themeButton);

    exportButton.onClick = [this] { exportReport(); };
    addAndMakeVisible (exportButton);

    snapshotA.onClick = [this] { recallSnapshot (0); };
    snapshotB.onClick = [this] { recallSnapshot (1); };
    snapshotA.onClear = [this] { clearSnapshot (0); };
    snapshotB.onClear = [this] { clearSnapshot (1); };
    addAndMakeVisible (snapshotA);
    addAndMakeVisible (snapshotB);
}
