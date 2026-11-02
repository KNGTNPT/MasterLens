#include "AlbumSession.h"

#include <algorithm>
#include <cmath>
#include <map>

#include <juce_cryptography/juce_cryptography.h>

#include "../Version.h"

namespace ml::album
{
namespace
{
    juce::var statsToVar (const StatsSnapshot& s)
    {
        auto* o = new juce::DynamicObject();

        o->setProperty ("integrated", s.integrated);
        o->setProperty ("momentary", s.momentary);
        o->setProperty ("shortTerm", s.shortTerm);
        o->setProperty ("maxMomentary", s.maxMomentary);
        o->setProperty ("maxShortTerm", s.maxShortTerm);
        o->setProperty ("lra", s.lra);
        o->setProperty ("truePeakDb", s.truePeakDb);
        o->setProperty ("samplePeakDb", s.samplePeakDb);
        o->setProperty ("correlation", s.correlation);
        o->setProperty ("widthPct", s.widthPct);
        o->setProperty ("lowSidePct", s.lowSidePct);
        o->setProperty ("dcOffsetDb", s.dcOffsetDb);
        o->setProperty ("clipEvents", s.clipEvents);
        o->setProperty ("clickEvents", s.clickEvents);
        o->setProperty ("oversCount", s.oversCount);
        o->setProperty ("elapsed", s.elapsed);
        o->setProperty ("stereo", s.stereo);
        o->setProperty ("hasSignal", s.hasSignal);
        o->setProperty ("psr", s.psr);
        o->setProperty ("psrMin", s.psrMin);
        o->setProperty ("psrMinTime", s.psrMinTime);
        o->setProperty ("truePeakMaxTime", s.truePeakMaxTime);
        o->setProperty ("spikeCount", s.spikeCount);
        o->setProperty ("lastSpikeTime", s.lastSpikeTime);
        o->setProperty ("bandCorrLow", s.bandCorrLow);
        o->setProperty ("bandCorrMid", s.bandCorrMid);
        o->setProperty ("bandCorrHigh", s.bandCorrHigh);
        o->setProperty ("bandSideLow", s.bandSideLow);
        o->setProperty ("bandSideMid", s.bandSideMid);
        o->setProperty ("bandSideHigh", s.bandSideHigh);
        o->setProperty ("monoFoldLossDb", s.monoFoldLossDb);
        o->setProperty ("monoRetainPct", s.monoRetainPct);
        o->setProperty ("monoLowRetainPct", s.monoLowRetainPct);
        o->setProperty ("subSharePct", s.subSharePct);
        o->setProperty ("flatTopPct", s.flatTopPct);
        o->setProperty ("midLufs", s.midLufs);
        o->setProperty ("sideLufs", s.sideLufs);
        o->setProperty ("midShortTerm", s.midShortTerm);
        o->setProperty ("sideShortTerm", s.sideShortTerm);
        o->setProperty ("timeOrigin", s.timeOrigin);
        o->setProperty ("hostPosition", s.hostPosition);
        o->setProperty ("hostLength", s.hostLength);
        o->setProperty ("coveragePct", s.coveragePct);
        o->setProperty ("truePeakLeft", s.truePeakLeft);
        o->setProperty ("truePeakRight", s.truePeakRight);
        o->setProperty ("sideTruePeak", s.sideTruePeak);

        return juce::var (o);
    }

