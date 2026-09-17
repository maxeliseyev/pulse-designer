#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "dsp/Envelope.h"
#include "dsp/OneShotRenderer.h"
#include "dsp/Oscillator.h"
#include "dsp/NoiseGenerator.h"
#include "dsp/SynthEngine.h"
#include "dsp/TptStateVariableFilter.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

using Catch::Matchers::WithinAbs;

TEST_CASE("sine oscillator honours frequency and start phase")
{
    pulse::Oscillator oscillator;
    oscillator.prepare(48000.0);
    oscillator.start(1000.0f, 90.0f, pulse::Waveform::sine);

    REQUIRE_THAT(oscillator.processSample(), WithinAbs(1.0f, 1.0e-6f));
    REQUIRE_THAT(oscillator.processSample(), WithinAbs(0.9914449f, 1.0e-5f));
}

TEST_CASE("waveform selection is deterministic")
{
    pulse::Oscillator oscillator;
    oscillator.prepare(48000.0);

    oscillator.start(1000.0f, 0.0f, pulse::Waveform::triangle);
    REQUIRE_THAT(oscillator.processSample(), WithinAbs(-1.0f, 1.0e-6f));

    oscillator.start(1000.0f, 0.0f, pulse::Waveform::square);
    REQUIRE_THAT(oscillator.processSample(), WithinAbs(1.0f, 1.0e-6f));
}

TEST_CASE("noise generators are deterministic and finite")
{
    for (const auto type : { pulse::NoiseType::white,
                             pulse::NoiseType::pink,
                             pulse::NoiseType::metallic,
                             pulse::NoiseType::sampleAndHold })
    {
        pulse::NoiseGenerator first;
        pulse::NoiseGenerator second;
        first.prepare(48000.0);
        second.prepare(48000.0);
        first.setType(type);
        second.setType(type);
        first.setSampleAndHoldRate(800.0f);
        second.setSampleAndHoldRate(800.0f);
        first.reset(12345u);
        second.reset(12345u);

        for (int sample = 0; sample < 512; ++sample)
        {
            const auto firstSample = first.processSample();
            const auto secondSample = second.processSample();
            REQUIRE(std::isfinite(firstSample));
            REQUIRE(firstSample == secondSample);
            REQUIRE(std::abs(firstSample) <= 1.0f);
        }
    }
}

TEST_CASE("sample and hold noise keeps a value for its period")
{
    pulse::NoiseGenerator noise;
    noise.prepare(48000.0);
    noise.setType(pulse::NoiseType::sampleAndHold);
    noise.setSampleAndHoldRate(1000.0f);
    noise.reset(6789u);

    const auto first = noise.processSample();
    for (int sample = 1; sample < 48; ++sample)
        REQUIRE(noise.processSample() == first);

    REQUIRE(noise.processSample() != first);
}

TEST_CASE("TPT filter exposes a continuous low-band-high morph")
{
    pulse::TptStateVariableFilter filter;
    filter.prepare(48000.0);
    filter.setParameters(2000.0f, 0.2f, 0.0f);

    float lowEnergy = 0.0f;
    for (int sample = 0; sample < 512; ++sample)
    {
        const auto output = filter.processSample(sample == 0 ? 1.0f : 0.0f);
        REQUIRE(std::isfinite(output.low));
        REQUIRE(std::isfinite(output.band));
        REQUIRE(std::isfinite(output.high));
        REQUIRE_THAT(output.morphed, WithinAbs(output.low, 1.0e-7f));
        lowEnergy += std::abs(output.low);
    }

    filter.reset();
    filter.setParameters(2000.0f, 0.2f, 0.5f);
    const auto bandOutput = filter.processSample(1.0f);
    REQUIRE_THAT(bandOutput.morphed, WithinAbs(bandOutput.band, 1.0e-7f));

    filter.reset();
    filter.setParameters(2000.0f, 0.2f, 1.0f);
    float highEnergy = 0.0f;
    for (int sample = 0; sample < 512; ++sample)
    {
        const auto output = filter.processSample(sample == 0 ? 1.0f : 0.0f);
        REQUIRE_THAT(output.morphed, WithinAbs(output.high, 1.0e-7f));
        highEnergy += std::abs(output.high);
    }

    REQUIRE(lowEnergy > 0.0f);
    REQUIRE(highEnergy > 0.0f);
}

