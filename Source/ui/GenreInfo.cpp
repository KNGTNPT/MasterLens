#include "Widgets.h"
#include "../GenreWar.h"
#include "Logo.h"

namespace ml
{
namespace
{
    struct Layout
    {
        juce::Rectangle<int> title, facts, body, advice, sliders, close;
    };

    juce::Rectangle<int> cardBounds (juce::Rectangle<int> full)
    {
        const int w = juce::jlimit (620, 820, full.getWidth() - 120);
        const int h = juce::jlimit (460, 600, full.getHeight() - 100);
        return juce::Rectangle<int> (w, h).withCentre (full.getCentre());
    }

    Layout computeLayout (juce::Rectangle<int> full)
    {
        auto card = cardBounds (full).reduced (28, 22);

        Layout l;
        l.title = card.removeFromTop (28);
        card.removeFromTop (4);
        l.facts = card.removeFromTop (20);
        card.removeFromTop (12);
        l.body = card.removeFromTop (juce::jmin (96, card.getHeight() / 3));

        auto foot = card.removeFromBottom (44);
        l.close = foot.removeFromRight (96);
        l.sliders = card.removeFromBottom (juce::jmin (118, card.getHeight() / 2));
        card.removeFromBottom (8);
        l.advice = card;
        return l;
    }

