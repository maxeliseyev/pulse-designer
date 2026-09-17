#include "PluginProcessor.h"

#include "PluginEditor.h"

#include <juce_audio_processors/juce_audio_processors.h>

PulseDesignerAudioProcessor::PulseDesignerAudioProcessor()
    : AudioProcessor(createBusesProperties()),
      parameters(*this, nullptr, "PulseDesigner", pulse::createParameterLayout())
{
}

const juce::String PulseDesignerAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

juce::AudioProcessor::BusesProperties
PulseDesignerAudioProcessor::createBusesProperties()
{
    return BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true);
}

void PulseDesignerAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    synth.prepare(sampleRate, samplesPerBlock);
}

void PulseDesignerAudioProcessor::releaseResources()
{
    synth.reset();
}

bool PulseDesignerAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto input = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();

    if (!input.isDisabled())
        return false;

    return output == juce::AudioChannelSet::stereo();
}

void PulseDesignerAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                               juce::MidiBuffer& midi)
{
    juce::ignoreUnused(currentSampleRate);
    juce::ScopedNoDenormals noDenormals;

    int numEvents = 0;
    for (const juce::MidiMessageMetadata metadata : midi)
    {
        if (numEvents >= pulse::kMaxMidiEventsPerBlock)
            break;

        const auto message = metadata.getMessage();
        auto& event = midiEvents[static_cast<size_t>(numEvents)];
        event.sampleOffset = metadata.samplePosition;
        event.midiNote = message.getNoteNumber();
        event.velocity = message.getFloatVelocity();
        event.type = message.isNoteOn() ? pulse::NoteEventType::noteOn
                                        : pulse::NoteEventType::noteOff;
        ++numEvents;
    }

    synth.process(midiEvents.data(),
                  numEvents,
                  buffer.getArrayOfWritePointers(),
                  buffer.getNumChannels(),
                  buffer.getNumSamples());
}

juce::AudioProcessorEditor* PulseDesignerAudioProcessor::createEditor()
{
    return new PulseDesignerAudioProcessorEditor(*this);
}

void PulseDesignerAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto xml = parameters.copyState().createXml())
        copyXmlToBinary(*xml, destData);
}

void PulseDesignerAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary(data, sizeInBytes);
    if (xml == nullptr || !xml->hasTagName(parameters.state.getType()))
        return;

    parameters.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PulseDesignerAudioProcessor();
}
