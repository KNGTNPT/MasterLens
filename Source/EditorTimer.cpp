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
    const int next = juce::jlimit (0, ml::numPages - 1, index);
    const bool changed = next != selectedPage;

    selectedPage = next;
    rail.setSelected (selectedPage);
    detail.setPage ((ml::PageId) selectedPage);

    timeline.setVisible (false);
    resized();

    // A lightweight page enter: the destination is readable immediately and
    // settles from 0 to 1 alpha over 140 ms. Headless captures stay instant.
    if (changed && isShowing())
    {
        animator.cancelAnimation (&detail, true);
        detail.setAlpha (0.0f);
        animator.animateComponent (&detail, detail.getBounds(), 1.0f, 140, false, 0.0, 1.0);
    }
    else
        detail.setAlpha (1.0f);

    if (changed && processor.isAuditionActive() && lastAuditionIndex >= 0)
    {
        footerStatusText = T ("正在实时试听：", "Live audition: ")
                           + ml::signalAuditionLabel (lastAuditionIndex)
                           + T ("  · Esc 停止", "  · Esc stops");
        footerStatusColour = ml::theme::cyan;
    }
    else if (changed && processor.isAuditionActive() && activeBandSolo.isNotEmpty())
    {
        footerStatusText = T ("实时跟随 DAW · 独听 ", "Live with DAW · solo ")
                           + activeBandSolo + T (" · 不做等响 · Esc 停止", " · no makeup · Esc stops");
        footerStatusColour = ml::theme::cyan;
    }

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
    // Prefer the actual display clock. The timer is a fallback for offscreen
    // rendering, hosts without vblank support, and a stalled display callback.
    if (isShowing() && juce::Time::getMillisecondCounterHiRes() - lastVBlankMs < 100.0) return;
    updateUiFrame();
}

