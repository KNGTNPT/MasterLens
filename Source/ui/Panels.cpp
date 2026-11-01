#include "Widgets.h"
#include "../Delivery.h"

namespace ml
{
//==============================================================================
void paintEmptyState (juce::Graphics& g, juce::Rectangle<int> area)
{
    auto centre = area.withSizeKeepingCentre (juce::jmin (520, area.getWidth() - 32), 168);

    auto icon = juce::Rectangle<float> (0.0f, 0.0f, 56.0f, 56.0f)
                    .withCentre ({ centre.toFloat().getCentreX(), centre.toFloat().getY() + 18.0f });

    g.setColour (theme::cardAlt);
    g.fillRoundedRectangle (icon, 14.0f);
    g.setColour (theme::border);
    g.drawRoundedRectangle (icon.reduced (0.5f), 14.0f, 1.0f);

    const float heights[] { 0.32f, 0.66f, 0.92f, 0.52f };
    float x = icon.getX() + 12.0f;

    for (float h : heights)
    {
        const float bh = 26.0f * h + 4.0f;
        g.setColour (theme::accent.withAlpha (0.85f));
        g.fillRoundedRectangle (x, icon.getBottom() - 15.0f - bh, 5.0f, bh, 2.5f);
        x += 9.0f;
    }

    g.setColour (theme::text);
    g.setFont (uiFont (tokens::section, true));
    g.drawText (T ("播放音频后开始测量", "Play audio to start measuring"),
                centre.withTrimmedTop (82).withHeight (22), juce::Justification::centred);

    g.setColour (theme::textDim);
    g.setFont (uiFont (tokens::aux));
    g.drawText (T ("若长时间无响应，检查 Stereo Out 是否被旁路或静音",
                   "If nothing happens, check that Stereo Out is not bypassed or muted"),
                centre.withTrimmedTop (106).withHeight (20), juce::Justification::centred);

    g.setColour (theme::textFaint);
    g.setFont (uiFont (tokens::unit));
    g.drawText (T ("仅测量，不修改音频 · 零延迟", "Measurement only · zero latency"),
                centre.withTrimmedTop (128).withHeight (18), juce::Justification::centred);
}

//==============================================================================
void ReadoutBand::setValues (const StatsSnapshot& s, const BasisContext& b,
                             const PlatformSummary& p, float targetLufs,
                             float ceilingDb, bool signal)
{
    stats = s;
    basis = b;
    platform = p;
    target = targetLufs;
    ceiling = ceilingDb;
    hasSignal = signal;
    repaint();
}

void ReadoutBand::paintCell (juce::Graphics& g, juce::Rectangle<int> cell,
                             const juce::String& label, const juce::String& value,
                             const juce::String& unit, const juce::String& hint,
                             juce::Colour status)
{
    const bool compact = cell.getHeight() < 70;
    auto inner = cell.reduced (16, compact ? 4 : 8);

    g.setColour (theme::textFaint);
    g.setFont (uiFont (tokens::unit));
    g.drawText (label, inner.removeFromTop (compact ? 11 : 13), juce::Justification::centredLeft, false);

    auto valueRow = inner.removeFromTop (compact ? 26 : 34);
    auto unitArea = valueRow.removeFromRight (40);

    g.setColour (theme::text);
    g.setFont (monoFont (compact ? 26.0f : tokens::display, true));
    g.drawText (value, valueRow, juce::Justification::centredRight, false);

    g.setColour (theme::textFaint);
    g.setFont (uiFont (tokens::unit));
    g.drawText (unit, unitArea.withTrimmedBottom (compact ? 2 : 7),
                juce::Justification::bottomRight, false);

    auto rule = inner.removeFromTop (4).removeFromLeft (24);
    g.setColour (status);
    g.fillRect (rule);

    if (! compact)
    {
        g.setColour (theme::textFaint);
        g.setFont (uiFont (tokens::unit));
        g.drawText (hint, inner.removeFromTop (13), juce::Justification::centredLeft, false);
    }
}

void ReadoutBand::paint (juce::Graphics& g)
{
    auto b = getLocalBounds();
    g.fillAll (theme::background);

    // Hairline top and bottom, no card.
    surface::hairlineH (g, (float) b.getX(), (float) b.getRight(), (float) b.getY());
    surface::hairlineH (g, (float) b.getX(), (float) b.getRight(), (float) b.getBottom() - 1.0f);

    const bool live = hasSignal && stats.integrated > -70.0f;
    const bool tpValid = hasSignal && stats.truePeakDb > -70.0f;
    const bool lraValid = hasSignal && stats.lra > 0.05f;
    const bool plrValid = live && tpValid;

    const auto noReading = juce::String::charToString (0x2014);
    auto basisValue = noReading;
    auto basisHint = basis.label();
    auto basisStatus = theme::textFaint;

    if (basis.hasComparison())
    {
        const float diff = stats.integrated - basis.comparisonLufs();
        basisValue = sgn (diff, 1);
        basisHint = basis.label();
        basisStatus = std::abs (diff) <= 1.0f ? theme::good
                        : (std::abs (diff) <= 2.0f ? theme::warn : theme::bad);
    }
    else if (basis.mode == BasisMode::manual && live)
    {
        const float diff = stats.integrated - basis.manualLufs;
        basisValue = sgn (diff, 1);
        basisHint = T ("手动", "manual");
        basisStatus = std::abs (diff) <= 1.5f ? theme::good : theme::warn;
    }

    const int gap = 0;
    const int widths[5] { 30, 22, 19, 18, 15 };
    int x = b.getX();

    auto cell = [&] (int i)
    {
        auto r = juce::Rectangle<int> (x, b.getY(), b.getWidth() * widths[i] / 100, b.getHeight());
        x = r.getRight();
        return r;
    };

    const auto c0 = cell (0);
    const auto c1 = cell (1);
    const auto c2 = cell (2);
    const auto c3 = cell (3);
    auto c4 = b.withTrimmedLeft (c3.getRight()).withTrimmedRight (gap);

    for (int i = 1; i < 5; ++i)
    {
        const auto& r = (i == 1 ? c1 : (i == 2 ? c2 : (i == 3 ? c3 : c4)));
        surface::hairlineV (g, (float) r.getX(), (float) b.getY() + 12.0f,
                            (float) b.getBottom() - 12.0f);
    }

    const auto tpStatus = ! tpValid ? theme::textFaint
                         : (stats.truePeakDb > ceiling ? theme::bad
                         : (stats.truePeakDb > ceiling - 1.0f ? theme::warn : theme::good));
    const auto lraStatus = ! lraValid ? theme::textFaint
                          : (stats.lra < 3.0f ? theme::warn
                          : (stats.lra > 16.0f ? theme::info : theme::good));
    const auto plrValue = plrValid ? stats.getPlr() : -100.0f;
    const auto plrStatus = ! plrValid ? theme::textFaint
                          : (plrValue < 8.0f ? theme::warn
                          : (plrValue < 10.0f ? theme::info : theme::good));

    paintCell (g, c0, T ("I", "I"), live ? num (stats.integrated) : noReading,
               "LUFS", T ("综合响度", "integrated"),
               live ? (std::abs (stats.integrated - target) <= 1.0f ? theme::good
                                                                    : (std::abs (stats.integrated - target) <= 2.0f
                                                                           ? theme::warn : theme::textFaint))
                    : theme::textFaint);

    paintCell (g, c1, T ("TP", "TP"), tpValid ? num (stats.truePeakDb) : noReading,
               "dBTP", T ("真峰值 · 4×", "true peak · 4x"), tpStatus);

    paintCell (g, c2, T ("LRA", "LRA"), lraValid ? num (stats.lra) : noReading,
               "LU", T ("响度范围", "range"), lraStatus);

    paintCell (g, c3, T ("PLR", "PLR"), plrValid ? num (plrValue) : noReading,
               "dB", T ("峰响比", "peak / loud"), plrStatus);

    paintCell (g, c4, T ("Δ", "Δ"), basisValue, "LU", basisHint, basisStatus);
}

//==============================================================================
void MetricGrid::setValues (const StatsSnapshot& s, const BasisContext& b,
                            const PlatformSummary& p, float targetLufs,
                            float ceilingDb, bool signal)
{
    stats = s;
    basis = b;
    platform = p;
    target = targetLufs;
    ceiling = ceilingDb;
    hasSignal = signal;
    repaint();
}

void MetricGrid::paintCard (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& title,
                            const juce::String& value, const juce::String& unit,
                            const juce::String& hint, const juce::String& pill, juce::Colour colour)
{
    const auto r = area.toFloat();

    g.setColour (theme::raised);
    g.fillRoundedRectangle (r, tokens::radiusCard);
    g.setColour (juce::Colours::white.withAlpha (0.04f));
    g.drawHorizontalLine (area.getY() + 1, r.getX() + tokens::radiusCard,
                          r.getRight() - tokens::radiusCard);

    const auto statusFont = uiFont (tokens::unit);
    const bool narrow = area.getWidth() < 165;
    const int statusWidth = pill.isNotEmpty() && ! narrow
                                ? juce::roundToInt (juce::GlyphArrangement::getStringWidth (statusFont, pill))
                                : 0;

    g.setColour (theme::textDim);
    g.setFont (uiFont (tokens::label));
    g.drawFittedText (title, area.getX() + 16, area.getY() + 11,
                      juce::jmax (20, area.getWidth() - 32 - (statusWidth > 0 ? statusWidth + 10 : 0)), 18,
                      juce::Justification::centredLeft, 1);

    if (pill.isNotEmpty() && ! narrow)
    {
        g.setColour (colour);
        g.setFont (statusFont);
        g.drawText (pill, area.getRight() - 16 - statusWidth, area.getY() + 12,
                    statusWidth, 16, juce::Justification::centredRight, false);
    }
    else
    {
        g.setColour (theme::textFaint);
        g.setFont (monoFont (tokens::unit));
        g.drawText (unit, area.getRight() - 48, area.getY() + 12, 32, 16,
                    juce::Justification::centredRight, false);
    }

    const auto valueFont = monoFont (narrow ? 28.0f : tokens::value, true);
    const int valueWidth = juce::roundToInt (
        juce::GlyphArrangement::getStringWidth (valueFont, value));

    auto valueRow = juce::Rectangle<int> (area.getX() + 16, area.getY() + 36,
                                          area.getWidth() - 32, 38);

    g.setColour (theme::text);
    g.setFont (valueFont);
    g.drawText (value, valueRow.withWidth (valueWidth + 6), juce::Justification::centredLeft);

    if (! narrow)
    {
        g.setColour (theme::textFaint);
        g.setFont (monoFont (tokens::unit));
        g.drawText (unit, valueRow.withTrimmedLeft (valueWidth + 10).withTrimmedTop (14),
                    juce::Justification::centredLeft);
    }

    // Status is a tiny underline, not a coloured number.
    g.setColour (colour.withAlpha (0.9f));
    g.fillRoundedRectangle ((float) area.getX() + 16.0f, (float) area.getY() + 76.0f,
                            28.0f, 3.0f, 1.5f);

    g.setFont (uiFont (area.getHeight() < 100 ? tokens::unit : tokens::aux));
    g.setColour (theme::textFaint);
    g.drawText (hint, area.getX() + 16, area.getY() + (area.getHeight() < 100 ? 82 : 86),
                area.getWidth() - 32, area.getHeight() < 100 ? 14 : 18,
                juce::Justification::centredLeft);
}

void MetricGrid::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds();
    const int gap = tokens::gap;
    const bool fourAcross = b.getWidth() >= 560;

