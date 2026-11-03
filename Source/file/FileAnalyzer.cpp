#include "FileAnalyzer.h"

#include <algorithm>
#include <cmath>

#include <juce_dsp/juce_dsp.h>
#include <juce_cryptography/juce_cryptography.h>

#include "../dsp/SpectrumAnalyzer.h"

namespace ml
{
namespace
{
    class CancellableSource : public juce::FileInputStream
    {
    public:
        CancellableSource (const juce::File& f, std::function<bool()> exit)
            : juce::FileInputStream (f), cancelled (std::move (exit)) {}
        int read (void* dest, int count) override
        {
            return cancelled() ? 0 : juce::FileInputStream::read (dest, count);
        }
    private:
        std::function<bool()> cancelled;
    };
    constexpr int fftOrder = 13;
    constexpr int fftSize = 1 << fftOrder;
    constexpr int fftHop = fftSize / 2;

    juce::String extensionList()
    {
        return "*.wav;*.aif;*.aiff;*.flac";
    }
} // namespace

FileAnalyzer::FileAnalyzer()
    : juce::Thread ("MasterLens file analyser")
{
}

FileAnalyzer::~FileAnalyzer()
{
    signalThreadShouldExit();
    stopThread (4000);
}

bool FileAnalyzer::isSupportedFile (const juce::File& file)
{
    const auto ext = file.getFileExtension().toLowerCase();
    return ext == ".wav" || ext == ".aif" || ext == ".aiff" || ext == ".flac";
}

juce::StringArray FileAnalyzer::expandPaths (const juce::StringArray& paths)
{
    std::vector<juce::File> files;

    for (const auto& path : paths)
    {
        const juce::File file (path);

        if (file.isDirectory())
        {
            juce::Array<juce::File> found;
            file.findChildFiles (found, juce::File::findFiles, true, extensionList());

            for (const auto& f : found)
                if (isSupportedFile (f))
                    files.push_back (f);
        }
        else if (file.existsAsFile() && isSupportedFile (file))
        {
            files.push_back (file);
        }
    }

    std::sort (files.begin(), files.end(), [] (const juce::File& a, const juce::File& b)
    {
        return a.getFileName().compareNatural (b.getFileName()) < 0;
    });

    juce::StringArray result;

    for (const auto& f : files)
        result.addIfNotAlreadyThere (f.getFullPathName());

    return result;
}

bool FileAnalyzer::analyse (const juce::StringArray& paths)
{
    if (busy.load (std::memory_order_acquire))
        return false;

    auto expanded = expandPaths (paths);

    if (expanded.isEmpty())
        return false;

    signalThreadShouldExit();
    stopThread (4000);

    {
        const juce::SpinLock::ScopedLockType sl (lock);
        pending = expanded;
        results.clear();
        status = "queued";
        current.clear();
    }

    total.store (expanded.size(), std::memory_order_release);
    done.store (0, std::memory_order_release);
    busy.store (true, std::memory_order_release);
    startThread();
    return true;
}

void FileAnalyzer::cancel()
{
    signalThreadShouldExit();
}

juce::String FileAnalyzer::getStatus() const
{
    const juce::SpinLock::ScopedLockType sl (lock);
    return status;
}

juce::String FileAnalyzer::getCurrentFile() const
{
    const juce::SpinLock::ScopedLockType sl (lock);
    return current;
}

std::vector<FileAnalysisResult> FileAnalyzer::takeResults()
{
    const juce::SpinLock::ScopedLockType sl (lock);
    std::vector<FileAnalysisResult> out;
    out.swap (results);
    return out;
}

FileAnalysisResult FileAnalyzer::analyseFile (const juce::File& file)
{
    FileAnalysisResult result;
    result.path = file.getFullPathName();
    result.fileName = file.getFileName();

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));

    if (reader == nullptr)
    {
        result.error = "unsupported or unreadable";
        return result;
    }

    const int sourceChannels = juce::jlimit (1, 2, (int) reader->numChannels);
    const auto length = reader->lengthInSamples;

    if (reader->numChannels < 1 || reader->numChannels > 2)
    {
        result.error = "only mono / stereo files are supported";
        return result;
    }

    if (length <= 0)
    {
        result.error = "empty file";
        return result;
    }

    const double sourceRate = reader->sampleRate > 0.0 ? reader->sampleRate : 48000.0;

    result.sampleRate = sourceRate;
    result.bitDepth = reader->bitsPerSample;
    result.numChannels = (int) reader->numChannels;
    result.numSamples = length;
    result.durationSeconds = (double) length / sourceRate;
    const auto initialSize = file.getSize();
    const auto initialTime = file.getLastModificationTime();
    CancellableSource source (file, [this] { return threadShouldExit(); });
    if (! source.openedOk()) { result.error = "cannot read source"; return result; }
    const auto sourceHash = juce::SHA256 (source).toHexString();
    if (threadShouldExit()) { result.error = "cancelled"; return result; }
    if (source.getStatus().failed() || source.getPosition() != initialSize)
    { result.error = "source hash read failed"; return result; }

    // ------------------------------------------------------------------
    // 1. Loudness / dynamics / signal counters, exactly the live engine.
    // ------------------------------------------------------------------
    AnalysisEngine engine;
    Stats liveStats;
    resetStats (liveStats);
    engine.prepare (sourceRate, sourceChannels);

    const int chunk = juce::jmax (1024, (int) std::round (sourceRate * 0.1));
    juce::AudioBuffer<float> buffer (sourceChannels, chunk);
    FileForensicsScanner scanner (sourceRate, sourceChannels, result.bitDepth);

    for (juce::int64 pos = 0; pos < length; pos += chunk)
    {
        if (threadShouldExit())
        {
            result.error = "cancelled";
            return result;
        }

        const int n = (int) juce::jmin ((juce::int64) chunk, length - pos);
        buffer.clear();
        if (! reader->read (&buffer, 0, n, pos, true, true))
        {
            result.error = "audio decode failed";
            return result;
        }
        if (! scanner.process (buffer, n))
        {
            result.forensic = scanner.finish();
            result.forensic.sha256 = sourceHash;
            result.error = "invalid NaN / Inf audio samples";
            return result;
        }
        engine.process (buffer, liveStats, n);
    }

    engine.finish (liveStats);
    result.stats = ml::snapshot (liveStats);
    result.forensic = scanner.finish();
    result.forensic.sha256 = sourceHash;

    // The offline pass covers the complete file, not a DAW segment.
    result.stats.timeOrigin = 0.0;
    result.stats.hostPosition = 0.0;
    result.stats.hostLength = result.durationSeconds;
    result.stats.coveragePct = 100.0f;

    // ------------------------------------------------------------------
    // 2. Welch-style long-term spectrum for the 10 QC bands and defects.
    // ------------------------------------------------------------------
    juce::dsp::FFT fft (fftOrder);
    std::vector<float> window ((size_t) fftSize);

    for (int i = 0; i < fftSize; ++i)
        window[(size_t) i] = 0.5f - 0.5f * (float) std::cos (2.0 * juce::MathConstants<double>::pi
                                                             * (double) i / (double) (fftSize - 1));

    const int numBins = fftSize / 2 + 1;
    std::vector<double> power ((size_t) numBins, 0.0);
    std::vector<double> midPower ((size_t) numBins, 0.0), sidePower ((size_t) numBins, 0.0);
    std::vector<float> fftData ((size_t) fftSize * 2, 0.0f);
    std::vector<float> rightData ((size_t) fftSize * 2, 0.0f);
    juce::AudioBuffer<float> fftBuffer (sourceChannels, fftSize);

    int frames = 0;

    for (juce::int64 pos = 0; pos < length; pos += fftHop)
    {
        if (threadShouldExit())
        {
            result.error = "cancelled";
            return result;
        }

        fftBuffer.clear();
        const int available = (int) juce::jmin ((juce::int64) fftSize, length - pos);
        if (! reader->read (&fftBuffer, 0, available, pos, true, true))
        {
            result.error = "spectrum decode failed";
            return result;
        }

        std::fill (fftData.begin(), fftData.end(), 0.0f);
        std::fill (rightData.begin(), rightData.end(), 0.0f);

        for (int n = 0; n < fftSize; ++n)
        {
            fftData[(size_t) n] = fftBuffer.getSample (0, n) * window[(size_t) n];
            rightData[(size_t) n] = fftBuffer.getSample (sourceChannels - 1, n) * window[(size_t) n];
        }

        fft.performRealOnlyForwardTransform (fftData.data(), true);
        fft.performRealOnlyForwardTransform (rightData.data(), true);

        for (int b = 1; b < numBins; ++b)
        {
            const size_t k = (size_t) b * 2;
            const double lr = fftData[k], li = fftData[k + 1];
            const double rr = rightData[k], ri = rightData[k + 1];
            // Channel energy is preserved even when L and R cancel in mono.
            power[(size_t) b] += (lr * lr + li * li + rr * rr + ri * ri) * 0.5;
            midPower[(size_t) b] += ((lr + rr) * (lr + rr) + (li + ri) * (li + ri)) * 0.25;
            sidePower[(size_t) b] += ((lr - rr) * (lr - rr) + (li - ri) * (li - ri)) * 0.25;
        }

        ++frames;
    }

    std::vector<float> averageDb ((size_t) numBins, kSilentDb);

    if (frames > 0)
        for (int b = 1; b < numBins; ++b)
        {
            const double mean = power[(size_t) b] / (double) frames;
            averageDb[(size_t) b] = mean > 1.0e-20 ? (float) (10.0 * std::log10 (mean))
                                                   : kSilentDb;
        }

    double totalPower = 0.0;
    for (int k = 1; k < numBins; ++k) totalPower += power[(size_t) k];
    result.bandDb.fill (kSilentDb);
    // Same band table and octave normalisation as SpectrumAnalyzer.
    for (int b = 0; b < SpectrumAnalyzer::numBands; ++b)
    {
        const auto& info = SpectrumAnalyzer::getBandInfo (b);
        double bandPower = 0.0;
        double mid = 0.0, side = 0.0;
        int bins = 0;

        for (int k = 1; k < numBins; ++k)
        {
            const double f = (double) k * sourceRate / (double) fftSize;

            if (f >= (double) info.lo && f < (double) info.hi)
            {
                bandPower += power[(size_t) k] / (double) juce::jmax (1, frames);
                mid += midPower[(size_t) k] / (double) juce::jmax (1, frames);
                side += sidePower[(size_t) k] / (double) juce::jmax (1, frames);
                ++bins;
            }
        }

        if (bins > 0 && bandPower > 1.0e-20)
        {
            const double octaves = std::log2 ((double) info.hi / (double) info.lo);
            result.bandDb[(size_t) b] = (float) (10.0 * std::log10 (bandPower)
                                                 - 10.0 * std::log10 (juce::jmax (1.0e-9, octaves)));
            auto& q = result.forensic;
            const auto i = (size_t) b;
            q.energySharePct[i] = totalPower > 0.0 ? (float) (100.0 * bandPower * frames / totalPower) : 0.0f;
            // Ignore spectral leakage / inaudibly small bands in the ranking.
            q.bandAvailable[i] = q.energySharePct[i] >= 0.1f;
            q.monoLossDb[i] = (float) juce::jlimit (0.0, 60.0, -10.0 * std::log10 (std::max (1.0e-6, mid / bandPower)));
            q.sideEnergyPct[i] = (float) juce::jlimit (0.0, 100.0, 100.0 * side / bandPower);
            if (q.bandAvailable[i] && (q.worstMonoBand < 0 || q.monoLossDb[i] > q.monoLossDb[(size_t) q.worstMonoBand]))
                q.worstMonoBand = b;
        }
    }

    result.defects = analyseSignalDefects (averageDb, sourceRate / (double) fftSize, result.stats);

    if (file.getSize() != initialSize || file.getLastModificationTime() != initialTime)
    {
        result.error = "source changed during analysis; retry";
        return result;
    }

    CancellableSource finalSource (file, [this] { return threadShouldExit(); });
    const auto finalHash = juce::SHA256 (finalSource).toHexString();
    if (threadShouldExit() || finalHash != sourceHash)
    { result.error = "source changed during analysis"; return result; }
    result.ok = true;
    return result;
}

void FileAnalyzer::run()
{
    juce::StringArray files;

    {
        const juce::SpinLock::ScopedLockType sl (lock);
        files = pending;
        status = "analysing";
    }

    int index = 0;

    for (const auto& path : files)
    {
        if (threadShouldExit())
            break;

        {
            const juce::SpinLock::ScopedLockType sl (lock);
            current = juce::File (path).getFileName();
        }

        auto result = analyseFile (juce::File (path));

        {
            const juce::SpinLock::ScopedLockType sl (lock);
            results.push_back (std::move (result));
        }

        ++index;
        done.store (index, std::memory_order_release);
    }

    {
        const juce::SpinLock::ScopedLockType sl (lock);
        status = threadShouldExit() ? "cancelled" : "done";
        current.clear();
    }

    busy.store (false, std::memory_order_release);
}
} // namespace ml

