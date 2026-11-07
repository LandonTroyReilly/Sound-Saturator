#pragma once

#include "dsp/SaturatorChain.h"

#include <juce_audio_processors/juce_audio_processors.h>

class SoundSaturatorAudioProcessor : public juce::AudioProcessor
{
public:
    SoundSaturatorAudioProcessor();
    ~SoundSaturatorAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Sound Saturator"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Init"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    OversamplingLatencies getOversamplingLatencies() const noexcept { return chain.latencies(); }

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float> gainReductionDb { 0.0f };
    std::atomic<float> outputPeakLinear { 0.0f };

private:
    ParameterSnapshot captureSnapshot() const;
    float readRaw (const char* parameterId) const;
    int readChoice (const char* parameterId, int maxIndex) const;
    bool readFlag (const char* parameterId) const;

    SaturatorChain chain;
    int preparedBlockSize = 512;
    bool prepared = false;
    bool stateRestored = false;
    bool appliedDefaultQuality = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SoundSaturatorAudioProcessor)
};
