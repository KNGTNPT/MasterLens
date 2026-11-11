#include "Widgets.h"
#include "../Version.h"

namespace ml
{
static juce::String formatHz (double hz)
{
    if (hz >= 1000.0)
        return juce::String (hz / 1000.0, hz >= 10000.0 ? 1 : 2) + " kHz";

    return juce::String (juce::roundToInt (hz)) + " Hz";
}

//==============================================================================
SpectrumDisplay::SpectrumDisplay()
{
    setOpaque (true);
    getProperties().set ("paddingLeft", 56);
    getProperties().set ("paddingTop", 32);
    getProperties().set ("paddingRight", 32);
    getProperties().set ("paddingBottom", 48);
    curvePath.preallocateSpace (8192);
    holdPath.preallocateSpace (8192);
    fillPath.preallocateSpace (8192);
}

void SpectrumDisplay::invalidateBackground()
{
    backgroundDirty = true;
    ensureBackground(); // lifecycle/state update, never lazy allocation in paint
    repaint();
}

void SpectrumDisplay::setAnalyzer (const SpectrumAnalyzer* a)
{
    analyzer = a;
    if (a == nullptr) { peakLabel.clear(); return; }
    int peakBin = 1;
    float peakDb = -100.0f;
    for (int i = 1; i < a->getNumBins(); ++i)
        if (a->getMagnitudes()[i] > peakDb) { peakDb = a->getMagnitudes()[i]; peakBin = i; }
    peakLabel = T ("峰值 ", "Peak ") + fmt1 (peakDb) + " dBFS  @ " + formatHz (peakBin * a->getBinHz());
}

namespace
{
// One mapping for grid and curve: -90 dB floor, 6 dB headroom, so the plotted
// energy fills the window instead of leaving the lower third empty.
constexpr float spectrumFloorDb = -90.0f;
constexpr float spectrumTopDb = 6.0f;

inline float spectrumDbToT (float db)
{
    return juce::jlimit (0.0f, 1.0f, (db - spectrumFloorDb) / (spectrumTopDb - spectrumFloorDb));
}
} // namespace

void SpectrumDisplay::drawGrid (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();

    const float leftPad = 56.0f, bottomPad = 48.0f, topPad = 32.0f, rightPad = 32.0f;
    auto plot = area.withTrimmedLeft (leftPad).withTrimmedRight (rightPad)
                    .withTrimmedTop (topPad).withTrimmedBottom (bottomPad);

    if (plot.getWidth() < 20.0f || plot.getHeight() < 20.0f)
        return;

    const auto yForDb = [plot] (float db)
    {
        const float t = spectrumDbToT (db);
        return plot.getBottom() - t * plot.getHeight();
    };

    g.setFont (getValueFont());

    for (float db = -80.0f; db <= 0.1f; db += 20.0f)
    {
        const float y = yForDb (db);

        g.setColour (theme::dark ? (db == 0.0f ? theme::border : theme::borderSoft) : juce::Colour (0xff455153));
        g.drawHorizontalLine (juce::roundToInt (y), plot.getX(), plot.getRight());

        g.setColour (theme::dark ? theme::textFaint : juce::Colour (0xffa7b3ae));
        g.drawText (num (db, 0),
                    juce::Rectangle<float> (0.0f, y - 6.0f, leftPad - 8.0f, 12.0f),
                    juce::Justification::centredRight);
    }

    const double freqTicks[] = { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };

    for (double f : freqTicks)
    {
        const float x = plot.getX() + plot.getWidth()
                        * (float) (std::log (f / 20.0) / std::log (20000.0 / 20.0));

        g.setColour (theme::dark ? theme::borderSoft : juce::Colour (0xff455153));
        g.drawVerticalLine (juce::roundToInt (x), plot.getY(), plot.getBottom());

        g.setColour (theme::dark ? theme::textFaint : juce::Colour (0xffa7b3ae));
        const auto label = f >= 1000.0 ? juce::String (f / 1000.0, 0) + "k" : juce::String (f, 0);
        auto labelRect = juce::Rectangle<float> (x - 18.0f, plot.getBottom() + 4.0f, 36.0f, 12.0f);
        labelRect.setX (juce::jlimit (plot.getX(), plot.getRight() - labelRect.getWidth(), labelRect.getX()));
        g.drawText (label, labelRect, juce::Justification::centred);
    }

    g.setFont (getLabelFont());
    g.setColour (theme::dark ? theme::textFaint : juce::Colour (0xffa7b3ae));
    g.drawText ("dBFS", juce::Rectangle<float> (0.0f, plot.getY() - 14.0f, leftPad, 12.0f),
                juce::Justification::centredRight);
}

void SpectrumDisplay::ensureBackground()
{
    const int w = getWidth();
    const int h = getHeight();

    if (w <= 0 || h <= 0)
        return;

    const float scale = juce::jmax (1.0f, getDesktopScaleFactor());

    background = juce::Image (juce::Image::ARGB,
                              juce::roundToInt ((float) w * scale),
                              juce::roundToInt ((float) h * scale),
                              true);

    juce::Graphics bg (background);
    bg.addTransform (juce::AffineTransform::scale (scale));
    bg.fillAll ((theme::dark ? theme::card : juce::Colour (0xff283133)));
    drawGrid (bg);
    fillGradient.point1 = { 56.0f, 32.0f };
    fillGradient.point2 = { 56.0f, (float) h - 48.0f };
    fillGradient.setColour (0, theme::accent.withAlpha (0.10f));
    fillGradient.setColour (1, theme::accent.withAlpha (0.0f));

    backgroundDirty = false;
    cachedThemeRevision = theme::revision();
}

void SpectrumDisplay::paint (juce::Graphics& g)
{
    if (backgroundDirty || background.isNull() || cachedThemeRevision != theme::revision())
    {
        g.fillAll ((theme::dark ? theme::card : juce::Colour (0xff283133)));
        return;
    }

    g.drawImage (background, getLocalBounds().toFloat());

    if (analyzer == nullptr)
        return;

    auto area = getLocalBounds().toFloat();

    const float leftPad = 56.0f, bottomPad = 48.0f, topPad = 32.0f, rightPad = 32.0f;
    auto plot = area.withTrimmedLeft (leftPad).withTrimmedRight (rightPad)
                    .withTrimmedTop (topPad).withTrimmedBottom (bottomPad);

    if (plot.getWidth() < 20.0f || plot.getHeight() < 20.0f)
        return;

    const auto xFor = [plot] (double t)
    {
        return plot.getX() + (float) juce::jlimit (0.0, 1.0, t) * plot.getWidth();
    };

    const auto yForDb = [plot] (float db)
    {
        const float t = spectrumDbToT (db);
        return plot.getBottom() - t * plot.getHeight();
    };

    const int numBins = analyzer->getNumBins();

    if (numBins < 4)
        return;

    // 420 points is visually indistinguishable at this plot width and keeps
    // the 60 fps spectrum comfortably inside the 8 ms paint budget.
    const int steps = juce::jlimit (140, 420, getWidth());

    const double logRange = std::log (20000.0 / 20.0);

    // Fixed buffers, reused every frame: no vector allocation in paint.
    values.fill (-100.0f);
    holds.fill (-100.0f);

    for (int px = 0; px <= steps; ++px)
    {
        const double f = 20.0 * std::exp (logRange * ((double) px + 0.5) / (double) steps);

        values[(size_t) px] = analyzer->getMagnitudeAt (f);
        holds[(size_t) px] = analyzer->getPeakAt (f);
    }

    const auto xAt = [&] (int px)
    {
        return xFor ((double) px / (double) steps);
    };

    const auto makeSmoothPath = [&] (juce::Path& p, const std::array<float, maxPoints>& data)
    {
        p.clear();

        if (steps < 3)
            return;

        const auto pointAt = [&] (int index)
        {
            const int clamped = juce::jlimit (0, steps, index);
            return juce::Point<float> (xAt (clamped), yForDb (data[(size_t) clamped]));
        };

        p.startNewSubPath (pointAt (0));

        for (int i = 0; i < steps; ++i)
        {
            const auto p0 = pointAt (i - 1);
            const auto p1 = pointAt (i);
            const auto p2 = pointAt (i + 1);
            const auto p3 = pointAt (i + 2);

            const auto c1 = p1 + (p2 - p0) / 6.0f;
            const auto c2 = p2 - (p3 - p1) / 6.0f;

            p.cubicTo (c1, c2, p2);
        }
    };

    makeSmoothPath (curvePath, values);
    makeSmoothPath (holdPath, holds);

    fillPath.clear();
    fillPath.addPath (curvePath);
    fillPath.lineTo (plot.getRight(), plot.getBottom());
    fillPath.lineTo (plot.getX(), plot.getBottom());
    fillPath.closeSubPath();

    g.setGradientFill (fillGradient);
    g.fillPath (fillPath);

    g.setColour (theme::textFaint.withAlpha (0.35f));
    g.strokePath (holdPath, juce::PathStrokeType (1.0f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));

    // The live measurement is drawn last, so the hold trace cannot dim it.
    g.setColour (juce::Colours::lightgrey.withAlpha (0.92f));
    g.strokePath (curvePath, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));

