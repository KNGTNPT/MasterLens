#include "Modules.h"

#include <algorithm>

namespace ml
{
void DetailArea::setFilePlaybackAvailable (bool available)
{
    filePlaybackAvailable = available;
    filePlay.setVisible (available && page == PageId::files);
    fileProgress.setVisible (available && page == PageId::files);
    fileTime.setVisible (available && page == PageId::files);
    setCodecAvailable (available && selectedFile() != nullptr && selectedFile()->ok);

    if (! available)
        setFileTransportState (FileTransportState::unavailable);
    else if (const auto* selected = selectedFile())
    {
        setFileTransportState (FileTransportState::ready);
        setFileProgress (0.0, selected->durationSeconds);
    }
    else
        setFileTransportState (FileTransportState::unloaded);
}

void DetailArea::setFilePlayActive (bool active)
{
    filePlay.setButtonText (ml::grid::controlText (active ? T ("停止播放", "Stop file")
                                                          : T ("播放文件", "Play file")));
    filePlay.setToggleState (active, juce::dontSendNotification);
    setFileTransportState (active ? FileTransportState::playing
                                  : (selectedFile() != nullptr ? FileTransportState::ready
                                                               : FileTransportState::unloaded));
    repaint();
}

void DetailArea::setFileProgress (double positionSeconds, double durationSeconds)
{
    fileProgressDuration = juce::jmax (0.001, durationSeconds);
    fileProgressPosition = juce::jlimit (0.0, fileProgressDuration, positionSeconds);

    if (fileTransportState == FileTransportState::unloaded)
        setFileTransportState (FileTransportState::ready);

    if (std::abs (fileProgress.getMaximum() - fileProgressDuration) > 0.01)
        fileProgress.setRange (0.0, fileProgressDuration, 0.01);

    updatingFileProgress = true;
    fileProgress.setValue (fileProgressPosition, juce::dontSendNotification);
    updatingFileProgress = false;
    updateFileTransportText();
}

void DetailArea::setFileTransportState (FileTransportState state)
{
    fileTransportState = state;
    updateFileTransportText();
}

void DetailArea::updateFileTransportText()
{
    if (! filePlaybackAvailable || fileTransportState == FileTransportState::unavailable)
    {
        fileTime.setText ("— / —", juce::dontSendNotification);
        return;
    }

    if (fileTransportState == FileTransportState::unloaded || fileProgressDuration <= 0.0)
    {
        fileTime.setText (T ("未加载", "NOT LOADED"), juce::dontSendNotification);
        return;
    }

    const auto prefix = fileTransportState == FileTransportState::playing
                            ? (fileBuffering ? T ("缓冲中 ", "BUFFERING ")
                                             : T ("播放中 ", "PLAYING "))
                            : T ("可播放 ", "READY ");
    fileTime.setText (prefix + ml::fmtTime (fileProgressPosition)
                          + " / " + ml::fmtTime (fileProgressDuration),
                      juce::dontSendNotification);
}

void DetailArea::setFileInspectorCollapsed (bool collapsed)
{
    if (fileInspectorCollapsed == collapsed)
        return;

    fileInspectorCollapsed = collapsed;
    fileInspectorToggle.setButtonText (ml::grid::controlText (
        collapsed ? T ("展开证据", "Expand") : T ("收起证据", "Collapse")));
    resized();
    repaint();
}

namespace
{
constexpr int kFileRowHeight = 32;
constexpr int kFileHeaderHeight = 26;
constexpr int kFileButtonRow = 40;
constexpr int kFileTransportRow = 48;
constexpr int kFileBandRow = 22;
constexpr int kFileDropGap = 8;

juce::String fileReviewStatus (const FileAnalysisResult& r)
{
    if (! r.ok)
        return T ("分析失败", "ERROR");

    if (r.forensic.blockers() > 0)
        return T ("需修复", "FIX");

    if (r.forensic.findings.empty())
        return T ("通过", "PASS");

    const bool allReviewed = std::all_of (r.forensic.findings.begin(), r.forensic.findings.end(),
                                          [] (const FileFinding& f) { return f.reviewed || f.ignored; });
    if (allReviewed)
        return T ("已复核", "REVIEWED");

    return T ("建议回听", "REVIEW");
}

std::vector<int> sortedFileOrder (const std::vector<FileAnalysisResult>& results,
                                  int column, bool ascending)
{
    std::vector<int> order;
    order.reserve (results.size());

    for (int i = 0; i < (int) results.size(); ++i)
        order.push_back (i);

    const auto risk = [] (const FileAnalysisResult& r)
    {
        return r.ok ? r.defects.worstRisk() + 100.0f * r.forensic.blockers() : 9999.0f;
    };

    std::sort (order.begin(), order.end(), [&] (int a, int b)
    {
        const auto& x = results[(size_t) a];
        const auto& y = results[(size_t) b];
        int cmp = 0;

        switch (column)
        {
            case 0: cmp = x.fileName.compareIgnoreCase (y.fileName); break;
            case 1: cmp = x.sampleRate < y.sampleRate ? -1 : (x.sampleRate > y.sampleRate ? 1 : 0); break;
            case 2: cmp = x.durationSeconds < y.durationSeconds ? -1 : (x.durationSeconds > y.durationSeconds ? 1 : 0); break;
            case 3: cmp = x.stats.integrated < y.stats.integrated ? -1 : (x.stats.integrated > y.stats.integrated ? 1 : 0); break;
            case 4: cmp = x.stats.truePeakDb < y.stats.truePeakDb ? -1 : (x.stats.truePeakDb > y.stats.truePeakDb ? 1 : 0); break;
            case 5: cmp = x.stats.lra < y.stats.lra ? -1 : (x.stats.lra > y.stats.lra ? 1 : 0); break;
            default: cmp = risk (x) < risk (y) ? -1 : (risk (x) > risk (y) ? 1 : 0); break;
        }

        return ascending ? cmp < 0 : cmp > 0;
    });

    return order;
}

struct FileColumns
{
    std::array<juce::Rectangle<int>, 7> cells;

