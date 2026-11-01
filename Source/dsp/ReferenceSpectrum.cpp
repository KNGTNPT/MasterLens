#include "ReferenceSpectrum.h"

namespace ml
{
ReferenceSpectrum::ReferenceSpectrum()
    : juce::Thread ("MasterLens reference loader")
{
}

ReferenceSpectrum::~ReferenceSpectrum()
{
    stopThread (4000);
}

void ReferenceSpectrum::load (const juce::StringArray& files, double sampleRate, int fftOrder)
{
    stopThread (4000);

    pendingFiles = files;
    pendingSampleRate = sampleRate > 0.0 ? sampleRate : 48000.0;
    pendingOrder = fftOrder;

    {
        const juce::SpinLock::ScopedLockType sl (lock);
        status = files.isEmpty() ? juce::String ("empty") : juce::String ("loading");
    }

    ready.store (false);
    busy.store (true);

    if (files.isEmpty())
    {
        busy.store (false);
        return;
    }

    startThread();
}

void ReferenceSpectrum::clear()
{
    stopThread (4000);

    {
        const juce::SpinLock::ScopedLockType sl (lock);
        spectrumDb.clear();
        refStats = StatsSnapshot {};
        status = "empty";
    }

    pendingFiles.clear();
    ready.store (false);
    statsReady.store (false);
    busy.store (false);
}

juce::String ReferenceSpectrum::getStatus() const
{
    const juce::SpinLock::ScopedLockType sl (lock);
    return status;
}

juce::StringArray ReferenceSpectrum::getFiles() const
{
    return pendingFiles;
}

std::vector<float> ReferenceSpectrum::getSpectrumDb() const
{
    const juce::SpinLock::ScopedLockType sl (lock);
    return spectrumDb;
}

StatsSnapshot ReferenceSpectrum::getStats() const
{
    const juce::SpinLock::ScopedLockType sl (lock);
    return refStats;
}

void ReferenceSpectrum::run()
{
    const int fftSize = 1 << pendingOrder;
    const int numBins = fftSize / 2 + 1;

    std::vector<double> power ((size_t) numBins, 0.0);
    int totalBlocks = 0;

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    juce::dsp::FFT fft (pendingOrder);
    std::vector<float> window ((size_t) fftSize);

    for (int i = 0; i < fftSize; ++i)
        window[(size_t) i] = 0.5f - 0.5f * (float) std::cos (2.0 * juce::MathConstants<double>::pi
                                                             * (double) i / (double) (fftSize - 1));

    std::vector<float> fftData ((size_t) fftSize * 2, 0.0f);
    std::vector<float> block ((size_t) fftSize, 0.0f);

    AnalysisEngine refEngine;
    Stats refEngineStats;
    resetStats (refEngineStats);
    refEngine.prepare (pendingSampleRate, 2);

    for (const auto& path : pendingFiles)
    {
        if (threadShouldExit())
            return;

        juce::File file (path);

        if (! file.existsAsFile())
            continue;

        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));

        if (reader == nullptr)
            continue;

        const int channels = juce::jlimit (1, 2, (int) reader->numChannels);
        const double sourceRate = reader->sampleRate > 0.0 ? reader->sampleRate : 48000.0;
        const juce::int64 length = reader->lengthInSamples;

        // Everything is brought to the project sample rate before the FFT and
        // loudness pass. This makes 44.1 / 48 / 88.2 / 96 kHz references
        // directly comparable instead of reading 96 kHz samples as if they
        // were 48 kHz (the 1.5.x behaviour).
        juce::AudioFormatReaderSource readerSource (reader.get(), false);
        juce::ResamplingAudioSource resampler (&readerSource, false, channels);
        resampler.setResamplingRatio (sourceRate / pendingSampleRate);
        resampler.prepareToPlay (fftSize, pendingSampleRate);

        juce::AudioBuffer<float> readBuffer (channels, fftSize);
        const juce::int64 outputLength = (juce::int64) std::ceil ((double) length
                                                                  * pendingSampleRate / sourceRate);

        for (juce::int64 start = 0; start < outputLength; start += fftSize)
        {
            if (threadShouldExit())
            {
                resampler.releaseResources();
                return;
            }

            const int n = (int) juce::jmin ((juce::int64) fftSize, outputLength - start);
            readBuffer.clear();

            juce::AudioSourceChannelInfo info;
            info.buffer = &readBuffer;
            info.startSample = 0;
            info.numSamples = n;
            resampler.getNextAudioBlock (info);

            std::fill (block.begin(), block.end(), 0.0f);

            for (int ch = 0; ch < channels; ++ch)
                for (int k = 0; k < n; ++k)
                    block[(size_t) k] += readBuffer.getSample (ch, k);

            const float norm = 1.0f / (float) channels;

            std::fill (fftData.begin(), fftData.end(), 0.0f);

            for (int k = 0; k < n; ++k)
                fftData[(size_t) k] = block[(size_t) k] * norm * window[(size_t) k];

            fft.performFrequencyOnlyForwardTransform (fftData.data(), true);

            for (int b = 1; b < numBins; ++b)
            {
                const double m = (double) fftData[(size_t) b];
                power[(size_t) b] += m * m;
            }

            refEngine.process (readBuffer, refEngineStats, n);
            ++totalBlocks;
        }

        resampler.releaseResources();
    }

    refEngine.finish (refEngineStats);

    if (totalBlocks < 4)
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        status = "failed";
        busy.store (false);
        return;
    }

    std::vector<float> result ((size_t) numBins, kSilentDb);
    const double inv = 1.0 / (double) totalBlocks;

    for (int b = 1; b < numBins; ++b)
    {
        const double mean = power[(size_t) b] * inv;
        result[(size_t) b] = mean > 1.0e-20 ? (float) (10.0 * std::log10 (mean)) : kSilentDb;
    }

    {
        const juce::SpinLock::ScopedLockType sl (lock);
        spectrumDb = std::move (result);
        refStats = ml::snapshot (refEngineStats);
        status = "ready";
        statsReady.store (refStats.hasSignal);
    }

    ready.store (true);
    busy.store (false);
}
} // namespace ml
