#include "PluginEditor.h"
#include "file/FFmpegRuntime.h"

using ml::T;

void MasterLensAudioProcessorEditor::showCodecMenu()
{
    const auto* selected = detail.selectedFile();

    if (selected == nullptr || ! selected->ok)
    {
        footerStatusText = T ("先选择一个已分析的文件", "Select an analysed file first");
        footerStatusColour = ml::theme::warn;
        repaint();
        return;
    }

    juce::PopupMenu menu;

    auto& runtime = ml::FFmpegRuntime::get();
    runtime.resolve();
    const bool aacReady = runtime.supports ("aac");
    const bool mp3Ready = runtime.supports ("mp3");

    menu.addSectionHeader (T ("编码后端 / BACKEND", "CODEC BACKEND"));
    menu.addItem (100, runtime.getStatusText(), false, false);

    if (hasCodecPreview)
    {
        const int side = processor.getCodecSide();
        menu.addItem (1, T ("监听 A 原始", "Monitor A source"), true, side == 0);
        menu.addItem (2, T ("监听 B 编码后 ", "Monitor B encoded ") + codecResult.label,
                      true, side == 1);
        menu.addSeparator();
    }

    menu.addItem (3, T ("编码 AAC 256k", "Encode AAC 256k"), aacReady);
    menu.addItem (4, T ("编码 MP3 320k", "Encode MP3 320k"), mp3Ready);

    if (! aacReady && ! mp3Ready)
        menu.addItem (6, T ("编码不可用：", "Codec unavailable: ") + runtime.getStatusText(), false);

    if (hasCodecPreview)
    {
        menu.addSeparator();
        menu.addItem (5, T ("停止编码 A/B 并删除临时文件",
                            "Stop codec A/B and delete temporary files"));
    }

    juce::Component::SafePointer<MasterLensAudioProcessorEditor> safe (this);
    const auto area = detail.codecButtonScreenBounds();

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (area),
                        [safe] (int choice)
                        {
                            if (safe == nullptr || choice == 0)
                                return;

                            if (choice == 1)
                                safe->toggleCodecSide (0);
                            else if (choice == 2)
                                safe->toggleCodecSide (1);
                            else if (choice == 3)
                                safe->startCodecAB ("aac");
                            else if (choice == 4)
                                safe->startCodecAB ("mp3");
                            else if (choice == 5)
                                safe->stopCodecAB();
                        });
}

void MasterLensAudioProcessorEditor::startCodecAB (const juce::String& codec)
{
    const auto* selected = detail.selectedFile();

    if (selected == nullptr || ! selected->ok)
    {
        footerStatusText = T ("先选择一个已分析的文件", "Select an analysed file first");
        footerStatusColour = ml::theme::warn;
        repaint();
        return;
    }

    stopCodecAB();

    auto& runtime = ml::FFmpegRuntime::get();
    runtime.resolve();

    if (! runtime.supports (codec))
    {
        footerStatusText = T ("编码不可用：", "Codec unavailable: ") + runtime.getStatusText();
        footerStatusColour = ml::theme::bad;
        repaint();
        return;
    }

    codecSourcePath = selected->path;
    codecSourceLufs = selected->stats.integrated;
    codecSourceTp = selected->stats.truePeakDb;
    detail.setCodecABState (codec == "aac" ? T ("编码中 AAC…", "Encoding AAC…")
                                           : T ("编码中 MP3…", "Encoding MP3…"), false);

    footerStatusText = T ("编码中：", "Encoding: ")
                       + (codec == "aac" ? juce::String ("AAC 256k") : juce::String ("MP3 320k"))
                       + " · " + runtime.getStatusText();
    footerStatusColour = ml::theme::cyan;
    repaint();

    codecJob.start (juce::File (selected->path), codec);
}


void MasterLensAudioProcessorEditor::applyCodecResult (const ml::CodecABResult& measured)
{
    codecResult = measured;
    hasCodecPreview = true;

    if (measured.decodedFile != juce::File())
        processor.openCodecPreview (measured.decodedFile);

    detail.setCodecABState (T ("A 原始 / B ", "A source / B ") + measured.label, true);

    if (const auto* selected = detail.selectedFile())
        detail.setFileCodecResult (selected->path, measured.label, measured.decodedLufs,
                                   measured.decodedTp, measured.encodedBytes);

    footerStatusText = measured.label + T (" 就绪 · ", " ready · ")
                       + T ("ΔLUFS ", "ΔLUFS ") + ml::sgn (measured.decodedLufs - codecSourceLufs, 1)
                       + T (" · ΔTP ", " · ΔTP ") + ml::sgn (measured.decodedTp - codecSourceTp, 1)
                       + " dB" + (measured.backend.isNotEmpty()
                                      ? " · " + measured.backend + " " + measured.ffmpegVersion
                                      : juce::String())
                       + T (" · 点按钮或按 B 切换", " · click or press B to switch");
    footerStatusColour = ml::theme::good;
    repaint();
}

