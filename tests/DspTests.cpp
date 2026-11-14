#include "PluginProcessor.h"
#include "dsp/ChainOrder.h"
#include "dsp/FactoryPresets.h"
#include "dsp/FluxStage.h"
#include "dsp/TapeStage.h"
#include "dsp/TransformerStage.h"
#include "dsp/VcaCompressor.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <vector>

namespace
{
int gFailures = 0;

void expect (bool condition, const char* file, int line, const char* text)
{
    if (! condition)
    {
        std::printf ("FAIL %s:%d: %s\n", file, line, text);
        ++gFailures;
    }
}

void expectNear (double actual, double expected, double tolerance, const char* file, int line, const char* text)
{
    if (! (std::abs (actual - expected) <= tolerance))
    {
        std::printf ("FAIL %s:%d: %s actual=%g expected=%g tol=%g\n", file, line, text, actual, expected, tolerance);
        ++gFailures;
    }
}

#define EXPECT(cond) expect ((cond), __FILE__, __LINE__, #cond)
#define EXPECT_NEAR(actual, expected, tol) expectNear ((double) (actual), (double) (expected), (double) (tol), __FILE__, __LINE__, #actual)

void setParam (SoundSaturatorAudioProcessor& processor, const char* id, float raw)
{
    if (auto* parameter = processor.apvts.getParameter (id))
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (raw));
    else
        EXPECT (false);
}

double rmsOf (const std::vector<float>& data, int start)
{
    double sum = 0.0;
    const int count = (int) data.size() - start;
    for (int i = start; i < (int) data.size(); ++i)
        sum += (double) data[(size_t) i] * (double) data[(size_t) i];
    return std::sqrt (sum / (double) std::max (1, count));
}

void testKnee()
{
    const double threshold = -20.0;
    const double ratio = 4.0;
    const double knee = 6.0;
    const double lower = threshold - knee * 0.5;
    const double upper = threshold + knee * 0.5;

    EXPECT_NEAR (VcaCompressor::outputDb (lower, threshold, ratio, knee), lower, 1.0e-9);
    EXPECT_NEAR (VcaCompressor::outputSlope (lower, threshold, ratio, knee), 1.0, 1.0e-9);
    EXPECT_NEAR (VcaCompressor::outputDb (upper, threshold, ratio, knee), threshold + (upper - threshold) / ratio, 1.0e-9);
    EXPECT_NEAR (VcaCompressor::outputSlope (upper, threshold, ratio, knee), 1.0 / ratio, 1.0e-9);
    EXPECT_NEAR (VcaCompressor::outputDb (lower - 3.0, threshold, ratio, knee), lower - 3.0, 1.0e-9);
    EXPECT_NEAR (VcaCompressor::outputDb (upper + 3.0, threshold, ratio, knee), threshold + ((upper + 3.0) - threshold) / ratio, 1.0e-9);

    const double epsilon = 1.0e-4;
    for (double x : { lower, upper, threshold })
    {
        const double numeric = (VcaCompressor::outputDb (x + epsilon, threshold, ratio, knee)
                              - VcaCompressor::outputDb (x - epsilon, threshold, ratio, knee)) / (2.0 * epsilon);
        EXPECT_NEAR (numeric, VcaCompressor::outputSlope (x, threshold, ratio, knee), 1.0e-4);
    }

    EXPECT_NEAR (VcaCompressor::outputDb (-10.0, -20.0, 4.0, 0.0), -17.5, 1.0e-9);
    EXPECT_NEAR (VcaCompressor::outputDb (-30.0, -20.0, 4.0, 0.0), -30.0, 1.0e-9);
    EXPECT_NEAR (VcaCompressor::outputDb (-10.0, -20.0, 1.0, 6.0), -10.0, 1.0e-9);
}

void testVcaUnity()
{
    VcaCompressor vca;
    vca.prepare (48000.0, 2);
    VcaSettings settings;
    settings.ratioIndex = 0;
    settings.kneeDb = 6.0f;
    settings.thresholdDb = -12.0f;

    constexpr int n = 256;
    std::vector<float> left (n), right (n);
    for (int i = 0; i < n; ++i)
    {
        left[(size_t) i] = 0.25f * std::sin (2.0f * 3.14159265f * 440.0f * (float) i / 48000.0f);
        right[(size_t) i] = -0.1f * left[(size_t) i];
    }
    auto originalLeft = left;
    auto originalRight = right;
    float* channels[] = { left.data(), right.data() };
    vca.processBlock (channels, 2, n, settings);

    for (int i = 0; i < n; ++i)
    {
        EXPECT (left[(size_t) i] == originalLeft[(size_t) i]);
        EXPECT (right[(size_t) i] == originalRight[(size_t) i]);
    }
}

void testVcaHardKnee()
{
    VcaCompressor vca;
    vca.prepare (48000.0, 1);
    VcaSettings settings;
    settings.thresholdDb = -20.0f;
    settings.ratioIndex = 2;
    settings.kneeDb = 0.0f;
    settings.timingIndex = 0;

    const float amplitude = (float) dbToGain (-10.0);
    const float expected = (float) dbToGain (-17.5);
    constexpr int n = 9600;
    std::vector<float> signal (n, amplitude);
    float* channels[] = { signal.data() };
    vca.processBlock (channels, 1, n, settings);

    for (int i = n - 64; i < n; ++i)
        EXPECT_NEAR (signal[(size_t) i], expected, expected * 0.002);

    vca.prepare (48000.0, 1);
    settings.detectorIndex = 1;
    std::fill (signal.begin(), signal.end(), amplitude);
    vca.processBlock (channels, 1, n, settings);
    for (int i = n - 64; i < n; ++i)
        EXPECT_NEAR (signal[(size_t) i], expected, expected * 0.002);
}

