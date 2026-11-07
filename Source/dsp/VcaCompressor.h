#pragma once

#include "PrototypeConstants.h"

#include <array>

// Feedforward compressor. Gain reduction only: makeup is applied by the chain
// after the Flux detector tap. One linked gain, taken from the louder channel.

struct VcaSettings
{
    float thresholdDb = -18.0f;
    float kneeDb = 6.0f;
    int ratioIndex = 0;
    int timingIndex = 3;
    int detectorIndex = 0;
    bool sidechainHpf = false;
    float sidechainHpfHz = 80.0f;
    float bypassAmount = 0.0f;
};

class VcaCompressor
{
public:
    void prepare (double sampleRate, int numChannels)
    {
        fs = sampleRate;
        channels = std::clamp (numChannels, 1, kMaxChannels);
        hpfFrequency = -1.0f;
        timingCached = -1;
        reset();
    }

    void reset() noexcept
    {
        gainEnv = 1.0;
        reductionDb = 0.0f;
        rms.fill (0.0);
        for (auto& filter : hpf)
        {
            filter.z1 = 0.0;
            filter.z2 = 0.0;
        }
    }

    // Static gain computer. Hard knee is kneeDb == 0.
    // Soft knee matches value and slope at T ± W/2.
    static double outputDb (double inputDb, double thresholdDb, double ratio, double kneeDb) noexcept
    {
        if (ratio <= 1.0000001)
            return inputDb;

        if (kneeDb <= 1.0e-8)
            return inputDb <= thresholdDb ? inputDb : thresholdDb + (inputDb - thresholdDb) / ratio;

        const double lower = thresholdDb - kneeDb * 0.5;
        const double upper = thresholdDb + kneeDb * 0.5;

        if (inputDb <= lower)
            return inputDb;
        if (inputDb >= upper)
            return thresholdDb + (inputDb - thresholdDb) / ratio;

        const double delta = inputDb - lower;
        return inputDb + (1.0 / ratio - 1.0) * delta * delta / (2.0 * kneeDb);
    }

    static double outputSlope (double inputDb, double thresholdDb, double ratio, double kneeDb) noexcept
    {
        if (ratio <= 1.0000001)
            return 1.0;

        if (kneeDb <= 1.0e-8)
            return inputDb < thresholdDb ? 1.0 : (inputDb > thresholdDb ? 1.0 / ratio : 1.0);

        const double lower = thresholdDb - kneeDb * 0.5;
        const double upper = thresholdDb + kneeDb * 0.5;

        if (inputDb <= lower)
            return 1.0;
        if (inputDb >= upper)
            return 1.0 / ratio;

        const double delta = inputDb - lower;
        return 1.0 + (1.0 / ratio - 1.0) * delta / kneeDb;
    }

    void processSample (float* samples, int numChannels, const VcaSettings& settings) noexcept
    {
        numChannels = std::clamp (numChannels, 1, channels);
        updateCoefficients (settings);

        double linked = 0.0;
        double side[kMaxChannels];

        for (int c = 0; c < numChannels; ++c)
        {
            const float raw = sanitize (samples[c]);
            samples[c] = raw;
            const double filtered = hpf[c].process (raw);
            const double detectorIn = settings.sidechainHpf ? filtered : (double) raw;
            side[c] = settings.detectorIndex == 0 ? std::abs (detectorIn) : rmsLevel (c, detectorIn, releaseCoeff);
            linked = std::max (linked, side[c]);
        }

        const float ratio = vcaRatioFromIndex (settings.ratioIndex);
        double targetGain = 1.0;

        if (ratio > 1.0001f && linked > 1.0e-8)
        {
            const double inDb = 20.0 * std::log10 (linked);
            const double outDb = outputDb (inDb, settings.thresholdDb, ratio, std::max (0.0f, settings.kneeDb));
            targetGain = std::pow (10.0, (outDb - inDb) / 20.0);
        }

        const double coeff = targetGain < gainEnv ? attackCoeff : releaseCoeff;
        gainEnv = coeff * gainEnv + (1.0 - coeff) * targetGain;

        if (! std::isfinite (gainEnv) || gainEnv < 0.0)
        {
            gainEnv = 1.0;
            rms.fill (0.0);
        }

        reductionDb = (float) std::max (0.0, -20.0 * std::log10 (std::max (gainEnv, 1.0e-8)));
        const float bypass = std::clamp (settings.bypassAmount, 0.0f, 1.0f);
        const float applied = (float) gainEnv;

        for (int c = 0; c < numChannels; ++c)
        {
            const float dry = samples[c];
            const float wet = dry * applied;
            float y = bypass <= 0.0f ? wet : (bypass >= 1.0f ? dry : dry + (1.0f - bypass) * (wet - dry));
            samples[c] = std::isfinite (y) ? y : 0.0f;
        }
    }

    void processBlock (float** channelData, int numChannels, int numSamples, const VcaSettings& settings) noexcept
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

    float gainReductionDb() const noexcept { return reductionDb; }

private:
    struct Biquad
    {
        double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
        double z1 = 0.0, z2 = 0.0;

        void setHighpass (double sampleRate, double frequency) noexcept
        {
            const double w0 = 2.0 * 3.14159265358979323846 * frequency / sampleRate;
            const double cosw = std::cos (w0);
            const double sinw = std::sin (w0);
            const double q = 0.7071067811865476;
            const double alpha = sinw / (2.0 * q);
            const double a0 = 1.0 + alpha;
            b0 = ((1.0 + cosw) * 0.5) / a0;
            b1 = (-(1.0 + cosw)) / a0;
            b2 = b0;
            a1 = (-2.0 * cosw) / a0;
            a2 = (1.0 - alpha) / a0;
        }

        double process (double x) noexcept
        {
            const double y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
    };

    void updateCoefficients (const VcaSettings& settings) noexcept
    {
        if (settings.timingIndex != timingCached)
        {
            timingCached = std::clamp (settings.timingIndex, 0, 9);
            const auto timing = kVcaTiming[timingCached];
            attackCoeff = std::exp (-1.0 / (timing.attackMs * 0.001 * fs));
            releaseCoeff = std::exp (-1.0 / (timing.releaseMs * 0.001 * fs));
        }

        const float hz = std::clamp (settings.sidechainHpfHz, 20.0f, 250.0f);
        if (std::abs (hz - hpfFrequency) > 0.05f)
        {
            hpfFrequency = hz;
            for (int c = 0; c < channels; ++c)
                hpf[c].setHighpass (fs, hz);
        }
    }

    double rmsLevel (int channel, double x, double coeff) noexcept
    {
        auto& meanSq = rms[(size_t) channel];
        meanSq = coeff * meanSq + (1.0 - coeff) * x * x;
        if (meanSq < 0.0)
            meanSq = 0.0;
        return std::sqrt (meanSq);
    }

    double fs = 48000.0;
    int channels = 2;
    int timingCached = -1;
    float hpfFrequency = -1.0f;
    double attackCoeff = 0.0;
    double releaseCoeff = 0.0;
    double gainEnv = 1.0;
    float reductionDb = 0.0f;
    std::array<double, kMaxChannels> rms {};
    std::array<Biquad, kMaxChannels> hpf {};
};