    StatsSnapshot statsFromVar (const juce::var& v)
    {
        StatsSnapshot s;

        if (auto* o = v.getDynamicObject())
        {
            const auto get = [o] (const char* key, double fallback)
            {
                return o->hasProperty (key) ? (float) (double) o->getProperty (key)
                                             : (float) fallback;
            };

            s.integrated = get ("integrated", -100.0);
            s.momentary = get ("momentary", -100.0);
            s.shortTerm = get ("shortTerm", -100.0);
            s.maxMomentary = get ("maxMomentary", -100.0);
            s.maxShortTerm = get ("maxShortTerm", -100.0);
            s.lra = get ("lra", 0.0);
            s.truePeakDb = get ("truePeakDb", -100.0);
            s.samplePeakDb = get ("samplePeakDb", -100.0);
            s.correlation = get ("correlation", 0.0);
            s.widthPct = get ("widthPct", 0.0);
            s.lowSidePct = get ("lowSidePct", 0.0);
            s.dcOffsetDb = get ("dcOffsetDb", -100.0);
            s.clipEvents = (int) get ("clipEvents", 0.0);
            s.clickEvents = (int) get ("clickEvents", 0.0);
            s.oversCount = (int) get ("oversCount", 0.0);
            s.elapsed = get ("elapsed", 0.0);
            s.stereo = (bool) o->getProperty ("stereo");
            s.hasSignal = (bool) o->getProperty ("hasSignal");
            s.psr = get ("psr", -100.0);
            s.psrMin = get ("psrMin", -100.0);
            s.psrMinTime = get ("psrMinTime", 0.0);
            s.truePeakMaxTime = get ("truePeakMaxTime", 0.0);
            s.spikeCount = (int) get ("spikeCount", 0.0);
            s.lastSpikeTime = get ("lastSpikeTime", 0.0);
            s.bandCorrLow = get ("bandCorrLow", 0.0);
            s.bandCorrMid = get ("bandCorrMid", 0.0);
            s.bandCorrHigh = get ("bandCorrHigh", 0.0);
            s.bandSideLow = get ("bandSideLow", 0.0);
            s.bandSideMid = get ("bandSideMid", 0.0);
            s.bandSideHigh = get ("bandSideHigh", 0.0);
            s.monoFoldLossDb = get ("monoFoldLossDb", 0.0);
            s.monoRetainPct = get ("monoRetainPct", 100.0);
            s.monoLowRetainPct = get ("monoLowRetainPct", 100.0);
            s.subSharePct = get ("subSharePct", 0.0);
            s.flatTopPct = get ("flatTopPct", 0.0);
            s.midLufs = get ("midLufs", -100.0);
            s.sideLufs = get ("sideLufs", -100.0);
            s.midShortTerm = get ("midShortTerm", -100.0);
            s.sideShortTerm = get ("sideShortTerm", -100.0);
            s.timeOrigin = get ("timeOrigin", 0.0);
            s.hostPosition = get ("hostPosition", 0.0);
            s.hostLength = get ("hostLength", 0.0);
            s.coveragePct = get ("coveragePct", 100.0);
            s.truePeakLeft = get ("truePeakLeft", -100.0);
            s.truePeakRight = get ("truePeakRight", -100.0);
            s.sideTruePeak = get ("sideTruePeak", -100.0);
        }

        return s;
    }
} // namespace

float medianOf (std::vector<float> values)
{
    if (values.empty())
        return 0.0f;

    std::sort (values.begin(), values.end());
    const size_t n = values.size();

    if ((n & 1u) != 0u)
        return values[n / 2];

    return 0.5f * (values[n / 2 - 1] + values[n / 2]);
}

namespace
{
    juce::var defectsToVar (const SignalDefects& d)
    {
        auto* o = new juce::DynamicObject();

        o->setProperty ("ready", d.ready);
        o->setProperty ("harmonicExcessDb", d.harmonicExcessDb);
        o->setProperty ("imdSidebandDb", d.imdSidebandDb);
        o->setProperty ("flatTopPct", d.flatTopPct);
        o->setProperty ("distortionRisk", d.distortionRisk);
        o->setProperty ("hfTiltDb", d.hfTiltDb);
        o->setProperty ("aliasExcessDb", d.aliasExcessDb);
        o->setProperty ("aliasRisk", d.aliasRisk);
        o->setProperty ("subSharePct", d.subSharePct);
        o->setProperty ("subTiltDb", d.subTiltDb);
        o->setProperty ("subRisk", d.subRisk);
        o->setProperty ("transientScore", d.transientScore);
        o->setProperty ("crestLossDb", d.crestLossDb);
        o->setProperty ("monoFoldLossDb", d.monoFoldLossDb);
        o->setProperty ("monoRetainPct", d.monoRetainPct);
        o->setProperty ("monoLowRetainPct", d.monoLowRetainPct);
        o->setProperty ("monoRisk", d.monoRisk);
        o->setProperty ("maskingDb", d.maskingDb);
        o->setProperty ("maskingRisk", d.maskingRisk);
        o->setProperty ("attributionCount", d.attributionCount);

        return juce::var (o);
    }

    SignalDefects defectsFromVar (const juce::var& v)
    {
        SignalDefects d;

        if (auto* o = v.getDynamicObject())
        {
            const auto get = [o] (const char* key, double fallback)
            {
                return o->hasProperty (key) ? (float) (double) o->getProperty (key)
                                             : (float) fallback;
            };

            d.ready = (bool) o->getProperty ("ready");
            d.harmonicExcessDb = get ("harmonicExcessDb", 0.0);
            d.imdSidebandDb = get ("imdSidebandDb", 0.0);
            d.flatTopPct = get ("flatTopPct", 0.0);
            d.distortionRisk = get ("distortionRisk", 0.0);
            d.hfTiltDb = get ("hfTiltDb", 0.0);
            d.aliasExcessDb = get ("aliasExcessDb", 0.0);
            d.aliasRisk = get ("aliasRisk", 0.0);
            d.subSharePct = get ("subSharePct", 0.0);
            d.subTiltDb = get ("subTiltDb", 0.0);
            d.subRisk = get ("subRisk", 0.0);
            d.transientScore = get ("transientScore", 100.0);
            d.crestLossDb = get ("crestLossDb", 0.0);
            d.monoFoldLossDb = get ("monoFoldLossDb", 0.0);
            d.monoRetainPct = get ("monoRetainPct", 100.0);
            d.monoLowRetainPct = get ("monoLowRetainPct", 100.0);
            d.monoRisk = get ("monoRisk", 0.0);
            d.maskingDb = get ("maskingDb", 0.0);
            d.maskingRisk = get ("maskingRisk", 0.0);
            d.attributionCount = (int) get ("attributionCount", 0.0);
        }

        return d;
    }

