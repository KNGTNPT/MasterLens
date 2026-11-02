#pragma once

#include "dsp/AnalysisEngine.h"
#include "ui/Localisation.h"

namespace ml
{
/** How a single track is judged without a fixed genre loudness target. */
enum class BasisMode { platform = 0, reference = 1, album = 2, manual = 3 };

struct BasisContext
{
    BasisMode mode = BasisMode::platform;

    bool hasReference = false;
    StatsSnapshot reference;
    juce::String referenceLabel;

    bool hasAlbum = false;
    StatsSnapshot albumMedian;
    juce::String albumLabel;

    float manualLufs = -14.0f;

    /** The comparison snapshot actually in use, or nullptr for platform-only. */
    const StatsSnapshot* comparison() const noexcept
    {
        if (mode == BasisMode::reference && hasReference)
            return &reference;

        if (mode == BasisMode::album && hasAlbum)
            return &albumMedian;

        return nullptr;
    }

    bool hasComparison() const noexcept { return comparison() != nullptr; }

    float comparisonLufs() const noexcept
    {
        if (mode == BasisMode::manual)
            return manualLufs;

        if (auto* s = comparison())
            return s->integrated;

        return -100.0f;
    }

    float comparisonLra() const noexcept
    {
        if (auto* s = comparison())
            return s->lra;

        return 0.0f;
    }

    float comparisonPlr() const noexcept
    {
        if (auto* s = comparison())
            return s->getPlr();

        return 0.0f;
    }

    juce::String label() const
    {
        if (mode == BasisMode::reference)
            return hasReference ? T ("参考轨", "reference") : T ("平台（无参考）", "platform (no reference)");

        if (mode == BasisMode::album)
            return hasAlbum ? T ("专辑中位", "album median") : T ("平台（专辑不足）", "platform (album incomplete)");

        if (mode == BasisMode::manual)
            return T ("手动参考", "manual reference");

        return T ("平台归一化", "platform normalisation");
    }
};

/** Worst-case platform consequence across all static destinations. */
struct PlatformSummary
{
    float maxCutDb = 0.0f;       // positive number of dB the service will attenuate
    float maxBoostDb = 0.0f;     // positive number the service will raise
    float maxLimitedDb = 0.0f;   // legacy name: largest peak-constrained target shortfall
    float maxWastedDb = 0.0f;    // loudness bought that normalisation throws away
    int overCount = 0;
    int warningCount = 0;
};
} // namespace ml
