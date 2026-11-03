#include "PluginEditor.h"

#include "Version.h"

using ml::T;

namespace
{
    juce::String formatFileStatus (bool busy, int done, int total, const juce::String& current)
    {
        if (busy)
            return "Analysing " + juce::String (done) + "/" + juce::String (total)
                   + (current.isNotEmpty() ? "  |  " + current : juce::String());

        if (total > 0)
            return "Done  |  " + juce::String (done) + "/" + juce::String (total);

        return {};
    }
} // namespace

bool MasterLensAudioProcessorEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& path : files)
    {
        const juce::File f (path);

        if (f.isDirectory() || ml::FileAnalyzer::isSupportedFile (f))
            return true;
    }

    return false;
}

void MasterLensAudioProcessorEditor::setExportResult (bool ok)
{
    footerStatusText = ok ? T ("导出完成", "Export complete") : T ("导出失败：请检查权限和磁盘空间", "Export failed: check permissions and disk space");
    footerStatusColour = ok ? ml::theme::good : ml::theme::bad;
    repaint();
}

void MasterLensAudioProcessorEditor::filesDropped (const juce::StringArray& files, int, int)
{
    startFileAnalysis (files);
}

void MasterLensAudioProcessorEditor::chooseFiles()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        T ("选择要离线分析的音频文件", "Choose audio files for offline analysis"),
        juce::File::getSpecialLocation (juce::File::userMusicDirectory),
        "*.wav;*.aif;*.aiff;*.flac");

    fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                                  | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::canSelectMultipleItems,
                              [this, lifetime = juce::Component::SafePointer<MasterLensAudioProcessorEditor> (this)] (const juce::FileChooser& fc)
                              {
                                  if (lifetime == nullptr) return;
                                  juce::StringArray paths;

                                  for (const auto& f : fc.getResults())
                                      if (f.existsAsFile())
                                          paths.add (f.getFullPathName());

                                  if (! paths.isEmpty())
                                      startFileAnalysis (paths);
                              });
}

void MasterLensAudioProcessorEditor::chooseFolder()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        T ("选择专辑文件夹", "Choose an album folder"),
        juce::File::getSpecialLocation (juce::File::userMusicDirectory));

    fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                                  | juce::FileBrowserComponent::canSelectDirectories,
                              [this, lifetime = juce::Component::SafePointer<MasterLensAudioProcessorEditor> (this)] (const juce::FileChooser& fc)
                              {
                                  if (lifetime == nullptr) return;
                                  const auto folder = fc.getResult();

                                  if (folder.isDirectory())
                                      startFileAnalysis ({ folder.getFullPathName() });
                              });
}

void MasterLensAudioProcessorEditor::toggleFilePlayback()
{
    if (processor.wrapperType != juce::AudioProcessor::wrapperType_Standalone)
        return;

    const auto* selected = detail.selectedFile();

    if (selected == nullptr || ! selected->ok)
    {
        footerStatusText = T ("先选择一个已分析的文件", "Select an analysed file first");
        footerStatusColour = ml::theme::warn;
        repaint();
        return;
    }

    if (processor.isFilePreviewActive() && filePreviewPath == selected->path)
    {
        processor.stopFilePreview();
        filePreviewPath.clear();
        detail.setFilePlayActive (false);
        footerStatusText = T ("文件播放已停止", "File playback stopped");
        footerStatusColour = ml::theme::textDim;
        repaint();
        return;
    }

    processor.stopFilePreview();
    filePreviewPath = selected->path;
    const double duration = juce::jmax (1.0, selected->durationSeconds);
    processor.startFilePreview (juce::File (selected->path), 0.0, duration, false);
    detail.setFilePlayActive (true);
    footerStatusText = T ("独立版播放 · ", "Standalone playback · ") + selected->fileName;
    footerStatusColour = ml::theme::accent;
    repaint();
}

