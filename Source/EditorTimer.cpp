#include "PluginEditor.h"

using ml::T;
#include "Delivery.h"
#include "GenreWar.h"
#include "ui/StatusText.h"

using ml::statusLabel;
using ml::statusColour;

//==============================================================================
double MasterLensAudioProcessorEditor::timelineNow() const
{
    return stats.hostPosition > 0.0 ? stats.hostPosition : stats.elapsed;
}

double MasterLensAudioProcessorEditor::timelineTotal() const
{
    if (stats.hostLength > 0.0)
        return juce::jmax (stats.hostLength, timelineNow());

    return juce::jmax (30.0, timelineNow() + 1.0);
}

void MasterLensAudioProcessorEditor::setPage (int index)
{
    selectedPage = juce::jlimit (0, ml::numPages - 1, index);
    rail.setSelected (selectedPage);
    detail.setPage ((ml::PageId) selectedPage);

    const bool offline = selectedPage == (int) ml::PageId::files
                      || selectedPage == (int) ml::PageId::album;
    timeline.setVisible (! offline);
    resized();
    repaint();
}

void MasterLensAudioProcessorEditor::addMarker (double t, int severity, int category,
                                                const juce::String& text)
{
    if (t <= 0.05)
        return;

    for (const auto& m : markers)
        if (m.category == category && std::abs (m.time - t) < 0.45)
            return;

    markers.push_back ({ t, severity, category, text });

    if (markers.size() > 64)
        markers.erase (markers.begin());
}

void MasterLensAudioProcessorEditor::addManualMarker (double time)
{
    const auto t = juce::jlimit (0.0, juce::jmax (0.5, timelineTotal()), time);
    addMarker (t, 0, 4, T ("手动标记 ", "Manual marker ") + ml::fmtTime (t));
    detail.invalidateDisplays();
    updateRail();
}