bool MasterLensAudioProcessorEditor::applyCodecReadyForSnapshot (const juce::String& label,
                                                                 float lufs, float tp,
                                                                 juce::int64 bytes)
{
    ml::CodecABResult measured;
    measured.ok = true;
    measured.codec = label.startsWithIgnoreCase ("mp3") ? "mp3" : "aac";
    measured.label = label;
    measured.decodedLufs = lufs;
    measured.decodedTp = tp;
    measured.encodedBytes = bytes;

    if (detail.selectedFile() != nullptr)
        codecSourcePath = detail.selectedFile()->path;

    codecSourceLufs = detail.selectedFile() != nullptr ? detail.selectedFile()->stats.integrated
                                                       : ml::kSilentDb;
    codecSourceTp = detail.selectedFile() != nullptr ? detail.selectedFile()->stats.truePeakDb
                                                     : ml::kSilentDb;

    applyCodecResult (measured);
    return hasCodecPreview && detail.isCodecABReady();
}

void MasterLensAudioProcessorEditor::applyCodecEncodingForSnapshot (bool aac)
{
    detail.setCodecABState (aac ? T ("编码中 AAC…", "Encoding AAC…")
                                : T ("编码中 MP3…", "Encoding MP3…"), false);
    footerStatusText = T ("编码中：", "Encoding: ")
                       + (aac ? juce::String ("AAC 256k") : juce::String ("MP3 320k"));
    footerStatusColour = ml::theme::cyan;
    repaint();
}

void MasterLensAudioProcessorEditor::applyCodecFailureForSnapshot (const juce::String& error)
{
    hasCodecPreview = false;
    detail.setCodecABState (T ("编码 A/B", "Codec A/B"), true);
    footerStatusText = T ("编码 A/B 失败：", "Codec A/B failed: ") + error;
    footerStatusColour = ml::theme::bad;
    repaint();
}

void MasterLensAudioProcessorEditor::pollCodecAB()
{
    ml::CodecABResult measured;

    if (! codecJob.takeResult (measured))
        return;

    if (! measured.ok)
    {
        hasCodecPreview = false;
        detail.setCodecABState (T ("编码 A/B", "Codec A/B"), true);
        footerStatusText = T ("编码 A/B 失败：", "Codec A/B failed: ") + measured.error;
        footerStatusColour = ml::theme::bad;
        repaint();
        return;
    }

    const auto* selected = detail.selectedFile();

    if (selected == nullptr || selected->path != codecSourcePath)
    {
        hasCodecPreview = false;
        codecResult = ml::CodecABResult {};
        detail.setCodecABState (T ("编码 A/B", "Codec A/B"), selected != nullptr);
        return;
    }

    applyCodecResult (measured);
}

void MasterLensAudioProcessorEditor::toggleCodecSide()
{
    if (! hasCodecPreview)
    {
        footerStatusText = T ("先生成编码预览", "Render a codec preview first");
        footerStatusColour = ml::theme::warn;
        repaint();
        return;
    }

    toggleCodecSide (processor.getCodecSide() == 0 ? 1 : 0);
}

void MasterLensAudioProcessorEditor::toggleCodecSide (int side)
{
    if (! hasCodecPreview)
        return;

    const int next = side == 1 ? 1 : 0;
    processor.setCodecSide (next);
    detail.setCodecABState (next == 0 ? T ("A 原始 / B ", "A source / B ") + codecResult.label
                                      : T ("B ", "B ") + codecResult.label
                                            + T (" / A 原始", " / A source"),
                            true);

    footerStatusText = next == 0
        ? T ("监听 A · 原始文件", "Monitoring A · source")
        : T ("监听 B · ", "Monitoring B · ") + codecResult.label
              + T (" · ΔLUFS ", " · ΔLUFS ") + ml::sgn (codecResult.decodedLufs - codecSourceLufs, 1)
              + T (" · ΔTP ", " · ΔTP ") + ml::sgn (codecResult.decodedTp - codecSourceTp, 1) + " dB";
    footerStatusColour = next == 0 ? ml::theme::cyan : ml::theme::warn;
    repaint();
}

void MasterLensAudioProcessorEditor::stopCodecAB()
{
    processor.stopCodecPreview();

    hasCodecPreview = false;
    codecResult = ml::CodecABResult {};
    codecSourcePath.clear();
    codecJob.clear();

    if (filePreviewPath.isEmpty())
        detail.setFilePlayActive (false);

    detail.setCodecABState (T ("编码 A/B", "Codec A/B"), true);
}
