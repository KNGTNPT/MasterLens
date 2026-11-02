#include "FileAnalyzer.h"
#include "DeliverySpec.h"

#include <algorithm>

namespace ml
{
juce::var fileResultToVar (const FileAnalysisResult& r, float target, float ceiling,
                           const DeliverySpec* spec)
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("file", r.fileName);
    o->setProperty ("path", r.path);
    o->setProperty ("ok", r.ok);
    o->setProperty ("error", r.error);
    o->setProperty ("sampleRate", r.sampleRate);
    o->setProperty ("bitDepth", r.bitDepth);
    o->setProperty ("channels", r.numChannels);
    o->setProperty ("durationSeconds", r.durationSeconds);
    o->setProperty ("coveragePct", r.ok ? r.stats.coveragePct : 0.0f);

    auto* s = new juce::DynamicObject();
    s->setProperty ("integratedLufs", r.stats.integrated);
    s->setProperty ("momentaryLufs", r.stats.momentary);
    s->setProperty ("shortTermLufs", r.stats.shortTerm);
    s->setProperty ("lraLu", r.stats.lra);
    s->setProperty ("plrDb", r.stats.getPlr());
    s->setProperty ("truePeakDbtp", r.stats.truePeakDb);
    s->setProperty ("truePeakLeftDbtp", r.stats.truePeakLeft);
    s->setProperty ("truePeakRightDbtp", r.stats.truePeakRight);
    s->setProperty ("sideTruePeakDbtp", r.stats.sideTruePeak);
    s->setProperty ("midIntegratedLufs", r.stats.midLufs);
    s->setProperty ("sideIntegratedLufs", r.stats.sideLufs);
    s->setProperty ("midShortTermLufs", r.stats.midShortTerm);
    s->setProperty ("sideShortTermLufs", r.stats.sideShortTerm);
    s->setProperty ("correlation", r.stats.correlation);
    s->setProperty ("widthPct", r.stats.widthPct);
    s->setProperty ("lowSidePct", r.stats.lowSidePct);
    s->setProperty ("clipEvents", r.stats.clipEvents);
    s->setProperty ("clickEvents", r.stats.clickEvents);
    s->setProperty ("oversCount", r.stats.oversCount);
    o->setProperty ("stats", juce::var (s));

    juce::Array<juce::var> bands;

    for (float b : r.bandDb)
        bands.add (b);

    o->setProperty ("bandsDb", bands);

    auto* d = new juce::DynamicObject();
    d->setProperty ("distortionRisk", r.defects.distortionRisk);
    d->setProperty ("aliasRisk", r.defects.aliasRisk);
    d->setProperty ("subRisk", r.defects.subRisk);
    d->setProperty ("transientScore", r.defects.transientScore);
    d->setProperty ("monoRisk", r.defects.monoRisk);
    d->setProperty ("maskingRisk", r.defects.maskingRisk);
    d->setProperty ("worstRisk", r.defects.worstRisk());
    o->setProperty ("defects", juce::var (d));

    o->setProperty ("forensic", r.forensic.toVar (r.sampleRate));
    auto budget = gainBudget (r.stats.integrated, r.stats.truePeakDb, target, ceiling);
    budget.valid = budget.valid && r.ok;
    o->setProperty ("gainBudget", budget.toVar());

    if (spec != nullptr && spec->enabled)
    {
        o->setProperty ("deliverySpec", spec->toVar());
        o->setProperty ("specCheck", evaluateSpec (r, *spec).toVar());
    }

    return juce::var (o);
}


juce::String fileResultsCsv (const std::vector<FileAnalysisResult>& results, float target, float ceiling)
{
    juce::StringArray lines;
    lines.add ("file,status,error,sample_rate_hz,bit_depth,channels,duration_s,integrated_lufs,true_peak_dbtp,lra_lu,plr_db,blockers,reported_findings,reviewed_findings,leading_silence_s,trailing_silence_s,dc_left,dc_right,internal_silences,clipped_runs,target_lufs,ceiling_dbtp,safe_gain_db,achieved_lufs_estimate,shortfall_lu,sha256");
    const auto quote = [] (const juce::String& text) { return "\"" + text.replace ("\"", "\"\"") + "\""; };
    for (const auto& r : results)
    {
        const auto p = gainBudget (r.stats.integrated, r.stats.truePeakDb, target, ceiling);
        const auto& q = r.forensic;
        juce::StringArray row;
        row.add (quote (r.fileName)); row.add (r.ok ? "ok" : "error"); row.add (quote (r.error));
        row.add (juce::String (r.sampleRate, 0)); row.add (juce::String (r.bitDepth)); row.add (juce::String (r.numChannels));
        row.add (juce::String (r.durationSeconds, 6));
        for (float v : { r.stats.integrated, r.stats.truePeakDb, r.stats.lra, r.stats.getPlr() }) row.add (r.ok ? juce::String (v, 3) : "");
        row.add (juce::String (q.blockers())); row.add (juce::String ((int) q.findings.size()));
        row.add (juce::String ((int) std::count_if (q.findings.begin(), q.findings.end(), [] (const auto& f) { return f.reviewed; })));
        row.add (juce::String (q.leadingSilenceSeconds, 6)); row.add (juce::String (q.trailingSilenceSeconds, 6));
        row.add (juce::String (q.dcOffset[0], 8)); row.add (juce::String (q.dcOffset[1], 8));
        row.add (juce::String (q.internalSilenceCount)); row.add (juce::String (q.clippedRunCount));
        row.add (juce::String (target, 2)); row.add (juce::String (ceiling, 2));
        for (float v : { p.safeGainDb, p.achievedLufsEstimate, p.shortfallLu }) row.add (r.ok && p.valid ? juce::String (v, 3) : "");
        row.add (q.sha256);
        lines.add (row.joinIntoString (","));
    }
    return lines.joinIntoString ("\n") + "\n";
}
} // namespace ml
