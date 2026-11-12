#include "CodecABJob.h"

#include "CodecCheck.h"

namespace ml
{

namespace
{
/** Time of the largest decoded sample: where the encoded side actually peaks. */
double decodedPeakSeconds (const juce::File& file)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));

    if (reader == nullptr || reader->lengthInSamples <= 0 || reader->sampleRate <= 0.0
        || reader->numChannels <= 0)
        return -1.0;

    constexpr int block = 8192;
    juce::AudioBuffer<float> buffer (juce::jmin (2, (int) reader->numChannels), block);
    juce::int64 pos = 0;
    float peak = 0.0f;
    juce::int64 peakPos = 0;

    while (pos < reader->lengthInSamples)
    {
        const int n = (int) juce::jmin ((juce::int64) block, (juce::int64) reader->lengthInSamples - pos);
        buffer.clear();
        reader->read (&buffer, 0, n, pos, true, true);

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            for (int i = 0; i < n; ++i)
            {
                const float v = std::abs (buffer.getSample (ch, i));

                if (v > peak)
                {
                    peak = v;
                    peakPos = pos + i;
                }
            }

        pos += n;
    }

    // 3.3: refine the sample peak with 4x windowed-sinc interpolation so the
    // locator reports the encoded true-peak position, not just the nearest sample.
    constexpr int taps = 16;

    if (peakPos >= taps && peakPos + taps < reader->lengthInSamples)
    {
        const int count = 2 * taps + 1;
        juce::AudioBuffer<float> window (juce::jmin (2, (int) reader->numChannels), count);
        window.clear();
        reader->read (&window, 0, count, peakPos - taps, true, true);

        std::vector<float> mono ((size_t) count, 0.0f);

        for (int ch = 0; ch < window.getNumChannels(); ++ch)
            for (int i = 0; i < count; ++i)
                mono[(size_t) i] += window.getSample (ch, i) / (float) window.getNumChannels();

        float best = peak;
        double bestPos = (double) peakPos;

        for (const double frac : { 0.25, 0.5, 0.75 })
        {
            const float v = std::abs (interpolateSample (mono.data(), taps, frac, taps));

            if (v > best)
            {
                best = v;
                bestPos = (double) peakPos + frac;
            }
        }

        return bestPos / reader->sampleRate;
    }

    return (double) peakPos / reader->sampleRate;
}
} // namespace
CodecABJob::CodecABJob()
    : juce::Thread ("MasterLens codec A/B")
{
}

CodecABJob::~CodecABJob()
{
    signalThreadShouldExit();
    notify();
    stopThread (4000);

    const juce::SpinLock::ScopedLockType sl (lock);

    if (workDirectory != juce::File())
        workDirectory.deleteRecursively();
}

void CodecABJob::start (const juce::File& source, const juce::String& codec)
{
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        pendingSource = source;
        pendingCodec = codec;
        statusText = "encoding";
        hasResult = false;
    }

    busy.store (true, std::memory_order_release);
    pending.store (true, std::memory_order_release);
    cancelFlag.store (false, std::memory_order_release);
    progress.store (0.0, std::memory_order_relaxed);
    request.fetch_add (1, std::memory_order_acq_rel);

    if (! isThreadRunning())
        startThread();
    else
        notify();
}

void CodecABJob::cancel()
{
    pending.store (false, std::memory_order_release);
    cancelFlag.store (true, std::memory_order_release);
    request.fetch_add (1, std::memory_order_acq_rel);
    busy.store (false, std::memory_order_release);

    const juce::SpinLock::ScopedLockType sl (lock);
    statusText = "cancelled";
    hasResult = false;
}

void CodecABJob::clear()
{
    cancel();
    cancelFlag.store (true, std::memory_order_release);
    signalThreadShouldExit();
    notify();
    stopThread (4000);

    const juce::SpinLock::ScopedLockType sl (lock);

    if (workDirectory != juce::File())
    {
        workDirectory.deleteRecursively();
        workDirectory = juce::File();
    }
}

juce::String CodecABJob::getStatus() const
{
    const juce::SpinLock::ScopedLockType sl (lock);
    return statusText;
}

