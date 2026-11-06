#pragma once

#include "Layout.h"

#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <functional>
#include <vector>

#include "../dsp/AnalysisEngine.h"
#include "../dsp/DefectAnalyzer.h"
#include "../dsp/SpectrumAnalyzer.h"
#include "../Basis.h"
#include "../album/AlbumSession.h"
#include "../file/FileAnalyzer.h"
#include "../file/DeliverySpec.h"
#include "Advisor.h"
#include "Theme.h"
#include "Widgets.h"

namespace ml
{
struct Marker
{
    double time = 0.0;
    int severity = 0;      // 0 info, 1 warn, 2 over
    int category = 0;      // 0 TP, 1 loudness, 2 phase, 3 spectrum, 4 manual
    juce::String text;
};

/** A timestamped region that triggered a signal-health layer. The region is a
    fixed 2 s window around the moment the layer crossed its warning threshold;
    it is used for the explicit "loop this problem area" audition. */
struct SignalProblem
{
    double start = 0.0;
    double end = 0.0;
    int card = 0;
    int severity = 1;
    juce::String label;
};

/** Markers that happen within one short window, merged into one attribution. */
struct MergedMarkerEvent
{
    double start = 0.0;
    double end = 0.0;
    int severity = 0;
    int count = 0;
    juce::String text;       // "time  ·  root cause  ·  N events"
};

/** Clusters markers inside ±windowSeconds and attributes the group. */
std::vector<MergedMarkerEvent> mergeMarkerEvents (const std::vector<Marker>& markers,
                                                  double windowSeconds = 0.8);

/** A measured timeline region (100 ms envelope estimate). */
struct RegionMeasurement
{
    bool valid = false;
    double start = 0.0;
    double end = 0.0;
    float lufs = kSilentDb;
    float midLufs = kSilentDb;
    float sideLufs = kSilentDb;
    float truePeak = kSilentDb;
    float plr = 0.0f;
    float correlation = 0.0f;
    float widthPct = 0.0f;
    std::array<float, 10> bands {};
    juce::String name { "Region" };
};

struct UiState
{
    StatsSnapshot stats;
    std::vector<float> bandDevs;
    std::array<float, 10> bandDb {};
    std::array<float, 4> ratios { 0.0f, 0.0f, 0.0f, 0.0f };
    std::vector<SpectrumAnalyzer::Resonance> resonances;
    std::vector<TpEvent> tpEvents;
    std::vector<Marker> markers;
    std::vector<SignalProblem> signalProblems;
    SignalDefects defects;
    BasisContext basis;
    AlbumSnapshot album;
    album::GateResult albumGate;
    album::Diff albumDiff;
    PlatformSummary platform;
    int targetGenreIndex = 16;
    float targetWarPct = 50.0f;
    float targetPlanLufs = -9.5f;
    float targetPlanLra = 6.0f;
    float targetPlanCeiling = -1.0f;
    bool hasTonalBasis = false;
    StatsSnapshot referenceStats;
    bool hasReferenceStats = false;
    float target = -14.0f;
    float ceiling = -1.0f;
    bool hasReference = false;
    int signalAuditionIndex = -1;
    juce::String liveAuditionLabel;
    juce::String referenceStatus;
    const SpectrumAnalyzer* analyzer = nullptr;

