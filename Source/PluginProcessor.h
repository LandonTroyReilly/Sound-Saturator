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

    const juce::String getName() const override { return "Bombap"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    OversamplingLatencies getOversamplingLatencies() const noexcept { return chain.latencies(); }
    int getUiZoomPercent() const noexcept { return uiZoomPercent; }
    void setUiZoomPercent (int percent) noexcept;

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float> gainReductionDb { 0.0f };
    std::atomic<float> outputPeakLinear { 0.0f };

private:
    ParameterSnapshot captureSnapshot() const;
    void applyPreset (int index);
    float readRaw (const char* parameterId) const;
    int readChoice (const char* parameterId, int maxIndex) const;
    bool readFlag (const char* parameterId) const;

    SaturatorChain chain;
    int currentProgram = 0;
    int uiZoomPercent = 75;
    int preparedBlockSize = 512;
    bool prepared = false;
    bool stateRestored = false;
    bool appliedDefaultQuality = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SoundSaturatorAudioProcessor)
};
