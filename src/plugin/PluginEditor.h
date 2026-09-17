#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

class PulseDesignerAudioProcessor;

class PulseDesignerAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit PulseDesignerAudioProcessorEditor(PulseDesignerAudioProcessor&);
    ~PulseDesignerAudioProcessorEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    PulseDesignerAudioProcessor& audioProcessor;
    juce::Label statusLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PulseDesignerAudioProcessorEditor)
};
