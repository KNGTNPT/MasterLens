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
    // 3.1: persistent measurement / monitoring context (what is measured vs what is heard).
    {
        const int ctxX = 372;
        const int labelY = headerArea.getY() + 18;
        const int valueY = labelY + 13;
        g.setColour (ml::theme::textFaint);
        g.setFont (ml::getMicroFont());
        g.drawText (T ("测量", "METERING"), ctxX, labelY, 110, 12,
                    juce::Justification::centredLeft, false);
        g.drawText (T ("监听", "MONITORING"), ctxX + 132, labelY, 110, 12,
                    juce::Justification::centredLeft, false);
        g.setColour (ml::theme::textDim);
        g.setFont (ml::getMonoFont (13.0f));
        const juce::String metering = selectedPage == (int) ml::PageId::files
            ? T ("文件输入", "FILE INPUT") : T ("工程输入", "PROJECT INPUT");
        juce::String monitoring = T ("工程 · 主输出", "PROJECT · MAIN OUT");
        if (processor.referencePlayer.isEnabled())      monitoring = T ("参考", "REFERENCE");
        else if (processor.isAuditionActive())          monitoring = T ("问题片段", "EVIDENCE");
        g.drawText (metering, ctxX, valueY, 120, 16, juce::Justification::centredLeft, false);
        g.drawText (monitoring, ctxX + 132, valueY, 120, 16, juce::Justification::centredLeft, false);
        g.setColour (ml::theme::text.withAlpha (0.12f));
        g.fillRect ((float) (ctxX + 120), (float) labelY - 2.0f, 1.0f, 32.0f);
    }

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

        // Statuses are shown while a non-project source is monitored, and also
        // when they carry a codec result or an error: otherwise a finished codec
        // check (or a failure) disappears behind the technical line as soon as
        // the preview is not running.
        const bool statusVisible = footerStatusText.isNotEmpty()
                                   && (! sourceIsProject || hasCodecPreview
                                       || footerStatusColour == ml::theme::bad);
        const auto leftText = statusVisible ? footerStatusText : tech;
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

//==============================================================================
void MasterLensAudioProcessorEditor::paintOverChildren (juce::Graphics& g)
{
    // oil-ui header squares: sun / info / gear glyphs instead of text labels.
    const auto drawGlyph = [&] (juce::Rectangle<int> button, int which)
    {
        auto box = button.toFloat().withSizeKeepingCentre (18.0f, 18.0f);
        const float s = box.getWidth() / 16.0f;
        const auto c = box.getCentre();
        g.setColour (ml::theme::textDim);
        const float stroke = juce::jmax (1.0f, 1.25f * s);

        if (which == 0) // sun
        {
            g.drawEllipse (c.x - 3.4f * s, c.y - 3.4f * s, 6.8f * s, 6.8f * s, stroke);
            for (int i = 0; i < 8; ++i)
            {
                const float a = juce::MathConstants<float>::twoPi * (float) i / 8.0f;
                const float r0 = 5.2f * s, r1 = 7.0f * s;
                g.drawLine (c.x + std::cos (a) * r0, c.y + std::sin (a) * r0,
                            c.x + std::cos (a) * r1, c.y + std::sin (a) * r1, stroke);
            }
        }
        else if (which == 1) // info
        {
            g.drawEllipse (c.x - 6.2f * s, c.y - 6.2f * s, 12.4f * s, 12.4f * s, stroke);
            g.drawLine (c.x, c.y - 1.6f * s, c.x, c.y + 3.2f * s, stroke);
            g.fillEllipse (c.x - 0.8f * s, c.y - 3.8f * s, 1.6f * s, 1.6f * s);
        }
        else // gear
        {
            g.drawEllipse (c.x - 2.2f * s, c.y - 2.2f * s, 4.4f * s, 4.4f * s, stroke);
            for (int i = 0; i < 8; ++i)
            {
                const float a = juce::MathConstants<float>::twoPi * (float) i / 8.0f;
                const float r0 = 4.6f * s, r1 = 6.6f * s;
                g.drawLine (c.x + std::cos (a) * r0, c.y + std::sin (a) * r0,
                            c.x + std::cos (a) * r1, c.y + std::sin (a) * r1, stroke);
            }
        }
    };

    drawGlyph (themeButton.getBounds(), 0);
    drawGlyph (aboutButton.getBounds(), 1);
    drawGlyph (settingsButton.getBounds(), 2);
}