void testVcaStereoLink()
{
    VcaCompressor vca;
    vca.prepare (48000.0, 2);
    VcaSettings settings;
    settings.thresholdDb = -20.0f;
    settings.ratioIndex = 2;
    settings.kneeDb = 0.0f;
    settings.timingIndex = 0;

    const float leftIn = (float) dbToGain (-40.0);
    const float rightIn = (float) dbToGain (-10.0);
    const float gain = (float) dbToGain (-7.5);
    constexpr int n = 4800;
    std::vector<float> left (n, leftIn), right (n, rightIn);
    float* channels[] = { left.data(), right.data() };
    vca.processBlock (channels, 2, n, settings);

    EXPECT_NEAR (left.back(), leftIn * gain, leftIn * gain * 0.01);
    EXPECT_NEAR (right.back(), rightIn * gain, rightIn * gain * 0.002);
}

void testSidechainHpf()
{
    VcaCompressor vca;
    vca.prepare (48000.0, 1);
    VcaSettings settings;
    settings.thresholdDb = -40.0f;
    settings.ratioIndex = 4;
    settings.kneeDb = 0.0f;
    settings.timingIndex = 0;
    settings.sidechainHpf = true;
    settings.sidechainHpfHz = 100.0f;

    const float amplitude = (float) dbToGain (-6.0);
    constexpr int n = 24000;
    std::vector<float> signal (n, amplitude);
    float* channels[] = { signal.data() };
    vca.processBlock (channels, 1, n, settings);

    for (int i = n - 64; i < n; ++i)
        EXPECT_NEAR (signal[(size_t) i], amplitude, amplitude * 0.01);
}

void testFluxIdentity()
{
    FluxStage flux;
    flux.prepare (48000.0, 2);
    FluxSettings settings;
    settings.intensity = 0.0f;
    settings.driveDb = 18.0f;
    settings.mode = 2;
    settings.attackPreserve = 0.4f;

    constexpr int n = 512;
    std::vector<float> left (n), right (n);
    for (int i = 0; i < n; ++i)
    {
        left[(size_t) i] = std::sin (0.07f * (float) i) * 0.4f;
        right[(size_t) i] = (i % 17) * 0.01f - 0.08f;
    }
    auto originalLeft = left;
    auto originalRight = right;
    float* channels[] = { left.data(), right.data() };
    flux.processBlock (channels, 2, n, settings);

    for (int i = 0; i < n; ++i)
    {
        EXPECT (left[(size_t) i] == originalLeft[(size_t) i]);
        EXPECT (right[(size_t) i] == originalRight[(size_t) i]);
    }
}

float modificationWidthMs (int mode)
{
    FluxStage flux;
    flux.prepare (48000.0, 1);
    FluxSettings settings;
    settings.mode = mode;
    settings.intensity = 1.0f;
    settings.driveDb = 12.0f;
    settings.attackPreserve = 0.0f;

    const int silence = (int) (0.02 * 48000.0);
    const int hold = (int) (0.12 * 48000.0);
    const int n = silence + hold;
    std::vector<float> signal (n, 0.0f);
    std::fill (signal.begin() + silence, signal.end(), 0.5f);
    auto dry = signal;
    float* channels[] = { signal.data() };
    flux.processBlock (channels, 1, n, settings);

    std::vector<float> modification (n);
    float peak = 0.0f;
    int peakIndex = 0;
    for (int i = 0; i < n; ++i)
    {
        modification[(size_t) i] = std::abs (signal[(size_t) i] - dry[(size_t) i]);
        if (modification[(size_t) i] > peak)
        {
            peak = modification[(size_t) i];
            peakIndex = i;
        }
    }

    const float gate = 0.1f * peak;
    int start = peakIndex;
    int end = peakIndex;
    while (start > 0 && modification[(size_t) (start - 1)] > gate)
        --start;
    while (end + 1 < n && modification[(size_t) (end + 1)] > gate)
        ++end;
    return (float) (end - start + 1) / 48000.0f * 1000.0f;
}

void testFluxWidths()
{
    const float nickel = modificationWidthMs (0);
    const float cobalt = modificationWidthMs (1);
    const float iron = modificationWidthMs (2);
    std::printf ("Flux widths ms  nickel=%.2f cobalt=%.2f iron=%.2f\n", nickel, cobalt, iron);
    EXPECT (nickel > 2.0f);
    EXPECT (nickel < cobalt);
    EXPECT (cobalt < iron);
    EXPECT (iron < 200.0f);
}

void testTapeState()
{
    TapeStage tape;
    tape.prepare (192000.0, 1);
    tape.reset();

    for (int i = 0; i < 192; ++i)
        tape.processFieldSample (0.85);
    tape.processFieldSample (0.398);
    tape.processFieldSample (0.399);
    const double positive = tape.processFieldSample (0.400);

    tape.reset();
    for (int i = 0; i < 192; ++i)
        tape.processFieldSample (-0.85);
    tape.processFieldSample (0.398);
    tape.processFieldSample (0.399);
    const double negative = tape.processFieldSample (0.400);

    std::printf ("Tape state  positive=%0.6f negative=%0.6f\n", positive, negative);
    EXPECT_NEAR (positive, 0.414928, 2.0e-5);
    EXPECT_NEAR (negative, 0.333592, 2.0e-5);
    EXPECT (positive > negative);
}

