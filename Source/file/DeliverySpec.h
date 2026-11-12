#pragma once

#include <juce_core/juce_core.h>
#include "FileAnalyzer.h"

namespace ml
{
/** Client-facing delivery requirements. A zero sample-rate / bit-depth /
    channel count means "do not constrain this field". */
struct DeliverySpec
{
    bool enabled = false;
    float targetLufs = -14.0f;
    float ceilingDbtp = -1.0f;
    float loudnessToleranceLu = 0.3f;
    int sampleRate = 0;
    int bitDepth = 0;
    int channels = 0;
    juce::String namingPattern;

    bool validate (juce::String& error) const;
    static bool parse (const juce::var&, DeliverySpec&, juce::String& error);
    juce::var toVar() const;
    static DeliverySpec fromVar (const juce::var&);
};

struct SpecCheck
{
    bool ok = true;
    juce::StringArray failures;
    juce::var toVar() const;
};

SpecCheck evaluateSpec (const FileAnalysisResult&, const DeliverySpec&);
/** One resolver for GUI, CLI, CSV and JSON: an enabled client spec wins over
    the platform/manual target, which wins over the -14 LUFS default. */
struct EffectiveDelivery
{
    float targetLufs = -14.0f;
    float ceilingDbtp = -1.0f;
};

inline EffectiveDelivery effectiveDelivery (const DeliverySpec& spec, float targetLufs, float ceilingDbtp)
{
    if (spec.enabled)
        return { spec.targetLufs, spec.ceilingDbtp };

    return { targetLufs, ceilingDbtp };
}

} // namespace ml
