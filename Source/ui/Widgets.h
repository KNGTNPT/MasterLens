#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <functional>
#include <set>
#include <vector>

#include "../Basis.h"
#include "../Text.h"
#include "../dsp/AnalysisEngine.h"
#include "../dsp/SpectrumAnalyzer.h"
#include "Advisor.h"
#include "Localisation.h"
#include "Theme.h"

namespace ml
{
/** Two saved readings for before / after comparison. */
struct Snapshot
{
    bool valid = false;
    StatsSnapshot stats;
    double capturedAt = 0.0;
};

/** One deterministic check on the diagnostics page. */
struct CheckResult
{
    juce::String label;
    juce::String value;
    Severity severity = Severity::good;
    juce::String hint;
};

/** Builds the full checklist (passed and failed) from the current readings.

    Thresholds are the same static numbers the advisor and the delivery table
    use, so the list can always be traced back to a written rule.
*/
std::vector<CheckResult> buildChecklist (const StatsSnapshot& stats,
                                         float ceilingDb,
                                         const std::vector<float>& bandDevs,
                                         const SignalDefects& defects = {},
                                         const BasisContext& basis = {},
                                         const PlatformSummary& platform = {},
                                         bool hasTonalBasis = false);

/** Centred "play audio to start measuring" illustration + copy. */
void paintEmptyState (juce::Graphics&, juce::Rectangle<int> area);

//==============================================================================
/** 4 metric cards (I / TP / LRA / PLR), fixed 104 px, 4-up or 2x2. */
class MetricGrid : public juce::Component
{
public:
    void setValues (const StatsSnapshot& s, const BasisContext& basis,
                    const PlatformSummary& platform, float targetLufs,
                    float ceilingDb, bool signal);

    void paint (juce::Graphics&) override;

private:
    void paintCard (juce::Graphics&, juce::Rectangle<int>, const juce::String& title,
                    const juce::String& value, const juce::String& unit,
                    const juce::String& hint, const juce::String& pill, juce::Colour colour);

    StatsSnapshot stats;
    BasisContext basis;
    PlatformSummary platform;
    float target = -14.0f;
    float ceiling = -1.0f;
    bool hasSignal = false;
};

//==============================================================================
/** M / S horizontal meters with ticks, a -14 target line and tabular readouts. */
class LevelMeters : public juce::Component
{
public:
    void setValues (const StatsSnapshot& s, float targetLufs);
    void paint (juce::Graphics&) override;

private:
    void paintRow (juce::Graphics&, juce::Rectangle<int>, const juce::String& label,
                   const juce::String& windowText, float valueDb, float holdDb, juce::Colour colour);

    StatsSnapshot stats;
    float target = -14.0f;
    float holdM = -100.0f;
    float holdS = -100.0f;
};

//==============================================================================
/** Correlation / width / low-end mono, three bands and the artefact counters. */
class StereoMeters : public juce::Component
{
public:
    void setValues (const StatsSnapshot& s);
    void paint (juce::Graphics&) override;

private:
    void paintMeter (juce::Graphics&, juce::Rectangle<int>, const juce::String& label,
                     const juce::String& readout, float normalised, juce::Colour fill,
                     float marker = -1.0f, juce::Colour status = juce::Colour (0xff6c6f75));

    StatsSnapshot stats;
};

//==============================================================================
/** Real-time log-frequency spectrum with peak hold (grid cached offscreen). */
class SpectrumDisplay : public juce::Component
{
public:
    SpectrumDisplay();

    void setAnalyzer (const SpectrumAnalyzer* a) { analyzer = a; }
    void paint (juce::Graphics&) override;
    void resized() override { backgroundDirty = true; }
    void refreshLanguage() { backgroundDirty = true; repaint(); }
    void invalidateBackground() { backgroundDirty = true; repaint(); }

private:
    void ensureBackground();
    void drawGrid (juce::Graphics&);

    const SpectrumAnalyzer* analyzer = nullptr;
    juce::Image background;
    bool backgroundDirty = true;
    int cachedThemeRevision = -1;
};

/** Time / frequency waterfall and 10-band heat map (2.0 beta2).

    Columns are captured from the already-smoothed SpectrumAnalyzer at ~20 Hz.
    This is a UI-side history, so the audio thread never pays for it and the
    display does not require a second FFT.
*/
class SpectrogramDisplay : public juce::Component
{
public:
    enum class Mode { spectrogram = 0, bands = 1 };

    SpectrogramDisplay();

    void setMode (Mode m) { mode = m; backgroundDirty = true; repaint(); }
    Mode getMode() const noexcept { return mode; }

    /** Push one column from the current analyser frame. */
    void capture (const SpectrumAnalyzer& analyser);
    void clear();

    void paint (juce::Graphics&) override;
    void resized() override { backgroundDirty = true; }
    void refreshLanguage() { backgroundDirty = true; repaint(); }
    void invalidateBackground() { backgroundDirty = true; repaint(); }

private:
    static constexpr int maxColumns = 256;
    static constexpr int numBins = 64;

    void ensureBackground();
    void drawGrid (juce::Graphics&);
    static juce::Colour colourFor (float value);

    Mode mode = Mode::spectrogram;
    std::array<std::array<float, numBins>, maxColumns> columns {};
    std::array<std::array<float, 10>, maxColumns> bandColumns {};
    int writePos = 0;
    int count = 0;
    juce::Image background;
    bool backgroundDirty = true;
    int cachedThemeRevision = -1;
};

/** Ten band tonal balance deviation bars (pink-noise / octave reference). */
class BandBalanceDisplay : public juce::Component
{
public:
    BandBalanceDisplay();