    /** Manual wrap: CJK has no spaces, so juce's word wrap would shrink the text. */
    juce::String wrap (const juce::String& in, int maxChars)
    {
        juce::String out;
        int len = 0;

        for (int i = 0; i < in.length(); ++i)
        {
            const auto c = in[i];
            out << c;

            if (c == '\n')
            {
                len = 0;
                continue;
            }

            if (++len >= maxChars && i + 1 < in.length())
            {
                out << '\n';
                len = 0;
            }
        }

        return out;
    }
} // namespace

//==============================================================================
GenrePanel::GenrePanel()
{
    setOpaque (false);

    const auto captionFont = getLabelFont();

    for (auto* l : { &loudnessCaption, &lraCaption, &clipCaption })
    {
        l->setFont (captionFont);
        l->setColour (juce::Label::textColourId, theme::textDim);
        addAndMakeVisible (*l);
    }

    for (auto* s : { &loudness, &lra, &clip })
    {
        s->setSliderStyle (juce::Slider::LinearHorizontal);
        s->setTextBoxStyle (juce::Slider::TextBoxRight, true, 68, 24);
        addAndMakeVisible (*s);
    }

    loudness.setRange (-30.0, -5.0, 0.1);
    loudness.setDoubleClickReturnValue (true, -14.0);
    lra.setRange (1.0, 20.0, 0.5);
    lra.setDoubleClickReturnValue (true, 8.0);
    clip.setRange (0.0, 3.0, 0.1);
    clip.setDoubleClickReturnValue (true, 0.0);

    const auto changed = [this]
    {
        if (updating)
            return;

        repaint();

        if (onSettingsChanged != nullptr)
            onSettingsChanged ((float) lra.getValue(), (float) clip.getValue());

        if (onTargetChanged != nullptr)
            onTargetChanged ((float) loudness.getValue());
    };

    loudness.onValueChange = changed;
    lra.onValueChange = changed;
    clip.onValueChange = changed;

    closeButton.onClick = [this] { if (onClose != nullptr) onClose(); };
    addAndMakeVisible (closeButton);
}

//==============================================================================
void GenrePanel::show (int index, float target, float lraTarget, float clipAllow)
{
    presetIndex = juce::jlimit (0, presets::count - 1, index);

    updating = true;
    loudness.setValue (target, juce::dontSendNotification);
    lra.setValue (lraTarget, juce::dontSendNotification);
    clip.setValue (clipAllow, juce::dontSendNotification);
    updating = false;

    refreshTexts();
    setVisibleAndFocus (true);
}

void GenrePanel::setVisibleAndFocus (bool shouldBeVisible)
{
    setVisible (shouldBeVisible);

    if (shouldBeVisible)
    {
        toFront (true);
        refreshTexts();
        closeButton.grabKeyboardFocus();
    }
}

void GenrePanel::refreshTexts()
{
    const auto& e = presets::entries[presetIndex];
    const auto& w = presets::war::notes[presetIndex];
    const bool en = Localisation::isEnglish();

    closeButton.setButtonText (ml::grid::controlText (T ("关闭", "Close")));
    loudnessCaption.setText (T ("目标响度 LUFS", "Target LUFS"), juce::dontSendNotification);
    lraCaption.setText (T ("LRA 目标 LU", "LRA goal LU"), juce::dontSendNotification);
    clipCaption.setText (T ("允许削波 dB", "Clip allowed dB"), juce::dontSendNotification);

    bodyText = wrap (U8 (en ? w.en : w.zh), en ? 62 : 44);

    if (e.kind == 0)
        factsText = T ("规范值 ", "Spec ") + U8 (en ? e.en : e.zh) + sep()
                    + "TP " + num (e.ceiling) + " dBTP";
    else
        factsText = T ("发行作品实测 ", "Released masters ") + num (w.lufsLo, 0) + " ~ " + num (w.lufsHi, 0)
                    + " LUFS" + sep() + T ("目标 ", "target ") + num (e.target) + " LUFS"
                    + sep() + "LRA " + num (e.lraLo, 0) + "-" + num (e.lraHi, 0) + " LU";

    repaint();
}

juce::String GenrePanel::adviceText() const
{
    const auto& e = presets::entries[presetIndex];
    const bool en = Localisation::isEnglish();

    const float target = (float) loudness.getValue();
    const float clipDb = (float) clip.getValue();
    const float gr = target <= -9.5f ? 4.0f : (target <= -13.5f ? 2.5f : 1.0f);
    const int release = target <= -9.5f ? 120 : (target <= -13.5f ? 220 : 400);
    const auto ratio = juce::String (target <= -12.0f ? "1.5:1" : "2:1");

    juce::StringArray rows;

    if (en)
    {
        rows.add ("Chain: bus EQ -> slow comp (ratio " + ratio
                  + ", 10-30 ms attack, auto release) -> saturation -> clipper "
                  + num (clipDb) + " dB -> limiter");
        rows.add ("Limiter: ceiling " + num (e.ceiling) + " dBTP, about " + num (gr)
                  + " dB GR, " + juce::String (release) + " ms release, lookahead on");
        rows.add ("Below target by 1 LU: raise limiter input in 0.5 dB steps. "
                  "Above target by 1 LU: back off 0.5 dB and recheck true peak");
        rows.add ("LRA under " + num (e.lraLo, 0) + " LU: more compression than this style needs - "
                  "drop the ratio or let the clipper take the peaks");
    }
    else
    {
        rows.add (U8 ("建议链路：总线 EQ → 慢压缩（比例 ") + ratio
                  + U8 ("，attack 10-30 ms，auto release）→ 饱和 → 削波 ")
                  + num (clipDb) + U8 (" dB → 限幅器"));
        rows.add (U8 ("限幅器：ceiling ") + num (e.ceiling) + " dBTP" + U8 ("，压出约 ")
                  + num (gr) + U8 (" dB 增益衰减，release ") + juce::String (release)
                  + U8 (" ms，开启 lookahead"));
        rows.add (U8 ("实测低于目标 1 LU：按 0.5 dB 步进提高限幅输入；高于目标 1 LU：回退 0.5 dB 并复查真峰值"));
        rows.add (U8 ("LRA 低于 ") + num (e.lraLo, 0) + U8 (" LU：压缩量已超过该曲风需要，降比例或让削波吃掉峰值"));
    }

    if (clipDb >= 1.5f)
        rows.add (U8 (en ? "Clipping past 1.5 dB is audible on transients - check drum hits before shipping"
                         : "削波超过 1.5 dB 会在瞬态上听出来，交付前重点检查鼓点"));

    if (e.kind == 0)
        rows.add (U8 (en ? "Platform spec: do not chase loudness here, the service normalises it anyway"
                         : "平台规范：不必在这里追响度，平台会统一归一化"));

    return rows.joinIntoString ("\n");
}

//==============================================================================
void GenrePanel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (theme::dark ? 0.55f : 0.28f));

    const auto card = cardBounds (getLocalBounds()).toFloat();

    surface::raised (g, card, tokens::radiusCard);

    auto l = computeLayout (getLocalBounds());
    const auto& e = presets::entries[presetIndex];
    const bool en = Localisation::isEnglish();

    g.setColour (theme::text);
    g.setFont (getLabelFont());
    g.drawText (U8 (en ? e.en : e.zh), l.title.withTrimmedRight (112),
                juce::Justification::centredLeft, true);

    paintBerryWordmark (g, l.title.withTrimmedLeft (l.title.getWidth() - 104),
                        theme::textFaint, 8.5f, juce::Justification::right);

    g.setColour (theme::textFaint);
    g.setFont (getValueFont());
    g.drawText (factsText, l.facts, juce::Justification::centredLeft, false);

    g.setColour (theme::textDim);
    g.setFont (getLabelFont());
    g.drawFittedText (bodyText, l.body, juce::Justification::topLeft,
                      juce::jmax (2, l.body.getHeight() / 15), 0.9f);

    auto adviceArea = l.advice;
    adviceArea = adviceArea.withTop (l.body.getBottom() + 14).withBottom (l.sliders.getY() - 6);

    g.setColour (theme::accent.withAlpha (0.9f));
    g.setFont (getLabelFont());
    g.drawText (T ("调整建议  HOW TO GET THERE", "HOW TO GET THERE"),
                adviceArea.removeFromTop (16), juce::Justification::topLeft, false);

    // One fixed line box per row: drawFittedText() squeezes multi-line CJK blocks
    // into a single cramped box, which reads as overlapping text.
    g.setColour (theme::textDim);
    g.setFont (getLabelFont());

    const int lineH = 15;

    for (const auto& line : juce::StringArray::fromLines (adviceText()))
    {
        if (adviceArea.getHeight() < lineH)
            break;

        g.drawText (line, adviceArea.removeFromTop (lineH), juce::Justification::topLeft, false);
    }
}

void GenrePanel::resized()
{
    const auto l = computeLayout (getLocalBounds());

    const int labelW = 118;
    const int rowH = juce::jmax (26, l.sliders.getHeight() / 3);

    auto s = l.sliders;

    auto placeRow = [&] (juce::Label& cap, juce::Slider& sl)
    {
        auto row = s.removeFromTop (rowH);
        cap.setBounds (row.removeFromLeft (labelW));
        sl.setBounds (row.withSizeKeepingCentre (row.getWidth(), 24));
    };

    placeRow (loudnessCaption, loudness);
    placeRow (lraCaption, lra);
    placeRow (clipCaption, clip);

    closeButton.setBounds (l.close.withSizeKeepingCentre (96, 30));
    grid::alignChildren (*this);
}

void GenrePanel::mouseDown (const juce::MouseEvent& e)
{
    if (! cardBounds (getLocalBounds()).contains (e.getPosition()))
        if (onClose != nullptr)
            onClose();
}

} // namespace ml
