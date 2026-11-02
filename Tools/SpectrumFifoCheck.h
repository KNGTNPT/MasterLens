#pragma once

#if JUCE_DEBUG

#include "PluginEditor.h"

#include <chrono>
#include <cstdio>
#include <functional>

/** Debug acceptance probe: real processBlock producer thread and the editor's
    actual message-thread timer. It observes the editor cache, never becomes a
    second FIFO consumer. Run MasterLensUiShot spectrumfifo.
*/
class SpectrumFifoCheck : private juce::Timer, private juce::Thread
{
public:
    static bool preflight()
    {
        using Fifo = ml::SpectrumFifo;
        auto p = std::make_unique<MasterLensAudioProcessor>();
        p->setRateAndBufferSizeDetails (48000.0, 512);
        p->prepareToPlay (48000.0, 512);
        Fifo::Reader reader;
        juce::MidiBuffer midi;
        juce::AudioBuffer<float> block (2, Fifo::fftSize);

        // Check the tap is AFTER output trim, while measurement sees the input.
        p->apvts.getParameter ("monitorMatch")->setValueNotifyingHost (1.0f);
        p->stats.integrated.store (-20.0f);
        p->setMonitorTargetLufs (-14.0f);
        const auto gain = juce::Decibels::decibelsToGain (6.0f);
        for (int i = 0; i < block.getNumSamples(); ++i)
        {
            block.setSample (0, i, 0.1f);
            block.setSample (1, i, 0.3f);
        }
        p->processBlock (block, midi);
        bool ok = reader.update (p->outputSpectrumFifo);
        for (int i = 0; i < Fifo::fftSize; ++i)
            ok = ok && std::abs (reader.getFrame().samples[(size_t) i] - 0.2f * gain) < 1.0e-6f;
        ok = ok && p->getStatsSnapshot().samplePeakDb < -10.0f;

        // Producer restarts must reject frames from a previous sample-rate epoch.
        p->prepareToPlay (96000.0, 512);
        block.clear();
        p->outputSpectrumFifo.pushMainOutput (block); // leave an old frame queued
        p->prepareToPlay (44100.0, 512);
        ok = ! reader.update (p->outputSpectrumFifo) && ! reader.hasFrame() && ok;
        p->apvts.getParameter ("monitorMatch")->setValueNotifyingHost (0.0f);

        // Oversized mono host blocks must wrap safely; a full queue must not
        // block or overwrite a pending frame. New frames resume after a drain.
        juce::AudioBuffer<float> mono (1, Fifo::fftSize + 8 * Fifo::hopSize);
        for (int i = 0; i < mono.getNumSamples(); ++i)
            mono.setSample (0, i, (float) i / (float) mono.getNumSamples());
        p->processBlock (mono, midi);
        ok = reader.update (p->outputSpectrumFifo) && ok;
        ok = ok && p->outputSpectrumFifo.getDroppedFrames() == 5;
        const auto& raw = reader.getFrame();
        ok = ok && raw.endSample == Fifo::fftSize + 3 * Fifo::hopSize && raw.sampleRate == 44100.0;
        for (int i = 0; i < Fifo::fftSize; ++i)
            ok = ok && raw.samples[(size_t) i] == mono.getSample (0, 3 * Fifo::hopSize + i);

        juce::AudioBuffer<float> antiPhase (2, Fifo::fftSize);
        for (int i = 0; i < Fifo::fftSize; ++i)
        {
            antiPhase.setSample (0, i, 0.25f);
            antiPhase.setSample (1, i, -0.25f);
        }
        p->processBlock (antiPhase, midi);
        ok = reader.update (p->outputSpectrumFifo) && ok;
        for (const auto v : reader.getFrame().samples)
            ok = ok && v == 0.0f;

        std::printf ("SPECTRUM_FIFO_PREFLIGHT output_trim/measurement_input/mono/wrap/overflow/restart/antiphase=%s\n",
                     ok ? "PASS" : "FAIL");
        return ok;
    }

    SpectrumFifoCheck (MasterLensAudioProcessor& p, MasterLensAudioProcessorEditor& e,
                       std::function<void(bool)> done)
        : juce::Thread ("Spectrum FIFO acceptance audio"), processor (p), editor (e), finished (std::move (done))
    {
        started = juce::Time::getMillisecondCounterHiRes();
        startThread();
        startTimerHz (60);
    }

