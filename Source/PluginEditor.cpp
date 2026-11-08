#include "PluginEditor.h"
#include "dsp/ChainOrder.h"
#include "dsp/FactoryPresets.h"

#include <cmath>
#include <initializer_list>

namespace
{
const juce::Colour kBackground { 0xff12100e };
const juce::Colour kCard { 0xff1c1815 };
const juce::Colour kInk { 0xfff4efe8 };
const juce::Colour kMuted { 0xff8d8278 };
const juce::Colour kBrass { 0xffe0a15a };
const juce::Colour kCopper { 0xffd06a45 };
const juce::Colour kGold { 0xffd7b07a };
const juce::Colour kIron { 0xffa9c0c8 };

juce::Colour accentOf (const juce::Button& button)
{
    const auto stored = juce::Colour::fromString (button.getProperties()["accent"].toString());
    return stored.isTransparent() ? kBrass : stored;
}
}

SaturatorLookAndFeel::SaturatorLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, kInk);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, kBrass.withAlpha (0.35f));
    setColour (juce::Label::textColourId, kMuted);
    setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xff2a241f));
    setColour (juce::TooltipWindow::textColourId, kInk);
    setColour (juce::TooltipWindow::outlineColourId, juce::Colour (0xff4a4038));
    setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff2a241f));
    setColour (juce::ComboBox::textColourId, kInk);
    setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff4a4038));
    setColour (juce::ComboBox::arrowColourId, kBrass);
    setColour (juce::ComboBox::focusedOutlineColourId, kBrass);
    setColour (juce::PopupMenu::backgroundColourId, kCard);
    setColour (juce::PopupMenu::textColourId, kInk);
    setColour (juce::PopupMenu::headerTextColourId, kBrass);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, kBrass);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colour (0xff1a120c));
}