    explicit FileColumns (juce::Rectangle<int> area)
    {
        const int wName = juce::roundToInt ((float) area.getWidth() * 0.34f);
        const int wRate = juce::roundToInt ((float) area.getWidth() * 0.16f);
        const int wDur  = juce::roundToInt ((float) area.getWidth() * 0.10f);
        const int wLufs = juce::roundToInt ((float) area.getWidth() * 0.10f);
        const int wTp   = juce::roundToInt ((float) area.getWidth() * 0.10f);
        const int wLra  = juce::roundToInt ((float) area.getWidth() * 0.08f);

        cells[0] = area.removeFromLeft (wName);
        cells[1] = area.removeFromLeft (wRate);
        cells[2] = area.removeFromLeft (wDur);
        cells[3] = area.removeFromLeft (wLufs);
        cells[4] = area.removeFromLeft (wTp);
        cells[5] = area.removeFromLeft (wLra);
        cells[6] = area;
    }
};
} // namespace

juce::Rectangle<int> DetailArea::fileTableBounds() const
{
    auto area = contentArea().withTrimmedTop (32);
    area.removeFromBottom (kFileButtonRow);

    if (filePlaybackAvailable)
        area.removeFromBottom (kFileTransportRow);

    area.removeFromTop (kFileBandRow + kFileDropGap);

    const int dropH = fileResults.empty() ? 76 : 28;
    area.removeFromTop (dropH + kFileDropGap);

    if (fileSelected >= 0)
        area.setBottom (fileInspectorBounds().getY() - 8);

    return area;
}

juce::Rectangle<int> DetailArea::fileInspectorBounds() const
{
    auto area = contentArea().withTrimmedTop (30 + kFileBandRow + kFileDropGap + 28 + kFileDropGap);
    area.removeFromBottom (kFileButtonRow);

    if (filePlaybackAvailable)
        area.removeFromBottom (kFileTransportRow);

    if (fileInspectorCollapsed)
        return area.removeFromBottom (34);

    // Keep at least two complete file rows visible: the list is the task,
    // evidence is the explanation. The user can still expand or collapse it.
    constexpr int minTableHeight = kFileHeaderHeight + 3 * kFileRowHeight + 8;
    const int maxInspector = juce::jmax (88, area.getHeight() - minTableHeight);
    return area.removeFromBottom (juce::jlimit (88, maxInspector,
        juce::roundToInt (area.getHeight() * 0.58f)));
}

void DetailArea::paintFiles (juce::Graphics& g)
{
    auto area = contentArea().withTrimmedTop (32);
    area.removeFromBottom (kFileButtonRow);

    if (filePlaybackAvailable)
        area.removeFromBottom (kFileTransportRow);

    // ---- offline mode band (flat, no card) ----------------------------------
    {
        auto band = area.removeFromTop (kFileBandRow);
        g.setColour (theme::accent);
        g.fillEllipse ((float) band.getX(), (float) band.getCentreY() - 3.0f, 6.0f, 6.0f);
        g.setColour (theme::textDim);
        g.setFont (getLabelFont());
        g.drawText (T ("文件 QC · 离线结果 · 独立版实时播放",
                       "File QC · offline · standalone live play"),
                    band.withTrimmedLeft (16), juce::Justification::centredLeft);
        g.setColour (theme::textFaint);
        g.setFont (getValueFont());
        g.drawText (T ("原生采样率 · 后台线程", "native rate · worker thread"),
                    band, juce::Justification::centredRight);
        surface::hairlineH (g, (float) band.getX(), (float) band.getRight(),
                            (float) band.getBottom() - 1.0f);
        area.removeFromTop (kFileDropGap);
    }

    // ---- drop zone / compact status -----------------------------------------
    const int dropH = fileResults.empty() ? 76 : 28;
    auto drop = area.removeFromTop (dropH);
    area.removeFromTop (kFileDropGap);

    auto text = drop;

    if (fileResults.empty())
    {
        g.setColour (theme::text);
        g.setFont (getLabelFont());
        g.drawText (T ("把 WAV / AIFF / FLAC 或整个专辑文件夹拖进来",
                       "Drop WAV / AIFF / FLAC files or an album folder here"),
                    text.removeFromTop (24), juce::Justification::centredLeft, false);

        g.setColour (theme::textDim);
        g.setFont (getLabelFont());
        g.drawFittedText (T ("后台线程按文件自己的采样率分析：LUFS / TP / LRA / PLR / 频段 / 风险；"
                             "完成后自动写入专辑门禁。",
                             "A worker thread analyses each file at its native sample rate: LUFS / TP / "
                             "LRA / PLR / bands / risk, then adds it to the album gate."),
                          text.removeFromTop (36), juce::Justification::topLeft, 2);
    }
    else
    {
        g.setColour (theme::textDim);
        g.setFont (getLabelFont());
        g.drawText (fileStatus.isNotEmpty() ? fileStatus
                                            : T ("结果已就绪；点击表头排序，点击行查看单曲详情",
                                                 "Results ready; click a header to sort, a row for details"),
                    text, juce::Justification::centredLeft, false);
    }

    if (fileBusy && fileTotal > 0)
    {
        auto progress = drop.removeFromBottom (12);
        auto bar = progress.removeFromRight (juce::jmin (180, progress.getWidth() / 2));
        const float p = (float) fileDone / (float) juce::jmax (1, fileTotal);

        g.setColour (theme::borderSoft);
        g.fillRoundedRectangle (bar.toFloat(), 2.0f);
        g.setColour (theme::accent);
        g.fillRoundedRectangle (bar.toFloat().withWidth (bar.toFloat().getWidth() * p), 2.0f);
    }

    // ---- result table --------------------------------------------------------
    auto table = fileTableBounds();

    if (fileResults.empty())
    {
        g.setColour (theme::textFaint);
        g.setFont (getLabelFont());
        g.drawFittedText (T ("还没有文件结果。分析一个专辑文件夹后，这里会列出每首歌的读数。",
                             "No file results yet. Analyse an album folder and every track will appear here."),
                          table.removeFromTop (24).reduced (2, 0),
                          juce::Justification::centredLeft, 1);
        return;
    }


    auto header = table.removeFromTop (kFileHeaderHeight);
    FileColumns cols (header.withTrimmedRight (8).reduced (10, 0));

    const char* labelsZh[] { "文件", "采样率 / 位深", "时长", "LUFS", "TP", "LRA", "状态" };
    const char* labelsEn[] { "FILE", "RATE / DEPTH", "LEN", "LUFS", "TP", "LRA", "STATUS" };

    g.setFont (getLabelFont());
    for (int i = 0; i < 7; ++i)
    {
        const bool numeric = i >= 2;
        g.setColour (i == fileSortColumn ? theme::textDim : theme::textFaint);
        auto cell = cols.cells[(size_t) i];
        auto arrow = cell.removeFromRight (i == fileSortColumn ? 12 : 0);

        g.setFont (labelFontFor (T (labelsZh[i], labelsEn[i])));
        g.drawText (T (labelsZh[i], labelsEn[i]),
                    cell.withTrimmedRight (i == 6 ? 0 : 8),
                    numeric ? juce::Justification::centredRight : juce::Justification::centredLeft);

        if (i == fileSortColumn)
        {
            const float cx = (float) arrow.getCentreX();
            const float cy = (float) arrow.getCentreY();
            juce::Path tri;

            if (fileSortAscending)
                tri.addTriangle (cx - 4.0f, cy + 2.0f, cx + 4.0f, cy + 2.0f, cx, cy - 3.0f);
            else
                tri.addTriangle (cx - 4.0f, cy - 2.0f, cx + 4.0f, cy - 2.0f, cx, cy + 3.0f);

            g.fillPath (tri);
        }
    }

    g.setColour (theme::divider);
    g.drawHorizontalLine (table.getY(), (float) table.getX(), (float) table.getRight());

    const auto order = sortedFileOrder (fileResults, fileSortColumn, fileSortAscending);
    const int visibleRows = juce::jmax (1, table.getHeight() / kFileRowHeight);
    const int maxScroll = juce::jmax (0, (int) order.size() - visibleRows);
    fileScroll = juce::jlimit (0, maxScroll, fileScroll);

    auto body = table;

    for (int row = 0; row < visibleRows && fileScroll + row < (int) order.size(); ++row)
    {
        const int resultIndex = order[(size_t) (fileScroll + row)];
        const auto& r = fileResults[(size_t) resultIndex];
        auto rowArea = body.removeFromTop (juce::jmin (kFileRowHeight, body.getHeight()));

        if (rowArea.getHeight() < 18)
            break;

        if (resultIndex == fileSelected)
        {
            g.setColour (theme::accentSoft);
            g.fillRect (rowArea.withTrimmedRight (8));
        }

        surface::hairlineH (g, (float) rowArea.getX(), (float) rowArea.getRight() - 8.0f,
                            (float) rowArea.getBottom() - 1.0f);

        auto c = rowArea.withTrimmedRight (8).reduced (10, 0);
        FileColumns rowCols (c);

        g.setColour (r.ok ? theme::text : theme::bad);
        g.setFont (getLabelFont());
        g.drawText (r.fileName, rowCols.cells[0].withTrimmedRight (8),
                    juce::Justification::centredLeft, true);

        g.setColour (theme::textFaint);
        g.setFont (getValueFont());

        const auto rate = r.sampleRate > 0.0
                              ? juce::String (r.sampleRate / 1000.0, 1)
                                    + (rowCols.cells[1].getWidth() < 125 ? "k / " : " kHz / ")
                                    + juce::String (r.bitDepth)
                              : juce::String ("-");
        g.drawText (rate, rowCols.cells[1], juce::Justification::centredLeft, false);
        g.drawText (fmtTime (r.durationSeconds), rowCols.cells[2], juce::Justification::centredRight, false);

        if (! r.ok)
        {
            g.setColour (theme::bad);
            g.drawText (r.error, rowCols.cells[3].withWidth (rowCols.cells[6].getRight()
                                                             - rowCols.cells[3].getX()),
                        juce::Justification::centredRight, false);
            continue;
        }

        g.setColour (theme::text);
        g.drawText (num (r.stats.integrated), rowCols.cells[3], juce::Justification::centredRight, false);
        g.drawText (num (r.stats.truePeakDb), rowCols.cells[4], juce::Justification::centredRight, false);
        g.drawText (num (r.stats.lra), rowCols.cells[5], juce::Justification::centredRight, false);

        const bool allReviewed = ! r.forensic.findings.empty()
            && std::all_of (r.forensic.findings.begin(), r.forensic.findings.end(),
                            [] (const FileFinding& f) { return f.reviewed || f.ignored; });
        const bool needsReview = r.ok && ! r.forensic.findings.empty() && ! allReviewed
            && r.forensic.blockers() == 0;
        const auto reviewStatus = fileReviewStatus (r);
        g.setColour (! r.ok || r.forensic.blockers() > 0 ? theme::bad
                                                          : (needsReview ? theme::warn : theme::good));
        g.setFont (getLabelFont());
        g.drawText (reviewStatus, rowCols.cells[6], juce::Justification::centredRight, false);
    }


    // ---- visible scrollbar ---------------------------------------------------
    if (maxScroll > 0)
    {
        auto rail = table.removeFromRight (8);
        const float trackH = (float) rail.getHeight();
        const float thumbH = juce::jmax (20.0f, trackH * (float) visibleRows / (float) order.size());
        const float t = (float) fileScroll / (float) maxScroll;
        g.setColour (theme::borderSoft);
        g.fillRoundedRectangle (rail.toFloat(), 2.0f);
        g.setColour (theme::textFaint);
        g.fillRoundedRectangle (rail.toFloat().withY (rail.getY() + t * (trackH - thumbH))
                                    .withHeight (thumbH), 2.0f);
    }

    paintFileInspector (g);
}




namespace
{
/** In-plugin client delivery spec: edit the same fields the CLI consumes and
    see pass / block against the selected file before applying it. */
class SpecEditorPopup final : public juce::Component
{
public:
    SpecEditorPopup (const ml::DeliverySpec& initial, const ml::FileAnalysisResult* file,
                     std::function<void (ml::DeliverySpec)> onApply)
        : applyFn (std::move (onApply))
    {
        if (file != nullptr)
        {
            previewCopy = *file;
            hasPreview = true;
        }

        setup (target, juce::String (initial.targetLufs, 1));
        setup (ceiling, juce::String (initial.ceilingDbtp, 1));
        setup (tolerance, juce::String (initial.loudnessToleranceLu, 2));
        setup (rate, juce::String (initial.sampleRate));
        setup (depth, juce::String (initial.bitDepth));
        setup (channels, juce::String (initial.channels));
        setup (pattern, initial.namingPattern);

        addAndMakeVisible (enabled);
        enabled.setButtonText (T ("启用客户规格", "Enable client spec"));
        enabled.setToggleState (initial.enabled, juce::dontSendNotification);
        enabled.onClick = [this] { refreshPreview(); };

        addAndMakeVisible (ok);
        ok.setButtonText (T ("应用", "Apply"));
        ok.onClick = [this] { apply(); };

        addAndMakeVisible (cancel);
        cancel.setButtonText (T ("取消", "Cancel"));
        cancel.onClick = [this]
        {
            if (auto* box = findParentComponentOfClass<juce::CallOutBox>())
                box->dismiss();
        };

        refreshPreview();
        setSize (410, 360);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (theme::card);
        g.setColour (theme::text);
        g.setFont (getPageTitleFont());
        g.drawText (T ("客户交付规格", "CLIENT DELIVERY SPEC"), 14, 10, 260, 20,
                    juce::Justification::centredLeft, false);

        const char* zh[] { "目标 LUFS", "真峰值上限 dBTP", "响度容差 LU",
                           "采样率 Hz（0 = 不限）", "位深（0 = 不限）", "声道（0 = 不限）", "命名规则" };
        const char* en[] { "Target LUFS", "TP ceiling dBTP", "Tolerance LU",
                           "Sample rate Hz (0 = any)", "Bit depth (0 = any)", "Channels (0 = any)", "Naming pattern" };

        g.setFont (getLabelFont());
        g.setColour (theme::textFaint);
        for (int i = 0; i < 7; ++i)
            g.drawText (T (zh[i], en[i]), 14, rowY (i) + 4, 132, 20,
                        juce::Justification::centredLeft, true);

        auto preview = getLocalBounds().removeFromBottom (78).reduced (14, 0).removeFromTop (46);
        g.setColour (previewOk ? theme::good : theme::bad);
        g.setFont (getLabelFont());
        g.drawFittedText (previewText, preview, juce::Justification::topLeft, 2);
    }

