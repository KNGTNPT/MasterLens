#include "PluginEditor.h"

using ml::T;
#include "Delivery.h"
#include "Version.h"
#include "GenreWar.h"

#include <algorithm>

//==============================================================================
void MasterLensAudioProcessorEditor::applyPreset (int index)
{
    if (updatingPreset)
        return;

    basisMode = (ml::BasisMode) juce::jlimit (0, 3, index);
    processor.apvts.state.setProperty ("basisMode", (int) basisMode, nullptr);

    updateTexts();
    detail.refreshTexts();
    detail.invalidateDisplays();
    updateRail();
    repaint();
}

void MasterLensAudioProcessorEditor::showGenreInfo()
{
    // Fixed genre loudness targets are no longer a pass line; the old panel is
    // intentionally unreachable in 1.5.
}

void MasterLensAudioProcessorEditor::syncPresetToParameters()
{
    updatingPreset = true;
    presetBox.setSelectedId ((int) basisMode + 1, juce::dontSendNotification);
    updatingPreset = false;
}

ml::BasisContext MasterLensAudioProcessorEditor::currentBasis() const
{
    ml::BasisContext b;
    b.mode = basisMode;
    b.manualLufs = processor.getTargetLufs();

    if (basisMode == ml::BasisMode::reference)
    {
        if (reference.hasStats())
        {
            b.hasReference = true;
            b.reference = reference.getStats();
            b.referenceLabel = reference.getFiles().isEmpty() ? juce::String()
                                                              : reference.getFiles()[0];
        }
        else if (albumSnapshot.hasReference)
        {
            b.hasReference = true;
            b.reference = albumSnapshot.referenceStats;
            b.referenceLabel = albumSnapshot.referenceLabel.isNotEmpty()
                                   ? albumSnapshot.referenceLabel
                                   : T ("已保存的参考快照", "saved reference snapshot");
        }
    }

    if (basisMode == ml::BasisMode::album && albumSnapshot.hasMedian)
    {
        b.hasAlbum = true;
        b.albumMedian = albumSnapshot.medianStats;
        b.albumLabel = albumSnapshot.albumName;
    }

    return b;
}

std::vector<float> MasterLensAudioProcessorEditor::currentBandDevs() const
{
    if (basisMode == ml::BasisMode::album && albumSnapshot.hasMedian)
    {
        const auto& current = analyzer.getBandDensities();
        const auto& median = albumSnapshot.medianBandDb;

        float currentSum = 0.0f, medianSum = 0.0f;
        int currentValid = 0, medianValid = 0;

        for (int b = 0; b < 10; ++b)
        {
            if (current[(size_t) b] > -99.0f)
            {
                currentSum += current[(size_t) b];
                ++currentValid;
            }

            if (median[(size_t) b] > -99.0f)
            {
                medianSum += median[(size_t) b];
                ++medianValid;
            }
        }

        if (currentValid < 3 || medianValid < 3)
            return {};

        const float currentMean = currentSum / (float) currentValid;
        const float medianMean = medianSum / (float) medianValid;

        std::vector<float> out ((size_t) 10, 0.0f);

        for (int b = 0; b < 10; ++b)
            if (current[(size_t) b] > -99.0f && median[(size_t) b] > -99.0f)
                out[(size_t) b] = (current[(size_t) b] - currentMean)
                                - (median[(size_t) b] - medianMean);

        return out;
    }

    if (analyzer.hasReference())
    {
        const auto& d = analyzer.getBandDeviations();
        return std::vector<float> (d.begin(), d.end());
    }

    if (basisMode == ml::BasisMode::reference && albumSnapshot.hasReference)
    {
        const auto& current = analyzer.getBandDensities();
        const auto& reference = albumSnapshot.referenceBandDb;

        float currentSum = 0.0f, refSum = 0.0f;
        int currentValid = 0, refValid = 0;

        for (int b = 0; b < 10; ++b)
        {
            if (current[(size_t) b] > -99.0f)
            {
                currentSum += current[(size_t) b];
                ++currentValid;
            }

            if (reference[(size_t) b] > -99.0f)
            {
                refSum += reference[(size_t) b];
                ++refValid;
            }
        }

        if (currentValid < 3 || refValid < 3)
            return {};

        const float currentMean = currentSum / (float) currentValid;
        const float refMean = refSum / (float) refValid;

        std::vector<float> out ((size_t) 10, 0.0f);

        for (int b = 0; b < 10; ++b)
            if (current[(size_t) b] > -99.0f && reference[(size_t) b] > -99.0f)
                out[(size_t) b] = (current[(size_t) b] - currentMean)
                                - (reference[(size_t) b] - refMean);

        return out;
    }

    return {};
}