    juce::var trackToVar (const TrackRecord& t)
    {
        auto* o = new juce::DynamicObject();

        o->setProperty ("id", t.id);
        o->setProperty ("label", t.label);
        o->setProperty ("capturedAt", t.capturedAt);
        o->setProperty ("stats", statsToVar (t.stats));

        juce::Array<juce::var> bands;

        for (float b : t.bandDb)
            bands.add (b);

        o->setProperty ("bandDb", bands);
        o->setProperty ("defects", defectsToVar (t.defects));
        o->setProperty ("platformOverCount", t.platformOverCount);
        o->setProperty ("platformLimitedDb", t.platformLimitedDb);
        o->setProperty ("wastedLoudnessDb", t.wastedLoudnessDb);
        o->setProperty ("tpCeiling", t.tpCeiling);
        o->setProperty ("sourceFile", t.sourceFile);
        o->setProperty ("pluginVersion", t.pluginVersion);
        o->setProperty ("durationSeconds", t.durationSeconds);
        o->setProperty ("sampleRate", t.sampleRate);
        o->setProperty ("bitDepth", t.bitDepth);
        o->setProperty ("coveragePct", t.coveragePct);
        o->setProperty ("fileForensics", t.fileForensics);

        juce::Array<juce::var> issueArray;

        for (const auto& issue : t.issues)
        {
            auto* i = new juce::DynamicObject();
            i->setProperty ("time", issue.first);
            i->setProperty ("text", issue.second);
            issueArray.add (juce::var (i));
        }

        o->setProperty ("issues", issueArray);
        return juce::var (o);
    }

    TrackRecord trackFromVar (const juce::var& v)
    {
        TrackRecord t;

        if (auto* o = v.getDynamicObject())
        {
            t.id = o->getProperty ("id").toString();
            t.label = o->getProperty ("label").toString();
            t.capturedAt = o->getProperty ("capturedAt").toString();
            t.stats = statsFromVar (o->getProperty ("stats"));

            if (auto* bands = o->getProperty ("bandDb").getArray())
                for (int i = 0; i < juce::jmin (10, bands->size()); ++i)
                    t.bandDb[(size_t) i] = (float) (double) bands->getUnchecked (i);

            t.defects = defectsFromVar (o->getProperty ("defects"));
            t.platformOverCount = (int) o->getProperty ("platformOverCount");
            t.platformLimitedDb = (float) (double) o->getProperty ("platformLimitedDb");
            t.wastedLoudnessDb = (float) (double) o->getProperty ("wastedLoudnessDb");
            t.tpCeiling = (float) (double) o->getProperty ("tpCeiling");
            t.sourceFile = o->getProperty ("sourceFile").toString();
            t.pluginVersion = o->getProperty ("pluginVersion").toString();
            t.durationSeconds = (double) o->getProperty ("durationSeconds");
            t.sampleRate = (double) o->getProperty ("sampleRate");
            t.bitDepth = (int) o->getProperty ("bitDepth");
            t.coveragePct = (float) (double) o->getProperty ("coveragePct");
            t.fileForensics = o->getProperty ("fileForensics");

            if (auto* issues = o->getProperty ("issues").getArray())
                for (const auto& item : *issues)
                    if (auto* io = item.getDynamicObject())
                        t.issues.emplace_back ((double) io->getProperty ("time"),
                                               io->getProperty ("text").toString());
        }

        return t;
    }

    juce::var rulesToVar (const Rules& r)
    {
        auto* o = new juce::DynamicObject();

        o->setProperty ("lufsSpread", r.lufsSpread);
        o->setProperty ("lraSpread", r.lraSpread);
        o->setProperty ("plrSpread", r.plrSpread);
        o->setProperty ("tpSpread", r.tpSpread);
        o->setProperty ("bandSpread", r.bandSpread);
        o->setProperty ("corrSpread", r.corrSpread);
        o->setProperty ("maxSignalRisk", r.maxSignalRisk);
        o->setProperty ("maxMonoLoss", r.maxMonoLoss);
        o->setProperty ("blockPlatform", r.blockPlatform);
        o->setProperty ("blockTp", r.blockTp);
        o->setProperty ("blockSignal", r.blockSignal);
        o->setProperty ("blockMono", r.blockMono);
        o->setProperty ("blockInconsistency", r.blockInconsistency);

        return juce::var (o);
    }

    Rules rulesFromVar (const juce::var& v)
    {
        Rules r;

        if (auto* o = v.getDynamicObject())
        {
            const auto get = [o] (const char* key, double fallback)
            {
                return o->hasProperty (key) ? (float) (double) o->getProperty (key)
                                             : (float) fallback;
            };

            r.lufsSpread = get ("lufsSpread", 1.5);
            r.lraSpread = get ("lraSpread", 2.0);
            r.plrSpread = get ("plrSpread", 2.0);
            r.tpSpread = get ("tpSpread", 0.5);
            r.bandSpread = get ("bandSpread", 4.0);
            r.corrSpread = get ("corrSpread", 0.20);
            r.maxSignalRisk = get ("maxSignalRisk", 65.0);
            r.maxMonoLoss = get ("maxMonoLoss", 1.5);
            r.blockPlatform = (bool) o->getProperty ("blockPlatform");
            r.blockTp = (bool) o->getProperty ("blockTp");
            r.blockSignal = (bool) o->getProperty ("blockSignal");
            r.blockMono = (bool) o->getProperty ("blockMono");
            r.blockInconsistency = (bool) o->getProperty ("blockInconsistency");
        }

        return r;
    }

    juce::String formatSeconds (double seconds)
    {
        const int total = juce::jmax (0, (int) std::floor (seconds));
        return juce::String (total / 60).paddedLeft ('0', 2) + ":"
             + juce::String (total % 60).paddedLeft ('0', 2);
    }