void SaturatorLookAndFeel::drawRotarySlider (juce::Graphics& graphics, int x, int y, int width, int height,
                                             float sliderPos, float startAngle, float endAngle, juce::Slider& slider)
{
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (6.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = startAngle + sliderPos * (endAngle - startAngle);
    const float track = radius * 0.78f;
    const float thickness = juce::jmax (3.0f, radius * 0.11f);
    auto colour = slider.findColour (juce::Slider::rotarySliderFillColourId);
    if (! slider.isEnabled())
        colour = colour.withMultipliedAlpha (0.35f);

    juce::Path background;
    background.addCentredArc (centre.x, centre.y, track, track, 0.0f, startAngle, endAngle, true);
    graphics.setColour (juce::Colour (0xff3a332c));
    graphics.strokePath (background, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    if (sliderPos > 0.001f)
    {
        juce::Path value;
        value.addCentredArc (centre.x, centre.y, track, track, 0.0f, startAngle, angle, true);
        graphics.setColour (colour);
        graphics.strokePath (value, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    const float cap = radius * 0.58f;
    graphics.setColour (slider.isEnabled() ? juce::Colour (0xff2a241f) : juce::Colour (0xff221e1b));
    graphics.fillEllipse (centre.x - cap, centre.y - cap, cap * 2.0f, cap * 2.0f);
    graphics.setColour (juce::Colour (0xff4a4038));
    graphics.drawEllipse (centre.x - cap, centre.y - cap, cap * 2.0f, cap * 2.0f, 1.2f);

    const auto pointer = centre.getPointOnCircumference (cap * 0.72f, angle);
    graphics.setColour (colour);
    graphics.drawLine (centre.x, centre.y, pointer.x, pointer.y, 2.0f);
}

void SaturatorLookAndFeel::drawButtonBackground (juce::Graphics& graphics, juce::Button& button, const juce::Colour&,
                                                 bool highlighted, bool down)
{
    const auto kind = button.getProperties()["kind"].toString();
    const bool on = button.getToggleState();
    auto accent = accentOf (button);
    auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);

    if (kind == "power")
    {
        auto colour = on ? accent : juce::Colour (0xff5c534b);
        if (! button.isEnabled())
            colour = colour.withMultipliedAlpha (0.4f);
        else if (highlighted)
            colour = colour.brighter (0.12f);
        if (down)
            colour = colour.darker (0.15f);

        graphics.setColour (on ? accent.withAlpha (0.16f) : juce::Colour (0xff161310));
        graphics.fillEllipse (bounds);
        graphics.setColour (colour);
        graphics.drawEllipse (bounds.reduced (0.7f), 1.5f);

        const auto centre = bounds.getCentre();
        const float radius = bounds.getWidth() * 0.22f;
        juce::Path arc;
        arc.addCentredArc (centre.x, centre.y + radius * 0.12f, radius, radius, 0.0f,
                           juce::degreesToRadians (48.0f), juce::degreesToRadians (312.0f), true);
        graphics.strokePath (arc, juce::PathStrokeType (1.7f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        graphics.drawLine (centre.x, centre.y - radius * 1.2f, centre.x, centre.y + radius * 0.05f, 1.7f);
        return;
    }

    auto fill = on ? accent : juce::Colour (0xff2a241f);
    if (! button.isEnabled())
        fill = fill.withMultipliedAlpha (0.45f);
    else if (highlighted)
        fill = fill.brighter (0.08f);
    if (down)
        fill = fill.darker (0.12f);

    const float corner = kind == "bypass" ? 8.0f : 7.0f;
    graphics.setColour (fill);
    graphics.fillRoundedRectangle (bounds, corner);
    graphics.setColour (on ? accent.brighter (0.25f) : juce::Colour (0xff4a4038));
    graphics.drawRoundedRectangle (bounds, corner, 1.0f);
}

void SaturatorLookAndFeel::drawButtonText (juce::Graphics& graphics, juce::TextButton& button, bool, bool)
{
    if (button.getProperties()["kind"].toString() == "power")
        return;

    auto text = button.getToggleState() ? juce::Colour (0xff1a120c) : kInk;
    if (! button.isEnabled())
        text = text.withMultipliedAlpha (0.45f);
    graphics.setColour (text);
    graphics.setFont (juce::FontOptions (12.5f));
    graphics.drawFittedText (button.getButtonText(), button.getLocalBounds().reduced (4, 0), juce::Justification::centred, 1);
}

juce::Label* SaturatorLookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* label = LookAndFeel_V4::createSliderTextBox (slider);
    label->setFont (juce::FontOptions (12.0f));
    label->setJustificationType (juce::Justification::centred);
    return label;
}

SoundSaturatorAudioProcessorEditor::SoundSaturatorAudioProcessorEditor (SoundSaturatorAudioProcessor& processorIn)
    : juce::AudioProcessorEditor (processorIn),
      tooltipWindow (this, 600),
      processor (processorIn)
{
    setLookAndFeel (&lookAndFeel);
    setColour (juce::ResizableWindow::backgroundColourId, kBackground);
    addAndMakeVisible (surface);

    juce::String lastGroup;
    for (int index = 0; index < kFactoryPresetCount; ++index)
    {
        const auto& preset = kFactoryPresets[index];
        const juce::String group { preset.group };
        if (group != lastGroup)
        {
            presetBox.addSectionHeading (group);
            lastGroup = group;
        }
        presetBox.addItem (preset.name, index + 1);
    }
    presetBox.setTextWhenNothingSelected ("Preset");
    presetBox.setTooltip (kFactoryPresets[processor.getCurrentProgram()].detail);
    presetBox.onChange = [this]
    {
        const int id = presetBox.getSelectedId();
        if (id > 0)
            processor.setCurrentProgram (id - 1);
        if (const int program = processor.getCurrentProgram(); program >= 0 && program < kFactoryPresetCount)
            presetBox.setTooltip (kFactoryPresets[program].detail);
        syncFromParameters();
    };
    surface.addAndMakeVisible (presetBox);
    presetBox.setSelectedId (processor.getCurrentProgram() + 1, juce::dontSendNotification);

    globalBypass.getProperties().set ("kind", "bypass");
    globalBypass.getProperties().set ("accent", kBrass.toString());
    globalBypass.setClickingTogglesState (true);
    globalBypass.setTooltip ("Hear the original signal");
    surface.addAndMakeVisible (globalBypass);

    zoomLabel.setText ("Zoom", juce::dontSendNotification);
    zoomLabel.setJustificationType (juce::Justification::centredRight);
    zoomLabel.setFont (juce::FontOptions (12.5f));
    zoomLabel.setColour (juce::Label::textColourId, kMuted);
    surface.addAndMakeVisible (zoomLabel);

    auto setupZoom = [this] (juce::TextButton& button, int percent)
    {
        button.getProperties().set ("kind", "mode");
        button.getProperties().set ("accent", kBrass.toString());
        button.setClickingTogglesState (false);
        button.setTooltip ("Zoom the window to " + juce::String (percent) + "%");
        surface.addAndMakeVisible (button);
        button.onClick = [this, percent] { setZoomPercent (percent); };
    };
    setupZoom (zoomSmall, 50);
    setupZoom (zoomMedium, 75);
    setupZoom (zoomFull, 100);
    buttonAttachments.push_back (std::make_unique<ButtonAttachment> (processor.apvts, ids::globalBypass, globalBypass));

    addKnob (inputTrim, inputTrimLabel, "Input", ids::inputTrim, kGold);
    addKnob (mix, mixLabel, "Mix", ids::mix, kBrass);
    addKnob (output, outputLabel, "Output", ids::finalOutput, kInk);

    addPower (vcaPower, ids::vcaBypass, kGold, "Compressor");
    addKnob (threshold, thresholdLabel, "Threshold", ids::vcaThreshold, kGold);
    addKnob (ratio, ratioLabel, "Ratio", ids::vcaRatio, kGold);
    addKnob (makeup, makeupLabel, "Makeup", ids::vcaMakeup, kGold);
    addKnob (vcaIn, vcaInLabel, "In", ids::vcaInput, kGold);
    addKnob (vcaOut, vcaOutLabel, "Out", ids::vcaOutput, kGold);
    vcaIn.setTooltip ("Level into the compressor");
    vcaOut.setTooltip ("Level leaving the compressor");

    addPower (fluxPower, ids::fluxBypass, kBrass, "Transient colour");
    addMode (fluxNickel, 0);
    addMode (fluxCobalt, 1);
    addMode (fluxIron, 2);
    addKnob (intensity, intensityLabel, "Amount", ids::fluxIntensity, kBrass);
    addKnob (magneticDrive, driveLabel, "Drive", ids::magneticDrive, kBrass);
    addKnob (fluxIn, fluxInLabel, "In", ids::fluxInput, kBrass);
    addKnob (fluxOut, fluxOutLabel, "Out", ids::fluxOutput, kBrass);
    fluxIn.setTooltip ("Level into Flux");
    fluxOut.setTooltip ("Level leaving Flux");
    intensity.textFromValueFunction = [] (double value)
    {
        return juce::String (juce::roundToInt (value * 100.0)) + " %";
    };
    intensity.valueFromTextFunction = [] (const juce::String& text)
    {
        return text.getDoubleValue() / 100.0;
    };

    addPower (tapePower, ids::tapeBypass, kCopper, "Tape");
    addKnob (tapeDrive, tapeDriveLabel, "Drive", ids::tapeDrive, kCopper);
    addKnob (tapeBody, bodyLabel, "Body", ids::tapeBody, kCopper);
    addKnob (tapeIn, tapeInLabel, "In", ids::tapeInput, kCopper);
    addKnob (tapeOut, tapeOutLabel, "Out", ids::tapeOutput, kCopper);
    tapeIn.setTooltip ("Level into the tape");
    tapeOut.setTooltip ("Level leaving the tape");

    addPower (transformerPower, ids::transformerBypass, kIron, "Output transformer");
    addKnob (transformerDrive, transformerDriveLabel, "Drive", ids::transformerDrive, kIron);
    addKnob (transformerWeight, transformerWeightLabel, "Weight", ids::transformerWeight, kIron);
    addKnob (transformerIn, transformerInLabel, "In", ids::transformerInput, kIron);
    addKnob (transformerOut, transformerOutLabel, "Out", ids::transformerOutput, kIron);
    transformerIn.setTooltip ("Level into the transformer");
    transformerOut.setTooltip ("Level leaving the transformer");

    setResizable (true, true);
    setResizeLimits (kDesignWidth / 2, kDesignHeight / 2, kDesignWidth, kDesignHeight);
    if (auto* bounds = getConstrainer())
        bounds->setFixedAspectRatio ((double) kDesignWidth / (double) kDesignHeight);
    const float savedZoom = processor.getUiZoomPercent() / 100.0f;
    setSize (juce::roundToInt (kDesignWidth * savedZoom), juce::roundToInt (kDesignHeight * savedZoom));
    syncFromParameters();
    startTimerHz (30);
}

SoundSaturatorAudioProcessorEditor::~SoundSaturatorAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void SoundSaturatorAudioProcessorEditor::addKnob (juce::Slider& slider, juce::Label& label, const juce::String& text,
                                                  const char* parameterId, juce::Colour accent)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 76, 18);
    slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
    slider.setScrollWheelEnabled (true);
    surface.addAndMakeVisible (slider);

    label.setText (text, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::FontOptions (12.0f));
    label.setColour (juce::Label::textColourId, kMuted);
    surface.addAndMakeVisible (label);

    sliderAttachments.push_back (std::make_unique<SliderAttachment> (processor.apvts, parameterId, slider));
}

void SoundSaturatorAudioProcessorEditor::addPower (juce::TextButton& button, const char* bypassId, juce::Colour accent, const juce::String& tip)
{
    button.setButtonText ("Bypass");
    button.getProperties().set ("kind", "bypass");
    button.getProperties().set ("accent", accent.toString());
    button.setClickingTogglesState (true);
    button.setTooltip (tip);
    surface.addAndMakeVisible (button);
    button.onClick = [this, bypassId, &button]
    {
        setBool (bypassId, button.getToggleState());
    };
}

void SoundSaturatorAudioProcessorEditor::addMode (juce::TextButton& button, int index)
{
    button.getProperties().set ("kind", "mode");
    button.getProperties().set ("accent", kBrass.toString());
    button.setClickingTogglesState (false);
    button.setTooltip ("Flux window");
    surface.addAndMakeVisible (button);
    button.onClick = [this, index]
    {
        setChoice (ids::fluxMode, index);
        syncFromParameters();
    };
}

void SoundSaturatorAudioProcessorEditor::setBool (const char* parameterId, bool value)
{
    if (auto* parameter = processor.apvts.getParameter (parameterId))
    {
        const float normalised = value ? 1.0f : 0.0f;
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (normalised);
        parameter->endChangeGesture();
    }
}

void SoundSaturatorAudioProcessorEditor::setChoice (const char* parameterId, int index)
{
    if (auto* parameter = processor.apvts.getParameter (parameterId))
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) index));
        parameter->endChangeGesture();
    }
}