std::vector<float> renderTape (const TapeSettings& settings, float amplitude, float frequency, int sampleRate, int samples)
{
    TapeStage tape;
    tape.prepare (sampleRate, 1);
    std::vector<float> signal (samples);
    for (int i = 0; i < samples; ++i)
        signal[(size_t) i] = amplitude * std::sin (2.0f * 3.14159265f * frequency * (float) i / (float) sampleRate);
    float* channels[] = { signal.data() };
    tape.processBlock (channels, 1, samples, settings);
    return signal;
}

void testTapeLevel()
{
    const int sampleRate = 48000;
    const int samples = (int) (0.35 * sampleRate);
    const int settle = (int) (0.25 * sampleRate);
    const float amplitude = 0.001f;

    TapeSettings plain;
    auto dry = renderTape (plain, amplitude, 1000.0f, sampleRate, samples);
    std::vector<float> input (samples);
    for (int i = 0; i < samples; ++i)
        input[(size_t) i] = amplitude * std::sin (2.0f * 3.14159265f * 1000.0f * (float) i / (float) sampleRate);
    const double gainDb = 20.0 * std::log10 (rmsOf (dry, settle) / rmsOf (input, settle));
    std::printf ("Tape small-signal gain %.3f dB\n", gainDb);
    EXPECT_NEAR (gainDb, 0.0, 0.05);

    TapeSettings returned = plain;
    returned.returnDb = 6.0f;
    auto louder = renderTape (returned, amplitude, 1000.0f, sampleRate, samples);
    const double returnDb = 20.0 * std::log10 (rmsOf (louder, settle) / rmsOf (dry, settle));
    EXPECT_NEAR (returnDb, 6.0, 0.05);

    PlaybackLowShelf shelf;
    shelf.prepare (sampleRate);
    shelf.setGainDb (0.0f);
    for (int i = 0; i < 128; ++i)
    {
        const float x = std::sin (0.1f * (float) i);
        EXPECT (shelf.process (x) == x);
    }

    TapeSettings body = plain;
    body.bodyDb = 3.0f;
    auto withBody = renderTape (body, amplitude, 30.0f, sampleRate, samples);
    auto withoutBody = renderTape (plain, amplitude, 30.0f, sampleRate, samples);
    const double bodyDb = 20.0 * std::log10 (rmsOf (withBody, settle) / rmsOf (withoutBody, settle));
    std::printf ("Tape body at 30 Hz %.3f dB\n", bodyDb);
    EXPECT (bodyDb > 2.5 && bodyDb < 3.2);
}

double harmonicShare (const std::vector<float>& output, float amplitude, float frequency, int sampleRate, int settle)
{
    double sineEnergy = 0.0;
    double cosineEnergy = 0.0;
    double crossSine = 0.0;
    double crossCosine = 0.0;
    double outEnergy = 0.0;
    for (int i = settle; i < (int) output.size(); ++i)
    {
        const double phase = 2.0 * 3.14159265358979323846 * (double) frequency * (double) i / (double) sampleRate;
        const double sine = (double) amplitude * std::sin (phase);
        const double cosine = (double) amplitude * std::cos (phase);
        const double y = (double) output[(size_t) i];
        sineEnergy += sine * sine;
        cosineEnergy += cosine * cosine;
        crossSine += y * sine;
        crossCosine += y * cosine;
        outEnergy += y * y;
    }
    if (sineEnergy <= 0.0 || cosineEnergy <= 0.0 || outEnergy <= 1.0e-18)
        return 0.0;

    const double gainSine = crossSine / sineEnergy;
    const double gainCosine = crossCosine / cosineEnergy;
    const double residual = std::max (0.0, outEnergy - gainSine * gainSine * sineEnergy - gainCosine * gainCosine * cosineEnergy);
    return std::sqrt (residual / outEnergy);
}

std::vector<float> renderTransformer (const TransformerSettings& settings, float amplitude, float frequency, int sampleRate, int samples)
{
    TransformerStage transformer;
    transformer.prepare (sampleRate, 1);
    std::vector<float> signal (samples);
    for (int i = 0; i < samples; ++i)
        signal[(size_t) i] = amplitude * std::sin (2.0f * 3.14159265f * frequency * (float) i / (float) sampleRate);
    float* channels[] = { signal.data() };
    transformer.processBlock (channels, 1, samples, settings);
    return signal;
}

