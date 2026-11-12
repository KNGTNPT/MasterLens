#include "PluginEditor.h"

using ml::T;

namespace
{
juce::File projectDefaultFile()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
        .getChildFile ("MasterLens-Project.json");
}
}

void MasterLensAudioProcessorEditor::showProjectMenu()
{
    juce::PopupMenu menu;
    menu.addItem (1, T ("保存项目…", "Save project…"));
    menu.addItem (2, T ("打开项目…", "Open project…"));
    menu.addSeparator();
    menu.addItem (3, T ("导出差异证明…", "Export version diff…"));
    menu.addSeparator();
    menu.addItem (4, T ("导入交付规格 JSON…", "Load delivery spec JSON…"));
    menu.addItem (5, T ("导出规格模板…", "Export spec template…"));
    menu.addSeparator();
    menu.addItem (6, watchFolderEnabled ? T ("停止文件夹监控", "Stop folder watch")
                                        : T ("监控文件夹…", "Watch folder…"));
    menu.addItem (7, T ("停止文件证据试听", "Stop file preview"), processor.isFilePreviewActive());

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&detail),
                        [safe = juce::Component::SafePointer<MasterLensAudioProcessorEditor> (this)] (int result)
    {
        if (safe == nullptr || result <= 0)
            return;

        if (result == 1) safe->saveProjectAs();
        else if (result == 2) safe->openProject();
        else if (result == 3) safe->exportAlbumProof();
        else if (result == 4) safe->loadDeliverySpec();
        else if (result == 5) safe->exportDeliverySpecTemplate();
        else if (result == 6) safe->toggleFolderWatch();
        else if (result == 7) safe->processor.stopFilePreview();
    });
}

void MasterLensAudioProcessorEditor::saveProjectAs()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        T ("保存 MasterLens 项目", "Save MasterLens project"), projectDefaultFile(), "*.json");

    fileChooser->launchAsync (juce::FileBrowserComponent::saveMode
                                  | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this, lifetime = juce::Component::SafePointer<MasterLensAudioProcessorEditor> (this)] (const juce::FileChooser& fc)
                              {
                                  if (lifetime == nullptr) return;
                                  auto file = fc.getResult();
                                  if (file == juce::File())
                                      return;
                                  file = file.withFileExtension ("json");
                                  const auto gate = ml::album::Session::evaluate (processor.albumSession.snapshot());
                                  const auto version = "v" + juce::String (processor.albumSession.snapshot().revision)
                                                       + (gate.blocked ? "-blocked" : (gate.incomplete ? "-incomplete" : "-pass"));
                                  if (processor.albumSession.saveProject (file, version))
                                  {
                                      const auto parsed = juce::JSON::parse (file.loadFileAsString());
                                      if (auto* root = parsed.getDynamicObject())
                                      {
                                          root->setProperty ("deliverySpec", deliverySpec.toVar());
                                          if (! ml::writeTextAtomic (file, juce::JSON::toString (juce::var (root), true))) { setExportResult (false); return; }
                                      }
                                      footerStatusText = T ("项目已保存 · ", "Project saved · ") + file.getFileName();
                                      footerStatusColour = ml::theme::good;
                                  }
                                  else
                                  {
                                      footerStatusText = T ("无法保存项目", "Could not save project");
                                      footerStatusColour = ml::theme::bad;
                                  }
                                  repaint();
                              });
}

void MasterLensAudioProcessorEditor::openProject()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        T ("打开 MasterLens 项目", "Open MasterLens project"),
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.json");

    fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                                  | juce::FileBrowserComponent::canSelectFiles,
                              [this, lifetime = juce::Component::SafePointer<MasterLensAudioProcessorEditor> (this)] (const juce::FileChooser& fc)
                              {
                                  if (lifetime == nullptr) return;
                                  const auto file = fc.getResult();
                                  if (file == juce::File()
                                      || ! processor.albumSession.loadProject (file))
                                  {
                                      footerStatusText = T ("项目读取失败", "Could not open project");
                                      footerStatusColour = ml::theme::bad;
                                      repaint();
                                      return;
                                  }

                                  const auto parsed = juce::JSON::parse (file.loadFileAsString());
                                  if (auto* root = parsed.getDynamicObject())
                                      deliverySpec = ml::DeliverySpec::fromVar (root->getProperty ("deliverySpec"));

                                  refreshAlbum();
                                  detail.setDeliverySpec (deliverySpec);
                                  detail.invalidateDisplays();
                                  footerStatusText = T ("项目已打开 · ", "Project opened · ") + file.getFileName();
                                  footerStatusColour = ml::theme::good;
                                  repaint();
                              });
}

void MasterLensAudioProcessorEditor::toggleFolderWatch()
{
    if (watchFolderEnabled)
    {
        watchFolderEnabled = false;
        watchedFolder = juce::File();
        watchedStamps.clear();
        footerStatusText = T ("文件夹监控已停止", "Folder watch stopped");
        footerStatusColour = ml::theme::textDim;
        repaint();
        return;
    }

    fileChooser = std::make_unique<juce::FileChooser> (
        T ("选择要监控的文件夹", "Choose a folder to watch"),
        juce::File::getSpecialLocation (juce::File::userMusicDirectory));

    fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                                  | juce::FileBrowserComponent::canSelectDirectories,
                              [this, lifetime = juce::Component::SafePointer<MasterLensAudioProcessorEditor> (this)] (const juce::FileChooser& fc)
                              {
                                  if (lifetime == nullptr) return;
                                  const auto folder = fc.getResult();
                                  if (! folder.isDirectory())
                                      return;

                                  watchedFolder = folder;
                                  watchedStamps.clear();
                                  for (const auto& path : ml::FileAnalyzer::expandPaths ({ folder.getFullPathName() }))
                                  {
                                      const juce::File f (path);
                                      watchedStamps[path] = { f.getSize(), f.getLastModificationTime().toMilliseconds() };
                                  }
                                  watchFolderEnabled = true;
                                  lastWatchScanMs = juce::Time::getMillisecondCounterHiRes();
                                  footerStatusText = T ("正在监控 · ", "Watching · ") + folder.getFileName();
                                  footerStatusColour = ml::theme::accent;
                                  repaint();
                              });
}

