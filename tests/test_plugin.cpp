#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "PluginProcessor.h"

#include <juce_audio_utils/juce_audio_utils.h>

#include <cmath>

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
    REQUIRE(std::abs(buffer.getSample(0, 37)) > 0.5f);
    REQUIRE_THAT(buffer.getSample(0, 37),
                 WithinAbs(buffer.getSample(1, 37), 1.0e-7f));
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