    int snapshotSlot = -1;          // -1 = live, 0 = A, 1 = B
    float snapshotDelta[2] { 0.0f, 0.0f };
    bool snapshotValid[2] { false, false };
};

enum class PageId { loudness = 0, truePeak, spectrum, stereo, delivery, album, signal, diagnostics, timeline, target, files };
constexpr int numPages = 11;

juce::String signalAuditionLabel (int index);

//==============================================================================
/** Left hand module list: verdict header + one 40 px row per module. */
class ModuleRail : public juce::Component,
                   public juce::SettableTooltipClient,
                   private juce::Timer
{
public:
    ModuleRail();

    void setModule (int index, juce::String title, juce::String value,
                    juce::String status, juce::Colour colour);
    void setSelected (int index);
    int getSelected() const noexcept { return selected; }
    void setVerdict (juce::String headline, juce::String subline, juce::Colour colour);
    void beginBatch() noexcept { ++batchDepth; }
    void endBatch();

    juce::String getVerdictHeadline() const { return verdictHeadline; }
    juce::Colour getVerdictColour() const { return verdictColour; }

    std::function<void (int)> onSelect;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    juce::String getTooltip() override;

private:
    struct Item
    {
        juce::String title, value, status;
        juce::Colour colour { theme::textFaint };
    };

    juce::Rectangle<int> rowBounds (int index) const;
    void timerCallback() override;

    std::array<Item, numPages> items;
    juce::String verdictHeadline, verdictSubline;
    juce::Colour verdictColour { theme::textDim };
    int selected = 0;
    int hoverRow = -1;
    int batchDepth = 0;
    float visualSelected = 0.0f;
    float animationFrom = 0.0f;
    double animationStarted = 0.0;
    bool animationActive = false;
};

//==============================================================================
/** Song-length marker strip with a live playhead; double click adds a marker. */
class TimelineStrip : public juce::Component
{
public:
    TimelineStrip();

    void setMarkers (const std::vector<Marker>& m, double totalSeconds);
    void setPlayhead (double seconds);

    std::function<void (double)> onDoubleClick;
    std::function<void (double, double)> onSelectionChanged;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    juce::Rectangle<int> trackArea() const;
    double timeAtX (int x) const;

    std::vector<Marker> markers;
    double total = 60.0;
    double playhead = 0.0;
    double selectionStart = 0.0;
    double selectionEnd = 0.0;
    bool draggingSelection = false;
};

//==============================================================================
/** Primary content area: one page per module, plus the shared empty state. */
class DetailArea : public juce::Component
{
public:
    DetailArea();
    int getMeterPaints() const noexcept { return page == PageId::stereo ? meters.getPaintCount() : levelBars.getPaintCount(); }
    int getMeterUpdates() const noexcept { return page == PageId::stereo ? meters.getUpdateCount() : levelBars.getUpdateCount(); }

    void setPage (PageId p);
    PageId getPage() const noexcept { return page; }

    void setState (UiState s);
    void setIssues (std::vector<Issue> issues);
    void setHistory (const std::vector<float>& curve, float targetLufs, float lra);
    void setPlayhead (double seconds, double totalSeconds);
    void expandFirstIssueForSnapshot();
    void setReferenceInfo (bool hasRef, const juce::String& status);
    void setReferencePlaybackState (bool playing, const juce::String& status);
    void setReferenceSlots (const juce::StringArray& names, int active,
                            double offsetSeconds, double gainDb);
    void setSpectrumView (int view);   // 0 spectrum, 1 spectrogram, 2 band heat map
    int getSpectrumView() const noexcept { return spectrumView; }
    void setRegionMeasurement (const RegionMeasurement& r) { region = r; repaint(); }
    void refreshTexts();
    void invalidateDisplays();

    std::function<void()> onImportReference;
    std::function<void()> onClearReference;
    std::function<void()> onToggleReferencePlayback;
    std::function<void()> onAutoAlignReference;
    std::function<void(int)> onReferenceSlotChanged;
    std::function<void(double)> onReferenceOffsetChanged;
    std::function<void(double)> onReferenceGainChanged;
    std::function<void()> onExportReport;
    std::function<void()> onAddMarkerNow;
    std::function<void (double)> onAddMarkerAt;
    std::function<void (double)> onHistorySeek;
    std::function<void (double, double)> onHistorySelection;
    std::function<void()> onExportMarkers;
    std::function<void()> onImportMarkers;
    std::function<void()> onMatchSelection;
    std::function<void()> onAlbumCapture;
    std::function<void()> onAlbumExportProof;
    std::function<void()> onAlbumImport;
    std::function<void()> onAlbumClear;
    std::function<void()> onProjectMenu;
    std::function<void (int, bool)> onAlbumRuleChanged;
    std::function<void (double)> onTrackLengthChanged;
    std::function<void (int)> onAuditionSignal;
    std::function<void (int, double, double)> onAuditionSignalRegion;
    std::function<void (const juce::String&)> onAuditionRiskDetail;
    /** Spectrum band solo: low/high edge (Hz) plus a localised label. */
    std::function<void (float, float, const juce::String&)> onAuditionBand;
    std::function<void (int)> onTargetGenreChanged;
    std::function<void (float)> onTargetWarChanged;
    std::function<void()> onApplyTargetPlan;

