#include "PluginEditor.h"

using ml::T;

void MasterLensAudioProcessorEditor::requestSelectionMatch()
{
    if (! hasSelection || lastSelectionLufs <= -70.0)
    {
        footerStatusText = T ("先在时间线拖动选择一段并完成测量",
                              "Drag a selection on the timeline and measure it first");
        footerStatusColour = ml::theme::warn;
        repaint();
        return;
    }

    if (referencePaths.isEmpty() || referenceSlot < 0 || referenceSlot >= referencePaths.size())
    {
        footerStatusText = T ("先导入参考轨，再对选区做等响",
                              "Import a reference before matching a selection");
        footerStatusColour = ml::theme::warn;
        repaint();
        return;
    }

    if (! processor.referencePlayer.isReady())
    {
        footerStatusText = T ("等参考轨载入完成再匹配", "Wait for the reference to finish loading");
        footerStatusColour = ml::theme::warn;
        repaint();
        return;
    }

    const int slot = referenceSlot;
    const double offset = referenceOffsets[(size_t) slot];
    double from = offset + lastSelectionStart;
    double to = offset + lastSelectionEnd;

    // A positive OFS can push the mapped range before the file start; slide the
    // window instead of silently shrinking it.
    if (from < 0.0)
    {
        to -= from;
        from = 0.0;
    }

    if (to - from < 0.4)
    {
        footerStatusText = T ("映射后的参考选段太短（< 0.4 s）",
                              "The mapped reference range is shorter than 0.4 s");
        footerStatusColour = ml::theme::warn;
        repaint();
        return;
    }

    pendingMatchSlot = slot;
    referenceRangeMeter.start (juce::File (referencePaths[slot]), from, to);

    footerStatusText = T ("正在测量参考选段 ", "Measuring reference range ")
                       + ml::fmtTime (from) + ml::enDash() + ml::fmtTime (to) + " …";
    footerStatusColour = ml::theme::cyan;
    repaint();
}

bool MasterLensAudioProcessorEditor::seedSelectionForSnapshot (double startSeconds,
                                                              double endSeconds)
{
    if (endSeconds < startSeconds)
        std::swap (startSeconds, endSeconds);

    rawSeries.clear();

    for (double t = startSeconds; t <= endSeconds + 1.0e-9; t += 0.1)
    {
        RawSeriesPoint p;
        p.time = t;
        p.momentary = -14.0f;
        p.shortTerm = -14.0f;
        p.integrated = -14.0f;
        p.lra = 5.0f;
        p.truePeak = -1.0f;
        p.correlation = 0.8f;
        p.width = 30.0f;
        p.midShortTerm = -17.0f;
        p.sideShortTerm = -20.0f;
        rawSeries.push_back (p);
    }

    measureRegion (startSeconds, endSeconds);

    return hasSelection && detail.isSelectionMatchReady();
}

void MasterLensAudioProcessorEditor::pollSelectionMatch()
{
    ml::RangeLoudness measured;

    if (! referenceRangeMeter.takeResult (measured))
        return;

    if (! measured.ok)
    {
        footerStatusText = T ("参考选段无法测量：", "Could not measure the reference range: ")
                           + measured.error;
        footerStatusColour = ml::theme::bad;
        repaint();
        return;
    }

    const int slot = pendingMatchSlot;

    if (slot < 0 || slot >= (int) referencePaths.size() || slot != referenceSlot)
    {
        footerStatusText = T ("参考槽位已改变，本次匹配已取消",
                              "The reference slot changed; matching was cancelled");
        footerStatusColour = ml::theme::warn;
        repaint();
        return;
    }

    const double deltaDb = juce::jlimit (-24.0, 12.0, lastSelectionLufs - measured.lufs);
    referenceGains[(size_t) slot] = deltaDb;
    processor.referencePlayer.setGainDb ((float) deltaDb);
    saveReferenceSettings();

    juce::StringArray names;

    for (const auto& path : referencePaths)
        names.add (juce::File (path).getFileName());

    detail.setReferenceSlots (names, slot, referenceOffsets[(size_t) slot], deltaDb);

    footerStatusText = T ("选区 ", "Region ") + ml::num (lastSelectionLufs)
                       + T (" LUFS · 参考段 ", " LUFS · reference ")
                       + ml::num (measured.lufs) + T (" LUFS · Δ ", " LUFS · Δ ")
                       + ml::sgn (deltaDb, 1) + T (" dB 已写入参考 GAIN",
                                                   " dB written to the reference GAIN");
    footerStatusColour = ml::theme::good;
    repaint();
}
