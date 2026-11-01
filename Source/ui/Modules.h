#pragma once

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
    juce::String referenceStatus;
    const SpectrumAnalyzer* analyzer = nullptr;

    int snapshotSlot = -1;          // -1 = live, 0 = A, 1 = B
    float snapshotDelta[2] { 0.0f, 0.0f };
    bool snapshotValid[2] { false, false };
};

enum class PageId { loudness = 0, truePeak, spectrum, stereo, delivery, album, signal, diagnostics, timeline, target, files };
constexpr int numPages = 11;

//==============================================================================
/** Left hand module list: verdict header + one 40 px row per module. */
class ModuleRail : public juce::Component,
                   public juce::SettableTooltipClient
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

    std::array<Item, numPages> items;
    juce::String verdictHeadline, verdictSubline;
    juce::Colour verdictColour { theme::textDim };
    int selected = 0;
    int hoverRow = -1;
    int batchDepth = 0;
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

    void setPage (PageId p);
    PageId getPage() const noexcept { return page; }

    void setState (UiState s);
    void setIssues (std::vector<Issue> issues);
    void setHistory (const std::vector<float>& curve, float targetLufs, float lra);
    void setPlayhead (double seconds, double totalSeconds);
    void expandFirstIssueForSnapshot();
    void setReferenceInfo (bool hasRef, const juce::String& status);
    void setReferencePlaybackState (bool playing, const juce::String& status);
    void setSpectrumView (int view);   // 0 spectrum, 1 spectrogram, 2 band heat map
    int getSpectrumView() const noexcept { return spectrumView; }
    void setRegionMeasurement (const RegionMeasurement& r) { region = r; repaint(); }
    void refreshTexts();
    void invalidateDisplays();

    std::function<void()> onImportReference;
    std::function<void()> onClearReference;
    std::function<void()> onToggleReferencePlayback;
    std::function<void()> onAutoAlignReference;
    std::function<void()> onExportReport;
    std::function<void()> onAddMarkerNow;
    std::function<void (double)> onAddMarkerAt;
    std::function<void (double)> onHistorySeek;
    std::function<void (double, double)> onHistorySelection;
    std::function<void()> onExportMarkers;
    std::function<void()> onImportMarkers;
    std::function<void()> onAlbumCapture;
    std::function<void()> onAlbumExportProof;
    std::function<void()> onAlbumImport;
    std::function<void()> onAlbumClear;
    std::function<void (int, bool)> onAlbumRuleChanged;
    std::function<void (double)> onTrackLengthChanged;
    std::function<void (int)> onAuditionSignal;
    std::function<void (int, double, double)> onAuditionSignalRegion;
    std::function<void (const juce::String&)> onAuditionRiskDetail;
    std::function<void (int)> onTargetGenreChanged;
    std::function<void (float)> onTargetWarChanged;
    std::function<void()> onApplyTargetPlan;

    std::function<void()> onChooseFiles;
    std::function<void()> onChooseFolder;
    std::function<void()> onAddFileResultsToAlbum;
    std::function<void()> onClearFileResults;
    std::function<void()> onExportFileTable;

    void setFileResults (std::vector<FileAnalysisResult> results, bool busy,
                         int done, int total, const juce::String& status);

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
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    void layoutForPage();
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
    bool refPlaying = false;
    juce::TextButton markerExport, markerImport;
    juce::TextButton albumCapture, albumProof, albumImport, albumClearButton;
    juce::TextButton albumExportMarkers, albumImportMarkers;
    std::array<juce::TextButton, 5> albumRuleButtons;
    juce::TextEditor albumTrackName, albumNameEditor, albumTrackLength;

    juce::ComboBox genreBox;
    juce::Slider warSlider;
    juce::TextButton applyTarget;

    juce::TextButton fileChoose, fileFolder, fileAddAlbum, fileClear, fileExport;
    std::vector<FileAnalysisResult> fileResults;
    juce::String fileStatus;
    bool fileBusy = false;
    int fileDone = 0, fileTotal = 0;
    int fileScroll = 0;
    int fileSortColumn = 0;
    bool fileSortAscending = true;
    int fileSelected = -1;

    juce::Rectangle<int> fileTableBounds() const;
    bool handleFileMouseDown (const juce::MouseEvent&);
    void handleFileWheel (const juce::MouseWheelDetails&);

    juce::Rectangle<int> signalProblemRowBounds, signalAttributionBounds, signalGridBounds;
    juce::Rectangle<int> loudnessSideArea;
    juce::Rectangle<int> spectrumSideArea;
    std::array<juce::Rectangle<int>, 4> signalChipBounds;

    juce::Image backgroundCache;
    bool backgroundDirty = true;
    int cachedBackgroundRevision = -1;
};
} // namespace ml