void MasterLensAudioProcessorEditor::updateUiFrame()
{
    pollSelectionMatch();
    pollCodecAB();

    // Hidden real windows cost nothing; the offscreen UiShot renderer has no
    // peer and must still be able to drive its timer.
    if (! isShowing() && getPeer() != nullptr)
        return;

    const double logoNow = juce::Time::getMillisecondCounterHiRes();
    const double dt = logoTickMs > 0.0 ? juce::jlimit (0.0, 0.1, (logoNow - logoTickMs) * 0.001) : 0.0;
    logoTickMs = logoNow;
    const auto produced = processor.outputSpectrumFifo.getProducedSamples();
    if (produced != lastAudioSample) { lastAudioSample = produced; lastAudioMs = logoNow; }
    const auto liveStats = processor.getStatsSnapshot();
    const bool active = processor.isFilePreviewActive() || processor.isStreamingPreviewActive()
        || processor.isAuditionActive()
        || ((logoNow - lastAudioMs) < 150.0 &&
            (processor.hostTransportPlaying.load (std::memory_order_relaxed)
             || (liveStats.hasSignal && liveStats.tailSilenceMs < 250.0f)));
    if (active)
    {
        logoAngle = std::fmod (logoAngle + (float) dt * juce::MathConstants<float>::twoPi / 8.0f,
                              juce::MathConstants<float>::twoPi);
        repaint (headerArea.toFloat().transformedBy (juce::AffineTransform::scale (uiScale)).toNearestInt());
    }

    // Drain even on slow pages and during Hold. The output tap has its own
    // Blackman-windowed cache; it does not replace the existing input analyser.
    outputSpectrumReader.update (processor.outputSpectrumFifo);

    ++frameCounter;
    ++timerTicks;

    if (holdFromKeyboard && ! juce::KeyPress::isKeyCurrentlyDown (juce::KeyPress::spaceKey))
    {
        holdFromKeyboard = false;
        setHold (false, false);
    }

    const bool spectrumVisible = selectedPage == (int) ml::PageId::spectrum;
    const bool fastPage = selectedPage == (int) ml::PageId::loudness
                       || selectedPage == (int) ml::PageId::stereo
                       || selectedPage == (int) ml::PageId::truePeak
                       || spectrumVisible;

    // Present moving meters on every tick. Static reports can update at half
    // this rate; FFT and event attribution have separate slower schedules.
    if (! fastPage && (frameCounter & 1) != 0)
        return;

    const double sr = processor.getSampleRateValue();

    if (std::abs (sr - analyzerSampleRate) > 0.5)
    {
        analyzerSampleRate = sr;
        analyzer.prepare (sr, 13);
    }

    analyzer.setSmoothing (processor.getSmoothing());

    if ((frameCounter % 6) == 0)
        pollFileAnalysis();

    if ((frameCounter % 120) == 0)
        pollFolderWatch();

    if ((frameCounter % 12) == 0)
    {
        detail.setReferencePlaybackState (processor.referencePlayer.isEnabled()
                                              && processor.referencePlayer.isReady(),
                                          processor.referencePlayer.getStatus());

        if (filePreviewPath.isNotEmpty())
        {
            const auto* selected = detail.selectedFile();
            const bool codecSideActive = processor.isCodecPreviewActive();
            const bool streaming = processor.isStreamingPreviewActive() || codecSideActive;
            detail.setFileBuffering (streaming && (processor.isStreamingPreviewBuffering()
                                                   || processor.isCodecPreviewBuffering()));

            detail.setFileProgress (codecSideActive ? processor.getCodecPosition()
                                                    : (streaming ? processor.getStreamingPosition()
                                                                 : processor.getFilePreviewPosition()),
                                    codecSideActive ? processor.getCodecDuration()
                                                    : (streaming ? processor.getStreamingDuration()
                                                                 : processor.getFilePreviewDuration()));

            if ((! streaming && ! processor.isFilePreviewActive())
                || selected == nullptr || selected->path != filePreviewPath)
            {
                if (selected == nullptr || selected->path != filePreviewPath)
                {
                    processor.stopFilePreview();
                    processor.stopStreamingPreview();
                }

                filePreviewPath.clear();
                detail.setFilePlayActive (false);
            }
        }
    }

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

    // Spectrum frames update on the 30 Hz analytical pass; other pages still
    // get a slow band refresh. No FFT while Hold is engaged.
    const bool runAnalyzer = (spectrumVisible && (frameCounter % 2) == 0) || (frameCounter % 12) == 0;

    if (! frozen && runAnalyzer)
        analyzer.update (processor.spectrumRing);

    if (reference.isReady() && ! analyzer.hasReference())
        analyzer.setReferenceSpectrum (reference.getSpectrumDb());

    if (frozen)
    {
        timeline.setPlayhead (timelineNow());
        return;
    }

    ++presentationTicks;
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
        monitorButton.setButtonText (ml::grid::controlText (processor.isMonitorMatch()
                                         ? T ("等响 ", "match ") + ml::sgn (trim) + " dB"
                                         : T ("等响监听", "Match")));
    }

    if ((frameCounter % 30) == 0)
        refreshAlbum();

    const auto basis = currentBasis();
    const auto platform = ml::platformSummary (stats);
    const auto devs = currentBandDevs();
    const bool hasTonalBasis = basis.hasComparison() && basis.mode != ml::BasisMode::manual;

    const float target = basis.hasComparison() ? basis.comparisonLufs()
                       : (basis.mode == ml::BasisMode::manual ? manualTarget : -14.0f);
    processor.setMonitorTargetLufs (target);

    if (selectedPage == (int) ml::PageId::files)
    {
        const auto* file = detail.selectedFile();
        const auto offline = file != nullptr && file->ok ? file->stats : ml::StatsSnapshot {};
        readout.setValues (offline, basis, ml::platformSummary (offline), target, ceiling, offline.hasSignal);
    }
    else readout.setValues (stats, basis, platform, target, ceiling, stats.hasSignal);

    const auto& resos = analyzer.getResonances();
    if ((frameCounter % 12) == 0)
    {
        const auto& ring = processor.engine.getTpEvents();
        const int n = ring.size();

        tpEvents.clear();
        tpEvents.reserve ((size_t) n);

        for (int i = 0; i < n; ++i)
            tpEvents.push_back (ring.get (i));

        bandRatios = analyzer.getBandRatios();

        if ((frameCounter % 24) == 0)
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
    }

    ml::UiState us;
    us.stats = stats;
    const int auditionIndex = processor.isAuditionActive() ? lastAuditionIndex : -1;
    us.signalAuditionIndex = auditionIndex >= 0 ? auditionIndex : -1;
    if (us.signalAuditionIndex >= 0)
        us.liveAuditionLabel = ml::signalAuditionLabel (us.signalAuditionIndex);
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

    if ((frameCounter % 6) == 0)
    {
        if (viewingSnapshot < 0)
        {
            processor.copyShortTermCurve (curve, 1800);
            detail.setHistory (curve, target, stats.lra);
        }
    }

    detail.setPlayhead (timelineNow(), timelineTotal());

    // The advisor, issue list and rail only need a few updates per second; the
    // meters and spectrum keep their full frame rate.
    if ((frameCounter % 12) == 0)
    {
        const int command = processor.consumePendingUiCommand();

        if (command == 2)
            captureAlbum();
        else if (command == 3)
        {
            const auto file = juce::File::getSpecialLocation (juce::File::userDesktopDirectory)
                                  .getChildFile ("MasterLens-Album-Proof-auto.json");
            setExportResult (ml::writeTextAtomic (file, processor.albumSession.exportProofJson()));
        }
        else if (command == 4)
            exportMarkers();
        else if (command == 5)
            clearAlbum();

        advisor.update (stats, devs, target, ceiling, signalDefects, basis, hasTonalBasis);
        issueCache = advisor.getIssues();

        detail.setIssues (issueCache);
        updateRail();
    }
}
