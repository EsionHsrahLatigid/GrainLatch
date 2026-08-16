#include "PluginEditor.h"
#include "ParameterIDs.h"

namespace
{
namespace design = ehl::juce_design;
}

GrainLatchAudioProcessorEditor::GrainLatchAudioProcessorEditor(GrainLatchAudioProcessor& owner)
    : AudioProcessorEditor(owner), ownerProcessor(owner)
{
    setLookAndFeel(&lookAndFeel);
    setName("GrainLatch editor");
    setComponentID("grainlatch-editor");
    setTitle("GrainLatch");
    setDescription("GrainLatch monochrome 8-bit live granular damage editor");
    setWantsKeyboardFocus(true);

    matrix.setComponentID("grainlatch-grain-field");
    addAndMakeVisible(matrix);

    design::styleLabel(status);
    status.setComponentID("grainlatch-status");
    status.setJustificationType(juce::Justification::centredLeft);
    status.setColour(juce::Label::textColourId, design::Palette::mid());
    addAndMakeVisible(status);

    const juce::StringArray sliderNames { "SIZE", "DENS", "JIT", "REV", "STUT", "DMG", "MIX", "OUT" };
    for (std::size_t i = 0; i < sliders.size(); ++i)
        configureControl(sliders[i], labels[i], sliderNames[static_cast<int>(i)]);

    design::styleLabel(labels[8]);
    labels[8].setText("FREEZE", juce::dontSendNotification);
    labels[8].setJustificationType(juce::Justification::centred);
    addAndMakeVisible(labels[8]);

    design::styleLabel(labels[9]);
    labels[9].setText("TRIG", juce::dontSendNotification);
    labels[9].setJustificationType(juce::Justification::centred);
    addAndMakeVisible(labels[9]);

    freezeButton.setComponentID("grainlatch-freeze");
    freezeButton.setButtonText("");
    freezeButton.setClickingTogglesState(true);
    freezeButton.setColour(juce::ToggleButton::tickColourId, design::Palette::paper());
    freezeButton.setColour(juce::ToggleButton::tickDisabledColourId, design::Palette::mid());
    addAndMakeVisible(freezeButton);

    retriggerButton.setComponentID("grainlatch-retrigger");
    retriggerButton.setButtonText("");
    retriggerButton.setClickingTogglesState(true);
    retriggerButton.setColour(juce::ToggleButton::tickColourId, design::Palette::paper());
    retriggerButton.setColour(juce::ToggleButton::tickDisabledColourId, design::Palette::mid());
    addAndMakeVisible(retriggerButton);

    sliderAttachments[0] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        ownerProcessor.parameters, grainlatch::parameters::grainMs, sliders[0]);
    sliderAttachments[1] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        ownerProcessor.parameters, grainlatch::parameters::density, sliders[1]);
    sliderAttachments[2] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        ownerProcessor.parameters, grainlatch::parameters::jitter, sliders[2]);
    sliderAttachments[3] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        ownerProcessor.parameters, grainlatch::parameters::reverse, sliders[3]);
    sliderAttachments[4] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        ownerProcessor.parameters, grainlatch::parameters::stutter, sliders[4]);
    sliderAttachments[5] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        ownerProcessor.parameters, grainlatch::parameters::damage, sliders[5]);
    sliderAttachments[6] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        ownerProcessor.parameters, grainlatch::parameters::mix, sliders[6]);
    sliderAttachments[7] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        ownerProcessor.parameters, grainlatch::parameters::output, sliders[7]);
    freezeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        ownerProcessor.parameters, grainlatch::parameters::freeze, freezeButton);
    retriggerAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        ownerProcessor.parameters, grainlatch::parameters::retrigger, retriggerButton);

    setResizable(false, false);
    setSize(defaultWidth, defaultHeight);
    startTimerHz(24);
    updateReadout();
}

GrainLatchAudioProcessorEditor::~GrainLatchAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
}

void GrainLatchAudioProcessorEditor::paint(juce::Graphics& graphics)
{
    design::paintEditorChrome(graphics, getLocalBounds(), "GrainLatch", "LIVE GRAIN DAMAGE");

    auto bounds = getLocalBounds().withTrimmedTop(64).reduced(design::Metrics::margin, 0);
    auto matrixBounds = bounds.removeFromTop(132);
    graphics.setColour(design::Palette::low());
    graphics.fillRect(matrixBounds);
    graphics.setColour(design::Palette::mid());
    graphics.drawRect(matrixBounds, 1);

    auto statusBounds = bounds.removeFromTop(24).withTrimmedTop(8);
    graphics.setColour(design::Palette::ink());
    graphics.fillRect(statusBounds);
    graphics.setColour(design::Palette::mid());
    graphics.drawRect(statusBounds, 1);

    auto controls = bounds.withTrimmedTop(8).withTrimmedBottom(design::Metrics::margin);
    graphics.setColour(design::Palette::low());
    graphics.drawRect(controls, 1);
}

void GrainLatchAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().withTrimmedTop(64).reduced(design::Metrics::margin, 0);
    matrix.setBounds(bounds.removeFromTop(132).reduced(8));
    status.setBounds(bounds.removeFromTop(24).withTrimmedTop(8).reduced(8, 0));
    auto controls = bounds.withTrimmedTop(8).withTrimmedBottom(design::Metrics::margin);

    const auto gap = 4;
    const auto labelH = 14;
    const auto rowH = controls.getHeight() / 2;
    auto top = controls.removeFromTop(rowH).reduced(8, 4);
    auto bottom = controls.reduced(8, 4);

    const auto topW = top.getWidth() / 5;
    for (int i = 0; i < 5; ++i)
    {
        auto cell = top.removeFromLeft(topW).reduced(gap, 0);
        labels[static_cast<std::size_t>(i)].setBounds(cell.removeFromTop(labelH));
        sliders[static_cast<std::size_t>(i)].setBounds(cell);
    }

    const auto bottomW = bottom.getWidth() / 5;
    for (int i = 5; i < 8; ++i)
    {
        auto cell = bottom.removeFromLeft(bottomW).reduced(gap, 0);
        labels[static_cast<std::size_t>(i)].setBounds(cell.removeFromTop(labelH));
        sliders[static_cast<std::size_t>(i)].setBounds(cell);
    }
    auto freezeCell = bottom.removeFromLeft(bottomW).reduced(gap, 0);
    labels[8].setBounds(freezeCell.removeFromTop(labelH));
    freezeButton.setBounds(freezeCell.reduced(12, 2));

    auto freezeCell2 = bottom.reduced(gap, 0);
    labels[9].setBounds(freezeCell2.removeFromTop(labelH));
    retriggerButton.setBounds(freezeCell2.reduced(12, 2));
}

void GrainLatchAudioProcessorEditor::timerCallback()
{
    grainlatch::dsp::GrainSnapshot snapshot;
    ownerProcessor.copyGrainSnapshot(snapshot);
    matrix.setSnapshot(snapshot);
    updateReadout();
}

void GrainLatchAudioProcessorEditor::updateReadout()
{
    grainlatch::dsp::GrainSnapshot snapshot;
    ownerProcessor.copyGrainSnapshot(snapshot);
    matrix.setSnapshot(snapshot);

    auto state = juce::String("LIVE");
    if (snapshot.frozen)
        state = "FREEZE";
    else if (snapshot.recovery)
        state = "RECOVER";
    else if (snapshot.capturing)
        state = "CAPTURE";

    status.setText(juce::String::formatted("%s   IN %.4f   WET %.4f   HOLD %.4f   G %d",
                                           state.toRawUTF8(),
                                           snapshot.inputRms,
                                           snapshot.wetRms,
                                           snapshot.heldRms,
                                           snapshot.activeGrains),
                   juce::dontSendNotification);
}

void GrainLatchAudioProcessorEditor::configureControl(juce::Slider& slider,
                                                       juce::Label& label,
                                                       const juce::String& text)
{
    label.setComponentID("grainlatch-label-" + text.toLowerCase());
    design::styleLabel(label);
    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(label);

    slider.setComponentID("grainlatch-control-" + text.toLowerCase());
    slider.setSliderStyle(juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 48, 18);
    slider.setColour(juce::Slider::trackColourId, design::Palette::paper());
    slider.setColour(juce::Slider::backgroundColourId, design::Palette::low());
    slider.setColour(juce::Slider::thumbColourId, design::Palette::paper());
    slider.setColour(juce::Slider::textBoxTextColourId, design::Palette::paper());
    slider.setColour(juce::Slider::textBoxOutlineColourId, design::Palette::mid());
    addAndMakeVisible(slider);
}

void GrainLatchAudioProcessorEditor::GrainField::setSnapshot(const grainlatch::dsp::GrainSnapshot& next)
{
    snapshot = next;
    repaint();
}

void GrainLatchAudioProcessorEditor::GrainField::paint(juce::Graphics& graphics)
{
    graphics.fillAll(design::Palette::ink());
    const auto area = getLocalBounds();
    const auto cellW = area.getWidth() / grainlatch::dsp::GrainSnapshot::columns;
    const auto cellH = area.getHeight() / grainlatch::dsp::GrainSnapshot::rows;

    for (int y = 0; y < grainlatch::dsp::GrainSnapshot::rows; ++y)
    {
        for (int x = 0; x < grainlatch::dsp::GrainSnapshot::columns; ++x)
        {
            const auto value = snapshot.cells[static_cast<std::size_t>(y * grainlatch::dsp::GrainSnapshot::columns + x)];
            auto cell = juce::Rectangle<int>(area.getX() + x * cellW,
                                             area.getY() + y * cellH,
                                             juce::jmax(1, cellW - 1),
                                             juce::jmax(1, cellH - 1));
            graphics.setColour(value > 0.66f ? design::Palette::paper()
                              : value > 0.25f ? design::Palette::mid()
                              : design::Palette::low());
            if (value > 0.0f)
                graphics.fillRect(cell);
            else
                graphics.drawRect(cell, 1);
        }
    }

    if (snapshot.recovery)
    {
        graphics.setColour(design::Palette::paper());
        for (int x = 0; x < area.getWidth(); x += 8)
            graphics.drawVerticalLine(area.getX() + x, static_cast<float>(area.getY()), static_cast<float>(area.getBottom()));
    }

    if (snapshot.frozen)
    {
        graphics.setColour(design::Palette::paper());
        graphics.drawRect(area.reduced(2), 2);
    }

    if (hasKeyboardFocus(true))
    {
        graphics.setColour(design::Palette::paper());
        graphics.drawRect(area, 2);
    }
}
