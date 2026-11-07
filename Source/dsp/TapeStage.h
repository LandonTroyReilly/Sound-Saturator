#pragma once

#include "PrototypeConstants.h"

#include <array>

// Playback low shelf. 0 dB is a true bypass so it does not change level or phase.
class PlaybackLowShelf
{
public:
    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        setGainDb (0.0f);
        reset();
    }

    void reset() noexcept
    {
        z1 = 0.0;
        z2 = 0.0;
    }

    void setGainDb (float gainDb) noexcept
    {
        if (gainDb <= 1.0e-4f)
        {
            bypassed = true;
            currentDb = 0.0f;
            return;
        }

        if (! bypassed && std::abs (gainDb - currentDb) < 0.001f)
            return;

        bypassed = false;
        currentDb = gainDb;

        const double A = std::pow (10.0, (double) gainDb / 40.0);
        const double w0 = 2.0 * 3.14159265358979323846 * (double) kTapeBodyHz / fs;
        const double cosw = std::cos (w0);
        const double sinw = std::sin (w0);
        const double alpha = sinw * 0.5 * std::sqrt ((A + 1.0 / A) * (1.0 / (double) kTapeShelfSlope - 1.0) + 2.0);
        const double sqrtA = std::sqrt (A);
        const double a0 = (A + 1.0) + (A - 1.0) * cosw + 2.0 * sqrtA * alpha;

        b0 = A * ((A + 1.0) - (A - 1.0) * cosw + 2.0 * sqrtA * alpha) / a0;
        b1 = 2.0 * A * ((A - 1.0) - (A + 1.0) * cosw) / a0;
        b2 = A * ((A + 1.0) - (A - 1.0) * cosw - 2.0 * sqrtA * alpha) / a0;
        a1 = -2.0 * ((A - 1.0) + (A + 1.0) * cosw) / a0;
        a2 = ((A + 1.0) + (A - 1.0) * cosw - 2.0 * sqrtA * alpha) / a0;
    }

    float process (float x) noexcept
    {
        if (bypassed)
            return x;

        const double y = b0 * (double) x + z1;
        z1 = b1 * (double) x - a1 * y + z2;
        z2 = b2 * (double) x - a2 * y;
        return std::isfinite (y) ? (float) y : 0.0f;
    }

private:
    double fs = 48000.0;
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
    double z1 = 0.0, z2 = 0.0;
    float currentDb = 0.0f;
    bool bypassed = true;
};

struct TapeSettings
{
    float driveDb = 0.0f;
    float bodyDb = 0.0f;
    float returnDb = 0.0f;
    int biasIndex = 1;
    float bypassAmount = 0.0f;
};

// Bias-averaged play/relaxation bank from the development plan.
// processFieldSample returns the raw normalised magnetisation M.
class TapeStage
{
public:
    void prepare (double sampleRate, int numChannels)
    {
        fs = sampleRate;
        channels = std::clamp (numChannels, 1, kMaxChannels);
        bias = -1;
        setBiasIndex (1);
        for (auto& shelf : shelves)
            shelf.prepare (sampleRate);
        reset();
    }

    void reset() noexcept
    {
        for (auto& channel : state)
            channel.fill (0.0);
        for (auto& shelf : shelves)
            shelf.reset();
    }

    void setBiasIndex (int index) noexcept
    {
        index = std::clamp (index, 0, 2);
        if (index == bias)
            return;
        bias = index;
        const auto& profile = kBiasProfiles[bias];
        for (int i = 0; i < 3; ++i)
            lambda[(size_t) i] = std::exp (-1.0 / ((double) profile.tauMs[i] * 0.001 * fs));
    }

    // One step of the published recurrence. h is the field variable, not audio.
    double processFieldSample (double h) noexcept
    {
        if (! std::isfinite (h))
        {
            state[0].fill (0.0);
            return 0.0;
        }
        return step (0, h);
    }

    void processBlock (float** channelData, int numChannels, int numSamples, const TapeSettings& settings) noexcept
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

    void processSample (float* samples, int numChannels, const TapeSettings& settings) noexcept
    {
        numChannels = std::clamp (numChannels, 1, channels);
        setBiasIndex (settings.biasIndex);

        const auto& profile = kBiasProfiles[bias];
        const float drive = std::max (1.0f, dbToGain (std::max (0.0f, settings.driveDb)));
        const float returnGain = dbToGain (settings.returnDb);
        const float bypass = std::clamp (settings.bypassAmount, 0.0f, 1.0f);
        const double scale = 1.0 / ((double) profile.a0 * (double) kField);

        for (int c = 0; c < numChannels; ++c)
        {
            const float dry = sanitize (samples[c]);
            shelves[(size_t) c].setGainDb (std::max (0.0f, settings.bodyDb));

            const double h = (double) kField * (double) drive * (double) dry;
            double magnetisation = step (c, h);
            if (! std::isfinite (magnetisation))
            {
                state[(size_t) c].fill (0.0);
                magnetisation = 0.0;
            }

            float wet = (float) (magnetisation * scale);
            wet = shelves[(size_t) c].process (wet);
            wet *= returnGain;

            float y = bypass <= 0.0f ? wet : (bypass >= 1.0f ? dry : dry + (1.0f - bypass) * (wet - dry));
            samples[c] = std::isfinite (y) ? y : 0.0f;
        }
    }

private:
    double step (int channel, double h) noexcept
    {
        const auto& profile = kBiasProfiles[bias];
        auto& p = state[(size_t) channel];

        for (int i = 0; i < 3; ++i)
        {
            double lagged = lambda[(size_t) i] * p[(size_t) i];
            const double lo = h - (double) profile.r[i];
            const double hi = h + (double) profile.r[i];
            if (lagged < lo)
                lagged = lo;
            else if (lagged > hi)
                lagged = hi;
            p[(size_t) i] = lagged;
        }

        double m = (double) profile.a0 * std::tanh (h);
        for (int i = 0; i < 3; ++i)
            m += (double) profile.a[i] * std::tanh (p[(size_t) i]);
        return m;
    }

    double fs = 48000.0;
    int channels = 2;
    int bias = -1;
    std::array<double, 3> lambda {};
    std::array<std::array<double, 3>, kMaxChannels> state {};
    std::array<PlaybackLowShelf, kMaxChannels> shelves {};
};
