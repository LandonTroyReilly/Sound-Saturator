#pragma once

#include "PluginProcessor.h"

#include <memory>
#include <vector>

class SoundSaturatorAudioProcessorEditor : public juce::AudioProcessorEditor,
                                           private juce::Timer
{
public:
    explicit SoundSaturatorAudioProcessorEditor (SoundSaturatorAudioProcessor&);
    ~SoundSaturatorAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    void timerCallback() override;
    void configureSlider (juce::Slider& slider, juce::Label& label, const juce::String& text, const char* parameterId);
    void configureChoice (juce::ComboBox& box, juce::Label& label, const juce::String& text, const char* parameterId, const juce::StringArray& items);
    void configureToggle (juce::ToggleButton& button, const juce::String& text, const char* parameterId);
    void placeSlider (juce::Rectangle<int>& area, juce::Label& label, juce::Slider& slider);
    void placeChoice (juce::Rectangle<int>& area, juce::Label& label, juce::ComboBox& box);
    void placeToggle (juce::Rectangle<int>& area, juce::ToggleButton& button);

    SoundSaturatorAudioProcessor& processor;
    juce::GroupComponent globalGroup { {}, "Global" };
    juce::GroupComponent vcaGroup { {}, "VCA" };
    juce::GroupComponent fluxGroup { {}, "Flux" };
    juce::GroupComponent tapeGroup { {}, "Tape" };

    juce::Label inputTrimLabel, mixLabel, outputLabel, qualityLabel;
    juce::Slider inputTrim, mix, output;
    juce::ComboBox quality;
    juce::ToggleButton globalBypass;

    juce::Label thresholdLabel, ratioLabel, timingLabel, kneeLabel, detectorLabel, hpfLabel, makeupLabel;
    juce::Slider threshold, knee, hpfHz, makeup;
    juce::ComboBox ratio, timing, detector;
    juce::ToggleButton sidechainHpf, vcaBypass;

    juce::Label modeLabel, intensityLabel, driveLabel, preserveLabel;
    juce::Slider intensity, magneticDrive, attackPreserve;
    juce::ComboBox fluxMode;
    juce::ToggleButton fluxBypass;

    juce::Label tapeDriveLabel, bodyLabel, returnLabel, biasLabel;
    juce::Slider tapeDrive, tapeBody, tapeReturn;
    juce::ComboBox tapeBias;
    juce::ToggleButton tapeBypass;

    juce::Rectangle<int> meterBounds;
    float shownReductionDb = 0.0f;
    float shownPeakDb = -100.0f;

    std::vector<std::unique_ptr<SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<ComboAttachment>> comboAttachments;
    std::vector<std::unique_ptr<ButtonAttachment>> buttonAttachments;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SoundSaturatorAudioProcessorEditor)
};