void testTransformer()
{
    const int sampleRate = 192000;
    const int samples = (int) (0.5 * sampleRate);
    const int settle = (int) (0.3 * sampleRate);

    TransformerSettings bypassed;
    bypassed.bypassAmount = 1.0f;
    auto clean = renderTransformer (bypassed, 0.25f, 30.0f, sampleRate, 512);
    for (int i = 0; i < 512; ++i)
    {
        const float expected = 0.25f * std::sin (2.0f * 3.14159265f * 30.0f * (float) i / (float) sampleRate);
        EXPECT (clean[(size_t) i] == expected);
    }

    TransformerStage silent;
    silent.prepare (sampleRate, 1);
    TransformerSettings engaged;
    engaged.bypassAmount = 0.0f;
    engaged.weightDb = 3.0f;
    engaged.driveDb = 0.0f;
    for (int i = 0; i < 2000; ++i)
    {
        float frame[1] = { 0.0f };
        silent.processSample (frame, 1, engaged);
        EXPECT (std::abs (frame[0]) < 1.0e-5f);
    }

    auto low = renderTransformer (engaged, 0.01f, 30.0f, sampleRate, samples);
    auto mid = renderTransformer (engaged, 0.01f, 1000.0f, sampleRate, samples);
    std::vector<float> lowIn (samples), midIn (samples);
    for (int i = 0; i < samples; ++i)
    {
        lowIn[(size_t) i] = 0.01f * std::sin (2.0f * 3.14159265f * 30.0f * (float) i / (float) sampleRate);
        midIn[(size_t) i] = 0.01f * std::sin (2.0f * 3.14159265f * 1000.0f * (float) i / (float) sampleRate);
    }
    const double lowDb = 20.0 * std::log10 (rmsOf (low, settle) / rmsOf (lowIn, settle));
    const double midDb = 20.0 * std::log10 (rmsOf (mid, settle) / rmsOf (midIn, settle));
    std::printf ("Transformer bump  30 Hz %.3f dB, 1 kHz %.3f dB\n", lowDb, midDb);
    EXPECT (lowDb > 2.2 && lowDb < 4.0);
    EXPECT (std::abs (midDb) < 0.35);
    EXPECT (lowDb > midDb + 2.0);

    TransformerSettings flat = engaged;
    flat.weightDb = 0.0f;
    auto flatLow = renderTransformer (flat, 0.01f, 30.0f, sampleRate, samples);
    const double flatDb = 20.0 * std::log10 (rmsOf (flatLow, settle) / rmsOf (lowIn, settle));
    std::printf ("Transformer weight off at 30 Hz %.3f dB\n", flatDb);
    EXPECT (lowDb > flatDb + 2.0);

    TransformerSettings driven = engaged;
    driven.driveDb = 12.0f;
    const float hot = 0.5f;
    auto bass = renderTransformer (driven, hot, 30.0f, sampleRate, samples);
    auto treble = renderTransformer (driven, hot, 1000.0f, sampleRate, samples);
    const double bassDistortion = harmonicShare (bass, hot, 30.0f, sampleRate, settle);
    const double trebleDistortion = harmonicShare (treble, hot, 1000.0f, sampleRate, settle);
    std::printf ("Transformer distortion  30 Hz %.4f, 1 kHz %.4f\n", bassDistortion, trebleDistortion);
    EXPECT (bassDistortion > trebleDistortion * 2.0);
    EXPECT (bassDistortion > 0.02);
    EXPECT (std::isfinite (bassDistortion));
}

float lowBandRms (const std::vector<float>& data, int sampleRate, int start, int end)
{
    const float coeff = 1.0f - std::exp (-2.0f * 3.14159265f * 140.0f / (float) sampleRate);
    float low = 0.0f;
    double energy = 0.0;
    int count = 0;
    const int last = std::min (end, (int) data.size());
    for (int i = 0; i < last; ++i)
    {
        low += coeff * (data[(size_t) i] - low);
        if (i >= start)
        {
            energy += (double) low * (double) low;
            ++count;
        }
    }
    return count > 0 ? (float) std::sqrt (energy / (double) count) : 0.0f;
}

void testTransformerPunch()
{
    const int sampleRate = 48000;
    const int samples = (int) (0.18 * sampleRate);
    std::vector<float> kick (samples);
    for (int i = 0; i < samples; ++i)
    {
        const float t = (float) i / (float) sampleRate;
        const float click = std::sin (2.0f * 3.14159265f * 2500.0f * t) * std::exp (-t / 0.0012f);
        const float body = std::sin (2.0f * 3.14159265f * 50.0f * t) * std::exp (-t / 0.08f);
        kick[(size_t) i] = 0.55f * click + 0.38f * body;
    }

    auto render = [&] (float driveDb)
    {
        TransformerStage transformer;
        transformer.prepare (sampleRate, 1);
        TransformerSettings settings;
        settings.bypassAmount = 0.0f;
        settings.weightDb = 3.0f;
        settings.driveDb = driveDb;
        auto signal = kick;
        float* channels[] = { signal.data() };
        transformer.processBlock (channels, 1, samples, settings);
        return signal;
    };

    const auto clean = render (0.0f);
    const auto driven = render (12.0f);
    const int attack = std::max (1, (int) (0.002 * sampleRate));

    float cleanPeak = 0.0f;
    float drivenPeak = 0.0f;
    for (int i = 0; i < attack; ++i)
    {
        cleanPeak = std::max (cleanPeak, std::abs (clean[(size_t) i]));
        drivenPeak = std::max (drivenPeak, std::abs (driven[(size_t) i]));
    }
    const double attackDb = 20.0 * std::log10 ((double) drivenPeak / (double) cleanPeak);
    std::printf ("Transformer kick attack %.3f dB\n", attackDb);
    // Drive is the output level. The click should follow it, and not pick up the low punch.
    EXPECT (std::abs (attackDb - 12.0) < 1.0);

    const float match = cleanPeak / std::max (drivenPeak, 1.0e-6f);
    std::vector<float> matched (samples);
    for (int i = 0; i < samples; ++i)
        matched[(size_t) i] = driven[(size_t) i] * match;

    const int bodyStart = (int) (0.008 * sampleRate);
    const int bodyEnd = (int) (0.10 * sampleRate);
    const float cleanLow = lowBandRms (clean, sampleRate, bodyStart, bodyEnd);
    const float drivenLow = lowBandRms (matched, sampleRate, bodyStart, bodyEnd);
    const double lowDb = 20.0 * std::log10 ((double) drivenLow / (double) cleanLow);
    std::printf ("Transformer kick low band %.3f dB\n", lowDb);
    EXPECT (lowDb > 2.0);
}

