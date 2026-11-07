#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "dsp/FactoryPresets.h"

#include <cmath>

SoundSaturatorAudioProcessor::SoundSaturatorAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

void SoundSaturatorAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const int numChannels = juce::jmax (1, getTotalNumInputChannels());
    preparedBlockSize = juce::jmax (1, samplesPerBlock);
    chain.prepare (sampleRate, preparedBlockSize, numChannels);

    if (! appliedDefaultQuality && ! stateRestored)
    {
        appliedDefaultQuality = true;
        const int recommended = chain.recommendDefaultQuality();
        if (auto* parameter = apvts.getParameter (ids::quality))
            parameter->setValue (parameter->convertTo0to1 ((float) recommended));
    }

    chain.selectQuality (readChoice (ids::quality, 2));
    setLatencySamples (chain.currentLatency());
    prepared = true;
}

void SoundSaturatorAudioProcessor::releaseResources() {}

void SoundSaturatorAudioProcessor::reset()
{
    chain.reset();
}

bool SoundSaturatorAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto input = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();

    if (input != output)
        return false;

    return input == juce::AudioChannelSet::mono() || input == juce::AudioChannelSet::stereo();
}

void SoundSaturatorAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (! prepared)
    {
        buffer.clear();
        return;
    }

    if (buffer.getNumSamples() == 0)
        return;

    for (int channel = getTotalNumInputChannels(); channel < getTotalNumOutputChannels(); ++channel)
        buffer.clear (channel, 0, buffer.getNumSamples());

    const ParameterSnapshot snapshot = captureSnapshot();
    const ChainMeters meters { &gainReductionDb, &outputPeakLinear };
    const int total = buffer.getNumSamples();

    for (int offset = 0; offset < total; offset += preparedBlockSize)
    {
        const int count = juce::jmin (preparedBlockSize, total - offset);
        chain.process (buffer, offset, count, snapshot, meters);
    }

    const int latency = chain.currentLatency();
    if (latency != getLatencySamples())
        setLatencySamples (latency);
}

int SoundSaturatorAudioProcessor::getNumPrograms()
{
    return kFactoryPresetCount;
}

int SoundSaturatorAudioProcessor::getCurrentProgram()
{
    return currentProgram;
}

void SoundSaturatorAudioProcessor::setCurrentProgram (int index)
{
    applyPreset (index);
}

const juce::String SoundSaturatorAudioProcessor::getProgramName (int index)
{
    if (index < 0 || index >= kFactoryPresetCount)
        return {};

    return kFactoryPresets[index].name;
}

void SoundSaturatorAudioProcessor::applyPreset (int index)
{
    if (index < 0 || index >= kFactoryPresetCount)
        return;

    currentProgram = index;
    const FactoryPreset& preset = kFactoryPresets[index];

    auto set = [this] (const char* parameterId, float raw)
    {
        if (auto* parameter = apvts.getParameter (parameterId))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (raw));
    };

    set (ids::inputTrim, preset.inputTrimDb);
    set (ids::vcaThreshold, preset.vcaThresholdDb);
    set (ids::vcaRatio, (float) preset.vcaRatio);
    set (ids::vcaTiming, (float) preset.vcaTiming);
    set (ids::vcaKnee, preset.vcaKneeDb);
    set (ids::vcaDetector, (float) preset.vcaDetector);
    set (ids::vcaScHpf, preset.vcaScHpf ? 1.0f : 0.0f);
    set (ids::vcaScHpfHz, preset.vcaScHpfHz);
    set (ids::vcaMakeup, preset.vcaMakeupDb);
    set (ids::vcaBypass, preset.vcaBypass ? 1.0f : 0.0f);
    set (ids::fluxMode, (float) preset.fluxMode);
    set (ids::fluxIntensity, preset.fluxIntensity);
    set (ids::magneticDrive, preset.magneticDriveDb);
    set (ids::attackPreserve, preset.attackPreserve);
    set (ids::fluxBypass, preset.fluxBypass ? 1.0f : 0.0f);
    set (ids::tapeDrive, preset.tapeDriveDb);
    set (ids::tapeBody, preset.tapeBodyDb);
    set (ids::tapeReturn, preset.tapeReturnDb);
    set (ids::tapeBias, (float) preset.tapeBias);
    set (ids::tapeBypass, preset.tapeBypass ? 1.0f : 0.0f);
    set (ids::transformerDrive, preset.transformerDriveDb);
    set (ids::transformerWeight, preset.transformerWeightDb);
    set (ids::transformerBypass, preset.transformerBypass ? 1.0f : 0.0f);
    set (ids::vcaInput, 0.0f);
    set (ids::vcaOutput, 0.0f);
    set (ids::fluxInput, 0.0f);
    set (ids::fluxOutput, 0.0f);
    set (ids::tapeInput, 0.0f);
    set (ids::tapeOutput, 0.0f);
    set (ids::transformerInput, 0.0f);
    set (ids::transformerOutput, 0.0f);
    set (ids::mix, preset.mixPercent);
    set (ids::finalOutput, preset.finalOutputDb);
    set (ids::globalBypass, 0.0f);

    updateHostDisplay (juce::AudioProcessorListener::ChangeDetails().withProgramChanged (true));
}