    void resized() override
    {
        enabled.setBounds (14, 34, 240, 22);
        for (int i = 0; i < 7; ++i)
            field (i).setBounds (146, rowY (i), 248, 24);

        auto buttons = getLocalBounds().removeFromBottom (34).reduced (14, 2);
        cancel.setBounds (buttons.removeFromRight (76));
        buttons.removeFromRight (8);
        ok.setBounds (buttons.removeFromRight (76));
    }

private:
    static int rowY (int i) { return 64 + i * 30; }

    void setup (juce::TextEditor& e, const juce::String& value)
    {
        addAndMakeVisible (e);
        e.setText (value, false);
        e.setFont (getValueFont());
        e.setJustification (juce::Justification::centredLeft);
        e.setColour (juce::TextEditor::backgroundColourId, theme::cardAlt);
        e.setColour (juce::TextEditor::textColourId, theme::text);
        e.setColour (juce::TextEditor::outlineColourId, theme::border);
        e.setColour (juce::TextEditor::focusedOutlineColourId, theme::accent);
        e.onTextChange = [this] { refreshPreview(); };
    }

    juce::TextEditor& field (int i)
    {
        switch (i)
        {
            case 0: return target;
            case 1: return ceiling;
            case 2: return tolerance;
            case 3: return rate;
            case 4: return depth;
            case 5: return channels;
            default: return pattern;
        }
    }

