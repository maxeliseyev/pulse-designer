#include "DcBlocker.h"

#include <algorithm>
#include <cmath>

namespace pulse
{

namespace
{
constexpr float kCutoffHz = 10.0f;
constexpr float kTwoPi = 6.28318530717958647692f;
}

void DcBlocker::prepare(double sampleRate) noexcept
{
    currentSampleRate = std::max(sampleRate, 1.0);
    coefficient = std::exp(-kTwoPi * kCutoffHz / static_cast<float>(currentSampleRate));
    reset();
}

void DcBlocker::reset() noexcept
{
    previousInput = 0.0f;
    previousOutput = 0.0f;
}

float DcBlocker::processSample(float input) noexcept
{
    const auto output = input - previousInput + coefficient * previousOutput;
    previousInput = input;
    previousOutput = output;
    return output;
}

} // namespace pulse
