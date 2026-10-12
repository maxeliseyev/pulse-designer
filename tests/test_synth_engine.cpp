#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "dsp/Constants.h"
#include "dsp/Envelope.h"
#include "dsp/DcBlocker.h"
#include "dsp/OneShotRenderer.h"
#include "dsp/Oscillator.h"
#include "dsp/NoiseGenerator.h"
#include "dsp/NonlinearStage.h"
#include "dsp/SynthEngine.h"
#include "dsp/ToneFilter.h"
#include "dsp/TptStateVariableFilter.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>
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
    withoutPitchSweep.noiseMix = 0.0f;
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
    config.noiseMix = 0.0f;
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
    config.noiseMix = 0.0f;
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
    envelope.start(current, 5.0f, 100.0f, 1.0f);

    REQUIRE_THAT(envelope.processSample(), WithinAbs(current, 1.0e-6f));

    float peak = current;
    for (int sample = 0; sample < 240; ++sample)
        peak = std::max(peak, envelope.processSample());

    REQUIRE_THAT(peak, WithinAbs(1.0f, 1.0e-6f));
}

TEST_CASE("zero attack retrigger returns the envelope to its peak")
{
    pulse::ExponentialEnvelope envelope;
    envelope.prepare(48000.0);
    envelope.start(0.0f, 0.0f, 100.0f, 1.0f);

    for (int sample = 0; sample < 2400; ++sample)
        envelope.processSample();

    REQUIRE(envelope.value() < 0.1f);
    envelope.start(envelope.value(), 0.0f, 100.0f, 1.0f);

    REQUIRE_THAT(envelope.processSample(), WithinAbs(1.0f, 1.0e-6f));
}