bool CodecABJob::takeResult (CodecABResult& out)
{
    const juce::SpinLock::ScopedLockType sl (lock);

    if (! hasResult)
        return false;

    out = result;
    hasResult = false;
    return true;
}

juce::File CodecABJob::getDecodedFile() const
{
    const juce::SpinLock::ScopedLockType sl (lock);
    return hasResult ? result.decodedFile : juce::File();
}

void CodecABJob::run()
{
    while (! threadShouldExit())
    {
        if (! pending.exchange (false, std::memory_order_acq_rel))
        {
            wait (40);
            continue;
        }

        const auto serial = request.load (std::memory_order_acquire);
        juce::File source;
        juce::String codec;

        {
            const juce::SpinLock::ScopedLockType sl (lock);
            source = pendingSource;
            codec = pendingCodec;
        }

        {
            const juce::SpinLock::ScopedLockType sl (lock);
            statusText = "encoding";
        }

        const auto parent = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                .getChildFile ("MasterLensCodec");

        double duration = 0.0;
        {
            juce::AudioFormatManager formats;
            formats.registerBasicFormats();
            std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (source));

            if (reader != nullptr && reader->sampleRate > 0.0)
                duration = (double) reader->lengthInSamples / reader->sampleRate;
        }

        auto preview = createCodecPreview (source, codec, parent, &cancelFlag, duration,
                                           [this] (double fraction)
                                           {
                                               progress.store (fraction, std::memory_order_relaxed);
                                           });

        if (request.load() != serial || cancelFlag.load (std::memory_order_acquire))
        {
            if (preview.workDirectory != juce::File())
                preview.workDirectory.deleteRecursively();

            const juce::SpinLock::ScopedLockType sl (lock);
            statusText = "cancelled";
            busy.store (false, std::memory_order_release);
            continue;
        }

        progress.store (1.0, std::memory_order_relaxed);

        CodecABResult measured;
        measured.codec = preview.codec;
        measured.label = preview.codec == "aac" ? "AAC 256k" : "MP3 320k";
        measured.decodedFile = preview.decodedFile;
        measured.delaySamples = preview.delaySamples;
        measured.tailSamples = preview.tailSamples;
        measured.residual = preview.residual;
        measured.peakTimeSeconds = decodedPeakSeconds (preview.decodedFile);
        measured.encodedBytes = preview.encodedBytes;
        measured.backend = preview.backend;
        measured.ffmpegVersion = preview.ffmpegVersion;

        if (! preview.ok)
        {
            measured.error = preview.error;

            const juce::SpinLock::ScopedLockType sl (lock);
            result = measured;
            hasResult = true;
            statusText = "failed";
            busy.store (false, std::memory_order_release);
            continue;
        }

        {
            const juce::SpinLock::ScopedLockType sl (lock);
            statusText = "measuring";
        }

        FileAnalyzer analyzer;
        analyzer.analyse ({ preview.decodedFile.getFullPathName() });

        for (int wait = 0; wait < 2400 && analyzer.isBusy(); ++wait)
        {
            if (threadShouldExit() || request.load() != serial)
                break;

            juce::Thread::sleep (10);
        }

        auto results = analyzer.takeResults();

        if (request.load() != serial)
        {
            preview.workDirectory.deleteRecursively();
            continue;
        }

        if (results.size() != 1 || ! results[0].ok)
        {
            measured.error = "decoded file analysis failed";
            preview.workDirectory.deleteRecursively();
        }
        else
        {
            measured.ok = true;
            measured.decodedLufs = results[0].stats.integrated;
            measured.decodedTp = results[0].stats.truePeakDb;
        }

        {
            const juce::SpinLock::ScopedLockType sl (lock);

            if (workDirectory != juce::File() && workDirectory != preview.workDirectory)
                workDirectory.deleteRecursively();

            workDirectory = measured.ok ? preview.workDirectory : juce::File();
            result = measured;
            hasResult = true;
            statusText = measured.ok ? "ready" : "failed";
        }

        busy.store (false, std::memory_order_release);
    }
}
} // namespace ml
