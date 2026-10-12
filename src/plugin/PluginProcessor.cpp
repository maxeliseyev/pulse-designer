#include "PluginProcessor.h"

#include "PluginEditor.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace
{
const juce::Identifier stateVersionProperty { "stateVersion" };
} // namespace

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
    synth.prepare(sampleRate, samplesPerBlock);
    synth.setConfig(parameterSnapshot.read());
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
    juce::ScopedNoDenormals noDenormals;

    // The engine latches this snapshot on each note-on inside the block.
    synth.setConfig(parameterSnapshot.read());

    int numEvents = 0;
    for (const juce::MidiMessageMetadata metadata : midi)
    {
        if (numEvents >= pulse::kMaxMidiEventsPerBlock)
            break;

        const auto message = metadata.getMessage();
        if (!message.isNoteOnOrOff())
            continue;

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
    auto state = parameters.copyState();
    state.setProperty(stateVersionProperty, kStateVersion, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary(*xml, destData);
}

void PulseDesignerAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary(data, sizeInBytes);
    if (xml == nullptr || !xml->hasTagName(parameters.state.getType()))
        return;

    auto state = juce::ValueTree::fromXml(*xml);
    // Version 1 holds the first public parameter set; a later format adds its
    // migration here before the tree reaches the APVTS. Newer states load the
    // parameters this build knows and ignore the rest.
    state.removeProperty(stateVersionProperty, nullptr);

    // The APVTS gives a parameter missing from the tree its default value, so an
    // older state never inherits values from the project this instance held before.
    parameters.replaceState(state);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PulseDesignerAudioProcessor();
}
