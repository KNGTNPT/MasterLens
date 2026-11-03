#include "DeliverySpec.h"
#include <cmath>

namespace ml
{
bool DeliverySpec::validate (juce::String& error) const
{
    if (! std::isfinite (targetLufs) || targetLufs < -70.0f || targetLufs > 0.0f
        || ! std::isfinite (ceilingDbtp) || ceilingDbtp < -24.0f || ceilingDbtp > 0.0f
        || ! std::isfinite (loudnessToleranceLu) || loudnessToleranceLu < 0.0f || loudnessToleranceLu > 24.0f
        || (sampleRate != 0 && (sampleRate < 8000 || sampleRate > 384000))
        || (bitDepth != 0 && bitDepth != 8 && bitDepth != 16 && bitDepth != 24 && bitDepth != 32)
        || channels < 0 || channels > 2)
    { error = "invalid delivery spec type/range"; return false; }
    return true;
}

bool DeliverySpec::parse (const juce::var& value, DeliverySpec& result, juce::String& error)
{
    auto* object = value.getDynamicObject();
    if (! object) { error = "spec must be a JSON object"; return false; }
    for (const auto* key : { "targetLufs", "ceilingDbtp", "loudnessToleranceLu", "sampleRate", "bitDepth", "channels" })
        if (object->hasProperty (key))
        {
            const auto v = object->getProperty (key);
            if (! (v.isInt() || v.isInt64() || v.isDouble()))
            { error = juce::String ("numeric field required: ") + key; return false; }
            if (! std::isfinite ((double) v) || std::abs ((double) v) > 1000000.0)
            { error = "nonfinite or excessive delivery field"; return false; }
            if ((juce::String (key) == "sampleRate" || juce::String (key) == "bitDepth" || juce::String (key) == "channels")
                && (double) v != std::floor ((double) v))
            { error = "integer delivery field required"; return false; }
        }
    if (object->hasProperty ("namingPattern") && ! object->getProperty ("namingPattern").isString())
    { error = "namingPattern must be text"; return false; }
    result = fromVar (value);
    return result.validate (error);
}

juce::var DeliverySpec::toVar() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("enabled", enabled);
    o->setProperty ("targetLufs", targetLufs);
    o->setProperty ("ceilingDbtp", ceilingDbtp);
    o->setProperty ("loudnessToleranceLu", loudnessToleranceLu);
    o->setProperty ("sampleRate", sampleRate);
    o->setProperty ("bitDepth", bitDepth);
    o->setProperty ("channels", channels);
    o->setProperty ("namingPattern", namingPattern);
    return juce::var (o);
}

DeliverySpec DeliverySpec::fromVar (const juce::var& value)
{
    DeliverySpec s;
    if (auto* o = value.getDynamicObject())
    {
        if (o->hasProperty ("enabled")) s.enabled = (bool) o->getProperty ("enabled");
        if (o->hasProperty ("targetLufs")) s.targetLufs = (float) (double) o->getProperty ("targetLufs");
        if (o->hasProperty ("ceilingDbtp")) s.ceilingDbtp = (float) (double) o->getProperty ("ceilingDbtp");
        if (o->hasProperty ("loudnessToleranceLu")) s.loudnessToleranceLu = (float) (double) o->getProperty ("loudnessToleranceLu");
        if (o->hasProperty ("sampleRate")) s.sampleRate = (int) o->getProperty ("sampleRate");
        if (o->hasProperty ("bitDepth")) s.bitDepth = (int) o->getProperty ("bitDepth");
        if (o->hasProperty ("channels")) s.channels = (int) o->getProperty ("channels");
        s.namingPattern = o->getProperty ("namingPattern").toString();
    }
    return s;
}

juce::var SpecCheck::toVar() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("ok", ok);
    juce::Array<juce::var> list;
    for (const auto& f : failures)
        list.add (f);
    o->setProperty ("failures", list);
    return juce::var (o);
}

SpecCheck evaluateSpec (const FileAnalysisResult& r, const DeliverySpec& s)
{
    SpecCheck c;
    if (! s.enabled) return c;
    if (! r.ok || r.stats.integrated <= -70.0f)
    { c.ok = false; c.failures.add ("no valid measurable programme"); return c; }

    const auto fail = [&c] (const juce::String& text)
    {
        c.ok = false;
        c.failures.add (text);
    };

    if (s.sampleRate > 0 && std::abs (r.sampleRate - (double) s.sampleRate) > 0.5)
        fail (juce::String ("sample rate ") + juce::String (r.sampleRate, 0)
              + " != " + juce::String (s.sampleRate));

    if (s.bitDepth > 0 && r.bitDepth != s.bitDepth)
        fail (juce::String ("bit depth ") + juce::String (r.bitDepth)
              + " != " + juce::String (s.bitDepth));

    if (s.channels > 0 && r.numChannels != s.channels)
        fail (juce::String ("channels ") + juce::String (r.numChannels)
              + " != " + juce::String (s.channels));

    if (r.stats.integrated > -70.0f
        && std::abs (r.stats.integrated - s.targetLufs) > s.loudnessToleranceLu)
        fail (juce::String ("loudness ") + juce::String (r.stats.integrated, 2)
              + " LUFS, target " + juce::String (s.targetLufs, 2)
              + " +/- " + juce::String (s.loudnessToleranceLu, 2));

    if (r.stats.truePeakDb > s.ceilingDbtp + 0.05f)
        fail (juce::String ("true peak ") + juce::String (r.stats.truePeakDb, 2)
              + " dBTP > " + juce::String (s.ceilingDbtp, 2));

    if (s.namingPattern.isNotEmpty())
    {
        auto expected = s.namingPattern;
        const auto file = juce::File (r.fileName);
        const auto base = file.getFileNameWithoutExtension();
        const auto ext = file.getFileExtension().trimCharactersAtStart (".");
        expected = expected.replace ("{name}", base).replace ("{ext}", ext);
        if (! r.fileName.equalsIgnoreCase (expected))
            fail (juce::String ("naming ") + r.fileName + " != " + expected);
    }

    return c;
}
} // namespace ml
