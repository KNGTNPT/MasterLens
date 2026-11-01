#include "Widgets.h"

namespace ml
{
//==============================================================================
void StereoMeters::setValues (const StatsSnapshot& s)
{
    stats = s;
    repaint();
}

void StereoMeters::paint (juce::Graphics& g)
{
    auto b = getLocalBounds();

    if (b.getWidth() < 80 || b.getHeight() < 80)
        return;

    const bool compact = b.getWidth() < 720 || b.getHeight() < 390;
    const int sideW = compact ? juce::jlimit (190, 230, b.getWidth() / 3)
                              : juce::jlimit (230, 270, b.getWidth() / 3);
    auto right = b.removeFromRight (sideW);
    b.removeFromRight (20);

    const bool stereo = stats.stereo;
    const float corr = stereo ? stats.correlation : 1.0f;
    const float width = stereo ? stats.widthPct : 0.0f;
    const float lowSide = stereo ? stats.lowSidePct : 0.0f;

    const auto corrColour = ! stereo ? theme::textFaint
                           : (corr < 0.0f ? theme::bad
                           : (corr < 0.15f ? theme::warn : theme::good));
    const auto widthColour = ! stereo ? theme::textFaint
                            : (width < 15.0f ? theme::info
                            : (width > 150.0f ? theme::warn : theme::good));
    const auto lowColour = ! stereo ? theme::textFaint
                          : (lowSide > 35.0f ? theme::warn : theme::good);

    // ---- left: three tracks, no cards --------------------------------------
    {
        auto title = b.removeFromTop (22);
        g.setColour (theme::textFaint);
        g.setFont (uiFont (tokens::unit));
        g.drawText ("01", title.removeFromLeft (24), juce::Justification::centredLeft, false);
        g.drawText (T ("空间 / 相位", "SPACE / PHASE"), title, juce::Justification::centredLeft, false);
        surface::hairlineH (g, 0.0f, (float) b.getRight(), (float) b.getY() - 1.0f);

        const auto drawTrack = [&] (juce::Rectangle<int> row, const juce::String& label,
                                    const juce::String& value, float normalised,
                                    float markerNorm, juce::Colour status)
        {
            auto inner = row;
            auto header = inner.removeFromTop (18);
            g.setColour (theme::textDim);
            g.setFont (uiFont (tokens::label));
            g.drawText (label, header, juce::Justification::centredLeft, false);

            g.setColour (theme::text);
            g.setFont (monoFont (15.0f, true));
            g.drawText (value, header, juce::Justification::centredRight, false);

            auto track = inner.removeFromTop (4).withTrimmedTop (8).toFloat();
            g.setColour (theme::borderSoft);
            g.fillRect (track);
            g.setColour (juce::Colours::black.withAlpha (0.40f));
            g.fillRect (track.withHeight (1.0f));

            const float n = juce::jlimit (0.0f, 1.0f, normalised);
            g.setColour (status);
            g.fillRect (track.withWidth (juce::jmax (2.0f, track.getWidth() * n)));

            if (markerNorm >= 0.0f)
            {
                const float mx = track.getX() + track.getWidth() * juce::jlimit (0.0f, 1.0f, markerNorm);
                g.setColour (theme::accent.withAlpha (0.85f));
                g.fillRect (mx - 0.5f, track.getY() - 4.0f, 1.0f, track.getHeight() + 8.0f);
            }

            auto rule = inner.removeFromTop (4).removeFromLeft (24);
            g.setColour (status);
            g.fillRect (rule);
        };

        const int trackH = compact ? 62 : juce::jlimit (70, 92, (b.getHeight() - 150) / 3);
        drawTrack (b.removeFromTop (trackH), T ("声道相关性", "CORRELATION"),
                   stereo ? sgn (corr, 2) : noValue(), (corr + 1.0f) * 0.5f, 0.5f, corrColour);
        drawTrack (b.removeFromTop (trackH), T ("立体声宽度", "STEREO WIDTH"),
                   stereo ? num (width) + "%" : noValue(), width / 200.0f, 0.5f, widthColour);
        drawTrack (b.removeFromTop (trackH), T ("低频侧向", "LOW-END SIDE"),
                   stereo ? num (lowSide) + "%" : noValue(), lowSide / 100.0f, 0.35f, lowColour);

    // ---- right: per-channel values, no four equal cards ---------------------
    {
        auto rightArea = right;
        auto title = rightArea.removeFromTop (22);
        g.setColour (theme::textFaint);
        g.setFont (uiFont (tokens::unit));
        g.drawText ("02", title.removeFromLeft (24), juce::Justification::centredLeft, false);
        g.drawText (T ("通道 / 折叠", "CHANNEL / FOLD"), title, juce::Justification::centredLeft, false);
        surface::hairlineH (g, (float) rightArea.getX(), (float) rightArea.getRight(),
                            (float) rightArea.getY() - 1.0f);

        const auto drawValue = [&] (juce::Rectangle<int> row, const juce::String& label,
                                    const juce::String& value, const juce::String& detail,
                                    juce::Colour status)
        {
            g.setColour (theme::textFaint);
            g.setFont (uiFont (tokens::unit));
            g.drawText (label, row.removeFromTop (13), juce::Justification::centredLeft, false);

            g.setColour (theme::text);
            g.setFont (monoFont (15.0f, true));
            g.drawText (value, row.removeFromTop (18), juce::Justification::centredLeft, false);

            g.setColour (status);
            g.fillRect (juce::Rectangle<float> ((float) row.getX(), (float) row.getY() + 2.0f,
                                                18.0f, 3.0f));

            g.setColour (theme::textFaint);
            g.setFont (uiFont (tokens::unit));
            g.drawText (detail, row.withTrimmedLeft (24), juce::Justification::centredLeft, false);
        };

        const int rowH = compact ? 44 : 54;
        drawValue (rightArea.removeFromTop (rowH), T ("左声道 TP", "L TRUE PEAK"),
                   stereo ? fmtDb (stats.truePeakLeft) + " dBTP" : noValue(),
                   T ("独立", "separate"), stereo ? theme::textDim : theme::textFaint);
        drawValue (rightArea.removeFromTop (rowH), T ("右声道 TP", "R TRUE PEAK"),
                   stereo ? fmtDb (stats.truePeakRight) + " dBTP" : noValue(),
                   T ("独立", "separate"), stereo ? theme::textDim : theme::textFaint);
        drawValue (rightArea.removeFromTop (rowH), T ("Side TP", "SIDE TRUE PEAK"),
                   stereo && stats.sideTruePeak > -99.0f ? fmtDb (stats.sideTruePeak) + " dBTP" : noValue(),
                   T ("单声道隐患", "mono risk"),
                   stereo && stats.sideTruePeak > -20.0f ? theme::warn : theme::textDim);

        const auto foldStatus = stats.monoFoldLossDb < -0.5f ? theme::warn : theme::good;
        drawValue (rightArea.removeFromTop (rowH), T ("单声道折叠", "MONO FOLD"),
                   fmtDb (stats.monoFoldLossDb) + " dB",
                   T ("保留 ", "retained ") + num (stats.monoRetainPct) + "%", foldStatus);

        rightArea.removeFromTop (8);
        surface::hairlineH (g, (float) rightArea.getX(), (float) rightArea.getRight(),
                            (float) rightArea.getY());

        g.setColour (theme::textFaint);
        g.setFont (monoFont (tokens::unit));
        g.drawText ("M " + fmtDb (stats.midLufs) + " LUFS   ·   S " + fmtDb (stats.sideLufs) + " LUFS",
                    rightArea.removeFromTop (20), juce::Justification::centredLeft, false);
    }

    }

    // ---- bottom: three bands as a table, artefacts as text ------------------
    {
        auto bottom = b.removeFromBottom (compact ? 92 : 112);
        auto bands = bottom.removeFromLeft (juce::roundToInt ((float) bottom.getWidth() * 0.64f));
        bottom.removeFromLeft (20);

        surface::hairlineH (g, (float) bands.getX(), (float) bottom.getRight(),
                            (float) bands.getY());
        bands.removeFromTop (10);

        const auto bandCell = [&] (juce::Rectangle<int> cell, const juce::String& title,
                                   const juce::String& range, float c, float side)
        {
            g.setColour (theme::textDim);
            g.setFont (uiFont (tokens::label));
            g.drawText (title, cell.removeFromTop (15), juce::Justification::centredLeft, false);

            g.setColour (theme::textFaint);
            g.setFont (monoFont (tokens::unit));
            g.drawText (range, cell.removeFromTop (12), juce::Justification::centredLeft, false);

            g.setColour (theme::text);
            g.setFont (monoFont (13.0f, true));
            g.drawText (T ("相关 ", "corr ") + sgn (c, 2), cell.removeFromTop (17),
                        juce::Justification::centredLeft, false);

            g.setColour (theme::textFaint);
            g.setFont (uiFont (tokens::unit));
            g.drawText (T ("侧向 ", "side ") + num (side) + "%", cell, juce::Justification::centredLeft, false);
        };

        const int bandW = bands.getWidth() / 3;
        bandCell (bands.removeFromLeft (bandW), T ("低频", "LOW"), "< 150 Hz",
                  stats.bandCorrLow, stats.bandSideLow);
        surface::hairlineV (g, (float) bands.getX(), (float) bands.getY(),
                            (float) bands.getBottom());
        bandCell (bands.removeFromLeft (bandW), T ("中频", "MID"), "150 Hz - 2 kHz",
                  stats.bandCorrMid, stats.bandSideMid);
        surface::hairlineV (g, (float) bands.getX(), (float) bands.getY(),
                            (float) bands.getBottom());
        bandCell (bands, T ("高频", "HIGH"), "> 2 kHz",
                  stats.bandCorrHigh, stats.bandSideHigh);

        auto artefacts = bottom;
        g.setColour (theme::textFaint);
        g.setFont (uiFont (tokens::unit));
        g.drawText (T ("伪影", "ARTEFACTS"), artefacts.removeFromTop (14),
                    juce::Justification::centredLeft, false);

        const auto artefact = [&] (const juce::String& label, int count, juce::Colour status)
        {
            auto row = artefacts.removeFromTop (juce::jmax (13, artefacts.getHeight() / 4));
            g.setColour (status);
            g.fillRect (juce::Rectangle<float> ((float) row.getX(), (float) row.getCentreY() - 1.0f,
                                                3.0f, 3.0f));
            g.setColour (theme::textDim);
            g.setFont (uiFont (tokens::unit));
            g.drawText (label + " " + juce::String (count), row.withTrimmedLeft (9),
                        juce::Justification::centredLeft, false);
        };

        artefact (T ("DC 偏移", "DC"), juce::roundToInt (std::abs (stats.dcOffsetDb)),
                  stats.dcOffsetDb > -46.0f ? theme::warn : theme::good);
        artefact (T ("削波", "Clip"), stats.clipEvents,
                  stats.clipEvents > 0 ? theme::bad : theme::good);
        artefact (T ("瞬态", "Clicks"), stats.clickEvents,
                  stats.clickEvents > 0 ? theme::warn : theme::good);
        artefact (T ("跨采样", "Overs"), stats.oversCount,
                  stats.oversCount > 0 ? theme::warn : theme::good);
    }
}
} // namespace ml