    void deriveMedian (Snapshot& s)
    {
        s.hasMedian = s.tracks.size() >= 2;

        if (! s.hasMedian)
            return;

        std::vector<float> lufs, lra, tp, corr, width, lowSide, sub, mono, psr;

        for (const auto& t : s.tracks)
        {
            lufs.push_back (t.stats.integrated);
            lra.push_back (t.stats.lra);
            tp.push_back (t.stats.truePeakDb);
            corr.push_back (t.stats.correlation);
            width.push_back (t.stats.widthPct);
            lowSide.push_back (t.stats.lowSidePct);
            sub.push_back (t.stats.subSharePct);
            mono.push_back (t.stats.monoFoldLossDb);
            psr.push_back (t.stats.psr);
        }

        s.medianStats = StatsSnapshot {};
        s.medianStats.integrated = medianOf (lufs);
        s.medianStats.lra = medianOf (lra);
        s.medianStats.truePeakDb = medianOf (tp);
        s.medianStats.correlation = medianOf (corr);
        s.medianStats.widthPct = medianOf (width);
        s.medianStats.lowSidePct = medianOf (lowSide);
        s.medianStats.subSharePct = medianOf (sub);
        s.medianStats.monoFoldLossDb = medianOf (mono);
        s.medianStats.psr = medianOf (psr);
        s.medianStats.hasSignal = true;
        s.medianStats.stereo = true;

        for (int b = 0; b < 10; ++b)
        {
            std::vector<float> band;

            for (const auto& t : s.tracks)
                band.push_back (t.bandDb[(size_t) b]);

            s.medianBandDb[(size_t) b] = medianOf (band);
        }
    }
} // namespace

juce::File Session::albumFile()
{
    const auto overridePath = juce::SystemStats::getEnvironmentVariable ("MASTERLENS_ALBUM_FILE", {});

    if (overridePath.isNotEmpty())
    {
        juce::File file (overridePath);
        file.getParentDirectory().createDirectory();
        return file;
    }

    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("BerryStudio")
                   .getChildFile ("MasterLens");
    dir.createDirectory();
    return dir.getChildFile ("album.json");
}

juce::String Session::toJson (const Snapshot& s, bool includeHash) const
{
    auto* root = new juce::DynamicObject();

    root->setProperty ("schemaVersion", s.schemaVersion);
    root->setProperty ("revision", s.revision);
    root->setProperty ("reviewedEvidence", juce::var (s.reviewedEvidence));
    root->setProperty ("albumName", s.albumName);
    root->setProperty ("projectId", s.projectId);
    root->setProperty ("projectVersion", s.projectVersion);
    root->setProperty ("updatedAt", s.updatedAt);
    root->setProperty ("rules", rulesToVar (s.rules));

    juce::Array<juce::var> tracks;

    for (const auto& t : s.tracks)
        tracks.add (trackToVar (t));

    root->setProperty ("tracks", tracks);

    juce::Array<juce::var> previous;

    for (const auto& t : s.previousTracks)
        previous.add (trackToVar (t));

    root->setProperty ("previousTracks", previous);
    root->setProperty ("hasPrevious", s.hasPrevious);
    root->setProperty ("previousHash", s.previousHash);
    root->setProperty ("hasMedian", s.hasMedian);
    root->setProperty ("medianStats", statsToVar (s.medianStats));

    juce::Array<juce::var> medianBands;

    for (float b : s.medianBandDb)
        medianBands.add (b);

    root->setProperty ("medianBandDb", medianBands);
    root->setProperty ("hasReference", s.hasReference);
    root->setProperty ("referenceLabel", s.referenceLabel);
    root->setProperty ("referenceStats", statsToVar (s.referenceStats));

    juce::Array<juce::var> referenceBands;

    for (float b : s.referenceBandDb)
        referenceBands.add (b);

    root->setProperty ("referenceBandDb", referenceBands);

    if (includeHash)
        root->setProperty ("hash", s.hash);

    return juce::JSON::toString (juce::var (root), false);
}

bool Session::fromJson (const juce::String& text, Snapshot& s) const
{
    const auto parsed = juce::JSON::parse (text);
    auto* root = parsed.getDynamicObject();

    if (root == nullptr)
        return false;

    s = Snapshot {};
    s.schemaVersion = (int) root->getProperty ("schemaVersion");
    s.revision = (int) root->getProperty ("revision");
    if (auto* reviews = root->getProperty ("reviewedEvidence").getArray())
        for (const auto& v : *reviews) s.reviewedEvidence.addIfNotAlreadyThere (v.toString());
    s.albumName = root->getProperty ("albumName").toString();
    s.projectId = root->getProperty ("projectId").toString();
    s.projectVersion = root->getProperty ("projectVersion").toString();
    s.updatedAt = root->getProperty ("updatedAt").toString();
    s.hash = root->getProperty ("hash").toString();
    s.rules = rulesFromVar (root->getProperty ("rules"));

    if (auto* tracks = root->getProperty ("tracks").getArray())
        for (const auto& item : *tracks)
            s.tracks.push_back (trackFromVar (item));

    if (auto* previous = root->getProperty ("previousTracks").getArray())
        for (const auto& item : *previous)
            s.previousTracks.push_back (trackFromVar (item));

    s.hasPrevious = (bool) root->getProperty ("hasPrevious");
    s.previousHash = root->getProperty ("previousHash").toString();
    s.hasMedian = (bool) root->getProperty ("hasMedian");
    s.medianStats = statsFromVar (root->getProperty ("medianStats"));

    if (auto* bands = root->getProperty ("medianBandDb").getArray())
        for (int i = 0; i < juce::jmin (10, bands->size()); ++i)
            s.medianBandDb[(size_t) i] = (float) (double) bands->getUnchecked (i);

    s.hasReference = (bool) root->getProperty ("hasReference");
    s.referenceLabel = root->getProperty ("referenceLabel").toString();
    s.referenceStats = statsFromVar (root->getProperty ("referenceStats"));

    if (auto* bands = root->getProperty ("referenceBandDb").getArray())
        for (int i = 0; i < juce::jmin (10, bands->size()); ++i)
            s.referenceBandDb[(size_t) i] = (float) (double) bands->getUnchecked (i);

    return true;
}

void Session::reload()
{
    const juce::ScopedLock sl (lock);
    if (! sharedCache) { loaded = true; return; }
    const auto file = albumFile();
    const auto stamp = file.existsAsFile() ? file.getLastModificationTime().toMilliseconds() : 0;

    if (! loaded || stamp != cachedStamp)
    {
        if (file.existsAsFile())
        {
            if (! fromJson (file.loadFileAsString(), cached))
                cached = Snapshot {};
        }
        else
        {
            cached = Snapshot {};
        }

        cachedStamp = stamp;
        loaded = true;
    }
}

Snapshot Session::snapshot()
{
    reload();

    const juce::ScopedLock sl (lock);
    return cached;
}

void Session::save (Snapshot& s)
{
    if (s.albumName.isEmpty())
        s.albumName = "Album";

    deriveMedian (s);
    s.updatedAt = juce::Time::getCurrentTime().toISO8601 (true);

    const auto payload = toJson (s, false);
    s.hash = juce::SHA256 (payload.toRawUTF8(),
                           (size_t) payload.getNumBytesAsUTF8()).toHexString();

    const auto file = sharedCache ? albumFile() : juce::File {};
    if (sharedCache) file.replaceWithText (toJson (s, true));

    // Named sidecar for backups / project hand-off. The global album.json stays
    // the "most recent" cache, exactly as the 1.5 session did.
    auto safeName = s.albumName.retainCharacters ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_ ");
    safeName = safeName.trim().replaceCharacter (' ', '_');

    if (sharedCache && safeName.isNotEmpty())
        file.getSiblingFile ("Album_" + safeName + ".json").replaceWithText (toJson (s, true));

    const juce::ScopedLock sl (lock);
    cached = s;
    cachedStamp = file.existsAsFile() ? file.getLastModificationTime().toMilliseconds() : 0;
    loaded = true;
}

void Session::addTrack (const TrackRecord& track)
{
    auto s = snapshot();

    s.previousTracks = s.tracks;
    s.hasPrevious = ! s.tracks.empty();
    s.previousHash = s.hash;

    bool replaced = false;

    for (auto& t : s.tracks)
    {
        if (t.label.equalsIgnoreCase (track.label))
        {
            t = track;
            replaced = true;
            break;
        }
    }

    if (! replaced)
        s.tracks.push_back (track);

    ++s.revision;
    save (s);
}

void Session::setReviewed (const juce::String& id, bool reviewed)
{
    if (id.isEmpty()) return;
    auto s = snapshot();
    if (reviewed) s.reviewedEvidence.addIfNotAlreadyThere (id); else s.reviewedEvidence.removeString (id);
    for (auto& track : s.tracks)
    {
        // Deep-copy before editing: a previous revision must remain immutable.
        auto forensic = juce::JSON::parse (juce::JSON::toString (track.fileForensics));
        if (auto* events = forensic["findings"].getArray())
            for (auto& event : *events)
                if (event["evidenceId"].toString() == id)
                    if (auto* object = event.getDynamicObject()) object->setProperty ("reviewed", reviewed);
        track.fileForensics = forensic;
    }
    ++s.revision; save (s);
}

void Session::clearTracks()
{
    auto s = snapshot();
    s.previousTracks = s.tracks;
    s.hasPrevious = ! s.tracks.empty();
    s.previousHash = s.hash;
    s.tracks.clear();
    s.hasMedian = false;
    ++s.revision;
    save (s);
}

void Session::setRules (const Rules& rules)
{
    auto s = snapshot();
    s.rules = rules;
    ++s.revision;
    save (s);
}

void Session::setAlbumName (const juce::String& name)
{
    if (name.isEmpty())
        return;

    auto s = snapshot();
    s.albumName = name;
    ++s.revision;
    save (s);
}

void Session::setReference (const StatsSnapshot& stats, const std::array<float, 10>& bands,
                            const juce::String& label)
{
    auto s = snapshot();
    s.hasReference = true;
    s.referenceStats = stats;
    s.referenceBandDb = bands;
    s.referenceLabel = label;
    save (s);
}

bool Session::importJson (const juce::File& file)
{
    if (! file.existsAsFile())
        return false;

    Snapshot imported;

    if (! fromJson (file.loadFileAsString(), imported))
        return false;

    ++imported.revision;
    save (imported);
    return true;
}

bool Session::saveProject (const juce::File& file, const juce::String& versionLabel)
{
    auto s = snapshot();

    if (s.projectId.isEmpty())
    {
        const auto seed = s.albumName + "|"
                          + (s.tracks.empty() ? juce::String ("empty") : s.tracks.front().id);
        s.projectId = "prj-" + juce::SHA256 (seed.toRawUTF8(),
                                             (size_t) seed.getNumBytesAsUTF8()).toHexString().substring (0, 16);
    }

    s.projectVersion = versionLabel.isNotEmpty() ? versionLabel : "1.0";
    s.updatedAt = juce::Time::getCurrentTime().toISO8601 (true);

    save (s);
    const auto json = toJson (s, true);
    juce::TemporaryFile temp (file);
    return temp.getFile().replaceWithText (json) && temp.overwriteTargetFileWithTemporary();
}

bool Session::loadProject (const juce::File& file)
{
    if (! file.existsAsFile())
        return false;

    Snapshot imported;

    if (! fromJson (file.loadFileAsString(), imported))
        return false;

    ++imported.revision;
    save (imported);
    return true;
}

GateResult Session::evaluate (const Snapshot& input)
{
    Snapshot s = input;
    deriveMedian (s);

    GateResult gate;

    if (s.tracks.empty())
    {
        gate.incomplete = true;
        gate.reasons.add (T ("尚未采集曲目；交付门禁未建立", "No tracks captured; the release gate is not established"));
        return gate;
    }

    bool anyValid = false;

    for (const auto& t : s.tracks)
        if (t.stats.hasSignal && t.stats.integrated > -70.0f)
            anyValid = true;

    if (! anyValid)
    {
        gate.incomplete = true;
        gate.reasons.add (T ("采集数据无效；请播放后重新采集", "Captured data is invalid; play audio and capture again"));
        return gate;
    }

    for (const auto& t : s.tracks)
    {
        const auto add = [&] (int severity, float score, const juce::String& dimension,
                              const juce::String& detail)
        {
            Risk r;
            r.severity = severity;
            r.score = score;
            r.track = t.label;
            r.dimension = dimension;
            r.detail = detail;
            r.time = t.issues.empty() ? formatSeconds (t.stats.truePeakMaxTime)
                                      : formatSeconds (t.issues.back().first);
            gate.risks.push_back (std::move (r));
        };

        if (t.durationSeconds > 0.0 && t.coveragePct > 0.0f && t.coveragePct < 95.0f)
            add (2, 120.0f + (95.0f - t.coveragePct),
                 T ("测量覆盖度", "measurement coverage"),
                 T ("仅覆盖 ", "only ") + juce::String (t.coveragePct, 1)
                     + T ("% 曲目；未覆盖全程不允许通过", "% of the track; no pass without full coverage"));

        if (t.platformOverCount > 0 || t.platformLimitedDb > 0.05f)
            add (s.rules.blockPlatform ? 2 : 1,
                 100.0f + t.platformLimitedDb * 12.0f + (float) t.platformOverCount * 5.0f,
                 T ("平台归一化", "platform normalisation"),
                 T ("情景目标尚差 ", "scenario target shortfall ")
                     + juce::String (t.platformLimitedDb, 1) + T (" LU；", " LU; ")
                     + T ("被丢弃 ", "wasted ") + juce::String (t.wastedLoudnessDb, 1) + " LU");

        const float ceiling = t.tpCeiling <= -90.0f ? -1.0f : t.tpCeiling;

        if (t.stats.truePeakDb > ceiling + 0.05f)
            add (s.rules.blockTp ? 2 : 1,
                 100.0f + (t.stats.truePeakDb - ceiling) * 20.0f,
                 T ("真峰值 / 编码", "true peak / codec"),
                 T ("TP ", "TP ") + juce::String (t.stats.truePeakDb, 1)
                     + T (" dBTP，超过 ", " dBTP over ") + juce::String (ceiling, 1) + " dBTP");
        else if (t.stats.truePeakDb > -1.0f)
            add (1, 40.0f + (t.stats.truePeakDb + 1.0f) * 20.0f,
                 T ("编码余量", "codec headroom"),
                 T ("TP ", "TP ") + juce::String (t.stats.truePeakDb, 1)
                     + T (" dBTP，距有损编码建议值不足 1 dB", " dBTP is within 1 dB of the lossy-codec recommendation"));

        const float signalRisk = t.defects.worstRisk();

        if (signalRisk >= s.rules.maxSignalRisk)
            add (s.rules.blockSignal ? 2 : 1, 100.0f + signalRisk,
                 T ("信号缺陷", "signal integrity"),
                 T ("最高风险 ", "max risk ") + juce::String (juce::roundToInt (signalRisk)));

        if (auto* qc = t.fileForensics.getDynamicObject())
        {
            const int count = (int) qc->getProperty ("blockers");
            if (count > 0)
                add (s.rules.blockSignal ? 2 : 1, 160.0f + count,
                     T ("文件样本 QC", "file sample QC"),
                     juce::String (count) + T (" 类阻断证据；详见文件 QC / JSON", " blocker classes; see File QC / JSON"));
        }

        if (t.stats.monoFoldLossDb >= s.rules.maxMonoLoss)
            add (s.rules.blockMono ? 2 : 1, 100.0f + t.stats.monoFoldLossDb * 10.0f,
                 T ("单声道折叠", "mono fold"),
                 T ("损失 ", "loss ") + juce::String (t.stats.monoFoldLossDb, 1)
                     + T (" dB，保留 ", " dB, retained ") + juce::String (t.stats.monoRetainPct, 1) + "%");

        if (s.hasMedian)
        {
            const float dLufs = std::abs (t.stats.integrated - s.medianStats.integrated);
            const float dLra = std::abs (t.stats.lra - s.medianStats.lra);
            const float dPlr = std::abs (t.stats.getPlr() - s.medianStats.getPlr());
            const float dTp = std::abs (t.stats.truePeakDb - s.medianStats.truePeakDb);

            float worstBand = 0.0f;
            int worstBandIndex = 0;

            for (int b = 0; b < 10; ++b)
            {
                const float d = std::abs (t.bandDb[(size_t) b] - s.medianBandDb[(size_t) b]);

                if (d > worstBand)
                {
                    worstBand = d;
                    worstBandIndex = b;
                }
            }

            const float dCorr = std::abs (t.stats.correlation - s.medianStats.correlation);

            if (dLufs > s.rules.lufsSpread || dLra > s.rules.lraSpread
                || dPlr > s.rules.plrSpread || dTp > s.rules.tpSpread
                || worstBand > s.rules.bandSpread || dCorr > s.rules.corrSpread)
            {
                juce::StringArray parts;

                if (dLufs > s.rules.lufsSpread)
                    parts.add ("LUFS " + juce::String (t.stats.integrated - s.medianStats.integrated, 1));

                if (dLra > s.rules.lraSpread)
                    parts.add ("LRA " + juce::String (t.stats.lra - s.medianStats.lra, 1));

                if (dPlr > s.rules.plrSpread)
                    parts.add ("PLR " + juce::String (t.stats.getPlr() - s.medianStats.getPlr(), 1));

                if (dTp > s.rules.tpSpread)
                    parts.add ("TP " + juce::String (t.stats.truePeakDb - s.medianStats.truePeakDb, 1));

                if (worstBand > s.rules.bandSpread)
                    parts.add (T ("频段 ", "band ") + juce::String (worstBandIndex + 1)
                               + " " + juce::String (t.bandDb[(size_t) worstBandIndex]
                                                     - s.medianBandDb[(size_t) worstBandIndex], 1) + " dB");

                if (dCorr > s.rules.corrSpread)
                    parts.add ("corr " + juce::String (t.stats.correlation - s.medianStats.correlation, 2));

                add (s.rules.blockInconsistency ? 2 : 1,
                     80.0f + dLufs * 8.0f + worstBand * 2.0f,
                     T ("专辑中位偏离", "album-median deviation"),
                     parts.joinIntoString ("  "));
            }
        }
    }

    std::sort (gate.risks.begin(), gate.risks.end(), [] (const Risk& a, const Risk& b)
    {
        if (a.severity != b.severity)
            return a.severity > b.severity;

        return a.score > b.score;
    });

    for (const auto& r : gate.risks)
    {
        if (r.severity < 2)
            continue;

        ++gate.blockers;
        gate.reasons.add (r.track + " · " + r.dimension + " · " + r.detail);
    }

    gate.blocked = gate.blockers > 0;

    if (! gate.blocked)
        gate.reasons.add (gate.risks.empty() ? T ("无阻断项", "no blockers")
                                             : T ("仅有警告，未触发阻断规则", "warnings only, no blocking rule fired"));

    return gate;
}

namespace
{
    float trackRiskScore (const TrackRecord& t)
    {
        const float tpExcess = juce::jmax (0.0f, t.stats.truePeakDb + 1.0f);
        return juce::jmax (juce::jmax (t.defects.worstRisk(),
                                       (float) t.platformOverCount * 30.0f + t.platformLimitedDb * 12.0f),
                           juce::jmax (tpExcess * 20.0f, t.stats.monoFoldLossDb * 15.0f));
    }
} // namespace

Diff Session::diffVersions (const Snapshot& s)
{
    Diff d;

    if (! s.hasPrevious || s.previousTracks.empty())
    {
        d.added = (int) s.tracks.size();

        for (const auto& t : s.tracks)
            d.lines.add (T ("新增 ", "new ") + t.label);

        if (d.added == 0)
            d.lines.add (T ("首版基准，无对比", "first baseline, nothing to compare"));

        return d;
    }

    std::map<juce::String, const TrackRecord*> previous;

    for (const auto& t : s.previousTracks)
        previous[t.label.toLowerCase()] = &t;

    for (const auto& t : s.tracks)
    {
        auto it = previous.find (t.label.toLowerCase());

        if (it == previous.end())
        {
            ++d.added;
            d.lines.add (T ("新增 ", "new ") + t.label);
            continue;
        }

        const float before = trackRiskScore (*it->second);
        const float now = trackRiskScore (t);

        if (now > before + 3.0f)
        {
            ++d.worsened;
            d.lines.add (T ("恶化 ", "worse ") + t.label + "  "
                         + juce::String (before, 0) + " → " + juce::String (now, 0));
        }
        else if (now < before - 3.0f)
        {
            ++d.improved;
            d.lines.add (T ("改善 ", "better ") + t.label + "  "
                         + juce::String (before, 0) + " → " + juce::String (now, 0));
        }
    }

    if (d.lines.isEmpty())
        d.lines.add (T ("与上一版相比无新增或恶化项", "no new or worsened item since the previous revision"));

    return d;
}

juce::String Session::buildEvidenceJson (const Snapshot& s, const GateResult& gate,
                                         const Diff& diff) const
{
    auto rootVar = juce::var (new juce::DynamicObject());
    auto* root = rootVar.getDynamicObject();

    root->setProperty ("product", "MasterLens");
    root->setProperty ("version", kVersion);
    root->setProperty ("schemaVersion", s.schemaVersion);
    root->setProperty ("revision", s.revision);
    root->setProperty ("reviewedEvidence", juce::var (s.reviewedEvidence));
    root->setProperty ("albumName", s.albumName);
    root->setProperty ("snapshotHash", s.hash);
    root->setProperty ("snapshot", juce::JSON::parse (toJson (s, true)));
    root->setProperty ("rules", rulesToVar (s.rules));

    auto* g = new juce::DynamicObject();
    g->setProperty ("blocked", gate.blocked);
    g->setProperty ("incomplete", gate.incomplete);
    g->setProperty ("blockers", gate.blockers);

    juce::Array<juce::var> reasons;

    for (const auto& r : gate.reasons)
        reasons.add (r);

    g->setProperty ("reasons", reasons);

    juce::Array<juce::var> risks;

    for (const auto& r : gate.risks)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("severity", r.severity);
        o->setProperty ("score", r.score);
        o->setProperty ("track", r.track);
        o->setProperty ("dimension", r.dimension);
        o->setProperty ("detail", r.detail);
        o->setProperty ("time", r.time);
        risks.add (juce::var (o));
    }

