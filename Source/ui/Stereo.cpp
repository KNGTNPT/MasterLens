#include "Widgets.h"

namespace ml
{
//==============================================================================
void StereoMeters::setValues (const StatsSnapshot& s)
{
    stats = s;
    repaint();
}

void StereoMeters::paintMeter (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& label,
                               const juce::String& readout, float normalised, juce::Colour fill,
                               float marker, juce::Colour status)
{
    auto row = area;
    auto readoutArea = row.removeFromRight (86);

    g.setColour (theme::textDim);
    g.setFont (uiFont (tokens::label));
    g.drawText (label, row.removeFromLeft (112), juce::Justification::centredLeft);

    auto track = row.reduced (0, juce::jmax (0, row.getHeight() / 2 - 2)).toFloat();

    g.setColour (theme::borderSoft);
    g.fillRoundedRectangle (track, 2.0f);

    const float norm = juce::jlimit (0.0f, 1.0f, normalised);
    g.setColour (fill);
    g.fillRoundedRectangle (track.withWidth (juce::jmax (3.0f, track.getWidth() * norm)), 2.0f);

    // 2 px peak cap at the current value, plus a hairline target marker.
    const float px = track.getX() + track.getWidth() * norm;
    g.setColour (theme::text.withAlpha (0.8f));
    g.fillRect (px - 1.0f, track.getY() - 3.0f, 2.0f, track.getHeight() + 6.0f);

    if (marker >= 0.0f)
    {
        const float mx = track.getX() + track.getWidth() * juce::jlimit (0.0f, 1.0f, marker);
        g.setColour (theme::accent.withAlpha (0.85f));
        g.fillRect (mx - 0.5f, track.getY() - 4.0f, 1.0f, track.getHeight() + 8.0f);
    }

    g.setColour (theme::text);
    g.setFont (monoFont (15.0f, true));
    g.drawText (readout, readoutArea, juce::Justification::centredRight);

    g.setColour (status);
    g.fillRoundedRectangle ((float) readoutArea.getRight() - 24.0f,
                            (float) readoutArea.getBottom() - 2.0f, 24.0f, 2.0f, 1.0f);
}

void StereoMeters::paint (juce::Graphics& g)
{
    auto b = getLocalBounds();
    const bool compact = b.getHeight() < 370;
    auto chipRow = b.removeFromBottom (26);
    b.removeFromBottom (tokens::gap);
    auto bandRow = b.removeFromBottom (compact ? 62 : juce::jmin (68, juce::jmax (54, b.getHeight() / 4)));
    b.removeFromBottom (tokens::gap);

    const float corr = stats.stereo ? stats.correlation : 1.0f;
    const auto corrColour = ! stats.stereo ? theme::textFaint
                          : (corr < 0.0f ? theme::bad
                          : (corr < 0.15f ? theme::warn : theme::good));

    const int scaleH = 14;
    const int meterH = compact ? 32 : juce::jlimit (32, 48,
        (b.getHeight() - 3 * scaleH - 2 * 6 - 100) / 3);

    const auto paintScale = [&g] (juce::Rectangle<int> row,
                                  std::initializer_list<std::pair<juce::String, float>> ticks)
    {
        const auto track = row.withTrimmedLeft (112).withTrimmedRight (86);

        g.setColour (theme::textFaint);
        g.setFont (monoFont (tokens::unit));

        for (const auto& t : ticks)
        {
            const float x = (float) track.getX() + (float) track.getWidth() * t.second;
            auto r = juce::Rectangle<int> (juce::roundToInt (x) - 22, row.getY(), 44, 12);
            r.setX (juce::jlimit (track.getX(), track.getRight() - 44, r.getX()));
            g.drawText (t.first, r, juce::Justification::centred);
        }
    };

    // Use the full page height instead of centring a fixed block: on a large
    // window the old version left huge empty bands above and below the meters.
    auto cursor = b;

    paintMeter (g, cursor.removeFromTop (meterH), T ("声道相关性", "Correlation"),
                stats.stereo ? sgn (corr, 2) : noValue(),
                (corr + 1.0f) * 0.5f, theme::accent, 0.5f, corrColour);
    paintScale (cursor.removeFromTop (scaleH),
                { { minusSign() + "1", 0.0f }, { "0", 0.5f }, { "+1", 1.0f } });
    cursor.removeFromTop (6);

    const float width = stats.stereo ? stats.widthPct : 0.0f;
    const auto widthColour = ! stats.stereo ? theme::textFaint
                           : (width < 15.0f ? theme::info
                           : (width > 150.0f ? theme::warn : theme::good));

    paintMeter (g, cursor.removeFromTop (meterH), T ("立体声宽度", "Stereo width"),
                stats.stereo ? num (width) + "%" : noValue(),
                width / 200.0f, theme::accent, 0.075f, widthColour);
    paintScale (cursor.removeFromTop (scaleH),
                { { "0", 0.0f }, { "50", 0.25f }, { "100", 0.5f }, { "150", 0.75f }, { "200%", 1.0f } });
    cursor.removeFromTop (6);

    const auto lowColour = ! stats.stereo ? theme::textFaint
                         : (stats.lowSidePct > 35.0f ? theme::warn : theme::good);

    paintMeter (g, cursor.removeFromTop (meterH), T ("低频侧向能量", "Low-end side"),
                stats.stereo ? num (stats.lowSidePct) + "%" : noValue(),
                stats.lowSidePct / 100.0f, theme::accent, 0.35f, lowColour);
    paintScale (cursor.removeFromTop (scaleH),
                { { "0", 0.0f }, { "25", 0.25f }, { "50", 0.5f }, { "75", 0.75f }, { "100%", 1.0f } });

    // ---- explanation under the meters ---------------------------------------
    if (! compact && cursor.getHeight() >= 20)
    {
        g.setColour (theme::textFaint);
        g.setFont (uiFont (tokens::aux));
        g.drawFittedText (T ("相关性 +1 = 单声道，0 = 左右独立，负值 = 反相；低频侧向 < 35% 时单声道折叠安全",
                             "Correlation +1 = mono, 0 = independent, negative = out of phase; "
                             "keep low-end side below 35% for safe mono folding"),
                          cursor.removeFromTop (juce::jmin (36, cursor.getHeight())),
                          juce::Justification::centredLeft, 2);
    }

    // ---- per-channel / side true peak (v2.0) --------------------------------
    if (cursor.getHeight() >= 58)
    {
        auto info = cursor.removeFromTop (juce::jmin (compact ? 74 : 86, cursor.getHeight()));
        auto tileArea = info.removeFromTop (juce::jmin (compact ? 56 : 68, info.getHeight()));
        const int w = (tileArea.getWidth() - tokens::gap * 3) / 4;
        auto cells = tileArea;

        const auto tile = [&g, &cells, w] (const juce::String& label,
                                           const juce::String& value,
                                           const juce::String& detail)
        {
            auto cell = cells.removeFromLeft (w);
            cells.removeFromLeft (tokens::gap);

            g.setColour (theme::raised);
            g.fillRoundedRectangle (cell.toFloat(), tokens::radiusCard);
            g.setColour (juce::Colours::white.withAlpha (0.04f));
            g.drawHorizontalLine (cell.getY() + 1, (float) cell.getX() + 8.0f,
                                  (float) cell.getRight() - 8.0f);

            auto inner = cell.reduced (12, 9);
            g.setColour (theme::textFaint);
            g.setFont (uiFont (tokens::unit));
            g.drawText (label, inner.removeFromTop (14), juce::Justification::centredLeft, false);

            g.setColour (theme::text);
            g.setFont (monoFont (juce::jmin (22.0f, (float) inner.getHeight() * 0.64f), true));
            g.drawFittedText (value, inner.removeFromTop (24),
                              juce::Justification::centredLeft, 1, 0.62f);

            g.setColour (theme::textFaint);
            g.setFont (uiFont (tokens::unit));
            g.drawText (detail, inner, juce::Justification::centredLeft, false);
        };

        const bool stereo = stats.stereo;
        tile (T ("左声道真峰值", "L true peak"),
              stereo ? fmtDb (stats.truePeakLeft) + " dBTP" : noValue(),
              T ("独立于总 TP", "separate from total"));
        tile (T ("右声道真峰值", "R true peak"),
              stereo ? fmtDb (stats.truePeakRight) + " dBTP" : noValue(),
              T ("独立于总 TP", "separate from total"));

        const float side = stats.sideTruePeak;
        tile (T ("Side 真峰值", "Side true peak"),
              stereo && side > -99.0f ? fmtDb (side) + " dBTP" : noValue(),
              T ("单声道兼容隐患", "mono-compat risk"));

        tile (T ("单声道折叠", "Mono fold"),
              fmtDb (stats.monoFoldLossDb) + " dB",
              T ("保留 ", "retained ") + num (stats.monoRetainPct) + "%");

        // M/S integrated values stay as text, not as two more cards.
        g.setColour (theme::textFaint);
        g.setFont (monoFont (tokens::unit));
        g.drawText (T ("M ", "M ") + fmtDb (stats.midLufs) + " LUFS   ·   S "
                       + fmtDb (stats.sideLufs) + " LUFS",
                    info.getX(), info.getY() + 2, info.getWidth(), 14,
                    juce::Justification::centredLeft);
    }

    // ---- three band correlation table ---------------------------------------
    const auto band = [&] (juce::Rectangle<int> cell, const juce::String& title,
                           const juce::String& range, float c, float side)
    {
        auto inner = cell.reduced (0, 4);
        g.setColour (theme::divider);
        g.fillRect ((float) inner.getX(), (float) inner.getY(), 1.0f, (float) inner.getHeight());

        inner.removeFromLeft (12);

        g.setColour (theme::textDim);
        g.setFont (uiFont (tokens::label));
        g.drawText (title, inner.removeFromTop (14), juce::Justification::centredLeft);

        g.setColour (theme::textFaint);
        g.setFont (monoFont (tokens::unit));
        g.drawText (range, inner.removeFromTop (12), juce::Justification::centredLeft);

        g.setColour (theme::text);
        g.setFont (monoFont (14.0f, true));
        g.drawText (T ("相关 ", "corr ") + sgn (c, 2), inner.removeFromTop (15),
                    juce::Justification::centredLeft);

        g.setColour (theme::textFaint);
        g.setFont (monoFont (tokens::unit));
        g.drawText (T ("侧向 ", "side ") + num (side) + "%", inner.removeFromTop (12),
                    juce::Justification::centredLeft);
    };

    const int bandW = (bandRow.getWidth() - tokens::gap * 2) / 3;
    auto bandCells = bandRow;

    band (bandCells.removeFromLeft (bandW), T ("低频", "Low"), "< 150 Hz",
          stats.bandCorrLow, stats.bandSideLow);
    bandCells.removeFromLeft (tokens::gap);
    band (bandCells.removeFromLeft (bandW), T ("中频", "Mid"),
          juce::String ("150 Hz ") + enDash() + " 2 kHz", stats.bandCorrMid, stats.bandSideMid);
    bandCells.removeFromLeft (tokens::gap);
    band (bandCells, T ("高频", "High"), "> 2 kHz",
          stats.bandCorrHigh, stats.bandSideHigh);

    // ---- artefact chips ------------------------------------------------------
    const auto drawChip = [&g] (juce::Rectangle<int> r, const juce::String& text, juce::Colour colour)
    {
        g.setColour (theme::raised);
        g.fillRoundedRectangle (r.toFloat(), tokens::radiusControl);
        g.setColour (colour.withAlpha (0.9f));
        g.fillRoundedRectangle ((float) r.getX() + 8.0f, (float) r.getBottom() - 4.0f,
                                16.0f, 2.0f, 1.0f);
        g.setColour (theme::textDim);
        g.setFont (uiFont (tokens::unit));
        g.drawText (text, r, juce::Justification::centred);
    };

    const int chipW = (chipRow.getWidth() - tokens::gap * 3) / 4;

    drawChip (chipRow.removeFromLeft (chipW),
              T ("DC 偏移 ", "DC ") + fmtDb (stats.dcOffsetDb) + " dB",
              stats.dcOffsetDb > -46.0f ? theme::warn : theme::good);

    chipRow.removeFromLeft (tokens::gap);
    drawChip (chipRow.removeFromLeft (chipW),
              T ("削波 ", "Clip ") + juce::String (stats.clipEvents),
              stats.clipEvents > 0 ? theme::bad : theme::good);

    chipRow.removeFromLeft (tokens::gap);
    drawChip (chipRow.removeFromLeft (chipW),
              T ("瞬态 ", "Clicks ") + juce::String (stats.clickEvents),
              stats.clickEvents > 0 ? theme::warn : theme::good);

    chipRow.removeFromLeft (tokens::gap);
    drawChip (chipRow, T ("跨采样过载 ", "Overs ") + juce::String (stats.oversCount),
              stats.oversCount > 0 ? theme::warn : theme::good);
}

} // namespace ml
