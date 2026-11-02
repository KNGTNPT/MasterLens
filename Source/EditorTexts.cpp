#include "PluginEditor.h"

using ml::T;
#include "Version.h"

//==============================================================================
void MasterLensAudioProcessorEditor::updateTexts()
{
    const bool en = ml::Localisation::isEnglish();

    holdButton.setButtonText (frozen ? T ("恢复", "Resume") : T ("保持", "Hold"));
    clearButton.setButtonText (T ("清零", "Clear"));
    aboutButton.setButtonText (T ("关于", "About"));
    settingsButton.setButtonText (T ("设置", "Settings"));
    themeButton.setButtonText (ml::theme::dark ? T ("深色", "Dark") : T ("浅色", "Light"));
    languageButton.setButtonText (en ? juce::String::fromUTF8 ("中文") : juce::String ("EN"));
    exportButton.setButtonText (T ("导出报告", "Export"));

    const float trim = processor.getMonitorTrimDb();
    monitorButton.setButtonText (processor.isMonitorMatch()
                                     ? T ("等响 ", "match ") + ml::sgn (trim) + " dB"
                                     : T ("等响监听", "Match"));
    monitorButton.setTooltip (T ("等响监听：按参考 LUFS 自动衰减 / 提升监听电平；关闭时输出逐位不变",
                                 "Loudness-matched monitor: trim to the reference LUFS; bypassed when off"));

    targetCaption.setText ("REF", juce::dontSendNotification);
    ceilingCaption.setText ("CEIL", juce::dontSendNotification);

    targetSlider.setEnabled (basisMode == ml::BasisMode::manual);
    targetSlider.setTooltip (T ("选择“手动参考值”后可键入目标 LUFS", "Choose Manual reference to type a target LUFS"));
    ceilingSlider.setTooltip (T ("可直接键入真峰值上限，失焦后生效", "Type the true-peak ceiling directly"));

    updatingPreset = true;
    presetBox.clear (juce::dontSendNotification);
    presetBox.addItem (T ("平台归一化后果", "Platform normalisation"), 1);
    presetBox.addItem (T ("参考轨基准", "Reference tracks"), 2);
    presetBox.addItem (T ("专辑中位基准", "Album median"), 3);
    presetBox.addItem (T ("手动参考值", "Manual reference"), 4);
    presetBox.setSelectedId ((int) basisMode + 1, juce::dontSendNotification);
    updatingPreset = false;

    presetBox.setTooltip (T ("主基准：平台归一化后果 / 参考轨 / 专辑中位 / 手动参考。"
                             "固定曲风响度不再作为合格线。",
                             "Primary basis: platform consequence / reference tracks / album median / "
                             "manual. Fixed genre loudness is no longer a pass line."));

    infoButton.setVisible (false);

    holdButton.setTooltip (T ("保持 (Hold)：锁定当前读数，便于对比改动前后；再次点击恢复实时（按住 Space）",
                              "Hold: freezes the current readings for before / after comparison; "
                              "press again to resume (hold Space)"));
    clearButton.setTooltip (T ("清零统计：Integrated / LRA / 真峰值超限计数归零，不改变任何参数（R）",
                               "Clear stats: resets Integrated / LRA / true-peak overs. "
                               "Parameters are untouched (R)"));
    targetSlider.setTooltip (T ("可键入数值，方向键步进 0.1 LU，双击复位 −14.0",
                                "Type a value, use the arrow keys for 0.1 LU steps, "
                                "double-click to return to -14.0"));
    ceilingSlider.setTooltip (T ("可键入数值，方向键步进 0.1 dB，双击复位 −1.0",
                                 "Type a value, use the arrow keys for 0.1 dB steps, "
                                 "double-click to return to -1.0"));
    themeButton.setTooltip (T ("切换深色 / 浅色主题", "Switch dark / light theme"));
    languageButton.setTooltip (T ("切换界面语言", "Switch interface language"));
    aboutButton.setTooltip (T ("关于 MasterLens", "About MasterLens"));
    settingsButton.setTooltip (T ("设置：界面缩放 100–200%，字号与布局同步放大（⌘ + / ⌘ − 快速调整）",
                                  "Settings: interface scale 100-200% - type and layout zoom together "
                                  "(Cmd + / Cmd -)"));
    exportButton.setTooltip (T ("导出 TXT / JSON / CSV 检查报告（⌘E）",
                                "Export TXT / JSON / CSV QC report (Cmd+E)"));
}