    ml::DeliverySpec read() const
    {
        ml::DeliverySpec s;
        s.enabled = enabled.getToggleState();
        s.targetLufs = juce::jlimit (-40.0f, -5.0f, target.getText().getFloatValue());
        s.ceilingDbtp = juce::jlimit (-12.0f, 0.0f, ceiling.getText().getFloatValue());
        s.loudnessToleranceLu = juce::jlimit (0.0f, 5.0f, tolerance.getText().getFloatValue());
        s.sampleRate = juce::jlimit (0, 384000, rate.getText().getIntValue());
        s.bitDepth = juce::jlimit (0, 32, depth.getText().getIntValue());
        s.channels = juce::jlimit (0, 8, channels.getText().getIntValue());
        s.namingPattern = pattern.getText().trim();
        return s;
    }

    void refreshPreview()
    {
        const auto s = read();

        if (! hasPreview)
        {
            previewText = T ("没有可预览的文件。", "No file selected to preview.");
            previewOk = false;
        }
        else if (! s.enabled)
        {
            previewText = T ("规格未启用；应用后仍保持关闭。", "Spec is off; applying keeps it disabled.");
            previewOk = true;
        }
        else
        {
            const auto check = ml::evaluateSpec (previewCopy, s);
            previewOk = check.ok;
            previewText = check.ok ? T ("预览：通过", "Preview: PASS")
                                   : T ("预览：未通过 · ", "Preview: FAIL · ")
                                         + check.failures.joinIntoString ("; ");
        }

        repaint();
    }

