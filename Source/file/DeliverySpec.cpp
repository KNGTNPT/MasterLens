#include "DeliverySpec.h"

namespace ml
{
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
        s.enabled = (bool) o->getProperty ("enabled");
        s.targetLufs = (float) (double) o->getProperty ("targetLufs");
        s.ceilingDbtp = (float) (double) o->getProperty ("ceilingDbtp");
        s.loudnessToleranceLu = (float) (double) o->getProperty ("loudnessToleranceLu");
        s.sampleRate = (int) o->getProperty ("sampleRate");
        s.bitDepth = (int) o->getProperty ("bitDepth");
        s.channels = (int) o->getProperty ("channels");
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
    if (! s.enabled || ! r.ok)
        return c;

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