void MasterLensAudioProcessorEditor::refreshAlbum()
{
    auto& session = ml::album::Session::get();
    albumSnapshot = session.snapshot();
    albumGate = ml::album::Session::evaluate (albumSnapshot);
    albumDiff = ml::album::Session::diffVersions (albumSnapshot);

    if (presetBox.getSelectedId() != (int) basisMode + 1)
        syncPresetToParameters();

    if (albumSnapshot.albumName.isNotEmpty()
        && (detail.getAlbumName().isEmpty() || ! albumNameEditorInitialised
            || detail.getAlbumName() == lastAlbumName))
    {
        if (detail.getAlbumName() != albumSnapshot.albumName)
            detail.setAlbumName (albumSnapshot.albumName);

        albumNameEditorInitialised = true;
    }

    lastAlbumName = albumSnapshot.albumName;
}

void MasterLensAudioProcessorEditor::captureAlbum()
{
    if (! stats.hasSignal || stats.integrated <= -70.0f)
    {
        footerStatusText = T ("无有效测量：先播放并等待响度稳定", "No valid measurement: play audio first");
        footerStatusColour = ml::theme::warn;
        repaint();
        return;
    }

    auto& session = ml::album::Session::get();
    auto snap = session.snapshot();

    const auto label = detail.getAlbumTrackLabel().isNotEmpty()
                           ? detail.getAlbumTrackLabel()
                           : "Track " + juce::String ((int) snap.tracks.size() + 1);

    ml::album::TrackRecord t;
    t.id = juce::String::toHexString (juce::Time::getCurrentTime().toMilliseconds());
    t.label = label;
    t.capturedAt = juce::Time::getCurrentTime().toISO8601 (true);
    t.stats = stats;
    t.defects = signalDefects;
    t.bandDb = analyzer.getBandDensities();
    t.tpCeiling = processor.getCeilingDb();
    t.sourceFile = T ("实时母线", "live bus");
    t.pluginVersion = ml::kVersion;
    t.durationSeconds = stats.hostLength > 0.0 ? stats.hostLength : stats.elapsed;
    t.sampleRate = processor.getSampleRateValue();
    t.bitDepth = 32;
    t.coveragePct = stats.coveragePct;

    const auto platform = ml::platformSummary (stats);
    t.platformOverCount = platform.overCount;
    t.platformLimitedDb = platform.maxLimitedDb;
    t.wastedLoudnessDb = platform.maxWastedDb;

    for (const auto& m : markers)
        t.issues.emplace_back (m.time, m.text);

    const auto name = detail.getAlbumName();

    if (name.isNotEmpty() && name != albumSnapshot.albumName)
        session.setAlbumName (name);

    if (reference.hasStats())
        session.setReference (reference.getStats(), analyzer.getReferenceBandDensities(), "reference");

    session.addTrack (t);

    albumNameEditorInitialised = true;
    refreshAlbum();
    detail.setAlbumTrackLabel ("Track " + juce::String ((int) albumSnapshot.tracks.size() + 1));
    detail.invalidateDisplays();
    updateRail();
    repaint();
}

void MasterLensAudioProcessorEditor::exportAlbumProof()
{
    const auto stamp = juce::Time::getCurrentTime().formatted ("%Y%m%d-%H%M");
    const auto defaultFile = juce::File::getSpecialLocation (juce::File::userDesktopDirectory)
                                 .getChildFile ("MasterLens-Album-Proof-" + stamp + ".json");

    fileChooser = std::make_unique<juce::FileChooser> (
        T ("导出专辑交付证明（JSON / TXT）", "Export album delivery proof (JSON / TXT)"),
        defaultFile, "*.json");

    fileChooser->launchAsync (juce::FileBrowserComponent::saveMode
                                  | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this] (const juce::FileChooser& fc)
                              {
                                  const auto file = fc.getResult();

                                  if (file == juce::File {})
                                      return;

                                  auto& session = ml::album::Session::get();
                                  file.replaceWithText (session.exportProofJson());
                                  file.withFileExtension (".txt").replaceWithText (session.exportProofText());
                              });
}