    g->setProperty ("risks", risks);
    root->setProperty ("gate", g);

    auto* dv = new juce::DynamicObject();
    dv->setProperty ("added", diff.added);
    dv->setProperty ("worsened", diff.worsened);
    dv->setProperty ("improved", diff.improved);

    juce::Array<juce::var> lines;

    for (const auto& l : diff.lines)
        lines.add (l);

    dv->setProperty ("lines", lines);
    root->setProperty ("diff", dv);

    const auto canonical = juce::JSON::toString (rootVar, false);
    const auto evidenceHash = juce::SHA256 (canonical.toRawUTF8(),
                                            (size_t) canonical.getNumBytesAsUTF8()).toHexString();
    root->setProperty ("generatedAt", juce::Time::getCurrentTime().toISO8601 (true));
    root->setProperty ("evidenceHash", evidenceHash);

    return juce::JSON::toString (rootVar, false);
}

juce::String Session::serializeState() const
{
    const auto s = const_cast<Session*> (this)->snapshot();
    return toJson (s, true);
}

bool Session::restoreState (const juce::String& json)
{
    Snapshot restored;

    if (! fromJson (json, restored))
        return false;

    save (restored);
    return true;
}

juce::String Session::exportProofJson() const
{
    const auto s = const_cast<Session*> (this)->snapshot();
    const auto gate = evaluate (s);
    const auto diff = diffVersions (s);
    return buildEvidenceJson (s, gate, diff);
}