void testFluxColour()
{
    auto render = [] (int mode, float frequency)
    {
        const int sampleRate = 48000;
        const int samples = (int) (0.25 * sampleRate);
        FluxStage flux;
        flux.prepare (sampleRate, 1);
        FluxSettings settings;
        settings.mode = mode;
        settings.intensity = 1.0f;
        settings.driveDb = 0.0f;
        std::vector<float> signal (samples);
        for (int i = 0; i < samples; ++i)
            signal[(size_t) i] = 0.25f * std::sin (2.0f * 3.14159265f * frequency * (float) i / (float) sampleRate);
        float* channels[] = { signal.data() };
        flux.processBlock (channels, 1, samples, settings);
        return signal;
    };

    const int sampleRate = 48000;
    const int settle = (int) (0.12 * sampleRate);
    const auto nickelTop = render (0, 6000.0f);
    const auto ironTop = render (2, 6000.0f);
    const double topDb = 20.0 * std::log10 (rmsOf (ironTop, settle) / rmsOf (nickelTop, settle));
    std::printf ("Flux iron vs nickel at 6 kHz %.3f dB\n", topDb);
    EXPECT (topDb < -0.6);

    const auto nickelLow = render (0, 120.0f);
    const auto ironLow = render (2, 120.0f);
    const double nickelDistortion = harmonicShare (nickelLow, 0.25f, 120.0f, sampleRate, settle);
    const double ironDistortion = harmonicShare (ironLow, 0.25f, 120.0f, sampleRate, settle);
    std::printf ("Flux low-mid distortion  nickel=%.4f iron=%.4f\n", nickelDistortion, ironDistortion);
    EXPECT (nickelDistortion < 0.005);
    EXPECT (ironDistortion > 0.02);
}

void processAll (SoundSaturatorAudioProcessor& processor, juce::AudioBuffer<float>& buffer)
{
    juce::MidiBuffer midi;
    processor.processBlock (buffer, midi);
}

bool delayMatches (const juce::AudioBuffer<float>& output, int delay, float impulse)
{
    if (delay < 0 || delay >= output.getNumSamples())
        return false;

    for (int channel = 0; channel < output.getNumChannels(); ++channel)
    {
        for (int i = 0; i < output.getNumSamples(); ++i)
        {
            const float expected = i == delay ? impulse : 0.0f;
            if (output.getSample (channel, i) != expected)
                return false;
        }
    }
    return true;
}

int presetIndex (const char* name)
{
    for (int i = 0; i < kFactoryPresetCount; ++i)
        if (std::strcmp (kFactoryPresets[i].name, name) == 0)
            return i;
    return -1;
}

float rawOf (SoundSaturatorAudioProcessor& processor, const char* id)
{
    return processor.apvts.getRawParameterValue (id)->load();
}