TEST_CASE("one-shot renderer produces a finite decaying hit")
{
    pulse::SynthConfig config;
    config.noiseMix = 0.0f;
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

namespace
{
float windowRms(const std::vector<float>& rendered, int first, int last)
{
    double energy = 0.0;
    const int length = std::max(last - first, 1);
    for (int sample = first; sample < last; ++sample)
    {
        const auto value = static_cast<double>(rendered[static_cast<std::size_t>(sample)]);
        energy += value * value;
    }

    return static_cast<float>(std::sqrt(energy / static_cast<double>(length)));
}

pulse::SynthConfig steadyToneConfig()
{
    pulse::SynthConfig config;
    config.noiseMix = 0.0f;
    config.pitchEnvelopeAmountSemitones = 0.0f;
    config.ampDecayMs = 200.0f;
    return config;
}

pulse::SynthConfig linearMixConfig()
{
    pulse::SynthConfig config;
    config.pitchEnvelopeAmountSemitones = 0.0f;
    config.ampDecayMs = 80.0f;
    config.noiseAmpDecayMs = 80.0f;
    config.shape = 0.0f;
    config.drive = 0.0f;
    config.tone = 0.0f;
    config.outputGainDb = 0.0f;
    return config;
}
} // namespace

TEST_CASE("tone filter bypasses exactly at zero tilt")
{
    pulse::ToneFilter filter;
    filter.prepare(48000.0);
    filter.setTilt(0.0f);

    for (int sample = 0; sample < 256; ++sample)
    {
        const auto input = std::sin(0.07f * static_cast<float>(sample));
        REQUIRE(filter.processSample(input) == input);
    }
}

TEST_CASE("tone filter settles to a six decibel tilt at every sample rate")
{
    const auto darker = std::pow(10.0f, 6.0f / 20.0f);
    const auto brighter = std::pow(10.0f, -6.0f / 20.0f);

    for (const auto sampleRate : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        const auto sampleCount = static_cast<int>(sampleRate);

        pulse::ToneFilter bright;
        bright.prepare(sampleRate);
        bright.setTilt(1.0f);
        float brightOutput = 0.0f;
        for (int sample = 0; sample < sampleCount; ++sample)
            brightOutput = bright.processSample(1.0f);

        pulse::ToneFilter dark;
        dark.prepare(sampleRate);
        dark.setTilt(-1.0f);
        float darkOutput = 0.0f;
        for (int sample = 0; sample < sampleCount; ++sample)
            darkOutput = dark.processSample(1.0f);

        REQUIRE_THAT(brightOutput, WithinAbs(brighter, 1.0e-4f));
        REQUIRE_THAT(darkOutput, WithinAbs(darker, 1.0e-4f));
    }
}

TEST_CASE("positive tone brightens the voice and negative tone darkens it")
{
    const auto config = steadyToneConfig();
    auto brightConfig = config;
    auto darkConfig = config;
    brightConfig.tone = 1.0f;
    darkConfig.tone = -1.0f;

    const auto flat = pulse::renderOneShot(config, 48000.0, 4096);
    const auto bright = pulse::renderOneShot(brightConfig, 48000.0, 4096);
    const auto dark = pulse::renderOneShot(darkConfig, 48000.0, 4096);
    const auto flatLevel = windowRms(flat, 1000, 3000);

    REQUIRE(windowRms(bright, 1000, 3000) < flatLevel * 0.65f);
    REQUIRE(windowRms(dark, 1000, 3000) > flatLevel * 1.6f);

    auto highConfig = config;
    highConfig.pitchHz = 8000.0f;
    highConfig.ampDecayMs = 40.0f;
    auto highBright = highConfig;
    auto highDark = highConfig;
    highBright.tone = 1.0f;
    highDark.tone = -1.0f;
    const auto brightHigh = pulse::renderOneShot(highBright, 48000.0, 2048);
    const auto darkHigh = pulse::renderOneShot(highDark, 48000.0, 2048);

    REQUIRE(windowRms(brightHigh, 200, 1200) > windowRms(darkHigh, 200, 1200) * 1.5f);

    for (float sample : flat)
        REQUIRE(std::isfinite(sample));
    for (float sample : bright)
        REQUIRE(std::isfinite(sample));
    for (float sample : dark)
        REQUIRE(std::isfinite(sample));
}

TEST_CASE("tone filter does not add a direct-current offset to a sine")
{
    constexpr double sampleRate = 48000.0;
    constexpr int sampleCount = 48000;

    for (const auto tilt : { -1.0f, 1.0f })
    {
        pulse::ToneFilter filter;
        filter.prepare(sampleRate);
        filter.setTilt(tilt);

        double sum = 0.0;
        for (int sample = 0; sample < sampleCount; ++sample)
        {
            const auto phase = 2.0 * 3.141592653589793 * 100.0
                               * static_cast<double>(sample) / sampleRate;
            const auto output = filter.processSample(static_cast<float>(std::sin(phase)));
            sum += static_cast<double>(output);
        }

        REQUIRE(std::abs(sum / static_cast<double>(sampleCount)) < 1.0e-4);
    }
}

TEST_CASE("output gain is applied in decibels and clamped")
{
    const auto unity = pulse::renderOneShot(steadyToneConfig(), 48000.0, 128);
    auto boostedConfig = steadyToneConfig();
    boostedConfig.outputGainDb = 6.0f;
    const auto boosted = pulse::renderOneShot(boostedConfig, 48000.0, 128);

    REQUIRE_THAT(boosted.front() / unity.front(),
                 WithinAbs(std::pow(10.0f, 6.0f / 20.0f), 1.0e-5f));

    auto maximumConfig = steadyToneConfig();
    auto aboveMaximumConfig = maximumConfig;
    maximumConfig.outputGainDb = 12.0f;
    aboveMaximumConfig.outputGainDb = 24.0f;
    REQUIRE(pulse::renderOneShot(maximumConfig, 48000.0, 128)
            == pulse::renderOneShot(aboveMaximumConfig, 48000.0, 128));

    auto silentConfig = steadyToneConfig();
    silentConfig.outputGainDb = -96.0f;
    const auto silent = pulse::renderOneShot(silentConfig, 48000.0, 128);
    for (float sample : silent)
        REQUIRE(sample == 0.0f);
}

TEST_CASE("mono render keeps tone and gain but ignores pan")
{
    auto panned = steadyToneConfig();
    panned.tone = 0.5f;
    panned.outputGainDb = -3.0f;
    panned.pan = 1.0f;
    auto centred = panned;
    centred.pan = 0.0f;

    REQUIRE(pulse::renderOneShot(panned, 48000.0, 256)
            == pulse::renderOneShot(centred, 48000.0, 256));
}

TEST_CASE("stereo output uses equal-power pan")
{
    constexpr int sampleCount = 256;
    constexpr double sampleRate = 48000.0;
    const pulse::NoteEvent event { pulse::NoteEventType::noteOn, 0, 60, 1.0f };
    const auto config = steadyToneConfig();

    pulse::SynthEngine monoEngine;
    monoEngine.prepare(sampleRate, sampleCount);
    monoEngine.setConfig(config);
    std::vector<float> mono(static_cast<std::size_t>(sampleCount));
    monoEngine.processMono(&event, 1, mono.data(), sampleCount);

    auto renderStereo = [&](float pan) {
        auto panned = config;
        panned.pan = pan;
        pulse::SynthEngine engine;
        engine.prepare(sampleRate, sampleCount);
        engine.setConfig(panned);
        std::vector<float> left(static_cast<std::size_t>(sampleCount), 1.0f);
        std::vector<float> right(static_cast<std::size_t>(sampleCount), -1.0f);
        float* outputs[] = { left.data(), right.data() };
        engine.process(&event, 1, outputs, 2, sampleCount);
        return std::pair { std::move(left), std::move(right) };
    };

    const auto centre = renderStereo(0.0f);
    const auto leftSide = renderStereo(-1.0f);
    const auto rightSide = renderStereo(1.0f);
    const auto partial = renderStereo(0.35f);

    for (int sample = 0; sample < sampleCount; ++sample)
    {
        const auto index = static_cast<std::size_t>(sample);
        const auto monoSample = mono[index];
        REQUIRE(centre.first[index] == monoSample * 0.7071067811865475f);
        REQUIRE(centre.second[index] == centre.first[index]);
        REQUIRE(leftSide.first[index] == monoSample);
        REQUIRE(leftSide.second[index] == 0.0f);
        REQUIRE(rightSide.first[index] == 0.0f);
        REQUIRE(rightSide.second[index] == monoSample);

        const auto power = static_cast<double>(partial.first[index])
                               * static_cast<double>(partial.first[index])
                           + static_cast<double>(partial.second[index])
                                 * static_cast<double>(partial.second[index]);
        const auto monoPower = static_cast<double>(monoSample) * static_cast<double>(monoSample);
        REQUIRE_THAT(static_cast<float>(power),
                     WithinAbs(static_cast<float>(monoPower), 1.0e-5f));
    }

    REQUIRE(partial.second.front() > partial.first.front());
}

TEST_CASE("tone gain and pan stay latched until the next note")
{
    constexpr int totalSamples = 512;
    constexpr int blockSize = 128;
    auto config = steadyToneConfig();
    config.tone = -1.0f;
    config.outputGainDb = -6.0f;
    config.pan = -1.0f;
    const pulse::NoteEvent event { pulse::NoteEventType::noteOn, 0, 60, 1.0f };

    pulse::SynthEngine steady;
    steady.prepare(48000.0, totalSamples);
    steady.setConfig(config);
    std::vector<float> expectedLeft(static_cast<std::size_t>(totalSamples));
    std::vector<float> expectedRight(static_cast<std::size_t>(totalSamples));
    float* expectedOutputs[] = { expectedLeft.data(), expectedRight.data() };
    steady.process(&event, 1, expectedOutputs, 2, totalSamples);

    pulse::SynthEngine changed;
    changed.prepare(48000.0, blockSize);
    changed.setConfig(config);
    std::vector<float> left(static_cast<std::size_t>(totalSamples));
    std::vector<float> right(static_cast<std::size_t>(totalSamples));

    int rendered = 0;
    while (rendered < totalSamples)
    {
        if (rendered == blockSize)
        {
            config.tone = 1.0f;
            config.outputGainDb = 12.0f;
            config.pan = 1.0f;
            changed.setConfig(config);
        }

        const int samples = std::min(blockSize, totalSamples - rendered);
        float* outputs[] = { left.data() + rendered, right.data() + rendered };
        const pulse::NoteEvent* events = rendered == 0 ? &event : nullptr;
        const int numEvents = rendered == 0 ? 1 : 0;
        changed.process(events, numEvents, outputs, 2, samples);
        rendered += samples;
    }

    REQUIRE(left == expectedLeft);
    REQUIRE(right == expectedRight);
}

TEST_CASE("retrigger latches the pan of the new note")
{
    constexpr int blockSize = 64;
    auto config = steadyToneConfig();
    config.pan = -1.0f;
    const pulse::NoteEvent event { pulse::NoteEventType::noteOn, 0, 60, 1.0f };

    pulse::SynthEngine held;
    held.prepare(48000.0, blockSize);
    held.setConfig(config);
    std::vector<float> heldLeft(static_cast<std::size_t>(blockSize * 2));
    std::vector<float> heldRight(static_cast<std::size_t>(blockSize * 2));
    float* heldFirst[] = { heldLeft.data(), heldRight.data() };
    held.process(&event, 1, heldFirst, 2, blockSize);
    float* heldSecond[] = { heldLeft.data() + blockSize, heldRight.data() + blockSize };
    held.process(&event, 1, heldSecond, 2, blockSize);

    pulse::SynthEngine moved;
    moved.prepare(48000.0, blockSize);
    moved.setConfig(config);
    std::vector<float> movedLeft(static_cast<std::size_t>(blockSize * 2));
    std::vector<float> movedRight(static_cast<std::size_t>(blockSize * 2));
    float* movedFirst[] = { movedLeft.data(), movedRight.data() };
    moved.process(&event, 1, movedFirst, 2, blockSize);
    config.pan = 1.0f;
    moved.setConfig(config);
    float* movedSecond[] = { movedLeft.data() + blockSize, movedRight.data() + blockSize };
    moved.process(&event, 1, movedSecond, 2, blockSize);

    for (int sample = blockSize; sample < blockSize * 2; ++sample)
    {
        const auto index = static_cast<std::size_t>(sample);
        REQUIRE(movedLeft[index] == 0.0f);
        REQUIRE(movedRight[index] == heldLeft[index]);
        REQUIRE(heldRight[index] == 0.0f);
    }
}

TEST_CASE("tone gain and pan are independent of block size")
{
    constexpr double sampleRate = 48000.0;
    constexpr int totalSamples = 2048;
    constexpr int eventOffset = 41;
    auto config = steadyToneConfig();
    config.tone = 0.75f;
    config.outputGainDb = 3.0f;
    config.pan = -0.4f;

    pulse::SynthEngine oneBlock;
    oneBlock.prepare(sampleRate, totalSamples);
    oneBlock.setConfig(config);
    const pulse::NoteEvent event { pulse::NoteEventType::noteOn, eventOffset, 60, 1.0f };
    std::vector<float> expectedLeft(static_cast<std::size_t>(totalSamples));
    std::vector<float> expectedRight(static_cast<std::size_t>(totalSamples));
    float* expectedOutputs[] = { expectedLeft.data(), expectedRight.data() };
    oneBlock.process(&event, 1, expectedOutputs, 2, totalSamples);

    pulse::SynthEngine splitBlocks;
    splitBlocks.prepare(sampleRate, 64);
    splitBlocks.setConfig(config);
    std::vector<float> left(static_cast<std::size_t>(totalSamples));
    std::vector<float> right(static_cast<std::size_t>(totalSamples));

    int rendered = 0;
    while (rendered < totalSamples)
    {
        const int blockSize = std::min(64, totalSamples - rendered);
        float* outputs[] = { left.data() + rendered, right.data() + rendered };
        const bool firstBlock = rendered == 0;
        const pulse::NoteEvent* events = firstBlock ? &event : nullptr;
        const int numEvents = firstBlock ? 1 : 0;
        splitBlocks.process(events, numEvents, outputs, 2, blockSize);
        rendered += blockSize;
    }

    REQUIRE(left == expectedLeft);
    REQUIRE(right == expectedRight);
}

TEST_CASE("mix default is an equal linear sum of oscillator and noise")
{
    const auto config = linearMixConfig();
    REQUIRE(config.noiseMix == 0.5f);

    auto oscillatorOnly = config;
    auto noiseOnly = config;
    oscillatorOnly.noiseMix = 0.0f;
    noiseOnly.noiseMix = 1.0f;

    const auto mixed = pulse::renderOneShot(config, 48000.0, 256);
    const auto oscillator = pulse::renderOneShot(oscillatorOnly, 48000.0, 256);
    const auto noise = pulse::renderOneShot(noiseOnly, 48000.0, 256);

    REQUIRE(mixed != oscillator);
    REQUIRE(mixed != noise);
    for (std::size_t index = 0; index < mixed.size(); ++index)
    {
        const auto expected = 0.5f * (oscillator[index] + noise[index]);
        REQUIRE_THAT(mixed[index], WithinAbs(expected, 1.0e-5f));
    }
}

TEST_CASE("mix endpoints select one source")
{
    auto oscillatorOnly = linearMixConfig();
    oscillatorOnly.noiseMix = 0.0f;
    auto otherSeed = oscillatorOnly;
    otherSeed.noiseSeed = 99u;
    REQUIRE(pulse::renderOneShot(oscillatorOnly, 48000.0, 128)
            == pulse::renderOneShot(otherSeed, 48000.0, 128));

    auto noiseOnly = linearMixConfig();
    noiseOnly.noiseMix = 1.0f;
    auto otherPitch = noiseOnly;
    otherPitch.pitchHz = 440.0f;
    otherPitch.waveform = pulse::Waveform::square;
    REQUIRE(pulse::renderOneShot(noiseOnly, 48000.0, 128)
            == pulse::renderOneShot(otherPitch, 48000.0, 128));

    auto above = linearMixConfig();
    auto below = linearMixConfig();
    above.noiseMix = 4.0f;
    below.noiseMix = -2.0f;
    REQUIRE(pulse::renderOneShot(above, 48000.0, 128)
            == pulse::renderOneShot(noiseOnly, 48000.0, 128));
    REQUIRE(pulse::renderOneShot(below, 48000.0, 128)
            == pulse::renderOneShot(oscillatorOnly, 48000.0, 128));

    auto missing = linearMixConfig();
    missing.noiseMix = std::nanf("");
    REQUIRE(pulse::renderOneShot(missing, 48000.0, 64)
            == pulse::renderOneShot(linearMixConfig(), 48000.0, 64));
}

TEST_CASE("mix stays latched until the next note")
{
    constexpr int blockSize = 64;
    auto config = linearMixConfig();
    config.noiseMix = 0.0f;
    const pulse::NoteEvent event { pulse::NoteEventType::noteOn, 0, 60, 1.0f };

    pulse::SynthEngine steady;
    steady.prepare(48000.0, blockSize * 2);
    steady.setConfig(config);
    std::vector<float> expected(static_cast<std::size_t>(blockSize * 2));
    steady.processMono(&event, 1, expected.data(), blockSize * 2);

    pulse::SynthEngine changed;
    changed.prepare(48000.0, blockSize);
    changed.setConfig(config);
    std::vector<float> actual(static_cast<std::size_t>(blockSize * 2));
    changed.processMono(&event, 1, actual.data(), blockSize);
    config.noiseMix = 1.0f;
    changed.setConfig(config);
    changed.processMono(nullptr, 0, actual.data() + blockSize, blockSize);

    REQUIRE(actual == expected);
}

namespace
{
std::vector<float> renderEvents(const pulse::SynthConfig& config,
                                const std::vector<pulse::NoteEvent>& events,
                                int numSamples,
                                double sampleRate = 48000.0)
{
    pulse::SynthEngine engine;
    engine.prepare(sampleRate, numSamples);
    engine.setConfig(config);
    std::vector<float> rendered(static_cast<std::size_t>(numSamples));
    engine.processMono(events.data(),
                       static_cast<int>(events.size()),
                       rendered.data(),
                       numSamples);
    return rendered;
}

pulse::NoteEvent noteOnAt(int sampleOffset, int midiNote = 60, float velocity = 1.0f)
{
    return { pulse::NoteEventType::noteOn, sampleOffset, midiNote, velocity };
}

float peakBetween(const std::vector<float>& rendered, int first, int last)
{
    float peak = 0.0f;
    for (int sample = first; sample < last; ++sample)
        peak = std::max(peak, std::abs(rendered[static_cast<std::size_t>(sample)]));

    return peak;
}

float largestStep(const std::vector<float>& rendered, int first, int last)
{
    float step = 0.0f;
    for (int sample = std::max(first, 1); sample < last; ++sample)
    {
        const auto index = static_cast<std::size_t>(sample);
        step = std::max(step, std::abs(rendered[index] - rendered[index - 1]));
    }

    return step;
}
} // namespace

TEST_CASE("note filter accepts omni, a single note or an inclusive range")
{
    auto config = steadyToneConfig();
    constexpr int numSamples = 256;

    const auto hits = [&](int midiNote)
    {
        const auto rendered = renderEvents(config, { noteOnAt(0, midiNote) }, numSamples);
        return peakBetween(rendered, 0, numSamples) > 0.1f;
    };

    REQUIRE(hits(12));
    REQUIRE(hits(100));

    config.noteFilterMode = pulse::NoteFilterMode::note;
    config.noteFilterNote = 38;
    REQUIRE(hits(38));
    REQUIRE_FALSE(hits(37));
    REQUIRE_FALSE(hits(39));

    config.noteFilterMode = pulse::NoteFilterMode::range;
    config.noteFilterLow = 48;
    config.noteFilterHigh = 50;
    REQUIRE(hits(48));
    REQUIRE(hits(50));
    REQUIRE_FALSE(hits(47));
    REQUIRE_FALSE(hits(51));

    config.noteFilterLow = 50;
    config.noteFilterHigh = 48;
    REQUIRE(hits(49));
    REQUIRE_FALSE(hits(51));
}

TEST_CASE("note filter is read when the note arrives")
{
    auto config = steadyToneConfig();
    config.noteFilterMode = pulse::NoteFilterMode::note;
    config.noteFilterNote = 36;

    pulse::SynthEngine engine;
    engine.prepare(48000.0, 64);
    engine.setConfig(config);
    std::vector<float> rendered(64);
    const auto event = noteOnAt(0, 60);
    engine.processMono(&event, 1, rendered.data(), 64);
    REQUIRE(engine.busyVoiceCount() == 0);

    config.noteFilterNote = 60;
    engine.setConfig(config);
    engine.processMono(&event, 1, rendered.data(), 64);
    REQUIRE(engine.busyVoiceCount() == 1);
}

TEST_CASE("overlap mode sums independent voices")
{
    auto config = steadyToneConfig();
    config.voiceMode = pulse::VoiceMode::overlap;
    config.voices = 4;
    constexpr int numSamples = 9600;
    constexpr int second = 480;

    const auto both = renderEvents(config, { noteOnAt(0), noteOnAt(second) }, numSamples);
    const auto first = renderEvents(config, { noteOnAt(0) }, numSamples);
    const auto later = renderEvents(config, { noteOnAt(second) }, numSamples);

    for (std::size_t sample = 0; sample < both.size(); ++sample)
        REQUIRE_THAT(both[sample], WithinAbs(first[sample] + later[sample], 1.0e-6f));
}

TEST_CASE("restart mode uses one voice and overlap steals the oldest")
{
    auto config = steadyToneConfig();
    constexpr int blockSize = 64;
    std::vector<float> rendered(blockSize);

    pulse::SynthEngine restart;
    restart.prepare(48000.0, blockSize);
    restart.setConfig(config);
    for (int hit = 0; hit < 3; ++hit)
    {
        const auto event = noteOnAt(0);
        restart.processMono(&event, 1, rendered.data(), blockSize);
    }
    REQUIRE(restart.busyVoiceCount() == 1);

    config.voiceMode = pulse::VoiceMode::overlap;
    config.voices = 2;
    pulse::SynthEngine overlap;
    overlap.prepare(48000.0, blockSize);
    overlap.setConfig(config);
    for (int hit = 0; hit < 3; ++hit)
    {
        const auto event = noteOnAt(0);
        overlap.processMono(&event, 1, rendered.data(), blockSize);
        REQUIRE(overlap.busyVoiceCount() == std::min(hit + 1, 2));
    }

    config.voices = 8;
    pulse::SynthEngine wide;
    wide.prepare(48000.0, blockSize);
    wide.setConfig(config);
    for (int hit = 0; hit < 10; ++hit)
    {
        const auto event = noteOnAt(0);
        wide.processMono(&event, 1, rendered.data(), blockSize);
    }
    REQUIRE(wide.busyVoiceCount() == pulse::kMaxVoices);
}

TEST_CASE("finished voices are freed after their tail")
{
    auto config = steadyToneConfig();
    config.voiceMode = pulse::VoiceMode::overlap;
    config.voices = 8;
    config.ampDecayMs = 50.0f;
    config.drive = 1.0f;
    config.driveType = pulse::DriveType::asymmetric;

    pulse::SynthEngine engine;
    engine.prepare(48000.0, 4800);
    engine.setConfig(config);
    std::vector<float> rendered(4800);
    const std::vector<pulse::NoteEvent> events { noteOnAt(0), noteOnAt(100), noteOnAt(200) };
    engine.processMono(events.data(), 3, rendered.data(), 4800);
    REQUIRE(engine.busyVoiceCount() == 3);

    for (int block = 0; block < 20 && engine.busyVoiceCount() > 0; ++block)
        engine.processMono(nullptr, 0, rendered.data(), 4800);

    REQUIRE(engine.busyVoiceCount() == 0);
}

TEST_CASE("a restarted hit returns to full level with its pitch sweep")
{
    pulse::SynthConfig config;
    config.noiseMix = 0.0f;
    constexpr int numSamples = 48000;

    for (const int gapMs : { 20, 100, 250 })
    {
        const int gap = gapMs * 48;
        const auto single = renderEvents(config, { noteOnAt(0) }, numSamples);
        const auto twice = renderEvents(config, { noteOnAt(0), noteOnAt(gap) }, numSamples);

        const auto firstPeak = peakBetween(single, 0, 480);
        const auto retriggerPeak = peakBetween(twice, gap, gap + 480);
        REQUIRE(retriggerPeak > 0.9f * firstPeak);
    }
}

TEST_CASE("retrigger after 5 ms does not click")
{
    // A click is a step larger than the hit's own onset from silence. Retriggers
    // keep oscillator phase and filter state, so only envelopes move.
    for (const auto waveform : { pulse::Waveform::sine, pulse::Waveform::triangle })
    {
        for (const double sampleRate : { 44100.0, 48000.0, 96000.0, 192000.0 })
        {
            pulse::SynthConfig config;
            config.waveform = waveform;
            config.startPhaseDegrees = 0.0f;
            config.ampAttackMs = 1.0f;
            config.noiseAmpAttackMs = 1.0f;
            const int samplesPerMs = static_cast<int>(sampleRate / 1000.0);
            const int numSamples = 100 * samplesPerMs;

            std::vector<pulse::NoteEvent> events;
            for (int hit = 0; hit < 10; ++hit)
                events.push_back(noteOnAt(hit * 5 * samplesPerMs));

            const auto single = renderEvents(config, { noteOnAt(0) }, numSamples, sampleRate);
            const auto roll = renderEvents(config, events, numSamples, sampleRate);
            const auto singleStep = largestStep(single, 0, 5 * samplesPerMs);

            for (int hit = 1; hit < 10; ++hit)
            {
                const int onset = hit * 5 * samplesPerMs;
                REQUIRE(largestStep(roll, onset - 4, onset + 4) <= 1.5f * singleStep);
            }

            for (float sample : roll)
                REQUIRE(std::isfinite(sample));
        }
    }
}

TEST_CASE("overlapping MIDI is independent of block size")
{
    auto config = steadyToneConfig();
    config.voiceMode = pulse::VoiceMode::overlap;
    config.voices = 3;
    config.noiseMix = 0.5f;
    config.noiseBursts = 3;
    config.drive = 0.6f;
    config.pan = 0.3f;
    constexpr int totalSamples = 8192;
    const std::vector<pulse::NoteEvent> events {
        noteOnAt(31, 60, 1.0f), noteOnAt(31, 64, 0.5f), noteOnAt(700, 48, 0.8f),
        noteOnAt(701, 72, 0.3f), noteOnAt(2049, 60, 0.9f), noteOnAt(4095, 55, 1.0f)
    };

    const auto renderInBlocks = [&](int blockSize)
    {
        pulse::SynthEngine engine;
        engine.prepare(48000.0, blockSize);
        engine.setConfig(config);
        std::vector<float> left(totalSamples);
        std::vector<float> right(totalSamples);
        std::vector<pulse::NoteEvent> blockEvents;

        for (int start = 0; start < totalSamples; start += blockSize)
        {
            const int samples = std::min(blockSize, totalSamples - start);
            blockEvents.clear();
            for (auto event : events)
            {
                if (event.sampleOffset >= start && event.sampleOffset < start + samples)
                {
                    event.sampleOffset -= start;
                    blockEvents.push_back(event);
                }
            }

            float* outputs[] = { left.data() + start, right.data() + start };
            engine.process(blockEvents.data(),
                           static_cast<int>(blockEvents.size()),
                           outputs,
                           2,
                           samples);
        }

        return std::pair { left, right };
    };

    const auto expected = renderInBlocks(totalSamples);
    for (const int blockSize : { 32, 64, 512, 2048 })
        REQUIRE(renderInBlocks(blockSize) == expected);
}
