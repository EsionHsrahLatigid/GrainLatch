#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace grainlatch::dsp
{

struct GranularParameters
{
    float grainMs = 38.0f;
    float density = 42.0f;
    float jitter = 0.22f;
    float reverse = 0.18f;
    float stutter = 0.20f;
    bool freeze = false;
    float retrigger = 0.0f;
    float damage = 0.34f;
    float mix = 0.75f;
    float outputDb = 0.0f;
};

struct GrainSnapshot
{
    static constexpr int columns = 40;
    static constexpr int rows = 12;
    std::array<float, columns * rows> cells {};
    float inputRms = 0.0f;
    float wetRms = 0.0f;
    float heldRms = 0.0f;
    int activeGrains = 0;
    bool capturing = false;
    bool frozen = false;
    bool recovery = false;
};

class GranularCore
{
public:
    static constexpr int maxSampleRate = 192000;
    static constexpr int ringSeconds = 4;
    static constexpr int ringSize = maxSampleRate * ringSeconds;
    static constexpr int maxVoices = 64;
    static constexpr int maxGrainSamples = maxSampleRate / 2;

    GranularCore();

    void prepare(double sampleRate, int channels) noexcept;
    void reset() noexcept;
    float processSample(float input, const GranularParameters& parameters) noexcept;
    void copySnapshot(GrainSnapshot& destination) const noexcept;

private:
    struct Grain
    {
        float read = 0.0f;
        float step = 1.0f;
        float amp = 0.0f;
        int age = 0;
        int length = 0;
        int strideGate = 0;
        int stridePhase = 0;
        bool active = false;
    };

    struct AtomicSnapshot
    {
        std::atomic<std::uint64_t> sequence { 0 };
        std::array<std::atomic<std::uint32_t>, GrainSnapshot::columns * GrainSnapshot::rows> cells {};
        std::atomic<std::uint32_t> inputRms { 0 };
        std::atomic<std::uint32_t> wetRms { 0 };
        std::atomic<std::uint32_t> heldRms { 0 };
        std::atomic<std::uint32_t> activeGrains { 0 };
        std::atomic<std::uint8_t> capturing { 0 };
        std::atomic<std::uint8_t> frozen { 0 };
        std::atomic<std::uint8_t> recovery { 0 };
    };

    std::array<float, ringSize> ring {};
    std::array<Grain, maxVoices> voices {};
    std::array<float, GrainSnapshot::columns * GrainSnapshot::rows> snapshotWrite {};
    mutable AtomicSnapshot snapshot {};

    double currentSampleRate = 48000.0;
    int usableRing = 48000 * ringSeconds;
    int writeIndex = 0;
    int freezeAnchor = 0;
    int nextGrainIn = 64;
    int captureSamples = 0;
    int heldSamples = 0;
    unsigned rng = 0x474c6174u;
    float dcX1 = 0.0f;
    float dcY1 = 0.0f;
    float limiter = 1.0f;
    float inputRmsAcc = 0.0f;
    float wetRmsAcc = 0.0f;
    float heldRmsAcc = 0.0f;
    int rmsCount = 0;
    bool wasFrozen = false;

    unsigned randomU32() noexcept;
    float random01() noexcept;
    void maybeLaunchGrain(const GranularParameters& parameters) noexcept;
    void launchGrain(const GranularParameters& parameters) noexcept;
    float readRingLinear(float position) const noexcept;
    void publishSnapshot(float inputRms, float wetRms, float heldRms, int active, bool frozen, bool recovery) noexcept;
};

} // namespace grainlatch::dsp
