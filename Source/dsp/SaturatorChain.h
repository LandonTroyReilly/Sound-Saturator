#pragma once

#include "FluxStage.h"
#include "Parameters.h"
#include "TapeStage.h"
#include "VcaCompressor.h"

#include <juce_dsp/juce_dsp.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <vector>

struct ChainMeters
{
    std::atomic<float>* gainReductionDb = nullptr;
    std::atomic<float>* outputPeak = nullptr;
};

struct OversamplingLatencies
{
    int factor2 = 0;
    int factor4 = 0;
    int factor8 = 0;
    int shared = 0;
};

// Host-rate dry tap, oversampled VCA / Flux / tape, aligned mix, final output.
// Extension points for the later Pre EQ, colour, tube, output iron and Post EQ
// stages are marked in process().
class SaturatorChain
{
public:
    void prepare (double sampleRate, int maximumBlockSize, int numChannels)
    {
        hostRate = sampleRate;
        channels = std::clamp (numChannels, 1, kMaxChannels);
        maxBlock = std::max (1, maximumBlockSize);
        needsSnap = true;

        for (int i = 0; i < 3; ++i)
            preparePath (i);

        latency2 = paths[0].osLatency;
        latency4 = paths[1].osLatency;
        latency8 = paths[2].osLatency;
        sharedLatency = 0;
        considerShared (latency2);
        considerShared (latency4);
        considerShared (latency8);
        if (sharedLatency == 0)
            sharedLatency = latency2;

        const int delayCap = std::max ({ 8, sharedLatency, latency2, latency4, latency8 }) + 8;
        dryDelay.prepare (delayCap, channels);
        for (auto& path : paths)
            path.pad.prepare (delayCap, channels);

        wetA.setSize (channels, maxBlock, false, false, true);
        wetB.setSize (channels, maxBlock, false, false, true);

        const double rate = hostRate;
        inputTrim.reset (rate, kSmoothSeconds);
        mix.reset (rate, kSmoothSeconds);
        outputGain.reset (rate, kSmoothSeconds);
        identity.reset (rate, kCrossfadeSeconds);
        globalBypass.reset (rate, kCrossfadeSeconds);
        qualityFade.reset (rate, kCrossfadeSeconds);

        meterPeak = 0.0f;
        renderedQuality = 1;
        incomingQuality = -1;
        reportedLatency = latencyForQuality (renderedQuality);
        dryDelay.setDelay (reportedLatency);
        dryDelay.reset();
    }

    void selectQuality (int qualityIndex) noexcept
    {
        renderedQuality = std::clamp (qualityIndex, 0, 2);
        requestedQuality = renderedQuality;
        incomingQuality = -1;
        reportedLatency = latencyForQuality (renderedQuality);
        dryDelay.setDelay (reportedLatency);
    }

    void reset() noexcept
    {
        dryDelay.reset();
        for (auto& path : paths)
        {
            path.vca.reset();
            path.flux.reset();
            path.tape.reset();
            path.pad.reset();
            if (path.oversampling != nullptr)
                path.oversampling->reset();
        }
        meterPeak = 0.0f;
    }

    int recommendDefaultQuality() const noexcept
    {
        return latency4 <= kLatencyBudgetSamples ? 1 : 0;
    }

    int latencyForQuality (int qualityIndex) const noexcept
    {
        const int raw = rawLatency (qualityIndex);
        if (raw <= kLatencyBudgetSamples)
            return sharedLatency;
        return raw;
    }

    int currentLatency() const noexcept { return reportedLatency; }

    OversamplingLatencies latencies() const noexcept
    {
        return { latency2, latency4, latency8, sharedLatency };
    }