//==============================================================================
bool MasterLensAudioProcessorEditor::keyPressed (const juce::KeyPress& key)
{
    if (auto* focused = juce::Component::getCurrentlyFocusedComponent())
        if (dynamic_cast<juce::TextEditor*> (focused) != nullptr)
            return false;

    if (key == juce::KeyPress::escapeKey && processor.isAuditionActive())
    {
        processor.stopAudition();
        lastAuditionIndex = -1;
        footerStatusText = T ("试听已停止", "Audition stopped");
        footerStatusColour = ml::theme::textDim;
        repaint();
        return true;
    }

    if (key == juce::KeyPress::spaceKey)
    {
        holdFromKeyboard = true;
        setHold (true, true);
        return true;
    }

    if (key.getModifiers().isCommandDown())
    {
        const auto scaleChar = key.getTextCharacter();

        if (scaleChar == '=' || scaleChar == '+' || scaleChar == '-')
        {
            const int step = juce::jlimit (0, (int) ml::ui::scaleSteps.size() - 1,
                                           ml::ui::stepIndexFor (ml::ui::scale)
                                               + (scaleChar == '-' ? -1 : 1));
            setUiScale (ml::ui::scaleSteps[(size_t) step], true);
            return true;
        }
    }

    if (key.getModifiers().isCommandDown() && (key.getTextCharacter() == 'e' || key.getTextCharacter() == 'E'))
    {
        exportReport();
        return true;
    }

    const auto c = key.getTextCharacter();

    if (c == 'r' || c == 'R')
    {
        resetAnalysis();
        return true;
    }

    if (c >= '1' && c <= '9')
    {
        setPage ((int) (c - '1'));
        return true;
    }

    if (c == '0')
    {
        setPage ((int) ml::PageId::target);
        return true;
    }

    if (c == 'f' || c == 'F')
    {
        setPage ((int) ml::PageId::files);
        return true;
    }

    if (key == juce::KeyPress::escapeKey)
    {
        if (settingsPanel.isVisible())
            settingsPanel.setVisibleAndFocus (false);
        else if (aboutPanel.isVisible())
            aboutPanel.setVisibleAndFocus (false);
        else if (genrePanel.isVisible())
            genrePanel.setVisibleAndFocus (false);
        else
            setHold (false, false);

        return true;
    }

    return false;
}

bool MasterLensAudioProcessorEditor::keyPressed (const juce::KeyPress& key, juce::Component*)
{
    return keyPressed (key);
}

bool MasterLensAudioProcessorEditor::keyStateChanged (bool)
{
    if (holdFromKeyboard && ! juce::KeyPress::isKeyCurrentlyDown (juce::KeyPress::spaceKey))
    {
        holdFromKeyboard = false;
        setHold (false, false);
        return true;
    }

    return false;
}

void MasterLensAudioProcessorEditor::setHold (bool shouldHold, bool fromKeyboard)
{
    frozen = shouldHold;
    holdFromKeyboard = fromKeyboard && shouldHold;

    if (holdButton.getToggleState() != frozen)
        holdButton.setToggleState (frozen, juce::dontSendNotification);

    updateTexts();
    repaint();
}

//==============================================================================
void MasterLensAudioProcessorEditor::updateSnapshotChips()
{
    snapshotA.setState (snapshots[0].valid, viewingSnapshot == 0);
    snapshotB.setState (snapshots[1].valid, viewingSnapshot == 1);
}

void MasterLensAudioProcessorEditor::captureSnapshot (int slot)
{
    if (slot < 0 || slot > 1 || ! stats.hasSignal)
        return;

    snapshots[slot].valid = true;
    snapshots[slot].stats = stats;
    snapshots[slot].capturedAt = stats.elapsed;
    viewingSnapshot = slot;

    updateSnapshotChips();
    repaint();
}

void MasterLensAudioProcessorEditor::recallSnapshot (int slot)
{
    if (slot < 0 || slot > 1)
        return;

    if (! snapshots[slot].valid)
    {
        captureSnapshot (slot);
        return;
    }

    viewingSnapshot = viewingSnapshot == slot ? -1 : slot;
    updateSnapshotChips();
    repaint();
}

void MasterLensAudioProcessorEditor::clearSnapshot (int slot)
{
    if (slot < 0 || slot > 1)
        return;

    snapshots[slot] = ml::Snapshot {};

    if (viewingSnapshot == slot)
        viewingSnapshot = -1;

    updateSnapshotChips();
    repaint();
}

void MasterLensAudioProcessorEditor::captureSnapshotForSnapshot (int slot)
{
    captureSnapshot (slot);
}
