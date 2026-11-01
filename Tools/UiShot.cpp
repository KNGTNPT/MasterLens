#include <juce_gui_extra/juce_gui_extra.h>

#include <cstdlib>
#include <ctime>

#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "Version.h"
#include "ui/Logo.h"

/** Offscreen renderer used to verify the editor layout without a screen recorder.

    Feeds a synthetic music-like signal through the analysis engine, lets the
    60 fps editor timer run for a couple of seconds, then writes a PNG snapshot.
*/
class UiShotApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "MasterLensUiShot"; }
    const juce::String getApplicationVersion() override { return "1.0"; }
    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise (const juce::String& commandLine) override
    {
        if (commandLine.containsIgnoreCase ("logo"))
        {
            renderLogo();
            quit();
            return;
        }

        english = commandLine.containsIgnoreCase ("en");
        aboutShot = commandLine.containsIgnoreCase ("about");
        expandedShot = commandLine.containsIgnoreCase ("expand");
        smallShot = commandLine.containsIgnoreCase ("small");
        minShot = commandLine.containsIgnoreCase ("minsize");
        lightShot = commandLine.containsIgnoreCase ("light");
        emptyShot = commandLine.containsIgnoreCase ("empty");
        snapShot = commandLine.containsIgnoreCase ("snap");
        genreShot = commandLine.containsIgnoreCase ("info");
        settingsShot = commandLine.containsIgnoreCase ("settings");
        fileQcShot = commandLine.containsIgnoreCase ("fileqc");
        benchMode = commandLine.containsIgnoreCase ("bench");

        const int outTag = commandLine.indexOfIgnoreCase ("out=");

        if (outTag >= 0)
            outSuffix = commandLine.substring (outTag + 4).upToFirstOccurrenceOf (" ", false, false);

        float scale = 1.0f;

        if (commandLine.containsIgnoreCase ("scale200")) scale = 2.0f;
        else if (commandLine.containsIgnoreCase ("scale175")) scale = 1.75f;
        else if (commandLine.containsIgnoreCase ("scale150")) scale = 1.5f;
        else if (commandLine.containsIgnoreCase ("scale125")) scale = 1.25f;

        for (int p = 0; p < ml::numPages; ++p)
            if (commandLine.containsIgnoreCase ("page" + juce::String (p)))
                pageShot = p;

        setenv ("MASTERLENS_ALBUM_FILE", "/tmp/masterlens_ui_album.json", 1);
        processor = std::make_unique<MasterLensAudioProcessor>();
        processor->setRateAndBufferSizeDetails (48000.0, 512);
        processor->prepareToPlay (48000.0, 512);

        if (! emptyShot)
            feedTestAudio (200.0);

        processor->apvts.state.setProperty ("language", english ? 1 : 0, nullptr);
        processor->apvts.state.setProperty ("appearance", lightShot ? 0 : 1, nullptr);
        processor->apvts.state.setProperty ("uiScale", scale, nullptr);

        editor = std::make_unique<MasterLensAudioProcessorEditor> (*processor);
        editor->setSize (juce::roundToInt ((minShot ? 880.0f : (smallShot ? 900.0f : 1280.0f)) * scale),
                         juce::roundToInt ((minShot ? 560.0f : (smallShot ? 620.0f : 820.0f)) * scale));

        std::printf ("UiShot %s page=%d scale=%.2f size=%dx%d\n", ml::kVersion, pageShot,
                     scale, editor->getWidth(), editor->getHeight());

        if (pageShot >= 0)
            editor->setPageForSnapshot (pageShot);

        if (fileQcShot)
            runFileQcDemo (*editor);

        if (commandLine.containsIgnoreCase ("spectrumview1"))
        {
            editor->setSpectrumViewForSnapshot (1);

            for (int i = 0; i < 800; ++i)
                editor->runTimerTickForSnapshot();
        }
        else if (commandLine.containsIgnoreCase ("spectrumview2"))
        {
            editor->setSpectrumViewForSnapshot (2);

            for (int i = 0; i < 800; ++i)
                editor->runTimerTickForSnapshot();
        }

        if (commandLine.containsIgnoreCase ("pdf"))
            editor->writeReportPdfForSnapshot (juce::File ("/tmp/masterlens-ui.pdf"));

        if (commandLine.containsIgnoreCase ("albumcap"))
        {
            editor->runTimerTickForSnapshot();
            editor->runTimerTickForSnapshot();
            editor->captureAlbumAsForSnapshot ("Track A");
            editor->captureAlbumAsForSnapshot ("Track B");
            editor->runTimerTickForSnapshot();

            if (commandLine.containsIgnoreCase ("albumproof"))
                juce::File ("/tmp/masterlens_ui_proof.json")
                    .replaceWithText (ml::album::Session::get().exportProofJson());
        }

        if (aboutShot)
            editor->setAboutVisible (true);

        if (settingsShot)
            editor->setSettingsVisible (true);

        for (int p = 0; p < 40; ++p)
            if (commandLine.contains ("preset" + juce::String (p)))
                editor->setPresetForSnapshot (p);

        if (genreShot)
            editor->showGenreInfoForSnapshot();

        if (benchMode)
        {
            benchCpuStart = std::clock();
            benchWallStart = juce::Time::getMillisecondCounterHiRes();

            juce::Timer::callAfterDelay (5000, [this]
            {
                const double wall = (juce::Time::getMillisecondCounterHiRes() - benchWallStart) / 1000.0;
                const double cpu = ((double) (std::clock() - benchCpuStart)) / (double) CLOCKS_PER_SEC;

                std::printf ("UI_BENCH page=%d cpu=%.3f s wall=%.3f s load=%.2f%%\n",
                             pageShot, cpu, wall, wall > 0.0 ? 100.0 * cpu / wall : 0.0);
                quit();
            });
        }
        else
        {
            juce::Timer::callAfterDelay (2500, [this] { saveAndQuit(); });
        }
    }

    void shutdown() override
    {
        editor.reset();
        processor.reset();
    }

    void systemRequestedQuit() override { quit(); }