    void process (juce::AudioBuffer<float>& buffer, int offset, int numSamples, const ParameterSnapshot& snapshot, ChainMeters meters)
    {
        if (numSamples <= 0)
            return;

        numSamples = std::min (numSamples, maxBlock);

        for (int i = 0; i < numSamples; ++i)
            for (int c = 0; c < channels; ++c)
                buffer.setSample (c, offset + i, sanitize (buffer.getSample (c, offset + i)));

        if (needsSnap)
        {
            snapTo (snapshot);
            needsSnap = false;
        }

        setHostTargets (snapshot);
        updateQuality (snapshot);

        const bool wantWet = identity.getCurrentValue() < 0.999f || identity.getTargetValue() < 0.5f;
        const int dryLatency = latencyForQuality (requestedQuality);
        if (dryLatency != reportedLatency)
        {
            reportedLatency = dryLatency;
            dryDelay.setDelay (reportedLatency);
        }

        // Extension: Pre EQ runs at the host rate on this trimmed wet signal, before oversampling.
        if (wantWet)
        {
            copyTrimmed (buffer, offset, numSamples, wetA);
            renderPath (renderedQuality, wetA, numSamples, snapshot);
            if (incomingQuality >= 0)
            {
                copyTrimmed (buffer, offset, numSamples, wetB);
                renderPath (incomingQuality, wetB, numSamples, snapshot);
            }
        }

        float blockPeak = 0.0f;
        const bool fading = incomingQuality >= 0;

        for (int i = 0; i < numSamples; ++i)
        {
            (void) inputTrim.getNextValue();
            const float wetMix = std::clamp (mix.getNextValue(), 0.0f, 1.0f);
            const float outDb = outputGain.getNextValue();
            const float outLin = dbToGain (outDb);
            const float id = std::clamp (identity.getNextValue(), 0.0f, 1.0f);
            const float bypass = std::clamp (globalBypass.getNextValue(), 0.0f, 1.0f);
            const float fade = fading ? std::clamp (qualityFade.getNextValue(), 0.0f, 1.0f) : 0.0f;

            for (int c = 0; c < channels; ++c)
            {
                const float drySample = dryDelay.process (c, buffer.getSample (c, offset + i));
                float processed = drySample;

                if (wantWet)
                {
                    const float from = wetA.getSample (c, i);
                    if (! fading || fade <= 0.0f)
                        processed = from;
                    else if (fade >= 1.0f)
                        processed = wetB.getSample (c, i);
                    else
                        processed = from + fade * (wetB.getSample (c, i) - from);

                    if (id >= 1.0f)
                        processed = drySample;
                    else if (id > 0.0f)
                        processed = drySample + (1.0f - id) * (processed - drySample);
                }

                // Extension: Post EQ is a host-rate stage on `processed`, before the global mix.
                float y = wetMix <= 0.0f ? drySample : (wetMix >= 1.0f ? processed : drySample + wetMix * (processed - drySample));
                if (outLin != 1.0f)
                    y *= outLin;
                if (bypass >= 1.0f)
                    y = drySample;
                else if (bypass > 0.0f)
                    y = drySample + (1.0f - bypass) * (y - drySample);

                if (! std::isfinite (y))
                {
                    y = 0.0f;
                    armReset = true;
                }

                buffer.setSample (c, offset + i, y);
                blockPeak = std::max (blockPeak, std::abs (y));
            }

            dryDelay.advance();
        }

        if (fading && qualityFade.getCurrentValue() >= 0.999f)
        {
            renderedQuality = incomingQuality;
            incomingQuality = -1;
        }

        if (armReset)
        {
            reset();
            armReset = false;
        }

        const float release = std::exp (-(float) numSamples / (0.35f * (float) hostRate));
        if (blockPeak >= meterPeak)
            meterPeak = blockPeak;
        else
            meterPeak = blockPeak + release * (meterPeak - blockPeak);

        if (meters.outputPeak != nullptr)
            meters.outputPeak->store (meterPeak, std::memory_order_relaxed);

        float reduction = 0.0f;
        if (wantWet)
        {
            const int grIndex = incomingQuality >= 0 && qualityFade.getCurrentValue() >= 0.5f ? incomingQuality : renderedQuality;
            reduction = paths[(size_t) std::clamp (grIndex, 0, 2)].vca.gainReductionDb();
        }
        if (meters.gainReductionDb != nullptr)
            meters.gainReductionDb->store (reduction, std::memory_order_relaxed);
    }

private:
    class IntegerDelay
    {
    public:
        void prepare (int maxDelaySamples, int numChannels)
        {
            maxDelay = std::max (1, maxDelaySamples);
            count = std::clamp (numChannels, 1, kMaxChannels);
            const int size = maxDelay + 1;
            for (int c = 0; c < count; ++c)
                data[(size_t) c].assign ((size_t) size, 0.0f);
            write = 0;
            delay = 0;
        }

        void setDelay (int samples) noexcept
        {
            delay = std::clamp (samples, 0, maxDelay);
        }

        void reset() noexcept
        {
            for (int c = 0; c < count; ++c)
                std::fill (data[(size_t) c].begin(), data[(size_t) c].end(), 0.0f);
            write = 0;
        }