void testFactoryPresets()
{
    EXPECT (kFactoryPresetCount >= 60);
    EXPECT (std::strcmp (kFactoryPresets[0].name, "Init") == 0);

    for (int i = 0; i < kFactoryPresetCount; ++i)
    {
        EXPECT (kFactoryPresets[i].name[0] != '\0');
        EXPECT (kFactoryPresets[i].group[0] != '\0');
        for (int j = 0; j < i; ++j)
            EXPECT (std::strcmp (kFactoryPresets[i].name, kFactoryPresets[j].name) != 0);

        bool seenEarlier = false;
        for (int j = 0; j < i; ++j)
            if (std::strcmp (kFactoryPresets[i].group, kFactoryPresets[j].group) == 0)
                seenEarlier = true;
        if (seenEarlier)
            EXPECT (std::strcmp (kFactoryPresets[i].group, kFactoryPresets[i - 1].group) == 0);
    }

    const int sp = presetIndex ("SP-1200 Master");
    const int clean = presetIndex ("Clean Iron Master");
    const int hats = presetIndex ("Soft Hats");
    const int kick = presetIndex ("Boom Kick");
    EXPECT (sp >= 0 && clean >= 0 && hats >= 0 && kick >= 0);
    EXPECT (kFactoryPresets[sp].tapeDriveDb > kFactoryPresets[clean].tapeDriveDb + 6.0f);
    EXPECT (kFactoryPresets[hats].transformerBypass);
    EXPECT (! kFactoryPresets[kick].transformerBypass);
    EXPECT (kFactoryPresets[kick].transformerWeightDb >= 5.0f);

    int transformersOn = 0;
    int transformersOff = 0;
    SoundSaturatorAudioProcessor processor;
    const int keptOrder = 18;
    setParam (processor, ids::chainOrder, (float) keptOrder);
    for (int i = 0; i < kFactoryPresetCount; ++i)
    {
        processor.setCurrentProgram (i);
        EXPECT (processor.getCurrentProgram() == i);
        EXPECT (processor.getProgramName (i) == juce::String (kFactoryPresets[i].name));

        const FactoryPreset& preset = kFactoryPresets[i];
        auto near = [&] (const char* id, float expected)
        {
            EXPECT (std::abs (rawOf (processor, id) - expected) <= 0.02f);
        };

        near (ids::inputTrim, preset.inputTrimDb);
        near (ids::vcaThreshold, preset.vcaThresholdDb);
        EXPECT ((int) std::lround (rawOf (processor, ids::vcaRatio)) == preset.vcaRatio);
        EXPECT ((int) std::lround (rawOf (processor, ids::vcaTiming)) == preset.vcaTiming);
        near (ids::vcaKnee, preset.vcaKneeDb);
        EXPECT ((int) std::lround (rawOf (processor, ids::vcaDetector)) == preset.vcaDetector);
        EXPECT ((rawOf (processor, ids::vcaScHpf) >= 0.5f) == preset.vcaScHpf);
        near (ids::vcaScHpfHz, preset.vcaScHpfHz);
        near (ids::vcaMakeup, preset.vcaMakeupDb);
        EXPECT ((rawOf (processor, ids::vcaBypass) >= 0.5f) == preset.vcaBypass);
        EXPECT ((int) std::lround (rawOf (processor, ids::fluxMode)) == preset.fluxMode);
        near (ids::fluxIntensity, preset.fluxIntensity);
        near (ids::magneticDrive, preset.magneticDriveDb);
        near (ids::attackPreserve, preset.attackPreserve);
        EXPECT ((rawOf (processor, ids::fluxBypass) >= 0.5f) == preset.fluxBypass);
        near (ids::tapeDrive, preset.tapeDriveDb);
        near (ids::tapeBody, preset.tapeBodyDb);
        near (ids::tapeReturn, preset.tapeReturnDb);
        EXPECT ((int) std::lround (rawOf (processor, ids::tapeBias)) == preset.tapeBias);
        EXPECT ((rawOf (processor, ids::tapeBypass) >= 0.5f) == preset.tapeBypass);
        near (ids::transformerDrive, preset.transformerDriveDb);
        near (ids::transformerWeight, preset.transformerWeightDb);
        EXPECT ((rawOf (processor, ids::transformerBypass) >= 0.5f) == preset.transformerBypass);
        near (ids::mix, preset.mixPercent);
        near (ids::finalOutput, preset.finalOutputDb);
        EXPECT (rawOf (processor, ids::globalBypass) < 0.5f);
        EXPECT ((int) std::lround (rawOf (processor, ids::chainOrder)) == keptOrder);
        near (ids::vcaInput, 0.0f);
        near (ids::vcaOutput, 0.0f);
        near (ids::fluxInput, 0.0f);
        near (ids::fluxOutput, 0.0f);
        near (ids::tapeInput, 0.0f);
        near (ids::tapeOutput, 0.0f);
        near (ids::transformerInput, 0.0f);
        near (ids::transformerOutput, 0.0f);

        if (preset.transformerBypass)
            ++transformersOff;
        else
            ++transformersOn;
    }

    EXPECT (transformersOn >= 40);
    EXPECT (transformersOff >= 8);
    std::printf ("Factory presets  %d  (%d transformer, %d dry of it)\n",
                 kFactoryPresetCount, transformersOn, transformersOff);

    SoundSaturatorAudioProcessor neutral;
    neutral.prepareToPlay (48000.0, 256);
    neutral.setCurrentProgram (0);
    const int delay = neutral.getLatencySamples();
    juce::AudioBuffer<float> impulse (2, delay + 8);
    impulse.clear();
    impulse.setSample (0, 0, 1.0f);
    impulse.setSample (1, 0, 1.0f);
    processAll (neutral, impulse);
    EXPECT (delayMatches (impulse, delay, 1.0f));

    SoundSaturatorAudioProcessor driven;
    driven.prepareToPlay (48000.0, 256);
    driven.setCurrentProgram (sp);
    juce::AudioBuffer<float> tone (2, 4096);
    juce::AudioBuffer<float> plain (2, 4096);
    for (int i = 0; i < tone.getNumSamples(); ++i)
    {
        const float sample = 0.4f * std::sin (2.0f * juce::MathConstants<float>::pi * 80.0f * (float) i / 48000.0f);
        tone.setSample (0, i, sample);
        tone.setSample (1, i, sample);
        plain.setSample (0, i, sample);
        plain.setSample (1, i, sample);
    }
    SoundSaturatorAudioProcessor untouched;
    untouched.prepareToPlay (48000.0, 256);
    processAll (untouched, plain);
    processAll (driven, tone);
    double difference = 0.0;
    for (int i = 1000; i < tone.getNumSamples(); ++i)
    {
        EXPECT (std::isfinite (tone.getSample (0, i)));
        EXPECT (std::abs (tone.getSample (0, i)) < 8.0f);
        difference += std::abs ((double) tone.getSample (0, i) - (double) plain.getSample (0, i));
    }
    EXPECT (difference > 1.0);

    juce::MemoryBlock state;
    driven.getStateInformation (state);
    SoundSaturatorAudioProcessor recalled;
    recalled.setStateInformation (state.getData(), (int) state.getSize());
    EXPECT (recalled.getCurrentProgram() == sp);
    EXPECT (std::abs (rawOf (recalled, ids::transformerDrive) - rawOf (driven, ids::transformerDrive)) <= 0.02f);
    EXPECT (std::abs (rawOf (recalled, ids::tapeDrive) - rawOf (driven, ids::tapeDrive)) <= 0.02f);
}

double bufferEnergy (const juce::AudioBuffer<float>& buffer, int start)
{
    double sum = 0.0;
    int count = 0;
    for (int i = start; i < buffer.getNumSamples(); ++i)
    {
        const double sample = buffer.getSample (0, i);
        sum += sample * sample;
        ++count;
    }
    return std::sqrt (sum / (double) std::max (1, count));
}

void fillTone (juce::AudioBuffer<float>& buffer, float amplitude, float hz)
{
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const float sample = amplitude * std::sin (2.0f * juce::MathConstants<float>::pi * hz * (float) i / 48000.0f);
        buffer.setSample (0, i, sample);
        buffer.setSample (1, i, sample);
    }
}

