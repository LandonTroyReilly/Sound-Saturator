#include "PluginProcessor.h"
#include "PluginEditor.h"

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

juce::AudioProcessorEditor* SoundSaturatorAudioProcessor::createEditor()
{
    return new SoundSaturatorAudioProcessorEditor (*this);
}

void SoundSaturatorAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree state = apvts.copyState();
    state.setProperty ("stateVersion", ids::stateVersion, nullptr);

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
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
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
