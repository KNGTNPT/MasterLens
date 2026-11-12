#include "ReferencePlayer.h"

#include <cmath>

namespace ml
{
ReferencePlayer::ReferencePlayer()
    : juce::Thread ("MasterLens reference player")
{
    // Fixed read-ahead chunk; never resized on the audio thread.
    streamChunk.setSize (2, 8196);
}

ReferencePlayer::~ReferencePlayer()
{
    signalThreadShouldExit();
    notify();
    stopThread (-1);
}

void ReferencePlayer::load (const juce::StringArray& files, double sampleRate)
{
    const juce::SpinLock::ScopedLockType command (lock);
    pendingPath.clear();

    for (const auto& f : files)
        if (f.isNotEmpty())
        {
            pendingPath = f;
            break;
        }

    targetRate.store (sampleRate > 0.0 ? sampleRate : 48000.0, std::memory_order_relaxed);

    {
        status = pendingPath.isEmpty() ? "empty" : "loading";
        fileName.clear();


    }

    ready.store (false, std::memory_order_release);
    busy.store (! pendingPath.isEmpty(), std::memory_order_release);

    enabled.store (false);
    request.fetch_add (1);
    if (! isThreadRunning()) startThread();
    notify();
}

void ReferencePlayer::clear()
{
    load ({}, targetRate.load());
    enabled.store (false);
    duration.store (0.0);
    offsetSeconds.store (0.0);
    gainDb.store (0.0f);
    loudnessLufs.store (kSilentDb);

    {
        const juce::SpinLock::ScopedLockType sl (streamLock);
        if (streamReader != nullptr)
        {
            retiredReaders.push_back (streamReader);
            streamReader.reset();
        }
        streamLength = 0;
    }

    streaming.store (false, std::memory_order_release);
    memoryBytes.store (0, std::memory_order_relaxed);

    {
        const juce::SpinLock::ScopedLockType sl (loopLock);
        streamLoop.setSize (0, 0);
        streamLoopReady.store (false, std::memory_order_release);
        streamLoopPos = 0.0;
    }
}

juce::String ReferencePlayer::getStatus() const
{
    const juce::SpinLock::ScopedLockType sl (lock);
    return status;
}

juce::String ReferencePlayer::getFileName() const
{
    const juce::SpinLock::ScopedLockType sl (lock);
    return fileName;
}

void ReferencePlayer::preloadStreamLoop()
{
    const double start = loopStartSeconds.load (std::memory_order_relaxed);
    const double length = loopLengthSeconds.load (std::memory_order_relaxed);

    if (! streaming.load (std::memory_order_acquire) || length <= 0.05 || length > 30.0)
    {
        const juce::SpinLock::ScopedLockType sl (loopLock);
        streamLoop.setSize (0, 0);
        streamLoopReady.store (false, std::memory_order_release);
        return;
    }

    juce::String path;
    { const juce::SpinLock::ScopedLockType sl (lock); path = streamPath; }

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (juce::File (path)));

    if (reader == nullptr || reader->sampleRate <= 0.0)
        return;

    const auto frames = (juce::int64) juce::jmax (1.0, length * reader->sampleRate);
    juce::AudioBuffer<float> loaded (juce::jmin (2, (int) reader->numChannels), (int) frames);
    loaded.clear();
    reader->read (&loaded, 0, (int) frames,
                   (juce::int64) juce::jmax (0.0, start * reader->sampleRate), true, true);

    std::vector<float> check;
    bool finite = true;

    for (int ch = 0; ch < loaded.getNumChannels() && finite; ++ch)
        for (int i = 0; i < loaded.getNumSamples(); i += 97)
            finite = finite && std::isfinite (loaded.getSample (ch, i));

    if (! finite)
        return;

    // 3.2: measure the loop region so equal-loudness monitoring also works
    // for streamed references (the full file is never decoded).
    AnalysisEngine measure;
    Stats measured;
    resetStats (measured);
    measure.prepare (reader->sampleRate, loaded.getNumChannels());
    measure.process (loaded, measured, loaded.getNumSamples());
    measure.finish (measured);
    const float loopLufs = snapshot (measured).integrated;