    void apply()
    {
        auto s = read();
        juce::String error;

        if (! s.validate (error))
        {
            previewText = error;
            previewOk = false;
            repaint();
            return;
        }

        if (applyFn != nullptr)
            applyFn (s);

        if (auto* box = findParentComponentOfClass<juce::CallOutBox>())
            box->dismiss();
    }

    juce::TextEditor target, ceiling, tolerance, rate, depth, channels, pattern;
    juce::ToggleButton enabled;
    juce::TextButton ok, cancel;
    ml::FileAnalysisResult previewCopy;
    bool hasPreview = false;
    bool previewOk = false;
    juce::String previewText;
    std::function<void (ml::DeliverySpec)> applyFn;
};
} // namespace

namespace
{
/** Small popup for one evidence note; the inspector is too short to host an
    inline editor at 1120x700 without hiding the finding list. */
class FindingNotePopup final : public juce::Component
{
public:
    FindingNotePopup (const juce::String& initial, bool ignored,
                      std::function<void (juce::String, bool)> onDone)
        : done (std::move (onDone))
    {
        addAndMakeVisible (note);
        note.setMultiLine (true, true);
        note.setReturnKeyStartsNewLine (true);
        note.setText (initial, false);
        note.setFont (getLabelFont());
        note.setColour (juce::TextEditor::backgroundColourId, theme::cardAlt);
        note.setColour (juce::TextEditor::textColourId, theme::text);
        note.setColour (juce::TextEditor::outlineColourId, theme::border);
        note.setColour (juce::TextEditor::focusedOutlineColourId, theme::accent);

        addAndMakeVisible (ignore);
        ignore.setButtonText (T ("可忽略 / 已确认", "Ignore / accepted"));
        ignore.setToggleState (ignored, juce::dontSendNotification);

        addAndMakeVisible (ok);
        ok.setButtonText (T ("完成", "Done"));
        ok.onClick = [this]
        {
            if (done != nullptr)
                done (note.getText(), ignore.getToggleState());

            if (auto* box = findParentComponentOfClass<juce::CallOutBox>())
                box->dismiss();
        };

        setSize (320, 118);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (12);
        auto buttons = area.removeFromBottom (28);
        ok.setBounds (buttons.removeFromRight (76));
        ignore.setBounds (buttons);
        area.removeFromBottom (8);
        note.setBounds (area);
    }

private:
    juce::TextEditor note;
    juce::ToggleButton ignore;
    juce::TextButton ok;
    std::function<void (juce::String, bool)> done;
};
} // namespace

void DetailArea::syncFindingNote()
{
    if (fileSelected < 0 || fileSelected >= (int) fileResults.size())
    {
        findingNote.setText ({}, false);
        return;
    }

    auto& findings = fileResults[(size_t) fileSelected].forensic.findings;

    if (fileFindingSelected < 0 || fileFindingSelected >= (int) findings.size())
    {
        findingNote.setText ({}, false);
        return;
    }

    updatingFindingNote = true;
    findingNote.setText (findings[(size_t) fileFindingSelected].note, false);
    findingIgnore.setToggleState (findings[(size_t) fileFindingSelected].ignored,
                                  juce::dontSendNotification);
    updatingFindingNote = false;
}

void DetailArea::applyFindingNote()
{
    if (updatingFindingNote || fileSelected < 0 || fileSelected >= (int) fileResults.size())
        return;

    auto& findings = fileResults[(size_t) fileSelected].forensic.findings;

    if (fileFindingSelected < 0 || fileFindingSelected >= (int) findings.size())
        return;

    findings[(size_t) fileFindingSelected].note = findingNote.getText();
}

void DetailArea::auditionNextUnreviewed()
{
    if (fileResults.empty())
        return;

    const int n = (int) fileResults.size();
    const int start = juce::jlimit (0, n - 1, fileSelected < 0 ? 0 : fileSelected);

    for (int step = 0; step < n; ++step)
    {
        const int fi = (start + step) % n;
        auto& r = fileResults[(size_t) fi];

        if (! r.ok || r.sampleRate <= 0.0)
            continue;

        for (size_t k = 0; k < r.forensic.findings.size(); ++k)
        {
            auto& f = r.forensic.findings[(size_t) k];

            if (f.reviewed)
                continue;

            fileSelected = fi;
            fileFindingScroll = juce::jmax (0, (int) k - 2);

            if (onPreviewFile != nullptr)
                onPreviewFile (r.path, (double) f.startSample / r.sampleRate);

            resized();
            repaint();
            return;
        }
    }
}


namespace
{
/** Rename a reference cue (double-click A/B). */
class CueNamePopup final : public juce::Component
{
public:
    CueNamePopup (const juce::String& current, const juce::String& time,
                  std::function<void (juce::String)> onApply)
        : applyFn (std::move (onApply))
    {
        addAndMakeVisible (name);
        name.setText (current, false);
        name.setFont (getValueFont());
        name.setColour (juce::TextEditor::backgroundColourId, theme::cardAlt);
        name.setColour (juce::TextEditor::textColourId, theme::text);
        name.setColour (juce::TextEditor::outlineColourId, theme::border);
        name.setColour (juce::TextEditor::focusedOutlineColourId, theme::accent);
        name.onReturnKey = [this] { apply(); };

        addAndMakeVisible (ok);
        ok.setButtonText (T ("命名", "Name"));
        ok.onClick = [this] { apply(); };

        timeLabel = time;
        setSize (300, 108);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (theme::card);
        g.setColour (theme::textFaint);
        g.setFont (getMetaFont());
        g.drawText (timeLabel, 12, 8, 276, 16, juce::Justification::centredLeft, false);
    }

