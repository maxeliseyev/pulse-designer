#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "PluginProcessor.h"

#include <juce_audio_utils/juce_audio_utils.h>

#include <cmath>
#include <set>

using Catch::Matchers::WithinAbs;

TEST_CASE("plugin renders a note-on at its exact sample offset")
{
    juce::ScopedJuceInitialiser_GUI gui;
    PulseDesignerAudioProcessor processor;
    processor.prepareToPlay(48000.0, 128);

    juce::AudioBuffer<float> buffer(2, 128);
    buffer.clear();
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, 1.0f), 37);

    processor.processBlock(buffer, midi);

    REQUIRE_THAT(buffer.getSample(0, 36), WithinAbs(0.0f, 1.0e-7f));
    REQUIRE(std::abs(buffer.getSample(0, 37)) > 0.2f);
    REQUIRE_THAT(buffer.getSample(0, 37),
                 WithinAbs(buffer.getSample(1, 37), 1.0e-7f));
}

TEST_CASE("plugin ignores MIDI messages that are not notes")
{
    juce::ScopedJuceInitialiser_GUI gui;
    PulseDesignerAudioProcessor processor;
    processor.prepareToPlay(48000.0, 128);

    juce::AudioBuffer<float> buffer(2, 128);
    buffer.clear();
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::controllerEvent(1, 7, 100), 0);
    midi.addEvent(juce::MidiMessage::pitchWheel(1, 12000), 3);
    midi.addEvent(juce::MidiMessage::noteOff(1, 60), 5);

    processor.processBlock(buffer, midi);

    REQUIRE_FALSE(buffer.getMagnitude(0, 128) > 0.0f);
}

TEST_CASE("plugin exposes an instrument bus layout")
{
    PulseDesignerAudioProcessor processor;
    const auto layouts = processor.getBusesLayout();

    REQUIRE(layouts.getMainInputChannelSet().isDisabled());
    REQUIRE(layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo());
    REQUIRE(processor.acceptsMidi());
    REQUIRE_FALSE(processor.producesMidi());
}

namespace
{
void setParameter(PulseDesignerAudioProcessor& processor, const char* id, float value)
{
    auto* parameter = processor.getValueTreeState().getParameter(id);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}

float renderPeak(PulseDesignerAudioProcessor& processor, int midiNote = 60)
{
    juce::AudioBuffer<float> buffer(2, 512);
    buffer.clear();
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, midiNote, 1.0f), 0);
    processor.processBlock(buffer, midi);
    return buffer.getMagnitude(0, 512);
}

void requireSameConfig(const pulse::SynthConfig& actual, const pulse::SynthConfig& expected)
{
    const auto close = [](float a, float b)
    { return std::abs(a - b) <= 1.0e-4f * std::max(1.0f, std::abs(b)); };

    REQUIRE(actual.waveform == expected.waveform);
    REQUIRE(close(actual.pitchHz, expected.pitchHz));
    REQUIRE(close(actual.startPhaseDegrees, expected.startPhaseDegrees));
    REQUIRE(close(actual.pitchEnvelopeAmountSemitones, expected.pitchEnvelopeAmountSemitones));
    REQUIRE(close(actual.pitchEnvelopeDecayMs, expected.pitchEnvelopeDecayMs));
    REQUIRE(close(actual.pitchEnvelopeCurve, expected.pitchEnvelopeCurve));
    REQUIRE(close(actual.ampAttackMs, expected.ampAttackMs));
    REQUIRE(close(actual.ampDecayMs, expected.ampDecayMs));
    REQUIRE(close(actual.ampCurve, expected.ampCurve));
    REQUIRE(close(actual.keyTracking, expected.keyTracking));
    REQUIRE(close(actual.shape, expected.shape));
    REQUIRE(actual.driveType == expected.driveType);
    REQUIRE(close(actual.drive, expected.drive));
    REQUIRE(actual.oversampling == expected.oversampling);
    REQUIRE(close(actual.tone, expected.tone));
    REQUIRE(close(actual.outputGainDb, expected.outputGainDb));
    REQUIRE(close(actual.pan, expected.pan));
    REQUIRE(actual.noiseType == expected.noiseType);
    REQUIRE(close(actual.noiseMix, expected.noiseMix));
    REQUIRE(close(actual.noiseCutoffHz, expected.noiseCutoffHz));
    REQUIRE(close(actual.noiseResonance, expected.noiseResonance));
    REQUIRE(close(actual.noiseFilterMorph, expected.noiseFilterMorph));
    REQUIRE(close(actual.filterEnvelopeAmount, expected.filterEnvelopeAmount));
    REQUIRE(close(actual.filterEnvelopeDecayMs, expected.filterEnvelopeDecayMs));
    REQUIRE(close(actual.noiseAmpAttackMs, expected.noiseAmpAttackMs));
    REQUIRE(close(actual.noiseAmpDecayMs, expected.noiseAmpDecayMs));
    REQUIRE(close(actual.noiseAmpCurve, expected.noiseAmpCurve));
    REQUIRE(actual.noiseBursts == expected.noiseBursts);
    REQUIRE(close(actual.burstSpacingMs, expected.burstSpacingMs));
    REQUIRE(close(actual.velocityToLevel, expected.velocityToLevel));
    REQUIRE(close(actual.velocityToPitchEnvelope, expected.velocityToPitchEnvelope));
    REQUIRE(close(actual.velocityToCutoff, expected.velocityToCutoff));
    REQUIRE(close(actual.velocityCurve, expected.velocityCurve));
    REQUIRE(actual.voiceMode == expected.voiceMode);
    REQUIRE(actual.voices == expected.voices);
    REQUIRE(actual.noteFilterMode == expected.noteFilterMode);
    REQUIRE(actual.noteFilterNote == expected.noteFilterNote);
    REQUIRE(actual.noteFilterLow == expected.noteFilterLow);
    REQUIRE(actual.noteFilterHigh == expected.noteFilterHigh);
}
} // namespace

