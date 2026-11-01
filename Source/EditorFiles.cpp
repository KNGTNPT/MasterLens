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
                              [this] (const juce::FileChooser& fc)
                              {
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
                              [this] (const juce::FileChooser& fc)
                              {
                                  const auto folder = fc.getResult();

                                  if (folder.isDirectory())
                                      startFileAnalysis ({ folder.getFullPathName() });
                              });
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

        const auto gate = ml::album::Session::evaluate (ml::album::Session::get().snapshot());
        footerStatusText = gate.blocked ? T ("文件已入专辑 · 门禁阻断", "Files added · gate blocked")
                                        : T ("文件已入专辑 · 门禁通过", "Files added · gate pass");
        footerStatusColour = gate.blocked ? ml::theme::bad : ml::theme::good;
        repaint();
    }
}

void MasterLensAudioProcessorEditor::addFileResultToAlbum (const ml::FileAnalysisResult& result)
{
    if (! result.ok)
        return;

    auto& session = ml::album::Session::get();

    if (detail.getAlbumName().isEmpty() && pendingAlbumName.isNotEmpty())
    {
        session.setAlbumName (pendingAlbumName);
        detail.setAlbumName (pendingAlbumName);
    }

    ml::album::TrackRecord t;
    t.id = juce::String::toHexString (juce::Time::getCurrentTime().toMilliseconds())
           + "-" + result.fileName;
    t.label = juce::File (result.fileName).getFileNameWithoutExtension();
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