void MasterLensAudioProcessorEditor::startFileAnalysis (const juce::StringArray& paths)
{
    const auto expanded = ml::FileAnalyzer::expandPaths (paths);

    if (expanded.isEmpty())
    {
        footerStatusText = T ("没有找到 WAV / AIFF / FLAC 文件",
                              "No WAV / AIFF / FLAC files found");
        footerStatusColour = ml::theme::warn;
        repaint();
        return;
    }

    if (fileAnalyzer.isBusy())
    {
        footerStatusText = T ("上一批文件仍在分析", "The previous batch is still running");
        footerStatusColour = ml::theme::warn;
        repaint();
        return;
    }

    juce::File first (expanded[0]);
    pendingAlbumName = first.isDirectory() ? first.getFileName()
                                           : first.getParentDirectory().getFileName();

    if (pendingAlbumName.isEmpty() || pendingAlbumName == "/")
        pendingAlbumName = "File QC";

    fileResults.clear();
    fileAnalysisStatus = T ("准备分析 ", "Preparing ") + juce::String (expanded.size())
                         + T (" 个文件", " files");
    detail.setFileResults ({}, true, 0, expanded.size(), fileAnalysisStatus);
    setPage ((int) ml::PageId::files);

    if (! fileAnalyzer.analyse (expanded))
    {
        footerStatusText = T ("无法启动文件分析", "Could not start file analysis");
        footerStatusColour = ml::theme::bad;
        repaint();
        return;
    }

    footerStatusText = T ("离线分析中 · ", "Offline analysis · ") + juce::String (expanded.size())
                       + T (" 个文件", " files");
    footerStatusColour = ml::theme::cyan;
    repaint();
}


void MasterLensAudioProcessorEditor::pollFileAnalysis()
{
    auto incoming = fileAnalyzer.takeResults();
    bool added = false;

    for (auto& result : incoming)
    {
        const auto reviews = processor.albumSession.snapshot().reviewedEvidence;
        for (auto& finding : result.forensic.findings)
            finding.reviewed = reviews.contains (result.forensic.evidenceKey (finding));
        if (result.ok)
        {
            addFileResultToAlbum (result);
            added = true;
        }

        fileResults.push_back (std::move (result));
    }

    const bool busy = fileAnalyzer.isBusy();
    const int done = fileAnalyzer.getDone();
    const int total = fileAnalyzer.getTotal();
    const auto current = fileAnalyzer.getCurrentFile();

    if (busy || ! fileResults.empty())
        fileAnalysisStatus = formatFileStatus (busy, done, total, current);

    if (fileResults.empty() && ! busy)
        fileAnalysisStatus = {};

    detail.setFileResults (fileResults, busy, done, total, fileAnalysisStatus);

    if (added)
    {
        refreshAlbum();
        updateRail();
        detail.invalidateDisplays();

        const auto gate = ml::album::Session::evaluate (processor.albumSession.snapshot());
        footerStatusText = ! gate.passed() ? T ("文件已入专辑 · 门禁未通过", "Files added · gate not passed")
                                        : T ("文件已入专辑 · 门禁通过", "Files added · gate pass");
        footerStatusColour = gate.passed() ? ml::theme::good : ml::theme::bad;
        repaint();
    }
}

void MasterLensAudioProcessorEditor::addFileResultToAlbum (const ml::FileAnalysisResult& result)
{
    if (! result.ok)
        return;

    auto& session = processor.albumSession;

    if (detail.getAlbumName().isEmpty() && pendingAlbumName.isNotEmpty())
    {
        session.setAlbumName (pendingAlbumName);
        detail.setAlbumName (pendingAlbumName);
    }

    ml::album::TrackRecord t;
    t.label = juce::File (result.fileName).getFileNameWithoutExtension();
    t.id = ml::album::stableTrackId (t.label, result.path);
    t.capturedAt = juce::Time::getCurrentTime().toISO8601 (true);
    t.stats = result.stats;
    t.defects = result.defects;
    t.bandDb = result.bandDb;
    t.tpCeiling = processor.getCeilingDb();

    const auto platform = ml::platformSummary (result.stats);
    t.platformOverCount = platform.overCount;
    t.platformLimitedDb = platform.maxLimitedDb;
    t.wastedLoudnessDb = platform.maxWastedDb;

    t.sourceFile = result.fileName;
    t.pluginVersion = ml::kVersion;
    t.durationSeconds = result.durationSeconds;
    t.sampleRate = result.sampleRate;
    t.bitDepth = result.bitDepth;
    t.coveragePct = 100.0f;
    t.fileForensics = result.forensic.toVar (result.sampleRate);
    for (const auto& f : result.forensic.findings)
        t.issues.emplace_back (f.startSample / result.sampleRate,
                                ml::findingText (f, ml::Localisation::isEnglish()));

    session.addTrack (t);
}

