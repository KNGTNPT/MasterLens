#include "Modules.h"

namespace ml
{
void DetailArea::paintFiles (juce::Graphics& g)
{
    auto area = contentArea().withTrimmedTop (30);
    area.removeFromBottom (40);   // real buttons live here

    // ---- drop zone -----------------------------------------------------------
    auto drop = area.removeFromTop (juce::jmin (92, juce::jmax (72, area.getHeight() / 4)));
    area.removeFromTop (8);

    g.setColour (fileBusy ? theme::accent.withAlpha (0.08f) : theme::cardAlt.withAlpha (0.65f));
    g.fillRoundedRectangle (drop.toFloat(), tokens::radiusCard);
    g.setColour (fileBusy ? theme::accent : theme::border);
    g.drawRoundedRectangle (drop.toFloat().reduced (0.5f), tokens::radiusCard, 1.0f);

    auto text = drop.reduced (14, 9);

    g.setColour (theme::text);
    g.setFont (uiFont (tokens::section, true));
    g.drawText (T ("把 WAV / AIFF / FLAC 或整个专辑文件夹拖进来",
                   "Drop WAV / AIFF / FLAC files or an album folder here"),
                text.removeFromTop (22), juce::Justification::centredLeft, false);

    g.setColour (theme::textDim);
    g.setFont (uiFont (tokens::aux));
    g.drawFittedText (T ("后台线程按文件自己的采样率分析：LUFS / TP / LRA / PLR / 频段 / 风险；"
                         "完成后自动写入专辑门禁。12 首 × 4 分钟不再需要实时等 48 分钟。",
                         "A worker thread analyses each file at its native sample rate: LUFS / TP / "
                         "LRA / PLR / bands / risk, then adds it to the album gate. No real-time wait."),
                      text.removeFromTop (34), juce::Justification::topLeft, 2);

    auto progress = drop.removeFromBottom (14);
    g.setColour (theme::textFaint);
    g.setFont (monoFont (tokens::unit));
    g.drawText (fileStatus.isNotEmpty() ? fileStatus
                                        : T ("等待文件；也可以点下方按钮选择",
                                             "Waiting for files; use the buttons below"),
                progress, juce::Justification::centredLeft, false);

    if (fileBusy && fileTotal > 0)
    {
        auto bar = progress.removeFromRight (juce::jmin (180, progress.getWidth() / 2));
        const float p = (float) fileDone / (float) juce::jmax (1, fileTotal);

        g.setColour (theme::borderSoft);
        g.fillRoundedRectangle (bar.toFloat(), 2.0f);
        g.setColour (theme::accent);
        g.fillRoundedRectangle (bar.toFloat().withWidth (bar.toFloat().getWidth() * p), 2.0f);
    }


    // ---- result table --------------------------------------------------------
    if (fileResults.empty())
    {
        g.setColour (theme::textDim);
        g.setFont (uiFont (tokens::section));
        g.drawFittedText (T ("还没有文件结果。分析一个专辑文件夹后，这里会列出每首歌的读数。",
                             "No file results yet. Analyse an album folder and every track will appear here."),
                          area, juce::Justification::centred, 2);
        return;
    }

    auto header = area.removeFromTop (22);
    const int wName = juce::roundToInt ((float) area.getWidth() * 0.38f);
    const int wRate = juce::roundToInt ((float) area.getWidth() * 0.15f);
    const int wDur  = juce::roundToInt ((float) area.getWidth() * 0.11f);
    const int wLufs = juce::roundToInt ((float) area.getWidth() * 0.11f);
    const int wTp   = juce::roundToInt ((float) area.getWidth() * 0.11f);
    const int wLra  = juce::roundToInt ((float) area.getWidth() * 0.08f);

    g.setColour (theme::textFaint);
    g.setFont (uiFont (tokens::unit));

    auto h = header;
    g.drawText (T ("文件", "File"), h.removeFromLeft (wName), juce::Justification::centredLeft);
    g.drawText (T ("采样率 / 位深", "Rate / depth"), h.removeFromLeft (wRate), juce::Justification::centredLeft);
    g.drawText (T ("时长", "Length"), h.removeFromLeft (wDur), juce::Justification::centredRight);
    g.drawText ("LUFS", h.removeFromLeft (wLufs), juce::Justification::centredRight);
    g.drawText ("TP", h.removeFromLeft (wTp), juce::Justification::centredRight);
    g.drawText ("LRA", h.removeFromLeft (wLra), juce::Justification::centredRight);
    g.drawText (T ("结果", "Result"), h, juce::Justification::centredRight);

    int rowIndex = 0;

    for (const auto& r : fileResults)
    {
        if (area.getHeight() < 24 || rowIndex >= 16)
            break;

        auto row = area.removeFromTop (juce::jmin (27, area.getHeight()));
        g.setColour (rowIndex % 2 == 0 ? theme::cardAlt.withAlpha (0.45f) : theme::card.withAlpha (0.0f));
        g.fillRoundedRectangle (row.toFloat(), 4.0f);

        auto c = row.reduced (8, 0);
        g.setColour (r.ok ? theme::text : theme::bad);
        g.setFont (uiFont (tokens::aux));
        g.drawFittedText (r.fileName, c.removeFromLeft (wName).withTrimmedRight (8),
                          juce::Justification::centredLeft, 1);

        g.setColour (theme::textFaint);
        g.setFont (monoFont (tokens::unit));

        const auto rate = r.sampleRate > 0.0
                              ? juce::String (r.sampleRate / 1000.0, 1) + " kHz / "
                                    + juce::String (r.bitDepth) + "-bit"
                              : juce::String ("-");
        g.drawText (rate, c.removeFromLeft (wRate), juce::Justification::centredLeft, false);
        g.drawText (fmtTime (r.durationSeconds), c.removeFromLeft (wDur), juce::Justification::centredRight, false);

        if (! r.ok)
        {
            g.setColour (theme::bad);
            g.drawText (r.error, c, juce::Justification::centredRight, false);
            ++rowIndex;
            continue;
        }

        g.setColour (r.stats.truePeakDb > -1.0f ? theme::bad : theme::text);
        g.drawText (num (r.stats.integrated), c.removeFromLeft (wLufs), juce::Justification::centredRight, false);
        g.drawText (num (r.stats.truePeakDb), c.removeFromLeft (wTp), juce::Justification::centredRight, false);
        g.drawText (num (r.stats.lra), c.removeFromLeft (wLra), juce::Justification::centredRight, false);

        const int risk = juce::roundToInt (r.defects.worstRisk());
        g.setColour (risk >= 65 ? theme::bad : (risk >= 35 ? theme::warn : theme::good));
        g.setFont (uiFont (tokens::unit, true));
        g.drawText (risk >= 65 ? T ("需处理 ", "action ") + juce::String (risk)
                               : (risk >= 35 ? T ("注意 ", "review ") + juce::String (risk)
                                             : T ("通过 ", "pass ") + juce::String (risk)),
                    c, juce::Justification::centredRight, false);

        ++rowIndex;
    }
}
} // namespace ml