    const juce::SpinLock::ScopedLockType sl (loopLock);
    streamLoop = std::move (loaded);
    streamLoopPos = 0.0;
    streamLoopReady.store (true, std::memory_order_release);

    if (loopLufs > -70.0f)
        loudnessLufs.store (loopLufs, std::memory_order_relaxed);
}

bool ReferencePlayer::renderStreamLoop (juce::AudioBuffer<float>& output, bool hostPlaying,
                                        float extraGainDb, float inputGainDb)
{
    const juce::SpinLock::ScopedLockType sl (loopLock);

    if (! streamLoopReady.load (std::memory_order_acquire))
        return false;

    const int n = output.getNumSamples();
    const int frames = streamLoop.getNumSamples();
    const int channels = streamLoop.getNumChannels();

    if (n <= 0 || frames < 8 || channels <= 0)
        return false;

    const bool on = enabled.load (std::memory_order_acquire);
    const double ratio = streamRate / juce::jmax (1.0, targetRate.load (std::memory_order_relaxed));
    const float inputGain = juce::Decibels::decibelsToGain (inputGainDb);
    const float step = (float) (1.0 / (0.005 * juce::jmax (1.0, targetRate.load())));
    const float gain = juce::Decibels::decibelsToGain (
        extraGainDb + gainDb.load (std::memory_order_relaxed), -60.0f);
    const double fade = juce::jmin (64.0, frames * 0.25);

    auto sampleAt = [&] (int ch, double p)
    {
        const int i1 = juce::jlimit (0, frames - 1, (int) std::floor (p));
        const int i0 = juce::jmax (0, i1 - 1);
        const int i2 = juce::jmin (frames - 1, i1 + 1);
        const int i3 = juce::jmin (frames - 1, i1 + 2);
        const float t = (float) (p - (double) i1);
        const float p0 = streamLoop.getSample (ch, i0), p1 = streamLoop.getSample (ch, i1);
        const float p2 = streamLoop.getSample (ch, i2), p3 = streamLoop.getSample (ch, i3);
        const float a = -0.5f * p0 + 1.5f * p1 - 1.5f * p2 + 0.5f * p3;
        const float b = p0 - 2.5f * p1 + 2.0f * p2 - 0.5f * p3;
        const float c = -0.5f * p0 + 0.5f * p2;
        return ((a * t + b) * t + c) * t + p1;
    };

    double pos = streamLoopPos;

    if (pos < 0.0 || pos >= (double) frames)
        pos = 0.0;

    for (int i = 0; i < n; ++i)
    {
        const float target = on && hostPlaying ? 1.0f : 0.0f;
        audioMix += juce::jlimit (-step, step, target - audioMix);

        const float inputL = output.getSample (0, i) * inputGain;
        const float inputR = output.getNumChannels() > 1 ? output.getSample (1, i) * inputGain : inputL;

        float l = sampleAt (0, pos);
        float r = channels > 1 ? sampleAt (1, pos) : l;

        if (pos > frames - fade)
        {
            const float t = (float) juce::jlimit (0.0, 1.0, (pos - (frames - fade)) / fade);
            const double head = pos - (frames - fade);
            const float headL = sampleAt (0, head);
            const float headR = channels > 1 ? sampleAt (1, head) : headL;
            l = l * (1.0f - t) + headL * t;
            r = r * (1.0f - t) + headR * t;
        }

        output.setSample (0, i, inputL * (1.0f - audioMix) + l * gain * audioMix);

        if (output.getNumChannels() > 1)
            output.setSample (1, i, inputR * (1.0f - audioMix) + r * gain * audioMix);

        pos += ratio;

        if (pos >= (double) frames)
            pos = wrapLoopPosition (pos, 0.0, (double) frames);
    }

    for (int ch = 2; ch < output.getNumChannels(); ++ch)
        output.clear (ch, 0, n);

    streamLoopPos = pos;
    audioPosition = pos;
    liveMix.store (audioMix);
    return true;
}

