#pragma once

#include "SynthParameters.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace pulse
{

class NoiseGenerator final
{
public:
    static constexpr std::uint32_t kDefaultSeed = 0x9e3779b9u;
    static constexpr std::size_t kMetallicOscillatorCount = 6;

    void prepare(double sampleRate) noexcept;
    void reset(std::uint32_t seed = kDefaultSeed) noexcept;

    void setType(NoiseType newType) noexcept { type = newType; }
    void setSampleAndHoldRate(float rateHz) noexcept;

    float processSample() noexcept;

private:
    void updateSampleAndHoldPeriod() noexcept;
    void updateMetallicIncrements() noexcept;
    float nextWhite() noexcept;

    double currentSampleRate = 48000.0;
    NoiseType type = NoiseType::white;
    std::uint32_t randomState = kDefaultSeed;
    float sampleAndHoldRateHz = 800.0f;
    int sampleAndHoldPeriod = 60;
    int sampleAndHoldCounter = 0;
    float heldValue = 0.0f;
    std::array<float, 7> pinkState {};
    std::array<float, kMetallicOscillatorCount> metallicPhase {};
    std::array<float, kMetallicOscillatorCount> metallicIncrement {};
};

} // namespace pulse