        float process (int channel, float x) noexcept
        {
            auto& channelData = data[(size_t) channel];
            const int size = (int) channelData.size();
            channelData[(size_t) write] = x;
            int read = write - delay;
            if (read < 0)
                read += size;
            return channelData[(size_t) read];
        }

        void advance() noexcept
        {
            const int size = (int) data[0].size();
            if (++write >= size)
                write = 0;
        }

    private:
        int maxDelay = 1;
        int count = 1;
        int write = 0;
        int delay = 0;
        std::array<std::vector<float>, kMaxChannels> data;
    };

    struct Path
    {
        std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
        VcaCompressor vca;
        FluxStage flux;
        TapeStage tape;
        IntegerDelay pad;
        int osLatency = 0;
        double osRate = 48000.0;

        juce::SmoothedValue<float> threshold;
        juce::SmoothedValue<float> knee;
        juce::SmoothedValue<float> makeup;
        juce::SmoothedValue<float> vcaBypass;
        juce::SmoothedValue<float> sidechainHz;
        juce::SmoothedValue<float> intensity;
        juce::SmoothedValue<float> magneticDrive;
        juce::SmoothedValue<float> attackPreserve;
        juce::SmoothedValue<float> fluxBypass;
        juce::SmoothedValue<float> tapeDrive;
        juce::SmoothedValue<float> tapeBody;
        juce::SmoothedValue<float> tapeReturn;
        juce::SmoothedValue<float> tapeBypass;

        void prepareSmoothers (double rate)
        {
            threshold.reset (rate, kSmoothSeconds);
            knee.reset (rate, kSmoothSeconds);
            makeup.reset (rate, kSmoothSeconds);
            vcaBypass.reset (rate, kCrossfadeSeconds);
            sidechainHz.reset (rate, kSmoothSeconds);
            intensity.reset (rate, kSmoothSeconds);
            magneticDrive.reset (rate, kSmoothSeconds);
            attackPreserve.reset (rate, kSmoothSeconds);
            fluxBypass.reset (rate, kCrossfadeSeconds);
            tapeDrive.reset (rate, kSmoothSeconds);
            tapeBody.reset (rate, kSmoothSeconds);
            tapeReturn.reset (rate, kSmoothSeconds);
            tapeBypass.reset (rate, kCrossfadeSeconds);
        }

        void snap (const ParameterSnapshot& snapshot)
        {
            threshold.setCurrentAndTargetValue (snapshot.vcaThresholdDb);
            knee.setCurrentAndTargetValue (snapshot.vcaKneeDb);
            makeup.setCurrentAndTargetValue (snapshot.vcaMakeupDb);
            vcaBypass.setCurrentAndTargetValue (snapshot.vcaBypass ? 1.0f : 0.0f);
            sidechainHz.setCurrentAndTargetValue (snapshot.vcaScHpfHz);
            intensity.setCurrentAndTargetValue (snapshot.fluxIntensity);
            magneticDrive.setCurrentAndTargetValue (snapshot.magneticDriveDb);
            attackPreserve.setCurrentAndTargetValue (snapshot.attackPreserve);
            fluxBypass.setCurrentAndTargetValue (snapshot.fluxBypass ? 1.0f : 0.0f);
            tapeDrive.setCurrentAndTargetValue (snapshot.tapeDriveDb);
            tapeBody.setCurrentAndTargetValue (snapshot.tapeBodyDb);
            tapeReturn.setCurrentAndTargetValue (snapshot.tapeReturnDb);
            tapeBypass.setCurrentAndTargetValue (snapshot.tapeBypass ? 1.0f : 0.0f);
        }

        void setTargets (const ParameterSnapshot& snapshot)
        {
            threshold.setTargetValue (snapshot.vcaThresholdDb);
            knee.setTargetValue (snapshot.vcaKneeDb);
            makeup.setTargetValue (snapshot.vcaMakeupDb);
            vcaBypass.setTargetValue (snapshot.vcaBypass ? 1.0f : 0.0f);
            sidechainHz.setTargetValue (snapshot.vcaScHpfHz);
            intensity.setTargetValue (snapshot.fluxIntensity);
            magneticDrive.setTargetValue (snapshot.magneticDriveDb);
            attackPreserve.setTargetValue (snapshot.attackPreserve);
            fluxBypass.setTargetValue (snapshot.fluxBypass ? 1.0f : 0.0f);
            tapeDrive.setTargetValue (snapshot.tapeDriveDb);
            tapeBody.setTargetValue (snapshot.tapeBodyDb);
            tapeReturn.setTargetValue (snapshot.tapeReturnDb);
            tapeBypass.setTargetValue (snapshot.tapeBypass ? 1.0f : 0.0f);
        }
    };