void MasterLensAudioProcessorEditor::importAlbumSnapshot()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        T ("导入专辑基准 / 规则（JSON）", "Import album baseline / rules (JSON)"),
        juce::File::getSpecialLocation (juce::File::userDesktopDirectory), "*.json");

    fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                                  | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
                              {
                                  const auto file = fc.getResult();

                                  if (file != juce::File {} && ml::album::Session::get().importJson (file))
                                  {
                                      refreshAlbum();
                                      detail.invalidateDisplays();
                                      repaint();
                                  }
                              });
}

void MasterLensAudioProcessorEditor::clearAlbum()
{
    ml::album::Session::get().clearTracks();
    refreshAlbum();
    detail.invalidateDisplays();
    repaint();
}

void MasterLensAudioProcessorEditor::setAlbumRule (int index, bool enabled)
{
    auto rules = ml::album::Session::get().snapshot().rules;

    switch (index)
    {
        case 0: rules.blockPlatform = enabled; break;
        case 1: rules.blockTp = enabled; break;
        case 2: rules.blockSignal = enabled; break;
        case 3: rules.blockMono = enabled; break;
        default: rules.blockInconsistency = enabled; break;
    }

    ml::album::Session::get().setRules (rules);
    refreshAlbum();
    repaint();
}

void MasterLensAudioProcessorEditor::auditionRiskDetail (const juce::String& detail)
{
    auto lower = detail.toLowerCase();
    float lo = 20.0f, hi = 20000.0f;
    bool mono = false;

    if (lower.contains ("mono") || lower.contains ("单声道"))
    {
        lo = 60.0f; hi = 250.0f; mono = true;
    }
    else if (lower.contains ("sub") || lower.contains ("超低"))
    {
        lo = 20.0f; hi = 60.0f;
    }
    else if (lower.contains ("mask") || lower.contains ("掩蔽") || lower.contains ("中频"))
    {
        lo = 200.0f; hi = 800.0f;
    }
    else if (lower.contains ("alias") || lower.contains ("混叠"))
    {
        lo = 12000.0f; hi = 20000.0f;
    }
    else if (lower.contains ("distort") || lower.contains ("谐波") || lower.contains ("互调"))
    {
        lo = 2000.0f; hi = 12000.0f;
    }
    else if (lower.contains ("tp") || lower.contains ("true") || lower.contains ("编码"))
    {
        lo = 20.0f; hi = 20000.0f;
    }

    const double end = juce::jmax (0.5, timelineNow());
    processor.startAudition (juce::jmax (0.0, end - 1.5), end, lo, hi, mono);
    lastAuditionIndex = -2;
    footerStatusText = T ("试听风险：", "Audition risk: ") + detail.substring (0, 42);
    footerStatusColour = ml::theme::cyan;
    repaint();
}

void MasterLensAudioProcessorEditor::auditionSignal (int index)
{
    if (! stats.hasSignal || stats.elapsed <= 0.2)
        return;

    if (processor.isAuditionActive() && lastAuditionIndex == index)
    {
        processor.stopAudition();
        lastAuditionIndex = -1;
        footerStatusText = T ("试听已停止", "Audition stopped");
        footerStatusColour = ml::theme::textDim;
        repaint();
        return;
    }

    struct Band { float lo, hi; bool mono; const char* zh; const char* en; };
    static const Band bands[6] {
        { 2000.0f, 12000.0f, false, "谐波/互调 2–12 kHz", "harmonics/IMD 2-12 kHz" },
        { 12000.0f, 20000.0f, false, "混叠 12–20 kHz", "aliasing 12-20 kHz" },
        { 20.0f, 60.0f, false, "超低 20–60 Hz", "sub 20-60 Hz" },
        { 20.0f, 20000.0f, false, "全频瞬态", "full-band transient" },
        { 60.0f, 250.0f, true, "单声道 60–250 Hz", "mono 60-250 Hz" },
        { 200.0f, 800.0f, false, "掩蔽 200–800 Hz", "masking 200-800 Hz" }
    };

    const auto& b = bands[juce::jlimit (0, 5, index)];

    // Live solo: the selected band is filtered from the current DAW input and
    // follows the playhead. It never reads back a loop from history.
    processor.startLiveAudition (b.lo, b.hi, b.mono);
    lastAuditionIndex = index;
    footerStatusText = T ("实时跟随 DAW · ", "Live with DAW · ") + juce::String (T (b.zh, b.en))
                       + T ("  · Esc 停止", "  · Esc stops");
    footerStatusColour = ml::theme::cyan;
    repaint();
}