    g.setFont (getValueFont());
    g.setColour (theme::dark ? theme::textDim : juce::Colour (0xffb2bcb8));
    g.drawText (peakLabel,
                plot.withHeight (14.0f).translated (0.0f, -15.0f),
                juce::Justification::centredRight);
}


//==============================================================================
BandBalanceDisplay::BandBalanceDisplay()
{
    setOpaque (true);
}

void BandBalanceDisplay::ensureBackground()
{
    const int w = getWidth();
    const int h = getHeight();

    if (w <= 0 || h <= 0)
        return;

    const float scale = juce::jmax (1.0f, getDesktopScaleFactor());

    background = juce::Image (juce::Image::ARGB,
                              juce::roundToInt ((float) w * scale),
                              juce::roundToInt ((float) h * scale),
                              true);

    juce::Graphics g (background);
    g.addTransform (juce::AffineTransform::scale (scale));
    g.fillAll (theme::card);

    const int n = SpectrumAnalyzer::numBands;
    const int colW = w / n;
    const int labelH = 16;

    auto plot = getLocalBounds().withTrimmedTop (16).withTrimmedBottom (labelH);
    const float zeroY = (float) plot.getCentreY();

    g.setColour (theme::borderSoft);
    g.drawHorizontalLine (juce::roundToInt (zeroY), 0.0f, (float) w);

    for (int i = 0; i < n; ++i)
    {
        const auto& info = SpectrumAnalyzer::getBandInfo (i);

        g.setColour (theme::textFaint);
        g.setFont (getLabelFont());
        g.drawText (T (info.label, info.labelEn),
                    juce::Rectangle<int> (i * colW, h - labelH, colW, labelH),
                    juce::Justification::centred);
    }

    g.setColour (theme::textFaint);
    g.setFont (getValueFont());
    g.drawText ("dB", 4, 0, 24, 13, juce::Justification::centredLeft);

    backgroundDirty = false;
    cachedThemeRevision = theme::revision();
}

