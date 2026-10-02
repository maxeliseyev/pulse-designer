#include <catch2/catch_test_macros.hpp>

#include "dsp/NonlinearStage.h"
#include "dsp/OneShotRenderer.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr int kFftSize = 16384;
constexpr double kTargetFundamentalHz = 55.0;
// Fixed in docs/decisions/0005-aliasing-dc-gate.md before treating the result
// as a pass. -60 dB is the bar for a noticeable partial against the fundamental.
constexpr double kAliasThresholdDb = -60.0;
constexpr double kBareOversamplingDb = -65.0;
constexpr double kCleanSineThresholdDb = -90.0;
constexpr double kDcMeanThreshold = 1.0e-3;

const char* driveName(pulse::DriveType type)
{
    switch (type)
    {
        case pulse::DriveType::soft:
            return "soft";
        case pulse::DriveType::hard:
            return "hard";
        case pulse::DriveType::asymmetric:
            return "asymmetric";
        case pulse::DriveType::fold:
            return "fold";
    }

    return "unknown";
}

int fundamentalBin(double sampleRate)
{
    const auto bin = static_cast<int>(std::lround(
        kTargetFundamentalHz * static_cast<double>(kFftSize) / sampleRate));
    return std::max(bin, 2);
}

void forwardFft(std::vector<double>& real, std::vector<double>& imag)
{
    const int size = static_cast<int>(real.size());
    for (int index = 1, reversed = 0; index < size; ++index)
    {
        int bit = size >> 1;
        for (; (reversed & bit) != 0; bit >>= 1)
            reversed ^= bit;

        reversed ^= bit;
        if (index < reversed)
        {
            std::swap(real[static_cast<std::size_t>(index)],
                      real[static_cast<std::size_t>(reversed)]);
            std::swap(imag[static_cast<std::size_t>(index)],
                      imag[static_cast<std::size_t>(reversed)]);
        }
    }

    for (int length = 2; length <= size; length <<= 1)
    {
        const double theta = -2.0 * kPi / static_cast<double>(length);
        const double stepReal = std::cos(theta);
        const double stepImag = std::sin(theta);
        for (int start = 0; start < size; start += length)
        {
            double turnReal = 1.0;
            double turnImag = 0.0;
            const int half = length / 2;
            for (int offset = 0; offset < half; ++offset)
            {
                const int evenIndex = start + offset;
                const int oddIndex = evenIndex + half;
                const auto even = static_cast<std::size_t>(evenIndex);
                const auto odd = static_cast<std::size_t>(oddIndex);
                const double twistedReal = real[odd] * turnReal - imag[odd] * turnImag;
                const double twistedImag = real[odd] * turnImag + imag[odd] * turnReal;
                real[odd] = real[even] - twistedReal;
                imag[odd] = imag[even] - twistedImag;
                real[even] += twistedReal;
                imag[even] += twistedImag;

                const double nextReal = turnReal * stepReal - turnImag * stepImag;
                turnImag = turnReal * stepImag + turnImag * stepReal;
                turnReal = nextReal;
            }
        }
    }
}

struct AliasMeasurement
{
    double fundamentalPeakDb = -200.0;
    double belowFundamentalDb = -200.0;
    double below5kHzDb = -200.0;
};