    ~SpectrumFifoCheck() override { stopTimer(); stopThread (2000); }

private:
    static float signal (std::uint64_t sample, int channel)
    {
        // Change frequency after one second: a stale-but-plausible frame fails.
        const double hz = sample < 48000 ? 997.0 : 1501.0;
        return (channel == 0 ? 0.25f : 0.125f)
             * (float) std::sin (juce::MathConstants<double>::twoPi * hz * (double) sample / 48000.0
                                + (channel == 0 ? 0.0 : 0.4));
    }

    void run() override
    {
        const int sizes[] { 127, 511, 64, 1024 };
        juce::AudioBuffer<float> block (2, 1024);
        juce::MidiBuffer midi;
        const auto clockStart = std::chrono::steady_clock::now();
        std::uint64_t position = 0;
        int index = 0;
        while (! threadShouldExit())
        {
            const int n = sizes[index++ % 4];
            block.setSize (2, n, false, false, true);
            for (int i = 0; i < n; ++i)
                for (int ch = 0; ch < 2; ++ch)
                    block.setSample (ch, i, signal (position + (std::uint64_t) i, ch));
            processor.processBlock (block, midi);
            for (int i = 0; i < n; ++i)
                for (int ch = 0; ch < 2; ++ch)
                    if (block.getSample (ch, i) != signal (position + (std::uint64_t) i, ch))
                        transparent.store (false);
            position += (std::uint64_t) n;
            const auto deadline = clockStart + std::chrono::microseconds ((long long) (position * 1000000 / 48000));
            const auto left = std::chrono::duration_cast<std::chrono::milliseconds> (
                deadline - std::chrono::steady_clock::now()).count();
            if (left > 0)
                wait ((int) left);
        }
    }

    void inspectFrame()
    {
        const auto& reader = editor.getOutputSpectrumForDebug();
        if (! reader.hasFrame() || reader.getFrame().endSample == lastEnd)
            return;
        const auto& frame = reader.getFrame();
        ok = ok && frame.endSample > lastEnd && frame.sampleRate == 48000.0;
        lastEnd = frame.endSample;
        ++received;
        const auto produced = processor.outputSpectrumFifo.getProducedSamples();
        ok = ok && produced >= frame.endSample;
        maxLag = juce::jmax (maxLag, produced - frame.endSample);

        for (size_t i = 0; i < frame.samples.size(); ++i)
        {
            const auto t = frame.endSample - ml::SpectrumFifo::fftSize + i;
            const auto expected = 0.5f * (signal (t, 0) + signal (t, 1));
            maxRawError = juce::jmax (maxRawError, std::abs (frame.samples[i] - expected));
            const double phase = juce::MathConstants<double>::twoPi * (double) i
                               / (double) (ml::SpectrumFifo::fftSize - 1);
            const auto blackman = (float) (0.42 - 0.5 * std::cos (phase) + 0.08 * std::cos (2.0 * phase));
            maxWindowError = juce::jmax (maxWindowError,
                std::abs (reader.getWindowedSamples()[i] - expected * blackman));
        }
    }

    void timerCallback() override
    {
        inspectFrame();
        if (juce::Time::getMillisecondCounterHiRes() - started < 2200.0)
            return;

        stopTimer();
        stopThread (2000);
        editor.runTimerTickForSnapshot();
        inspectFrame();
        ok = ok && received >= 20 && lastEnd > 96000 && maxLag <= 8192
                && maxRawError < 1.0e-6f && maxWindowError < 1.0e-6f
                && transparent.load() && processor.getLatencySamples() == 0
                && processor.outputSpectrumFifo.getDroppedFrames() == 0;
        std::printf ("SPECTRUM_FIFO_UI %s frames=%d end_sample=%llu max_lag=%.2fms raw_error=%.3g blackman_error=%.3g bypass=%s\n",
                     ok ? "PASS" : "FAIL", received, (unsigned long long) lastEnd,
                     (double) maxLag / 48.0, maxRawError, maxWindowError,
                     transparent.load() ? "PASS" : "FAIL");
        finished (ok);
    }

    MasterLensAudioProcessor& processor;
    MasterLensAudioProcessorEditor& editor;
    std::function<void(bool)> finished;
    std::atomic<bool> transparent { true };
    double started = 0.0;
    std::uint64_t lastEnd = 0, maxLag = 0;
    int received = 0;
    float maxRawError = 0.0f, maxWindowError = 0.0f;
    bool ok = true;
};
#endif