void BandBalanceDisplay::paint (juce::Graphics& g)
{
    if (backgroundDirty || background.isNull() || cachedThemeRevision != theme::revision())
        ensureBackground();

    g.drawImage (background, getLocalBounds().toFloat());

    const int n = SpectrumAnalyzer::numBands;
    devs.resize ((size_t) n, 0.0f);

    const int colW = getWidth() / n;
    const int labelH = 16;
    const int valueH = 14;

    auto plot = getLocalBounds().withTrimmedTop (valueH).withTrimmedBottom (labelH);
    const float zeroY = (float) plot.getCentreY();
    const float halfH = plot.getHeight() * 0.5f - 2.0f;
    constexpr float maxDev = 12.0f;

    for (int i = 0; i < n; ++i)
    {
        auto cell = juce::Rectangle<int> (i * colW, 0, colW, getHeight());

        const float raw = devs[(size_t) i];
        const float dev = juce::jlimit (-maxDev, maxDev, raw);
        const float mag = std::abs (dev) / maxDev;
        const float h = juce::jmax (1.0f, mag * halfH);

        const auto colour = std::abs (raw) < 3.5f ? theme::accent
                          : (std::abs (raw) < 6.0f ? theme::warn : theme::bad);

        const float barW = juce::jmax (8.0f, colW * 0.34f);
        juce::Rectangle<float> bar (cell.getCentreX() - barW * 0.5f,
                                    dev >= 0.0f ? zeroY - h : zeroY,
                                    barW, h);

        g.setColour (colour.withAlpha (0.85f));
        g.fillRoundedRectangle (bar, 2.0f);

        if (std::abs (raw) >= 1.5f)
        {
            g.setColour (colour);
            g.setFont (getValueFont());
            g.drawText (sgn (raw, 1),
                        juce::Rectangle<int> (cell.getX() - 2, 0, cell.getWidth() + 4, valueH),
                        juce::Justification::centred);
        }
    }
}