    const int cols = fourAcross ? 4 : 2;
    const int rows = fourAcross ? 1 : 2;
    const int w = (b.getWidth() - gap * (cols - 1)) / cols;
    const int h = (b.getHeight() - gap * (rows - 1)) / rows;

    const auto cell = [&] (int c, int r)
    {
        return juce::Rectangle<int> (b.getX() + c * (w + gap), b.getY() + r * (h + gap), w, h);
    };

    const auto noReading = noValue();
    const bool live = hasSignal;

    // --- integrated loudness / basis consequence ------------------------------
    {
        const bool valid = live && stats.integrated > -70.0f;
        juce::String pill, hint;
        auto colour = theme::textFaint;

        if (! valid)
        {
            hint = T ("等待信号", "Waiting for signal");
        }
        else if (basis.hasComparison() && basis.mode != BasisMode::manual)
        {
            const float diff = stats.integrated - basis.comparisonLufs();
            pill = T ("Δ ", "Δ ") + sgn (diff, 1) + " LU";
            hint = basis.label() + ml::sep() + num (basis.comparisonLufs()) + " LUFS";
            colour = std::abs (diff) <= 1.5f ? theme::good : theme::info;
        }
        else if (basis.mode == BasisMode::manual)
        {
            const float diff = stats.integrated - basis.manualLufs;
            pill = T ("Δ ", "Δ ") + sgn (diff, 1) + " LU";
            hint = T ("手动参考 ", "Manual ") + num (basis.manualLufs) + " LUFS";
            colour = theme::info;
        }
        else
        {
            pill = (platform.maxCutDb > 0.05f ? T ("衰减 ", "cut ") : T ("提升 ", "boost "))
                 + num (platform.maxCutDb > 0.05f ? platform.maxCutDb : platform.maxBoostDb) + " dB";
            hint = T ("丢弃 ", "wasted ")
                 + num (platform.maxWastedDb) + " LU";
            colour = platform.maxLimitedDb > 0.05f ? theme::warn
                   : (platform.maxWastedDb > 3.0f ? theme::info : theme::good);
        }

        paintCard (g, cell (0, 0), T ("综合响度", "INTEGRATED"),
                   valid ? num (stats.integrated) : noReading, "LUFS", hint, pill, colour);
    }

