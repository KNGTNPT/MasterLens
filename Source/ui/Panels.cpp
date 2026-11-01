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
