#include "Widgets.h"
#include "../Version.h"
#include "Logo.h"

namespace ml
{
namespace
{
    struct SettingsLayout
    {
        juce::Rectangle<int> title, subtitle, section, buttons, note, list, footer;
    };

    juce::Rectangle<int> settingsCard (juce::Rectangle<int> full)
    {
        const int w = juce::jlimit (540, 720, full.getWidth() - 120);
        const int h = juce::jlimit (380, 430, full.getHeight() - 100);
        return juce::Rectangle<int> (w, h).withCentre (full.getCentre());
    }

    SettingsLayout settingsLayout (juce::Rectangle<int> full);

    struct FooterParts
    {
        juce::Rectangle<int> brand, developer, close;
    };

    FooterParts settingsFooterParts (juce::Rectangle<int> full)
    {
        const auto footer = settingsLayout (full).footer;
        constexpr int brandW = 150, developerW = 170, closeW = 80, gap = 16;
        const int total = brandW + developerW + closeW + gap * 2;
        // Snap the whole group once: grid::alignChildren() used to shrink only the
        // real button (30 -> 28 px, y 555 -> 552), lifting it ~4 px above the two
        // painted labels. Snapping here keeps paint and hit bounds on one axis.
        const int h = grid::down (juce::jmin (30, footer.getHeight()));
        const int x = footer.getCentreX() - total / 2;
        const int y = grid::down (footer.getCentreY() - h / 2);

        FooterParts p;
        p.brand = { x, y, brandW, h };
        p.developer = { p.brand.getRight() + gap, y, developerW, h };
        p.close = { p.developer.getRight() + gap, y, closeW, h };
        return p;
    }

