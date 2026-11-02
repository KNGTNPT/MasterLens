#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"
#include "Text.h"
#include "Basis.h"
#include "album/AlbumSession.h"
#include "dsp/ReferenceSpectrum.h"
#include "ui/Modules.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"

#if JUCE_DEBUG && defined(MASTERLENS_ENABLE_INSPECTOR)
namespace melatonin { class Inspector; }
#endif

class MasterLensAudioProcessorEditor : public juce::AudioProcessorEditor,
                                      public juce::FileDragAndDropTarget,
                                      private juce::Timer,
                                      private juce::KeyListener
{
public:
    friend class MasterLensLayoutCheck;
    explicit MasterLensAudioProcessorEditor (MasterLensAudioProcessor&);
    ~MasterLensAudioProcessorEditor() override;
#if JUCE_DEBUG && defined(MASTERLENS_ENABLE_INSPECTOR)
    void showInspector();
#endif

    void paint (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray&, int, int) override;

    bool keyPressed (const juce::KeyPress&) override;
    bool keyPressed (const juce::KeyPress&, juce::Component*) override;
    bool keyStateChanged (bool isKeyDown) override;

    /** Used by the offscreen snapshot tool. */
    void setAboutVisible (bool shouldBeVisible) { aboutPanel.setVisibleAndFocus (shouldBeVisible); }
    void setSettingsVisible (bool shouldBeVisible) { settingsPanel.setVisibleAndFocus (shouldBeVisible); }
    void showGenreInfoForSnapshot() { showGenreInfo(); }
    void setPresetForSnapshot (int index) { presetBox.setSelectedId (index + 1, juce::dontSendNotification); applyPreset (index); }
    void expandFirstIssueForSnapshot();
    void setPageForSnapshot (int page);
    void setFileViewForSnapshot (int view) { detail.setFileView (view); }
    void setThemeForSnapshot (bool useDark);
    void setScaleForSnapshot (float scale) { setUiScale (scale, false); }
    void captureSnapshotForSnapshot (int slot);
    void setBasisForSnapshot (int basis) { presetBox.setSelectedId (basis + 1, juce::dontSendNotification); applyPreset (basis); }
    void captureAlbumForSnapshot() { captureAlbum(); }
    void captureAlbumAsForSnapshot (const juce::String& label)
    {
        detail.setAlbumTrackLabel (label);
        captureAlbum();
    }
    void runTimerTickForSnapshot() { timerCallback(); }
    int getMeterTicksForBenchmark() const noexcept { return detail.getMeterUpdates(); }
    int getReadoutTicksForBenchmark() const noexcept { return readout.getUpdateCount(); }
    int getTimerTicksForBenchmark() const noexcept { return timerTicks; }
    int getPresentationTicksForBenchmark() const noexcept { return presentationTicks; }
#if JUCE_DEBUG
    /** Inspect the UI-owned cache; never consume the SPSC FIFO from a second reader. */
    const ml::SpectrumFifo::Reader& getOutputSpectrumForDebug() const noexcept { return outputSpectrumReader; }
#endif
    void setSpectrumViewForSnapshot (int view) { detail.setSpectrumView (view); }

    void writeReportPdfForSnapshot (const juce::File& file)
    {
        const auto pdf = buildReportPdf();
        file.replaceWithData (pdf.getData(), pdf.getSize());
    }

private:
    void timerCallback() override;
    void buildControls();
    void updateTexts();
    void updateRail();
    void applyPreset (int index);
    void showGenreInfo();
    void revealOverlay (juce::Component&);
    void hideOverlay (juce::Component&);
    void syncPresetToParameters();
    void refreshAlbum();
    void captureAlbum();
    void exportAlbumProof();
    void importAlbumSnapshot();
    void clearAlbum();
    void setAlbumRule (int index, bool enabled);
    void auditionSignal (int index);
    void auditionSignalRegion (int index, double start, double end);
    void auditionRiskDetail (const juce::String& detail);
    void setTargetGenre (int index);
    void setTargetWar (float percent);
    void applyTargetPlan();
    void updateSignalProblems();
    double timelineNow() const;
    double timelineTotal() const;
    ml::BasisContext currentBasis() const;
    std::vector<float> currentBandDevs() const;
    void resetAnalysis();
    void exportReport();
    void importReference();
    void clearReference();
    void toggleReferencePlayback();
    void autoAlignReference();
    void chooseFiles();
    void chooseFolder();
    void startFileAnalysis (const juce::StringArray& paths);
    void pollFileAnalysis();
    void addFileResultToAlbum (const ml::FileAnalysisResult& result);
    void addFileResultsToAlbum();
    void clearFileResults();
    void exportFileTable();
    void captureRawSeries();
    void resetRawSeries();
    void measureRegion (double start, double end);
    juce::String buildReport() const;
    juce::String buildReportJson() const;
    juce::String buildReportCsv() const;
    juce::MemoryBlock buildReportPdf() const;
    juce::String buildRawTimeSeriesCsv() const;
    void setParameter (const char* id, float value);
    void setHold (bool shouldHold, bool fromKeyboard);
    void setPage (int index);
    void toggleTheme();
    void applyTheme();
    void setUiScale (float scale, bool store);
    float computeInitialUiScale() const;
    juce::Rectangle<int> baseBounds() const;
    void attachShortcutListener (juce::Component& c);
    void addMarker (double t, int severity, int category, const juce::String& text);
    void addManualMarker (double time);
    void exportMarkers();
    void importMarkers();
    void captureSnapshot (int slot);
    void recallSnapshot (int slot);
    void clearSnapshot (int slot);
    void updateSnapshotChips();

    MasterLensAudioProcessor& processor;
    ml::MasterLensLookAndFeel lnf;
    juce::TooltipWindow tooltipWindow { this, 650 };
    juce::ComponentAnimator animator;
#if JUCE_DEBUG && defined(MASTERLENS_ENABLE_INSPECTOR)
    std::unique_ptr<melatonin::Inspector> inspector;
#endif

    juce::ComboBox presetBox;
    juce::TextButton infoButton;
    juce::Label targetCaption, ceilingCaption;
    juce::Slider targetSlider, ceilingSlider;
    juce::TextButton holdButton, clearButton, languageButton, aboutButton, themeButton, exportButton, settingsButton;
    juce::TextButton monitorButton;

    ml::ReadoutBand readout;
    ml::ModuleRail rail;
    ml::DetailArea detail;
    ml::TimelineStrip timeline;
    ml::AboutPanel aboutPanel;
    ml::GenrePanel genrePanel;
    ml::SettingsPanel settingsPanel;
    ml::SnapshotChip snapshotA { "A" };
    ml::SnapshotChip snapshotB { "B" };

    ml::SpectrumAnalyzer analyzer;
    ml::SpectrumFifo::Reader outputSpectrumReader;
    ml::Advisor advisor;
    ml::ReferenceSpectrum reference;
    ml::FileAnalyzer fileAnalyzer;

    std::vector<ml::FileAnalysisResult> fileResults;
    juce::String fileAnalysisStatus;
    juce::String pendingAlbumName;

    struct RawSeriesPoint
    {
        double time = 0.0;
        float momentary = 0.0f;
        float shortTerm = 0.0f;
        float integrated = 0.0f;
        float lra = 0.0f;
        float truePeak = 0.0f;
        float correlation = 0.0f;
        float width = 0.0f;
        float midShortTerm = 0.0f;
        float sideShortTerm = 0.0f;
        std::array<float, 10> bands {};
    };

    std::vector<RawSeriesPoint> rawSeries;
    double lastRawSeriesTime = -1.0;
    ml::RegionMeasurement lastRegion;

    std::vector<float> curve;
    ml::StatsSnapshot stats;
    std::vector<ml::Marker> markers;
    std::vector<ml::SignalProblem> signalProblems;
    std::array<double, 6> lastSignalProblemTime { -100.0, -100.0, -100.0, -100.0, -100.0, -100.0 };
    std::vector<ml::TpEvent> tpEvents;
    std::vector<ml::Issue> issueCache;
    ml::SignalDefects signalDefects;
    std::array<float, 4> bandRatios { 0.0f, 0.0f, 0.0f, 0.0f };

    ml::Snapshot snapshots[2];
    int viewingSnapshot = -1;
    int lastAuditionIndex = -1;

    ml::BasisMode basisMode = ml::BasisMode::platform;
    int targetGenreIndex = 16;
    float targetWarPct = 50.0f;
    ml::AlbumSnapshot albumSnapshot;
    ml::album::GateResult albumGate;
    ml::album::Diff albumDiff;
    bool albumNameEditorInitialised = false;
    juce::String lastAlbumName;

    double analyzerSampleRate = 0.0;
    bool frozen = false;
    bool holdFromKeyboard = false;
    bool updatingPreset = false;
    float uiScale = 2.0f;
    float lraTargetSetting = 8.0f;
    float clipAllowSetting = 0.0f;
    int frameCounter = 0;
    int timerTicks = 0, presentationTicks = 0;
    juce::String lastStatsSignature;
    int tick = 0;
    int selectedPage = 0;

    juce::Rectangle<int> headerArea, paramArea, footerArea, spectrumArea, readingsArea;

    juce::String footerStatusText;
    juce::Colour footerStatusColour { ml::theme::textFaint };

    std::unique_ptr<juce::FileChooser> fileChooser;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> targetAttachment, ceilingAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> monitorAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MasterLensAudioProcessorEditor)
};
