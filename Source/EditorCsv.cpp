#include "PluginEditor.h"

using ml::T;
#include "Delivery.h"
#include "GenreWar.h"
#include "Version.h"

//==============================================================================
juce::String MasterLensAudioProcessorEditor::buildReportCsv() const
{
    juce::String csv;
    const auto stamp = juce::Time::getCurrentTime().toISO8601 (true);

    const auto line = [&csv] (const juce::String& section, const juce::String& item,
                              const juce::String& value)
    {
        csv << section << ',' << item << ',' << value << '\n';
    };

    csv << "section,item,value\n";
    line ("meta", "generatedAt", stamp);
    line ("meta", "version", ml::kVersion);
    line ("meta", "developer", juce::String::fromUTF8 (ml::kDeveloper));
    line ("meta", "organisation", ml::kOrganisation);
    line ("meta", "sampleRate", juce::String (processor.getSampleRateValue(), 0));
    line ("meta", "timeOrigin", ml::num (stats.timeOrigin, 3));
    line ("meta", "hostPosition", ml::num (stats.hostPosition, 3));
    line ("meta", "hostLength", ml::num (stats.hostLength, 3));
    line ("meta", "coveragePct", ml::num (stats.coveragePct));

    line ("loudness", "integratedLufs", ml::num (stats.integrated));
    line ("loudness", "shortTermLufs", ml::num (stats.shortTerm));
    line ("loudness", "momentaryLufs", ml::num (stats.momentary));
    line ("loudness", "lraLu", ml::num (stats.lra));
    line ("loudness", "midIntegratedLufs", ml::num (stats.midLufs));
    line ("loudness", "sideIntegratedLufs", ml::num (stats.sideLufs));
    line ("loudness", "plrDb", ml::num (stats.getPlr()));
    line ("loudness", "psrDb", ml::num (stats.psr));
    line ("loudness", "psrMinDb", ml::num (stats.psrMin));
    line ("loudness", "targetLufs", ml::num (processor.getTargetLufs()));

    {
        const int gi = juce::jlimit (0, ml::presets::customIndex - 1, targetGenreIndex);
        const auto& g = ml::presets::entries[gi];
        line ("genreTarget", "genre", T (g.zh, g.en));
        line ("genreTarget", "warParticipationPct", juce::String (juce::roundToInt (targetWarPct)));
        line ("genreTarget", "targetLufs", ml::num (ml::presets::war::warTargetLufs (gi, targetWarPct)));
        line ("genreTarget", "suggestedLra", ml::num (ml::presets::war::warTargetLra (gi, targetWarPct)));
        line ("genreTarget", "ceilingDbtp", ml::num (g.ceiling));
        line ("genreTarget", "tailSilenceMs", ml::num (stats.tailSilenceMs, 0));
    }

    line ("truePeak", "maxDbtp", ml::num (stats.truePeakDb));
    line ("truePeak", "leftDbtp", ml::num (stats.truePeakLeft));
    line ("truePeak", "rightDbtp", ml::num (stats.truePeakRight));
    line ("truePeak", "sideDbtp", ml::num (stats.sideTruePeak));
    line ("truePeak", "ceilingDbtp", ml::num (processor.getCeilingDb()));
    line ("truePeak", "overWindows", juce::String (stats.oversCount));
    line ("truePeak", "maxAt", ml::fmtTime (stats.truePeakMaxTime));

    line ("stereo", "correlation", ml::sgn (stats.correlation, 2));
    line ("stereo", "widthPct", ml::num (stats.widthPct));
    line ("stereo", "lowSidePct", ml::num (stats.lowSidePct));

    for (int i = 0; i < 4; ++i)
        line ("spectrum", "band" + juce::String (i + 1) + "Pct", ml::num (bandRatios[(size_t) i]));

    for (int i = 0; i < ml::numPlatforms; ++i)
    {
        const auto& p = ml::platform (i);
        const auto res = ml::evaluate (stats, p);
        const auto name = T (p.nameZh, p.nameEn);

        line ("platform", name + " targetLufs", ml::num (p.targetLufs));
        line ("platform", name + " ceilingDbtp", ml::num (p.tpCeiling));
        line ("platform", name + " gainDb", ml::sgn (res.gainDb));
        line ("platform", name + " predictedTp", ml::num (res.predictedTp));
        line ("platform", name + " serviceLimitingDb", ml::num (res.limitedDb));
        line ("platform", name + " targetShortfallLu", ml::num (res.limitedDb));
        line ("platform", name + " requestedGainDb", ml::num (res.requestedGainDb));
        line ("platform", name + " scenarioOnly", "true");
        line ("platform", name + " wastedLoudnessLu", ml::num (res.wastedLoudnessDb));
        line ("platform", name + " state",
              res.state == ml::DeliveryState::over ? "over"
            : res.state == ml::DeliveryState::warning ? "warning" : "ok");
    }

    line ("signal", "harmonicExcessDb", ml::num (signalDefects.harmonicExcessDb));
    line ("signal", "imdSidebandDb", ml::num (signalDefects.imdSidebandDb));
    line ("signal", "flatTopPct", ml::num (signalDefects.flatTopPct, 2));
    line ("signal", "distortionRisk", ml::num (signalDefects.distortionRisk, 0));
    line ("signal", "aliasRisk", ml::num (signalDefects.aliasRisk, 0));
    line ("signal", "subSharePct", ml::num (signalDefects.subSharePct));
    line ("signal", "subTiltDb", ml::num (signalDefects.subTiltDb));
    line ("signal", "transientScore", ml::num (signalDefects.transientScore, 0));
    line ("signal", "monoFoldLossDb", ml::num (signalDefects.monoFoldLossDb, 2));
    line ("signal", "monoRetainPct", ml::num (signalDefects.monoRetainPct));
    line ("signal", "maskingDb", ml::num (signalDefects.maskingDb));
    line ("signal", "attributionLayers", juce::String (signalDefects.attributionCount));

    if (reference.hasStats())
    {
        const auto rs = reference.getStats();
        line ("reference", "integratedLufs", ml::num (rs.integrated));
        line ("reference", "deltaLufs", ml::num (stats.integrated - rs.integrated));
        line ("reference", "lra", ml::num (rs.lra));
        line ("reference", "deltaLra", ml::num (stats.lra - rs.lra));
        line ("reference", "plr", ml::num (rs.getPlr()));
        line ("reference", "deltaPlr", ml::num (stats.getPlr() - rs.getPlr()));
    }

    for (const auto& c : ml::buildChecklist (stats,
                                             processor.getCeilingDb(),
                                             currentBandDevs(),
                                             signalDefects,
                                             currentBasis(),
                                             ml::platformSummary (stats),
                                             analyzer.hasReference() || currentBasis().hasAlbum))
        line ("check", c.label, c.value);

    if (lastRegion.valid)
    {
        line ("region", "name", lastRegion.name);
        line ("region", "start", ml::num (lastRegion.start, 3));
        line ("region", "end", ml::num (lastRegion.end, 3));
        line ("region", "integratedLufs", ml::num (lastRegion.lufs));
        line ("region", "midLufs", ml::num (lastRegion.midLufs));
        line ("region", "sideLufs", ml::num (lastRegion.sideLufs));
        line ("region", "truePeakDbtp", ml::num (lastRegion.truePeak));
        line ("region", "plrDb", ml::num (lastRegion.plr));
        line ("region", "correlation", ml::sgn (lastRegion.correlation, 2));
        line ("region", "widthPct", ml::num (lastRegion.widthPct));

        for (int i = 0; i < 10; ++i)
            line ("region", "band" + juce::String (i + 1) + "Db", ml::num (lastRegion.bands[(size_t) i]));
    }

    for (const auto& m : markers)
        line ("marker", ml::fmtTime (m.time), m.text);

    return csv;
}

//==============================================================================
void MasterLensAudioProcessorEditor::setPageForSnapshot (int page)
{
    setPage (page);
}

void MasterLensAudioProcessorEditor::expandFirstIssueForSnapshot()
{
    setPage (6);
    detail.expandFirstIssueForSnapshot();
}