bool ReferencePlayer::renderStreaming (juce::BufferingAudioReader& reader, juce::AudioBuffer<float>& output,
                                       double hostSeconds, bool hostPlaying,
                                       float extraGainDb, float inputGainDb)
{
    if (loopOn.load (std::memory_order_acquire) && streamLoopReady.load (std::memory_order_acquire))
        return renderStreamLoop (output, hostPlaying, extraGainDb, inputGainDb);

    const int n = output.getNumSamples();
    const int channels = juce::jmin (2, output.getNumChannels());

    if (n <= 0 || channels <= 0 || n + 4 > streamChunk.getNumSamples() || streamLength <= 1)
        return false;

    const bool on = enabled.load (std::memory_order_acquire);
    const double target = juce::jmax (1.0, targetRate.load (std::memory_order_relaxed));
    const double ratio = streamRate / target;

    double pos = (hostPlaying ? hostSeconds + offsetSeconds.load (std::memory_order_relaxed)
                              : audioPosition) * streamRate;

    if (! std::isfinite (pos))
        pos = 0.0;

    const juce::int64 start = juce::jmax ((juce::int64) 0,
                                          (juce::int64) std::floor (pos) - 1);
    const int need = n + 4;
    streamChunk.clear();

    const bool got = reader.read (&streamChunk, 0, need, start, true, true);

    const float inputGain = juce::Decibels::decibelsToGain (inputGainDb);
    const float step = (float) (1.0 / (0.005 * target));
    const float gain = juce::Decibels::decibelsToGain (
        extraGainDb + gainDb.load (std::memory_order_relaxed), -60.0f);

    const auto sampleCubic = [&] (int ch, double rel)
    {
        const int i1 = (int) std::floor (rel);
        const float t = (float) (rel - i1);
        const auto s = [&] (int i)
        {
            return streamChunk.getSample (ch, juce::jlimit (0, streamChunk.getNumSamples() - 1, i));
        };
        const float p0 = s (i1 - 1), p1 = s (i1), p2 = s (i1 + 1), p3 = s (i1 + 2);
        const float a = -0.5f * p0 + 1.5f * p1 - 1.5f * p2 + 0.5f * p3;
        const float b = p0 - 2.5f * p1 + 2.0f * p2 - 0.5f * p3;
        const float c = -0.5f * p0 + 0.5f * p2;
        return ((a * t + b) * t + c) * t + p1;
    };

    for (int i = 0; i < n; ++i)
    {
        const bool inRange = pos >= 0.0 && pos < (double) streamLength;
        const float mixTarget = on && hostPlaying && got && inRange ? 1.0f : 0.0f;
        audioMix += juce::jlimit (-step, step, mixTarget - audioMix);

        const float inputL = output.getSample (0, i) * inputGain;
        const float inputR = output.getNumChannels() > 1 ? output.getSample (1, i) * inputGain : inputL;
        const double rel = pos - (double) start;
        const float l = got && inRange ? sampleCubic (0, rel) : 0.0f;
        const float r = got && inRange ? (channels > 1 ? sampleCubic (1, rel) : l) : 0.0f;

        output.setSample (0, i, inputL * (1.0f - audioMix) + l * gain * audioMix);

        if (output.getNumChannels() > 1)
            output.setSample (1, i, inputR * (1.0f - audioMix) + r * gain * audioMix);

        pos += ratio;
    }

    for (int ch = 2; ch < output.getNumChannels(); ++ch)
        output.clear (ch, 0, n);

    audioPosition = pos;
    liveMix.store (audioMix);
    return true;
}