bool SoundSaturatorAudioProcessorEditor::syncToggle (juce::Button& button, bool shouldBeOn)
{
    if (button.getToggleState() == shouldBeOn)
        return false;

    button.setToggleState (shouldBeOn, juce::dontSendNotification);
    return true;
}

void SoundSaturatorAudioProcessorEditor::syncFromParameters()
{
    const auto flag = [this] (const char* parameterId)
    {
        if (auto* value = processor.apvts.getRawParameterValue (parameterId))
            return value->load() >= 0.5f;
        return false;
    };

    const bool vcaOn = ! flag (ids::vcaBypass);
    const bool fluxOn = ! flag (ids::fluxBypass);
    const bool tapeOn = ! flag (ids::tapeBypass);
    const bool transformerOn = ! flag (ids::transformerBypass);
    syncToggle (vcaPower, ! vcaOn);
    syncToggle (fluxPower, ! fluxOn);
    syncToggle (tapePower, ! tapeOn);
    syncToggle (transformerPower, ! transformerOn);

    for (auto* control : { &threshold, &ratio, &makeup })
        control->setEnabled (vcaOn);
    for (auto* control : { &intensity, &magneticDrive })
        control->setEnabled (fluxOn);
    for (auto* button : { &fluxNickel, &fluxCobalt, &fluxIron })
        button->setEnabled (fluxOn);
    for (auto* control : { &tapeDrive, &tapeBody })
        control->setEnabled (tapeOn);
    for (auto* control : { &transformerDrive, &transformerWeight })
        control->setEnabled (transformerOn);

    const int mode = juce::jlimit (0, 2, (int) std::lround (processor.apvts.getRawParameterValue (ids::fluxMode) != nullptr
                                                                ? processor.apvts.getRawParameterValue (ids::fluxMode)->load()
                                                                : 1.0f));
    syncToggle (fluxNickel, mode == 0);
    syncToggle (fluxCobalt, mode == 1);
    syncToggle (fluxIron, mode == 2);
}

