#include "PluginEditor.h"

using ml::T;
#include "Version.h"
#include "ui/Logo.h"

//==============================================================================
void MasterLensAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (ml::theme::background);
    g.addTransform (juce::AffineTransform::scale (uiScale));

    // ---- header -------------------------------------------------------------
    {
        auto content = headerArea.reduced (14, 10);
        auto logo = content.removeFromLeft (32).toFloat();

        ml::paintLogoMark (g, logo);

        content.removeFromLeft (12);

        const int textX = content.getX();
        const int brandRight = juce::jmax (textX + 80, presetBox.getX() - 12);
        const int maxW = juce::jmax (80, brandRight - textX);

        g.saveState();
        g.reduceClipRegion (juce::Rectangle<int> (textX, headerArea.getY(), maxW, headerArea.getHeight()));

        const auto nameFont = ml::uiFont (15.0f, true);
        const auto name = juce::String ("MasterLens");
        const int nameW = juce::roundToInt (juce::GlyphArrangement::getStringWidth (nameFont, name));

        g.setColour (ml::theme::text);
        g.setFont (nameFont);
        g.drawText (name, textX, content.getY() - 1, nameW + 4, 22,
                    juce::Justification::centredLeft, false);

        int x = textX + nameW + 10;

        const auto versionText = "v" + juce::String (ml::kVersion);
        const auto versionFont = ml::uiFont (ml::tokens::aux);
        const int versionW = juce::roundToInt (
            juce::GlyphArrangement::getStringWidth (versionFont, versionText)) + 12;

        if (x + versionW < brandRight)
        {
            auto chip = juce::Rectangle<int> (x, content.getY(), versionW, 17);

            g.setColour (ml::theme::cardAlt);
            g.fillRoundedRectangle (chip.toFloat(), 8.5f);
            g.setColour (ml::theme::border);
            g.drawRoundedRectangle (chip.toFloat().reduced (0.5f), 8.5f, 1.0f);
            g.setColour (ml::theme::textFaint);
            g.setFont (versionFont);
            g.drawText (versionText, chip, juce::Justification::centred, false);

            x += versionW + 8;
        }

        const auto devFont = ml::uiFont (ml::tokens::unit, true);
        const auto devText = juce::String ("by BerryStudio");
        const int devW = juce::roundToInt (juce::GlyphArrangement::getStringWidth (devFont, devText)) + 4;

        if (x + devW < brandRight)
        {
            g.setColour (ml::theme::cyan);
            g.setFont (devFont);
            g.drawText (devText, x, content.getY() + 1, devW, 20,
                        juce::Justification::centredLeft, false);
        }

        g.setColour (ml::theme::textDim);
        g.setFont (ml::uiFont (ml::tokens::unit));
        g.drawText (T ("BerryStudio · 母带交付检查 · 默认旁路，试听时改变输出",
                       "BerryStudio · mastering QC · bypassed unless auditioning"),
                    textX, content.getY() + 24, maxW, 14, juce::Justification::centredLeft);

        g.restoreState();

        if (frozen && maxW > 360)
        {
            const auto pill = juce::Rectangle<int> (brandRight - 64, content.getY() + 23, 56, 16);

            g.setColour (ml::theme::warn.withAlpha (0.16f));
            g.fillRoundedRectangle (pill.toFloat(), 8.0f);
            g.setColour (ml::theme::warn);
            g.setFont (ml::uiFont (ml::tokens::unit));
            g.drawText (T ("已保持", "Held"), pill, juce::Justification::centred);
        }

        g.setColour (ml::theme::border);
        g.drawHorizontalLine (headerArea.getBottom() - 1, 0.0f, (float) baseBounds().getWidth());
    }

    // ---- parameter bar ------------------------------------------------------
    {
        g.setColour (ml::theme::card);
        g.fillRect (paramArea);

        g.setColour (ml::theme::border);
        g.drawHorizontalLine (paramArea.getBottom() - 1, 0.0f, (float) baseBounds().getWidth());
    }

    // ---- footer -------------------------------------------------------------
    {
        g.setColour (ml::theme::border);
        g.drawHorizontalLine (footerArea.getY(), 0.0f, (float) baseBounds().getWidth());

        auto f = footerArea.reduced (14, 4);

        g.setColour (ml::theme::textFaint);
        g.setFont (ml::monoFont (ml::tokens::unit));
        auto tech = juce::String (processor.getSampleRateValue() / 1000.0, 1) + " kHz"
                    + ml::sep() + (stats.stereo ? T ("立体声", "stereo") : T ("单声道", "mono"))
                    + ml::sep() + juce::String ("4") + ml::times() + " TP";

        if (stats.hostLength > 0.0)
            tech << ml::sep() << T ("覆盖 ", "cov ") << ml::num (stats.coveragePct) << "%";

        g.drawText (tech, f.getX(), f.getY(), 360, f.getHeight(),
                    juce::Justification::centredLeft);

        if (footerStatusText.isNotEmpty())
        {
            auto pill = juce::Rectangle<int> (f.getCentreX() - 46, f.getY(), 92, f.getHeight());

            g.setColour (footerStatusColour.withAlpha (0.12f));
            g.fillRoundedRectangle (pill.toFloat(), 9.0f);

            g.setColour (footerStatusColour);
            g.fillEllipse ((float) pill.getX() + 12.0f, (float) pill.getCentreY() - 3.0f, 6.0f, 6.0f);

            g.setFont (ml::uiFont (ml::tokens::unit));
            g.drawText (footerStatusText, pill.withTrimmedLeft (24),
                        juce::Justification::centredLeft);
        }

        // Developer lockup, right-aligned before the A / B chips.
        if (snapshotA.getX() > f.getX() + 400)
        {
            auto brand = juce::Rectangle<int> (f.getX() + 340, f.getY(),
                                               snapshotA.getX() - f.getX() - 352, f.getHeight());

            ml::paintBerryWordmark (g, brand, ml::theme::textFaint, 9.0f,
                                    juce::Justification::right);
        }
    }
}
