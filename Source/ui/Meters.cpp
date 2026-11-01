#include "Widgets.h"

namespace ml
{
//==============================================================================
void LevelMeters::setValues (const StatsSnapshot& s, float targetLufs)
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    const float dt = lastUpdateMs > 0.0
                         ? (float) juce::jlimit (0.001, 0.05, (now - lastUpdateMs) / 1000.0)
                         : 0.016f;
    lastUpdateMs = now;

    stats = s;
    target = targetLufs;

    const float attack = 1.0f - std::exp (-dt / 0.035f);
    const float release = 1.0f - std::exp (-dt / 0.30f);

    const auto follow = [&] (float& shown, float targetValue)
    {
        if (targetValue <= -99.0f)
            return;

        if (shown <= -99.0f)
            shown = targetValue;
        else
            shown += (targetValue - shown) * (targetValue > shown ? attack : release);
    };

    follow (displayM, s.momentary);
    follow (displayS, s.shortTerm);

    // Peak hold falls at 10 dB/s, independent of the timer rate.
    holdM = juce::jmax (displayM, holdM - 10.0f * dt);
    holdS = juce::jmax (displayS, holdS - 10.0f * dt);

    repaint();
}

void LevelMeters::paintRow (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& label,
                            const juce::String& windowText, float valueDb, float holdDb, juce::Colour colour)
{
    auto row = area;

    g.setColour (theme::textDim);
    g.setFont (uiFont (tokens::label));
    g.drawText (label, row.getX(), row.getY() + 2, 66, 17, juce::Justification::centredLeft);

    if (row.getHeight() >= 36)
    {
        g.setColour (theme::textFaint);
        g.setFont (uiFont (tokens::unit));
        g.drawText (windowText, row.getX(), row.getY() + 20, 66, 14, juce::Justification::centredLeft);
    }

    auto readout = row.removeFromRight (64);
    const bool over = valueDb > 0.0f;

    g.setColour (theme::text);
    g.setFont (monoFont (16.0f, true));
    g.drawText (valueDb <= -99.0f ? noValue() : num (valueDb), readout,
                juce::Justification::centredRight);

    if (over)
    {
        g.setColour (theme::bad);
        g.fillRoundedRectangle ((float) readout.getRight() - 24.0f,
                                (float) readout.getBottom() - 3.0f, 24.0f, 2.0f, 1.0f);
    }

    auto trackArea = row.withTrimmedLeft (72).withTrimmedRight (12);
    auto track = juce::Rectangle<float> ((float) trackArea.getX(),
                                         (float) trackArea.getCentreY() - 2.0f,
                                         (float) trackArea.getWidth(), 4.0f);

    g.setColour (theme::borderSoft);
    g.fillRoundedRectangle (track, 2.0f);

    const auto normFor = [] (float db) { return juce::jlimit (0.0f, 1.0f, (db + 36.0f) / 36.0f); };

    if (valueDb > -99.0f)
    {
        const float w = juce::jmax (4.0f, track.getWidth() * normFor (valueDb));
        auto fill = track.withWidth (w);

        g.setGradientFill (juce::ColourGradient (colour.withAlpha (0.55f), fill.getX(), fill.getY(),
                                                 colour, fill.getRight(), fill.getY(), false));
        g.fillRoundedRectangle (fill, 2.0f);
    }

    // 2 px peak hold at the current / recent maximum.
    const float hold = juce::jlimit (-99.0f, 3.0f, holdDb > -90.0f ? holdDb : -99.0f);
    if (hold > -90.0f)
    {
        const float hx = track.getX() + track.getWidth() * normFor (hold);
        g.setColour (theme::text.withAlpha (0.75f));
        g.fillRect (hx - 1.0f, track.getY() - 3.0f, 2.0f, track.getHeight() + 6.0f);
    }

    // target line
    const float tx = track.getX() + track.getWidth() * normFor (target);
    g.setColour (theme::accent.withAlpha (0.75f));
    g.fillRect (tx - 0.5f, track.getY() - 4.0f, 1.0f, track.getHeight() + 8.0f);

    const float tickDbs[] { -36.0f, -24.0f, -12.0f, -6.0f, 0.0f };

    for (float db : tickDbs)
    {
        const float x = track.getX() + track.getWidth() * normFor (db);

        g.setColour (theme::divider);
        g.fillRect (x - 0.5f, track.getBottom() + 2.0f, 1.0f, 4.0f);
    }
}

void LevelMeters::paint (juce::Graphics& g)
{
    auto b = getLocalBounds();

    if (b.getHeight() < 40)
        return;

    auto scaleRow = b.removeFromBottom (14);
    const int rowH = b.getHeight() / 2;

    paintRow (g, b.removeFromTop (rowH), T ("M 瞬时", "M momentary"), "400 ms",
              displayM, holdM, theme::accent);

    paintRow (g, b, T ("S 短时", "S short-term"), "3 s",
              displayS, holdS, theme::accent.withAlpha (0.72f));

    // ---- shared scale labels -------------------------------------------------
    const auto trackArea = scaleRow.withTrimmedLeft (72).withTrimmedRight (76);
    const auto normFor = [] (float db) { return juce::jlimit (0.0f, 1.0f, (db + 36.0f) / 36.0f); };
    const float labelDbs[] { -36.0f, -24.0f, -12.0f, -6.0f, 0.0f };

    g.setFont (monoFont (tokens::unit));

    for (float db : labelDbs)
    {
        const float x = (float) trackArea.getX() + (float) trackArea.getWidth() * normFor (db);
        const juce::String text = db == 0.0f ? juce::String ("0") : num (db, 0);
        const int tw = 30;

        auto lr = juce::Rectangle<int> (juce::roundToInt (x) - tw / 2, scaleRow.getY(), tw, 12);
        lr.setX (juce::jlimit (trackArea.getX(), trackArea.getRight() - tw, lr.getX()));

        g.setColour (theme::textFaint);
        g.drawText (text, lr, juce::Justification::centred);
    }
}
} // namespace ml