void MasterLensAudioProcessorEditor::auditionSignalRegion (int index, double start, double end)
{
    if (! stats.hasSignal || end <= start + 0.05)
        return;

    struct Band { float lo, hi; bool mono; const char* zh; const char* en; };
    static const Band bands[6] {
        { 2000.0f, 12000.0f, false, "谐波/互调 2–12 kHz", "harmonics/IMD 2-12 kHz" },
        { 12000.0f, 20000.0f, false, "混叠 12–20 kHz", "aliasing 12-20 kHz" },
        { 20.0f, 60.0f, false, "超低 20–60 Hz", "sub 20-60 Hz" },
        { 20.0f, 20000.0f, false, "全频瞬态", "full-band transient" },
        { 60.0f, 250.0f, true, "单声道 60–250 Hz", "mono 60-250 Hz" },
        { 200.0f, 800.0f, false, "掩蔽 200–800 Hz", "masking 200-800 Hz" }
    };

    const auto& b = bands[juce::jlimit (0, 5, index)];

    // Explicit timestamp loop: 2 s region around the detected problem.
    processor.startAudition (juce::jmax (0.0, start), juce::jmax (start + 0.2, end),
                             b.lo, b.hi, b.mono);
    lastAuditionIndex = index;
    footerStatusText = T ("循环问题区域 ", "Looping region ") + ml::fmtTime (start)
                       + "–" + ml::fmtTime (end) + T (" · Esc 停止", " · Esc stops");
    footerStatusColour = ml::theme::cyan;
    repaint();
}



void MasterLensAudioProcessorEditor::updateSignalProblems()
{
    if (! signalDefects.ready || ! stats.hasSignal || stats.elapsed < 1.0)
        return;

    struct Layer
    {
        float risk;
        float threshold;
        const char* zh;
        const char* en;
    };

    const Layer layers[6]
    {
        { signalDefects.distortionRisk, 55.0f, "谐波 / 互调", "harmonics / IMD" },
        { signalDefects.aliasRisk,      55.0f, "混叠 / 高频", "aliasing / HF" },
        { signalDefects.subRisk,        55.0f, "超低堆积", "sub build-up" },
        { 100.0f - signalDefects.transientScore, 55.0f, "瞬态完整性", "transient" },
        { signalDefects.monoRisk,       45.0f, "单声道折叠", "mono fold" },
        { signalDefects.maskingRisk,    55.0f, "中频掩蔽", "mid masking" }
    };

    const double now = timelineNow();

    for (int i = 0; i < 6; ++i)
    {
        const auto& l = layers[i];

        if (l.risk < l.threshold || now - lastSignalProblemTime[(size_t) i] < 6.0)
            continue;

        ml::SignalProblem p;
        p.start = juce::jmax (0.0, now - 2.0);
        p.end = now;
        p.card = i;
        p.severity = l.risk >= 75.0f ? 2 : 1;
        p.label = T (l.zh, l.en);

        signalProblems.insert (signalProblems.begin(), p);

        if (signalProblems.size() > 24)
            signalProblems.resize (24);

        lastSignalProblemTime[(size_t) i] = now;

        addMarker (p.start, p.severity, 6,
                   T ("问题区域 · ", "Problem region · ") + p.label
                       + " " + ml::fmtTime (p.start) + "–" + ml::fmtTime (p.end));
    }
}

void MasterLensAudioProcessorEditor::setTargetGenre (int index)
{
    targetGenreIndex = juce::jlimit (0, ml::presets::customIndex - 1, index);
    processor.apvts.state.setProperty ("targetGenre", targetGenreIndex, nullptr);
    detail.invalidateDisplays();
    updateRail();
    repaint();
}

void MasterLensAudioProcessorEditor::setTargetWar (float percent)
{
    targetWarPct = juce::jlimit (0.0f, 100.0f, percent);
    processor.apvts.state.setProperty ("targetWarPct", targetWarPct, nullptr);
    detail.invalidateDisplays();
    updateRail();
    repaint();
}

void MasterLensAudioProcessorEditor::applyTargetPlan()
{
    const float lufs = ml::presets::war::warTargetLufs (targetGenreIndex, targetWarPct);
    const auto& e = ml::presets::entries[targetGenreIndex];

    setParameter ("targetLufs", lufs);
    setParameter ("tpCeiling", e.ceiling);
    basisMode = ml::BasisMode::manual;
    processor.apvts.state.setProperty ("basisMode", (int) basisMode, nullptr);
    lraTargetSetting = ml::presets::war::warTargetLra (targetGenreIndex, targetWarPct);
    processor.apvts.state.setProperty ("lraTarget", lraTargetSetting, nullptr);

    syncPresetToParameters();
    updateTexts();
    detail.refreshTexts();
    detail.invalidateDisplays();
    updateRail();
    footerStatusText = T ("已应用曲风目标 ", "Genre target applied ")
                       + ml::num (lufs) + " LUFS";
    footerStatusColour = ml::theme::good;
    repaint();
}

