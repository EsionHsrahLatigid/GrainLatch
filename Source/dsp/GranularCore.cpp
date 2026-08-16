#include "GranularCore.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <thread>

namespace grainlatch::dsp
{
namespace
{
constexpr float pi = 3.14159265358979323846f;
constexpr float silenceThreshold = 0.00002f;

float sanitize(float value) noexcept
{
    return std::isfinite(value) && std::abs(value) > 1.0e-20f ? value : 0.0f;
}

float clamp01(float value) noexcept
{
    return std::clamp(sanitize(value), 0.0f, 1.0f);
}

float dbToGain(float db) noexcept
{
    return std::pow(10.0f, std::clamp(sanitize(db), -48.0f, 18.0f) / 20.0f);
}

std::uint32_t packFloat(float value) noexcept
{
    static_assert(std::atomic<std::uint32_t>::is_always_lock_free,
                  "snapshot float payload must be lock-free on the audio thread");
    static_assert(std::atomic<std::uint8_t>::is_always_lock_free,
                  "snapshot flag payload must be lock-free on the audio thread");
    static_assert(std::atomic<std::uint64_t>::is_always_lock_free,
                  "snapshot sequence must be lock-free on the audio thread");
    std::uint32_t bits = 0;
    const auto clean = sanitize(value);
    std::memcpy(&bits, &clean, sizeof(bits));
    return bits;
}

float unpackFloat(std::uint32_t bits) noexcept
{
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return sanitize(value);
}
} // namespace

GranularCore::GranularCore()
    : ring(std::make_unique<RingBuffer>())
{
    reset();
}

void GranularCore::prepare(double sampleRate, int) noexcept
{
    currentSampleRate = sampleRate > 1000.0 ? std::min(sampleRate, static_cast<double>(maxSampleRate)) : 48000.0;
    usableRing = std::clamp(static_cast<int>(currentSampleRate * ringSeconds), 1024, ringSize);
    reset();
}

void GranularCore::reset() noexcept
{
    ring->fill(0.0f);
    for (auto& voice : voices)
        voice = {};
    snapshotWrite.fill(0.0f);
    snapshot.sequence.store(1, std::memory_order_release);
    for (auto& cell : snapshot.cells)
        cell.store(packFloat(0.0f), std::memory_order_relaxed);
    snapshot.inputRms.store(packFloat(0.0f), std::memory_order_relaxed);
    snapshot.wetRms.store(packFloat(0.0f), std::memory_order_relaxed);
    snapshot.heldRms.store(packFloat(0.0f), std::memory_order_relaxed);
    snapshot.activeGrains.store(packFloat(0.0f), std::memory_order_relaxed);
    snapshot.capturing.store(0, std::memory_order_relaxed);
    snapshot.frozen.store(0, std::memory_order_relaxed);
    snapshot.recovery.store(0, std::memory_order_relaxed);
    snapshot.sequence.store(2, std::memory_order_release);
    writeIndex = 0;
    freezeAnchor = 0;
    nextGrainIn = 64;
    captureSamples = 0;
    heldSamples = 0;
    rng = 0x474c6174u;
    dcX1 = 0.0f;
    dcY1 = 0.0f;
    limiter = 1.0f;
    inputRmsAcc = 0.0f;
    wetRmsAcc = 0.0f;
    heldRmsAcc = 0.0f;
    rmsCount = 0;
    wasFrozen = false;
}

float GranularCore::processSample(float input, const GranularParameters& parameters) noexcept
{
    input = sanitize(input);
    const auto frozen = parameters.freeze;
    if (!frozen)
    {
        (*ring)[static_cast<std::size_t>(writeIndex)] = input;
        writeIndex = (writeIndex + 1) % usableRing;
        if (std::abs(input) > silenceThreshold)
            captureSamples = std::min(captureSamples + 1, usableRing);
    }
    else if (!wasFrozen)
    {
        freezeAnchor = writeIndex;
        heldSamples = captureSamples;
    }
    wasFrozen = frozen;

    maybeLaunchGrain(parameters);

    float wet = 0.0f;
    int active = 0;
    const auto damage = clamp01(parameters.damage);
    for (auto& voice : voices)
    {
        if (!voice.active)
            continue;
        if (voice.age >= voice.length)
        {
            voice.active = false;
            continue;
        }

        const auto phase = static_cast<float>(voice.age) / static_cast<float>(std::max(1, voice.length));
        const auto envelope = std::sin(pi * phase);
        auto grain = readRingLinear(voice.read) * envelope * voice.amp;
        if (voice.strideGate > 1 && (voice.stridePhase++ % voice.strideGate) == 0)
            grain = -grain * (0.35f + 0.65f * damage);
        wet += grain;

        voice.read += voice.step;
        while (voice.read < 0.0f)
            voice.read += static_cast<float>(usableRing);
        while (voice.read >= static_cast<float>(usableRing))
            voice.read -= static_cast<float>(usableRing);
        ++voice.age;
        ++active;
    }

    const auto normaliser = 1.0f / std::sqrt(static_cast<float>(std::max(1, active)));
    wet *= normaliser * (1.0f + damage * 0.85f);
    wet = std::tanh(wet * (1.0f + damage * 2.75f)) * (0.82f + damage * 0.14f);

    if (std::abs(input) <= silenceThreshold && !frozen && active == 0)
        wet = 0.0f;

    const auto mix = clamp01(parameters.mix);
    auto output = input + (wet - input) * mix;
    const auto dc = output - dcX1 + 0.995f * dcY1;
    dcX1 = output;
    dcY1 = sanitize(dc);

    limiter = std::max(0.25f, limiter * 0.9996f);
    const auto ceiling = 0.96f / limiter;
    auto guarded = std::clamp(dc * dbToGain(parameters.outputDb), -ceiling, ceiling);
    limiter = std::max(limiter, std::abs(guarded));
    guarded = sanitize(std::clamp(guarded, -0.96f, 0.96f));

    inputRmsAcc += input * input;
    wetRmsAcc += wet * wet;
    heldRmsAcc += frozen ? wet * wet : input * input;
    ++rmsCount;
    if (rmsCount >= 256)
    {
        const auto inputRms = std::sqrt(inputRmsAcc / static_cast<float>(rmsCount));
        const auto wetRms = std::sqrt(wetRmsAcc / static_cast<float>(rmsCount));
        const auto heldRms = std::sqrt(heldRmsAcc / static_cast<float>(rmsCount));
        const auto recovery = inputRms > 0.0005f && wetRms < inputRms * 0.05f && !frozen;
        publishSnapshot(inputRms, wetRms, heldRms, active, frozen, recovery);
        inputRmsAcc = 0.0f;
        wetRmsAcc = 0.0f;
        heldRmsAcc = 0.0f;
        rmsCount = 0;
    }

    return guarded;
}

unsigned GranularCore::randomU32() noexcept
{
    rng ^= rng << 13u;
    rng ^= rng >> 17u;
    rng ^= rng << 5u;
    return rng;
}

float GranularCore::random01() noexcept
{
    return static_cast<float>(randomU32() & 0x00ffffffu) / static_cast<float>(0x01000000u);
}

void GranularCore::maybeLaunchGrain(const GranularParameters& parameters) noexcept
{
    const auto density = std::clamp(sanitize(parameters.density), 1.0f, 220.0f);
    --nextGrainIn;
    if (nextGrainIn > 0)
        return;

    launchGrain(parameters);
    const auto base = static_cast<int>(currentSampleRate / density);
    const auto jitter = (random01() * 2.0f - 1.0f) * clamp01(parameters.jitter);
    nextGrainIn = std::max(1, static_cast<int>(static_cast<float>(base) * (1.0f + jitter * 0.92f)));
}

void GranularCore::launchGrain(const GranularParameters& parameters) noexcept
{
    const auto available = parameters.freeze ? heldSamples : captureSamples;
    if (available < 16)
        return;

    Grain* target = nullptr;
    for (auto& voice : voices)
    {
        if (!voice.active)
        {
            target = &voice;
            break;
        }
    }
    if (target == nullptr)
    {
        target = &voices[0];
        for (auto& voice : voices)
            if (voice.age > target->age)
                target = &voice;
    }

    const auto grainSamples = std::clamp(static_cast<int>(parameters.grainMs * 0.001f * static_cast<float>(currentSampleRate)),
                                         8,
                                         std::min(maxGrainSamples, std::max(16, available)));
    const auto window = std::min(available, usableRing - 1);
    const auto stutter = clamp01(parameters.stutter);
    const auto damage = clamp01(parameters.damage);
    const auto localSpan = std::max(grainSamples, static_cast<int>(static_cast<float>(window) * (1.0f - stutter)));
    const auto maxOffset = std::max(grainSamples, localSpan);
    const auto randomOffset = static_cast<int>(random01() * static_cast<float>(maxOffset - grainSamples + 1));
    const auto anchor = parameters.freeze ? freezeAnchor : writeIndex;
    auto start = anchor - grainSamples - randomOffset;
    while (start < 0)
        start += usableRing;

    target->read = static_cast<float>(start % usableRing);
    target->step = 1.0f;
    if (random01() < clamp01(parameters.reverse))
        target->step = -target->step;
    target->amp = 0.55f + 0.45f * random01();
    target->age = 0;
    target->length = grainSamples;
    target->strideGate = damage > 0.02f ? 2 + static_cast<int>((1.0f - damage) * 10.0f) : 0;
    target->stridePhase = static_cast<int>(randomU32() & 31u);
    target->active = true;
}

float GranularCore::readRingLinear(float position) const noexcept
{
    auto index0 = static_cast<int>(position);
    const auto frac = position - static_cast<float>(index0);
    while (index0 < 0)
        index0 += usableRing;
    index0 %= usableRing;
    const auto index1 = (index0 + 1) % usableRing;
    return (*ring)[static_cast<std::size_t>(index0)] * (1.0f - frac)
         + (*ring)[static_cast<std::size_t>(index1)] * frac;
}

void GranularCore::publishSnapshot(float inputRms, float wetRms, float heldRms, int active, bool frozen, bool recovery) noexcept
{
    snapshotWrite.fill(0.0f);
    for (int column = 0; column < GrainSnapshot::columns; ++column)
    {
        const auto lookback = 1 + (column * std::max(1, usableRing - 1)) / GrainSnapshot::columns;
        auto index = writeIndex - lookback;
        while (index < 0)
            index += usableRing;
        const auto value = std::clamp(std::abs((*ring)[static_cast<std::size_t>(index)]) * 8.0f, 0.0f, 1.0f);
        const auto litRows = static_cast<int>(std::round(value * static_cast<float>(GrainSnapshot::rows)));
        for (int row = 0; row < GrainSnapshot::rows; ++row)
            snapshotWrite[static_cast<std::size_t>((GrainSnapshot::rows - 1 - row) * GrainSnapshot::columns + column)] =
                row < litRows ? value : 0.0f;
    }

    auto sequence = snapshot.sequence.load(std::memory_order_relaxed);
    if ((sequence & 1u) != 0u)
        ++sequence;
    snapshot.sequence.store(sequence + 1u, std::memory_order_release);
    for (std::size_t i = 0; i < snapshotWrite.size(); ++i)
        snapshot.cells[i].store(packFloat(snapshotWrite[i]), std::memory_order_relaxed);
    snapshot.inputRms.store(packFloat(inputRms), std::memory_order_relaxed);
    snapshot.wetRms.store(packFloat(wetRms), std::memory_order_relaxed);
    snapshot.heldRms.store(packFloat(heldRms), std::memory_order_relaxed);
    snapshot.activeGrains.store(packFloat(static_cast<float>(active)), std::memory_order_relaxed);
    snapshot.capturing.store(captureSamples > 16 ? 1u : 0u, std::memory_order_relaxed);
    snapshot.frozen.store(frozen ? 1u : 0u, std::memory_order_relaxed);
    snapshot.recovery.store(recovery ? 1u : 0u, std::memory_order_relaxed);
    snapshot.sequence.store(sequence + 2u, std::memory_order_release);
}

void GranularCore::copySnapshot(GrainSnapshot& destination) const noexcept
{
    GrainSnapshot candidate;
    for (int attempt = 0; attempt < 8; ++attempt)
    {
        const auto begin = snapshot.sequence.load(std::memory_order_acquire);
        if ((begin & 1u) != 0u)
        {
            std::this_thread::yield();
            continue;
        }

        for (std::size_t i = 0; i < candidate.cells.size(); ++i)
            candidate.cells[i] = unpackFloat(snapshot.cells[i].load(std::memory_order_relaxed));
        candidate.inputRms = unpackFloat(snapshot.inputRms.load(std::memory_order_relaxed));
        candidate.wetRms = unpackFloat(snapshot.wetRms.load(std::memory_order_relaxed));
        candidate.heldRms = unpackFloat(snapshot.heldRms.load(std::memory_order_relaxed));
        candidate.activeGrains = static_cast<int>(unpackFloat(snapshot.activeGrains.load(std::memory_order_relaxed)) + 0.5f);
        candidate.capturing = snapshot.capturing.load(std::memory_order_relaxed) != 0u;
        candidate.frozen = snapshot.frozen.load(std::memory_order_relaxed) != 0u;
        candidate.recovery = snapshot.recovery.load(std::memory_order_relaxed) != 0u;

        const auto end = snapshot.sequence.load(std::memory_order_acquire);
        if (begin == end && (end & 1u) == 0u)
        {
            destination = candidate;
            return;
        }
    }

    destination = candidate;
}

} // namespace grainlatch::dsp