    void setDeviations (std::vector<float> d) { devs = std::move (d); repaint(); }
    void paint (juce::Graphics&) override;
    void resized() override { backgroundDirty = true; }
    void refreshLanguage() { backgroundDirty = true; repaint(); }
    void invalidateBackground() { backgroundDirty = true; repaint(); }

private:
    void ensureBackground();

    std::vector<float> devs;
    juce::Image background;
    bool backgroundDirty = true;
    int cachedThemeRevision = -1;
};

//==============================================================================
/** Short-term loudness history: one relative time axis, window chips, hover. */
class HistoryGraph : public juce::Component
{
public:
    HistoryGraph();

    struct Event
    {
        float ageSeconds = 0.0f;
        int severity = 0;
    };

    void setData (std::vector<float> curve, float targetLufs);
    void setEvents (std::vector<Event> newEvents) { events = std::move (newEvents); repaint(); }
    void setBand (float lraValue) { bandLra = lraValue; repaint(); }
    void paint (juce::Graphics&) override;
    void resized() override {}
    void refreshLanguage() { backgroundDirty = true; repaint(); }
    void invalidateBackground() { backgroundDirty = true; repaint(); }

    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

    enum class Range { oneSecond = 0, tenSeconds = 1, oneMinute = 2, whole = 3 };
    Range getRange() const noexcept { return range; }

private:
    void ensureBackground();
    void paintPlot (juce::Graphics&);
    void paintOverlay (juce::Graphics&);
    juce::Rectangle<int> plotArea() const;
    std::array<juce::Rectangle<int>, 4> chipBounds() const;
    int pointsForRange() const;

    std::vector<float> data;
    std::vector<Event> events;
    float target = -14.0f;
    float bandLra = 0.0f;
    Range range = Range::oneMinute;
    juce::Image background;
    bool backgroundDirty = true;
    int cachedThemeRevision = -1;
    int hoverX = -1;

public:
    std::function<void (double)> onSeek;
};

//==============================================================================
/** Scrollable list of diagnosed issues with collapsible details. */
class IssueList : public juce::Component
{
public:
    IssueList();

    void setIssues (std::vector<Issue> items);

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Snapshot helper: opens the first row so the expanded layout can be verified. */
    void expandFirstForSnapshot();

private:
    class Content : public juce::Component
    {
    public:
        Content() { setOpaque (true); }

        void setItems (std::vector<Issue>, const std::set<juce::String>* expanded);
        int totalHeight() const noexcept;
        int rowHeightAt (int index) const noexcept;
        int rowTop (int index) const noexcept;
        int indexAtY (int y) const noexcept;
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;

        std::function<void (const juce::String&)> onRowClicked;

        static constexpr int collapsedHeight = 48;
        static constexpr int detailExtra = 60;

    private:
        std::vector<Issue> items;
        std::vector<int> rowHeights;
        const std::set<juce::String>* expandedIds = nullptr;
        int cachedThemeRevision = -1;
    };

    Content content;
    juce::Viewport viewport;
    std::set<juce::String> expanded;
    std::vector<Issue> viewIssuesCache;
    juce::String lastSignature;
};

//==============================================================================
/** A / B comparison chip. Left click captures or recalls, right click clears. */
class SnapshotChip : public juce::Component,
                     public juce::SettableTooltipClient
{
public:
    explicit SnapshotChip (juce::String letterText);

    void setState (bool hasData, bool active);
    bool hasData() const noexcept { return filled; }

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    juce::String getTooltip() override;

    std::function<void()> onClick;
    std::function<void()> onClear;

private:
    juce::String letter;
    bool filled = false;
    bool active = false;
    bool mouseOver = false;
};

//==============================================================================
/** Modal "About" overlay: product, developer, organisation, standards. */
class AboutPanel : public juce::Component
{
public:
    AboutPanel();

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void setVisibleAndFocus (bool shouldBeVisible);

    std::function<void()> onClose;

private:
    juce::TextButton closeButton;
};
//==============================================================================
/** Loudness-war explainer + target controls for the selected genre.

    Opens as a full-size overlay (same scrim pattern as AboutPanel) and shows:
    what released masters of that style measure, why, three sliders that drive
    the target loudness / LRA goal / clipping allowance, and a concrete chain
    recommendation derived from those numbers.
*/
class GenrePanel : public juce::Component
{
public:
    GenrePanel();

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

    void show (int presetIndex, float target, float lraTarget, float clipAllow);
    void setVisibleAndFocus (bool shouldBeVisible);

    std::function<void (float lraTarget, float clipAllow)> onSettingsChanged;
    std::function<void (float targetLufs)> onTargetChanged;
    std::function<void()> onClose;

private:
    void refreshTexts();
    juce::String adviceText() const;

    juce::Slider loudness, lra, clip;
    juce::Label loudnessCaption, lraCaption, clipCaption;
    juce::TextButton closeButton;

    int presetIndex = 0;
    bool updating = false;
    juce::String bodyText, factsText;
};

//==============================================================================
/** Interface settings: the whole editor is zoomed as one surface (100 - 200 %).

    Zooming scales glyphs, row heights, cards and the window together, so the
    layout keeps its proportions and no label is ever clipped by a font change.
*/
class SettingsPanel : public juce::Component
{
public:
    SettingsPanel();

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

    void setVisibleAndFocus (bool shouldBeVisible);
    void setScale (float newScale);
    float getScale() const noexcept { return scale; }
    void refreshTexts();

    std::function<void (float)> onScaleChanged;
    std::function<void()> onClose;

private:
    juce::Rectangle<int> cardBounds() const;

    float scale = 1.0f;
    std::array<juce::TextButton, 5> scaleButtons;
    juce::TextButton closeButton;
};
} // namespace ml

