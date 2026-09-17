#pragma once

#include "Parameters.h"
#include "dsp/Constants.h"
#include "dsp/SynthEngine.h"

#include <array>

#include <juce_audio_processors/juce_audio_processors.h>

class PulseDesignerAudioProcessor final : public juce::AudioProcessor
{
public:
    PulseDesignerAudioProcessor();
    ~PulseDesignerAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override;
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getParameters() { return parameters; }
    const juce::AudioProcessorValueTreeState& getParameters() const
    {
        return parameters;
    }

private:
    static BusesProperties createBusesProperties();

    juce::AudioProcessorValueTreeState parameters;
    pulse::SynthEngine synth;
    std::array<pulse::NoteEvent, pulse::kMaxMidiEventsPerBlock> midiEvents {};
    double currentSampleRate = 48000.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PulseDesignerAudioProcessor)
};
