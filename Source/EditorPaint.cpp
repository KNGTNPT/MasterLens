#include "PluginEditor.h"

using ml::T;
#include "Version.h"
#include "ui/Logo.h"

//==============================================================================
void MasterLensAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (ml::theme::background);
    g.addTransform (juce::AffineTransform::scale (uiScale));

    const float width = (float) baseBounds().getWidth();
    g.setColour (ml::theme::background);
    g.fillRect (headerArea);
    auto brand = headerArea.reduced (24, 8);
    // A 44 px grid slot holds the exact 32 * 1.3 px mark, centred vertically.
    ml::paintLogoMark (g, brand.removeFromLeft (44).toFloat()
        .withSizeKeepingCentre (32.0f * 1.3f, 32.0f * 1.3f), false, logoAngle);
    brand.removeFromLeft (16);
    const auto& wordmark = ml::getMasterWordmark();
    const auto& byline = ml::getBrandByline (ml::Localisation::isEnglish());
    const auto titleInk = wordmark.getBoundingBox (0, -1, true);
    const auto bylineInk = byline.getBoundingBox (0, -1, true);
    constexpr float brandScale = 1.15f;
    // Wordmark is 15 % vertically compressed, with 30 % wider tracking, and
    // 20 % less gap before the byline. The mark itself stays square.
    constexpr float titleSquash = 0.85f;
    constexpr float lineGap = 6.4f;
    const float titleHeight = titleInk.getHeight() * brandScale * titleSquash;
    const float totalHeight = titleHeight + lineGap + bylineInk.getHeight();
    const float top = (float) headerArea.getCentreY() - totalHeight * 0.5f;
    g.setColour (ml::theme::textDim);
    wordmark.draw (g, juce::AffineTransform::scale (brandScale, brandScale * titleSquash).translated (
        (float) brand.getX() - titleInk.getX() * brandScale,
        top - titleInk.getY() * brandScale * titleSquash));
    g.setColour (ml::theme::textFaint);
    byline.draw (g, juce::AffineTransform::translation (
        (float) brand.getX() - bylineInk.getX(),
        top + titleHeight + lineGap - bylineInk.getY()));
    ml::surface::hairlineH (g, 16.0f, width - 16.0f, 112.0f);

    g.setColour (ml::theme::card);
    g.fillRect (paramArea);
    ml::surface::hairlineH (g, 0.0f, width, (float) paramArea.getY(),
                           ml::theme::text.withAlpha (0.12f));
    g.setColour (ml::theme::textFaint);
    g.setFont (ml::labelFontFor (T ("比较基准", "COMPARISON BASIS")));
    g.drawText (T ("比较基准", "COMPARISON BASIS"), 24, paramArea.getY() + 12, 190, 16,
                juce::Justification::centredLeft, false);

    // ---- footer: technical line, one output state, brand, actions -----------
    {
        ml::surface::hairlineH (g, 0.0f, (float) baseBounds().getWidth(),
                                (float) footerArea.getY());

        auto f = footerArea.reduced (16, 4);

        g.setColour (ml::theme::textFaint);
        g.setFont (ml::getLabelFont());
        auto tech = juce::String (processor.getSampleRateValue() / 1000.0, 1) + " kHz"
                    + "  ·  " + (stats.stereo ? T ("stereo", "stereo") : T ("mono", "mono"))
                    + "  ·  4" + ml::times() + " TP";

        if (selectedPage == (int) ml::PageId::files)
            tech = T ("离线 · 文件 QC", "OFFLINE · file QC");
        else if (selectedPage == (int) ml::PageId::album)
            tech = T ("专辑 · 快照", "ALBUM · snapshot");
        else if (stats.hostLength > 0.0)
            tech << "  ·  " << T ("cov ", "cov ") << ml::num (stats.coveragePct) << "%";

        // Listening source is separate from output state: BYPASS still means
        // the project bus, and a reference / evidence preview must remain
        // visible on every page.
        juce::String source = T ("工程", "PROJECT");
        auto sourceColour = ml::theme::textDim;
        bool sourceIsProject = true;

        if (processor.referencePlayer.isEnabled())
        {
            source = T ("参考", "REFERENCE");
            sourceColour = ml::theme::warn;
            sourceIsProject = false;
        }
        else if (processor.isAuditionActive())
        {
            source = T ("问题片段", "EVIDENCE");
            sourceColour = ml::theme::accent;
            sourceIsProject = false;
        }
        else if (processor.isStreamingPreviewActive() || processor.isCodecPreviewActive())
        {
            source = T ("文件", "FILE");
            sourceColour = ml::theme::cyan;
            sourceIsProject = false;
        }
        else if (processor.isFilePreviewActive() && processor.filePreviewFeedsAnalysis())
        {
            source = T ("文件", "FILE");
            sourceColour = ml::theme::cyan;
            sourceIsProject = false;
        }
        else if (processor.isFilePreviewActive())
        {
            source = T ("问题片段", "EVIDENCE");
            sourceColour = ml::theme::accent;
            sourceIsProject = false;
        }

        const auto leftText = (! sourceIsProject && footerStatusText.isNotEmpty()) ? footerStatusText : tech;
        g.setColour (ml::theme::textFaint);
        g.setFont (ml::getLabelFont());
        g.drawFittedText (leftText, f.getX(), f.getY(), 300, f.getHeight(),
                          juce::Justification::centredLeft, 1);

        // One output state and one listening source, both in the 32 px strip.
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
        else if (processor.isStreamingPreviewActive() || processor.isCodecPreviewActive()
                 || processor.isFilePreviewActive())
        {
            outState = T ("FILE", "FILE");
            outColour = ml::theme::cyan;
        }
        else if (processor.isMonitorMatch())
        {
            outState = T ("MATCH ", "MATCH ") + ml::sgn (processor.getMonitorTrimDb(), 1) + " dB";
            outColour = ml::theme::accent;
        }

        const auto outFont = ml::getLabelFont();
        const int outW = juce::jmin (160, juce::roundToInt (
            juce::GlyphArrangement::getStringWidth (outFont, outState)) + 18);
        auto outArea = juce::Rectangle<int> (f.getRight() - outW, f.getY(), outW, f.getHeight());

        const int sourceW = 128;
        auto sourceArea = outArea.withX (outArea.getX() - sourceW - 12).withWidth (sourceW);
        g.setColour (sourceColour.withAlpha (0.12f));
        g.fillRoundedRectangle (sourceArea.toFloat(), 4.0f);
        g.setColour (sourceColour);
        g.setFont (outFont);
        g.drawFittedText (T ("监听 · ", "LISTEN · ") + source,
                          sourceArea.reduced (8, 0), juce::Justification::centredRight, 1);

        g.setColour (outColour);
        g.fillEllipse ((float) outArea.getX(), (float) outArea.getCentreY() - 2.5f, 5.0f, 5.0f);
        g.setFont (outFont);
        g.drawText (outState, outArea.withTrimmedLeft (12), juce::Justification::centredLeft, false);

        // Fixed brand lockup: 11 px, +4% tracking, weakest tone.
        auto brand = juce::Rectangle<int> (f.getCentreX() - 52, f.getY(), 104, f.getHeight());
        ml::paintBerryWordmark (g, brand, ml::theme::textFaint, 11.0f,
                                juce::Justification::centredLeft);
    }
}