TEST_CASE("noise voice can render a filtered hit")
{
    pulse::SynthConfig config;
    config.noiseType = pulse::NoiseType::pink;
    config.noiseMix = 1.0f;
    config.noiseCutoffHz = 4000.0f;
    config.noiseAmpDecayMs = 50.0f;

    const auto rendered = pulse::renderOneShot(config, 48000.0, 4800);

    REQUIRE(rendered.size() == 4800);
    REQUIRE(rendered.front() != 0.0f);
    REQUIRE(rendered.back() == 0.0f);
    for (float sample : rendered)
        REQUIRE(std::isfinite(sample));
}

TEST_CASE("noise render stays finite at supported sample rates")
{
    pulse::SynthConfig config;
    config.noiseType = pulse::NoiseType::metallic;
    config.noiseMix = 1.0f;
    config.noiseAmpDecayMs = 40.0f;

    for (const auto sampleRate : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        const auto sampleCount = static_cast<int>(sampleRate * 0.1);
        const auto rendered = pulse::renderOneShot(config, sampleRate, sampleCount);
        REQUIRE_FALSE(rendered.empty());
        for (float sample : rendered)
            REQUIRE(std::isfinite(sample));
        REQUIRE(rendered.back() == 0.0f);
    }
}

TEST_CASE("amp envelope is sample based and reaches its tail")
{
    pulse::ExponentialEnvelope envelope;
    envelope.prepare(48000.0);
    envelope.start(0.0f, 0.0f, 100.0f, 1.0f);

    REQUIRE_THAT(envelope.processSample(), WithinAbs(1.0f, 1.0e-6f));

    float previous = 1.0f;
    for (int sample = 1; sample < 4800; ++sample)
    {
        const float value = envelope.processSample();
        REQUIRE(value <= previous);
        previous = value;
    }

    REQUIRE(envelope.value() == 0.0f);
    REQUIRE_FALSE(envelope.isActive());
}

TEST_CASE("envelope retrigger starts from its current value")
{
    pulse::ExponentialEnvelope envelope;
    envelope.prepare(48000.0);
    envelope.start(0.0f, 0.0f, 100.0f, 1.0f);

    for (int sample = 0; sample < 100; ++sample)
        envelope.processSample();

    const float current = envelope.value();
    envelope.start(current, 0.0f, 100.0f, 1.0f);

    REQUIRE_THAT(envelope.processSample(), WithinAbs(current, 1.0e-6f));
}

TEST_CASE("one-shot renderer produces a finite decaying hit")
{
    pulse::SynthConfig config;
    config.pitchHz = 55.0f;
    config.ampDecayMs = 100.0f;

    const auto rendered = pulse::renderOneShot(config, 48000.0, 9600);

    REQUIRE(rendered.size() == 9600);
    REQUIRE(std::abs(rendered.front()) > 0.5f);
    REQUIRE(rendered.back() == 0.0f);
    for (float sample : rendered)
        REQUIRE(std::isfinite(sample));
}

TEST_CASE("the same MIDI event is independent of block size")
{
    constexpr double sampleRate = 48000.0;
    constexpr int totalSamples = 4096;
    constexpr int eventOffset = 37;

    pulse::SynthConfig config;
    config.pitchHz = 55.0f;
    config.ampDecayMs = 80.0f;

    pulse::SynthEngine oneBlock;
    oneBlock.prepare(sampleRate, totalSamples);
    oneBlock.setConfig(config);
    const pulse::NoteEvent event { pulse::NoteEventType::noteOn, eventOffset, 60, 1.0f };
    std::vector<float> expected(static_cast<size_t>(totalSamples));
    oneBlock.processMono(&event, 1, expected.data(), totalSamples);

    pulse::SynthEngine splitBlocks;
    splitBlocks.prepare(sampleRate, 512);
    splitBlocks.setConfig(config);
    std::vector<float> actual(static_cast<size_t>(totalSamples), 0.0f);

    int rendered = 0;
    while (rendered < totalSamples)
    {
        const int blockSize = std::min(512, totalSamples - rendered);
        const pulse::NoteEvent* events = rendered == 0 ? &event : nullptr;
        const int numEvents = rendered == 0 ? 1 : 0;
        splitBlocks.processMono(events,
                                numEvents,
                                actual.data() + rendered,
                                blockSize);
        rendered += blockSize;
    }

    REQUIRE(actual == expected);
}

TEST_CASE("the initial synth engine renders silence without events")
{
    pulse::SynthEngine synth;
    synth.prepare(48000.0, 128);

    std::array<float, 16> left {};
    std::array<float, 16> right {};
    left.fill(1.0f);
    right.fill(-1.0f);
    float* outputs[] = { left.data(), right.data() };

    synth.process(nullptr, 0, outputs, 2, static_cast<int>(left.size()));

    for (float sample : left)
        REQUIRE(sample == 0.0f);
    for (float sample : right)
        REQUIRE(sample == 0.0f);
}
