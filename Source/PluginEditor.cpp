#include "PluginEditor.h"
#include "Delivery.h"
#include "Presets.h"
#include "Version.h"

#if JUCE_DEBUG && defined(MASTERLENS_ENABLE_INSPECTOR)
#include <melatonin_inspector/melatonin_inspector.h>
#endif

using ml::T;

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

    addAndMakeVisible (readout);
    addAndMakeVisible (rail);
    addAndMakeVisible (detail);

    // 2.0.2: the separate timeline strip is retired; markers and the playhead
    // now live inside the history graph.
    addChildComponent (timeline);
    timeline.setVisible (false);

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
    detail.onHistorySeek = [this] (double age)
    {
        const double now = timelineNow();
        const double t = juce::jmax (0.0, now - age);
        processor.startAudition (juce::jmax (0.0, t - 1.0), t + 1.0, 20.0, 20000.0, false);
        footerStatusText = T ("试听曲线选点 · Esc 停止", "Auditioning graph point · Esc to stop");
        footerStatusColour = ml::theme::accent;
        repaint();
    };
    detail.onHistorySelection = [this] (double fromAge, double toAge)
    {
        const double now = timelineNow();
        measureRegion (juce::jmax (0.0, now - toAge), juce::jmax (0.0, now - fromAge));
    };
    detail.onAuditionRiskDetail = [this] (const juce::String& d) { auditionRiskDetail (d); };
    detail.onTargetGenreChanged = [this] (int index) { setTargetGenre (index); };
    detail.onTargetWarChanged = [this] (float pct) { setTargetWar (pct); };
    detail.onApplyTargetPlan = [this] { applyTargetPlan(); };
    detail.onChooseFiles = [this] { chooseFiles(); };
    detail.onChooseFolder = [this] { chooseFolder(); };
    detail.onAddFileResultsToAlbum = [this] { addFileResultsToAlbum(); };
    detail.onClearFileResults = [this] { clearFileResults(); };
    detail.onExportFileTable = [this] { exportFileTable(); };
    detail.onPreviewFile = [this] (const juce::String& path, double seconds)
    {
        processor.startFilePreview (juce::File (path), juce::jmax (0.0, seconds - 1.5),
                                    seconds + 2.5, true);
        footerStatusText = T ("试听文件证据 · 双击再次定位，Esc 停止",
                              "Previewing file evidence · double-click again, Esc stops");
        footerStatusColour = ml::theme::accent;
        repaint();
    };
    detail.onReviewEvidence = [this] (const juce::String& id, bool reviewed)
    {
        processor.albumSession.setReviewed (id, reviewed);
        for (auto& result : fileResults)
            for (auto& finding : result.forensic.findings)
                if (result.forensic.evidenceKey (finding) == id) finding.reviewed = reviewed;
        refreshAlbum();
    };
    detail.onRevealFile = [] (const juce::String& path)
    {
        juce::File (path).revealToUser();
    };
    detail.onProjectMenu = [this] { showProjectMenu(); };
    timeline.onDoubleClick   = [this] (double t) { addManualMarker (t); };
    timeline.onSelectionChanged = [this] (double start, double end) { measureRegion (start, end); };

    buildControls();

    aboutPanel.onClose = [this] { hideOverlay (aboutPanel); };
    addChildComponent (aboutPanel);
    aboutPanel.setVisible (false);

    lraTargetSetting = (float) processor.apvts.state.getProperty ("lraTarget", 8.0);
    clipAllowSetting = (float) processor.apvts.state.getProperty ("clipAllowance", 0.0);

    const double storedTrackLength = (double) processor.apvts.state.getProperty ("trackLength", 0.0);
    processor.setTrackLengthSeconds (storedTrackLength);

    if (storedTrackLength > 0.0)
        detail.setTrackLengthText (juce::String (storedTrackLength, 1));

    genrePanel.onClose = [this] { hideOverlay (genrePanel); };
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

    settingsPanel.onClose = [this] { hideOverlay (settingsPanel); };
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

    setResizable (true, true);
    setResizeLimits (896, 560, 2240, 1400);
    if (auto* limits = getConstrainer()) limits->setFixedAspectRatio (1.6);
    setSize (juce::roundToInt (1120.0f * uiScale), juce::roundToInt (700.0f * uiScale));

    updateTexts();
    detail.refreshTexts();
    settingsPanel.refreshTexts();
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
    lnf.bindButtons (*this);
    displayClock = juce::VBlankAttachment (this, [this] (double)
    {
        lastVBlankMs = juce::Time::getMillisecondCounterHiRes();
        updateUiFrame();
    });
    startTimerHz (90); // headroom for a 60 Hz presentation target under GUI load
}

MasterLensAudioProcessorEditor::~MasterLensAudioProcessorEditor()
{
#if JUCE_DEBUG && defined(MASTERLENS_ENABLE_INSPECTOR)
    inspector.reset();
#endif
    displayClock = {};
    stopTimer();
    ml::Localisation::clearOnChange();
    setLookAndFeel (nullptr);
}

#if JUCE_DEBUG && defined(MASTERLENS_ENABLE_INSPECTOR)
void MasterLensAudioProcessorEditor::showInspector()
{
    if (inspector == nullptr)
        inspector = std::make_unique<melatonin::Inspector> (*this, false);
    inspector->toggle (true);
}
#endif

void MasterLensAudioProcessorEditor::attachShortcutListener (juce::Component& c)
{
    c.addKeyListener (this);

    for (auto* child : c.getChildren())
        if (child != nullptr)
            attachShortcutListener (*child);
}
