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
    ++updateCount;
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
                             const juce::String& unit, const juce::String& hint, juce::Colour status)
{
    surface::hairlineH (g, (float) cell.getX(), (float) cell.getRight(), (float) cell.getY());
    auto inner = cell.reduced (0, 7);
    g.setFont (uiFont (tokens::unit));
    g.setColour (theme::textDim);
    g.drawText (label, inner.removeFromTop (14), juce::Justification::centredLeft, false);
    auto row = inner.removeFromTop (26);
    g.setFont (monoFont (22.0f));
    g.setColour (theme::text);
    g.drawText (value, row.withTrimmedRight (40), juce::Justification::centredLeft, false);
    g.setFont (monoFont (10.0f));
    g.setColour (theme::textFaint);
    g.drawText (unit, row.removeFromRight (40), juce::Justification::centredRight, false);
    g.setColour (status);
    g.fillEllipse ((float) cell.getRight()-4.0f, (float) cell.getY()+13.0f, 4.0f, 4.0f);
    juce::ignoreUnused (hint);
}

void ReadoutBand::paint (juce::Graphics& g)
{
    auto b = getLocalBounds();
    surface::raised (g, b.toFloat(), 5.0f);
    auto content = b.reduced (18, 12);
    const bool live = hasSignal && stats.integrated > -70.0f;
    const bool peak = hasSignal && stats.truePeakDb > -70.0f;
    auto hero = content.removeFromTop (juce::jlimit (142, 206, getHeight() * 43 / 100));
    g.setColour (theme::textDim);
    g.setFont (uiFont (11.0f, true).withExtraKerningFactor (0.05f));
    g.drawText (T ("综合响度", "INTEGRATED"), hero.removeFromTop (20),
                juce::Justification::centred, false);
    auto dial = hero.toFloat().reduced (4.0f, 2.0f);
    const float size = juce::jmin (dial.getWidth(), dial.getHeight() + 16.0f);
    dial = juce::Rectangle<float> (size, size).withCentre ({ dial.getCentreX(), dial.getY() + size * 0.5f });
    const float cx = dial.getCentreX(), cy = dial.getCentreY();
    const float radius = size * 0.43f;
    constexpr float start = -2.35f, end = 2.35f;
    for (int i = 0; i <= 32; ++i)
    {
        const float angle = start + (end-start) * (float) i / 32.0f;
        const float len = i%4==0 ? 7.0f : 3.0f;
        g.setColour (theme::textFaint.withAlpha (i%4==0 ? 0.7f : 0.35f));
        g.drawLine (cx+std::sin(angle)*(radius+6), cy-std::cos(angle)*(radius+6),
                    cx+std::sin(angle)*(radius+6+len), cy-std::cos(angle)*(radius+6+len), 1.0f);
    }
    juce::Path ring;
    ring.addCentredArc (cx, cy, radius, radius, 0.0f, start, end, true);
    g.setColour (theme::background);
    g.strokePath (ring, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    if (live)
    {
        juce::Path level;
        level.addCentredArc (cx, cy, radius, radius, 0.0f, start,
            start+(end-start)*juce::jlimit(0.0f,1.0f,(stats.integrated+36.0f)/36.0f),true);
        g.setColour (theme::accent);
        g.strokePath (level, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    const float ta = start+(end-start)*juce::jlimit(0.0f,1.0f,(target+36.0f)/36.0f);
    g.setColour (theme::text);
    if (basis.hasComparison() || basis.mode == BasisMode::manual)
        g.drawLine (cx+std::sin(ta)*(radius-4),cy-std::cos(ta)*(radius-4),
                    cx+std::sin(ta)*(radius+4),cy-std::cos(ta)*(radius+4),2.0f);
    g.setFont (monoFont (getWidth()<200 ? 31.0f : 36.0f));
    g.drawText (live ? num(stats.integrated) : noValue(),
        juce::Rectangle<float>(cx-size*0.5f,cy-22.0f,size,40.0f),juce::Justification::centred,false);
    g.setFont (monoFont (10.0f));
    g.setColour (theme::textFaint);
    g.drawText ("LUFS / EBU R128",juce::Rectangle<float>(cx-size*0.5f,cy+19.0f,size,18.0f),
                juce::Justification::centred,false);

    const int rowH = content.getHeight()/4;
    paintCell (g, content.removeFromTop(rowH), T("真峰值", "TRUE PEAK"), peak ? num(stats.truePeakDb) : noValue(),
        "dBTP", {}, !peak ? theme::textFaint : (stats.truePeakDb>ceiling ? theme::bad
            : (stats.truePeakDb>ceiling-1.0f ? theme::warn : theme::accent)));
    paintCell (g, content.removeFromTop(rowH), T("响度范围", "LOUDNESS RANGE"),live ? num(stats.lra) : noValue(),
        "LU", {}, live && stats.lra<3 ? theme::warn : theme::textFaint);
    paintCell (g, content.removeFromTop(rowH), T("峰响比", "PEAK / LOUDNESS"),live&&peak ? num(stats.getPlr()) : noValue(),
        "dB", {}, live && stats.getPlr()<8 ? theme::warn : theme::textFaint);
    const bool comparison = basis.hasComparison() || basis.mode == BasisMode::manual;
    const float delta = stats.integrated - (basis.hasComparison() ? basis.comparisonLufs() : target);
    paintCell (g, content, comparison ? T("参考差值", "REFERENCE DELTA") : T("平台最大衰减", "MAX PLATFORM CUT"),
        live ? (comparison ? sgn(delta) : num(platform.maxCutDb)) : noValue(), comparison ? "LU" : "dB", {},
        live && (comparison ? std::abs(delta)>2 : platform.maxCutDb>3) ? theme::warn : theme::accent);
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