    void preparePath (int index)
    {
        auto& path = paths[(size_t) index];
        path.oversampling = std::make_unique<juce::dsp::Oversampling<float>> (
            (size_t) channels,
            (size_t) (index + 1),
            juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple,
            false,
            true);
        path.oversampling->initProcessing ((size_t) maxBlock);
        path.osLatency = (int) path.oversampling->getLatencyInSamples();
        path.osRate = hostRate * (double) (1 << (index + 1));
        path.vca.prepare (path.osRate, channels);
        path.flux.prepare (path.osRate, channels);
        path.tape.prepare (path.osRate, channels);
        path.prepareSmoothers (path.osRate);
        path.oversampling->reset();
    }

    void considerShared (int latency) noexcept
    {
        if (latency <= kLatencyBudgetSamples)
            sharedLatency = std::max (sharedLatency, latency);
    }

    int rawLatency (int qualityIndex) const noexcept
    {
        switch (std::clamp (qualityIndex, 0, 2))
        {
            case 0: return latency2;
            case 1: return latency4;
            default: return latency8;
        }
    }

    static bool isIdentity (const ParameterSnapshot& snapshot) noexcept
    {
        const bool vcaNeutral = snapshot.vcaBypass
                             || (snapshot.vcaRatioIndex == 0 && std::abs (snapshot.vcaMakeupDb) <= 1.0e-3f);
        return vcaNeutral
            && snapshot.fluxBypass
            && snapshot.tapeBypass
            && std::abs (snapshot.inputTrimDb) <= 1.0e-3f;
    }

    void snapTo (const ParameterSnapshot& snapshot)
    {
        inputTrim.setCurrentAndTargetValue (snapshot.inputTrimDb);
        mix.setCurrentAndTargetValue (snapshot.mixPercent * 0.01f);
        outputGain.setCurrentAndTargetValue (snapshot.finalOutputDb);
        identity.setCurrentAndTargetValue (isIdentity (snapshot) ? 1.0f : 0.0f);
        globalBypass.setCurrentAndTargetValue (snapshot.globalBypass ? 1.0f : 0.0f);
        renderedQuality = std::clamp (snapshot.qualityIndex, 0, 2);
        incomingQuality = -1;
        qualityFade.setCurrentAndTargetValue (1.0f);
        for (auto& path : paths)
            path.snap (snapshot);
    }

    void setHostTargets (const ParameterSnapshot& snapshot)
    {
        inputTrim.setTargetValue (snapshot.inputTrimDb);
        mix.setTargetValue (snapshot.mixPercent * 0.01f);
        outputGain.setTargetValue (snapshot.finalOutputDb);
        identity.setTargetValue (isIdentity (snapshot) ? 1.0f : 0.0f);
        globalBypass.setTargetValue (snapshot.globalBypass ? 1.0f : 0.0f);
    }

    void updateQuality (const ParameterSnapshot& snapshot)
    {
        requestedQuality = std::clamp (snapshot.qualityIndex, 0, 2);
        const int heard = incomingQuality >= 0 ? incomingQuality : renderedQuality;
        if (requestedQuality == heard)
            return;

        incomingQuality = requestedQuality;
        qualityFade.setCurrentAndTargetValue (0.0f);
        qualityFade.setTargetValue (1.0f);
        paths[(size_t) incomingQuality].snap (snapshot);
    }

    void copyTrimmed (const juce::AudioBuffer<float>& source, int offset, int numSamples, juce::AudioBuffer<float>& dest)
    {
        auto trim = inputTrim;
        for (int i = 0; i < numSamples; ++i)
        {
            const float gain = dbToGain (trim.getNextValue());
            for (int c = 0; c < channels; ++c)
                dest.setSample (c, i, source.getSample (c, offset + i) * gain);
        }
    }