void SoundSaturatorAudioProcessorEditor::placeKnobs (juce::Rectangle<int> area,
                                                     std::initializer_list<std::pair<juce::Label*, juce::Slider*>> knobs)
{
    if (knobs.size() == 0)
        return;

    const int width = area.getWidth() / (int) knobs.size();
    for (auto knob : knobs)
    {
        auto slot = area.removeFromLeft (width).reduced (4, 0);
        knob.first->setBounds (slot.removeFromBottom (18));
        knob.second->setBounds (slot);
    }
}

void SoundSaturatorAudioProcessorEditor::paintCard (juce::Graphics& graphics, juce::Rectangle<int> bounds,
                                                    const juce::String& title, const juce::String& route,
                                                    juce::Colour accent, bool active, bool lifted)
{
    graphics.setColour (lifted ? kCard.brighter (0.08f) : kCard);
    graphics.fillRoundedRectangle (bounds.toFloat(), 14.0f);
    graphics.setColour (active ? accent.withAlpha (lifted ? 1.0f : 0.9f) : juce::Colour (0xff3a332c));
    graphics.drawRoundedRectangle (bounds.toFloat().reduced (0.5f), 14.0f, lifted ? 2.0f : 1.4f);

    auto header = bounds.reduced (16, 10).removeFromTop (40);
    header.removeFromRight (100);
    auto titleArea = header.removeFromTop (20);
    auto grip = titleArea.removeFromLeft (14);
    graphics.setColour (kMuted);
    for (int line = 0; line < 3; ++line)
    {
        const float y = grip.getY() + 4.0f + (float) line * 4.0f;
        graphics.fillRoundedRectangle (grip.getX() + 1.0f, y, 8.0f, 1.6f, 0.8f);
    }

    graphics.setColour (active ? kInk : kMuted);
    graphics.setFont (juce::FontOptions (15.0f).withStyle ("Bold"));
    graphics.drawFittedText (title, titleArea, juce::Justification::centredLeft, 1);
    graphics.setColour (kMuted);
    graphics.setFont (juce::FontOptions (11.0f));
    graphics.drawFittedText (route, header, juce::Justification::centredLeft, 1);
}

