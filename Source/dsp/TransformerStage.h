#pragma once

#include "PrototypeConstants.h"

#include <array>
#include <cmath>

// Output transformer. Flux is the integral of voltage, so bass saturates first.
// The 30 Hz resonance is the head bump; leakage inductance rounds off the top.

struct TransformerSettings
{
    float driveDb = 0.0f;
    float weightDb = 3.0f;
    float bypassAmount = 1.0f;
};

class TransformerStage
{
public:
    void prepare (double sampleRate, int numChannels)
    {
        fs = sampleRate;
        channels = std::clamp (numChannels, 1, kMaxChannels);
        bumpDb = -1000.0f;
        dcCoeff = (float) std::exp (-2.0 * 3.14159265358979323846 * 12.0 / fs);
        hfCoeff = 1.0f - (float) std::exp (-2.0 * 3.14159265358979323846 * 18000.0 / fs);
        retain = (float) std::exp (-1.0 / (0.5 * fs));
        const float scale = std::cosh (kSaturation * kBias);
        restScale = scale * scale;
        restValue = std::tanh (kSaturation * kBias) / kSaturation * restScale;
        configureBump (3.0f);
        reset();
    }

    void reset() noexcept
    {
        for (auto& channel : cores)
            channel.reset (restValue);
        for (auto& bump : bumps)
            bump.reset();
    }

    void processBlock (float** channelData, int numChannels, int numSamples, const TransformerSettings& settings) noexcept
    {
        for (int i = 0; i < numSamples; ++i)
        {
            float frame[kMaxChannels];
            for (int c = 0; c < numChannels; ++c)
                frame[c] = channelData[c][i];
            processSample (frame, numChannels, settings);
            for (int c = 0; c < numChannels; ++c)
                channelData[c][i] = frame[c];
        }
    }

    void processSample (float* samples, int numChannels, const TransformerSettings& settings) noexcept
    {
        numChannels = std::clamp (numChannels, 1, channels);
        const float drive = std::max (1.0f, dbToGain (std::max (0.0f, settings.driveDb)));
        const float bypass = std::clamp (settings.bypassAmount, 0.0f, 1.0f);
        configureBump (std::max (0.0f, settings.weightDb));

        for (int c = 0; c < numChannels; ++c)
        {
            const float dry = sanitize (samples[c]);
            float wet = bumps[(size_t) c].process (dry);
            wet = cores[(size_t) c].process (wet, drive, (float) fs, dcCoeff, hfCoeff, retain, restScale, restValue);
            if (! std::isfinite (wet))
            {
                cores[(size_t) c].reset (restValue);
                bumps[(size_t) c].reset();
                wet = 0.0f;
            }

            samples[c] = bypass >= 1.0f ? dry : (bypass <= 0.0f ? wet : dry + (1.0f - bypass) * (wet - dry));
        }
    }

private:
    static constexpr float kSaturation = 1.15f;
    static constexpr float kBias = 0.045f;
    static constexpr float kFluxGain = 380.0f;

    struct Bump
    {
        void setGainDb (double sampleRate, float gainDb) noexcept
        {
            const double A = std::pow (10.0, (double) gainDb / 40.0);
            const double w0 = 2.0 * 3.14159265358979323846 * (double) kTransformerHz / sampleRate;
            const double cosw = std::cos (w0);
            const double sinw = std::sin (w0);
            const double alpha = sinw / (2.0 * 0.85);
            const double a0 = 1.0 + alpha / A;
            b0 = (1.0 + alpha * A) / a0;
            b1 = (-2.0 * cosw) / a0;
            b2 = (1.0 - alpha * A) / a0;
            a1 = (-2.0 * cosw) / a0;
            a2 = (1.0 - alpha / A) / a0;
        }

        void reset() noexcept
        {
            z1 = 0.0;
            z2 = 0.0;
        }

        float process (float x) noexcept
        {
            const double y = b0 * (double) x + z1;
            z1 = b1 * (double) x - a1 * y + z2;
            z2 = b2 * (double) x - a2 * y;
            return (float) y;
        }

        double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
        double z1 = 0.0, z2 = 0.0;
    };

    struct Core
    {
        void reset (float rest) noexcept
        {
            flux = 0.0f;
            previous = rest;
            dcIn = 0.0f;
            dcOut = 0.0f;
            hf = 0.0f;
        }

        float process (float x, float drive, float sampleRate, float dcR, float hfA, float leak,
                       float scale, float rest) noexcept
        {
            const float blocked = x - dcIn + dcR * dcOut;
            dcIn = x;
            dcOut = blocked;

            flux = flux * leak + blocked * drive * (kFluxGain / sampleRate);
            flux = std::clamp (flux, -6.0f, 6.0f);

            const float saturated = std::tanh (kSaturation * (flux + kBias)) / kSaturation * scale;
            const float voltage = (saturated - previous) * (sampleRate / kFluxGain);
            previous = saturated;

            hf += hfA * (voltage - hf);
            if (std::abs (flux) < 1.0e-8f && std::abs (blocked) < 1.0e-8f)
            {
                flux = 0.0f;
                previous = rest;
                hf = 0.0f;
            }
            return hf;
        }

        float flux = 0.0f;
        float previous = 0.0f;
        float dcIn = 0.0f;
        float dcOut = 0.0f;
        float hf = 0.0f;
    };

    void configureBump (float gainDb) noexcept
    {
        if (std::abs (gainDb - bumpDb) < 0.01f)
            return;

        bumpDb = gainDb;
        for (int c = 0; c < channels; ++c)
            bumps[(size_t) c].setGainDb (fs, gainDb);
    }

    double fs = 48000.0;
    int channels = 1;
    float dcCoeff = 0.0f;
    float hfCoeff = 1.0f;
    float retain = 1.0f;
    float bumpDb = -1000.0f;
    float restScale = 1.0f;
    float restValue = 0.0f;
    std::array<Bump, kMaxChannels> bumps;
    std::array<Core, kMaxChannels> cores;
};