AliasMeasurement measureDrive(pulse::DriveType type,
                              float shapeAmount,
                              int oversampling,
                              double sampleRate)
{
    const int bin = fundamentalBin(sampleRate);
    const double cyclesPerSample = static_cast<double>(bin) / static_cast<double>(kFftSize);

    pulse::NonlinearStage shape(pulse::NonlinearStage::Kind::shape);
    pulse::NonlinearStage drive(pulse::NonlinearStage::Kind::drive);
    shape.setAmount(shapeAmount);
    shape.setOversampling(oversampling);
    drive.setAmount(1.0f);
    drive.setDriveType(type);
    drive.setOversampling(oversampling);
    shape.reset();
    drive.reset();

    const auto process = [&](double phase) {
        const float input = static_cast<float>(std::sin(2.0 * kPi * phase));
        return static_cast<double>(drive.processSample(shape.processSample(input)));
    };

    double phase = 0.0;
    for (int sample = 0; sample < kFftSize; ++sample)
    {
        process(phase);
        phase += cyclesPerSample;
    }

    std::vector<double> real(static_cast<std::size_t>(kFftSize));
    std::vector<double> imag(static_cast<std::size_t>(kFftSize), 0.0);
    phase = 0.0;
    for (int sample = 0; sample < kFftSize; ++sample)
    {
        real[static_cast<std::size_t>(sample)] = process(phase);
        phase += cyclesPerSample;
    }

    forwardFft(real, imag);

    const auto power = [&](int index) {
        const auto binIndex = static_cast<std::size_t>(index);
        return real[binIndex] * real[binIndex] + imag[binIndex] * imag[binIndex];
    };

    const double fundamentalPower = power(bin);
    double belowPower = 0.0;
    for (int index = 1; index < bin; ++index)
        belowPower = std::max(belowPower, power(index));

    double below5kHzPower = 0.0;
    const int nyquist = kFftSize / 2;
    for (int index = 1; index < nyquist; ++index)
    {
        if (index % bin == 0)
            continue;

        const double hz = static_cast<double>(index) * sampleRate
                          / static_cast<double>(kFftSize);
        if (hz < 5000.0)
            below5kHzPower = std::max(below5kHzPower, power(index));
    }

    const auto relativeDb = [fundamentalPower](double partialPower) {
        if (!(fundamentalPower > 0.0) || !(partialPower > 0.0))
            return -200.0;

        return 10.0 * std::log10(partialPower / fundamentalPower);
    };

    const double peak = 2.0 * std::sqrt(std::max(fundamentalPower, 0.0))
                        / static_cast<double>(kFftSize);
    AliasMeasurement measurement;
    measurement.fundamentalPeakDb = 20.0 * std::log10(std::max(peak, 1.0e-20));
    measurement.belowFundamentalDb = relativeDb(belowPower);
    measurement.below5kHzDb = relativeDb(below5kHzPower);
    return measurement;
}

double tailMean(const std::vector<float>& rendered, double sampleRate)
{
    const int tail = std::max(1, static_cast<int>(sampleRate * 0.1));
    const int start = static_cast<int>(rendered.size()) - tail;
    double sum = 0.0;
    for (int index = start; index < static_cast<int>(rendered.size()); ++index)
        sum += static_cast<double>(rendered[static_cast<std::size_t>(index)]);

    return sum / static_cast<double>(tail);
}
} // namespace

TEST_CASE("a clean sine has no energy below its fundamental")
{
    pulse::NonlinearStage shape(pulse::NonlinearStage::Kind::shape);
    pulse::NonlinearStage drive(pulse::NonlinearStage::Kind::drive);
    shape.setAmount(0.0f);
    drive.setAmount(0.0f);
    shape.setOversampling(4);
    drive.setOversampling(4);

    constexpr double sampleRate = 44100.0;
    const int bin = fundamentalBin(sampleRate);
    const double cyclesPerSample = static_cast<double>(bin) / static_cast<double>(kFftSize);
    std::vector<double> real(static_cast<std::size_t>(kFftSize));
    std::vector<double> imag(static_cast<std::size_t>(kFftSize), 0.0);
    double phase = 0.0;
    for (int sample = 0; sample < kFftSize; ++sample)
    {
        const float input = static_cast<float>(std::sin(2.0 * kPi * phase));
        real[static_cast<std::size_t>(sample)] = drive.processSample(shape.processSample(input));
        phase += cyclesPerSample;
    }

    forwardFft(real, imag);
    const auto power = [&](int index) {
        const auto binIndex = static_cast<std::size_t>(index);
        return real[binIndex] * real[binIndex] + imag[binIndex] * imag[binIndex];
    };

    double belowPower = 0.0;
    for (int index = 1; index < bin; ++index)
        belowPower = std::max(belowPower, power(index));

    const double relativeDb = 10.0 * std::log10(belowPower / power(bin));
    REQUIRE(relativeDb <= kCleanSineThresholdDb);
}

