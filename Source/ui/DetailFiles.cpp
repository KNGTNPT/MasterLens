#include "Modules.h"

#include <algorithm>

namespace ml
{
namespace
{
constexpr int kFileRowHeight = 32;
constexpr int kFileHeaderHeight = 26;
constexpr int kFileButtonRow = 40;
constexpr int kFileBandRow = 22;
constexpr int kFileDropGap = 8;

std::vector<int> sortedFileOrder (const std::vector<FileAnalysisResult>& results,
                                  int column, bool ascending)
{
    std::vector<int> order;
    order.reserve (results.size());

    for (int i = 0; i < (int) results.size(); ++i)
        order.push_back (i);

    const auto risk = [] (const FileAnalysisResult& r)
    {
        return r.ok ? r.defects.worstRisk() : 9999.0f;
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
    auto area = contentArea().withTrimmedTop (30);
    area.removeFromBottom (kFileButtonRow);
    area.removeFromTop (kFileBandRow + kFileDropGap);

    const int dropH = fileResults.empty() ? 88 : 34;
    area.removeFromTop (dropH + kFileDropGap);

    if (fileSelected >= 0 && area.getHeight() > 190)
        area.removeFromBottom (62);

    return area;
}

void DetailArea::paintFiles (juce::Graphics& g)
{
    auto area = contentArea().withTrimmedTop (30);
    area.removeFromBottom (kFileButtonRow);

    // ---- offline mode band (flat, no card) ----------------------------------
    {
        auto band = area.removeFromTop (kFileBandRow);
        g.setColour (theme::accent);
        g.fillEllipse ((float) band.getX(), (float) band.getCentreY() - 3.0f, 6.0f, 6.0f);
        g.setColour (theme::textDim);
        g.setFont (uiFont (tokens::aux));
        g.drawText (T ("文件 QC · 离线分析结果 · 不随播放更新",
                       "File QC · offline results · does not follow playback"),
                    band.withTrimmedLeft (14), juce::Justification::centredLeft);
        g.setColour (theme::textFaint);
        g.setFont (monoFont (tokens::unit));
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
        g.setFont (uiFont (tokens::section, true));
        g.drawText (T ("把 WAV / AIFF / FLAC 或整个专辑文件夹拖进来",
                       "Drop WAV / AIFF / FLAC files or an album folder here"),
                    text.removeFromTop (22), juce::Justification::centredLeft, false);

        g.setColour (theme::textDim);
        g.setFont (uiFont (tokens::aux));
        g.drawFittedText (T ("后台线程按文件自己的采样率分析：LUFS / TP / LRA / PLR / 频段 / 风险；"
                             "完成后自动写入专辑门禁。",
                             "A worker thread analyses each file at its native sample rate: LUFS / TP / "
                             "LRA / PLR / bands / risk, then adds it to the album gate."),
                          text.removeFromTop (34), juce::Justification::topLeft, 2);
    }
    else
    {
        g.setColour (theme::textDim);
        g.setFont (uiFont (tokens::aux));
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
        g.setFont (uiFont (tokens::section));
        g.drawFittedText (T ("还没有文件结果。分析一个专辑文件夹后，这里会列出每首歌的读数。",
                             "No file results yet. Analyse an album folder and every track will appear here."),
                          table.removeFromTop (22).reduced (2, 0),
                          juce::Justification::centredLeft, 1);
        return;
    }


    auto header = table.removeFromTop (kFileHeaderHeight);
    FileColumns cols (header);

    const char* labelsZh[] { "文件", "采样率 / 位深", "时长", "LUFS", "TP", "LRA", "风险" };
    const char* labelsEn[] { "FILE", "RATE / DEPTH", "LEN", "LUFS", "TP", "LRA", "RISK" };

    g.setFont (uiFont (tokens::unit));
    for (int i = 0; i < 7; ++i)
    {
        const bool numeric = i >= 2;
        g.setColour (i == fileSortColumn ? theme::textDim : theme::textFaint);
        auto cell = cols.cells[(size_t) i];
        auto arrow = cell.removeFromRight (i == fileSortColumn ? 12 : 0);

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
        g.setFont (uiFont (tokens::aux));
        g.drawFittedText (r.fileName, rowCols.cells[0].withTrimmedRight (8),
                          juce::Justification::centredLeft, 1);

        g.setColour (theme::textFaint);
        g.setFont (monoFont (tokens::unit));

        const auto rate = r.sampleRate > 0.0
                              ? juce::String (r.sampleRate / 1000.0, 1) + " kHz / "
                                    + juce::String (r.bitDepth) + "-bit"
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

        const int risk = juce::roundToInt (r.defects.worstRisk());
        g.setColour (risk >= 65 ? theme::bad : (risk >= 35 ? theme::warn : theme::good));
        g.setFont (uiFont (tokens::unit, true));
        g.drawText (T ("风险 ", "risk ") + juce::String (risk) + "/100",
                    rowCols.cells[6], juce::Justification::centredRight, false);
    }


    // ---- visible scrollbar ---------------------------------------------------
    if (maxScroll > 0)
    {
        auto rail = table.withTrimmedTop (kFileHeaderHeight).withTrimmedRight (2).withWidth (4);
        const float trackH = (float) rail.getHeight();
        const float thumbH = juce::jmax (20.0f, trackH * (float) visibleRows / (float) order.size());
        const float t = (float) fileScroll / (float) maxScroll;
        g.setColour (theme::borderSoft);
        g.fillRoundedRectangle (rail.toFloat(), 2.0f);
        g.setColour (theme::textFaint);
        g.fillRoundedRectangle (rail.toFloat().withY (rail.getY() + t * (trackH - thumbH))
                                    .withHeight (thumbH), 2.0f);
    }

    // ---- selected row detail ------------------------------------------------
    if (fileSelected >= 0 && fileSelected < (int) fileResults.size())
    {
        const auto& r = fileResults[(size_t) fileSelected];
        auto detail = table.withY (table.getBottom() + 8).withHeight (54);

        surface::hairlineH (g, (float) detail.getX(), (float) detail.getRight(),
                            (float) detail.getY());
        surface::hairlineH (g, (float) detail.getX(), (float) detail.getRight(),
                            (float) detail.getBottom() - 1.0f);
        auto inner = detail.reduced (10, 7);

        g.setColour (theme::text);
        g.setFont (uiFont (tokens::label, true));
        g.drawText (juce::File (r.fileName).getFileNameWithoutExtension(),
                    inner.removeFromTop (17), juce::Justification::centredLeft, false);

        g.setColour (theme::textFaint);
        g.setFont (monoFont (tokens::unit));
        g.drawText (r.ok ? T ("LUFS ", "LUFS ") + num (r.stats.integrated)
                               + "   TP " + num (r.stats.truePeakDb)
                               + "   LRA " + num (r.stats.lra)
                               + "   PLR " + num (r.stats.getPlr())
                               + "   " + T ("风险 ", "risk ")
                               + juce::String (juce::roundToInt (r.defects.worstRisk())) + "/100"
                         : r.error,
                    inner, juce::Justification::centredLeft, false);
    }
}

bool DetailArea::handleFileMouseDown (const juce::MouseEvent& e)
{
    if (page != PageId::files)
        return false;

    auto table = fileTableBounds();

    if (table.getWidth() <= 0 || table.getHeight() <= 0)
        return false;

    auto header = table.removeFromTop (kFileHeaderHeight);

    if (header.contains (e.getPosition()))
    {
        FileColumns cols (header);

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
        repaint();
        return true;
    }

    return false;
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
        handleFileWheel (wheel);
    else
        Component::mouseWheelMove (e, wheel);
}
} // namespace ml