    std::function<void()> onChooseFiles;
    std::function<void()> onChooseFolder;
    std::function<void()> onPlayFile;
    std::function<void()> onAddFileResultsToAlbum;
    std::function<void()> onClearFileResults;
    std::function<void()> onExportFileTable;
    std::function<void (const juce::String&, double)> onPreviewFile;
    std::function<void (const juce::String&)> onRevealFile;
    std::function<void (const juce::String&, bool)> onReviewEvidence;
    std::function<void()> onCodecMenu;

    void setFileResults (std::vector<FileAnalysisResult> results, bool busy,
                         int done, int total, const juce::String& status);
    /** Stores codec A/B evidence on the matching row for CSV / JSON export. */
    void setFileCodecResult (const juce::String& path, const juce::String& label,
                             float lufs, float tp, juce::int64 bytes)
    {
        for (auto& r : fileResults)
            if (r.path == path)
            {
                r.codecLabel = label;
                r.codecLufs = lufs;
                r.codecTp = tp;
                r.codecBytes = bytes;
            }

        repaint();
    }
    /** Enables the timeline "match selection" button once a region is measured. */
    void setSelectionMatchReady (bool ready) { selectionMatch.setEnabled (ready); }

    /** Codec A/B button label / availability on the file page. */
    void setCodecABState (const juce::String& label, bool enabled)
    {
        codecAB.setButtonText (ml::grid::controlText (label));
        codecAB.setEnabled (enabled);
    }

    juce::Rectangle<int> codecButtonScreenBounds() const { return codecAB.getScreenBounds(); }
    void setSpectrumSplit (int height, int bandHeight) { spectrumSplit = height; spectrumBandHeight = bandHeight; resized(); }
    void setFileView (int view) { fileView = juce::jlimit (0, 2, view); resized(); repaint(); }
    void setDeliverySpec (const DeliverySpec& s) { spec = s; repaint(); }
    void setFilePlaybackAvailable (bool available);
    void setFilePlayActive (bool active);
    /** Shows "buffering" inside the playing state for the streaming transport. */
    void setFileBuffering (bool buffering)
    {
        if (fileBuffering == buffering)
            return;

        fileBuffering = buffering;
        updateFileTransportText();
    }
    void setFileProgress (double positionSeconds, double durationSeconds);
    void setFileInspectorCollapsed (bool collapsed);
    bool isFileInspectorCollapsed() const noexcept { return fileInspectorCollapsed; }
    std::function<void (double)> onSeekFile;
    const std::vector<FileAnalysisResult>& getFileResults() const noexcept { return fileResults; }
    const FileAnalysisResult* selectedFile() const
    {
        return fileSelected >= 0 && fileSelected < (int) fileResults.size()
            ? &fileResults[(size_t) fileSelected] : nullptr;
    }

    juce::TextButton& getImportButton() noexcept { return refImport; }
    juce::TextButton& getClearButton() noexcept { return refClear; }
    juce::TextButton& getExportButton() noexcept { return reportButton; }
    juce::TextButton& getMarkButton() noexcept { return markButton; }

    juce::String getAlbumTrackLabel() const { return albumTrackName.getText().trim(); }
    void setAlbumTrackLabel (const juce::String& label) { albumTrackName.setText (label, false); }
    juce::String getAlbumName() const { return albumNameEditor.getText().trim(); }
    void setAlbumName (const juce::String& name) { albumNameEditor.setText (name, false); }
    juce::String getTrackLengthText() const { return albumTrackLength.getText().trim(); }
    void setTrackLengthText (const juce::String& text) { albumTrackLength.setText (text, false); }

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    void hideAllChildren();
    juce::Rectangle<int> contentArea() const;
    void paintPageHeader (juce::Graphics&, const juce::String& meta);
    void ensureBackground();

