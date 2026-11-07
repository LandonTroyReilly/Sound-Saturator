#pragma once

#include "PluginProcessor.h"

#include <memory>
#include <vector>

class SaturatorLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SaturatorLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPosProportional,
                           float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
};

class SoundSaturatorAudioProcessorEditor : public juce::AudioProcessorEditor,
                                           private juce::Timer
{
public:
    explicit SoundSaturatorAudioProcessorEditor (SoundSaturatorAudioProcessor&);
    ~SoundSaturatorAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    void timerCallback() override;
    void addKnob (juce::Slider& slider, juce::Label& label, const juce::String& text, const char* parameterId, juce::Colour accent);
    void addPower (juce::TextButton& button, const char* bypassId, juce::Colour accent, const juce::String& tip);
    void addMode (juce::TextButton& button, int index);
    void setBool (const char* parameterId, bool value);
    void setChoice (const char* parameterId, int index);
    bool syncToggle (juce::Button& button, bool shouldBeOn);
    void syncFromParameters();
    void placeKnobs (juce::Rectangle<int> area, std::initializer_list<std::pair<juce::Label*, juce::Slider*>> knobs);
    void paintCard (juce::Graphics&, juce::Rectangle<int> bounds, const juce::String& title, const juce::String& route,
                    juce::Colour accent, bool active, bool lifted);
    void paintMeter (juce::Graphics&, juce::Rectangle<int> bounds, const juce::String& name, float amount, const juce::String& value);
    void paintStage (juce::Graphics&, int stage);
    void readOrder();
    void layoutChain();
    void positionStage (int stage, juce::Rectangle<int> panel);
    void bringStageToFront (int stage);
    int readOrderIndex() const;
    int headerStage (juce::Point<int> position) const;
    juce::String routeText (int stage) const;

    SaturatorLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltipWindow;

    SoundSaturatorAudioProcessor& processor;

    juce::ComboBox presetBox { "Preset" };
    juce::TextButton globalBypass { "Bypass" };

    juce::Label inputTrimLabel, mixLabel, outputLabel;
    juce::Slider inputTrim, mix, output;

    juce::TextButton vcaPower, fluxPower, tapePower;
    juce::Label thresholdLabel, ratioLabel, makeupLabel;
    juce::Slider threshold, ratio, makeup;

    juce::TextButton fluxNickel { "Nickel" }, fluxCobalt { "Cobalt" }, fluxIron { "Iron" };
    juce::Label intensityLabel, driveLabel;
    juce::Slider intensity, magneticDrive;

    juce::Label tapeDriveLabel, bodyLabel;
    juce::Slider tapeDrive, tapeBody;
    juce::Label vcaInLabel, vcaOutLabel, fluxInLabel, fluxOutLabel, tapeInLabel, tapeOutLabel;
    juce::Slider vcaIn, vcaOut, fluxIn, fluxOut, tapeIn, tapeOut;

    juce::TextButton transformerPower;
    juce::Label transformerDriveLabel, transformerWeightLabel;
    juce::Slider transformerDrive, transformerWeight;
    juce::Label transformerInLabel, transformerOutLabel;
    juce::Slider transformerIn, transformerOut;

    juce::Rectangle<int> meterBounds, chainArea, slotRest[4], stageBounds[4];
    int order[4] = { stageVca, stageFlux, stageTape, stageTransformer };
    int visualOrder[4] = { stageVca, stageFlux, stageTape, stageTransformer };
    int orderIndex = 0;
    int draggingStage = -1;
    int dragGrab = 0;
    int dropIndex = 0;
    float shownReductionDb = 0.0f;
    float shownPeakDb = -100.0f;

    std::vector<std::unique_ptr<SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<ButtonAttachment>> buttonAttachments;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SoundSaturatorAudioProcessorEditor)
};
