#include "NoiseGenerator.h"

#include <algorithm>
#include <cmath>

namespace pulse
{

namespace
{
constexpr std::array<float, NoiseGenerator::kMetallicOscillatorCount>
    kMetallicFrequenciesHz { 863.0f, 1171.0f, 1439.0f, 1973.0f, 2671.0f, 3539.0f };

constexpr float kUint32Scale = 1.0f / 4294967295.0f;
}

void NoiseGenerator::prepare(double sampleRate) noexcept
{
    currentSampleRate = std::max(sampleRate, 1.0);
    updateSampleAndHoldPeriod();
    updateMetallicIncrements();
    reset();
}

void NoiseGenerator::reset(std::uint32_t seed) noexcept
{
    randomState = seed == 0u ? kDefaultSeed : seed;
    sampleAndHoldCounter = 0;
    heldValue = 0.0f;
    pinkState.fill(0.0f);
    metallicPhase.fill(0.0f);
}

void NoiseGenerator::setSampleAndHoldRate(float rateHz) noexcept
{
    sampleAndHoldRateHz = std::max(rateHz, 1.0f);
    updateSampleAndHoldPeriod();
    sampleAndHoldCounter = 0;
}

void NoiseGenerator::updateSampleAndHoldPeriod() noexcept
{
    const auto period = static_cast<int>(std::lround(
        currentSampleRate / static_cast<double>(std::max(sampleAndHoldRateHz, 1.0f))));
    sampleAndHoldPeriod = std::max(period, 1);
}

void NoiseGenerator::updateMetallicIncrements() noexcept
{
    const auto sampleRate = static_cast<float>(currentSampleRate);
    for (std::size_t index = 0; index < metallicIncrement.size(); ++index)
    {
        const auto frequency = std::clamp(kMetallicFrequenciesHz[index],
                                          0.0f,
                                          0.49f * sampleRate);
        metallicIncrement[index] = frequency / sampleRate;
    }
}

float NoiseGenerator::nextWhite() noexcept
{
    randomState ^= randomState << 13;
    randomState ^= randomState >> 17;
    randomState ^= randomState << 5;
    return 2.0f * static_cast<float>(randomState) * kUint32Scale - 1.0f;
}

float NoiseGenerator::processSample() noexcept
{
    switch (type)
    {
        case NoiseType::white:
            return nextWhite();

        case NoiseType::pink:
        {
            const auto white = nextWhite();
            pinkState[0] = 0.99886f * pinkState[0] + white * 0.0555179f;
            pinkState[1] = 0.99332f * pinkState[1] + white * 0.0750759f;
            pinkState[2] = 0.96900f * pinkState[2] + white * 0.1538520f;
            pinkState[3] = 0.86650f * pinkState[3] + white * 0.3104856f;
            pinkState[4] = 0.55000f * pinkState[4] + white * 0.5329522f;
            pinkState[5] = -0.7616f * pinkState[5] - white * 0.0168980f;
            pinkState[6] = white * 0.115926f;
            const auto pink = pinkState[0] + pinkState[1] + pinkState[2]
                              + pinkState[3] + pinkState[4] + pinkState[5]
                              + pinkState[6] + white * 0.5362f;
            return 0.11f * pink;
        }

        case NoiseType::metallic:
        {
            float output = 0.0f;
            for (std::size_t index = 0; index < metallicPhase.size(); ++index)
            {
                const auto phase = metallicPhase[index];
                output += phase < 0.5f ? 1.0f : -1.0f;
                metallicPhase[index] += metallicIncrement[index];
                metallicPhase[index] -= std::floor(metallicPhase[index]);
            }
            return output / static_cast<float>(metallicPhase.size());
        }

        case NoiseType::sampleAndHold:
            if (sampleAndHoldCounter <= 0)
            {
                heldValue = nextWhite();
                sampleAndHoldCounter = sampleAndHoldPeriod;
            }

            --sampleAndHoldCounter;
            return heldValue;
    }

    return 0.0f;
}

} // namespace pulse