//==============================================================================
SpectrogramDisplay::SpectrogramDisplay()
{
    setOpaque (true);
    getProperties().set ("paddingLeft", 56);
    getProperties().set ("paddingTop", 32);
    getProperties().set ("paddingRight", 32);
    getProperties().set ("paddingBottom", 48);
}

void SpectrogramDisplay::clear()
{
    columns = {};
    bandColumns = {};
    writePos = 0;
    count = 0;
    repaint();
}

juce::Colour SpectrogramDisplay::colourFor (float value)
{
    value = juce::jlimit (0.0f, 1.0f, value);

    // Dark navy -> blue -> cyan -> warm amber. This is deliberately not a
    // rainbow: quiet material stays dark and loud bands stand out.
    const juce::Colour stops[] {
        juce::Colour (0xff07101d),
        juce::Colour (0xff173b7a),
        juce::Colour (0xff1aa6c8),
        juce::Colour (0xff62d38b),
        juce::Colour (0xfff0c24a),
        juce::Colour (0xffef5b4c)
    };

    const float scaled = value * 5.0f;
    const int index = juce::jlimit (0, 4, (int) std::floor (scaled));
    const float t = scaled - (float) index;

    return stops[index].interpolatedWith (stops[index + 1], t);
}

void SpectrogramDisplay::capture (const SpectrumAnalyzer& analyser)
{
    auto& col = columns[(size_t) writePos];
    auto& bands = bandColumns[(size_t) writePos];

    const double range = std::log (20000.0 / 20.0);

    for (int i = 0; i < numBins; ++i)
    {
        const double f = 20.0 * std::exp (range * (double) i / (double) (numBins - 1));
        const float db = analyser.getMagnitudeAt (f);
        col[(size_t) i] = juce::jlimit (0.0f, 1.0f, (db + 100.0f) / 100.0f);
    }

    const auto densities = analyser.getBandDensities();

    for (int i = 0; i < 10; ++i)
        bands[(size_t) i] = juce::jlimit (0.0f, 1.0f, (densities[(size_t) i] + 80.0f) / 80.0f);

    writePos = (writePos + 1) % maxColumns;

    if (count < maxColumns)
        ++count;

    repaint();
}

void SpectrogramDisplay::ensureBackground()
{
    const int w = getWidth();
    const int h = getHeight();

    if (w <= 0 || h <= 0)
        return;

    const float scale = juce::jmax (1.0f, getDesktopScaleFactor());
    background = juce::Image (juce::Image::ARGB,
                              juce::roundToInt ((float) w * scale),
                              juce::roundToInt ((float) h * scale),
                              true);
    juce::Graphics bg (background);
    bg.addTransform (juce::AffineTransform::scale (scale));
    bg.fillAll (theme::card);
    drawGrid (bg);

    backgroundDirty = false;
    cachedThemeRevision = theme::revision();
}

void SpectrogramDisplay::drawGrid (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    auto plot = area.withTrimmedLeft (56.0f).withTrimmedRight (32.0f)
                    .withTrimmedTop (32.0f).withTrimmedBottom (48.0f);

    if (plot.getWidth() < 20.0f || plot.getHeight() < 20.0f)
        return;

    g.setColour (theme::borderSoft);

    for (float t = 0.0f; t <= 1.0f; t += 0.25f)
    {
        const float x = plot.getX() + t * plot.getWidth();
        g.drawVerticalLine (juce::roundToInt (x), plot.getY(), plot.getBottom());
    }

    g.setFont (getValueFont());
    g.setColour (theme::textFaint);

    if (mode == Mode::bands)
    {
        g.setFont (getLabelFont());
        const int labelStride = juce::jmax (1, (int) std::ceil (160.0f / plot.getHeight()));
        for (int i = 0; i < 10; ++i)
        {
            if (i % labelStride != 0) continue;
            const auto& info = SpectrumAnalyzer::getBandInfo (i);
            const float y = plot.getBottom() - (float) (i + 1) * plot.getHeight() / 10.0f
                            + plot.getHeight() / 20.0f;
            g.drawText (T (info.label, info.labelEn), 0, juce::roundToInt (y) - 6,
                        40, 12, juce::Justification::centredRight);
        }
    }
    else
    {
        const double ticks[] { 20000, 10000, 5000, 2000, 500, 100, 20 };
        float lastLabelY = -1000.0f;

        for (double f : ticks)
        {
            const double y = std::log (f / 20.0) / std::log (20000.0 / 20.0);
            const float py = plot.getBottom() - (float) y * plot.getHeight();
            if (py - lastLabelY < 16.0f) continue;
            lastLabelY = py;
            const auto label = f >= 1000.0 ? juce::String (f / 1000.0, 0) + "k"
                                           : juce::String ((int) f);
            g.drawText (label, 0, juce::roundToInt (py) - 6, 40, 12,
                        juce::Justification::centredRight);
        }
    }

    g.setFont (getLabelFont());
    g.drawText (T ("时间", "TIME"), juce::roundToInt (plot.getX()), (int) plot.getBottom() + 4,
                40, 12, juce::Justification::centredLeft);
    g.drawText (T ("现在", "NOW"), juce::roundToInt (plot.getRight() - 40),
                (int) plot.getBottom() + 4, 40, 12, juce::Justification::centredRight);
}

