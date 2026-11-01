#include "PluginEditor.h"

using ml::T;

void MasterLensAudioProcessorEditor::captureRawSeries()
{
    if (viewingSnapshot >= 0 || ! stats.hasSignal || stats.integrated <= -70.0f)
        return;

    const double t = timelineNow();

    if (t <= lastRawSeriesTime + 0.02)
        return;

    RawSeriesPoint p;
    p.time = t;
    p.momentary = stats.momentary;
    p.shortTerm = stats.shortTerm;
    p.integrated = stats.integrated;
    p.lra = stats.lra;
    p.truePeak = stats.truePeakDb;
    p.correlation = stats.correlation;
    p.width = stats.widthPct;
    p.midShortTerm = stats.midShortTerm;
    p.sideShortTerm = stats.sideShortTerm;
    p.bands = analyzer.getBandDensities();

    rawSeries.push_back (p);
    lastRawSeriesTime = t;

    // At 100 ms intervals this is about four hours; trim in large chunks so the
    // message thread does not pay for an erase every tick.
    if (rawSeries.size() > 150000)
        rawSeries.erase (rawSeries.begin(), rawSeries.begin() + 20000);
}

void MasterLensAudioProcessorEditor::resetRawSeries()
{
    rawSeries.clear();
    lastRawSeriesTime = -1.0;
}

void MasterLensAudioProcessorEditor::measureRegion (double start, double end)
{
    if (end < start)
        std::swap (start, end);

    std::vector<const RawSeriesPoint*> points;

    for (const auto& p : rawSeries)
        if (p.time >= start && p.time <= end)
            points.push_back (&p);

    if (points.size() < 3)
    {
        footerStatusText = T ("选区内没有足够的 100 ms 采样，请先播放该段",
                              "Not enough 100 ms samples in the selection; play that section first");
        footerStatusColour = ml::theme::warn;
        repaint();
        return;
    }

    double lufsEnergy = 0.0, midEnergy = 0.0, sideEnergy = 0.0;
    float tp = -100.0f, corr = 0.0f, width = 0.0f;
    std::array<float, 10> bands {};
    int bandCount = 0;

    for (const auto* p : points)
    {
        lufsEnergy += ml::lufsToEnergy (p->momentary);
        midEnergy += ml::lufsToEnergy (p->midShortTerm);
        sideEnergy += ml::lufsToEnergy (p->sideShortTerm);
        tp = juce::jmax (tp, p->truePeak);
        corr += p->correlation;
        width += p->width;

        for (int b = 0; b < 10; ++b)
            if (p->bands[(size_t) b] > -99.0f)
            {
                bands[(size_t) b] += p->bands[(size_t) b];
                ++bandCount;
            }
    }

    ml::RegionMeasurement r;
    r.valid = true;
    r.start = start;
    r.end = end;
    r.lufs = ml::energyToLufs (lufsEnergy / (double) points.size());
    r.midLufs = ml::energyToLufs (midEnergy / (double) points.size());
    r.sideLufs = ml::energyToLufs (sideEnergy / (double) points.size());
    r.truePeak = tp;
    r.plr = juce::jmax (0.0f, tp - r.lufs);
    r.correlation = corr / (float) points.size();
    r.widthPct = width / (float) points.size();

    for (int b = 0; b < 10; ++b)
        if (bandCount > 0)
            r.bands[(size_t) b] = bands[(size_t) b] / (float) (points.size());

    r.name = T ("选区 ", "Region ") + ml::fmtTime (start) + ml::enDash() + ml::fmtTime (end);
    lastRegion = r;
    detail.setRegionMeasurement (r);
    footerStatusText = T ("选区测量 ", "Region ") + ml::fmtTime (start) + "-" + ml::fmtTime (end)
                       + T (" · ", " · ") + ml::num (r.lufs) + " LUFS"
                       + T (" · ", " · ") + ml::num (r.truePeak) + " dBTP";
    footerStatusColour = ml::theme::cyan;
    repaint();
}

juce::String MasterLensAudioProcessorEditor::buildRawTimeSeriesCsv() const
{
    juce::String csv;
    csv << "schemaVersion,2\n";
    csv << "time_s,momentary_lufs,short_term_lufs,integrated_lufs,lra_lu,true_peak_dbtp,"
           "correlation,width_pct";

    for (int i = 0; i < 10; ++i)
        csv << ",band" << (i + 1) << "_db";

    csv << "\n";

    const auto number = [] (float v)
    {
        return juce::String (v, 3);
    };

    for (const auto& p : rawSeries)
    {
        csv << juce::String (p.time, 3) << ','
            << number (p.momentary) << ','
            << number (p.shortTerm) << ','
            << number (p.integrated) << ','
            << number (p.lra) << ','
            << number (p.truePeak) << ','
            << number (p.correlation) << ','
            << number (p.width);

        for (float b : p.bands)
            csv << ',' << number (b);

        csv << "\n";
    }

    return csv;
}
