#include "../Source/dsp/GranularCore.h"

#include <algorithm>
#include <cmath>
#include <exception>
#include <iostream>
#include <set>
#include <string>
#include <vector>

namespace
{
using namespace grainlatch::dsp;

static_assert(sizeof(GranularCore) < 64 * 1024, "GranularCore must fit on Windows default test stacks");

struct Failure final : std::exception
{
    explicit Failure(std::string messageIn) : message(std::move(messageIn)) {}
    const char* what() const noexcept override { return message.c_str(); }
    std::string message;
};

struct Metrics
{
    float rms = 0.0f;
    float peak = 0.0f;
    float dc = 0.0f;
    int clipped = 0;
    int zeroCrossings = 0;
    int uniqueBuckets = 0;
};

[[noreturn]] void fail(const std::string& message)
{
    throw Failure(message);
}

void expect(bool condition, const std::string& message)
{
    if (!condition)
        fail(message);
}

Metrics measure(const std::vector<float>& signal)
{
    Metrics result;
    double sum = 0.0;
    double sumSquares = 0.0;
    bool hadPrevious = false;
    float previous = 0.0f;
    std::set<int> buckets;

    for (const auto sample : signal)
    {
        expect(std::isfinite(sample), "output must be finite");
        sum += sample;
        sumSquares += static_cast<double>(sample) * sample;
        result.peak = std::max(result.peak, std::abs(sample));
        result.clipped += std::abs(sample) >= 0.979f ? 1 : 0;
        if (hadPrevious && ((previous < 0.0f && sample >= 0.0f) || (previous >= 0.0f && sample < 0.0f)))
            ++result.zeroCrossings;
        hadPrevious = true;
        previous = sample;
        buckets.insert(static_cast<int>(std::round(sample * 4096.0f)));
    }

    result.uniqueBuckets = static_cast<int>(buckets.size());
    result.rms = signal.empty() ? 0.0f : static_cast<float>(std::sqrt(sumSquares / static_cast<double>(signal.size())));
    result.dc = signal.empty() ? 0.0f : static_cast<float>(sum / static_cast<double>(signal.size()));
    return result;
}

std::vector<float> sine(float hz, std::size_t samples)
{
    std::vector<float> result(samples);
    for (std::size_t i = 0; i < samples; ++i)
        result[i] = 0.25f * std::sin(2.0f * 3.14159265358979323846f * hz * static_cast<float>(i) / 48000.0f);
    return result;
}

std::vector<float> seededNoise(std::size_t samples)
{
    std::vector<float> result(samples);
    unsigned state = 0x12345678u;
    for (auto& sample : result)
    {
        state = state * 1664525u + 1013904223u;
        sample = (static_cast<float>((state >> 8u) & 0xffffu) / 32768.0f - 1.0f) * 0.2f;
    }
    return result;
}

std::vector<float> render(GranularParameters params, const std::vector<float>& input)
{
    GranularCore core;
    core.prepare(48000.0, 1);
    std::vector<float> output;
    output.reserve(input.size());
    for (const auto sample : input)
        output.push_back(core.processSample(sample, params));
    return output;
}

void silence_stays_silent_without_held_grain()
{
    GranularParameters params;
    params.mix = 1.0f;
    params.damage = 1.0f;
    params.density = 220.0f;
    const std::vector<float> silence(48000, 0.0f);
    const auto metrics = measure(render(params, silence));
    expect(metrics.rms == 0.0f && metrics.peak == 0.0f, "silent input must not self-oscillate without captured grains");
}

void default_live_capture_is_audible_and_deterministic()
{
    GranularParameters params;
    const auto input = sine(330.0f, 48000 * 2);
    const auto first = render(params, input);
    const auto second = render(params, input);
    expect(first == second, "fixed launch RNG should make repeated renders identical");

    const auto metrics = measure(first);
    expect(metrics.rms > 0.025f, "default sine render should be audibly non-silent");
    expect(metrics.peak <= 0.981f, "default render should stay below the hard ceiling");
    expect(std::abs(metrics.dc) < 0.02f, "default render should not build DC");
    expect(metrics.zeroCrossings > 256, "default render should remain in the audible band");
    expect(metrics.uniqueBuckets > 96, "default render should not collapse into a constant");
}

void extremes_are_harsh_but_bounded()
{
    GranularParameters params;
    params.grainMs = 3.0f;
    params.density = 220.0f;
    params.jitter = 1.0f;
    params.reverse = 1.0f;
    params.stutter = 1.0f;
    params.damage = 1.0f;
    params.mix = 1.0f;
    params.outputDb = 12.0f;
    const auto metrics = measure(render(params, seededNoise(48000 * 2)));
    expect(metrics.rms > 0.015f, "extreme granular settings should remain audibly non-silent");
    expect(metrics.peak <= 0.981f, "extreme granular settings should remain bounded");
    expect(metrics.clipped < 500, "extreme granular settings should not become sustained rails");
    expect(std::abs(metrics.dc) < 0.05f, "extreme granular settings should not become DC");
    expect(metrics.zeroCrossings > 1024, "extreme granular settings should not collapse into static output");
    expect(metrics.uniqueBuckets > 128, "extreme granular settings should have varied sample values");
}

void freeze_holds_capture_and_empty_freeze_is_silent()
{
    GranularCore empty;
    empty.prepare(48000.0, 1);
    GranularParameters hold;
    hold.freeze = true;
    hold.mix = 1.0f;
    std::vector<float> emptyOutput;
    for (int i = 0; i < 24000; ++i)
        emptyOutput.push_back(empty.processSample(0.0f, hold));
    expect(measure(emptyOutput).rms == 0.0f, "freeze before capture must remain silent");

    GranularCore core;
    core.prepare(48000.0, 1);
    GranularParameters live;
    live.mix = 1.0f;
    for (const auto sample : sine(220.0f, 48000))
        core.processSample(sample, live);

    std::vector<float> frozen;
    hold.density = 120.0f;
    hold.damage = 0.55f;
    for (int i = 0; i < 48000; ++i)
        frozen.push_back(core.processSample(0.0f, hold));

    const auto metrics = measure(frozen);
    expect(metrics.rms > 0.01f, "freeze should emit held captured grains while input is silent");
    expect(metrics.peak <= 0.981f, "freeze output should remain bounded");
    expect(metrics.uniqueBuckets > 64, "freeze output should not become a constant");

    GrainSnapshot snapshot;
    core.copySnapshot(snapshot);
    expect(snapshot.frozen, "snapshot should report frozen state");
    expect(snapshot.heldRms > 0.001f, "snapshot should report held energy");
}

void reset_and_sample_rate_change_are_defined()
{
    GranularCore core;
    core.prepare(96000.0, 1);
    GranularParameters params;
    for (const auto sample : seededNoise(32000))
        core.processSample(sample, params);
    core.reset();
    std::vector<float> silence;
    for (int i = 0; i < 4096; ++i)
        silence.push_back(core.processSample(0.0f, params));
    expect(measure(silence).rms == 0.0f, "reset should clear captured history and voices");
}
} // namespace

int main()
{
    try
    {
        silence_stays_silent_without_held_grain();
        default_live_capture_is_audible_and_deterministic();
        extremes_are_harsh_but_bounded();
        freeze_holds_capture_and_empty_freeze_is_silent();
        reset_and_sample_rate_change_are_defined();
    }
    catch (const Failure& failure)
    {
        std::cerr << "[FAIL] " << failure.what() << '\n';
        return 1;
    }

    std::cout << "GrainLatch DSP checks passed\n";
    return 0;
}
