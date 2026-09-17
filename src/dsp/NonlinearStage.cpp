#include "NonlinearStage.h"

#include <algorithm>
#include <cmath>

namespace pulse
{

namespace
{
constexpr float kFoldPeriod = 4.0f;

float normalizedTanh(float input, float gain) noexcept
{
    const auto denominator = std::tanh(gain);
    return denominator > 0.0f ? std::tanh(input * gain) / denominator : input;
}
} // namespace

void NonlinearStage::reset() noexcept
{
    previousInput = 0.0f;
}

void NonlinearStage::setAmount(float newAmount) noexcept
{
    amount = std::clamp(newAmount, 0.0f, 1.0f);
}

void NonlinearStage::setOversampling(int factor) noexcept
{
    oversampling = normalizedOversampling(factor);
}

int NonlinearStage::normalizedOversampling(int factor) noexcept
{
    if (factor <= 1)
        return 1;
    if (factor <= 2)
        return 2;
    if (factor <= 4)
        return 4;
    return 8;
}

float NonlinearStage::foldSample(float input) noexcept
{
    auto folded = std::fmod(input + 1.0f, kFoldPeriod);
    if (folded < 0.0f)
        folded += kFoldPeriod;

    folded = folded <= 2.0f ? folded : kFoldPeriod - folded;
    return folded - 1.0f;
}

float NonlinearStage::processNonlinear(float input) const noexcept
{
    if (amount <= 0.0f)
        return input;

    if (kind == Kind::shape)
    {
        const auto shaped = std::tanh(3.0f * input);
        return input + amount * (shaped - input);
    }

    const auto preGain = 1.0f + 24.0f * amount;
    switch (driveType)
    {
        case DriveType::soft:
            return normalizedTanh(input, preGain);

        case DriveType::hard:
        {
            const auto threshold = std::max(0.05f, 1.0f - 0.95f * amount);
            return std::clamp(input * preGain, -threshold, threshold) / threshold;
        }

        case DriveType::asymmetric:
        {
            const auto bias = 0.35f * amount;
            const auto asymmetric = std::tanh(preGain * (input + bias));
            return asymmetric / std::tanh(preGain);
        }

        case DriveType::fold:
            return foldSample(input * (1.0f + 8.0f * amount));
    }

    return input;
}

float NonlinearStage::processSample(float input) noexcept
{
    if (amount <= 0.0f || oversampling == 1)
    {
        previousInput = input;
        return processNonlinear(input);
    }

    float output = 0.0f;
    for (int step = 1; step <= oversampling; ++step)
    {
        const auto interpolation = static_cast<float>(step)
                                   / static_cast<float>(oversampling);
        const auto oversampledInput = previousInput
                                      + (input - previousInput) * interpolation;
        output += processNonlinear(oversampledInput);
    }

    previousInput = input;
    return output / static_cast<float>(oversampling);
}

} // namespace pulse
