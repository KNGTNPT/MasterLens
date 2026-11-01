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
            g.fillRoundedRectangle (chip.toFloat(), 4.0f);
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

        const auto verdict = rail.getVerdictHeadline().isNotEmpty()
                                 ? rail.getVerdictHeadline()
                                 : T ("等待信号", "Waiting for signal");
        const auto verdictColour = rail.getVerdictHeadline().isNotEmpty()
                                       ? rail.getVerdictColour() : ml::theme::textFaint;

        g.setColour (verdictColour);
        g.fillEllipse ((float) textX, (float) content.getY() + 27.0f, 7.0f, 7.0f);
        g.setColour (ml::theme::textDim);
        g.setFont (ml::uiFont (ml::tokens::unit));
        g.drawText (verdict, textX + 14, content.getY() + 24, maxW - 132, 14,
                    juce::Justification::centredLeft, false);

        g.restoreState();

        // One output state in the header: BYPASS / MATCH / REFERENCE / AUDITION.
        juce::String outState = T ("BYPASS", "BYPASS");
        auto outColour = ml::theme::textFaint;

        if (processor.referencePlayer.isEnabled())
        {
            outState = T ("REFERENCE", "REFERENCE");
            outColour = ml::theme::warn;
        }
        else if (processor.isAuditionActive())
        {
            outState = T ("AUDITION", "AUDITION");
            outColour = ml::theme::accent;
        }
        else if (processor.isMonitorMatch())
        {
            outState = T ("MATCH ", "MATCH ") + ml::sgn (processor.getMonitorTrimDb(), 1) + " dB";
            outColour = ml::theme::accent;
        }

        const auto outFont = ml::uiFont (ml::tokens::unit);
        const int outW = juce::jmin (116, juce::roundToInt (
            juce::GlyphArrangement::getStringWidth (outFont, outState)) + 22);
        auto outChip = juce::Rectangle<int> (brandRight - outW, content.getY() + 1, outW, 18);

        g.setColour (outColour.withAlpha (0.12f));
        g.fillRoundedRectangle (outChip.toFloat(), 4.0f);
        g.setColour (outColour);
        g.fillEllipse ((float) outChip.getX() + 8.0f, (float) outChip.getCentreY() - 3.0f, 6.0f, 6.0f);
        g.setFont (outFont);
        g.drawText (outState, outChip.withTrimmedLeft (19), juce::Justification::centredLeft, false);

        if (frozen && maxW > 360)
        {
            const auto pill = juce::Rectangle<int> (brandRight - 176, content.getY() + 23, 56, 16);

            g.setColour (ml::theme::warn.withAlpha (0.16f));
            g.fillRoundedRectangle (pill.toFloat(), 4.0f);
            g.setColour (ml::theme::warn);
            g.setFont (ml::uiFont (ml::tokens::unit));
            g.drawText (T ("已保持", "Held"), pill, juce::Justification::centred);
        }

        g.setColour (ml::theme::divider);
        g.drawHorizontalLine (headerArea.getBottom() - 1, 0.0f, (float) baseBounds().getWidth());
    }

    // ---- parameter bar ------------------------------------------------------
    {
        g.setColour (ml::theme::card);
        g.fillRect (paramArea);

        g.setColour (ml::theme::divider);
        g.drawHorizontalLine (paramArea.getBottom() - 1, 0.0f, (float) baseBounds().getWidth());
    }

    // ---- footer -------------------------------------------------------------
    {
        g.setColour (ml::theme::divider);
        g.drawHorizontalLine (footerArea.getY(), 0.0f, (float) baseBounds().getWidth());

        auto f = footerArea.reduced (16, 4);

        g.setColour (ml::theme::textFaint);
        g.setFont (ml::monoFont (ml::tokens::unit));
        auto tech = juce::String (processor.getSampleRateValue() / 1000.0, 1) + " kHz"
                    + ml::sep() + (stats.stereo ? T ("立体声", "stereo") : T ("单声道", "mono"))
                    + ml::sep() + juce::String ("4") + ml::times() + " TP";

        if (selectedPage == (int) ml::PageId::files)
            tech = T ("OFFLINE · 离线分析结果 · 不随播放更新", "OFFLINE · analysis results · not live");
        else if (selectedPage == (int) ml::PageId::album)
            tech = T ("ALBUM · 专辑快照 · 离线记录", "ALBUM · snapshots · offline records");
        else if (stats.hostLength > 0.0)
            tech << ml::sep() << T ("覆盖 ", "cov ") << ml::num (stats.coveragePct) << "%";

        g.drawText (tech, f.getX(), f.getY(), 390, f.getHeight(),
                    juce::Justification::centredLeft);

        // The one global verdict lives in the header. Transient operation
        // messages stay here, in the muted text colour, without a second pill.
        if (footerStatusText.isNotEmpty() && footerStatusText != rail.getVerdictHeadline())
        {
            g.setColour (footerStatusColour.withAlpha (0.9f));
            g.fillEllipse ((float) f.getCentreX() - 54.0f, (float) f.getCentreY() - 3.0f, 6.0f, 6.0f);
            g.setColour (ml::theme::textDim);
            g.setFont (ml::uiFont (ml::tokens::unit));
            g.drawText (footerStatusText, f.getCentreX() - 42, f.getY(), 220, f.getHeight(),
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