juce::String Session::exportProofText() const
{
    const auto s = const_cast<Session*> (this)->snapshot();
    const auto gate = evaluate (s);
    const auto diff = diffVersions (s);

    juce::String text;

    text << "MasterLens " << kVersion << " — album delivery proof\n";
    text << "Album: " << s.albumName << "  revision " << s.revision << "  schema " << s.schemaVersion << "\n";
    text << "Snapshot hash: " << s.hash << "\n";
    text << "Generated: " << juce::Time::getCurrentTime().toISO8601 (true) << "\n\n";

    text << "TRACKS\n";

    for (const auto& t : s.tracks)
    {
        text << "  " << t.label << "  " << juce::String (t.stats.integrated, 1) << " LUFS  "
             << juce::String (t.stats.lra, 1) << " LU  " << juce::String (t.stats.getPlr(), 1) << " PLR  "
             << juce::String (t.stats.truePeakDb, 1) << " dBTP";

        if (t.durationSeconds > 0.0)
            text << "  " << formatSeconds (t.durationSeconds) << "  "
                 << "cov " << juce::String (t.coveragePct, 1) << "%";

        if (t.sourceFile.isNotEmpty())
            text << "  [" << t.sourceFile << "]";

        text << "\n";
    }

    if (s.hasMedian)
        text << "\nAlbum median: " << juce::String (s.medianStats.integrated, 1) << " LUFS  "
             << juce::String (s.medianStats.lra, 1) << " LU  "
             << juce::String (s.medianStats.getPlr(), 1) << " PLR  "
             << juce::String (s.medianStats.truePeakDb, 1) << " dBTP\n";

    text << "\nGATE: " << (gate.blocked ? "BLOCKED" : "PASS") << " (" << gate.blockers << " blockers)\n";

    for (const auto& r : gate.reasons)
        text << "  - " << r << "\n";

    text << "\nRISKS\n";

    for (const auto& r : gate.risks)
        text << "  [" << (r.severity >= 2 ? "BLOCK" : "WARN") << "] " << r.track
             << " · " << r.dimension << " · " << r.detail << "  @ " << r.time << "\n";

    text << "\nDIFF vs previous revision\n";

    for (const auto& l : diff.lines)
        text << "  " << l << "\n";

    text << "\nSnapshot hash: " << s.hash << "\n";
    return text;
}

juce::String stableTrackId (const juce::String& label, const juce::String& source)
{
    const auto key = label.trim().toLowerCase() + "|" + source;
    return "trk-" + juce::SHA256 (key.toRawUTF8(),
                                 (size_t) key.getNumBytesAsUTF8()).toHexString().substring (0, 16);
}
} // namespace ml::album