TEST_CASE("default oversampling keeps maximum drive within the aliasing gate")
{
    for (const double sampleRate : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        for (const int oversampling : { 4, 8 })
        {
            for (const float shapeAmount : { 0.0f, 1.0f })
            {
                for (const auto type : { pulse::DriveType::soft,
                                         pulse::DriveType::hard,
                                         pulse::DriveType::asymmetric,
                                         pulse::DriveType::fold })
                {
                    const auto measurement = measureDrive(type,
                                                          shapeAmount,
                                                          oversampling,
                                                          sampleRate);
                    CAPTURE(sampleRate,
                            oversampling,
                            shapeAmount,
                            driveName(type),
                            measurement.fundamentalPeakDb,
                            measurement.belowFundamentalDb,
                            measurement.below5kHzDb);
                    REQUIRE(measurement.fundamentalPeakDb > -20.0);
                    REQUIRE(measurement.belowFundamentalDb <= kAliasThresholdDb);
                    REQUIRE(measurement.below5kHzDb <= kAliasThresholdDb);
                }
            }
        }
    }
}

TEST_CASE("1x hard drive aliases a low sine below its fundamental")
{
    const auto measurement = measureDrive(pulse::DriveType::hard, 1.0f, 1, 44100.0);
    REQUIRE(measurement.fundamentalPeakDb > -20.0);
    REQUIRE(measurement.belowFundamentalDb > kBareOversamplingDb);
    REQUIRE(measurement.below5kHzDb > kBareOversamplingDb);
}

TEST_CASE("maximum drive one-shots settle to a near zero tail")
{
    for (const double sampleRate : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        for (const auto type : { pulse::DriveType::soft,
                                 pulse::DriveType::hard,
                                 pulse::DriveType::asymmetric,
                                 pulse::DriveType::fold })
        {
            for (const float decayMs : { 30.0f, 200.0f })
            {
                pulse::SynthConfig config;
                config.noiseMix = 0.0f;
                config.pitchEnvelopeAmountSemitones = 0.0f;
                config.pitchHz = 55.0f;
                config.shape = 1.0f;
                config.drive = 1.0f;
                config.driveType = type;
                config.oversampling = 4;
                config.ampDecayMs = decayMs;
                config.tone = 0.0f;
                config.outputGainDb = 0.0f;

                const auto rendered = pulse::renderOneShot(
                    config,
                    sampleRate,
                    static_cast<int>(sampleRate));
                for (float sample : rendered)
                    REQUIRE(std::isfinite(sample));

                const double mean = tailMean(rendered, sampleRate);
                CAPTURE(sampleRate, driveName(type), decayMs, mean);
                REQUIRE(std::abs(mean) < kDcMeanThreshold);
            }
        }
    }

    for (const double sampleRate : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        pulse::SynthConfig config;
        config.noiseMix = 1.0f;
        config.pitchEnvelopeAmountSemitones = 0.0f;
        config.shape = 1.0f;
        config.drive = 1.0f;
        config.driveType = pulse::DriveType::asymmetric;
        config.oversampling = 4;
        config.noiseAmpDecayMs = 40.0f;
        config.ampDecayMs = 40.0f;

        const auto rendered = pulse::renderOneShot(
            config,
            sampleRate,
            static_cast<int>(sampleRate));
        for (float sample : rendered)
            REQUIRE(std::isfinite(sample));

        const double mean = tailMean(rendered, sampleRate);
        CAPTURE(sampleRate, mean);
        REQUIRE(std::abs(mean) < kDcMeanThreshold);
    }
}