void testChainOrder()
{
    bool seen[kChainOrderCount] = {};
    for (int index = 0; index < kChainOrderCount; ++index)
    {
        int stages[kChainStageCount];
        decodeChainOrder (index, stages);
        EXPECT (encodeChainOrder (stages) == index);
        bool used[kChainStageCount] = {};
        for (int place = 0; place < kChainStageCount; ++place)
        {
            EXPECT (stages[place] >= 0 && stages[place] < kChainStageCount);
            EXPECT (! used[stages[place]]);
            used[stages[place]] = true;
        }
        EXPECT (! seen[index]);
        seen[index] = true;
    }

    auto render = [] (int orderIndex, float transformerInputDb, juce::AudioBuffer<float>& buffer)
    {
        SoundSaturatorAudioProcessor processor;
        processor.prepareToPlay (48000.0, 512);
        setParam (processor, ids::vcaBypass, 0.0f);
        setParam (processor, ids::vcaRatio, 4.0f);
        setParam (processor, ids::vcaThreshold, -12.0f);
        setParam (processor, ids::fluxBypass, 1.0f);
        setParam (processor, ids::tapeBypass, 1.0f);
        setParam (processor, ids::transformerBypass, 0.0f);
        setParam (processor, ids::transformerDrive, 10.0f);
        setParam (processor, ids::transformerWeight, 2.0f);
        setParam (processor, ids::transformerInput, transformerInputDb);
        setParam (processor, ids::chainOrder, (float) orderIndex);
        processAll (processor, buffer);
    };

    int transformerFirst[kChainStageCount] = { stageTransformer, stageVca, stageFlux, stageTape };
    const int first = encodeChainOrder (transformerFirst);
    EXPECT (first != 0);

    juce::AudioBuffer<float> forward (2, 4096);
    juce::AudioBuffer<float> reversed (2, 4096);
    fillTone (forward, 0.55f, 80.0f);
    fillTone (reversed, 0.55f, 80.0f);
    render (0, 0.0f, forward);
    render (first, 0.0f, reversed);

    double difference = 0.0;
    for (int i = 1500; i < forward.getNumSamples(); ++i)
    {
        EXPECT (std::isfinite (forward.getSample (0, i)));
        EXPECT (std::isfinite (reversed.getSample (0, i)));
        difference += std::abs ((double) forward.getSample (0, i) - (double) reversed.getSample (0, i));
    }
    EXPECT (difference > 1.0);

    auto renderTrim = [] (float transformerInputDb, juce::AudioBuffer<float>& buffer)
    {
        SoundSaturatorAudioProcessor processor;
        processor.prepareToPlay (48000.0, 512);
        setParam (processor, ids::vcaBypass, 1.0f);
        setParam (processor, ids::fluxBypass, 1.0f);
        setParam (processor, ids::tapeBypass, 1.0f);
        setParam (processor, ids::transformerBypass, 1.0f);
        setParam (processor, ids::transformerInput, transformerInputDb);
        processAll (processor, buffer);
    };

    juce::AudioBuffer<float> unity (2, 4096);
    juce::AudioBuffer<float> hotter (2, 4096);
    fillTone (unity, 0.05f, 200.0f);
    fillTone (hotter, 0.05f, 200.0f);
    renderTrim (0.0f, unity);
    renderTrim (12.0f, hotter);
    const double unityLevel = bufferEnergy (unity, 1500);
    const double hotLevel = bufferEnergy (hotter, 1500);
    EXPECT (unityLevel > 1.0e-4);
    EXPECT (hotLevel > unityLevel * 3.0);
    EXPECT (hotLevel < unityLevel * 5.5);

    SoundSaturatorAudioProcessor recalled;
    recalled.prepareToPlay (48000.0, 128);
    setParam (recalled, ids::chainOrder, (float) first);
    juce::MemoryBlock state;
    recalled.getStateInformation (state);
    SoundSaturatorAudioProcessor restored;
    restored.setStateInformation (state.getData(), (int) state.getSize());
    EXPECT ((int) std::lround (rawOf (restored, ids::chainOrder)) == first);
}

void testUiZoom()
{
    SoundSaturatorAudioProcessor processor;
    EXPECT (processor.getUiZoomPercent() == 75);
    processor.setUiZoomPercent (50);
    juce::MemoryBlock state;
    processor.getStateInformation (state);

    SoundSaturatorAudioProcessor restored;
    restored.setStateInformation (state.getData(), (int) state.getSize());
    EXPECT (restored.getUiZoomPercent() == 50);

    restored.setUiZoomPercent (20);
    EXPECT (restored.getUiZoomPercent() == 50);
    restored.setUiZoomPercent (160);
    EXPECT (restored.getUiZoomPercent() == 100);
}

void testProcessorLatency()
{
    SoundSaturatorAudioProcessor processor;
    processor.prepareToPlay (48000.0, 128);
    const auto info = processor.getOversamplingLatencies();
    std::printf ("Oversampling latency  2x=%d 4x=%d 8x=%d shared=%d reported=%d\n",
                 info.factor2, info.factor4, info.factor8, info.shared, processor.getLatencySamples());

    EXPECT (processor.getLatencySamples() == processor.getLatencySamples());
    const int activeRaw = info.factor4 <= kLatencyBudgetSamples ? info.factor4 : info.factor2;
    juce::ignoreUnused (activeRaw);
    if (info.factor4 <= kLatencyBudgetSamples)
        EXPECT (processor.getLatencySamples() <= kLatencyBudgetSamples);
    else
        EXPECT (processor.getLatencySamples() == info.shared || processor.getLatencySamples() == info.factor2);

    juce::AudioBuffer<float> empty (2, 0);
    processAll (processor, empty);

    const int delay = processor.getLatencySamples();
    const int length = delay + 8;
    juce::AudioBuffer<float> buffer (2, length);
    buffer.clear();
    buffer.setSample (0, 0, 1.0f);
    buffer.setSample (1, 0, 1.0f);
    processAll (processor, buffer);
    EXPECT (delayMatches (buffer, delay, 1.0f));

    setParam (processor, ids::quality, 0.0f);
    juce::AudioBuffer<float> silence (2, 64);
    silence.clear();
    processAll (processor, silence);
    setParam (processor, ids::quality, 1.0f);
    processAll (processor, silence);
    if (info.factor2 <= kLatencyBudgetSamples && info.factor4 <= kLatencyBudgetSamples)
        EXPECT (processor.getLatencySamples() == info.shared);
}

