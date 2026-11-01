#include "PluginEditor.h"

using ml::T;
#include "Delivery.h"
#include "GenreWar.h"
#include "Version.h"

//==============================================================================
juce::String MasterLensAudioProcessorEditor::buildReportJson() const
{
    auto* root = new juce::DynamicObject();

    root->setProperty ("product", "MasterLens");
    root->setProperty ("version", ml::kVersion);
    root->setProperty ("developer", ml::kDeveloper);
    root->setProperty ("organisation", ml::kOrganisation);
    root->setProperty ("generatedAt", juce::Time::getCurrentTime().toISO8601 (true));
    root->setProperty ("sampleRate", processor.getSampleRateValue());
    root->setProperty ("channels", stats.stereo ? 2 : 1);

    auto* loud = new juce::DynamicObject();
    loud->setProperty ("integratedLufs", stats.integrated);
    loud->setProperty ("shortTermLufs", stats.shortTerm);
    loud->setProperty ("momentaryLufs", stats.momentary);
    loud->setProperty ("lra", stats.lra);
    loud->setProperty ("plr", stats.getPlr());
    loud->setProperty ("midIntegratedLufs", stats.midLufs);
    loud->setProperty ("sideIntegratedLufs", stats.sideLufs);
    loud->setProperty ("psr", stats.psr);
    loud->setProperty ("psrMin", stats.psrMin);
    loud->setProperty ("psrMinTime", stats.psrMinTime);
    loud->setProperty ("targetLufs", processor.getTargetLufs());
    loud->setProperty ("timeOrigin", stats.timeOrigin);
    loud->setProperty ("hostPosition", stats.hostPosition);
    loud->setProperty ("hostLength", stats.hostLength);
    loud->setProperty ("coveragePct", stats.coveragePct);
    loud->setProperty ("monitorTrimDb", stats.monitorTrimDb);
    loud->setProperty ("basisMode", (int) currentBasis().mode);
    loud->setProperty ("basisLabel", currentBasis().label());
    loud->setProperty ("platformWorstCutDb", ml::platformSummary (stats).maxCutDb);
    loud->setProperty ("platformWastedLu", ml::platformSummary (stats).maxWastedDb);
    root->setProperty ("loudness", loud);

    auto* targetPlan = new juce::DynamicObject();
    const auto& genre = ml::presets::entries[juce::jlimit (0, ml::presets::customIndex - 1, targetGenreIndex)];
    targetPlan->setProperty ("genreIndex", targetGenreIndex);
    targetPlan->setProperty ("genreZh", juce::String::fromUTF8 (genre.zh));
    targetPlan->setProperty ("genreEn", juce::String::fromUTF8 (genre.en));
    targetPlan->setProperty ("warParticipationPct", targetWarPct);
    targetPlan->setProperty ("targetLufs", ml::presets::war::warTargetLufs (targetGenreIndex, targetWarPct));
    targetPlan->setProperty ("suggestedLra", ml::presets::war::warTargetLra (targetGenreIndex, targetWarPct));
    targetPlan->setProperty ("ceilingDbtp", genre.ceiling);
    targetPlan->setProperty ("tailSilenceMs", stats.tailSilenceMs);
    root->setProperty ("genreTarget", targetPlan);

    auto* album = new juce::DynamicObject();
    const auto albumSnap = albumSnapshot;
    const auto albumGate = ml::album::Session::evaluate (albumSnap);
    album->setProperty ("name", albumSnap.albumName);
    album->setProperty ("revision", albumSnap.revision);
    album->setProperty ("tracks", (int) albumSnap.tracks.size());
    album->setProperty ("blocked", albumGate.blocked);
    album->setProperty ("incomplete", albumGate.incomplete);
    album->setProperty ("blockers", albumGate.blockers);

    juce::Array<juce::var> albumReasons;

    for (const auto& reason : albumGate.reasons)
        albumReasons.add (reason);

    album->setProperty ("reasons", albumReasons);
    root->setProperty ("album", album);

    if (lastRegion.valid)
    {
        auto* regionObj = new juce::DynamicObject();
        regionObj->setProperty ("name", lastRegion.name);
        regionObj->setProperty ("start", lastRegion.start);
        regionObj->setProperty ("end", lastRegion.end);
        regionObj->setProperty ("integratedLufs", lastRegion.lufs);
        regionObj->setProperty ("midLufs", lastRegion.midLufs);
        regionObj->setProperty ("sideLufs", lastRegion.sideLufs);
        regionObj->setProperty ("truePeakDbtp", lastRegion.truePeak);
        regionObj->setProperty ("plrDb", lastRegion.plr);
        regionObj->setProperty ("correlation", lastRegion.correlation);
        regionObj->setProperty ("widthPct", lastRegion.widthPct);

        juce::Array<juce::var> regionBands;

        for (float b : lastRegion.bands)
            regionBands.add (b);

        regionObj->setProperty ("bandsDb", regionBands);
        root->setProperty ("region", juce::var (regionObj));
    }

    auto* peak = new juce::DynamicObject();
    peak->setProperty ("truePeakDbtp", stats.truePeakDb);
    peak->setProperty ("samplePeakDbfs", stats.samplePeakDb);
    peak->setProperty ("truePeakMaxTime", stats.truePeakMaxTime);
    peak->setProperty ("ceilingDbtp", processor.getCeilingDb());
    peak->setProperty ("truePeakLeftDbtp", stats.truePeakLeft);
    peak->setProperty ("truePeakRightDbtp", stats.truePeakRight);
    peak->setProperty ("sideTruePeakDbtp", stats.sideTruePeak);
    peak->setProperty ("oversampling", 4);
    peak->setProperty ("overWindows", stats.oversCount);

    juce::Array<juce::var> events;

    for (const auto& e : tpEvents)
    {
        auto* ev = new juce::DynamicObject();
        ev->setProperty ("time", e.time);
        ev->setProperty ("overshootDb", e.overshootDb);
        events.add (juce::var (ev));
    }

    peak->setProperty ("events", events);
    root->setProperty ("truePeak", peak);

    auto* stereo = new juce::DynamicObject();
    stereo->setProperty ("correlation", stats.correlation);
    stereo->setProperty ("widthPct", stats.widthPct);
    stereo->setProperty ("lowSidePct", stats.lowSidePct);
    stereo->setProperty ("bandCorrLow", stats.bandCorrLow);
    stereo->setProperty ("bandCorrMid", stats.bandCorrMid);
    stereo->setProperty ("bandCorrHigh", stats.bandCorrHigh);
    stereo->setProperty ("bandSideLow", stats.bandSideLow);
    stereo->setProperty ("bandSideMid", stats.bandSideMid);
    stereo->setProperty ("bandSideHigh", stats.bandSideHigh);
    root->setProperty ("stereo", stereo);

    juce::Array<juce::var> ratios;

    for (float v : bandRatios)
        ratios.add (v);

    root->setProperty ("bandRatios", ratios);

    juce::Array<juce::var> platformResults;

    for (int i = 0; i < ml::numPlatforms; ++i)
    {
        const auto& p = ml::platform (i);
        const auto res = ml::evaluate (stats, p);

        auto* o = new juce::DynamicObject();
        o->setProperty ("id", p.id);
        o->setProperty ("name", T (p.nameZh, p.nameEn));
        o->setProperty ("targetLufs", p.targetLufs);
        o->setProperty ("tpCeiling", p.tpCeiling);
        o->setProperty ("gainDb", res.gainDb);
        o->setProperty ("predictedTp", res.predictedTp);
        o->setProperty ("predictedTpBeforeLimiter", res.predictedTpRaw);
        o->setProperty ("serviceLimitingDb", res.limitedDb);
        o->setProperty ("wastedLoudnessDb", res.wastedLoudnessDb);
        o->setProperty ("normalises", p.normalises);
        o->setProperty ("codec", T (p.codecZh, p.codecEn));
        o->setProperty ("minTailSilenceMs", p.minTailSilenceMs);
        o->setProperty ("tailSilenceMs", stats.tailSilenceMs);
        o->setProperty ("tailOk", p.minTailSilenceMs <= 0
                                  || (stats.tailSilenceMs > 0.0f
                                      && stats.tailSilenceMs >= (float) p.minTailSilenceMs));
        o->setProperty ("clipOk", stats.clipEvents == 0);
        o->setProperty ("state", res.state == ml::DeliveryState::over ? "over"
                             : res.state == ml::DeliveryState::warning ? "warning" : "ok");
        platformResults.add (juce::var (o));
    }

    root->setProperty ("platforms", platformResults);

    auto* signal = new juce::DynamicObject();
    signal->setProperty ("harmonicExcessDb", signalDefects.harmonicExcessDb);
    signal->setProperty ("imdSidebandDb", signalDefects.imdSidebandDb);
    signal->setProperty ("flatTopPct", signalDefects.flatTopPct);
    signal->setProperty ("distortionRisk", signalDefects.distortionRisk);
    signal->setProperty ("hfTiltDb", signalDefects.hfTiltDb);
    signal->setProperty ("aliasExcessDb", signalDefects.aliasExcessDb);
    signal->setProperty ("aliasRisk", signalDefects.aliasRisk);
    signal->setProperty ("subSharePct", signalDefects.subSharePct);
    signal->setProperty ("subTiltDb", signalDefects.subTiltDb);
    signal->setProperty ("subRisk", signalDefects.subRisk);
    signal->setProperty ("transientScore", signalDefects.transientScore);
    signal->setProperty ("crestLossDb", signalDefects.crestLossDb);
    signal->setProperty ("monoFoldLossDb", signalDefects.monoFoldLossDb);
    signal->setProperty ("monoRetainPct", signalDefects.monoRetainPct);
    signal->setProperty ("monoLowRetainPct", signalDefects.monoLowRetainPct);
    signal->setProperty ("maskingDb", signalDefects.maskingDb);
    signal->setProperty ("maskingRisk", signalDefects.maskingRisk);
    signal->setProperty ("attributionLayers", signalDefects.attributionCount);
    root->setProperty ("signalIntegrity", signal);

    if (reference.hasStats())
    {
        const auto rs = reference.getStats();
        auto* ref = new juce::DynamicObject();
        ref->setProperty ("integratedLufs", rs.integrated);
        ref->setProperty ("lra", rs.lra);
        ref->setProperty ("plr", rs.getPlr());
        ref->setProperty ("truePeakDbtp", rs.truePeakDb);
        ref->setProperty ("lowSidePct", rs.lowSidePct);
        ref->setProperty ("subSharePct", rs.subSharePct);
        ref->setProperty ("deltaLufs", stats.integrated - rs.integrated);
        ref->setProperty ("deltaLra", stats.lra - rs.lra);
        ref->setProperty ("deltaPlr", stats.getPlr() - rs.getPlr());
        root->setProperty ("reference", ref);
    }

    juce::Array<juce::var> markerArray;

    for (const auto& m : markers)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("time", m.time);
        o->setProperty ("severity", m.severity);
        o->setProperty ("category", m.category);
        o->setProperty ("text", m.text);
        markerArray.add (juce::var (o));
    }

    root->setProperty ("markers", markerArray);

    juce::Array<juce::var> checks;

    for (const auto& c : ml::buildChecklist (stats,
                                             processor.getCeilingDb(),
                                             currentBandDevs(),
                                             signalDefects,
                                             currentBasis(),
                                             ml::platformSummary (stats),
                                             analyzer.hasReference() || currentBasis().hasAlbum))
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("label", c.label);
        o->setProperty ("value", c.value);
        o->setProperty ("severity", (int) c.severity);
        o->setProperty ("rule", c.hint);
        checks.add (juce::var (o));
    }

    root->setProperty ("checks", checks);

    return juce::JSON::toString (juce::var (root), false);
}
