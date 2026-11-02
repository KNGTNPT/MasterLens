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
        const int h = juce::jlimit (400, 520, full.getHeight() - 100);
        return juce::Rectangle<int> (w, h).withCentre (full.getCentre());
    }

    SettingsLayout settingsLayout (juce::Rectangle<int> full)
    {
        auto card = settingsCard (full).reduced (26, 22);

        SettingsLayout l;
        l.title = card.removeFromTop (24);
        l.subtitle = card.removeFromTop (18);
        card.removeFromTop (14);

        l.section = card.removeFromTop (18);
        card.removeFromTop (6);

        l.buttons = card.removeFromTop (36);
        card.removeFromTop (10);

        l.note = card.removeFromTop (40);
        card.removeFromTop (10);

        l.footer = card.removeFromBottom (34);
        card.removeFromBottom (10);

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
        b.setButtonText (ui::scaleText (ui::scaleSteps[(size_t) i]));
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
    closeButton.setButtonText (T ("关闭", "Close"));

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
    g.setFont (uiFont (18.0f, true));
    g.drawText (T ("设置", "Settings"), textX, l.title.getY() - 2,
                l.title.getWidth() - 56, 24, juce::Justification::centredLeft, false);

    g.setColour (theme::textDim);
    g.setFont (uiFont (tokens::aux));
    g.drawText ("MasterLens " + juce::String (ml::kVersion) + "  |  A BerryStudio product",
                textX, l.subtitle.getY(), l.subtitle.getWidth() - 56, 18,
                juce::Justification::centredLeft, false);

    g.setColour (theme::divider);
    g.drawHorizontalLine (l.title.getY() + 56, (float) l.title.getX(),
                          (float) l.title.getRight());

    g.setColour (theme::accent);
    g.setFont (uiFont (tokens::unit, true));
    g.drawText (T ("界面缩放 / 字号", "INTERFACE SCALE / TYPE SIZE"),
                l.section, juce::Justification::centredLeft, false);

    g.setColour (theme::textDim);
    g.setFont (uiFont (tokens::aux));
    g.drawText (T ("字号与布局一起等比放大，不会截断任何文字；窗口尺寸随缩放同步调整。",
                   "Type and layout zoom together, so no label is clipped; the window resizes "
                   "with the scale."),
                l.note, juce::Justification::topLeft, false);

    auto effective = l.note.withTrimmedTop (22);
    g.setColour (theme::textFaint);
    g.setFont (monoFont (tokens::unit));
    g.drawText (T ("当前 ", "Now ") + ui::scaleText (scale)
                    + T ("  ·  等效字号 ", "  |  effective ")
                    + juce::String (juce::roundToInt (tokens::unit * scale)) + " / "
                    + juce::String (juce::roundToInt (tokens::label * scale)) + " / "
                    + juce::String (juce::roundToInt (tokens::section * scale)) + " px",
                effective, juce::Justification::centredLeft);

    // ---- reference list -----------------------------------------------------
    const auto row = [&] (const juce::String& key, const juce::String& value)
    {
        auto r = l.list.removeFromTop (22);

        g.setColour (theme::textFaint);
        g.setFont (uiFont (tokens::unit));
        g.drawText (key, r.removeFromLeft (120), juce::Justification::centredLeft, false);

        g.setColour (theme::textDim);
        g.setFont (uiFont (tokens::aux));
        g.drawText (value, r, juce::Justification::centredLeft, false);
    };

    row (T ("快捷键", "Shortcuts"), T ("空格 保持 · R 清零 · 1-9 模块 · 0 曲风目标 · Esc 关闭 · Cmd+E 导出",
                                        "Space hold | R clear | 1-9 pages | 0 genre target | Esc close | Cmd+E export"));
    row (T ("缩放快捷键", "Zoom keys"), T ("Cmd + = 放大 · Cmd + - 缩小",
                                            "Cmd + = larger | Cmd + - smaller"));
    row (T ("主题 / 语言", "Theme / language"), T ("顶部按钮即时切换，偏好随工程保存",
                                                    "Header buttons; stored with the session"));
    row (T ("测量", "Measurement"), T ("只读分析，零延迟，不修改音频",
                                       "Read-only analysis, zero latency, never alters audio"));

    // ---- footer -------------------------------------------------------------
    paintBerryLockup (g, l.footer.withWidth (230), theme::textDim, 26.0f, 8.5f);

    g.setColour (theme::textFaint);
    g.setFont (uiFont (tokens::unit));
    g.drawText ("BerryStudio  |  " + juce::String::fromUTF8 (ml::kDeveloper),
                l.footer.withTrimmedRight (120), juce::Justification::centredRight, false);
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

    closeButton.setBounds (l.footer.getRight() - 96, l.footer.getY() - 2, 96, 30);
}

void SettingsPanel::mouseDown (const juce::MouseEvent& e)
{
    if (! cardBounds().contains (e.getPosition()))
        if (onClose != nullptr)
            onClose();
}
} // namespace ml

