#include "FileForensics.h"
#include "../Version.h"
#include "../dsp/SpectrumAnalyzer.h"
#include <algorithm>
#include <cmath>

namespace ml
{
void FileForensics::add (const FileFinding& f)
{
    // Reserve space for blockers even in a file containing thousands of pauses.
    if (findings.size() < 128)
        findings.push_back (f);
    else
    {
        ++omittedFindings;
        if (f.severity == 2)
            for (auto& old : findings)
                if (old.severity < 2) { old = f; break; }
    }
}

int FileForensics::blockers() const
{
    if (! measured) return 0;
    // Counters are uncapped; the evidence list is deliberately bounded.
    return (nonFiniteSamples > 0 ? 1 : 0) + (firstActiveSample < 0 ? 1 : 0)
           + (clippedRunCount > 0 ? 1 : 0)
           + (std::max (std::abs (dcOffset[0]), std::abs (dcOffset[1])) > 0.01 ? 1 : 0);
}

FileForensicsScanner::FileForensicsScanner (double r, int c, int bits)
    : rate (r), channels (juce::jlimit (1, 2, c)),
      fullScaleThreshold (1.0 - std::max (1.0e-7, std::ldexp (1.0, 1 - juce::jlimit (8, 32, bits))))
{
}

void FileForensicsScanner::closeClip (int ch)
{
    auto& start = clipStart[(size_t) ch];
    if (start < 0) return;
    const auto n = position - start;
    result.longestFullScaleRun[(size_t) ch] = std::max (result.longestFullScaleRun[(size_t) ch], n);
    if (n >= 3)
    {
        ++result.clippedRunCount;
        result.add ({ "full_scale_run", 2, start, n, ch, (double) n });
    }
    start = -1;
}

bool FileForensicsScanner::process (const juce::AudioBuffer<float>& buffer, int count)
{
    bool finite = true;
    for (int n = 0; n < count; ++n, ++position)
    {
        float peak = 0.0f;
        for (int ch = 0; ch < channels; ++ch)
        {
            const auto i = (size_t) ch;
            const float v = buffer.getSample (ch, n);
            if (! std::isfinite (v))
            {
                ++result.nonFiniteSamples;
                if (finite) result.add ({ "non_finite", 2, position, 1, ch, 0.0 });
                finite = false;
                continue;
            }
            if (position == 0) first[i] = v;
            last[i] = v;
            sums[i] += v;
            peak = std::max (peak, std::abs (v));
            if (std::abs ((double) v) >= fullScaleThreshold)
            {
                ++result.fullScaleSamples[i];
                if (clipStart[i] < 0) clipStart[i] = position;
            }
            else closeClip (ch);
        }
        // Active edges use -80 dBFS; digital gaps use effectively exact zero.
        if (peak > 0.0001f)
        {
            if (result.firstActiveSample < 0) result.firstActiveSample = position;
            result.lastActiveSample = position;
        }
        if (peak <= 1.0e-9f)
        {
            if (zeroStart < 0) zeroStart = position;
        }
        else if (zeroStart >= 0)
        {
            const auto len = position - zeroStart;
            if (result.firstActiveSample >= 0 && zeroStart > result.firstActiveSample
                && len >= (juce::int64) std::ceil (rate * 0.010))
            {
                ++result.internalSilenceCount;
                result.add ({ "internal_digital_silence", 1, zeroStart, len, -1, len / rate });
            }
            zeroStart = -1;
        }
    }
    return finite;
}

FileForensics FileForensicsScanner::finish()
{
    result.measured = true;
    for (int ch = 0; ch < channels; ++ch)
    {
        closeClip (ch);
        const auto i = (size_t) ch;
        result.dcOffset[i] = position > 0 ? sums[i] / (double) position : 0.0;
        if (std::abs (result.dcOffset[i]) > 0.001)
            result.add ({ "dc_offset", std::abs (result.dcOffset[i]) > 0.01 ? 2 : 1,
                          0, position, ch, result.dcOffset[i] });
        if (std::abs (first[i]) > 0.01f)
            result.add ({ "hot_start", 1, 0, 1, ch, first[i] });
        if (std::abs (last[i]) > 0.01f)
            result.add ({ "hot_end", 1, juce::jmax ((juce::int64) 0, position - 1), 1, ch, last[i] });
    }
    result.leadingSilenceSeconds = (result.firstActiveSample < 0 ? position : result.firstActiveSample) / rate;
    result.trailingSilenceSeconds = (result.lastActiveSample < 0 ? position : position - 1 - result.lastActiveSample) / rate;
    if (result.firstActiveSample < 0)
        result.add ({ "no_active_signal", 2, 0, position, -1, 0.0 });
    std::stable_sort (result.findings.begin(), result.findings.end(), [] (const auto& a, const auto& b)
    {
        return a.severity != b.severity ? a.severity > b.severity : a.startSample < b.startSample;
    });
    return result;
}

juce::String FileForensics::evidenceKey (const FileFinding& f) const
{
    if (sha256.length() != 64) return {};
    return sha256 + "|" + kVersion + "|" + f.code + "|" + juce::String (f.startSample)
        + "|" + juce::String (f.lengthSamples) + "|" + juce::String (f.channel);
}

juce::var FileForensics::toVar (double sr) const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("schemaVersion", 1);
    o->setProperty ("analysisVersion", kVersion);
    o->setProperty ("reviewBinding", "sha256+analysisVersion+event");
    o->setProperty ("measured", measured);
    o->setProperty ("sha256", sha256);
    o->setProperty ("activeThresholdDbfs", -80.0);
    o->setProperty ("firstActiveSample", firstActiveSample);
    o->setProperty ("lastActiveSample", lastActiveSample);
    o->setProperty ("leadingSilenceSeconds", leadingSilenceSeconds);
    o->setProperty ("trailingSilenceSeconds", trailingSilenceSeconds);
    o->setProperty ("nonFiniteSamples", nonFiniteSamples);
    o->setProperty ("internalSilenceCount", internalSilenceCount);
    o->setProperty ("clippedRunCount", clippedRunCount);
    o->setProperty ("omittedFindings", omittedFindings);
    o->setProperty ("blockers", blockers());
    o->setProperty ("worstMonoBand", worstMonoBand);
    juce::Array<juce::var> dc, full, longest, bands, events;
    for (size_t i = 0; i < 2; ++i)
    {
        dc.add (dcOffset[i]); full.add (fullScaleSamples[i]); longest.add (longestFullScaleRun[i]);
    }
    o->setProperty ("dcOffset", dc);
    o->setProperty ("fullScaleSamples", full);
    o->setProperty ("longestFullScaleRun", longest);
    for (size_t b = 0; b < 10; ++b)
    {
        auto* band = new juce::DynamicObject();
        const auto& info = SpectrumAnalyzer::getBandInfo ((int) b);
        band->setProperty ("lowHz", info.lo);
        band->setProperty ("highHz", info.hi);
        band->setProperty ("available", bandAvailable[b]);
        band->setProperty ("monoLossDb", monoLossDb[b]);
        band->setProperty ("sideEnergyPct", sideEnergyPct[b]);
        band->setProperty ("energySharePct", energySharePct[b]);
        bands.add (juce::var (band));
    }
    o->setProperty ("bands", bands);
    for (const auto& f : findings)
    {
        auto* e = new juce::DynamicObject();
        e->setProperty ("evidenceId", evidenceKey (f));
        e->setProperty ("code", f.code);
        e->setProperty ("severity", f.severity);
        e->setProperty ("startSample", f.startSample);
        e->setProperty ("lengthSamples", f.lengthSamples);
        e->setProperty ("startSeconds", sr > 0.0 ? f.startSample / sr : 0.0);
        e->setProperty ("durationSeconds", sr > 0.0 ? f.lengthSamples / sr : 0.0);
        e->setProperty ("channel", f.channel);
        e->setProperty ("value", f.value);
        e->setProperty ("reviewed", f.reviewed);
        e->setProperty ("ignored", f.ignored);
        e->setProperty ("note", f.note);
        events.add (juce::var (e));
    }
    o->setProperty ("findings", events);
    return juce::var (o);
}