void MasterLensAudioProcessorEditor::addFileResultsToAlbum()
{
    if (fileResults.empty())
        return;

    for (const auto& result : fileResults)
        addFileResultToAlbum (result);

    refreshAlbum();
    updateRail();
    detail.invalidateDisplays();
    footerStatusText = T ("文件结果已写入专辑", "File results written to album");
    footerStatusColour = ml::theme::good;
    repaint();
}

void MasterLensAudioProcessorEditor::clearFileResults()
{
    fileResults.clear();
    fileAnalysisStatus = {};
    detail.setFileResults ({}, false, 0, 0, {});
    repaint();
}

void MasterLensAudioProcessorEditor::exportFileTable()
{
    if (detail.getFileResults().empty())
    {
        footerStatusText = T ("没有可导出的文件结果", "No file results to export");
        footerStatusColour = ml::theme::warn;
        repaint();
        return;
    }

    fileChooser = std::make_unique<juce::FileChooser> (
        T ("导出文件 QC 表", "Export File QC table"),
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
            .getChildFile ("MasterLens-FileQC.csv"),
        "*.csv");

    fileChooser->launchAsync (juce::FileBrowserComponent::saveMode
                                  | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this, lifetime = juce::Component::SafePointer<MasterLensAudioProcessorEditor> (this)] (const juce::FileChooser& fc)
                              {
                                  if (lifetime == nullptr) return;
                                  const auto& exportResults = detail.getFileResults();
                                  auto file = fc.getResult();

                                  if (file == juce::File())
                                      return;

                                  file = file.withFileExtension ("csv");

                                  const auto jsonFile = file.withFileExtension ("json");
                                  for (const auto& r : exportResults)
                                      if (file == juce::File (r.path) || jsonFile == juce::File (r.path))
                                      {
                                          footerStatusText = T ("报告不能覆盖音频源文件", "Report cannot overwrite audio sources");
                                          footerStatusColour = ml::theme::bad;
                                          repaint();
                                          return;
                                      }
                                  auto* root = new juce::DynamicObject();
                                  root->setProperty ("product", "MasterLens");
                                  root->setProperty ("version", ml::kVersion);
                                  root->setProperty ("schemaVersion", 3);
                                  root->setProperty ("generatedAt", juce::Time::getCurrentTime().toISO8601 (true));
                                  const auto basis = currentBasis();
                                  const float target = basis.hasComparison() ? basis.comparisonLufs()
                                      : (basis.mode == ml::BasisMode::manual ? processor.getTargetLufs() : -14.0f);
                                  juce::Array<juce::var> files;
                                  for (const auto& r : exportResults)
                                      files.add (ml::fileResultToVar (r, target, processor.getCeilingDb(), &deliverySpec));
                                  root->setProperty ("files", files);
                                  root->setProperty ("deliverySpec", deliverySpec.toVar());
                                  if (ml::writeTextAtomic (file, ml::fileResultsCsv (exportResults, target, processor.getCeilingDb()))
                                      && ml::writeTextAtomic (jsonFile, juce::JSON::toString (juce::var (root), true)))
                                  {
                                      footerStatusText = T ("CSV + JSON 已导出 · ", "CSV + JSON exported · ")
                                                         + file.getFileName();
                                      footerStatusColour = ml::theme::good;
                                  }
                                  else
                                  {
                                      footerStatusText = T ("无法写入文件表", "Could not write file table");
                                      footerStatusColour = ml::theme::bad;
                                  }

                                  repaint();
                              });
}