void MasterLensAudioProcessorEditor::pollFolderWatch()
{
    if (! watchFolderEnabled || ! watchedFolder.isDirectory() || fileAnalyzer.isBusy())
        return;

    const auto now = juce::Time::getMillisecondCounterHiRes();
    if (now - lastWatchScanMs < 2000.0)
        return;
    lastWatchScanMs = now;

    const auto files = ml::FileAnalyzer::expandPaths ({ watchedFolder.getFullPathName() });
    juce::StringArray modified, removed;
    std::map<juce::String, std::pair<juce::int64, juce::int64>> next;

    for (const auto& path : files)
    {
        const juce::File f (path);
        const auto stamp = std::make_pair (f.getSize(), f.getLastModificationTime().toMilliseconds());
        next[path] = stamp;
        auto it = watchedStamps.find (path);
        if (it == watchedStamps.end() || it->second != stamp)
            modified.add (path);
    }

    for (const auto& old : watchedStamps)
        if (next.find (old.first) == next.end())
            removed.add (old.first);

    watchedStamps = std::move (next);

    // 3.4: per-file incremental. Only changed files go back to the analyser;
    // unchanged rows stay in the table, deleted files leave it.
    for (const auto& path : removed)
        fileResults.erase (std::remove_if (fileResults.begin(), fileResults.end(),
                                           [&path] (const ml::FileAnalysisResult& r) { return r.path == path; }),
                           fileResults.end());

    if (! modified.isEmpty())
    {
        footerStatusText = T ("正在复检 ", "Re-checking ")
                           + juce::String (modified.size()) + T (" 个变更文件…", " changed files…");
        footerStatusColour = ml::theme::cyan;
        repaint();
        startFileAnalysis (modified, true);
    }
    else if (! removed.isEmpty())
    {
        detail.setFileResults (fileResults, false, 0, 0, {});
        footerStatusText = T ("已移除 ", "Removed ") + juce::String (removed.size())
                           + T (" 个文件", " files");
        footerStatusColour = ml::theme::textDim;
        repaint();
    }
}

void MasterLensAudioProcessorEditor::loadDeliverySpec()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        T ("导入交付规格 JSON", "Load delivery spec JSON"),
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.json");

    fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                                  | juce::FileBrowserComponent::canSelectFiles,
                              [this, lifetime = juce::Component::SafePointer<MasterLensAudioProcessorEditor> (this)] (const juce::FileChooser& fc)
                              {
                                  if (lifetime == nullptr) return;
                                  const auto file = fc.getResult();
                                  if (file == juce::File())
                                      return;

                                  const auto parsed = juce::JSON::parse (file.loadFileAsString());
                                  if (auto* root = parsed.getDynamicObject())
                                  {
                                      ml::DeliverySpec candidate; juce::String error;
                                      if (! ml::DeliverySpec::parse (parsed, candidate, error))
                                      { footerStatusText = error; footerStatusColour = ml::theme::bad; repaint(); return; }
                                      deliverySpec = candidate;
                                      deliverySpec.enabled = true;
                                      detail.setDeliverySpec (deliverySpec);
                                      detail.invalidateDisplays();
                                      footerStatusText = T ("交付规格已载入", "Delivery spec loaded");
                                      footerStatusColour = ml::theme::good;
                                      repaint();
                                  }
                              });
}

void MasterLensAudioProcessorEditor::exportDeliverySpecTemplate()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        T ("导出交付规格模板", "Export delivery spec template"),
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
            .getChildFile ("MasterLens-Delivery-Spec.json"), "*.json");

    fileChooser->launchAsync (juce::FileBrowserComponent::saveMode
                                  | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this, lifetime = juce::Component::SafePointer<MasterLensAudioProcessorEditor> (this)] (const juce::FileChooser& fc)
                              {
                                  if (lifetime == nullptr) return;
                                  auto file = fc.getResult();
                                  if (file == juce::File())
                                      return;

                                  file = file.withFileExtension ("json");
                                  auto* root = new juce::DynamicObject();
                                  root->setProperty ("enabled", true);
                                  root->setProperty ("targetLufs", deliverySpec.targetLufs);
                                  root->setProperty ("ceilingDbtp", deliverySpec.ceilingDbtp);
                                  root->setProperty ("loudnessToleranceLu", deliverySpec.loudnessToleranceLu);
                                  root->setProperty ("sampleRate", deliverySpec.sampleRate);
                                  root->setProperty ("bitDepth", deliverySpec.bitDepth);
                                  root->setProperty ("channels", deliverySpec.channels);
                                  root->setProperty ("namingPattern", deliverySpec.namingPattern);
                                  if (! ml::writeTextAtomic (file, juce::JSON::toString (juce::var (root), true))) { setExportResult (false); return; }
                                  footerStatusText = T ("规格模板已导出", "Spec template exported");
                                  footerStatusColour = ml::theme::good;
                                  repaint();
                              });
}