void MasterLensAudioProcessorEditor::timerCallback()
{
    // Hidden real windows cost nothing; the offscreen UiShot renderer has no
    // peer and must still be able to drive its timer.
    if (! isShowing() && getPeer() != nullptr)
        return;

    ++frameCounter;

    if (holdFromKeyboard && ! juce::KeyPress::isKeyCurrentlyDown (juce::KeyPress::spaceKey))
    {
        holdFromKeyboard = false;
        setHold (false, false);
    }

    const double sr = processor.getSampleRateValue();

    if (std::abs (sr - analyzerSampleRate) > 0.5)
    {
        analyzerSampleRate = sr;
        analyzer.prepare (sr, 13);
    }

    analyzer.setSmoothing (processor.getSmoothing());

    if ((frameCounter % 6) == 0)
        pollFileAnalysis();

    if ((frameCounter % 12) == 0)
        detail.setReferencePlaybackState (processor.referencePlayer.isEnabled()
                                              && processor.referencePlayer.isReady(),
                                          processor.referencePlayer.getStatus());

    {
        double rejectedStart = 0.0;

        if (processor.auditionEngine.consumeRangeRejected (rejectedStart))
        {
            footerStatusText = T ("该段已超出 12 s 历史范围，请从该位置重播",
                                  "Outside the 12 s history - replay from that position");
            footerStatusColour = ml::theme::warn;
            lastAuditionIndex = -1;
            repaint();
        }
    }

    // 60 fps on the spectrum page, 5 fps band data on other pages, no FFT while
    // Hold is engaged.
    const bool spectrumVisible = selectedPage == (int) ml::PageId::spectrum;
    const bool runAnalyzer = spectrumVisible || (frameCounter % 12) == 0;

    if (! frozen && runAnalyzer)
        analyzer.update (processor.spectrumRing);

    if (reference.isReady() && ! analyzer.hasReference())
        analyzer.setReferenceSpectrum (reference.getSpectrumDb());

    if (frozen)
    {
        timeline.setPlayhead (timelineNow());
        return;
    }

    // The 60 Hz timer is reserved for the spectrum; every other page updates at
    // 30 Hz so the 1.4.3 low-load behaviour is preserved.
    if (! spectrumVisible && (frameCounter & 1) != 0)
        return;

    stats = processor.getStatsSnapshot();

    if (viewingSnapshot >= 0 && snapshots[viewingSnapshot].valid)
        stats = snapshots[viewingSnapshot].stats;

    if ((frameCounter % 6) == 0)
        captureRawSeries();

    const float manualTarget = processor.getTargetLufs();
    const float ceiling = processor.getCeilingDb();

    if ((frameCounter % 12) == 0)
    {
        const float trim = processor.getMonitorTrimDb();
        monitorButton.setButtonText (processor.isMonitorMatch()
                                         ? T ("等响 ", "match ") + ml::sgn (trim) + " dB"
                                         : T ("等响监听", "Loudness match"));
    }

    if ((frameCounter % 30) == 0)
        refreshAlbum();

    const auto basis = currentBasis();
    const auto platform = ml::platformSummary (stats);
    const auto devs = currentBandDevs();
    const bool hasTonalBasis = basis.hasComparison() && basis.mode != ml::BasisMode::manual;

    const float target = basis.hasComparison() ? basis.comparisonLufs()
                       : (basis.mode == ml::BasisMode::manual ? manualTarget : -14.0f);

    {
        const auto& ring = processor.engine.getTpEvents();
        const int n = ring.size();

        tpEvents.clear();
        tpEvents.reserve ((size_t) n);

        for (int i = 0; i < n; ++i)
            tpEvents.push_back (ring.get (i));
    }

    bandRatios = analyzer.getBandRatios();

    const auto& resos = analyzer.getResonances();

    if ((frameCounter % 30) == 0)
        signalDefects = ml::analyseSignalDefects (analyzer.getAverageSpectrumDb(),
                                                  analyzer.getBinHz(), stats);

    updateSignalProblems();

    for (const auto& e : tpEvents)
        addMarker (e.time, 2, 0, T ("真峰值 +", "TP +") + ml::fmt1 (e.overshootDb) + " dBTP");

    if (stats.psrMin > -99.0f && stats.psrMinTime > 0.0)
        addMarker (stats.psrMinTime, 1, 1, T ("PSR 最低 ", "PSR minimum ") + ml::fmt1 (stats.psrMin) + " dB");

    if (stats.lastSpikeTime > 0.0)
        addMarker (stats.lastSpikeTime, 1, 1, T ("响度瞬时尖峰", "Loudness spike"));

    if (stats.correlation < 0.0f && stats.elapsed > 1.0)
        addMarker (std::floor (timelineNow()), 2, 2, T ("相关性为负", "Negative correlation"));

    if (stats.elapsed > 3.0)
        for (const auto& r : resos)
            addMarker (std::floor (timelineNow() / 5.0) * 5.0, 1, 3,
                       T ("谐振 ", "Resonance ") + juce::String (juce::roundToInt (r.freq))
                           + " Hz +" + ml::fmt1 (r.excessDb));

    ml::UiState us;
    us.stats = stats;
    us.bandDevs = devs;
    us.ratios = bandRatios;
    us.resonances = resos;
    us.tpEvents = tpEvents;
    us.markers = markers;
    us.signalProblems = signalProblems;
    us.defects = signalDefects;
    us.basis = basis;
    us.platform = platform;
    us.targetGenreIndex = targetGenreIndex;
    us.targetWarPct = targetWarPct;
    us.targetPlanLufs = ml::presets::war::warTargetLufs (targetGenreIndex, targetWarPct);
    us.targetPlanLra = ml::presets::war::warTargetLra (targetGenreIndex, targetWarPct);
    us.targetPlanCeiling = ml::presets::entries[targetGenreIndex].ceiling;

    if (selectedPage == (int) ml::PageId::album)
    {
        us.album = albumSnapshot;
        us.albumGate = albumGate;
        us.albumDiff = albumDiff;
    }

    us.bandDb = analyzer.getBandDensities();
    us.hasTonalBasis = hasTonalBasis;
    us.hasReferenceStats = reference.hasStats();

    if (us.hasReferenceStats)
        us.referenceStats = reference.getStats();

    us.target = target;
    us.ceiling = ceiling;
    us.hasReference = analyzer.hasReference();
    us.referenceStatus = reference.getStatus();
    us.analyzer = &analyzer;
    us.snapshotSlot = viewingSnapshot;
    us.snapshotValid[0] = snapshots[0].valid;
    us.snapshotValid[1] = snapshots[1].valid;

    if (snapshots[0].valid && snapshots[1].valid)
    {
        us.snapshotDelta[0] = snapshots[1].stats.integrated - snapshots[0].stats.integrated;
        us.snapshotDelta[1] = snapshots[1].stats.truePeakDb - snapshots[0].stats.truePeakDb;
    }

    detail.setState (std::move (us));

    if ((frameCounter % 12) == 0)
        detail.setReferenceInfo (analyzer.hasReference(), reference.getStatus());

    if ((frameCounter % 30) == 0)
        timeline.setMarkers (markers, timelineTotal());

    if ((frameCounter % 3) == 0)
        timeline.setPlayhead (timelineNow());

    if ((frameCounter % 6) == 0)
    {
        if (viewingSnapshot < 0)
        {
            processor.copyShortTermCurve (curve, 1800);
            detail.setHistory (curve, target, stats.lra);
        }
    }

    // The advisor, issue list and rail only need a few updates per second; the
    // meters and spectrum keep their full frame rate.
    if ((frameCounter % 12) == 0)
    {
    if ((frameCounter % 12) == 0)
    {
        const int command = processor.consumePendingUiCommand();

        if (command == 2)
            captureAlbum();
        else if (command == 3)
        {
            const auto file = juce::File::getSpecialLocation (juce::File::userDesktopDirectory)
                                  .getChildFile ("MasterLens-Album-Proof-auto.json");
            file.replaceWithText (ml::album::Session::get().exportProofJson());
            footerStatusText = T ("已生成交付证明（桌面）", "Proof saved to Desktop");
            footerStatusColour = ml::theme::good;
        }
        else if (command == 4)
            exportMarkers();
        else if (command == 5)
            clearAlbum();
    }

    advisor.update (stats, devs, target, ceiling, signalDefects, basis, hasTonalBasis);
        issueCache = advisor.getIssues();

        detail.setIssues (issueCache);
        updateRail();
    }
}