private:
    void runFileQcDemo (MasterLensAudioProcessorEditor& target)
    {
        const auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                .getChildFile ("MasterLens-FileQC-Demo");
        folder.createDirectory();
        folder.deleteRecursively();
        folder.createDirectory();

        juce::StringArray paths;
        const double rates[] { 44100.0, 48000.0 };
        const char* names[] { "01-first-track.wav", "02-second-track.wav" };

        for (int f = 0; f < 2; ++f)
        {
            const auto file = folder.getChildFile (names[f]);
            const int n = (int) (rates[f] * 4.0);
            juce::AudioBuffer<float> buffer (2, n);

            for (int i = 0; i < n; ++i)
            {
                const double t = (double) i / rates[f];
                const float v = (float) (0.085 * std::sin (2.0 * juce::MathConstants<double>::pi * 110.0 * t)
                                         + 0.025 * std::sin (2.0 * juce::MathConstants<double>::pi * 997.0 * t));
                buffer.setSample (0, i, v);
                buffer.setSample (1, i, v * 0.92f);
            }

            if (auto out = file.createOutputStream())
            {
                juce::WavAudioFormat wav;
                std::unique_ptr<juce::AudioFormatWriter> writer (
                    wav.createWriterFor (out.release(), rates[f], 2, 24, {}, 0));

                if (writer != nullptr)
                {
                    writer->writeFromAudioSampleBuffer (buffer, 0, n);
                    writer.reset();
                }
            }

            paths.add (file.getFullPathName());
        }

        target.filesDropped (paths, 0, 0);

        for (int i = 0; i < 240; ++i)
        {
            target.runTimerTickForSnapshot();
            juce::Thread::sleep (20);
        }

        target.setPageForSnapshot (10);
        target.runTimerTickForSnapshot();
    }

    void feedTestAudio (double seconds)
    {
        constexpr int blockSize = 512;
        constexpr double sr = 48000.0;
        constexpr double twoPi = juce::MathConstants<double>::twoPi;

        juce::AudioBuffer<float> buffer (2, blockSize);
        juce::MidiBuffer midi;
        juce::Random rng (20240607);

        const int totalBlocks = (int) (seconds * sr / blockSize);

        float pl0 = 0.0f, pl1 = 0.0f, pl2 = 0.0f;
        float lpL = 0.0f, lpR = 0.0f;
        double sampleIndex = 0.0;

        for (int b = 0; b < totalBlocks; ++b)
        {
            for (int n = 0; n < blockSize; ++n)
            {
                const double t = (sampleIndex + n) / sr;

                // kick every 0.5 s
                const double beat = std::fmod (t, 0.5);
                const float kickEnv = (float) std::exp (-beat * 18.0);
                const float kick = 0.34f * kickEnv * (float) std::sin (twoPi * 55.0 * beat);

                // bass + chord bed
                const float bass = 0.12f * (float) std::sin (twoPi * 110.0 * t);
                const float chord = 0.05f * (float) (std::sin (twoPi * 440.0 * t)
                                                    + std::sin (twoPi * 554.37 * t)
                                                    + std::sin (twoPi * 659.25 * t));

                // hats
                const double hatBeat = std::fmod (t, 0.25);
                const float hatEnv = (float) std::exp (-hatBeat * 45.0);
                const float hat = 0.05f * hatEnv * (rng.nextFloat() * 2.0f - 1.0f);

                // shared pink noise bed (mono low end) + high-passed side signal
                const float n0 = rng.nextFloat() * 2.0f - 1.0f;
                pl0 = 0.99765f * pl0 + n0 * 0.0990460f;
                pl1 = 0.96300f * pl1 + n0 * 0.2965164f;
                pl2 = 0.57000f * pl2 + n0 * 1.0526913f;
                const float pinkCommon = (pl0 + pl1 + pl2 + n0 * 0.1848f) * 0.050f;

                const float nl = rng.nextFloat() * 2.0f - 1.0f;
                const float nr = rng.nextFloat() * 2.0f - 1.0f;
                lpL += 0.020f * (nl - lpL);
                lpR += 0.020f * (nr - lpR);
                const float sideL = (nl - lpL) * 0.105f;
                const float sideR = (nr - lpR) * 0.105f;

                // 24 s musical form so the history graph shows real dynamics
                const double section = std::fmod (t, 24.0);
                const float sectionGain = section < 12.0 ? 0.62f : 1.0f;
                const float swell = 0.88f + 0.12f * (float) std::sin (twoPi * t / 7.0);

                const float centre = (kick + bass + chord * 0.85f + hat * 0.7f) * sectionGain * swell;

                buffer.setSample (0, n, juce::jlimit (-1.0f, 1.0f, centre + (pinkCommon + sideL) * sectionGain));
                buffer.setSample (1, n, juce::jlimit (-1.0f, 1.0f, centre + (pinkCommon + sideR) * sectionGain));
            }

            processor->processBlock (buffer, midi);
            sampleIndex += blockSize;
        }
    }

    void renderLogo()
    {
        const auto write = [this] (int size, const juce::String& relative)
        {
            juce::Image image (juce::Image::ARGB, size, size, true);
            juce::Graphics g (image);

            ml::paintLogoMark (g, juce::Rectangle<float> (0.0f, 0.0f, (float) size, (float) size));

            const auto file = juce::File::getCurrentWorkingDirectory().getChildFile (relative);
            file.getParentDirectory().createDirectory();
            file.deleteFile();

            if (auto stream = std::unique_ptr<juce::FileOutputStream> (file.createOutputStream()))
            {
                juce::PNGImageFormat png;
                png.writeImageToStream (image, *stream);
                std::printf ("wrote %s (%d px)\n", relative.toRawUTF8(), size);
            }
        };

        write (1024, "packaging/icon.png");
        write (768, "docs/logo.png");
    }

    void saveAndQuit()
    {
        if (expandedShot)
            editor->expandFirstIssueForSnapshot();

        if (snapShot)
            editor->captureSnapshotForSnapshot (0);

        std::vector<float> curve;
        processor->copyShortTermCurve (curve, 1800);

        int valid = 0;
        float minV = 0.0f, maxV = -200.0f;

        for (auto v : curve)
        {
            if (v > -99.0f)
            {
                ++valid;
                minV = juce::jmin (minV == 0.0f ? v : minV, v);
                maxV = juce::jmax (maxV, v);
            }
        }

        std::printf ("curve points=%d valid=%d min=%.2f max=%.2f last=%.2f\n",
                     (int) curve.size(), valid, minV, maxV,
                     curve.empty() ? 0.0f : curve.back());

        const auto s = processor->getStatsSnapshot();
        std::printf ("integrated=%.2f  tp=%.2f  lra=%.2f  corr=%.2f  width=%.1f  lowside=%.1f\n",
                     s.integrated, s.truePeakDb, s.lra, s.correlation, s.widthPct, s.lowSidePct);

        const auto res = editor->getWidth() > 1600 ? 1.0f : 2.0f;
        const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, res);

        juce::String name = "ui_shot";

        if (english)      name << "_en";
        if (pageShot >= 0) name << "_page" << pageShot;
        if (expandedShot) name << "_expanded";
        if (aboutShot)    name << "_about";
        if (smallShot)    name << "_small";
        if (minShot)      name << "_min";
        if (lightShot)    name << "_light";
        if (emptyShot)    name << "_empty";
        if (snapShot)     name << "_snap";
        if (genreShot)    name << "_genre";
        if (settingsShot) name << "_settings";
        if (outSuffix.isNotEmpty()) name << "_" << outSuffix;

        const auto output = juce::File::getCurrentWorkingDirectory().getChildFile (name + ".png");

        juce::PNGImageFormat png;

        {
            juce::File copy (juce::File ("/tmp").getChildFile ("uishot_" + name + ".png"));
            copy.deleteFile();

            if (auto s = std::unique_ptr<juce::FileOutputStream> (copy.createOutputStream()))
                png.writeImageToStream (image, *s);
        }

        output.deleteFile();

        if (auto stream = std::unique_ptr<juce::FileOutputStream> (output.createOutputStream()))
            png.writeImageToStream (image, *stream);

        quit();
    }

    std::unique_ptr<MasterLensAudioProcessor> processor;
    std::unique_ptr<MasterLensAudioProcessorEditor> editor;
    bool english = false;
    bool aboutShot = false, genreShot = false;
    bool expandedShot = false;
    bool smallShot = false;
    bool minShot = false;
    bool lightShot = false;
    bool emptyShot = false;
    bool snapShot = false;
    bool settingsShot = false;
    bool fileQcShot = false;
    bool benchMode = false;
    std::clock_t benchCpuStart = 0;
    double benchWallStart = 0.0;
    juce::String outSuffix;
    int pageShot = -1;
};

START_JUCE_APPLICATION (UiShotApplication)
