#include "PluginEditor.h"
#include "Delivery.h"
#include "Presets.h"
#include "Version.h"

using ml::T;

namespace
{
    /** Keeps the very first window inside the current display, while never
        going below the supported 880 x 560 base layout. */
    juce::Point<int> fittedBaseSize (float scale, int width, int height)
    {
        if (auto* d = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
        {
            const auto area = d->userBounds.toNearestInt();
            const int maxW = juce::jmax (880, (int) std::floor ((float) area.getWidth() * 0.96f / scale));
            const int maxH = juce::jmax (560, (int) std::floor ((float) juce::jmax (320, area.getHeight() - 36) * 0.96f / scale));

            width = juce::jlimit (880, maxW, width);
            height = juce::jlimit (560, maxH, height);
        }

        return { width, height };
    }
} // namespace

//==============================================================================
MasterLensAudioProcessorEditor::MasterLensAudioProcessorEditor (MasterLensAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&lnf);
    tooltipWindow.setLookAndFeel (&lnf);
    setOpaque (true);
    setWantsKeyboardFocus (true);

    const int storedAppearance = (int) processor.apvts.state.getProperty ("appearance", 1);
    ml::theme::apply (storedAppearance != 0);
    lnf.refreshColours();

    uiScale = computeInitialUiScale();
    ml::ui::setScale (uiScale);

    addAndMakeVisible (rail);
    addAndMakeVisible (detail);
    addAndMakeVisible (timeline);

    rail.onSelect = [this] (int index) { setPage (index); };
    detail.onImportReference = [this] { importReference(); };
    detail.onClearReference  = [this] { clearReference(); };
    detail.onToggleReferencePlayback = [this] { toggleReferencePlayback(); };
    detail.onAutoAlignReference = [this] { autoAlignReference(); };
    detail.onExportReport    = [this] { exportReport(); };
    detail.onAddMarkerNow    = [this] { addManualMarker (timelineNow()); };
    detail.onAddMarkerAt     = [this] (double t) { addManualMarker (t); };
    detail.onExportMarkers   = [this] { exportMarkers(); };
    detail.onImportMarkers   = [this] { importMarkers(); };
    detail.onAlbumCapture    = [this] { captureAlbum(); };
    detail.onAlbumExportProof= [this] { exportAlbumProof(); };
    detail.onAlbumImport     = [this] { importAlbumSnapshot(); };
    detail.onAlbumClear      = [this] { clearAlbum(); };
    detail.onAlbumRuleChanged= [this] (int i, bool on) { setAlbumRule (i, on); };
    detail.onTrackLengthChanged = [this] (double seconds)
    {
        processor.setTrackLengthSeconds (seconds);
        processor.apvts.state.setProperty ("trackLength", seconds, nullptr);
    };
    detail.onAuditionSignal  = [this] (int i) { auditionSignal (i); };
    detail.onAuditionSignalRegion = [this] (int i, double s, double e) { auditionSignalRegion (i, s, e); };
    detail.onAuditionRiskDetail = [this] (const juce::String& d) { auditionRiskDetail (d); };
    detail.onTargetGenreChanged = [this] (int index) { setTargetGenre (index); };
    detail.onTargetWarChanged = [this] (float pct) { setTargetWar (pct); };
    detail.onApplyTargetPlan = [this] { applyTargetPlan(); };
    detail.onChooseFiles = [this] { chooseFiles(); };
    detail.onChooseFolder = [this] { chooseFolder(); };
    detail.onAddFileResultsToAlbum = [this] { addFileResultsToAlbum(); };
    detail.onClearFileResults = [this] { clearFileResults(); };
    timeline.onDoubleClick   = [this] (double t) { addManualMarker (t); };
    timeline.onSelectionChanged = [this] (double start, double end) { measureRegion (start, end); };

    buildControls();