    SettingsLayout settingsLayout (juce::Rectangle<int> full)
    {
        auto card = settingsCard (full).reduced (26, 22);

        SettingsLayout l;
        l.title = card.removeFromTop (24);
        l.subtitle = card.removeFromTop (20);
        card.removeFromTop (16);

        l.section = card.removeFromTop (20);
        card.removeFromTop (8);

        l.buttons = card.removeFromTop (36);
        card.removeFromTop (12);

        l.note = card.removeFromTop (40);
        card.removeFromTop (12);

        l.footer = card.removeFromBottom (36);
        card.removeFromBottom (12);

        l.list = card;
        return l;
    }
} // namespace

//==============================================================================
SettingsPanel::SettingsPanel()
{
    for (int i = 0; i < (int) ui::scaleSteps.size(); ++i)
    {
        auto& b = scaleButtons[(size_t) i];
        b.setButtonText (ml::grid::controlText (ui::scaleText (ui::scaleSteps[(size_t) i])));
        b.setClickingTogglesState (true);
        b.setRadioGroupId (7717);
        b.onClick = [this, i]
        {
            if (onScaleChanged != nullptr)
                onScaleChanged (ui::scaleSteps[(size_t) i]);
        };
        addAndMakeVisible (b);
    }

    closeButton.onClick = [this] { if (onClose != nullptr) onClose(); };
    addAndMakeVisible (closeButton);

    refreshTexts();
}

void SettingsPanel::refreshTexts()
{
    closeButton.setButtonText (ml::grid::controlText (T ("关闭", "Close")));

    for (int i = 0; i < (int) ui::scaleSteps.size(); ++i)
        scaleButtons[(size_t) i].setTooltip (T ("界面缩放 ", "Interface scale ")
                                             + ui::scaleText (ui::scaleSteps[(size_t) i]));
}

void SettingsPanel::setScale (float newScale)
{
    scale = juce::jlimit (ui::minScale, ui::maxScale, newScale);

    for (int i = 0; i < (int) ui::scaleSteps.size(); ++i)
        scaleButtons[(size_t) i].setToggleState (std::abs (ui::scaleSteps[(size_t) i] - scale) < 0.01f,
                                                 juce::dontSendNotification);

    repaint();
}

void SettingsPanel::setVisibleAndFocus (bool shouldBeVisible)
{
    setVisible (shouldBeVisible);

    if (shouldBeVisible)
    {
        toFront (true);
        refreshTexts();
        setScale (scale);
        closeButton.grabKeyboardFocus();
    }
}

juce::Rectangle<int> SettingsPanel::cardBounds() const
{
    return settingsCard (getLocalBounds());
}

void SettingsPanel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (theme::dark ? 0.55f : 0.28f));

    const auto card = cardBounds().toFloat();

    surface::raised (g, card, tokens::radiusCard);

    auto l = settingsLayout (getLocalBounds());

    const auto mark = juce::Rectangle<float> ((float) l.title.getX(), (float) l.title.getY(),
                                              44.0f, 44.0f);
    paintLogoMark (g, mark);

    const int textX = l.title.getX() + 56;

    g.setColour (theme::text);
    g.setFont (getLabelFont());
    g.drawText (T ("设置", "Settings"), textX, l.title.getY() - 2,
                l.title.getWidth() - 56, 24, juce::Justification::centredLeft, false);

    g.setColour (theme::textDim);
    g.setFont (getLabelFont());
    const auto subtitle = juce::String ("MasterLens ") + ml::kVersion
                          + T ("  |  一个 BerryStudio 产品", "  |  A BerryStudio product");
    g.drawText (subtitle, textX, l.subtitle.getY(), l.subtitle.getWidth() - 56, 18,
                juce::Justification::centredLeft, false);

    g.setColour (theme::divider);
    g.drawHorizontalLine (l.title.getY() + 56, (float) l.title.getX(),
                          (float) l.title.getRight());

    g.setColour (theme::accent);
    g.setFont (getLabelFont());
    g.drawText (T ("界面缩放 / 字号", "INTERFACE SCALE / TYPE SIZE"),
                l.section, juce::Justification::centredLeft, false);

    g.setColour (theme::textDim);
    g.setFont (getLabelFont());
    g.drawText (T ("字号与布局一起等比放大，不会截断任何文字；窗口尺寸随缩放同步调整。",
                   "Type and layout zoom together, so no label is clipped; the window resizes "
                   "with the scale."),
                l.note, juce::Justification::topLeft, false);

    auto effective = l.note.withTrimmedTop (24);
    g.setColour (theme::textFaint);
    g.setFont (getValueFont());
    g.drawText (T ("当前 ", "Now ") + ui::scaleText (scale)
                    + T ("  ·  字高 ", "  |  type height ")
                    + juce::String (juce::roundToInt (getLabelFont().getHeight() * scale)) + " / "
                    + juce::String (juce::roundToInt (getValueFont().getHeight() * scale)) + " / "
                    + juce::String (juce::roundToInt (getCriticalFont().getHeight() * scale)) + " px",
                effective, juce::Justification::centredLeft);

    // ---- reference list -----------------------------------------------------
    const auto row = [&] (const juce::String& key, const juce::String& value)
    {
        auto r = l.list.removeFromTop (28);

        g.setColour (theme::textFaint);
        g.setFont (getLabelFont());
        g.drawText (key, r.removeFromLeft (152), juce::Justification::centredLeft, false);

        g.setColour (theme::textDim);
        g.setFont (getLabelFont());
        g.drawText (value, r, juce::Justification::centredLeft, false);
    };

    row (T ("快捷键", "Shortcuts"), T ("Shift+空格 保持 · R 清零 · 1–9 模块",
                                        "Shift+Space hold · R clear · 1–9 pages"));
    row ("", T ("0 曲风目标 · Esc 关闭 · Cmd+E 导出",
                "0 genre target · Esc close · Cmd+E export"));
    row (T ("缩放快捷键", "Zoom keys"), T ("Cmd+= 放大 · Cmd+- 缩小",
                                            "Cmd+= larger · Cmd+- smaller"));
    row (T ("主题 / 语言", "Theme / language"), T ("顶部按钮即时切换，偏好随工程保存",
                                                    "Header buttons; stored with the session"));
    row (T ("测量", "Measurement"), T ("只读分析，零延迟，不修改音频",
                                       "Read-only analysis, zero latency, never alters audio"));

    // ---- footer: one centred horizontal group --------------------------------
    const auto footer = settingsFooterParts (getLocalBounds());
    paintBerryLockup (g, footer.brand, theme::textDim, 26.0f, 8.5f);

    g.setColour (theme::textFaint);
    g.setFont (getLabelFont());
    g.drawText (T ("开发者 ", "Developer ") + juce::String::fromUTF8 (
                    Localisation::isEnglish() ? ml::kDeveloperEnglish : ml::kDeveloper),
                footer.developer, juce::Justification::centredLeft, false);
}

void SettingsPanel::resized()
{
    const auto l = settingsLayout (getLocalBounds());

    auto row = l.buttons;
    const int gap = 8;
    const int w = (row.getWidth() - gap * 4) / 5;

    for (int i = 0; i < (int) scaleButtons.size(); ++i)
    {
        auto cell = row.removeFromLeft (w);
        scaleButtons[(size_t) i].setBounds (cell.withSizeKeepingCentre (w, 34));
        row.removeFromLeft (gap);
    }

    closeButton.setBounds (settingsFooterParts (getLocalBounds()).close);
    grid::alignChildren (*this);
}

void SettingsPanel::mouseDown (const juce::MouseEvent& e)
{
    if (! cardBounds().contains (e.getPosition()))
        if (onClose != nullptr)
            onClose();
}
} // namespace ml

