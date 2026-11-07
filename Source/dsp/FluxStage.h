#pragma once

#include "PrototypeConstants.h"

#include <array>

// Flux candidate A. Intensity 0 is exact identity.
// u = x + I * mF * (tanh(g * x) / g - x)

struct FluxSettings
{
    int mode = 1;
    float intensity = 0.0f;
    float driveDb = 0.0f;
    float attackPreserve = 0.0f;
    float bypassAmount = 0.0f;
};

class FluxStage
{
public:
    void prepare (double sampleRate, int numChannels)
    {
        fs = sampleRate;
        channels = std::clamp (numChannels, 1, kMaxChannels);
        reset();
    }

    void reset() noexcept
    {
        fast.fill (0.0f);
        slow.fill (0.0f);
        mask = 0.0f;
    }

    // Onset tap. Call with the post-VCA, pre-makeup signal.
    void analyse (const float* preMakeup, int numChannels, const FluxSettings& settings) noexcept
    {
        numChannels = std::clamp (numChannels, 1, channels);
        const float window = fluxWindowSeconds (settings.mode);
        const float silence = silenceLinear();

        float linkedOnset = 0.0f;
        float linkedPeak = 0.0f;

        for (int c = 0; c < numChannels; ++c)
        {
            const float level = std::abs (sanitize (preMakeup[c]));
            fast[(size_t) c] = follow (fast[(size_t) c], level, kFluxFastAttackSeconds, kFluxFastReleaseSeconds);
            slow[(size_t) c] = follow (slow[(size_t) c], level, window, window * kFluxSlowReleaseScale);

            const float peak = fast[(size_t) c];
            linkedPeak = std::max (linkedPeak, peak);
            if (peak > silence)
                linkedOnset = std::max (linkedOnset, std::max (0.0f, peak - slow[(size_t) c]) / peak);
        }

        float onset = linkedPeak < silence ? 0.0f : linkedOnset;
        const float preserve = std::clamp (settings.attackPreserve, 0.0f, 1.0f);

        if (onset >= mask)
        {
            if (preserve <= 1.0e-4f)
                mask = onset;
            else
                mask = follow (mask, onset, preserve * window, window);
        }
        else
        {
            mask = follow (mask, onset, window, window);
        }

        if (! std::isfinite (mask))
            mask = 0.0f;
    }

    // Waveshaper. Call with the post-makeup signal. Each channel keeps its own sample.
    void shape (float* postMakeup, int numChannels, const FluxSettings& settings) noexcept
    {
        numChannels = std::clamp (numChannels, 1, channels);
        const float intensity = std::clamp (settings.intensity, 0.0f, 1.0f);
        const float g = std::max (1.0f, dbToGain (std::max (0.0f, settings.driveDb)));
        const float bypass = std::clamp (settings.bypassAmount, 0.0f, 1.0f);

        for (int c = 0; c < numChannels; ++c)
        {
            const float x = sanitize (postMakeup[c]);
            const float residual = std::tanh (g * x) / g - x;
            float wet = x + intensity * mask * residual;

            if (bypass >= 1.0f || intensity <= 0.0f)
                wet = x;
            else if (bypass > 0.0f)
                wet = x + (1.0f - bypass) * (wet - x);

            postMakeup[c] = std::isfinite (wet) ? wet : 0.0f;
        }
    }

    void processBlock (float** channelData, int numChannels, int numSamples, const FluxSettings& settings) noexcept
    {
        for (int i = 0; i < numSamples; ++i)
        {
            float frame[kMaxChannels];
            for (int c = 0; c < numChannels; ++c)
                frame[c] = channelData[c][i];
            analyse (frame, numChannels, settings);
            shape (frame, numChannels, settings);
            for (int c = 0; c < numChannels; ++c)
                channelData[c][i] = frame[c];
        }
    }

    float maskValue() const noexcept { return mask; }

private:
    float follow (float env, float target, float attackSeconds, float releaseSeconds) const noexcept
    {
        const float tau = target > env ? attackSeconds : releaseSeconds;
        if (tau <= 1.0e-8f)
            return target;
        const float coeff = std::exp (-1.0f / (tau * (float) fs));
        return coeff * env + (1.0f - coeff) * target;
    }

    double fs = 48000.0;
    int channels = 2;
    std::array<float, kMaxChannels> fast {};
    std::array<float, kMaxChannels> slow {};
    float mask = 0.0f;
};