    aboutPanel.onClose = [this] { aboutPanel.setVisibleAndFocus (false); };
    addChildComponent (aboutPanel);
    aboutPanel.setVisible (false);

    lraTargetSetting = (float) processor.apvts.state.getProperty ("lraTarget", 8.0);
    clipAllowSetting = (float) processor.apvts.state.getProperty ("clipAllowance", 0.0);

    const double storedTrackLength = (double) processor.apvts.state.getProperty ("trackLength", 0.0);
    processor.setTrackLengthSeconds (storedTrackLength);

    if (storedTrackLength > 0.0)
        detail.setTrackLengthText (juce::String (storedTrackLength, 1));

    genrePanel.onClose = [this] { genrePanel.setVisibleAndFocus (false); };
    genrePanel.onSettingsChanged = [this] (float lraTarget, float clipAllow)
    {
        lraTargetSetting = lraTarget;
        clipAllowSetting = clipAllow;
        processor.apvts.state.setProperty ("lraTarget", lraTarget, nullptr);
        processor.apvts.state.setProperty ("clipAllowance", clipAllow, nullptr);
    };
    genrePanel.onTargetChanged = [this] (float target)
    {
        setParameter ("targetLufs", target);
    };
    addChildComponent (genrePanel);
    genrePanel.setVisible (false);

    settingsPanel.onClose = [this] { settingsPanel.setVisibleAndFocus (false); };
    settingsPanel.onScaleChanged = [this] (float s) { setUiScale (s, true); };
    settingsPanel.setScale (uiScale);
    addChildComponent (settingsPanel);
    settingsPanel.setVisible (false);

    const int storedLanguage = (int) processor.apvts.state.getProperty ("language", 0);
    ml::Localisation::set (storedLanguage == 1 ? ml::Language::english : ml::Language::chinese);
    basisMode = (ml::BasisMode) juce::jlimit (0, 3,
        (int) processor.apvts.state.getProperty ("basisMode", 0));
    targetGenreIndex = juce::jlimit (0, ml::presets::customIndex - 1,
        (int) processor.apvts.state.getProperty ("targetGenre", 16));
    targetWarPct = juce::jlimit (0.0f, 100.0f,
        (float) processor.apvts.state.getProperty ("targetWarPct", 50.0));

    const int storedW = (int) processor.apvts.state.getProperty ("editorWidth", 1120);
    const int storedH = (int) processor.apvts.state.getProperty ("editorHeight", 700);
    const auto start = fittedBaseSize (uiScale,
                                       juce::jlimit (880, 1900, storedW),
                                       juce::jlimit (560, 1400, storedH));

    setResizable (true, true);
    setResizeLimits (ml::ui::scaled (880), ml::ui::scaled (560),
                     ml::ui::scaled (1900), ml::ui::scaled (1400));
    setSize (ml::ui::scaled (start.x), ml::ui::scaled (start.y));

    updateTexts();
    syncPresetToParameters();
    updateSnapshotChips();
    refreshAlbum();

    analyzerSampleRate = processor.getSampleRateValue();
    analyzer.prepare (analyzerSampleRate, 13);

    const auto saved = processor.apvts.state.getProperty ("referenceFiles").toString();

    if (saved.isNotEmpty())
    {
        juce::StringArray files;
        files.addTokens (saved, "|", {});
        reference.load (files, analyzerSampleRate, 13);
        processor.referencePlayer.load (files, analyzerSampleRate);
    }

    attachShortcutListener (*this);
    startTimerHz (60);
}

MasterLensAudioProcessorEditor::~MasterLensAudioProcessorEditor()
{
    stopTimer();
    ml::Localisation::clearOnChange();
    setLookAndFeel (nullptr);
}

void MasterLensAudioProcessorEditor::attachShortcutListener (juce::Component& c)
{
    c.addKeyListener (this);

    for (auto* child : c.getChildren())
        if (child != nullptr)
            attachShortcutListener (*child);
}