void SoundSaturatorAudioProcessorEditor::paintMeter (juce::Graphics& graphics, juce::Rectangle<int> bounds,
                                                     const juce::String& name, float amount, const juce::String& value)
{
    graphics.setColour (kMuted);
    graphics.setFont (juce::FontOptions (11.0f));
    graphics.drawText (name, bounds.removeFromTop (16), juce::Justification::centred);

    auto readout = bounds.removeFromBottom (18);
    auto bar = bounds.reduced (10, 4);
    graphics.setColour (juce::Colour (0xff100e0c));
    graphics.fillRoundedRectangle (bar.toFloat(), 5.0f);

    const int filled = (int) std::round (bar.getHeight() * std::clamp (amount, 0.0f, 1.0f));
    if (filled > 0)
    {
        auto level = bar.withTop (bar.getBottom() - filled);
        graphics.setColour (name == "GR" ? kBrass : kInk);
        graphics.fillRoundedRectangle (level.toFloat(), 5.0f);
    }

    graphics.setColour (kInk);
    graphics.drawText (value, readout, juce::Justification::centred);
}

void SoundSaturatorAudioProcessorEditor::paint (juce::Graphics& graphics)
{
    graphics.fillAll (kBackground);
}

void SoundSaturatorAudioProcessorEditor::ZoomSurface::paint (juce::Graphics& graphics)
{
    editor.paintInterface (graphics);
}

void SoundSaturatorAudioProcessorEditor::ZoomSurface::resized()
{
    editor.layoutInterface();
}

void SoundSaturatorAudioProcessorEditor::ZoomSurface::mouseMove (const juce::MouseEvent& event)
{
    editor.handleMouseMove (event);
}

void SoundSaturatorAudioProcessorEditor::ZoomSurface::mouseDown (const juce::MouseEvent& event)
{
    editor.handleMouseDown (event);
}

void SoundSaturatorAudioProcessorEditor::ZoomSurface::mouseDrag (const juce::MouseEvent& event)
{
    editor.handleMouseDrag (event);
}

void SoundSaturatorAudioProcessorEditor::ZoomSurface::mouseUp (const juce::MouseEvent& event)
{
    editor.handleMouseUp (event);
}

void SoundSaturatorAudioProcessorEditor::setZoomPercent (int percent)
{
    processor.setUiZoomPercent (percent);
    const float scale = processor.getUiZoomPercent() / 100.0f;
    setSize (juce::roundToInt (kDesignWidth * scale), juce::roundToInt (kDesignHeight * scale));
    syncZoomButtons();
}

void SoundSaturatorAudioProcessorEditor::syncZoomButtons()
{
    const int percent = processor.getUiZoomPercent();
    auto mark = [percent] (juce::TextButton& button, int step)
    {
        button.setToggleState (std::abs (percent - step) <= 4, juce::dontSendNotification);
    };
    mark (zoomSmall, 50);
    mark (zoomMedium, 75);
    mark (zoomFull, 100);
}

