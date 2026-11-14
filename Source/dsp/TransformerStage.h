#pragma once

#include "PrototypeConstants.h"

#include <array>
#include <cmath>

// Output transformer, the Magnetismus 2 output stage.
// Flux is the integral of voltage, so the kick body saturates before the click.
// Weight is a 30 Hz head bump after the core. Drive fills that bump in while the
// low end is working, then stops adding it once the core is already saturated.
// A little hysteresis thickens moderate bass. The click stays on the dry path.

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
        const float pi = 3.14159265358979323846f;
        dcCoeff = std::exp (-2.0f * pi * 12.0f / (float) fs);
        hfCoeff = 1.0f - std::exp (-2.0f * pi * 18000.0f / (float) fs);
        retain = std::exp (-1.0f / (0.5f * (float) fs));
        hystCoeff = 1.0f - std::exp (-2.0f * pi * 35.0f / (float) fs);
        lowCoeff = 1.0f - std::exp (-2.0f * pi * 140.0f / (float) fs);
        punchAttack = 1.0f - std::exp (-1.0f / (0.006f * (float) fs));
        punchRelease = 1.0f - std::exp (-1.0f / (0.080f * (float) fs));
        const float scale = std::cosh (kSaturation * kBias);
        restScale = scale * scale;
        restValue = std::tanh (kSaturation * kBias) / kSaturation * restScale;
        bumpDb = -1000.0f;
        reset();
    }

    void reset() noexcept
    {
        for (auto& channel : cores)
            channel.reset (restValue);
        for (auto& bump : bumps)
            bump.reset();
        for (auto& punch : punches)
            punch.reset();
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
        const float weight = std::clamp (settings.weightDb, 0.0f, kHeadBumpDb);
        configureBump (weight);

        for (int c = 0; c < numChannels; ++c)
        {
            const float dry = sanitize (samples[c]);
            float wet = cores[(size_t) c].process (dry, drive, (float) fs, dcCoeff, hfCoeff, retain,
                                                   hystCoeff, restScale, restValue);
            if (! std::isfinite (wet))
            {
                cores[(size_t) c].reset (restValue);
                bumps[(size_t) c].reset();
                punches[(size_t) c].reset();
                wet = 0.0f;
            }

            wet = bumps[(size_t) c].process (wet);

            auto& punch = punches[(size_t) c];
            const float opened = punchAmount (cores[(size_t) c].fluxMagnitude);
            const float follow = opened > punch.env ? punchAttack : punchRelease;
            punch.env += follow * (opened - punch.env);

            // Drive is broadband, like the hardware Output knob, so the click comes up too.
            // This extra gain is lowpassed, and it only opens while the core is working the bass.
            punch.low += lowCoeff * (wet - punch.low);
            const float extra = dbToGain (punch.env * kPunchDb);
            wet += punch.low * (extra - 1.0f);

            if (! std::isfinite (wet))
            {
                cores[(size_t) c].reset (restValue);
                bumps[(size_t) c].reset();
                punch.reset();
                wet = 0.0f;
            }

            samples[c] = bypass >= 1.0f ? dry : (bypass <= 0.0f ? wet : dry + (1.0f - bypass) * (wet - dry));
        }
    }

private:
    static constexpr float kSaturation = 1.15f;
    static constexpr float kBias = 0.045f;
    static constexpr float kFluxGain = 380.0f;
    static constexpr float kHeadBumpDb = 6.0f;
    static constexpr float kPunchDb = 10.0f;
    static constexpr float kHystDepth = 0.20f;

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
            hyst = 0.0f;
            fluxMagnitude = 0.0f;
        }

        float process (float x, float drive, float sampleRate, float dcR, float hfA, float leak,
                       float hystA, float scale, float rest) noexcept
        {
            const float blocked = x - dcIn + dcR * dcOut;
            dcIn = x;
            dcOut = blocked;

            flux = flux * leak + blocked * drive * (kFluxGain / sampleRate);
            flux = std::clamp (flux, -6.0f, 6.0f);
            fluxMagnitude = std::abs (flux);

            // Dead-zone so a quiet 30 Hz tone stays linear. Once the kick body
            // is in the core, a lagged soft-sign bends the loop.
            const float depth = std::clamp ((fluxMagnitude - 0.40f) / 0.90f, 0.0f, 1.0f);
            const float aim = std::tanh (flux * 2.5f);
            hyst += hystA * (aim - hyst);
            const float loop = depth * kHystDepth * hyst;

            const float saturated = std::tanh (kSaturation * (flux + loop + kBias)) / kSaturation * scale;
            const float voltage = (saturated - previous) * (sampleRate / kFluxGain);
            previous = saturated;

            hf += hfA * (voltage - hf);
            if (std::abs (flux) < 1.0e-8f && std::abs (blocked) < 1.0e-8f)
            {
                flux = 0.0f;
                previous = rest;
                hf = 0.0f;
                hyst = 0.0f;
                fluxMagnitude = 0.0f;
            }
            return hf;
        }

        float flux = 0.0f;
        float previous = 0.0f;
        float dcIn = 0.0f;
        float dcOut = 0.0f;
        float hf = 0.0f;
        float hyst = 0.0f;
        float fluxMagnitude = 0.0f;
    };

    struct Punch
    {
        void reset() noexcept
        {
            env = 0.0f;
            low = 0.0f;
        }

        float env = 0.0f;
        float low = 0.0f;
    };

    void configureBump (float gainDb) noexcept
    {
        if (std::abs (gainDb - bumpDb) < 0.01f)
            return;

        bumpDb = gainDb;
        for (int c = 0; c < channels; ++c)
            bumps[(size_t) c].setGainDb (fs, gainDb);
    }

    // Opens through the punch zone, then eases off so a slammed core rounds
    // instead of growing a bigger boom.
    static float punchAmount (float flux) noexcept
    {
        if (flux < 0.35f)
            return 0.0f;
        if (flux < 1.15f)
            return (flux - 0.35f) / 0.80f;
        if (flux < 3.2f)
            return 1.0f - 0.75f * (flux - 1.15f) / 2.05f;
        return 0.25f;
    }

    double fs = 48000.0;
    int channels = 1;
    float dcCoeff = 0.0f;
    float hfCoeff = 1.0f;
    float retain = 1.0f;
    float hystCoeff = 0.0f;
    float lowCoeff = 0.0f;
    float punchAttack = 1.0f;
    float punchRelease = 1.0f;
    float bumpDb = -1000.0f;
    float restScale = 1.0f;
    float restValue = 0.0f;
    std::array<Bump, kMaxChannels> bumps;
    std::array<Core, kMaxChannels> cores;
    std::array<Punch, kMaxChannels> punches;
};