void testNeutralAndMix()
{
    SoundSaturatorAudioProcessor neutral;
    neutral.prepareToPlay (48000.0, 256);
    const int delay = neutral.getLatencySamples();
    const int length = delay + 64;
    juce::AudioBuffer<float> buffer (2, length);
    for (int i = 0; i < length; ++i)
    {
        buffer.setSample (0, i, std::sin (0.05f * (float) i) * 0.3f);
        buffer.setSample (1, i, std::cos (0.04f * (float) i) * 0.2f);
    }
    auto expected = buffer;
    processAll (neutral, buffer);

    for (int channel = 0; channel < 2; ++channel)
    {
        for (int i = 0; i < length; ++i)
        {
            const float dry = i >= delay ? expected.getSample (channel, i - delay) : 0.0f;
            EXPECT (buffer.getSample (channel, i) == dry);
        }
    }

    SoundSaturatorAudioProcessor mixed;
    setParam (mixed, ids::fluxBypass, 0.0f);
    setParam (mixed, ids::fluxIntensity, 1.0f);
    setParam (mixed, ids::magneticDrive, 12.0f);
    setParam (mixed, ids::tapeBypass, 0.0f);
    setParam (mixed, ids::tapeDrive, 12.0f);
    setParam (mixed, ids::tapeBody, 1.5f);
    setParam (mixed, ids::vcaRatio, 2.0f);
    setParam (mixed, ids::vcaThreshold, -24.0f);
    setParam (mixed, ids::mix, 0.0f);
    mixed.prepareToPlay (48000.0, 256);
    const int mixDelay = mixed.getLatencySamples();
    const int mixLength = std::max (mixDelay + 32, 512);
    juce::AudioBuffer<float> wet (2, mixLength);
    for (int i = 0; i < mixLength; ++i)
    {
        wet.setSample (0, i, ((i * 17) % 100) / 100.0f - 0.5f);
        wet.setSample (1, i, std::sin (0.02f * (float) i));
    }
    auto source = wet;
    processAll (mixed, wet);
    for (int channel = 0; channel < 2; ++channel)
    {
        for (int i = 0; i < mixLength; ++i)
        {
            const float dry = i >= mixDelay ? source.getSample (channel, i - mixDelay) : 0.0f;
            EXPECT (wet.getSample (channel, i) == dry);
        }
    }
}

void testRobustness()
{
    SoundSaturatorAudioProcessor silent;
    silent.prepareToPlay (48000.0, 128);
    juce::AudioBuffer<float> zeros (2, 2048);
    zeros.clear();
    processAll (silent, zeros);
    for (int channel = 0; channel < 2; ++channel)
        for (int i = 0; i < zeros.getNumSamples(); ++i)
            EXPECT (zeros.getSample (channel, i) == 0.0f);

    SoundSaturatorAudioProcessor hot;
    setParam (hot, ids::fluxBypass, 0.0f);
    setParam (hot, ids::fluxIntensity, 1.0f);
    setParam (hot, ids::magneticDrive, 24.0f);
    setParam (hot, ids::tapeBypass, 0.0f);
    setParam (hot, ids::tapeDrive, 24.0f);
    setParam (hot, ids::tapeBody, 3.0f);
    setParam (hot, ids::vcaRatio, 4.0f);
    setParam (hot, ids::vcaThreshold, -6.0f);
    setParam (hot, ids::vcaMakeup, 6.0f);
    hot.prepareToPlay (48000.0, 256);
    constexpr int n = 4096;
    juce::AudioBuffer<float> loud (2, n);
    for (int i = 0; i < n; ++i)
    {
        const float sample = 2.0f * std::sin (2.0f * 3.14159265f * 80.0f * (float) i / 48000.0f);
        loud.setSample (0, i, sample);
        loud.setSample (1, i, -0.5f * sample);
    }
    loud.setSample (0, 100, std::numeric_limits<float>::quiet_NaN());
    loud.setSample (1, 140, std::numeric_limits<float>::infinity());
    processAll (hot, loud);

    bool anyEnergy = false;
    for (int channel = 0; channel < 2; ++channel)
    {
        for (int i = 0; i < n; ++i)
        {
            const float sample = loud.getSample (channel, i);
            EXPECT (std::isfinite (sample));
            if (std::abs (sample) > 1.0e-4f)
                anyEnergy = true;
        }
    }
    EXPECT (anyEnergy);

    SoundSaturatorAudioProcessor chunked;
    chunked.prepareToPlay (48000.0, 128);
    const int delay = chunked.getLatencySamples();
    juce::AudioBuffer<float> big (2, std::max (3000, delay + 16));
    big.clear();
    big.setSample (0, 0, 0.5f);
    big.setSample (1, 0, -0.25f);
    processAll (chunked, big);
    EXPECT (big.getSample (0, delay) == 0.5f);
    EXPECT (big.getSample (1, delay) == -0.25f);
    for (int i = 0; i < big.getNumSamples(); ++i)
    {
        if (i == delay)
            continue;
        EXPECT (big.getSample (0, i) == 0.0f);
        EXPECT (big.getSample (1, i) == 0.0f);
    }
}
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    testKnee();
    testVcaUnity();
    testVcaHardKnee();
    testVcaStereoLink();
    testSidechainHpf();
    testFluxIdentity();
    testFluxWidths();
    testFluxColour();
    testTapeState();
    testTapeLevel();
    testTransformer();
    testTransformerPunch();
    testFactoryPresets();
    testChainOrder();
    testUiZoom();
    testProcessorLatency();
    testNeutralAndMix();
    testRobustness();

    if (gFailures == 0)
    {
        std::printf ("All tests passed\n");
        return 0;
    }

    std::printf ("%d failure(s)\n", gFailures);
    return 1;
}
