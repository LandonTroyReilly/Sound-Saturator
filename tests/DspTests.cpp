#include "PluginProcessor.h"
#include "dsp/FluxStage.h"
#include "dsp/TapeStage.h"
#include "dsp/VcaCompressor.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
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
        parameter->setValue (parameter->convertTo0to1 (raw));
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
    testTapeState();
    testTapeLevel();
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
