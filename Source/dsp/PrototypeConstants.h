#pragma once

#include <algorithm>
#include <cmath>

// Provisional calibration and detector seeds. Hardware captures replace these
// numbers without changing the signal order.

inline constexpr int kMaxChannels = 2;
inline constexpr int kLatencyBudgetSamples = 128;
inline constexpr float kSmoothSeconds = 0.02f;
inline constexpr float kCrossfadeSeconds = 0.01f;
inline constexpr float kSilenceDb = -60.0f;
inline constexpr float kTapeBodyHz = 80.0f;
inline constexpr float kTransformerHz = 30.0f;
inline constexpr float kTapeShelfSlope = 1.0f;

// A -18 dBFS peak at 0 dB Tape Drive maps to |h| = kField * 10^(-18/20) ≈ 0.25.
inline constexpr float kField = 2.0f;

struct VcaTiming
{
    float attackMs;
    float releaseMs;
};

// Authored drum timings from the development plan, positions 1..10.
inline constexpr VcaTiming kVcaTiming[10] = {
    { 0.1f, 20.0f },
    { 0.3f, 40.0f },
    { 1.0f, 60.0f },
    { 3.0f, 100.0f },
    { 7.0f, 140.0f },
    { 10.0f, 220.0f },
    { 20.0f, 350.0f },
    { 30.0f, 600.0f },
    { 60.0f, 1000.0f },
    { 100.0f, 2000.0f },
};

// Midpoints of the Nickel / Cobalt / Iron search ranges.
inline constexpr float kFluxWindowMs[3] = { 3.5f, 10.0f, 22.0f };
inline constexpr float kFluxFastAttackSeconds = 0.00005f;
inline constexpr float kFluxFastReleaseSeconds = 0.002f;
inline constexpr float kFluxSlowReleaseScale = 4.0f;

struct TapeProfile
{
    float r[3];
    float tauMs[3];
    float a0;
    float a[3];
};

// Nominal matches the published stateful experiment. Low and high are authored
// stand-ins until a bias capture exists. Weights in each profile sum to 1.
inline constexpr TapeProfile kBiasProfiles[3] = {
    { { 0.02f, 0.06f, 0.12f }, { 4.0f, 16.0f, 45.0f }, 0.45f, { 0.20f, 0.20f, 0.15f } },
    { { 0.04f, 0.12f, 0.25f }, { 3.0f, 12.0f, 35.0f }, 0.65f, { 0.10f, 0.12f, 0.13f } },
    { { 0.08f, 0.20f, 0.45f }, { 2.0f, 8.0f, 24.0f }, 0.82f, { 0.06f, 0.06f, 0.06f } },
};

inline float dbToGain (float db) noexcept
{
    return std::pow (10.0f, db * 0.05f);
}

// Float parameter steps are 0.01 dB. A stored 0 dB default round-trips a fraction
// of a micro-decibel away from zero, which would scale an otherwise unity trim.
inline float trimGain (float db) noexcept
{
    return std::abs (db) < 0.005f ? 1.0f : dbToGain (db);
}

inline double dbToGain (double db) noexcept
{
    return std::pow (10.0, db * 0.05);
}

inline float gainToDb (float gain) noexcept
{
    return 20.0f * std::log10 (std::max (gain, 1.0e-8f));
}

inline float vcaRatioFromIndex (int index) noexcept
{
    switch (std::clamp (index, 0, 4))
    {
        case 0: return 1.0f;
        case 1: return 2.0f;
        case 2: return 4.0f;
        case 3: return 8.0f;
        default: return 20.0f;
    }
}

inline float fluxWindowSeconds (int mode) noexcept
{
    return kFluxWindowMs[std::clamp (mode, 0, 2)] * 0.001f;
}

inline float silenceLinear() noexcept
{
    return std::pow (10.0f, kSilenceDb / 20.0f);
}

inline float sanitize (float x) noexcept
{
    return std::isfinite (x) ? x : 0.0f;
}