    // --- true peak ------------------------------------------------------------
    {
        const bool valid = live && stats.truePeakDb > -70.0f;
        const float headroom = ceiling - stats.truePeakDb;
        const auto colour = ! valid ? theme::textFaint
                          : (headroom < 0.0f ? theme::bad
                          : (headroom < 1.0f ? theme::warn : theme::good));

        paintCard (g, cell (1, 0), T ("真峰值 · 4×", "TRUE PEAK · 4×"),
                   valid ? num (stats.truePeakDb) : noReading, "dBTP",
                   valid ? T ("上限 ", "Ceiling ") + num (ceiling) + ml::sep()
                               + "OVERS " + juce::String (stats.oversCount)
                         : T ("等待信号", "Waiting for signal"),
                   valid ? T ("余量 ", "Headroom ") + num (headroom) : juce::String(), colour);
    }

    // --- loudness range -------------------------------------------------------
    {
        const bool valid = live && stats.lra > 0.05f;
        const auto colour = ! valid ? theme::textFaint
                          : (stats.lra < 3.0f ? theme::warn
                          : (stats.lra > 16.0f ? theme::info : theme::good));

        const juce::String rangeHint = T ("参考 ", "Typical ") + fmtRange (5.0f, 12.0f) + " LU";

        paintCard (g, cell (2, 0), T ("响度范围", "LRA"),
                   valid ? num (stats.lra) : noReading, "LU",
                   valid ? rangeHint : T ("等待信号", "Waiting for signal"),
                   juce::String(), colour);
    }

    // --- peak to loudness ratio ----------------------------------------------
    {
        const bool valid = live && stats.integrated > -70.0f && stats.truePeakDb > -70.0f;
        const float plr = stats.getPlr();
        const auto colour = ! valid ? theme::textFaint
                          : (plr < 8.0f ? theme::warn
                          : (plr < 10.0f ? theme::info : theme::good));

        const juce::String rangeHint = T ("参考 ", "Typical ") + fmtRange (10.0f, 16.0f) + " dB";

        paintCard (g, cell (3, 0), T ("峰响比", "PLR"),
                   valid ? num (plr) : noReading, "dB",
                   valid ? rangeHint : T ("等待信号", "Waiting for signal"),
                   juce::String(), colour);
    }
}

} // namespace ml