void SoundSaturatorAudioProcessorEditor::paintInterface (juce::Graphics& graphics)
{
    graphics.fillAll (kBackground);
    graphics.setColour (kBrass);
    graphics.fillRect (0, 0, kDesignWidth, 3);

    graphics.setColour (kInk);
    graphics.setFont (juce::FontOptions (22.0f).withStyle ("Bold"));
    graphics.drawText ("Sound Saturator", 22, 16, 280, 28, juce::Justification::centredLeft);
    graphics.setColour (kMuted);
    graphics.setFont (juce::FontOptions (12.5f));
    graphics.drawText ("Drag a title to move that stage. Each one keeps its own in and out.", 24, 44, 520, 16, juce::Justification::centredLeft);

    for (int slot = 0; slot < kChainStageCount - 1; ++slot)
    {
        const int x = (slotRest[slot].getRight() + slotRest[slot + 1].getX()) / 2;
        const int y = slotRest[slot].getY() + 24;
        juce::Path arrow;
        arrow.addTriangle ((float) x - 4.0f, (float) y - 4.0f, (float) x - 4.0f, (float) y + 4.0f, (float) x + 3.0f, (float) y);
        graphics.setColour (kMuted);
        graphics.fillPath (arrow);
    }

    for (int stage = 0; stage < kChainStageCount; ++stage)
        if (stage != draggingStage)
            paintStage (graphics, stage);
    if (draggingStage >= 0)
        paintStage (graphics, draggingStage);

    auto meters = meterBounds.reduced (4);
    auto reduction = meters.removeFromLeft (meters.getWidth() / 2);
    auto peak = meters;
    const float reductionAmount = juce::jlimit (0.0f, 1.0f, shownReductionDb / 24.0f);
    const float peakAmount = juce::jmap (juce::jlimit (-48.0f, 0.0f, shownPeakDb), -48.0f, 0.0f, 0.0f, 1.0f);
    paintMeter (graphics, reduction, "GR", reductionAmount, juce::String (shownReductionDb, 1));
    paintMeter (graphics, peak, "Peak", peakAmount, juce::String (shownPeakDb, 1));
}

int SoundSaturatorAudioProcessorEditor::readOrderIndex() const
{
    if (auto* value = processor.apvts.getRawParameterValue (ids::chainOrder))
        return juce::jlimit (0, kChainOrderCount - 1, (int) std::lround (value->load()));
    return 0;
}

void SoundSaturatorAudioProcessorEditor::readOrder()
{
    orderIndex = readOrderIndex();
    decodeChainOrder (orderIndex, order);
    for (int slot = 0; slot < kChainStageCount; ++slot)
        visualOrder[slot] = order[slot];
}

juce::String SoundSaturatorAudioProcessorEditor::routeText (int stage) const
{
    int slot = 0;
    for (int index = 0; index < kChainStageCount; ++index)
        if (visualOrder[index] == stage)
            slot = index;

    const juce::String from = slot == 0 ? "Input" : chainStageName (visualOrder[slot - 1]);
    const juce::String to = slot == kChainStageCount - 1 ? "Output" : chainStageName (visualOrder[slot + 1]);
    return "from " + from + "   to " + to;
}

void SoundSaturatorAudioProcessorEditor::paintStage (juce::Graphics& graphics, int stage)
{
    const bool lifted = stage == draggingStage;
    if (stage == stageVca)
        paintCard (graphics, stageBounds[stage], "VCA", routeText (stage), kGold, ! vcaPower.getToggleState(), lifted);
    else if (stage == stageFlux)
        paintCard (graphics, stageBounds[stage], "Flux", routeText (stage), kBrass, ! fluxPower.getToggleState(), lifted);
    else if (stage == stageTape)
        paintCard (graphics, stageBounds[stage], "Tape", routeText (stage), kCopper, ! tapePower.getToggleState(), lifted);
    else
        paintCard (graphics, stageBounds[stage], "Transformers", routeText (stage), kIron, ! transformerPower.getToggleState(), lifted);
}

void SoundSaturatorAudioProcessorEditor::positionStage (int stage, juce::Rectangle<int> panel)
{
    auto place = [this] (juce::Rectangle<int> bounds, juce::TextButton& power,
                         std::initializer_list<std::pair<juce::Label*, juce::Slider*>> knobs,
                         std::initializer_list<juce::TextButton*> modes,
                         std::initializer_list<std::pair<juce::Label*, juce::Slider*>> levels)
    {
        power.setBounds (bounds.getRight() - 108, bounds.getY() + 12, 92, 26);
        auto inner = bounds.reduced (12, 12);
        inner.removeFromTop (46);
        if (modes.size() > 0)
        {
            auto row = inner.removeFromTop (30);
            inner.removeFromTop (8);
            const int width = row.getWidth() / (int) modes.size();
            for (auto* mode : modes)
                mode->setBounds (row.removeFromLeft (width).reduced (3, 0));
        }
        auto levelRow = inner.removeFromBottom (112);
        inner.removeFromBottom (8);
        placeKnobs (inner, knobs);
        placeKnobs (levelRow, levels);
    };

    if (stage == stageVca)
        place (panel, vcaPower,
               { { &thresholdLabel, &threshold }, { &ratioLabel, &ratio }, { &makeupLabel, &makeup } },
               {},
               { { &vcaInLabel, &vcaIn }, { &vcaOutLabel, &vcaOut } });
    else if (stage == stageFlux)
        place (panel, fluxPower,
               { { &intensityLabel, &intensity }, { &driveLabel, &magneticDrive } },
               { &fluxNickel, &fluxCobalt, &fluxIron },
               { { &fluxInLabel, &fluxIn }, { &fluxOutLabel, &fluxOut } });
    else if (stage == stageTape)
        place (panel, tapePower,
               { { &tapeDriveLabel, &tapeDrive }, { &bodyLabel, &tapeBody } },
               {},
               { { &tapeInLabel, &tapeIn }, { &tapeOutLabel, &tapeOut } });
    else
        place (panel, transformerPower,
               { { &transformerDriveLabel, &transformerDrive }, { &transformerWeightLabel, &transformerWeight } },
               {},
               { { &transformerInLabel, &transformerIn }, { &transformerOutLabel, &transformerOut } });
}