TEST_CASE("parameter IDs are unique, versioned and complete")
{
    PulseDesignerAudioProcessor processor;
    std::set<juce::String> ids;

    for (auto* parameter : processor.getParameters())
    {
        auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*>(parameter);
        REQUIRE(withId != nullptr);
        REQUIRE(withId->getVersionHint() == 1);
        REQUIRE(ids.insert(withId->paramID).second);
    }

    REQUIRE(ids.size() == 39);
}

TEST_CASE("parameter defaults match the synth defaults from the spec")
{
    PulseDesignerAudioProcessor processor;
    const pulse::ParameterSnapshot snapshot(processor.getValueTreeState());

    requireSameConfig(snapshot.read(), pulse::SynthConfig {});
}

TEST_CASE("time and frequency parameters use logarithmic ranges")
{
    PulseDesignerAudioProcessor processor;
    auto& state = processor.getValueTreeState();

    auto* cutoff = state.getParameter(pulse::param::noiseCutoff);
    REQUIRE_THAT(cutoff->convertFrom0to1(0.5f), WithinAbs(632.456f, 0.05f));
    auto* decay = state.getParameter(pulse::param::oscAmpDecay);
    REQUIRE_THAT(decay->convertFrom0to1(0.0f), WithinAbs(5.0f, 1.0e-4f));
    REQUIRE_THAT(decay->convertFrom0to1(1.0f), WithinAbs(3000.0f, 0.01f));

    REQUIRE(cutoff->getText(cutoff->convertTo0to1(2000.0f), 32) == "2.00 kHz");
    REQUIRE_THAT(cutoff->convertFrom0to1(cutoff->getValueForText("2 kHz")),
                 WithinAbs(2000.0f, 0.1f));
    auto* gain = state.getParameter(pulse::param::gain);
    REQUIRE(gain->getText(0.0f, 32) == "-inf dB");
    REQUIRE(decay->getText(decay->convertTo0to1(1500.0f), 32) == "1.50 s");
}

TEST_CASE("parameter changes reach the next note")
{
    juce::ScopedJuceInitialiser_GUI gui;
    PulseDesignerAudioProcessor processor;
    processor.prepareToPlay(48000.0, 512);

    REQUIRE(renderPeak(processor) > 0.2f);

    processor.prepareToPlay(48000.0, 512);
    setParameter(processor, pulse::param::gain, -96.0f);
    REQUIRE_FALSE(renderPeak(processor) > 0.0f);

    processor.prepareToPlay(48000.0, 512);
    setParameter(processor, pulse::param::gain, 0.0f);
    setParameter(processor, pulse::param::noteFilterMode, 1.0f);
    setParameter(processor, pulse::param::noteFilterNote, 38.0f);
    REQUIRE_FALSE(renderPeak(processor, 60) > 0.0f);
    REQUIRE(renderPeak(processor, 38) > 0.2f);
}