GainBudget gainBudget (float integrated, float peak, float target, float ceiling)
{
    GainBudget p;
    p.targetLufs = target; p.ceilingDbtp = ceiling;
    p.valid = std::isfinite (integrated) && std::isfinite (peak) && integrated > -70.0f && peak > -99.0f
              && std::isfinite (target) && std::isfinite (ceiling);
    if (! p.valid) return p;
    p.requestedGainDb = target - integrated;
    p.safeGainDb = std::min (p.requestedGainDb, ceiling - peak);
    p.achievedLufsEstimate = integrated + p.safeGainDb;
    p.predictedTpDbtp = peak + p.safeGainDb;
    p.shortfallLu = std::max (0.0f, target - p.achievedLufsEstimate);
    return p;
}

juce::var GainBudget::toVar() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("valid", valid);
    o->setProperty ("targetLufs", targetLufs); o->setProperty ("ceilingDbtp", ceilingDbtp);
    o->setProperty ("requestedGainDb", requestedGainDb); o->setProperty ("safeGainDb", safeGainDb);
    o->setProperty ("achievedLufsEstimate", achievedLufsEstimate);
    o->setProperty ("predictedTpDbtp", predictedTpDbtp); o->setProperty ("shortfallLu", shortfallLu);
    o->setProperty ("method", "constant gain only; loudness estimate; no limiter or codec simulation");
    return juce::var (o);
}

juce::String findingText (const FileFinding& f, bool en)
{
    if (f.code == "full_scale_run") return juce::String::fromUTF8 (en ? "Consecutive full-scale samples" : "连续满刻度样本");
    if (f.code == "internal_digital_silence") return juce::String::fromUTF8 (en ? "Internal digital silence; review intent" : "内部数字静音，试听确认是否有意");
    if (f.code == "non_finite") return juce::String::fromUTF8 (en ? "Invalid NaN / Inf samples" : "无效 NaN / Inf 样本");
    if (f.code == "dc_offset") return juce::String::fromUTF8 (en ? "DC offset" : "直流偏移");
    if (f.code == "hot_start") return juce::String::fromUTF8 (en ? "Non-zero start; check edit / fade" : "开头非零，检查剪辑或淡入");
    if (f.code == "hot_end") return juce::String::fromUTF8 (en ? "Non-zero end; check edit / fade" : "结尾非零，检查剪辑或淡出");
    if (f.code == "no_active_signal") return juce::String::fromUTF8 (en ? "No signal above -80 dBFS" : "没有高于 -80 dBFS 的信号");
    return f.code;
}
} // namespace ml
