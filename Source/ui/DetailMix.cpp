#include "Modules.h"
#include "../Delivery.h"

namespace ml
{
void DetailArea::paintSpectrumExtras (juce::Graphics& g)
{
    if (spectrumSideArea.isEmpty())
        return;

    const bool compact = spectrumSideArea.getHeight() < 110;
    auto area = spectrumSideArea;

    const auto average = [this] (int from, int to)
    {
        float sum = 0.0f;
        int n = 0;

        for (int i = from; i <= to && i < (int) state.bandDevs.size(); ++i)
        {
            sum += state.bandDevs[(size_t) i];
            ++n;
        }

        return n > 0 ? sum / (float) n : 0.0f;
    };

    const auto absolute = [this] (int from, int to)
    {
        float sum = 0.0f;
        int n = 0;

        for (int i = from; i <= to && i < (int) state.bandDb.size(); ++i)
        {
            sum += state.bandDb[(size_t) i];
            ++n;
        }

        return n > 0 ? sum / (float) n : -100.0f;
    };

    bool compare = false;

    for (float d : state.bandDevs)
        compare = compare || std::abs (d) > 0.01f;

    const float low = compare ? average (0, 2) : absolute (0, 2);
    const float mid = compare ? average (3, 6) : absolute (3, 6);
    const float high = compare ? average (7, 9) : absolute (7, 9);

    auto title = area.removeFromTop (compact ? 0 : 24);

    if (! compact)
    {
        g.setColour (theme::textFaint);
        g.setFont (uiFont (tokens::unit));
        g.drawText ("03", title.removeFromLeft (24), juce::Justification::centredLeft, false);
        g.drawText (compare ? T ("频段偏差", "BAND BALANCE")
                            : T ("频段电平", "BAND LEVEL"),
                    title, juce::Justification::centredLeft, false);
        surface::hairlineH (g, (float) area.getX(), (float) area.getRight(),
                            (float) area.getY() - 6.0f);
    }

    const auto drawBand = [&] (juce::Rectangle<int> cell, const juce::String& label,
                               const juce::String& range, float dev)
    {
        auto inner = cell;

        g.setColour (theme::textDim);
        g.setFont (uiFont (tokens::label));
        g.drawText (label, inner.removeFromTop (15), juce::Justification::centredLeft, false);

        g.setColour (theme::textFaint);
        g.setFont (monoFont (tokens::unit));
        g.drawText (range, inner.removeFromTop (12), juce::Justification::centredLeft, false);

        g.setColour (theme::text);
        g.setFont (monoFont (13.0f, true));
        g.drawText (compare ? sgn (dev, 1) + " dB"
                            : (dev <= -99.0f ? noValue() : num (dev) + " dB"),
                    inner.removeFromTop (18), juce::Justification::centredLeft, false);

        auto track = inner.removeFromTop (4).toFloat();
        g.setColour (theme::borderSoft);
        g.fillRect (track);

        const float centre = track.getCentreX();
        g.setColour (theme::border);
        g.fillRect (centre - 0.5f, track.getY() - 2.0f, 1.0f, track.getHeight() + 4.0f);

        const auto colour = compare
            ? (std::abs (dev) < 3.5f ? theme::accent
               : (std::abs (dev) < 6.0f ? theme::warn : theme::bad))
            : theme::accent;
        const float norm = compare ? juce::jlimit (0.0f, 1.0f, std::abs (dev) / 12.0f)
                                   : juce::jlimit (0.0f, 1.0f, (dev + 60.0f) / 60.0f);
        const float w = juce::jmax (2.0f, track.getWidth() * (compare ? 0.5f : 1.0f) * norm);

        g.setColour (colour);
        if (compare)
        {
            if (dev >= 0.0f)
                g.fillRect (juce::Rectangle<float> (centre, track.getY(), w, track.getHeight()));
            else
                g.fillRect (juce::Rectangle<float> (centre - w, track.getY(), w, track.getHeight()));
        }
        else
        {
            g.fillRect (juce::Rectangle<float> (track.getX(), track.getY(), w, track.getHeight()));
        }
    };

    if (compact)
    {
        const int w = area.getWidth() / 3;
        drawBand (area.removeFromLeft (w), T ("低频", "LOW"), "< 150 Hz", low);
        drawBand (area.removeFromLeft (w), T ("中频", "MID"), "150 Hz - 2 kHz", mid);
        drawBand (area, T ("高频", "HIGH"), "> 2 kHz", high);
    }
    else
    {
        const int rowH = juce::jmax (54, area.getHeight() / 3);
        drawBand (area.removeFromTop (rowH), T ("低频", "LOW"), "< 150 Hz", low);
        surface::hairlineH (g, (float) area.getX(), (float) area.getRight(),
                            (float) area.getY() - 4.0f);
        drawBand (area.removeFromTop (rowH), T ("中频", "MID"), "150 Hz - 2 kHz", mid);
        surface::hairlineH (g, (float) area.getX(), (float) area.getRight(),
                            (float) area.getY() - 4.0f);
        drawBand (area, T ("高频", "HIGH"), "> 2 kHz", high);
    }
}

void DetailArea::paintStereoExtras (juce::Graphics& g)
{
    juce::ignoreUnused (g);
}

//==============================================================================
void DetailArea::paintDelivery (juce::Graphics& g)
{
    auto area = contentArea().withTrimmedTop (30);
    const auto& s = state.stats;

    int okCount = 0, warnCount = 0, overCount = 0, limitedCount = 0;
    float maxLimited = 0.0f, wasted = 0.0f;

    for (int i = 0; i < numPlatforms; ++i)
    {
        const auto r = evaluate (s, platform (i));

        if (r.state == DeliveryState::over)         ++overCount;
        else if (r.state == DeliveryState::warning) ++warnCount;
        else                                        ++okCount;

        if (r.limitedDb > 0.05f)
        {
            ++limitedCount;
            maxLimited = juce::jmax (maxLimited, r.limitedDb);
        }

        wasted = juce::jmax (wasted, r.wastedLoudnessDb);
    }

    juce::ignoreUnused (overCount);

    auto summary = area.removeFromTop (30);

    g.setColour (theme::textDim);
    g.setFont (uiFont (tokens::aux));
    g.drawText (T ("可行 ", "OK ") + juce::String (okCount) + ml::sep()
                + T ("临界 ", "Warning ") + juce::String (warnCount) + ml::sep()
                + T ("需限幅 ", "Limited ") + juce::String (limitedCount),
                summary, juce::Justification::centredLeft);

    g.setColour (theme::textFaint);
    g.setFont (monoFont (tokens::unit));
    g.drawText (T ("当前 ", "Current ") + num (s.integrated) + " LUFS" + ml::sep()
                + num (s.truePeakDb) + " dBTP",
                summary, juce::Justification::centredRight);

    area.removeFromTop (6);

    const int tableW = juce::roundToInt ((float) area.getWidth() * 0.60f);
    auto table = area.removeFromLeft (tableW);
    area.removeFromLeft (tokens::gap);
    auto ref = area;

    const int rowH = juce::jlimit (21, 30, (table.getHeight() - 24) / numPlatforms);
    auto header = table.removeFromTop (24);
    const int wPlatform = juce::roundToInt ((float) table.getWidth() * 0.26f);
    const int wTarget   = juce::roundToInt ((float) table.getWidth() * 0.11f);
    const int wGain     = juce::roundToInt ((float) table.getWidth() * 0.13f);
    const int wPredicted = juce::roundToInt ((float) table.getWidth() * 0.18f);
    const int wLimited  = juce::roundToInt ((float) table.getWidth() * 0.16f);

    g.setColour (theme::textFaint);
    g.setFont (uiFont (tokens::unit));

    auto h = header;
    g.drawText (T ("平台", "Platform"), h.removeFromLeft (wPlatform), juce::Justification::centredLeft);
    g.drawText (T ("目标", "Target"), h.removeFromLeft (wTarget), juce::Justification::centredRight);
    g.drawText (T ("增益", "Gain"), h.removeFromLeft (wGain), juce::Justification::centredRight);
    g.drawText (T ("预测 TP", "Pred. TP"), h.removeFromLeft (wPredicted), juce::Justification::centredRight);
    g.drawText (T ("服务限幅", "Limited"), h.removeFromLeft (wLimited), juce::Justification::centredRight);
    g.drawText (T ("状态", "Status"), h, juce::Justification::centredRight);

    for (int i = 0; i < numPlatforms; ++i)
    {
        const auto& p = platform (i);
        const auto r = evaluate (s, p);

        if (table.getHeight() < rowH)
            break;

        auto row = table.removeFromTop (rowH);
        const auto colour = r.state == DeliveryState::over ? theme::bad
                          : (r.state == DeliveryState::warning ? theme::warn : theme::good);

        if (r.state == DeliveryState::over)
        {
            g.setColour (theme::bad.withAlpha (0.06f));
            g.fillRect (row);
            g.setColour (theme::bad);
            g.fillRect (row.getX(), row.getY(), 2, row.getHeight());
        }
        else if (i % 2 == 0)
        {
            g.setColour (theme::borderSoft.withAlpha (0.35f));
            g.fillRect (row);
        }

        auto r0 = row;

        g.setColour (theme::text);
        g.setFont (uiFont (tokens::label));
        g.drawText (T (p.nameZh, p.nameEn), r0.removeFromLeft (wPlatform),
                    juce::Justification::centredLeft);

        g.setColour (theme::textDim);
        g.setFont (monoFont (12.0f));
        g.drawText (num (p.targetLufs), r0.removeFromLeft (wTarget), juce::Justification::centredRight);

        g.setColour (theme::text);
        g.setFont (monoFont (12.0f, true));
        g.drawText (sgn (r.gainDb) + " dB", r0.removeFromLeft (wGain),
                    juce::Justification::centredRight);

        g.setColour (colour);
        g.drawText (num (r.predictedTp), r0.removeFromLeft (wPredicted),
                    juce::Justification::centredRight);

        g.setColour (r.limitedDb > 0.05f ? theme::bad : theme::textFaint);
        g.drawText (r.limitedDb > 0.05f ? minusSign() + num (r.limitedDb) + " dB" : noValue(),
                    r0.removeFromLeft (wLimited), juce::Justification::centredRight);

        g.setFont (uiFont (tokens::unit));
        g.drawText (r.state == DeliveryState::over ? T ("超限", "Over")
                  : r.state == DeliveryState::warning ? T ("临界", "Warning") : T ("可行", "OK"),
                    r0, juce::Justification::centredRight);
    }

    if (table.getHeight() >= 20 && limitedCount > 0)
    {
        g.setColour (theme::bad);
        g.setFont (uiFont (tokens::unit));
        g.drawText (TF (T ("归一化后仍需服务端限幅 {n} 个平台，最高 {l} dB；最多丢弃 {w} LU 的响度",
                           "Still limited by the service after gain: {n} platforms, up to {l} dB; "
                           "wasted loudness up to {w} LU"),
                        { { "n", juce::String (limitedCount) },
                          { "l", num (maxLimited) },
                          { "w", num (wasted) } }),
                    table.removeFromTop (20), juce::Justification::centredLeft, false);
    }

    // ---- reference comparison ----------------------------------------------
    {
        g.setColour (theme::cardAlt);
        g.fillRoundedRectangle (ref.toFloat(), tokens::radiusCard);
        g.setColour (theme::border);
        g.drawRoundedRectangle (ref.toFloat().reduced (0.5f), tokens::radiusCard, 1.0f);

        auto inner = ref.reduced (14, 11);

        g.setColour (theme::text);
        g.setFont (uiFont (tokens::label, true));
        g.drawText (T ("参考作品对比", "VS REFERENCE"), inner.removeFromTop (18),
                    juce::Justification::centredLeft, false);

        if (! state.hasReferenceStats || state.referenceStats.integrated <= -70.0f)
        {
            g.setColour (theme::textFaint);
            g.setFont (uiFont (tokens::aux));
            g.drawFittedText (T ("在「频谱平衡」页导入 1–4 条参考轨，这里会显示响度、动态、"
                                 "峰响比与低频侧向的实际差值。",
                                 "Import 1-4 reference tracks on the Spectrum page to compare "
                                 "loudness, dynamics, PLR and low-end side energy here."),
                              inner.removeFromTop (juce::jmin (84, inner.getHeight())),
                              juce::Justification::topLeft, 3);
            return;
        }

        const auto& r = state.referenceStats;

        const auto row = [&] (const juce::String& label, float mine, float theirs,
                              const juce::String& unit, int decimals = 1)
        {
            if (inner.getHeight() < 22)
                return;

            auto line = inner.removeFromTop (22);

            g.setColour (theme::textFaint);
            g.setFont (uiFont (tokens::unit));
            g.drawText (label, line.removeFromLeft (74), juce::Justification::centredLeft, false);

            g.setColour (theme::text);
            g.setFont (monoFont (11.5f, true));
            g.drawText (num (mine, decimals), line.removeFromLeft (52), juce::Justification::centredRight);

            g.setColour (theme::textDim);
            g.setFont (monoFont (11.5f));
            g.drawText (num (theirs, decimals), line.removeFromLeft (48), juce::Justification::centredRight);
            g.drawText (sgn (mine - theirs, decimals) + " " + unit, line,
                        juce::Justification::centredRight);
        };

        auto head = inner.removeFromTop (16);
        g.setColour (theme::textFaint);
        g.setFont (uiFont (tokens::unit));
        g.drawText (T ("你的", "Yours"), head.withTrimmedLeft (74).withWidth (52),
                    juce::Justification::centredRight);
        g.drawText (T ("参考", "Ref"), head.withTrimmedLeft (126).withWidth (48),
                    juce::Justification::centredRight);
        g.drawText (T ("差值", "Delta"), head.withTrimmedLeft (174),
                    juce::Justification::centredRight);

        row (T ("综合响度", "Integrated"), s.integrated, r.integrated, "LU");
        row (T ("LRA", "LRA"), s.lra, r.lra, "LU");
        row (T ("PLR", "PLR"), s.getPlr(), r.getPlr(), "dB");
        row (T ("真峰值", "True peak"), s.truePeakDb, r.truePeakDb, "dB");
        row (T ("低频侧向", "Low side"), s.lowSidePct, r.lowSidePct, "%");
        row (T ("超低占比", "Sub share"), s.subSharePct, r.subSharePct, "%");

        inner.removeFromTop (4);

        g.setColour (theme::textDim);
        g.setFont (uiFont (tokens::unit));

        const float dLu = s.integrated - r.integrated;
        const float dLra = s.lra - r.lra;

        juce::String verdict;

        if (dLu > 1.0f && dLra < -1.0f)
            verdict = TF (T ("你比参考响 {d} LU、动态少 {l} LU；归一化后两者同为平台目标，"
                             "多的响度不会到达听众，只剩更少的瞬态余量。",
                             "You sit {d} LU louder with {l} LU less range; after normalisation "
                             "both land on the platform target - the extra loudness never reaches "
                             "the listener, only less transient headroom remains."),
                          { { "d", num (dLu) }, { "l", num (-dLra) } });
        else if (dLu < -1.0f && dLra > 1.0f)
            verdict = TF (T ("你比参考轻 {d} LU、动态多 {l} LU；在归一化平台上这会保留更多瞬态，"
                             "属于健康差异。",
                             "You are {d} LU quieter with {l} LU more range; on a normalising "
                             "platform that keeps more transients - a healthy difference."),
                          { { "d", num (-dLu) }, { "l", num (dLra) } });
        else
            verdict = T ("响度与动态接近参考，差异主要来自音色与低频分配。",
                         "Loudness and dynamics are close to the reference; the remaining difference "
                         "is tonal and low-end balance.");

        g.drawFittedText (verdict, inner.removeFromTop (juce::jmin (72, inner.getHeight())),
                          juce::Justification::topLeft, 3);
    }
}
} // namespace ml
