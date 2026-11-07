#include "PluginEditor.h"

#include <algorithm>
#include <cmath>

SoundSaturatorAudioProcessorEditor::SoundSaturatorAudioProcessorEditor (SoundSaturatorAudioProcessor& processorIn)
    : juce::AudioProcessorEditor (processorIn),
      processor (processorIn)
{
    addAndMakeVisible (globalGroup);
    addAndMakeVisible (vcaGroup);
    addAndMakeVisible (fluxGroup);
    addAndMakeVisible (tapeGroup);

    configureSlider (inputTrim, inputTrimLabel, "Input", ids::inputTrim);
    configureSlider (mix, mixLabel, "Mix", ids::mix);
    configureSlider (output, outputLabel, "Output", ids::finalOutput);
    configureChoice (quality, qualityLabel, "Quality", ids::quality, { "2x", "4x", "8x" });
    configureToggle (globalBypass, "Bypass", ids::globalBypass);

    configureSlider (threshold, thresholdLabel, "Threshold", ids::vcaThreshold);
    configureChoice (ratio, ratioLabel, "Ratio", ids::vcaRatio, { "1:1", "2:1", "4:1", "8:1", "Limiter" });
    juce::StringArray timingChoices;
    for (int i = 1; i <= 10; ++i)
        timingChoices.add (juce::String (i));
    configureChoice (timing, timingLabel, "Timing", ids::vcaTiming, timingChoices);
    configureSlider (knee, kneeLabel, "Knee", ids::vcaKnee);
    configureChoice (detector, detectorLabel, "Detector", ids::vcaDetector, { "Peak", "RMS" });
    configureToggle (sidechainHpf, "Sidechain HPF", ids::vcaScHpf);
    configureSlider (hpfHz, hpfLabel, "HPF Hz", ids::vcaScHpfHz);
    configureSlider (makeup, makeupLabel, "Makeup", ids::vcaMakeup);
    configureToggle (vcaBypass, "VCA Bypass", ids::vcaBypass);

    configureChoice (fluxMode, modeLabel, "Mode", ids::fluxMode, { "Nickel", "Cobalt", "Iron" });
    configureSlider (intensity, intensityLabel, "Intensity", ids::fluxIntensity);
    configureSlider (magneticDrive, driveLabel, "Drive", ids::magneticDrive);
    configureSlider (attackPreserve, preserveLabel, "Preserve", ids::attackPreserve);
    configureToggle (fluxBypass, "Flux Bypass", ids::fluxBypass);

    configureSlider (tapeDrive, tapeDriveLabel, "Drive", ids::tapeDrive);
    configureSlider (tapeBody, bodyLabel, "Body", ids::tapeBody);
    configureSlider (tapeReturn, returnLabel, "Return", ids::tapeReturn);
    configureChoice (tapeBias, biasLabel, "Bias", ids::tapeBias, { "Low", "Nominal", "High" });
    configureToggle (tapeBypass, "Tape Bypass", ids::tapeBypass);

    setSize (1040, 640);
    setResizable (true, true);
    setResizeLimits (900, 560, 1600, 1000);
    startTimerHz (30);
}

SoundSaturatorAudioProcessorEditor::~SoundSaturatorAudioProcessorEditor()
{
    stopTimer();
}