juce::Rectangle<int> SpectrogramDisplay::bandRowBounds (int band) const
{
    if (mode != Mode::bands)
        return {};

    auto plot = getLocalBounds().withTrimmedLeft (56).withTrimmedRight (32)
                               .withTrimmedTop (32).withTrimmedBottom (48);

    if (plot.getHeight() < 20 || plot.getWidth() < 20)
        return {};

    const int b = juce::jlimit (0, 9, band);
    const float cellH = (float) plot.getHeight() / 10.0f;
    const int top = plot.getBottom() - juce::roundToInt ((float) (b + 1) * cellH);
    const int bottom = plot.getBottom() - juce::roundToInt ((float) b * cellH);

    return { plot.getX(), top, plot.getWidth(), juce::jmax (1, bottom - top) };
}

void SpectrogramDisplay::mouseDown (const juce::MouseEvent& e)
{
    if (mode != Mode::bands || onBandClicked == nullptr)
        return;

    for (int b = 0; b < 10; ++b)
        if (bandRowBounds (b).contains (e.getPosition()))
        {
            onBandClicked (b);
            return;
        }
}

void SpectrogramDisplay::mouseMove (const juce::MouseEvent& e)
{
    bool over = false;

    if (mode == Mode::bands)
        for (int b = 0; b < 10 && ! over; ++b)
            over = bandRowBounds (b).contains (e.getPosition());

    setMouseCursor (over ? juce::MouseCursor::PointingHandCursor
                         : juce::MouseCursor::NormalCursor);
}

void SpectrogramDisplay::paint (juce::Graphics& g)
{
    if (backgroundDirty || background.isNull() || cachedThemeRevision != theme::revision())
        ensureBackground();

    g.drawImage (background, getLocalBounds().toFloat());

    auto area = getLocalBounds().toFloat();
    auto plot = area.withTrimmedLeft (56.0f).withTrimmedRight (32.0f)
                    .withTrimmedTop (32.0f).withTrimmedBottom (48.0f);

    if (plot.getWidth() < 20.0f || plot.getHeight() < 20.0f || count <= 0)
        return;

    const float cellW = plot.getWidth() / (float) maxColumns;

    for (int i = 0; i < count; ++i)
    {
        const int index = (writePos - count + i + maxColumns) % maxColumns;
        const float x = plot.getX() + (float) i * cellW;

        if (mode == Mode::bands)
        {
            const float cellH = plot.getHeight() / 10.0f;

            for (int b = 0; b < 10; ++b)
            {
                g.setColour (colourFor (bandColumns[(size_t) index][(size_t) b]));
                g.fillRect (juce::Rectangle<float> (x, plot.getBottom() - (float) (b + 1) * cellH,
                                                    cellW + 0.8f, cellH));
            }
        }
        else
        {
            const float cellH = plot.getHeight() / (float) numBins;

            for (int b = 0; b < numBins; ++b)
            {
                g.setColour (colourFor (columns[(size_t) index][(size_t) b]));
                g.fillRect (juce::Rectangle<float> (x, plot.getBottom() - (float) (b + 1) * cellH,
                                                    cellW + 0.8f, cellH));
            }
        }
    }
}

} // namespace ml
