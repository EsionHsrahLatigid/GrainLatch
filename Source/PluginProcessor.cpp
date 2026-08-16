#include "PluginProcessor.h"
#include "ParameterIDs.h"
#include "PluginEditor.h"

#include <algorithm>

namespace
{
using APVTS = juce::AudioProcessorValueTreeState;
using Layout = APVTS::ParameterLayout;

std::unique_ptr<juce::AudioParameterFloat> makeFloat(const char* id,
                                                      const char* name,
                                                      juce::NormalisableRange<float> range,
                                                      float defaultValue)
{
    return std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { id, 1 }, name, range, defaultValue);
}
} // namespace

GrainLatchAudioProcessor::GrainLatchAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, juce::Identifier("GrainLatchState"), createParameterLayout())
{
    cacheParameterPointers();
    setLatencySamples(0);
}

Layout GrainLatchAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> values;
    values.reserve(10);
    values.push_back(makeFloat(grainlatch::parameters::grainMs, "Grain ms", { 3.0f, 220.0f, 0.1f }, 38.0f));
    values.push_back(makeFloat(grainlatch::parameters::density, "Density", { 1.0f, 220.0f, 0.1f }, 42.0f));
    values.push_back(makeFloat(grainlatch::parameters::jitter, "Jitter", { 0.0f, 1.0f, 0.001f }, 0.22f));
    values.push_back(makeFloat(grainlatch::parameters::reverse, "Reverse", { 0.0f, 1.0f, 0.001f }, 0.18f));
    values.push_back(makeFloat(grainlatch::parameters::stutter, "Stutter", { 0.0f, 1.0f, 0.001f }, 0.20f));
    values.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { grainlatch::parameters::freeze, 1 }, "Freeze", false));
    values.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { grainlatch::parameters::retrigger, 1 }, "Retrigger", false));
    values.push_back(makeFloat(grainlatch::parameters::damage, "Damage", { 0.0f, 1.0f, 0.001f }, 0.34f));
    values.push_back(makeFloat(grainlatch::parameters::mix, "Mix", { 0.0f, 1.0f, 0.001f }, 0.75f));
    values.push_back(makeFloat(grainlatch::parameters::output, "Output", { -24.0f, 12.0f, 0.1f }, 0.0f));
    return { values.begin(), values.end() };
}

void GrainLatchAudioProcessor::cacheParameterPointers()
{
    parameter.grainMs = parameters.getRawParameterValue(grainlatch::parameters::grainMs);
    parameter.density = parameters.getRawParameterValue(grainlatch::parameters::density);
    parameter.jitter = parameters.getRawParameterValue(grainlatch::parameters::jitter);
    parameter.reverse = parameters.getRawParameterValue(grainlatch::parameters::reverse);
    parameter.stutter = parameters.getRawParameterValue(grainlatch::parameters::stutter);
    parameter.freeze = parameters.getRawParameterValue(grainlatch::parameters::freeze);
    parameter.retrigger = parameters.getRawParameterValue(grainlatch::parameters::retrigger);
    parameter.damage = parameters.getRawParameterValue(grainlatch::parameters::damage);
    parameter.mix = parameters.getRawParameterValue(grainlatch::parameters::mix);
    parameter.output = parameters.getRawParameterValue(grainlatch::parameters::output);
}

void GrainLatchAudioProcessor::prepareToPlay(double sampleRate, int)
{
    for (auto& core : cores)
        core.prepare(sampleRate, 1);
}

void GrainLatchAudioProcessor::releaseResources() {}

void GrainLatchAudioProcessor::reset()
{
    for (auto& core : cores)
        core.reset();
}

bool GrainLatchAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto input = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();
    return output == juce::AudioChannelSet::stereo()
        && (input == juce::AudioChannelSet::mono() || input == juce::AudioChannelSet::stereo());
}

grainlatch::dsp::GranularParameters GrainLatchAudioProcessor::readParameters() const noexcept
{
    grainlatch::dsp::GranularParameters result;
    result.grainMs = parameter.grainMs->load();
    result.density = parameter.density->load();
    result.jitter = parameter.jitter->load();
    result.reverse = parameter.reverse->load();
    result.stutter = parameter.stutter->load();
    result.freeze = parameter.freeze->load() >= 0.5f;
    result.retrigger = parameter.retrigger->load();
    result.damage = parameter.damage->load();
    result.mix = parameter.mix->load();
    result.outputDb = parameter.output->load();
    return result;
}

void GrainLatchAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    midiMessages.clear();

    const auto numSamples = buffer.getNumSamples();
    const auto inputChannels = std::clamp(getTotalNumInputChannels(), 1, 2);
    const auto outputChannels = std::min(buffer.getNumChannels(), 2);
    if (numSamples <= 0 || outputChannels <= 0)
        return;

    for (int channel = outputChannels; channel < buffer.getNumChannels(); ++channel)
        buffer.clear(channel, 0, numSamples);

    const auto params = readParameters();
    auto* left = buffer.getWritePointer(0);
    auto* right = outputChannels > 1 ? buffer.getWritePointer(1) : nullptr;
    for (int sample = 0; sample < numSamples; ++sample)
    {
        const auto leftIn = left[sample];
        const auto rightIn = inputChannels > 1 && right != nullptr ? right[sample] : leftIn;
        left[sample] = cores[0].processSample(leftIn, params);
        if (right != nullptr)
            right[sample] = cores[1].processSample(rightIn, params);
    }
}

void GrainLatchAudioProcessor::getStateInformation(juce::MemoryBlock& destinationData)
{
    if (const auto xml = parameters.copyState().createXml())
        copyXmlToBinary(*xml, destinationData);
}

void GrainLatchAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (const auto xml = getXmlFromBinary(data, sizeInBytes))
    {
        const auto state = juce::ValueTree::fromXml(*xml);
        if (state.isValid() && state.hasType(parameters.state.getType()))
            parameters.replaceState(state);
    }
}

void GrainLatchAudioProcessor::copyGrainSnapshot(grainlatch::dsp::GrainSnapshot& destination) const noexcept
{
    grainlatch::dsp::GrainSnapshot left;
    grainlatch::dsp::GrainSnapshot right;
    cores[0].copySnapshot(left);
    cores[1].copySnapshot(right);

    destination = left;
    for (std::size_t i = 0; i < destination.cells.size(); ++i)
        destination.cells[i] = std::max(left.cells[i], right.cells[i]);
    destination.inputRms = 0.5f * (left.inputRms + right.inputRms);
    destination.wetRms = 0.5f * (left.wetRms + right.wetRms);
    destination.heldRms = 0.5f * (left.heldRms + right.heldRms);
    destination.activeGrains = std::max(left.activeGrains, right.activeGrains);
    destination.capturing = left.capturing || right.capturing;
    destination.frozen = left.frozen || right.frozen;
    destination.recovery = left.recovery || right.recovery;
}

juce::AudioProcessorEditor* GrainLatchAudioProcessor::createEditor()
{
    return new GrainLatchAudioProcessorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GrainLatchAudioProcessor();
}