    void paintLoudness (juce::Graphics&);
    void paintTruePeak (juce::Graphics&);
    void paintSpectrumExtras (juce::Graphics&);
    void paintStereoExtras (juce::Graphics&);
    void paintDelivery (juce::Graphics&);
    void paintAlbum (juce::Graphics&);
    void paintSignal (juce::Graphics&);
    void paintDiagnostics (juce::Graphics&);
    void paintTimeline (juce::Graphics&);
    void paintTarget (juce::Graphics&);
    void paintFiles (juce::Graphics&);
    void paintReferenceBar (juce::Graphics&, juce::Rectangle<int>);

    PageId page = PageId::loudness;
    UiState state;
    juce::String refStatus;
    bool refReady = false;
    bool emptyMode = true;
    int tick = 0;

    MetricGrid tiles;
    LevelMeters levelBars;
    HistoryGraph history;
    SpectrumDisplay spectrum;
    SpectrogramDisplay spectrogram;
    BandBalanceDisplay bands;
    std::array<juce::TextButton, 3> spectrumViewButtons;
    int spectrumView = 0;
    RegionMeasurement region;
    StereoMeters meters;
    IssueList issues;

    juce::TextButton refImport, refClear, refPlay, refAlign, reportButton, markButton;
    juce::ComboBox refSlotBox;
    juce::TextEditor refOffset, refGain;
    bool updatingReferenceControls = false;
    bool refPlaying = false;
    juce::TextButton markerExport, markerImport;
    juce::TextButton selectionMatch;
    juce::TextButton albumCapture, albumProof, albumImport, albumClearButton, albumProject;
    juce::TextButton albumExportMarkers, albumImportMarkers;
    std::array<juce::TextButton, 5> albumRuleButtons;
    juce::TextEditor albumTrackName, albumNameEditor, albumTrackLength;

    juce::ComboBox genreBox;
    juce::Slider warSlider;
    juce::TextButton applyTarget;

    juce::TextButton fileChoose, fileFolder, filePlay, fileAddAlbum, fileClear, fileExport;
    juce::TextButton codecAB;
    juce::TextButton fileInspectorToggle;
    juce::Slider fileProgress;
    juce::Label fileTime;
    enum class FileTransportState { unavailable, unloaded, ready, playing };
    FileTransportState fileTransportState = FileTransportState::unloaded;
    bool filePlaybackAvailable = false;
    bool fileBuffering = false;
    bool fileInspectorCollapsed = false;
    bool updatingFileProgress = false;
    double fileProgressDuration = 0.0;
    double fileProgressPosition = 0.0;
    std::array<juce::TextButton, 3> fileViewButtons;
    std::vector<FileAnalysisResult> fileResults;
    DeliverySpec spec;
    juce::String fileStatus;
    bool fileBusy = false;
    int fileDone = 0, fileTotal = 0;
    int fileScroll = 0;
    int fileSortColumn = 0;
    bool fileSortAscending = true;
    int fileSelected = -1;
    int fileView = 0, fileFindingScroll = 0;
    std::vector<juce::Rectangle<int>> fileFindingRows;

    juce::Rectangle<int> fileTableBounds() const;
    void updateFileTransportText();
    void setFileTransportState (FileTransportState state);
    juce::Rectangle<int> fileInspectorBounds() const;
    void paintFileInspector (juce::Graphics&);
    bool handleFileMouseDown (const juce::MouseEvent&);
    void handleFileDoubleClick (const juce::MouseEvent&);
    void handleFileWheel (const juce::MouseWheelDetails&);

    juce::Rectangle<int> signalProblemRowBounds, signalAttributionBounds, signalGridBounds;
    juce::Rectangle<int> loudnessSideArea;
    int spectrumSplit = 0, spectrumBandHeight = 0;
    juce::Rectangle<int> spectrumSideArea;
    std::array<juce::Rectangle<int>, 4> signalChipBounds;
    std::array<juce::Rectangle<int>, 3> spectrumBandGroupBounds;

    juce::Image backgroundCache;
    bool backgroundDirty = true;
    int cachedBackgroundRevision = -1;
};
} // namespace ml
