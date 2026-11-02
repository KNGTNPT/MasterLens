#pragma once

#include <array>
#include <vector>

#include <juce_core/juce_core.h>

#include "../Basis.h"
#include "../Delivery.h"
#include "../dsp/DefectAnalyzer.h"

namespace ml::album
{
struct TrackRecord
{
    juce::String id;
    juce::String label;
    juce::String capturedAt;
    StatsSnapshot stats;
    std::array<float, 10> bandDb {};
    SignalDefects defects;
    int platformOverCount = 0;
    float platformLimitedDb = 0.0f;
    float wastedLoudnessDb = 0.0f;
    float tpCeiling = -1.0f;
    std::vector<std::pair<double, juce::String>> issues;

    // v2.0 source / provenance / coverage.
    juce::String sourceFile;
    juce::String pluginVersion;
    double durationSeconds = 0.0;
    double sampleRate = 0.0;
    int bitDepth = 0;
    float coveragePct = 100.0f;
    juce::var fileForensics; // v2.1 source SHA256 + sample-addressed offline evidence
};

struct Rules
{
    float lufsSpread = 1.5f;
    float lraSpread = 2.0f;
    float plrSpread = 2.0f;
    float tpSpread = 0.5f;
    float bandSpread = 4.0f;
    float corrSpread = 0.20f;
    float maxSignalRisk = 65.0f;
    float maxMonoLoss = 1.5f;

    bool blockPlatform = true;
    bool blockTp = true;
    bool blockSignal = true;
    bool blockMono = true;
    bool blockInconsistency = false;
};

struct Snapshot
{
    int schemaVersion = 1;
    int revision = 0;
    juce::StringArray reviewedEvidence;
    juce::String albumName { "Album" };
    juce::String projectId;
    juce::String projectVersion { "1.0" };
    juce::String updatedAt;
    juce::String hash;

    std::vector<TrackRecord> tracks;
    std::vector<TrackRecord> previousTracks;
    bool hasPrevious = false;
    juce::String previousHash;

    bool hasMedian = false;
    StatsSnapshot medianStats;
    std::array<float, 10> medianBandDb {};

    bool hasReference = false;
    juce::String referenceLabel;
    StatsSnapshot referenceStats;
    std::array<float, 10> referenceBandDb {};

    Rules rules;
};

struct Risk
{
    int severity = 1;          // 1 warn, 2 block
    float score = 0.0f;
    juce::String track;
    juce::String dimension;
    juce::String detail;
    juce::String time;
};

struct GateResult
{
    bool blocked = false;
    bool incomplete = false;
    int blockers = 0;
    std::vector<Risk> risks;
    juce::StringArray reasons;
};

struct Diff
{
    int added = 0;
    int worsened = 0;
    int improved = 0;
    juce::StringArray lines;
};

/** Shared, versioned album session stored under the user application data dir. */
class Session
{
public:
    explicit Session (bool useSharedCache = false) : sharedCache (useSharedCache) {}

    /** Reloads from disk when another instance changed the file. */
    Snapshot snapshot();

    void reload();

    /** Adds or replaces a track (by label), recomputes the median and saves. */
    void addTrack (const TrackRecord& track);
    void clearTracks();
    void setReviewed (const juce::String& evidenceId, bool reviewed);
    void setRules (const Rules& rules);
    void setAlbumName (const juce::String& name);
    void setReference (const StatsSnapshot&, const std::array<float, 10>& bands,
                       const juce::String& label);

    bool importJson (const juce::File& file);

    /** Project files keep the album snapshot, stable track IDs and a user-facing
        version label together. They are deliberately separate from album.json. */
    bool saveProject (const juce::File& file, const juce::String& versionLabel);
    bool loadProject (const juce::File& file);

    /** Project-state persistence: the host saves this string inside APVTS, while
        album.json remains the "last used" cache shared by instances. */
    juce::String serializeState() const;
    bool restoreState (const juce::String& json);

    juce::String exportProofJson() const;
    juce::String exportProofText() const;
    juce::String buildEvidenceJson (const Snapshot&, const GateResult&, const Diff&) const;

    static GateResult evaluate (const Snapshot&);
    static Diff diffVersions (const Snapshot&);

    static juce::File albumFile();

private:
    bool sharedCache = false;
    void save (Snapshot& snapshot);
    juce::String toJson (const Snapshot&, bool includeHash) const;
    bool fromJson (const juce::String&, Snapshot&) const;

    mutable juce::CriticalSection lock;
    Snapshot cached;
    juce::int64 cachedStamp = 0;
    bool loaded = false;
};

/** Stable across project revisions: derived from track label + source identity,
    not from the current measurement values. */
juce::String stableTrackId (const juce::String& label, const juce::String& source);

/** Convenience: median of a numeric vector (empty -> 0). */
float medianOf (std::vector<float> values);
} // namespace ml::album

namespace ml
{
using AlbumSnapshot = album::Snapshot;
}

