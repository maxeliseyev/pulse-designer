#include "PluginEditor.h"

#include "PluginProcessor.h"

PulseDesignerAudioProcessorEditor::PulseDesignerAudioProcessorEditor(
    PulseDesignerAudioProcessor& audioProcessorToEdit)
    : AudioProcessorEditor(audioProcessorToEdit), audioProcessor(audioProcessorToEdit)
{
    statusLabel.setText(audioProcessor.getName()
                            + juce::String("\nMIDI instrument skeleton"),
                        juce::dontSendNotification);
    statusLabel.setJustificationType(juce::Justification::centred);
    statusLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(statusLabel);

    setSize(640, 360);
}

void PulseDesignerAudioProcessorEditor::paint(juce::Graphics& graphics)
{
    graphics.fillAll(juce::Colour(0xff20242b));
}

void PulseDesignerAudioProcessorEditor::resized()
{
    statusLabel.setBounds(getLocalBounds().reduced(24));
}
