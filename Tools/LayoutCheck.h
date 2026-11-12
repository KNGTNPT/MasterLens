#pragma once

#include "PluginEditor.h"
#if JUCE_DEBUG && defined(MASTERLENS_ENABLE_INSPECTOR)
#include <melatonin_inspector/melatonin_inspector.h>
#endif

/** Checks the same logical component bounds exposed in Melatonin Inspector.
    Native JUCE caret/scrollbar plumbing is not authored by the instrument grid.
*/
class MasterLensLayoutCheck
{
public:
    static bool run (MasterLensAudioProcessorEditor& editor)
    {
        int failures = 0, checked = 0;
        const auto check = [&] (bool pass, const char* label)
        {
            if (! pass) { ++failures; std::fprintf (stderr, "INSTRUMENT_FAIL %s\n", label); }
        };
        const auto label = ml::getLabelFont(), value = ml::getValueFont();
        const auto title = ml::getTitleFont(), critical = ml::getCriticalFont();
        check (label.getHeight() == 14.0f && value.getHeight() == 18.0f
                   && title.getHeight() == 20.0f && critical.getHeight() == 30.0f,
               "font tiers must be 14 / 18 / 20 / 30");
        check (value.getHeight() - label.getHeight() >= 4.0f, "label/value separation");
        check (title.getHeight() > value.getHeight() && critical.getHeight() > title.getHeight(),
               "title and critical ordering");

        // The host window (standalone / DAW) drives the editor through this
        // constrainer, so its limits are what the user can actually resize to.
        if (auto* constrainer = editor.getConstrainer())
        {
            check (constrainer->getMinimumWidth() == 1000 && constrainer->getMinimumHeight() == 630,
                   "host minimum editor size 1000x630 (compact tier)");
            check (constrainer->getMaximumWidth() == 2240 && constrainer->getMaximumHeight() == 1400,
                   "host maximum editor size 2240x1400");
        }
        else
        {
            check (false, "editor constrainer missing");
        }

        editor.setBoundsConstrained (juce::Rectangle<int> (0, 0, 800, 500));
        check (editor.getWidth() >= 1000 && editor.getHeight() >= 630,
               "shrinking below the compact canvas is refused");
        editor.setBoundsConstrained (juce::Rectangle<int> (0, 0, 4000, 3000));
        check (editor.getWidth() <= 2240 && editor.getHeight() <= 1400,
               "growing beyond 200% is refused");
        const auto zeros = juce::GlyphArrangement::getStringWidth (value, "000000");
        const auto ones = juce::GlyphArrangement::getStringWidth (value, "111111");
        check (std::abs (zeros - ones) < 0.01f, "tabular numerals");
        check (! banned (label) && ! banned (value) && ! banned (title) && ! banned (critical),
               "font family");
        // Playback rotates around a fixed centre; a stopped/stale stream freezes.
        juce::AudioBuffer<float> block (2, 512); block.clear();
        editor.processor.hostTransportPlaying.store (true);
        editor.processor.outputSpectrumFifo.pushMainOutput (block);
        editor.logoTickMs = juce::Time::getMillisecondCounterHiRes() - 16.0;
        const float beforeAngle = editor.logoAngle;
        editor.updateUiFrame();
        check (editor.logoAngle > beforeAngle, "playing logo rotates");
        editor.processor.hostTransportPlaying.store (false);
        editor.lastAudioMs = juce::Time::getMillisecondCounterHiRes() - 500.0;
        const float stoppedAngle = editor.logoAngle;
        editor.updateUiFrame();
        check (editor.logoAngle == stoppedAngle, "stopped logo freezes");
        juce::TextButton button;
        button.setSize (80, 32);
        button.setLookAndFeel (&editor.lnf);
        const auto sample = [&] (bool hover, bool down)
        {
            juce::Image image (juce::Image::ARGB, 80, 32, true);
            juce::Graphics g (image);
            editor.lnf.drawButtonBackground (g, button, {}, hover, down);
            return image.getPixelAt (40, 16);
        };
        const auto normal = sample (false, false), hover = sample (true, false), down = sample (false, true);
        const auto expectedHover = normal.overlaidWith (juce::Colours::white.withAlpha (0.08f));
        check (std::abs ((int) hover.getRed() - expectedHover.getRed()) <= 1
            && std::abs ((int) hover.getGreen() - expectedHover.getGreen()) <= 1
            && std::abs ((int) hover.getBlue() - expectedHover.getBlue()) <= 1, "hover white overlay");
        check (down.getBrightness() < normal.getBrightness()
            && std::abs (down.getHue() - normal.getHue()) < 0.025f, "pressed brightness/hue");
        button.setLookAndFeel (nullptr);

        for (int language = 0; language < 2; ++language)
        {
            ml::Localisation::set (language == 0 ? ml::Language::chinese : ml::Language::english);
            editor.updateTexts();
            editor.detail.refreshTexts();
            for (const float scale : ml::ui::scaleSteps)
            {
                editor.setUiScale (scale, false);
                for (const auto size : { juce::Point<int> { 896, 560 }, { 1120, 700 }, { 1440, 900 }, { 1123, 703 } })
                {
                    editor.setSize (juce::roundToInt (size.x * scale), juce::roundToInt (size.y * scale));
                    check (std::abs (editor.uiScale - juce::jmin (editor.getWidth() / 1120.0f,
                        editor.getHeight() / 700.0f)) < 0.001f, "continuous resize scale");
                    check (editor.baseBounds() == juce::Rectangle<int> (1120, 700), "stable layout canvas");
                    for (int page = 0; page < ml::numPages; ++page)
                    {
                        editor.setPage (page);
                        editor.runTimerTickForSnapshot();
                        visit (editor, failures, checked);
                        if (page == (int) ml::PageId::spectrum)
                        {
                            const float ratio = (float) editor.spectrumArea.getHeight()
                                / (float) (editor.spectrumArea.getHeight() + editor.readingsArea.getHeight());
                            check (ratio >= 0.5f && ratio <= 0.6f, "spectrum proportion");
                            check (editor.readingsArea.getY() - editor.spectrumArea.getBottom() == 8, "spectrum/readings gap");
                            for (int view = 1; view < 3; ++view)
                            {
                                editor.detail.setSpectrumView (view);
                                visit (editor, failures, checked);
                            }
                            editor.detail.setSpectrumView (0);
                        }
                    }
                }
            }
        }
        editor.setUiScale (1.0f, false);
        editor.setSize (1120, 700);
        editor.settingsPanel.setVisible (true);
        visit (editor, failures, checked);
        editor.hideOverlay (editor.settingsPanel);
        editor.genrePanel.setVisible (true);
        editor.genrePanel.resized();
        visit (editor, failures, checked);
        editor.hideOverlay (editor.genrePanel);
        editor.setPage ((int) ml::PageId::spectrum);
        std::printf ("INSTRUMENT_LAYOUT %s boundaries=%d failures=%d fonts=%s/%s/%s/%s sizes=14/18/20/30\n",
            failures == 0 ? "PASS" : "FAIL", checked, failures,
            label.getTypefaceName().toRawUTF8(), value.getTypefaceName().toRawUTF8(),
            title.getTypefaceName().toRawUTF8(), critical.getTypefaceName().toRawUTF8());
        return failures == 0;
    }

#if JUCE_DEBUG && defined(MASTERLENS_ENABLE_INSPECTOR)
    static void saveInspector (MasterLensAudioProcessorEditor& editor)
    {
        editor.inspector->selectComponent (&editor.ceilingSlider);
        const auto image = editor.inspector->createComponentSnapshot (editor.inspector->getLocalBounds());
        const auto file = juce::File::getCurrentWorkingDirectory().getChildFile ("docs/instrument-inspector.png");
        if (auto stream = file.createOutputStream()) juce::PNGImageFormat().writeImageToStream (image, *stream);
    }
#endif

private:
    static bool banned (const juce::Font& font)
    {
        const auto family = font.getTypefaceName().toLowerCase();
        return family == "inter" || family.startsWith ("roboto") || family.startsWith ("arial")
            || family.startsWith ("helvetica") || family.startsWith ("open sans");
    }