bool ReferencePlayer::render (juce::AudioBuffer<float>& output, double hostSeconds,
                              bool hostPlaying, float extraGainDb, float inputGainDb)
{
    const bool on = enabled.load (std::memory_order_acquire);
    if (! on && audioMix <= 0.0f) return false;

    {
        std::shared_ptr<juce::BufferingAudioReader> shared;
        { const juce::SpinLock::ScopedLockType sl (streamLock); shared = streamReader; }

        if (shared != nullptr)
            return renderStreaming (*shared, output, hostSeconds, hostPlaying, extraGainDb, inputGainDb);
    }

    const auto d = data.acquire();

    if (! d || output.getNumSamples() <= 0)
    { audioMix = 0; liveMix.store (0); return false; }

    const auto& src = d->audio;
    const int sourceChannels = src.getNumChannels();

    if (sourceChannels <= 0 || d->length <= 1) return false;

    const double ratio = d->sourceRate / juce::jmax (1.0, targetRate.load (std::memory_order_relaxed));

    // 3.2 cue loop: when engaged, play the reference region A->B with a short
    // equal-power crossfade at the wrap so the loop point does not click.
    const double srcRate = d->sourceRate;
    const bool looping = loopOn.load (std::memory_order_acquire) && hostPlaying
                         && loopLengthSeconds.load (std::memory_order_relaxed) > 0.05;
    const double loopStartSamples = loopStartSeconds.load (std::memory_order_relaxed) * srcRate;
    const double loopLenSamples = loopLengthSeconds.load (std::memory_order_relaxed) * srcRate;
    const double loopEndSamples = loopStartSamples + loopLenSamples;
    const double fadeSamples = looping ? juce::jmin (64.0, loopLenSamples * 0.25) : 0.0;

    if (looping && (loopPosition < loopStartSamples || loopPosition >= loopEndSamples))
        loopPosition = loopStartSamples;

    double pos = looping ? loopPosition
                         : (hostPlaying ? (hostSeconds + offsetSeconds.load (std::memory_order_relaxed)) * d->sourceRate
                                        : audioPosition);
    const float inputGain = juce::Decibels::decibelsToGain (inputGainDb);
    const float step = (float) (1.0 / (0.005 * juce::jmax (1.0, targetRate.load())));
    const float gain = juce::Decibels::decibelsToGain (
        extraGainDb + gainDb.load (std::memory_order_relaxed), -60.0f);
    const auto length = (int) d->length;

    // Catmull-Rom (4-point cubic) keeps 44.1 / 48 / 88.2 / 96 kHz references
    // musical; linear interpolation was audible on highs at 2:1 ratios.
    const auto sampleCubic = [&src, length] (int ch, double p)
    {
        const int i1 = juce::jlimit (0, length - 1, (int) std::floor (p));
        const int i0 = juce::jmax (0, i1 - 1);
        const int i2 = juce::jmin (length - 1, i1 + 1);
        const int i3 = juce::jmin (length - 1, i1 + 2);
        const float t = (float) (p - (double) i1);
        const float p0 = src.getSample (ch, i0);
        const float p1 = src.getSample (ch, i1);
        const float p2 = src.getSample (ch, i2);
        const float p3 = src.getSample (ch, i3);
        const float a = -0.5f * p0 + 1.5f * p1 - 1.5f * p2 + 0.5f * p3;
        const float b = p0 - 2.5f * p1 + 2.0f * p2 - 0.5f * p3;
        const float c = -0.5f * p0 + 0.5f * p2;
        return ((a * t + b) * t + c) * t + p1;
    };

    for (int i = 0; i < output.getNumSamples(); ++i)
    {
        const auto index = (juce::int64) std::floor (pos);

        const bool inRange = index >= 0 && index < d->length;
        const float target = on && ready.load() && hostPlaying && inRange
                                 ? (looping ? 1.0f
                                            : juce::jlimit (0.0f, 1.0f, (float) ((d->length-pos) / (ratio / step))))
                                 : 0.0f;
        audioMix += juce::jlimit (-step, step, target - audioMix);
        const float inputL = output.getSample (0, i) * inputGain;
        const float inputR = output.getNumChannels() > 1 ? output.getSample (1, i) * inputGain : inputL;

        float l = inRange ? sampleCubic (0, pos) : 0.0f;
        float r = inRange && sourceChannels > 1 ? sampleCubic (1, pos) : l;

        if (looping && fadeSamples > 0.0 && pos > loopEndSamples - fadeSamples)
        {
            // Equal-power blend from the loop tail into the loop head.
            const float t = (float) juce::jlimit (0.0, 1.0, (pos - (loopEndSamples - fadeSamples)) / fadeSamples);
            const double tailPos = loopStartSamples + (pos - (loopEndSamples - fadeSamples));
            const float headL = inRange ? sampleCubic (0, tailPos) : 0.0f;
            const float headR = sourceChannels > 1 ? sampleCubic (1, tailPos) : headL;
            l = l * (1.0f - t) + headL * t;
            r = r * (1.0f - t) + headR * t;
        }

        if (output.getNumChannels() == 1)
            output.setSample (0, i, inputL * (1.0f-audioMix) + 0.5f * (l+r) * gain * audioMix);
        else
        {
            output.setSample (0, i, inputL * (1.0f-audioMix) + l * gain * audioMix);
            output.setSample (1, i, inputR * (1.0f-audioMix) + r * gain * audioMix);
        }

        pos += ratio;

        if (looping && pos >= loopEndSamples)
            pos = wrapLoopPosition (pos, loopStartSamples, loopLenSamples);
    }

    if (looping)
        loopPosition = pos;

    audioPosition = pos; liveMix.store (audioMix);
    if (! looping && pos >= (double) d->length && audioMix <= 0) enabled.store (false);
    for (int ch = 2; ch < output.getNumChannels(); ++ch)
        output.clear (ch, 0, output.getNumSamples());

    return true;
}