TEST_CASE("a parameter change does not alter a sounding note")
{
    juce::ScopedJuceInitialiser_GUI gui;
    constexpr int blockSize = 256;

    const auto render = [&](bool change)
    {
        PulseDesignerAudioProcessor processor;
        processor.prepareToPlay(48000.0, blockSize);
        juce::AudioBuffer<float> output(2, blockSize * 4);
        juce::AudioBuffer<float> block(2, blockSize);

        for (int index = 0; index < 4; ++index)
        {
            if (change && index == 1)
            {
                setParameter(processor, pulse::param::oscPitch, 400.0f);
                setParameter(processor, pulse::param::pan, 1.0f);
                setParameter(processor, pulse::param::drive, 1.0f);
            }

            block.clear();
            juce::MidiBuffer midi;
            if (index == 0)
                midi.addEvent(juce::MidiMessage::noteOn(1, 60, 1.0f), 10);

            processor.processBlock(block, midi);
            for (int channel = 0; channel < 2; ++channel)
                output.copyFrom(channel, index * blockSize, block, channel, 0, blockSize);
        }

        return output;
    };

    const auto steady = render(false);
    const auto changed = render(true);
    for (int channel = 0; channel < 2; ++channel)
    {
        for (int sample = 0; sample < blockSize * 4; ++sample)
            REQUIRE_FALSE(std::abs(steady.getSample(channel, sample)
                                   - changed.getSample(channel, sample))
                          > 0.0f);
    }
}

TEST_CASE("state round-trips every parameter")
{
    PulseDesignerAudioProcessor source;
    int index = 0;
    for (auto* parameter : source.getParameters())
    {
        const auto normalized = 0.1f + 0.8f * static_cast<float>(index % 7) / 6.0f;
        parameter->setValueNotifyingHost(normalized);
        ++index;
    }

    juce::MemoryBlock saved;
    source.getStateInformation(saved);

    const auto xml = juce::AudioProcessor::getXmlFromBinary(saved.getData(),
                                                            static_cast<int>(saved.getSize()));
    REQUIRE(xml != nullptr);
    REQUIRE(xml->getIntAttribute("stateVersion") == PulseDesignerAudioProcessor::kStateVersion);

    PulseDesignerAudioProcessor restored;
    restored.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));

    const auto& sourceParameters = source.getParameters();
    const auto& restoredParameters = restored.getParameters();
    REQUIRE(sourceParameters.size() == restoredParameters.size());
    for (int parameter = 0; parameter < sourceParameters.size(); ++parameter)
    {
        REQUIRE_THAT(restoredParameters[parameter]->getValue(),
                     WithinAbs(sourceParameters[parameter]->getValue(), 1.0e-6f));
    }

    const pulse::ParameterSnapshot sourceSnapshot(source.getValueTreeState());
    const pulse::ParameterSnapshot restoredSnapshot(restored.getValueTreeState());
    requireSameConfig(restoredSnapshot.read(), sourceSnapshot.read());
}

TEST_CASE("state without some parameters restores their defaults")
{
    PulseDesignerAudioProcessor processor;
    setParameter(processor, pulse::param::drive, 0.8f);
    setParameter(processor, pulse::param::oscPitch, 200.0f);

    juce::XmlElement legacy("PulseDesigner");
    auto* pitch = legacy.createNewChildElement("PARAM");
    pitch->setAttribute("id", pulse::param::oscPitch);
    pitch->setAttribute("value", 110.0);
    auto* unknown = legacy.createNewChildElement("PARAM");
    unknown->setAttribute("id", "retired_parameter");
    unknown->setAttribute("value", 1.0);

    juce::MemoryBlock saved;
    juce::AudioProcessor::copyXmlToBinary(legacy, saved);
    processor.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));

    const auto config = pulse::ParameterSnapshot(processor.getValueTreeState()).read();
    REQUIRE_THAT(config.pitchHz, WithinAbs(110.0f, 1.0e-3f));
    REQUIRE_THAT(config.drive, WithinAbs(0.0f, 1.0e-6f));
}

TEST_CASE("foreign or corrupt state is ignored")
{
    PulseDesignerAudioProcessor processor;
    setParameter(processor, pulse::param::drive, 0.8f);

    juce::XmlElement foreign("SomethingElse");
    juce::MemoryBlock saved;
    juce::AudioProcessor::copyXmlToBinary(foreign, saved);
    processor.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));

    const char garbage[] = "not a plugin state";
    processor.setStateInformation(garbage, static_cast<int>(sizeof(garbage)));

    const auto config = pulse::ParameterSnapshot(processor.getValueTreeState()).read();
    REQUIRE_THAT(config.drive, WithinAbs(0.8f, 1.0e-6f));
}
