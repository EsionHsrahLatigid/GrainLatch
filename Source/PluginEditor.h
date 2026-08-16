#pragma once

#include "PluginProcessor.h"
#include <ehl/juce_design/EhlDesign.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <array>
#include <memory>

class GrainLatchAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                             private juce::Timer
{
public:
    explicit GrainLatchAudioProcessorEditor(GrainLatchAudioProcessor&);
    ~GrainLatchAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    static constexpr int defaultWidth = 512;
    static constexpr int defaultHeight = 320;

private:
    class GrainField final : public juce::Component
    {
    public:
        void setSnapshot(const grainlatch::dsp::GrainSnapshot& next);
        void paint(juce::Graphics&) override;

    private:
        grainlatch::dsp::GrainSnapshot snapshot;
    };

    void timerCallback() override;
    void updateReadout();
    void configureControl(juce::Slider& slider, juce::Label& label, const juce::String& text);

    GrainLatchAudioProcessor& ownerProcessor;
    ehl::juce_design::LookAndFeel lookAndFeel;
    GrainField matrix;
    juce::Label status;
    std::array<juce::Slider, 8> sliders;
    std::array<juce::Label, 10> labels;
    juce::ToggleButton freezeButton;
    juce::ToggleButton retriggerButton;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 8> sliderAttachments;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> freezeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> retriggerAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GrainLatchAudioProcessorEditor)
};