void ReferencePlayer::run()
{
    unsigned handled = 0;
    while (! threadShouldExit())
    {
        data.collect();
        const auto serial = request.load();
        if (serial == handled) { wait (25); continue; }
        handled = serial;
        juce::String path;
        { const juce::SpinLock::ScopedLockType sl (lock); path = pendingPath; }
        if (path.isEmpty())
        {
            for (int i = 0; i < 50 && liveMix.load() > 0 && request.load() == serial; ++i) wait (2);
            if (request.load() == serial) data.publish ({});
            continue;
        }
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        const juce::File file (path);
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
        const auto fail = [&] ()
        {
            const juce::SpinLock::ScopedLockType sl (lock);
            if (request.load() == serial) { status = "failed"; ready.store (false); busy.store (false); }
        };
        if (! reader || ! std::isfinite (reader->sampleRate) || reader->sampleRate < 1000 || reader->sampleRate > 768000
            || reader->numChannels < 1 || reader->lengthInSamples <= 1 || reader->numChannels > 2
            || reader->lengthInSamples > (juce::int64) (reader->sampleRate * 3600.0))
        { fail(); continue; }
        // 3.2: long references stream from a bounded read-ahead buffer instead
        // of being decoded into one big AudioBuffer. 64 MB is the cut-off.
        const auto bytes = (juce::int64) reader->lengthInSamples * reader->numChannels * 4;

        if (bytes > 64LL * 1024 * 1024)
        {
            const auto rate = reader->sampleRate;
            const auto length = reader->lengthInSamples;
            const int samplesToBuffer = (int) juce::jlimit (48000.0, 786432.0, rate * 8.0);

            if (! streamThread.isThreadRunning())
                streamThread.startThread();

            std::shared_ptr<juce::BufferingAudioReader> stream (
                new juce::BufferingAudioReader (reader.release(), streamThread, samplesToBuffer));
            stream->setReadTimeout (0);

            {
                const juce::SpinLock::ScopedLockType sl (streamLock);
                if (streamReader != nullptr)
                    retiredReaders.push_back (streamReader);
                streamReader = stream;
                streamLength = length;
                streamRate = rate;
            }

            data.publish ({});
            {
                const juce::SpinLock::ScopedLockType sl (lock);
                streamPath = file.getFullPathName();
            }

            loudnessLufs.store (kSilentDb, std::memory_order_relaxed);

            // Bounded prefix loudness (<=60 s) for streamed references: the
            // full file is never decoded, but equal-loudness monitoring still
            // needs a usable estimate.
            {
                juce::AudioFormatManager prefixFormats;
                prefixFormats.registerBasicFormats();
                std::unique_ptr<juce::AudioFormatReader> prefixReader (prefixFormats.createReaderFor (file));

                if (prefixReader != nullptr && prefixReader->numChannels > 0)
                {
                    AnalysisEngine measure;
                    Stats measured;
                    resetStats (measured);
                    measure.prepare (rate, (int) prefixReader->numChannels);

                    juce::AudioBuffer<float> chunk ((int) prefixReader->numChannels, 4096);
                    const juce::int64 total = juce::jmin (length, (juce::int64) (rate * 60.0));

                    for (juce::int64 pos = 0; pos < total; pos += 4096)
                    {
                        const int n = (int) juce::jmin ((juce::int64) 4096, total - pos);
                        chunk.clear();
                        prefixReader->read (&chunk, 0, n, pos, true, true);
                        measure.process (chunk, measured, n);
                    }

                    measure.finish (measured);
                    const float prefixLufs = snapshot (measured).integrated;

                    if (prefixLufs > -70.0f)
                        loudnessLufs.store (prefixLufs, std::memory_order_relaxed);
                }
            }

            streaming.store (true, std::memory_order_release);
            const juce::int64 bytesUsed = (juce::int64) samplesToBuffer * 2 * 4
                                        + (juce::int64) streamChunk.getNumSamples() * 2 * 4;
            memoryBytes.store (bytesUsed, std::memory_order_relaxed);
            peakMemoryBytes.store (juce::jmax (peakMemoryBytes.load (std::memory_order_relaxed), bytesUsed),
                                   std::memory_order_relaxed);
            duration.store ((double) length / rate);
            ready.store (true); busy.store (false);

            {
                const juce::SpinLock::ScopedLockType sl (lock);
                status = "ready (stream)"; fileName = file.getFileName();
            }

            continue;
        }

        // Short references keep the exact in-memory path.
        {
            const juce::SpinLock::ScopedLockType sl (streamLock);
            if (streamReader != nullptr)
            {
                retiredReaders.push_back (streamReader);
                streamReader.reset();
            }
        }
        streaming.store (false, std::memory_order_release);

        auto loaded = std::make_unique<Data>();
        loaded->sourceRate = reader->sampleRate;
        loaded->length = reader->lengthInSamples;
        loaded->audio.setSize ((int) reader->numChannels, (int) loaded->length);
        AnalysisEngine measure; Stats measured; resetStats (measured);
        measure.prepare (loaded->sourceRate, loaded->audio.getNumChannels());
        bool ok = true;
        for (int offset = 0; offset < (int) loaded->length; offset += 4096)
        {
            if (threadShouldExit() || request.load() != serial) { ok = false; break; }
            const int count = juce::jmin (4096, (int) loaded->length - offset);
            if (! reader->read (&loaded->audio, offset, count, offset, true, true)) { ok = false; break; }
            juce::AudioBuffer<float> view (loaded->audio.getArrayOfWritePointers(),
                loaded->audio.getNumChannels(), offset, count);
            for (int ch = 0; ch < view.getNumChannels(); ++ch)
                for (int i = 0; i < count; ++i) if (! std::isfinite (view.getSample (ch, i))) ok = false;
            if (! ok) break;
            measure.process (view, measured, count);
        }
        if (! ok) { if (request.load() == serial) fail(); continue; }
        measure.finish (measured);
        for (int i = 0; i < 50 && liveMix.load() > 0 && request.load() == serial; ++i) wait (2);
        const juce::SpinLock::ScopedLockType sl (lock);
        if (request.load() != serial || threadShouldExit()) continue;
        const auto length = loaded->length;
        const auto rate = loaded->sourceRate;
        const auto channels = loaded->audio.getNumChannels();
        data.publish (std::move (loaded));
        const juce::int64 bytesUsed = (juce::int64) length * channels * 4;
        memoryBytes.store (bytesUsed, std::memory_order_relaxed);
        peakMemoryBytes.store (juce::jmax (peakMemoryBytes.load (std::memory_order_relaxed), bytesUsed),
                               std::memory_order_relaxed);
        status = "ready"; fileName = file.getFileName();
        loudnessLufs.store (snapshot (measured).integrated);
        duration.store ((double) length / rate);
        ready.store (true); busy.store (false);
    }
    data.publish ({});
}
} // namespace ml