    void renderPath (int qualityIndex, juce::AudioBuffer<float>& dest, int numSamples, const ParameterSnapshot& snapshot)
    {
        auto& path = paths[(size_t) qualityIndex];
        path.setTargets (snapshot);

        juce::dsp::AudioBlock<float> block (dest);
        auto hostBlock = block.getSubBlock (0, (size_t) numSamples);
        auto oversampled = path.oversampling->processSamplesUp (hostBlock);
        processOversampled (path, oversampled, snapshot);
        path.oversampling->processSamplesDown (hostBlock);

        const int padSamples = std::max (0, latencyForQuality (requestedQuality) - path.osLatency);
        path.pad.setDelay (padSamples);

        for (int i = 0; i < numSamples; ++i)
        {
            float delayed[kMaxChannels];
            for (int c = 0; c < channels; ++c)
                delayed[c] = path.pad.process (c, dest.getSample (c, i));
            path.pad.advance();
            for (int c = 0; c < channels; ++c)
                dest.setSample (c, i, delayed[c]);
        }
    }

    void processOversampled (Path& path, juce::dsp::AudioBlock<float>& block, const ParameterSnapshot& snapshot)
    {
        // Extension: colour rack, then the tube stage, before the VCA.
        const int numSamples = (int) block.getNumSamples();

        for (int i = 0; i < numSamples; ++i)
        {
            float frame[kMaxChannels];
            for (int c = 0; c < channels; ++c)
                frame[c] = block.getSample ((size_t) c, (size_t) i);

            VcaSettings vcaSettings;
            vcaSettings.thresholdDb = path.threshold.getNextValue();
            vcaSettings.kneeDb = path.knee.getNextValue();
            vcaSettings.ratioIndex = snapshot.vcaRatioIndex;
            vcaSettings.timingIndex = snapshot.vcaTimingIndex;
            vcaSettings.detectorIndex = snapshot.vcaDetectorIndex;
            vcaSettings.sidechainHpf = snapshot.vcaScHpf;
            vcaSettings.sidechainHpfHz = path.sidechainHz.getNextValue();
            vcaSettings.bypassAmount = path.vcaBypass.getNextValue();
            path.vca.processSample (frame, channels, vcaSettings);

            FluxSettings fluxSettings;
            fluxSettings.mode = snapshot.fluxMode;
            fluxSettings.intensity = path.intensity.getNextValue();
            fluxSettings.driveDb = path.magneticDrive.getNextValue();
            fluxSettings.attackPreserve = path.attackPreserve.getNextValue();
            fluxSettings.bypassAmount = path.fluxBypass.getNextValue();

            float preMakeup[kMaxChannels];
            for (int c = 0; c < channels; ++c)
                preMakeup[c] = frame[c];
            path.flux.analyse (preMakeup, channels, fluxSettings);

            const float makeupDb = path.makeup.getNextValue();
            const float makeup = dbToGain (makeupDb);
            const float makeupMix = vcaSettings.bypassAmount;
            const float makeupGain = makeupMix >= 1.0f ? 1.0f : (makeupMix <= 0.0f ? makeup : 1.0f + (1.0f - makeupMix) * (makeup - 1.0f));

            for (int c = 0; c < channels; ++c)
                frame[c] *= makeupGain;

            path.flux.shape (frame, channels, fluxSettings);

            TapeSettings tapeSettings;
            tapeSettings.driveDb = path.tapeDrive.getNextValue();
            tapeSettings.bodyDb = path.tapeBody.getNextValue();
            tapeSettings.returnDb = path.tapeReturn.getNextValue();
            tapeSettings.biasIndex = snapshot.tapeBias;
            tapeSettings.bypassAmount = path.tapeBypass.getNextValue();
            path.tape.processSample (frame, channels, tapeSettings);

            // Extension: output iron follows tape return, still inside this oversampled region.

            for (int c = 0; c < channels; ++c)
                block.setSample ((size_t) c, (size_t) i, frame[c]);
        }
    }

    Path paths[3];
    IntegerDelay dryDelay;
    juce::AudioBuffer<float> wetA;
    juce::AudioBuffer<float> wetB;

    juce::SmoothedValue<float> inputTrim;
    juce::SmoothedValue<float> mix;
    juce::SmoothedValue<float> outputGain;
    juce::SmoothedValue<float> identity;
    juce::SmoothedValue<float> globalBypass;
    juce::SmoothedValue<float> qualityFade;

    double hostRate = 48000.0;
    int channels = 2;
    int maxBlock = 512;
    int latency2 = 0;
    int latency4 = 0;
    int latency8 = 0;
    int sharedLatency = 0;
    int reportedLatency = 0;
    int renderedQuality = 1;
    int incomingQuality = -1;
    int requestedQuality = 1;
    bool needsSnap = true;
    bool armReset = false;
    float meterPeak = 0.0f;
};