void SoundSaturatorAudioProcessorEditor::configureSlider (juce::Slider& slider, juce::Label& label, const juce::String& text, const char* parameterId)
{
    slider.setSliderStyle (juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 72, 20);
    addAndMakeVisible (slider);
    label.setText (text, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (label);
    sliderAttachments.push_back (std::make_unique<SliderAttachment> (processor.apvts, parameterId, slider));
}

void SoundSaturatorAudioProcessorEditor::configureChoice (juce::ComboBox& box, juce::Label& label, const juce::String& text, const char* parameterId, const juce::StringArray& items)
{
    box.addItemList (items, 1);
    addAndMakeVisible (box);
    label.setText (text, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (label);
    comboAttachments.push_back (std::make_unique<ComboAttachment> (processor.apvts, parameterId, box));
}

void SoundSaturatorAudioProcessorEditor::configureToggle (juce::ToggleButton& button, const juce::String& text, const char* parameterId)
{
    button.setButtonText (text);
    addAndMakeVisible (button);
    buttonAttachments.push_back (std::make_unique<ButtonAttachment> (processor.apvts, parameterId, button));
}

void SoundSaturatorAudioProcessorEditor::placeSlider (juce::Rectangle<int>& area, juce::Label& label, juce::Slider& slider)
{
    auto row = area.removeFromTop (26);
    label.setBounds (row.removeFromLeft (88));
    slider.setBounds (row);
    area.removeFromTop (4);
}

void SoundSaturatorAudioProcessorEditor::placeChoice (juce::Rectangle<int>& area, juce::Label& label, juce::ComboBox& box)
{
    auto row = area.removeFromTop (26);
    label.setBounds (row.removeFromLeft (88));
    box.setBounds (row);
    area.removeFromTop (4);
}

void SoundSaturatorAudioProcessorEditor::placeToggle (juce::Rectangle<int>& area, juce::ToggleButton& button)
{
    button.setBounds (area.removeFromTop (26));
    area.removeFromTop (4);
}

void SoundSaturatorAudioProcessorEditor::paint (juce::Graphics& graphics)
{
    graphics.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    graphics.setColour (juce::Colours::white);
    graphics.setFont (juce::FontOptions (18.0f));
    graphics.drawText ("Sound Saturator", 16, 8, 280, 24, juce::Justification::centredLeft);

    auto meters = meterBounds.reduced (8);
    auto reductionMeter = meters.removeFromLeft (meters.getWidth() / 2).reduced (4, 0);
    auto peakMeter = meters.reduced (4, 0);

    auto drawMeter = [&] (juce::Rectangle<int> bounds, const juce::String& name, float amount, const juce::String& value)
    {
        graphics.setColour (juce::Colours::white.withAlpha (0.85f));
        graphics.setFont (juce::FontOptions (12.0f));
        graphics.drawText (name, bounds.removeFromTop (16), juce::Justification::centred);
        auto bar = bounds.removeFromTop (bounds.getHeight() - 18);
        graphics.setColour (juce::Colours::black.withAlpha (0.35f));
        graphics.fillRect (bar);
        graphics.setColour (juce::Colours::orange);
        const int filled = (int) std::round (bar.getHeight() * std::clamp (amount, 0.0f, 1.0f));
        graphics.fillRect (bar.withTop (bar.getBottom() - filled).withHeight (filled));
        graphics.setColour (juce::Colours::white);
        graphics.drawText (value, bounds, juce::Justification::centred);
    };

    const float reductionAmount = juce::jlimit (0.0f, 1.0f, shownReductionDb / 24.0f);
    const float peakAmount = juce::jmap (juce::jlimit (-60.0f, 6.0f, shownPeakDb), -60.0f, 6.0f, 0.0f, 1.0f);
    drawMeter (reductionMeter, "GR", reductionAmount, juce::String (shownReductionDb, 1) + " dB");
    drawMeter (peakMeter, "Peak", peakAmount, juce::String (shownPeakDb, 1) + " dB");
}

void SoundSaturatorAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (12);
    area.removeFromTop (28);
    meterBounds = area.removeFromRight (170);
    area.removeFromRight (8);

    auto global = area.removeFromTop (118);
    area.removeFromTop (8);
    const int columnWidth = area.getWidth() / 3;
    auto vca = area.removeFromLeft (columnWidth);
    auto flux = area.removeFromLeft (columnWidth);
    auto tape = area;

    globalGroup.setBounds (global);
    vcaGroup.setBounds (vca);
    fluxGroup.setBounds (flux);
    tapeGroup.setBounds (tape);

    global = global.reduced (10, 22);
    vca = vca.reduced (10, 22);
    flux = flux.reduced (10, 22);
    tape = tape.reduced (10, 22);

    auto globalTop = global.removeFromTop (30);
    globalBypass.setBounds (globalTop.removeFromLeft (110));
    qualityLabel.setBounds (globalTop.removeFromLeft (70));
    quality.setBounds (globalTop.removeFromLeft (120));
    placeSlider (global, inputTrimLabel, inputTrim);
    placeSlider (global, mixLabel, mix);
    placeSlider (global, outputLabel, output);

    placeSlider (vca, thresholdLabel, threshold);
    placeChoice (vca, ratioLabel, ratio);
    placeChoice (vca, timingLabel, timing);
    placeSlider (vca, kneeLabel, knee);
    placeChoice (vca, detectorLabel, detector);
    placeToggle (vca, sidechainHpf);
    placeSlider (vca, hpfLabel, hpfHz);
    placeSlider (vca, makeupLabel, makeup);
    placeToggle (vca, vcaBypass);

    placeChoice (flux, modeLabel, fluxMode);
    placeSlider (flux, intensityLabel, intensity);
    placeSlider (flux, driveLabel, magneticDrive);
    placeSlider (flux, preserveLabel, attackPreserve);
    placeToggle (flux, fluxBypass);

    placeSlider (tape, tapeDriveLabel, tapeDrive);
    placeSlider (tape, bodyLabel, tapeBody);
    placeSlider (tape, returnLabel, tapeReturn);
    placeChoice (tape, biasLabel, tapeBias);
    placeToggle (tape, tapeBypass);
}

void SoundSaturatorAudioProcessorEditor::timerCallback()
{
    shownReductionDb = processor.gainReductionDb.load (std::memory_order_relaxed);
    const float peak = processor.outputPeakLinear.load (std::memory_order_relaxed);
    shownPeakDb = juce::Decibels::gainToDecibels (peak, -100.0f);
    repaint (meterBounds);
}
