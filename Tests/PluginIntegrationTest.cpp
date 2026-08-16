#include "ParameterIDs.h"
#include "PluginProcessor.h"

#include <juce_events/juce_events.h>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace
{
bool check(bool condition, const char* message)
{
    if (!condition)
        std::cerr << "[FAIL] " << message << '\n';
    return condition;
}

bool checkNear(float actual, float expected, float tolerance, const char* message)
{
    return check(std::abs(actual - expected) <= tolerance, message);
}

bool checkFloatParameter(GrainLatchAudioProcessor& processor,
                         const char* id,
                         float start,
                         float end,
                         float interval,
                         float defaultValue)
{
    auto* parameter = processor.parameters.getParameter(id);
    if (!check(parameter != nullptr, (std::string("missing parameter ") + id).c_str()))
        return false;

    auto* floatParameter = dynamic_cast<juce::AudioParameterFloat*>(parameter);
    bool passed = check(floatParameter != nullptr, (std::string("parameter should be float ") + id).c_str());
    if (floatParameter != nullptr)
    {
        passed &= checkNear(floatParameter->range.start, start, 0.0001f, "float range start should match");
        passed &= checkNear(floatParameter->range.end, end, 0.0001f, "float range end should match");
        passed &= checkNear(floatParameter->range.interval, interval, 0.0001f, "float range interval should match");
        passed &= checkNear(processor.parameters.getRawParameterValue(id)->load(), defaultValue, 0.0001f,
                            "float default should match");
    }
    return passed;
}

void setFloatParameter(GrainLatchAudioProcessor& processor, const char* id, float value)
{
    if (auto* parameter = dynamic_cast<juce::AudioParameterFloat*>(processor.parameters.getParameter(id)))
        parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}

std::vector<float> processRetriggerProbe(bool retrigger)
{
    GrainLatchAudioProcessor processor;
    processor.prepareToPlay(48000.0, 512);
    setFloatParameter(processor, grainlatch::parameters::grainMs, 80.0f);
    setFloatParameter(processor, grainlatch::parameters::density, 1.0f);
    setFloatParameter(processor, grainlatch::parameters::jitter, 0.0f);
    setFloatParameter(processor, grainlatch::parameters::reverse, 0.0f);
    setFloatParameter(processor, grainlatch::parameters::stutter, 0.0f);
    setFloatParameter(processor, grainlatch::parameters::damage, 0.0f);
    setFloatParameter(processor, grainlatch::parameters::mix, 1.0f);

    juce::MidiBuffer midi;
    juce::AudioBuffer<float> capture(2, 24000);
    for (int sample = 0; sample < capture.getNumSamples(); ++sample)
    {
        const auto value = static_cast<float>(0.22 * std::sin(2.0 * juce::MathConstants<double>::pi
                                                             * 391.0 * sample / 48000.0));
        capture.setSample(0, sample, value);
        capture.setSample(1, sample, value);
    }
    processor.processBlock(capture, midi);

    if (auto* parameter = processor.parameters.getParameter(grainlatch::parameters::retrigger))
        parameter->setValueNotifyingHost(retrigger ? 1.0f : 0.0f);

    juce::AudioBuffer<float> output(2, 4096);
    output.clear();
    processor.processBlock(output, midi);

    std::vector<float> result(static_cast<std::size_t>(output.getNumSamples()));
    for (int sample = 0; sample < output.getNumSamples(); ++sample)
        result[static_cast<std::size_t>(sample)] = output.getSample(0, sample);
    return result;
}
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI initialiseJuce;
    auto processor = std::make_unique<GrainLatchAudioProcessor>();
    bool passed = true;

    passed &= check(processor->getName() == "GrainLatch", "product name should be GrainLatch");
    passed &= check(!processor->acceptsMidi(), "processor should not accept MIDI");
    passed &= check(!processor->isMidiEffect(), "processor should be an audio effect");
    passed &= check(processor->getLatencySamples() == 0, "processor should not report artificial latency");

    std::unique_ptr<juce::AudioProcessorEditor> editor(processor->createEditor());
    passed &= check(editor != nullptr, "processor should create an editor");
    if (editor != nullptr)
    {
        passed &= check(editor->getWidth() == 512 && editor->getHeight() == 320,
                        "editor should use the compact 512x320 workflow surface");
        passed &= check(editor->findChildWithID("grainlatch-grain-field") != nullptr,
                        "editor should expose the live grain field");
        passed &= check(editor->findChildWithID("grainlatch-control-size") != nullptr,
                        "editor should expose the grain size control");
        passed &= check(editor->findChildWithID("grainlatch-retrigger") != nullptr,
                        "editor should expose the retrigger control");
        passed &= check(editor->findChildWithID("grainlatch-freeze") != nullptr,
                        "editor should expose the freeze control");
        editor->setBounds(0, 0, 512, 320);
        editor->resized();
    }

    juce::AudioProcessor::BusesLayout monoToStereo;
    monoToStereo.inputBuses.add(juce::AudioChannelSet::mono());
    monoToStereo.outputBuses.add(juce::AudioChannelSet::stereo());
    passed &= check(processor->isBusesLayoutSupported(monoToStereo), "mono input/stereo output should be supported");

    passed &= check(processor->getParameters().size() == 10,
                    "processor should expose exactly ten public controls");
    passed &= checkFloatParameter(*processor, grainlatch::parameters::grainMs, 3.0f, 220.0f, 0.1f, 38.0f);
    passed &= checkFloatParameter(*processor, grainlatch::parameters::density, 1.0f, 220.0f, 0.1f, 42.0f);
    passed &= checkFloatParameter(*processor, grainlatch::parameters::jitter, 0.0f, 1.0f, 0.001f, 0.22f);
    passed &= checkFloatParameter(*processor, grainlatch::parameters::reverse, 0.0f, 1.0f, 0.001f, 0.18f);
    passed &= checkFloatParameter(*processor, grainlatch::parameters::stutter, 0.0f, 1.0f, 0.001f, 0.20f);
    passed &= checkFloatParameter(*processor, grainlatch::parameters::damage, 0.0f, 1.0f, 0.001f, 0.34f);
    passed &= checkFloatParameter(*processor, grainlatch::parameters::mix, 0.0f, 1.0f, 0.001f, 0.75f);
    passed &= checkFloatParameter(*processor, grainlatch::parameters::output, -24.0f, 12.0f, 0.1f, 0.0f);

    auto* retrigger = processor->parameters.getParameter(grainlatch::parameters::retrigger);
    auto* freeze = processor->parameters.getParameter(grainlatch::parameters::freeze);
    passed &= check(dynamic_cast<juce::AudioParameterBool*>(retrigger) != nullptr, "Retrigger parameter should be a bool");
    passed &= check(dynamic_cast<juce::AudioParameterBool*>(freeze) != nullptr, "Freeze parameter should be a bool");

    auto* density = processor->parameters.getParameter(grainlatch::parameters::density);
    if (density != nullptr && freeze != nullptr && retrigger != nullptr)
    {
        density->setValueNotifyingHost(density->convertTo0to1(123.0f));
        retrigger->setValueNotifyingHost(1.0f);
        freeze->setValueNotifyingHost(1.0f);
        juce::MemoryBlock state;
        processor->getStateInformation(state);
        density->setValueNotifyingHost(density->convertTo0to1(12.0f));
        retrigger->setValueNotifyingHost(0.0f);
        freeze->setValueNotifyingHost(0.0f);
        processor->setStateInformation(state.getData(), static_cast<int>(state.getSize()));
        passed &= check(std::abs(processor->parameters.getRawParameterValue(grainlatch::parameters::density)->load() - 123.0f) < 0.01f,
                        "APVTS float state should round-trip without transient ring capture");
        passed &= check(processor->parameters.getRawParameterValue(grainlatch::parameters::retrigger)->load() > 0.5f,
                        "APVTS retrigger state should round-trip");
        passed &= check(processor->parameters.getRawParameterValue(grainlatch::parameters::freeze)->load() > 0.5f,
                        "APVTS bool state should round-trip");
    }

    constexpr double sampleRate = 48000.0;
    processor->prepareToPlay(sampleRate, 1024);
    const int blockSizes[] { 32, 64, 127, 256, 511, 1024 };
    int generatedSamples = 0;
    double sumSquares = 0.0;
    for (const auto blockSize : blockSizes)
    {
        juce::AudioBuffer<float> audio(2, blockSize);
        for (int sample = 0; sample < blockSize; ++sample)
        {
            const auto value = static_cast<float>(0.2 * std::sin(2.0 * juce::MathConstants<double>::pi
                                                                 * 330.0 * generatedSamples / sampleRate));
            audio.setSample(0, sample, value);
            audio.setSample(1, sample, value);
            ++generatedSamples;
        }
        juce::MidiBuffer midi;
        processor->processBlock(audio, midi);
        passed &= check(midi.isEmpty(), "processor should clear MIDI");
        for (int channel = 0; channel < audio.getNumChannels(); ++channel)
            for (int sample = 0; sample < audio.getNumSamples(); ++sample)
            {
                const auto value = audio.getSample(channel, sample);
                passed &= check(std::isfinite(value), "processed audio should remain finite");
                sumSquares += static_cast<double>(value) * value;
            }
    }
    passed &= check(std::sqrt(sumSquares / static_cast<double>(generatedSamples * 2)) > 0.01,
                    "processor should produce audible live granular output");

    const auto retriggerOff = processRetriggerProbe(false);
    const auto retriggerOn = processRetriggerProbe(true);
    float retriggerDiff = 0.0f;
    float retriggerPeak = 0.0f;
    for (std::size_t i = 0; i < retriggerOff.size(); ++i)
    {
        retriggerDiff = std::max(retriggerDiff, std::abs(retriggerOn[i] - retriggerOff[i]));
        retriggerPeak = std::max(retriggerPeak, std::abs(retriggerOn[i]));
        passed &= check(std::isfinite(retriggerOn[i]), "APVTS retrigger output should remain finite");
    }
    passed &= check(retriggerDiff > 0.001f, "APVTS retrigger should change active processor output");
    passed &= check(retriggerPeak <= 0.981f, "APVTS retrigger output should remain bounded");

    processor->reset();
    juce::AudioBuffer<float> silence(2, 4096);
    silence.clear();
    juce::MidiBuffer midi;
    processor->processBlock(silence, midi);
    float silencePeak = 0.0f;
    for (int channel = 0; channel < silence.getNumChannels(); ++channel)
        silencePeak = std::max(silencePeak, silence.getMagnitude(channel, 0, silence.getNumSamples()));
    passed &= check(silencePeak == 0.0f, "silence in should remain silence out without held capture");

    if (passed)
        std::cout << "GrainLatch plug-in integration checks passed\n";
    return passed ? 0 : 1;
}