    void resized() override
    {
        name.setBounds (12, 30, 276, 26);
        ok.setBounds (212, 66, 76, 26);
    }

private:
    void apply()
    {
        if (applyFn != nullptr)
            applyFn (name.getText().trim());

        if (auto* box = findParentComponentOfClass<juce::CallOutBox>())
            box->dismiss();
    }

    juce::TextEditor name;
    juce::TextButton ok;
    juce::String timeLabel;
    std::function<void (juce::String)> applyFn;
};
} // namespace

void DetailArea::openCueNameEditor (int which)
{
    const bool isA = which == 0;
    const auto current = isA ? refCueAName : refCueBName;
    const auto time = isA ? refCueA.getTooltip() : refCueB.getTooltip();

    auto popup = std::make_unique<CueNamePopup> (current, time,
        [this, which] (juce::String text)
        {
            if (onReferenceCueName != nullptr)
                onReferenceCueName (which, text);
        });

    juce::CallOutBox::launchAsynchronously (std::move (popup),
                                            isA ? refCueA.getBounds() : refCueB.getBounds(), this);
}

void DetailArea::openSpecEditor()
{
    auto popup = std::make_unique<SpecEditorPopup> (
        spec, selectedFile(),
        [this] (const ml::DeliverySpec& s)
        {
            spec = s;
            if (onDeliverySpecChanged != nullptr)
                onDeliverySpecChanged (s);
            repaint();
        });

    juce::CallOutBox::launchAsynchronously (std::move (popup),
                                            specButton.getBounds(), this);
}

void DetailArea::paintFileInspector (juce::Graphics& g)
{
    fileFindingRows.clear();
    if (fileSelected < 0 || fileSelected >= (int) fileResults.size()) return;
    auto& r = fileResults[(size_t) fileSelected];
    auto& q = r.forensic;
    auto panel = fileInspectorBounds();
    surface::recessed (g, panel.toFloat());

    if (fileInspectorCollapsed)
    {
        auto line = panel.reduced (10, 0).removeFromTop (32);
        g.setColour (theme::textDim);
        g.setFont (getLabelFont());
        g.drawText (T ("证据详情", "EVIDENCE"), line.removeFromLeft (96),
                    juce::Justification::centredLeft, false);
        g.setColour (theme::textFaint);
        g.drawFittedText (r.fileName, line.withTrimmedRight (88), juce::Justification::centredLeft, 1);
        return;
    }

    auto area = panel.reduced (10, 0);
    auto title = area.removeFromTop (32).withTrimmedLeft (320).withTrimmedRight (88);
    g.setFont (getLabelFont());
    g.setColour (r.ok ? theme::textDim : theme::bad);
    g.drawText (r.fileName, title, juce::Justification::centredRight, true);
    area.reduce (2, 4);
    if (! r.ok)
    {
        g.setColour (theme::bad);
        g.drawText (r.error, area, juce::Justification::topLeft, true);
        return;
    }
    if (fileView == 0)
    {
        auto summary = area.removeFromTop (24);
        g.setFont (getValueFont());
        g.setColour (theme::textDim);
        const auto dcPct = [] (double v)
        {
            return num (std::abs (v) < 0.00005 ? 0.0f : (float) (100.0 * v), 2);
        };
        g.drawText (T ("头 / 尾 ", "HEAD / TAIL ") + num ((float) q.leadingSilenceSeconds, 3)
                    + " / " + num ((float) q.trailingSilenceSeconds, 3) + " s   DC "
                    + dcPct (q.dcOffset[0]) + " / " + dcPct (q.dcOffset[1])
                    + "%   " + T ("满刻度段 ", "FS RUNS ") + juce::String (q.clippedRunCount),
                    summary, juce::Justification::centredLeft, true);
        if (spec.enabled)
        {
            const auto sc = evaluateSpec (r, spec);
            auto specRow = area.removeFromTop (20);
            g.setFont (getValueFont());
            g.setColour (sc.ok ? theme::good : theme::bad);
            g.drawText (sc.ok ? T ("交付规格：通过", "DELIVERY SPEC: PASS")
                              : T ("交付规格：未通过 · ", "DELIVERY SPEC: FAIL · ")
                                    + sc.failures.joinIntoString ("; "),
                        specRow, juce::Justification::centredLeft, true);
        }

        auto hash = area.getHeight() >= 64 ? area.removeFromBottom (20) : juce::Rectangle<int>();
        g.setColour (theme::textFaint);
        g.setFont (getValueFont());
        g.drawText ("SHA256 " + q.sha256.substring (0, 24) + "...  "
                    + T ("完整证据见导出 QC", "full evidence: Export QC"), hash,
                    juce::Justification::centredLeft, true);
        const int rows = juce::jmax (1, area.getHeight() / 23);
        fileFindingScroll = juce::jlimit (0, juce::jmax (0, (int) q.findings.size() - rows), fileFindingScroll);
        if (q.findings.empty())
        {
            g.setColour (theme::good);
            g.setFont (getLabelFont());
            g.drawText (T ("未发现样本门禁问题；仍请完整试听。", "No sample-gate findings; listen through the master."),
                        area, juce::Justification::centredLeft, true);
        }
        for (int i = fileFindingScroll; i < (int) q.findings.size() && i < fileFindingScroll + rows; ++i)
        {
            auto& f = q.findings[(size_t) i];
            auto row = area.removeFromTop (24);
            fileFindingRows.push_back (row);
            auto check = row.removeFromRight (24);
            g.setColour (f.ignored ? theme::textFaint : (f.reviewed ? theme::good : theme::textFaint));
            g.setFont (getValueFont());
            g.drawText (f.ignored ? "[-]" : (f.reviewed ? "[x]" : "[ ]"), check,
                        juce::Justification::centred, false);
            auto time = row.removeFromLeft (92);
            g.setColour (f.severity == 2 ? theme::bad : theme::warn);
            g.setFont (getValueFont());
            g.drawText (juce::String (f.startSample / r.sampleRate, 3) + " s", time,
                        juce::Justification::centredLeft, true);
            auto address = row.removeFromRight (juce::jmin (190, row.getWidth() / 3));
            g.setColour (theme::textFaint);
            g.drawText ("#" + juce::String (f.startSample) + " +" + juce::String (f.lengthSamples)
                        + (f.channel >= 0 ? (f.channel == 0 ? " L" : " R") : " LR"),
                        address, juce::Justification::centredRight, true);
            g.setColour (f.reviewed ? theme::textDim : theme::text);
            g.setFont (getLabelFont());
            g.drawText (findingText (f, Localisation::isEnglish()), row, juce::Justification::centredLeft, true);
        }
    }
    else if (fileView == 1)
    {
        auto note = area.removeFromBottom (area.getHeight() < 80 ? 16 : 22);
        g.setFont (getLabelFont());
        g.setColour (theme::textFaint);
        g.drawText (T ("(L+R)/2 · 同相 0 dB，无关约 3 dB；灰色 = 能量不足 0.1%；6 dB 以上请试听",
                       "(L+R)/2 · mono 0 dB, uncorrelated ~3 dB · grey <0.1% energy · review above 6 dB"),
                    note, juce::Justification::centredLeft, true);
        const int w = area.getWidth() / 10;
        for (size_t b = 0; b < 10; ++b)
        {
            auto cell = area.removeFromLeft (w).reduced (4, 0);
            const auto& info = SpectrumAnalyzer::getBandInfo ((int) b);
            g.setFont (getValueFont());
            g.setColour (q.bandAvailable[b] ? theme::text : theme::textFaint);
            g.drawText (q.bandAvailable[b] ? num (q.monoLossDb[b]) : noValue(), cell.removeFromTop (20), juce::Justification::centred);
            auto label = cell.removeFromBottom (16);
            const auto hz = info.lo >= 1000 ? juce::String (info.lo / 1000.0f, 1) + "k" : juce::String ((int) info.lo);
            g.setColour (theme::textFaint);
            g.drawText (hz, label, juce::Justification::centred);
            if (cell.getHeight() < 8) continue;
            auto track = cell.toFloat().reduced ((float) cell.getWidth() * 0.30f, 3.0f);
            surface::recessed (g, track, 2.0f);
            const float h = track.getHeight() * juce::jlimit (0.0f, 1.0f, q.monoLossDb[b] / 12.0f);
            g.setColour (! q.bandAvailable[b] ? theme::border : q.monoLossDb[b] >= 6 ? theme::warn : theme::accent);
            g.fillRoundedRectangle (track.withTop (track.getBottom() - juce::jmax (2.0f, h)), 2.0f);
        }
    }
    else
    {
        const auto p = gainBudget (r.stats.integrated, r.stats.truePeakDb, state.target, state.ceiling);
        const bool compact = area.getHeight() < 80;
        auto note = area.removeFromBottom (compact ? 18 : (area.getHeight() > 100 ? 36 : 30));
        const char* zh[] { "目标所需", "峰值允许的增益", "预计可达 LUFS", "距目标还差 LU" };
        const char* en[] { "REQUESTED dB", "SAFE GAIN dB", "EST. LUFS", "SHORTFALL LU" };
        const float v[] { p.requestedGainDb, p.safeGainDb, p.achievedLufsEstimate, p.shortfallLu };
        const int w = area.getWidth() / 4;
        for (int i = 0; i < 4; ++i)
        {
            auto cell = area.removeFromLeft (w).reduced (8, 2);
            g.setFont (getLabelFont());
            g.setColour (theme::textFaint);
            g.drawText (T (zh[i], en[i]), cell.removeFromTop (compact ? 14 : 20), juce::Justification::centredLeft, true);
            g.setFont (getValueFont());
            g.setColour (i == 3 && p.shortfallLu > 0.1f ? theme::warn : theme::text);
            g.drawText (p.valid ? (i < 2 ? sgn (v[i]) : num (v[i])) : noValue(), cell, juce::Justification::centredLeft, true);
        }
        g.setFont (getLabelFont());
        g.setColour (theme::textDim);
        g.drawText ("REF " + num (state.target) + " LUFS / CEIL " + num (state.ceiling) + " dBTP  "
                    + T ("仅计算恒定增益；响度为估算，不写音频。", "Constant-gain estimate; no audio is written."),
                    note, juce::Justification::centredLeft, true);
    }
}

bool DetailArea::handleFileMouseDown (const juce::MouseEvent& e)
{
    if (page != PageId::files)
        return false;

    // The right-hand circle is the review checkbox. Clicking elsewhere on a
    // finding keeps selection/scroll behaviour intact; double-click previews it.
    if (fileView == 0 && fileInspectorBounds().contains (e.getPosition())
        && fileSelected >= 0 && fileSelected < (int) fileResults.size())
    {
        auto& findings = fileResults[(size_t) fileSelected].forensic.findings;

        for (int i = 0; i < (int) fileFindingRows.size() && i < (int) findings.size(); ++i)
            if (fileFindingRows[(size_t) i].contains (e.getPosition()))
            {
                if (e.x >= fileFindingRows[(size_t) i].getRight() - 28)
                {
                    findings[(size_t) i].reviewed = ! findings[(size_t) i].reviewed;
                    if (onReviewEvidence) onReviewEvidence (fileResults[(size_t) fileSelected].forensic.evidenceKey (findings[(size_t) i]), findings[(size_t) i].reviewed);
                    repaint();
                    return true;
                }

                // One click selects the finding (note editor appears) and
                // auditions the event; the right-hand checkbox toggles review.
                fileFindingSelected = i;
                syncFindingNote();
                resized();

                if (onPreviewFile != nullptr && fileResults[(size_t) fileSelected].sampleRate > 0.0)
                {
                    onPreviewFile (fileResults[(size_t) fileSelected].path,
                                   findings[(size_t) i].startSample
                                       / fileResults[(size_t) fileSelected].sampleRate);
                    return true;
                }
            }
    }

    auto table = fileTableBounds();

    if (table.getWidth() <= 0 || table.getHeight() <= 0)
        return false;

    auto header = table.removeFromTop (kFileHeaderHeight);

    if (header.contains (e.getPosition()))
    {
        FileColumns cols (header.withTrimmedRight (8).reduced (10, 0));

        for (int i = 0; i < 7; ++i)
            if (cols.cells[(size_t) i].contains (e.getPosition()))
            {
                if (fileSortColumn == i)
                    fileSortAscending = ! fileSortAscending;
                else
                {
                    fileSortColumn = i;
                    fileSortAscending = i == 0;
                }

                fileScroll = 0;
                repaint();
                break;
            }

        return true;
    }

    if (! table.contains (e.getPosition()) || fileResults.empty())
        return false;

    const int visibleRows = juce::jmax (1, table.getHeight() / kFileRowHeight);
    const int row = (e.y - table.getY()) / kFileRowHeight;
    const auto order = sortedFileOrder (fileResults, fileSortColumn, fileSortAscending);
    const int maxScroll = juce::jmax (0, (int) order.size() - visibleRows);
    fileScroll = juce::jlimit (0, maxScroll, fileScroll);

    const int index = fileScroll + row;

    if (row >= 0 && row < visibleRows && index < (int) order.size())
    {
        fileSelected = order[(size_t) index];
        fileFindingScroll = 0;
        fileFindingSelected = -1;
        if (selectedFile() != nullptr && ! selectedFile()->forensic.findings.empty())
            fileFindingSelected = 0;

        if (filePlaybackAvailable && selectedFile() != nullptr)
        {
            setFileTransportState (FileTransportState::ready);
            setFileProgress (0.0, selectedFile()->durationSeconds);
            setCodecAvailable (selectedFile()->ok);
        }

        resized();
        repaint();
        return true;
    }

    return false;
}

void DetailArea::handleFileDoubleClick (const juce::MouseEvent& e)
{
    if (page != PageId::files)
    {
        Component::mouseDoubleClick (e);
        return;
    }

    if (fileView == 0 && fileSelected >= 0 && fileSelected < (int) fileResults.size()
        && fileInspectorBounds().contains (e.getPosition()))
    {
        auto& r = fileResults[(size_t) fileSelected];
        auto& findings = r.forensic.findings;

        for (int i = 0; i < (int) fileFindingRows.size() && i < (int) findings.size(); ++i)
            if (fileFindingRows[(size_t) i].contains (e.getPosition()))
            {
                const int fi = fileSelected;
                const int fIndex = i;
                const auto row = fileFindingRows[(size_t) i];
                auto popup = std::make_unique<FindingNotePopup> (
                    findings[(size_t) i].note, findings[(size_t) i].ignored,
                    [this, fi, fIndex] (juce::String text, bool ignored)
                    {
                        if (fi < 0 || fi >= (int) fileResults.size())
                            return;

                        auto& list = fileResults[(size_t) fi].forensic.findings;

                        if (fIndex < 0 || fIndex >= (int) list.size())
                            return;

                        list[(size_t) fIndex].note = text;
                        list[(size_t) fIndex].ignored = ignored;
                        repaint();
                    });
                juce::CallOutBox::launchAsynchronously (std::move (popup), row, this);
                return;
            }
    }

    auto table = fileTableBounds();
    auto tableBody = table;
    tableBody.removeFromTop (kFileHeaderHeight);

    if (tableBody.contains (e.getPosition()) && fileSelected >= 0
        && fileSelected < (int) fileResults.size())
    {
        if (onRevealFile != nullptr)
            onRevealFile (fileResults[(size_t) fileSelected].path);
        return;
    }

    Component::mouseDoubleClick (e);
}

void DetailArea::handleFileWheel (const juce::MouseWheelDetails& wheel)
{
    if (page != PageId::files || fileResults.empty())
        return;

    const auto table = fileTableBounds();
    const int visibleRows = juce::jmax (1, table.getHeight() / kFileRowHeight);
    const int maxScroll = juce::jmax (0, (int) fileResults.size() - visibleRows);

    if (maxScroll <= 0)
        return;

    fileScroll = juce::jlimit (0, maxScroll,
                               fileScroll - juce::roundToInt (wheel.deltaY * 12.0f));
    repaint();
}

void DetailArea::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (page == PageId::files)
    {
        if (fileInspectorBounds().contains (e.getPosition()) && fileView == 0)
        {
            fileFindingScroll = juce::jmax (0, fileFindingScroll - juce::roundToInt (wheel.deltaY * 12.0f));
            repaint();
        }
        else handleFileWheel (wheel);
    }
    else
        Component::mouseWheelMove (e, wheel);
}
} // namespace ml