void SoundSaturatorAudioProcessorEditor::bringStageToFront (int stage)
{
    juce::Component* controls[16] = {};
    int count = 0;
    auto add = [&] (juce::Component& component)
    {
        controls[count++] = &component;
    };

    if (stage == stageVca)
    {
        add (vcaPower);
        add (threshold); add (thresholdLabel);
        add (ratio); add (ratioLabel);
        add (makeup); add (makeupLabel);
        add (vcaIn); add (vcaInLabel);
        add (vcaOut); add (vcaOutLabel);
    }
    else if (stage == stageFlux)
    {
        add (fluxPower);
        add (fluxNickel); add (fluxCobalt); add (fluxIron);
        add (intensity); add (intensityLabel);
        add (magneticDrive); add (driveLabel);
        add (fluxIn); add (fluxInLabel);
        add (fluxOut); add (fluxOutLabel);
    }
    else if (stage == stageTape)
    {
        add (tapePower);
        add (tapeDrive); add (tapeDriveLabel);
        add (tapeBody); add (bodyLabel);
        add (tapeIn); add (tapeInLabel);
        add (tapeOut); add (tapeOutLabel);
    }
    else
    {
        add (transformerPower);
        add (transformerDrive); add (transformerDriveLabel);
        add (transformerWeight); add (transformerWeightLabel);
        add (transformerIn); add (transformerInLabel);
        add (transformerOut); add (transformerOutLabel);
    }

    for (int index = 0; index < count; ++index)
        controls[index]->toFront (false);
}

void SoundSaturatorAudioProcessorEditor::layoutChain()
{
    const int columnWidth = chainArea.getWidth() / kChainStageCount;
    for (int slot = 0; slot < kChainStageCount; ++slot)
        slotRest[slot] = chainArea.withX (chainArea.getX() + slot * columnWidth).withWidth (columnWidth).reduced (6, 0);

    if (draggingStage >= 0)
    {
        int resting[kChainStageCount - 1];
        int count = 0;
        for (int slot = 0; slot < kChainStageCount; ++slot)
            if (order[slot] != draggingStage)
                resting[count++] = order[slot];

        int restingIndex = 0;
        for (int slot = 0; slot < kChainStageCount; ++slot)
        {
            if (slot == dropIndex)
                visualOrder[slot] = draggingStage;
            else
                visualOrder[slot] = resting[restingIndex++];
        }
    }

    for (int slot = 0; slot < kChainStageCount; ++slot)
    {
        const int stage = visualOrder[slot];
        auto bounds = slotRest[slot];
        if (stage == draggingStage)
            bounds.setX (juce::jlimit (chainArea.getX(), chainArea.getRight() - bounds.getWidth(), dragGrab));
        stageBounds[stage] = bounds;
        positionStage (stage, bounds);
    }

    if (draggingStage >= 0)
        bringStageToFront (draggingStage);
}

int SoundSaturatorAudioProcessorEditor::headerStage (juce::Point<int> position) const
{
    auto hit = [position] (juce::Rectangle<int> bounds)
    {
        auto header = bounds.removeFromTop (48);
        header.removeFromRight (116);
        return header.contains (position);
    };

    if (draggingStage >= 0 && hit (stageBounds[draggingStage]))
        return draggingStage;

    for (int slot = kChainStageCount - 1; slot >= 0; --slot)
        if (hit (stageBounds[visualOrder[slot]]))
            return visualOrder[slot];
    return -1;
}

void SoundSaturatorAudioProcessorEditor::resized()
{
    const float scale = juce::jlimit (0.5f, 1.0f, getWidth() / (float) kDesignWidth);
    processor.setUiZoomPercent (juce::roundToInt (scale * 100.0f));
    surface.setBounds (0, 0, kDesignWidth, kDesignHeight);
    surface.setTransform (juce::AffineTransform::scale (scale));
    syncZoomButtons();
}