void MasterLensAudioProcessorEditor::setParameter (const char* id, float value)
{
    if (auto* param = processor.apvts.getParameter (id))
        param->setValueNotifyingHost (param->convertTo0to1 (value));
}

//==============================================================================
void MasterLensAudioProcessorEditor::resetAnalysis()
{
    processor.requestReset();
    analyzer.reset();
    advisor.reset();

    curve.clear();
    stats = ml::StatsSnapshot {};
    markers.clear();
    tpEvents.clear();
    issueCache.clear();
    signalProblems.clear();
    lastSignalProblemTime.fill (-100.0);

    // Snapshots describe a musical moment; clearing the session clears them too.
    snapshots[0] = ml::Snapshot {};
    snapshots[1] = ml::Snapshot {};
    viewingSnapshot = -1;
    updateSnapshotChips();

    detail.setIssues ({});
    detail.setHistory ({}, processor.getTargetLufs(), 0.0f);
    timeline.setMarkers ({}, 30.0);
    timeline.setPlayhead (0.0);
    updateRail();
    repaint();
}

//==============================================================================
void MasterLensAudioProcessorEditor::importReference()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        T ("选择 1–4 条参考轨（WAV / AIFF / FLAC / MP3）",
           "Choose 1-4 reference tracks (WAV / AIFF / FLAC / MP3)"),
        juce::File::getSpecialLocation (juce::File::userMusicDirectory),
        "*.wav;*.aif;*.aiff;*.flac;*.mp3");

    fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                                  | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::canSelectMultipleItems,
                              [this] (const juce::FileChooser& fc)
                              {
                                  const auto results = fc.getResults();

                                  if (results.isEmpty())
                                      return;

                                  juce::StringArray files;

                                  for (int i = 0; i < juce::jmin (4, results.size()); ++i)
                                      files.add (results[i].getFullPathName());

                                  reference.load (files, analyzerSampleRate, 13);
                                  processor.referencePlayer.load (files, analyzerSampleRate);
                                  processor.apvts.state.setProperty ("referenceFiles", files.joinIntoString ("|"), nullptr);
                                  analyzer.clearReference();
                              });
}

void MasterLensAudioProcessorEditor::clearReference()
{
    reference.clear();
    processor.referencePlayer.clear();
    analyzer.clearReference();
    processor.apvts.state.setProperty ("referenceFiles", juce::String(), nullptr);
    detail.setReferenceInfo (false, {});
    detail.setReferencePlaybackState (false, {});
}

void MasterLensAudioProcessorEditor::toggleReferencePlayback()
{
    auto& player = processor.referencePlayer;

    if (! player.isReady())
    {
        footerStatusText = T ("先导入参考轨，再开启参考 A/B", "Import a reference track first");
        footerStatusColour = ml::theme::warn;
        repaint();
        return;
    }

    const bool next = ! player.isEnabled();
    player.setEnabled (next);
    detail.setReferencePlaybackState (next, player.getStatus());
    footerStatusText = next ? T ("参考 A/B：播放参考轨", "Reference A/B: playing reference")
                            : T ("参考 A/B：回到本曲", "Reference A/B: back to mix");
    footerStatusColour = next ? ml::theme::cyan : ml::theme::textDim;
    repaint();
}

//==============================================================================
void MasterLensAudioProcessorEditor::exportReport()
{
    const auto stamp = juce::Time::getCurrentTime().formatted ("%Y%m%d-%H%M");
    const auto defaultFile = juce::File::getSpecialLocation (juce::File::userDesktopDirectory)
                                 .getChildFile ("MasterLens-Report-" + stamp + ".txt");

    fileChooser = std::make_unique<juce::FileChooser> (T ("导出检查报告（TXT / JSON / CSV）",
                                                          "Export QC report (TXT / JSON / CSV)"),
                                                       defaultFile, "*.txt");

    fileChooser->launchAsync (juce::FileBrowserComponent::saveMode
                                  | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this] (const juce::FileChooser& fc)
                              {
                                  const auto file = fc.getResult();

                                  if (file == juce::File {})
                                      return;

                                  file.replaceWithText (buildReport());
                                  file.withFileExtension (".json").replaceWithText (buildReportJson());
                                  file.withFileExtension (".csv").replaceWithText (buildReportCsv());
                              });
}
