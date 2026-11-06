#include "AuditionEngine.h"

#include <algorithm>
#include <cmath>

namespace ml
{
void AuditionEngine::Biquad::setHighPass (double sr, double hz, double q) noexcept
{
    const double w0 = 2.0 * juce::MathConstants<double>::pi * juce::jlimit (10.0, sr * 0.45, hz) / sr;
    const double c = std::cos (w0), s = std::sin (w0);
    const double alpha = s / (2.0 * juce::jmax (0.1, q));
    const double a0 = 1.0 + alpha;

    b0 = (float) ((1.0 + c) * 0.5 / a0);
    b1 = (float) (-(1.0 + c) / a0);
    b2 = b0;
    a1 = (float) (-2.0 * c / a0);
    a2 = (float) ((1.0 - alpha) / a0);
    reset();
}

void AuditionEngine::Biquad::setLowPass (double sr, double hz, double q) noexcept
{
    const double w0 = 2.0 * juce::MathConstants<double>::pi * juce::jlimit (10.0, sr * 0.45, hz) / sr;
    const double c = std::cos (w0), s = std::sin (w0);
    const double alpha = s / (2.0 * juce::jmax (0.1, q));
    const double a0 = 1.0 + alpha;

    b0 = (float) ((1.0 - c) * 0.5 / a0);
    b1 = (float) ((1.0 - c) / a0);
    b2 = b0;
    a1 = (float) (-2.0 * c / a0);
    a2 = (float) ((1.0 - alpha) / a0);
    reset();
}

void AuditionEngine::prepare (double newSampleRate, int)
{
    active.store (false, std::memory_order_release);
    pendingStart.store (false, std::memory_order_release);
    mode.store ((int) Mode::off, std::memory_order_release);
    rangeRejected.store (0, std::memory_order_relaxed);

    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    ringLength = juce::jmax (1024, (int) std::round (sampleRate * 12.0));
    left.assign ((size_t) ringLength, 0.0f);
    right.assign ((size_t) ringLength, 0.0f);
    writePos = 0;
    totalSamples = 0;
    loopStart = loopLength = loopPos = 0;
    liveFade = 0;
    gain = 1.0f;
    fadeSamples = juce::jmax (1, (int) std::round (sampleRate * 0.005));
    fourthOrder = false;
    compensate = true;
    hp2L.reset(); hp2R.reset(); lp2L.reset(); lp2R.reset();
}

void AuditionEngine::reset()
{
    active.store (false, std::memory_order_release);
    pendingStart.store (false, std::memory_order_release);
    mode.store ((int) Mode::off, std::memory_order_release);
    rangeRejected.store (0, std::memory_order_relaxed);
    std::fill (left.begin(), left.end(), 0.0f);
    std::fill (right.begin(), right.end(), 0.0f);
    writePos = 0;
    totalSamples = 0;
    loopStart = loopLength = loopPos = 0;
    liveFade = 0;
    gain = 1.0f;
    fourthOrder = false;
    compensate = true;
    hpL.reset(); hpR.reset(); lpL.reset(); lpR.reset();
    hp2L.reset(); hp2R.reset(); lp2L.reset(); lp2R.reset();
}

void AuditionEngine::start (double startSeconds, double endSeconds, float low, float high, bool monoFold)
{
    if (high < low)
        std::swap (low, high);

    if (endSeconds <= startSeconds)
        endSeconds = startSeconds + 1.5;

    requestedStart.store (startSeconds, std::memory_order_relaxed);
    requestedEnd.store (endSeconds, std::memory_order_relaxed);
    requestedLo.store (juce::jlimit (20.0f, 20000.0f, low), std::memory_order_relaxed);
    requestedHi.store (juce::jlimit (20.0f, 20000.0f, high), std::memory_order_relaxed);
    requestedMono.store (monoFold, std::memory_order_relaxed);
    rangeRejected.store (0, std::memory_order_relaxed);
    mode.store ((int) Mode::loop, std::memory_order_relaxed);
    active.store (false, std::memory_order_release);
    pendingStart.store (true, std::memory_order_release);
}

void AuditionEngine::startLive (float low, float high, bool monoFold)
{
    startLiveBand (low, high, monoFold, 2, true);
}

void AuditionEngine::startLiveBand (float low, float high, bool monoFold, int order, bool compensateBand)
{
    if (high < low)
        std::swap (low, high);

    requestedLo.store (juce::jlimit (20.0f, 20000.0f, low), std::memory_order_relaxed);
    requestedHi.store (juce::jlimit (20.0f, 20000.0f, high), std::memory_order_relaxed);
    requestedMono.store (monoFold, std::memory_order_relaxed);
    requestedOrder.store (order >= 4 ? 4 : 2, std::memory_order_relaxed);
    requestedCompensate.store (compensateBand, std::memory_order_relaxed);
    mode.store ((int) Mode::live, std::memory_order_relaxed);
    active.store (false, std::memory_order_release);
    pendingStart.store (true, std::memory_order_release);
}

void AuditionEngine::stop()
{
    active.store (false, std::memory_order_release);
    pendingStart.store (false, std::memory_order_release);
    mode.store ((int) Mode::off, std::memory_order_release);
}

void AuditionEngine::process (juce::AudioBuffer<float>& buffer)
{
    const int n = buffer.getNumSamples();

    if (n <= 0 || ringLength <= 0)
        return;

    const float* inL = buffer.getReadPointer (0);
    const float* inR = buffer.getNumChannels() > 1 ? buffer.getReadPointer (1) : inL;

    // Capture in contiguous spans; the default bypass path used to perform
    // a variable integer division for every incoming sample.
    for (int offset = 0; offset < n;)
    {
        const int count = juce::jmin (n - offset, ringLength - writePos);
        std::copy_n (inL + offset, count, left.data() + writePos);
        std::copy_n (inR + offset, count, right.data() + writePos);
        writePos += count;
        if (writePos == ringLength)
            writePos = 0;
        offset += count;
    }
    totalSamples += n;

    if (pendingStart.exchange (false, std::memory_order_acq_rel))
    {
        const float lo = requestedLo.load (std::memory_order_relaxed);
        const float hi = requestedHi.load (std::memory_order_relaxed);
        const bool fullBand = lo <= 25.0f && hi >= 19000.0f;
        const bool wantFourth = requestedOrder.load (std::memory_order_relaxed) >= 4 && ! fullBand;
        const bool wantCompensate = requestedCompensate.load (std::memory_order_relaxed);

        Biquad tHpL, tHpR, tLpL, tLpR;
        Biquad t2HpL, t2HpR, t2LpL, t2LpR;

        if (! fullBand)
        {
            // 2nd order keeps the original signal-card slope. 4th order cascades
            // two Butterworth stages (Q 0.5412 / 1.3066) for 24 dB/oct.
            tHpL.setHighPass (sampleRate, lo, wantFourth ? 0.5412 : 0.7071);
            tHpR = tHpL;
            tLpL.setLowPass (sampleRate, hi, wantFourth ? 0.5412 : 0.7071);
            tLpR = tLpL;

            if (wantFourth)
            {
                t2HpL.setHighPass (sampleRate, lo, 1.3066);
                t2HpR = t2HpL;
                t2LpL.setLowPass (sampleRate, hi, 1.3066);
                t2LpR = t2LpL;
            }
        }

        if (mode.load (std::memory_order_relaxed) == (int) Mode::live)
        {
            hpL = tHpL;
            hpR = tHpR;
            lpL = tLpL;
            lpR = tLpR;
            hp2L = t2HpL;
            hp2R = t2HpR;
            lp2L = t2LpL;
            lp2R = t2LpR;
            fourthOrder = wantFourth;
            compensate = wantCompensate;
            loopStart = loopLength = loopPos = 0;
            liveFade = 0;
            gain = 1.0f;
            active.store (true, std::memory_order_release);
        }
        else
        {
        const double now = (double) totalSamples / sampleRate;
        double startSec = requestedStart.load (std::memory_order_relaxed);
        double endSec = requestedEnd.load (std::memory_order_relaxed);

        if (endSec <= startSec)
            endSec = startSec + 1.5;

        if (endSec - startSec > 2.0)
            endSec = startSec + 2.0;

        const double oldest = now - (double) ringLength / sampleRate;

        if (startSec < oldest || startSec > now - 0.05)
        {
            // Do not substitute a different slice. The editor polls this flag and
            // shows "outside the 12 s history, replay from this position".
            rangeRejected.store (1, std::memory_order_release);
            rejectedStart.store (startSec, std::memory_order_relaxed);
            active.store (false, std::memory_order_release);
            pendingStart.store (false, std::memory_order_release);
            mode.store ((int) Mode::off, std::memory_order_release);
            return;
        }

        const double span = juce::jlimit (0.2, 2.0, endSec - startSec);
        endSec = startSec + span;

        const int startOffset = juce::jlimit (0, ringLength - 64,
            (int) std::round ((now - startSec) * sampleRate));
        const int length = juce::jlimit (64, ringLength,
            (int) std::round ((endSec - startSec) * sampleRate));

        loopStart = (writePos - startOffset + ringLength) % ringLength;
        loopLength = juce::jmin (length, ringLength);
        loopPos = 0;

        double fullPower = 0.0, bandPower = 0.0;
        float bandPeak = 0.0f;
        const bool monoFold = requestedMono.load (std::memory_order_relaxed);

        for (int i = 0; i < loopLength; ++i)
        {
            const int idx = (loopStart + i) % ringLength;
            const float l = left[(size_t) idx];
            const float r = right[(size_t) idx];

            float fl = l, fr = r;

            if (! fullBand)
            {
                fl = tLpL.process (tHpL.process (fl));
                fr = tLpR.process (tHpR.process (fr));
            }

            if (monoFold)
            {
                const float full = 0.5f * (l + r);
                const float band = 0.5f * (fl + fr);
                fullPower += (double) full * full;
                bandPower += (double) band * band;
                bandPeak = juce::jmax (bandPeak, std::abs (band));
            }
            else
            {
                fullPower += 0.5 * ((double) l * l + (double) r * r);
                bandPower += 0.5 * ((double) fl * fl + (double) fr * fr);
                bandPeak = juce::jmax (bandPeak, juce::jmax (std::abs (fl), std::abs (fr)));
            }
        }

        const double fullRms = std::sqrt (fullPower / juce::jmax (1, loopLength));
        const double bandRms = std::sqrt (bandPower / juce::jmax (1, loopLength));

        gain = 1.0f;

        if (bandRms > 1.0e-7)
            gain = (float) juce::jlimit (0.20, 16.0, fullRms / bandRms);

        if (bandPeak > 1.0e-6f)
            gain = juce::jmin (gain, 0.96f / bandPeak);

        gain = juce::jlimit (0.05f, 16.0f, gain);

        hpL = tHpL;
        hpR = tHpR;
        lpL = tLpL;
        lpR = tLpR;
        fourthOrder = false;
        compensate = true;
        active.store (true, std::memory_order_release);
        }
    }

    if (! active.load (std::memory_order_acquire))
        return;

    auto* outL = buffer.getWritePointer (0);
    auto* outR = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    const bool monoFold = requestedMono.load (std::memory_order_relaxed);

    if (mode.load (std::memory_order_relaxed) == (int) Mode::live)
    {
        float inputPeak = 0.0f;

        for (int i = 0; i < n; ++i)
            inputPeak = juce::jmax (inputPeak, juce::jmax (std::abs (inL[i]), std::abs (inR[i])));

        if (inputPeak < 1.0e-6f)
        {
            hpL.reset(); hpR.reset(); lpL.reset(); lpR.reset();
            hp2L.reset(); hp2R.reset(); lp2L.reset(); lp2R.reset();
            gain = 1.0f;
            buffer.clear();
            return;
        }

        double fullPower = 0.0, bandPower = 0.0;
        float bandPeak = 0.0f;

        for (int i = 0; i < n; ++i)
        {
            // Read both channels before writing: a mono buffer aliases inL/outL.
            const float l = inL[i];
            const float r = inR[i];

            float fl = hpL.process (l);
            float fr = hpR.process (r);

            if (fourthOrder)
            {
                fl = hp2L.process (fl);
                fr = hp2R.process (fr);
            }

            fl = lpL.process (fl);
            fr = lpR.process (fr);

            if (fourthOrder)
            {
                fl = lp2L.process (fl);
                fr = lp2R.process (fr);
            }

            if (monoFold)
            {
                const float full = 0.5f * (l + r);
                const float band = 0.5f * (fl + fr);
                fullPower += (double) full * full;
                bandPower += (double) band * band;
                bandPeak = juce::jmax (bandPeak, std::abs (band));
                fl = fr = band;
            }
            else
            {
                fullPower += 0.5 * ((double) l * l + (double) r * r);
                bandPower += 0.5 * ((double) fl * fl + (double) fr * fr);
                bandPeak = juce::jmax (bandPeak, juce::jmax (std::abs (fl), std::abs (fr)));
            }

            const float fade = juce::jlimit (0.0f, 1.0f,
                (float) (liveFade + i + 1) / (float) fadeSamples);
            const float f = fade * gain;

            outL[i] = juce::jlimit (-1.0f, 1.0f, fl * f);

            if (outR != nullptr)
                outR[i] = juce::jlimit (-1.0f, 1.0f, fr * f);
        }

        liveFade = juce::jmin (fadeSamples, liveFade + n);

        const double fullRms = std::sqrt (fullPower / juce::jmax (1, n));
        const double bandRms = std::sqrt (bandPower / juce::jmax (1, n));

        float target = 1.0f;

        if (compensate)
        {
            if (bandRms > 1.0e-7)
                target = (float) juce::jlimit (0.25, 8.0, fullRms / bandRms);

            if (bandPeak > 1.0e-6f)
                target = juce::jmin (target, 0.96f / bandPeak);
        }
        else if (bandPeak > 1.0e-6f)
        {
            // Band solo keeps the real band level; only guard against overload.
            target = juce::jmin (1.0f, 0.96f / bandPeak);
        }

        gain += (juce::jlimit (0.05f, 8.0f, target) - gain) * 0.20f;

        for (int ch = 2; ch < buffer.getNumChannels(); ++ch)
            buffer.clear (ch, 0, n);

        return;
    }

    if (loopLength <= 0)
        return;

    for (int i = 0; i < n; ++i)
    {
        const int idx = (loopStart + loopPos) % ringLength;
        float l = left[(size_t) idx];
        float r = right[(size_t) idx];

        l = lpL.process (hpL.process (l));
        r = lpR.process (hpR.process (r));

        if (monoFold)
            l = r = 0.5f * (l + r);

        const float fadeIn = juce::jlimit (0.0f, 1.0f, (float) loopPos / (float) fadeSamples);
        const float fadeOut = juce::jlimit (0.0f, 1.0f, (float) (loopLength - loopPos) / (float) fadeSamples);
        const float f = juce::jmin (fadeIn, fadeOut) * gain;

        outL[i] = juce::jlimit (-1.0f, 1.0f, l * f);

        if (outR != nullptr)
            outR[i] = juce::jlimit (-1.0f, 1.0f, r * f);

        if (++loopPos >= loopLength)
            loopPos = 0;
    }

    for (int ch = 2; ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, n);
}
} // namespace ml