void SoundSaturatorAudioProcessorEditor::layoutInterface()
{
    auto area = juce::Rectangle<int> (0, 0, kDesignWidth, kDesignHeight).reduced (18);
    auto header = area.removeFromTop (58);
    globalBypass.setBounds (header.removeFromRight (104).withSizeKeepingCentre (104, 32));
    header.removeFromRight (10);
    auto zoomArea = header.removeFromRight (174).withSizeKeepingCentre (174, 32);
    header.removeFromRight (6);
    zoomLabel.setBounds (header.removeFromRight (52).withSizeKeepingCentre (52, 32));
    const int zoomWidth = zoomArea.getWidth() / 3;
    zoomSmall.setBounds (zoomArea.removeFromLeft (zoomWidth).reduced (3, 0));
    zoomMedium.setBounds (zoomArea.removeFromLeft (zoomWidth).reduced (3, 0));
    zoomFull.setBounds (zoomArea.reduced (3, 0));
    header.removeFromRight (10);
    const int presetWidth = juce::jmin (300, header.getWidth() / 2);
    presetBox.setBounds (header.removeFromRight (presetWidth).withSizeKeepingCentre (presetWidth, 32));

    meterBounds = area.removeFromRight (132).reduced (4, 8);
    area.removeFromRight (10);

    auto masters = area.removeFromTop (136);
    placeKnobs (masters, { { &inputTrimLabel, &inputTrim }, { &mixLabel, &mix }, { &outputLabel, &output } });

    area.removeFromTop (8);
    chainArea = area;
    if (draggingStage < 0)
        readOrder();
    layoutChain();
}

void SoundSaturatorAudioProcessorEditor::handleMouseMove (const juce::MouseEvent& event)
{
    surface.setMouseCursor (headerStage (event.getPosition()) >= 0 ? juce::MouseCursor::DraggingHandCursor
                                                                  : juce::MouseCursor::NormalCursor);
}

void SoundSaturatorAudioProcessorEditor::handleMouseDown (const juce::MouseEvent& event)
{
    const int stage = headerStage (event.getPosition());
    if (stage < 0)
        return;

    draggingStage = stage;
    dropIndex = 0;
    for (int slot = 0; slot < kChainStageCount; ++slot)
        if (order[slot] == stage)
            dropIndex = slot;
    dragGrab = stageBounds[stage].getX();
    bringStageToFront (stage);
}

void SoundSaturatorAudioProcessorEditor::handleMouseDrag (const juce::MouseEvent& event)
{
    if (draggingStage < 0)
        return;

    if (event.getDistanceFromDragStart() < 4)
        return;

    const int width = juce::jmax (1, slotRest[0].getWidth());
    dragGrab = juce::jlimit (chainArea.getX(), chainArea.getRight() - width, event.getPosition().x - width / 2);
    const int centre = dragGrab + width / 2;
    dropIndex = 0;
    for (int slot = 0; slot < kChainStageCount - 1; ++slot)
        if (centre > (slotRest[slot].getCentreX() + slotRest[slot + 1].getCentreX()) / 2)
            dropIndex = slot + 1;

    layoutChain();
    repaint();
}

void SoundSaturatorAudioProcessorEditor::handleMouseUp (const juce::MouseEvent&)
{
    if (draggingStage < 0)
        return;

    int next[kChainStageCount];
    for (int slot = 0; slot < kChainStageCount; ++slot)
        next[slot] = visualOrder[slot];

    draggingStage = -1;
    const int encoded = encodeChainOrder (next);
    if (encoded != orderIndex)
        setChoice (ids::chainOrder, encoded);

    readOrder();
    layoutChain();
    setMouseCursor (juce::MouseCursor::NormalCursor);
    repaint();
}

void SoundSaturatorAudioProcessorEditor::timerCallback()
{
    syncFromParameters();
    if (draggingStage < 0 && readOrderIndex() != orderIndex)
    {
        readOrder();
        layoutChain();
    }
    if (! presetBox.isPopupActive())
    {
        const int wanted = processor.getCurrentProgram() + 1;
        if (presetBox.getSelectedId() != wanted)
            presetBox.setSelectedId (wanted, juce::dontSendNotification);
    }
    shownReductionDb = processor.gainReductionDb.load (std::memory_order_relaxed);
    const float peak = processor.outputPeakLinear.load (std::memory_order_relaxed);
    shownPeakDb = juce::Decibels::gainToDecibels (peak, -100.0f);
    repaint();
}
