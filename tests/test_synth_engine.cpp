#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "dsp/Envelope.h"
#include "dsp/DcBlocker.h"
#include "dsp/OneShotRenderer.h"
#include "dsp/Oscillator.h"
#include "dsp/NoiseGenerator.h"
#include "dsp/NonlinearStage.h"
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
    REQUIRE(std::abs(rendered.back()) < 1.0e-5f);
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
        REQUIRE(std::abs(rendered.back()) < 1.0e-5f);
    }
}

TEST_CASE("pitch envelope sweeps the oscillator frequency")
{
    pulse::SynthConfig withoutPitchSweep;
    withoutPitchSweep.pitchEnvelopeAmountSemitones = 0.0f;
    withoutPitchSweep.ampDecayMs = 100.0f;

    auto withPitchSweep = withoutPitchSweep;
    withPitchSweep.pitchEnvelopeAmountSemitones = 24.0f;
    withPitchSweep.pitchEnvelopeDecayMs = 40.0f;

    const auto steady = pulse::renderOneShot(withoutPitchSweep, 48000.0, 512);
    const auto swept = pulse::renderOneShot(withPitchSweep, 48000.0, 512);

    REQUIRE(steady.front() == swept.front());
    REQUIRE(std::abs(steady[64] - swept[64]) > 1.0e-3f);
}

TEST_CASE("velocity mapping scales a softer hit")
{
    pulse::SynthConfig config;
    config.pitchEnvelopeAmountSemitones = 0.0f;
    config.ampDecayMs = 100.0f;

    const auto full = pulse::renderOneShot(config, 48000.0, 256, 60, 1.0f);
    const auto soft = pulse::renderOneShot(config, 48000.0, 256, 60, 0.25f);
    const auto shapedVelocity = std::pow(0.25f, 1.25f);
    const auto expectedLevel = 0.3f + 0.7f * shapedVelocity;

    REQUIRE_THAT(full.front(), WithinAbs(1.0f, 1.0e-6f));
    REQUIRE_THAT(soft.front(), WithinAbs(expectedLevel, 1.0e-6f));
    REQUIRE(soft.front() < full.front());
}

TEST_CASE("noise bursts create separated deterministic hits")
{
    pulse::SynthConfig config;
    config.noiseType = pulse::NoiseType::white;
    config.noiseMix = 1.0f;
    config.pitchEnvelopeAmountSemitones = 0.0f;
    config.noiseAmpDecayMs = 2.0f;
    config.noiseBursts = 3;
    config.burstSpacingMs = 5.0f;

    const auto rendered = pulse::renderOneShot(config, 48000.0, 1000);
    const auto energy = [&rendered](int first, int last) {
        float total = 0.0f;
        for (int sample = first; sample < last; ++sample)
            total += std::abs(rendered[static_cast<std::size_t>(sample)]);
        return total;
    };

    REQUIRE(energy(0, 96) > 0.0f);
    REQUIRE(energy(240, 336) > 0.0f);
    REQUIRE(energy(480, 576) > 0.0f);
    REQUIRE(energy(120, 200) < energy(0, 96) * 0.1f);
    REQUIRE(energy(360, 440) < energy(240, 336) * 0.1f);
}

TEST_CASE("noise bursts are independent of block size")
{
    constexpr double sampleRate = 48000.0;
    constexpr int totalSamples = 2048;
    constexpr int eventOffset = 37;

    pulse::SynthConfig config;
    config.noiseType = pulse::NoiseType::sampleAndHold;
    config.noiseMix = 1.0f;
    config.pitchEnvelopeAmountSemitones = 0.0f;
    config.noiseAmpDecayMs = 2.0f;
    config.noiseBursts = 4;
    config.burstSpacingMs = 7.0f;

    pulse::SynthEngine oneBlock;
    oneBlock.prepare(sampleRate, totalSamples);
    oneBlock.setConfig(config);
    const pulse::NoteEvent event { pulse::NoteEventType::noteOn, eventOffset, 60, 1.0f };
    std::vector<float> expected(static_cast<std::size_t>(totalSamples));
    oneBlock.processMono(&event, 1, expected.data(), totalSamples);

    pulse::SynthEngine splitBlocks;
    splitBlocks.prepare(sampleRate, 256);
    splitBlocks.setConfig(config);
    std::vector<float> actual(static_cast<std::size_t>(totalSamples), 0.0f);

    int rendered = 0;
    while (rendered < totalSamples)
    {
        const int blockSize = std::min(256, totalSamples - rendered);
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

TEST_CASE("nonlinear stages support every oversampling factor")
{
    for (const auto factor : { 1, 2, 4, 8 })
    {
        pulse::NonlinearStage shape(pulse::NonlinearStage::Kind::shape);
        shape.setAmount(0.75f);
        shape.setOversampling(factor);

        pulse::NonlinearStage drive(pulse::NonlinearStage::Kind::drive);
        drive.setAmount(0.8f);
        drive.setOversampling(factor);

        for (const auto type : { pulse::DriveType::soft,
                                 pulse::DriveType::hard,
                                 pulse::DriveType::asymmetric,
                                 pulse::DriveType::fold })
        {
            drive.setDriveType(type);
            for (int sample = 0; sample < 256; ++sample)
            {
                const auto input = std::sin(0.03f * static_cast<float>(sample));
                const auto shaped = shape.processSample(input);
                const auto driven = drive.processSample(shaped);
                REQUIRE(std::isfinite(shaped));
                REQUIRE(std::isfinite(driven));
            }

            shape.reset();
            drive.reset();
        }
    }
}

TEST_CASE("nonlinear stages bypass exactly at zero amount")
{
    pulse::NonlinearStage shape(pulse::NonlinearStage::Kind::shape);
    pulse::NonlinearStage drive(pulse::NonlinearStage::Kind::drive);
    drive.setDriveType(pulse::DriveType::fold);
    shape.setOversampling(8);
    drive.setOversampling(8);

    for (const auto input : { -0.8f, -0.1f, 0.0f, 0.25f, 0.9f })
    {
        REQUIRE(shape.processSample(input) == input);
        REQUIRE(drive.processSample(input) == input);
    }
}

TEST_CASE("10 Hz DC blocker removes a constant offset")
{
    pulse::DcBlocker blocker;
    blocker.prepare(48000.0);

    float output = 0.0f;
    for (int sample = 0; sample < 48000; ++sample)
        output = blocker.processSample(1.0f);

    REQUIRE(std::abs(output) < 1.0e-3f);
}

TEST_CASE("shape and drive are part of the one-shot render")
{
    pulse::SynthConfig config;
    config.pitchEnvelopeAmountSemitones = 0.0f;
    config.shape = 0.65f;
    config.driveType = pulse::DriveType::asymmetric;
    config.drive = 0.75f;
    config.oversampling = 4;
    config.ampDecayMs = 100.0f;

    const auto rendered = pulse::renderOneShot(config, 48000.0, 24000);

    REQUIRE(rendered.size() == 24000);
    for (float sample : rendered)
        REQUIRE(std::isfinite(sample));
    REQUIRE(std::abs(rendered.front()) <= 1.0f);
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
    REQUIRE(std::abs(rendered.back()) < 1.0e-5f);
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
