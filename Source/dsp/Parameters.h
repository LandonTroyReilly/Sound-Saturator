#pragma once

#include "PrototypeConstants.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace ids
{
inline constexpr const char* inputTrim = "inputTrim";
inline constexpr const char* vcaThreshold = "vcaThreshold";
inline constexpr const char* vcaRatio = "vcaRatio";
inline constexpr const char* vcaTiming = "vcaTiming";
inline constexpr const char* vcaKnee = "vcaKnee";
inline constexpr const char* vcaDetector = "vcaDetector";
inline constexpr const char* vcaScHpf = "vcaScHpf";
inline constexpr const char* vcaScHpfHz = "vcaScHpfHz";
inline constexpr const char* vcaMakeup = "vcaMakeup";
inline constexpr const char* vcaBypass = "vcaBypass";
inline constexpr const char* fluxMode = "fluxMode";
inline constexpr const char* fluxIntensity = "fluxIntensity";
inline constexpr const char* magneticDrive = "magneticDrive";
inline constexpr const char* attackPreserve = "attackPreserve";
inline constexpr const char* fluxBypass = "fluxBypass";
inline constexpr const char* tapeDrive = "tapeDrive";
inline constexpr const char* tapeBody = "tapeBody";
inline constexpr const char* tapeReturn = "tapeReturn";
inline constexpr const char* tapeBias = "tapeBias";
inline constexpr const char* tapeBypass = "tapeBypass";
inline constexpr const char* mix = "mix";
inline constexpr const char* finalOutput = "finalOutput";
inline constexpr const char* globalBypass = "globalBypass";
inline constexpr const char* quality = "quality";
inline constexpr int stateVersion = 1;
}

struct ParameterSnapshot
{
    float inputTrimDb = 0.0f;
    float vcaThresholdDb = -18.0f;
    int vcaRatioIndex = 0;
    int vcaTimingIndex = 3;
    float vcaKneeDb = 6.0f;
    int vcaDetectorIndex = 0;
    bool vcaScHpf = false;
    float vcaScHpfHz = 80.0f;
    float vcaMakeupDb = 0.0f;
    bool vcaBypass = false;
    int fluxMode = 1;
    float fluxIntensity = 0.0f;
    float magneticDriveDb = 0.0f;
    float attackPreserve = 0.0f;
    bool fluxBypass = true;
    float tapeDriveDb = 0.0f;
    float tapeBodyDb = 0.0f;
    float tapeReturnDb = 0.0f;
    int tapeBias = 1;
    bool tapeBypass = true;
    float mixPercent = 100.0f;
    float finalOutputDb = 0.0f;
    bool globalBypass = false;
    int qualityIndex = 1;
};

inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto addFloat = [&] (const char* id, const String& name, float min, float max, float step, float def, const String& label)
    {
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { id, 1 },
            name,
            NormalisableRange<float> (min, max, step),
            def,
            AudioParameterFloatAttributes().withLabel (label)));
    };

    auto addChoice = [&] (const char* id, const String& name, const StringArray& choices, int def)
    {
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, choices, def));
    };

    auto addBool = [&] (const char* id, const String& name, bool def)
    {
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def));
    };

    addFloat (ids::inputTrim, "Input Trim", -24.0f, 24.0f, 0.01f, 0.0f, "dB");
    addFloat (ids::vcaThreshold, "VCA Threshold", -48.0f, 0.0f, 0.01f, -18.0f, "dB");
    addChoice (ids::vcaRatio, "VCA Ratio", { "1:1", "2:1", "4:1", "8:1", "Limiter" }, 0);

    StringArray timing;
    for (int i = 1; i <= 10; ++i)
        timing.add (String (i));
    addChoice (ids::vcaTiming, "VCA Timing", timing, 3);

    addFloat (ids::vcaKnee, "VCA Knee", 0.0f, 12.0f, 0.01f, 6.0f, "dB");
    addChoice (ids::vcaDetector, "VCA Detector", { "Peak", "RMS" }, 0);
    addBool (ids::vcaScHpf, "Sidechain HPF", false);
    addFloat (ids::vcaScHpfHz, "Sidechain HPF Freq", 20.0f, 250.0f, 0.01f, 80.0f, "Hz");
    addFloat (ids::vcaMakeup, "VCA Makeup", -12.0f, 24.0f, 0.01f, 0.0f, "dB");
    addBool (ids::vcaBypass, "VCA Bypass", false);

    addChoice (ids::fluxMode, "Flux", { "Nickel", "Cobalt", "Iron" }, 1);
    addFloat (ids::fluxIntensity, "Intensity", 0.0f, 1.0f, 0.001f, 0.0f, "");
    addFloat (ids::magneticDrive, "Magnetic Drive", 0.0f, 24.0f, 0.01f, 0.0f, "dB");
    addFloat (ids::attackPreserve, "Attack Preserve", 0.0f, 1.0f, 0.001f, 0.0f, "");
    addBool (ids::fluxBypass, "Flux Bypass", true);

    addFloat (ids::tapeDrive, "Tape Drive", 0.0f, 24.0f, 0.01f, 0.0f, "dB");
    addFloat (ids::tapeBody, "Tape Body", 0.0f, 3.0f, 0.01f, 0.0f, "dB");
    addFloat (ids::tapeReturn, "Tape Return", -12.0f, 6.0f, 0.01f, 0.0f, "dB");
    addChoice (ids::tapeBias, "Tape Bias", { "Low", "Nominal", "High" }, 1);
    addBool (ids::tapeBypass, "Tape Bypass", true);

    addFloat (ids::mix, "Mix", 0.0f, 100.0f, 0.01f, 100.0f, "%");
    addFloat (ids::finalOutput, "Final Output", -36.0f, 12.0f, 0.01f, 0.0f, "dB");
    addBool (ids::globalBypass, "Bypass", false);
    addChoice (ids::quality, "Quality", { "2x", "4x", "8x" }, 1);

    return layout;
}