    static void visit (juce::Component& parent, int& failures, int& checked)
    {
        for (auto* c : parent.getChildren())
        {
            if (! c->isVisible()) continue;
            if ((int) c->getProperties()["mlGrid"] == 4)
            {
                ++checked;
                const auto b = c->getBounds();
                const bool aligned = b.getX() % 4 == 0 && b.getY() % 4 == 0
                                  && b.getWidth() % 4 == 0 && b.getHeight() % 4 == 0;
                const auto* editor = dynamic_cast<MasterLensAudioProcessorEditor*> (&parent);
                const auto logicalParent = editor != nullptr ? editor->baseBounds() : parent.getLocalBounds();
                const bool physicalInside = editor == nullptr || parent.getLocalBounds().toFloat().expanded (1.0f)
                    .contains (b.toFloat().transformedBy (c->getTransform()));
                if (! aligned || ! logicalParent.contains (b) || ! physicalInside)
                {
                    ++failures;
                    std::fprintf (stderr, "INSTRUMENT_BOUNDS %s %s parent=%s\n", typeid (*c).name(),
                        b.toString().toRawUTF8(), parent.getLocalBounds().toString().toRawUTF8());
                }
            }
            if (auto* label = dynamic_cast<juce::Label*> (c))
                if (banned (label->getLookAndFeel().getLabelFont (*label))) ++failures;
            if (auto* text = dynamic_cast<juce::TextEditor*> (c))
                if (banned (text->getFont())) ++failures;
            if (dynamic_cast<ml::SpectrumDisplay*> (c) != nullptr || dynamic_cast<ml::SpectrogramDisplay*> (c) != nullptr)
                for (const char* property : { "paddingLeft", "paddingTop", "paddingRight", "paddingBottom" })
                    if ((int) c->getProperties()[property] < 32) ++failures;
            visit (*c, failures, checked);
        }
    }
};
