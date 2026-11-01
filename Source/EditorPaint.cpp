#include "PluginEditor.h"

using ml::T;
#include "Version.h"
#include "ui/Logo.h"

//==============================================================================
void MasterLensAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (ml::theme::background);
    g.addTransform (juce::AffineTransform::scale (uiScale));

    // ---- header: brand, version, reference controls -------------------------
    {
        auto content = headerArea.reduced (20, 10);
        auto logo = content.removeFromLeft (24).toFloat();

        ml::paintLogoMark (g, logo);
        content.removeFromLeft (10);

        const auto nameFont = ml::uiFont (13.0f, true);
        const auto name = juce::String ("MasterLens");
        const int nameW = juce::roundToInt (juce::GlyphArrangement::getStringWidth (nameFont, name));

        g.setColour (ml::theme::text);
        g.setFont (nameFont);
        g.drawText (name, content.getX(), content.getY(), nameW + 4, 28,
                    juce::Justification::centredLeft, false);

        g.setColour (ml::theme::textFaint);
        g.setFont (ml::uiFont (ml::tokens::unit));
        g.drawText ("v" + juce::String (ml::kVersion), content.getX() + nameW + 8,
                    content.getY(), 70, 28, juce::Justification::centredLeft, false);

        ml::surface::hairlineH (g, 0.0f, (float) baseBounds().getWidth(),
                                (float) headerArea.getBottom() - 1.0f);
    }

    // ---- footer: technical line, one output state, brand, actions -----------
    {
        ml::surface::hairlineH (g, 0.0f, (float) baseBounds().getWidth(),
                                (float) footerArea.getY());

        auto f = footerArea.reduced (16, 4);

        g.setColour (ml::theme::textFaint);
        g.setFont (ml::monoFont (ml::tokens::unit));
        auto tech = juce::String (processor.getSampleRateValue() / 1000.0, 1) + " kHz"
                    + "  ·  " + (stats.stereo ? T ("stereo", "stereo") : T ("mono", "mono"))
                    + "  ·  4" + ml::times() + " TP";

        if (selectedPage == (int) ml::PageId::files)
            tech = T ("OFFLINE · file QC", "OFFLINE · file QC");
        else if (selectedPage == (int) ml::PageId::album)
            tech = T ("ALBUM · snapshot", "ALBUM · snapshot");
        else if (stats.hostLength > 0.0)
            tech << "  ·  " << T ("cov ", "cov ") << ml::num (stats.coveragePct) << "%";

        g.drawText (tech, f.getX(), f.getY(), 300, f.getHeight(),
                    juce::Justification::centredLeft, false);

        // One output state, centred in the 32 px strip.
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

        const auto outFont = ml::monoFont (ml::tokens::unit);
        const int outW = juce::jmin (160, juce::roundToInt (
            juce::GlyphArrangement::getStringWidth (outFont, outState)) + 18);
        auto outArea = juce::Rectangle<int> (f.getCentreX() - outW / 2, f.getY(), outW, f.getHeight());

        g.setColour (outColour);
        g.fillEllipse ((float) outArea.getX(), (float) outArea.getCentreY() - 2.5f, 5.0f, 5.0f);
        g.setFont (outFont);
        g.drawText (outState, outArea.withTrimmedLeft (12), juce::Justification::centredLeft, false);

        // Fixed brand lockup: 11 px, +4% tracking, weakest tone.
        auto brand = juce::Rectangle<int> (f.getX() + 318, f.getY(), 104, f.getHeight());
        ml::paintBerryWordmark (g, brand, ml::theme::textFaint, 11.0f,
                                juce::Justification::centredLeft);
    }
}