juce::AudioProcessorEditor* SoundSaturatorAudioProcessor::createEditor()
{
    return new SoundSaturatorAudioProcessorEditor (*this);
}

void SoundSaturatorAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree state = apvts.copyState();
    state.setProperty ("stateVersion", ids::stateVersion, nullptr);
    state.setProperty ("program", currentProgram, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void SoundSaturatorAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            stateRestored = true;
            auto restored = juce::ValueTree::fromXml (*xml);
            apvts.replaceState (restored);
            currentProgram = juce::jlimit (0, kFactoryPresetCount - 1, (int) restored.getProperty ("program", 0));
        }
    }
}

ParameterSnapshot SoundSaturatorAudioProcessor::captureSnapshot() const
{
    ParameterSnapshot snapshot;
    snapshot.inputTrimDb = readRaw (ids::inputTrim);
    snapshot.vcaThresholdDb = readRaw (ids::vcaThreshold);
    snapshot.vcaRatioIndex = readChoice (ids::vcaRatio, 4);
    snapshot.vcaTimingIndex = readChoice (ids::vcaTiming, 9);
    snapshot.vcaKneeDb = readRaw (ids::vcaKnee);
    snapshot.vcaDetectorIndex = readChoice (ids::vcaDetector, 1);
    snapshot.vcaScHpf = readFlag (ids::vcaScHpf);
    snapshot.vcaScHpfHz = readRaw (ids::vcaScHpfHz);
    snapshot.vcaMakeupDb = readRaw (ids::vcaMakeup);
    snapshot.vcaBypass = readFlag (ids::vcaBypass);
    snapshot.fluxMode = readChoice (ids::fluxMode, 2);
    snapshot.fluxIntensity = readRaw (ids::fluxIntensity);
    snapshot.magneticDriveDb = readRaw (ids::magneticDrive);
    snapshot.attackPreserve = readRaw (ids::attackPreserve);
    snapshot.fluxBypass = readFlag (ids::fluxBypass);
    snapshot.tapeDriveDb = readRaw (ids::tapeDrive);
    snapshot.tapeBodyDb = readRaw (ids::tapeBody);
    snapshot.tapeReturnDb = readRaw (ids::tapeReturn);
    snapshot.tapeBias = readChoice (ids::tapeBias, 2);
    snapshot.tapeBypass = readFlag (ids::tapeBypass);
    snapshot.transformerDriveDb = readRaw (ids::transformerDrive);
    snapshot.transformerWeightDb = readRaw (ids::transformerWeight);
    snapshot.transformerBypass = readFlag (ids::transformerBypass);
    snapshot.vcaInputDb = readRaw (ids::vcaInput);
    snapshot.vcaOutputDb = readRaw (ids::vcaOutput);
    snapshot.fluxInputDb = readRaw (ids::fluxInput);
    snapshot.fluxOutputDb = readRaw (ids::fluxOutput);
    snapshot.tapeInputDb = readRaw (ids::tapeInput);
    snapshot.tapeOutputDb = readRaw (ids::tapeOutput);
    snapshot.transformerInputDb = readRaw (ids::transformerInput);
    snapshot.transformerOutputDb = readRaw (ids::transformerOutput);
    snapshot.chainOrder = readChoice (ids::chainOrder, kChainOrderCount - 1);
    snapshot.mixPercent = readRaw (ids::mix);
    snapshot.finalOutputDb = readRaw (ids::finalOutput);
    snapshot.globalBypass = readFlag (ids::globalBypass);
    snapshot.qualityIndex = readChoice (ids::quality, 2);
    return snapshot;
}

float SoundSaturatorAudioProcessor::readRaw (const char* parameterId) const
{
    if (auto* value = apvts.getRawParameterValue (parameterId))
        return value->load();
    return 0.0f;
}

int SoundSaturatorAudioProcessor::readChoice (const char* parameterId, int maxIndex) const
{
    return juce::jlimit (0, maxIndex, (int) std::lround (readRaw (parameterId)));
}

bool SoundSaturatorAudioProcessor::readFlag (const char* parameterId) const
{
    return readRaw (parameterId) >= 0.5f;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SoundSaturatorAudioProcessor();
}
