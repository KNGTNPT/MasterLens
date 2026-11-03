#include "PluginProcessor.h"
#include "PluginEditor.h"

#include "album/AlbumSession.h"

namespace
{
constexpr auto targetId = "targetLufs";
constexpr auto ceilingId = "tpCeiling";
constexpr auto smoothingId = "smoothing";
constexpr auto autoResetId = "autoResetOnStart";
constexpr auto autoScanId = "autoScanOnStop";
constexpr auto autoReportId = "autoReportOnStop";
constexpr auto commandId = "autoCommand";
constexpr auto monitorId = "monitorMatch";
} // namespace

//==============================================================================
MasterLensAudioProcessor::MasterLensAudioProcessor()
    : juce::AudioProcessor (BusesProperties()
                                .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    spectrumRing.prepare (32768);
}

juce::AudioProcessorValueTreeState::ParameterLayout MasterLensAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { targetId, 1 }, "Target Loudness",
        juce::NormalisableRange<float> (-36.0f, 0.0f, 0.1f), -14.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ceilingId, 1 }, "True Peak Ceiling",
        juce::NormalisableRange<float> (-12.0f, 0.0f, 0.1f), -1.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { smoothingId, 1 }, "Spectrum Smoothing",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.55f));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { autoResetId, 1 }, "Auto reset on transport start", false));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { autoScanId, 1 }, "Auto capture album on stop", false));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { autoReportId, 1 }, "Auto export proof on stop", false));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { monitorId, 1 }, "Loudness-matched monitor", false));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { commandId, 1 }, "Automation command",
        juce::StringArray { "None", "Reset", "Capture track", "Export proof", "Export markers", "Clear album" },
        0));

    return { params.begin(), params.end() };
}

//==============================================================================
void MasterLensAudioProcessor::resetHostCoverage() noexcept { coverageTracker.reset(); }
void MasterLensAudioProcessor::markHostCoverage (double start, double end) noexcept
{
    coverageTracker.add (start, end);
}

void MasterLensAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);

    currentSampleRate = sampleRate > 0.0 ? sampleRate : 48000.0;

    outputSpectrumFifo.prepare (currentSampleRate);

    engine.prepare (currentSampleRate, juce::jmax (1, getTotalNumInputChannels()));
    auditionEngine.prepare (currentSampleRate, 2);
    filePreviewPlayer.prepare (currentSampleRate);
    referencePlayer.prepare (currentSampleRate);
    ml::resetStats (stats);
    spectrumRing.reset();
    resetHostCoverage();
    monitorTargetLufs.store (getTargetLufs(), std::memory_order_relaxed);

    resetRequested.store (false);
}

bool MasterLensAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& mainIn = layouts.getMainInputChannelSet();
    const auto& mainOut = layouts.getMainOutputChannelSet();

    if (mainIn != mainOut)
        return false;

    return mainIn == juce::AudioChannelSet::mono()
        || mainIn == juce::AudioChannelSet::stereo();
}

void MasterLensAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    midi.clear();

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    if (numSamples <= 0 || numChannels <= 0)
        return;

    // Host automation: reset at transport start, queue capture / proof on stop.
    bool playing = false;
    bool hostValid = false;
    double hostPosition = 0.0;

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            playing = pos->getIsPlaying();

            if (auto t = pos->getTimeInSeconds(); t && std::isfinite (*t))
            {
                hostPosition = *t;
                hostValid = true;
            }
        }

    if (lastTransportPlaying < 0)
        lastTransportPlaying = playing ? 1 : 0;

    if (playing && lastTransportPlaying == 0 && getBool (autoResetId))
        resetRequested.store (true);

    if (! playing && lastTransportPlaying == 1)
    {
        if (getBool (autoScanId))
            pendingUiCommand.store (2, std::memory_order_release);

        if (getBool (autoReportId))
            pendingUiCommand.store (3, std::memory_order_release);
    }

    lastTransportPlaying = playing ? 1 : 0;
    hostTransportPlaying.store (playing, std::memory_order_relaxed);

    if (auto* p = apvts.getRawParameterValue (commandId))
    {
        const int command = (int) p->load();

        if (command > 0 && command != lastCommandValue)
        {
            if (command == 1)
                resetRequested.store (true);
            else
                pendingUiCommand.store (command, std::memory_order_release);
        }

        lastCommandValue = command;
    }

    if (resetRequested.exchange (false))
    {
        engine.reset();
        engine.setTimeOrigin (hostValid ? hostPosition : 0.0);
        ml::resetStats (stats);
        spectrumRing.reset();
        auditionEngine.reset();
        resetHostCoverage();
    }
    else if (hostValid && stats.elapsed.load() <= 0.0)
    {
        // First block after prepare: anchor every event timestamp to the host
        // playhead rather than to the plug-in's private sample counter.
        engine.setTimeOrigin (hostPosition);
    }

    // mono sum for the visualiser
    if (numChannels >= 2)
    {
        spectrumRing.writeStereoBlock (buffer.getReadPointer (0),
                                       buffer.getReadPointer (1), numSamples);
    }
    else
    {
        spectrumRing.writeBlock (buffer.getReadPointer (0), numSamples);
    }

    if (hostValid) engine.setTimeOrigin (hostPosition - stats.elapsed.load());
    engine.process (buffer, stats, numSamples);
    auditionEngine.process (buffer);

    // File QC evidence preview is an explicit, user-triggered output override.
    // Measurements above still see the untouched bus.
    const bool previewOutput = filePreviewPlayer.render (buffer);

    const double knownLength = trackLengthSeconds.load (std::memory_order_relaxed);
    stats.hostLength = knownLength;

    if (hostValid)
        stats.hostPosition = hostPosition;

    if (hostValid && playing)
        markHostCoverage (hostPosition, hostPosition + (double) numSamples / currentSampleRate);

    const double covered = hostValid ? coverageTracker.seconds (knownLength) : -1.0;
    float coverage = covered >= 0.0 ? (float) (100.0 * covered / knownLength) : -1.0f;
    if (covered >= 0.0 && covered < knownLength - 1.0e-9)
        coverage = std::nextafter (juce::jmin (100.0f, coverage), 0.0f);
    stats.coveragePct = coverage;
    if (previewOutput) { outputSpectrumFifo.pushMainOutput (buffer); return; }

    // Optional loudness-matched monitor. It is deliberately off by default and
    // only changes the output when the user explicitly engages it; analysis has
    // already consumed the untouched signal above.
    const bool monitor = getBool (monitorId);
    const float matchTarget = monitorTargetLufs.load (std::memory_order_relaxed);
    float inputTrimDb = 0.0f;

    if (monitor && stats.integrated.load() > -70.0f)
        inputTrimDb = juce::jlimit (-24.0f, 12.0f, matchTarget - stats.integrated.load());

    stats.monitorTrimDb = inputTrimDb;

    // The reference is already mastered. Matching the input to the reference
    // and then applying the input's trim to the reference would double-trim it;
    // give each path its own target trim instead.
    float referenceTrimDb = 0.0f;

    if (monitor && referencePlayer.isReady() && referencePlayer.getLoudness() > -70.0f)
        referenceTrimDb = juce::jlimit (-24.0f, 12.0f,
                                       matchTarget - referencePlayer.getLoudness());

    if (! referencePlayer.render (buffer, hostValid ? hostPosition : 0.0, playing, referenceTrimDb, inputTrimDb))
        if (std::abs (inputTrimDb) > 0.001f)
            buffer.applyGain (juce::Decibels::decibelsToGain (inputTrimDb));

    for (int ch = 2; ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    // Independent display tap of what the user actually hears. Existing input
    // loudness / true-peak / M-S analysis above is deliberately left untouched.
    outputSpectrumFifo.pushMainOutput (buffer);
}

//==============================================================================
float MasterLensAudioProcessor::getTargetLufs() const
{
    if (auto* p = apvts.getRawParameterValue (targetId))
        return p->load();

    return -14.0f;
}

float MasterLensAudioProcessor::getCeilingDb() const
{
    if (auto* p = apvts.getRawParameterValue (ceilingId))
        return p->load();

    return -1.0f;
}

float MasterLensAudioProcessor::getSmoothing() const
{
    if (auto* p = apvts.getRawParameterValue (smoothingId))
        return p->load();

    return 0.55f;
}

//==============================================================================
juce::AudioProcessorEditor* MasterLensAudioProcessor::createEditor()
{
    return new MasterLensAudioProcessorEditor (*this);
}

void MasterLensAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // The shared album.json stays the cross-instance "last used" cache. The
    // active album is also embedded in the project state so reopening a session
    // restores its own track list and gate result.
    apvts.state.setProperty ("albumState", albumSession.serializeState(), nullptr);

    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void MasterLensAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));

    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
    {
        apvts.replaceState (juce::ValueTree::fromXml (*xml));

        const auto albumState = apvts.state.getProperty ("albumState").toString();

        if (albumState.isNotEmpty())
            albumSession.restoreState (albumState);
    }
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MasterLensAudioProcessor();
}
