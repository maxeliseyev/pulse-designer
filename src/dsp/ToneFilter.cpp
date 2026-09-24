#include "ToneFilter.h"

#include <algorithm>
#include <cmath>

namespace pulse
{

namespace
{
constexpr float kPivotHz = 1000.0f;
constexpr float kMaxTiltDb = 6.0f;
constexpr float kTwoPi = 6.28318530717958647692f;
}

void ToneFilter::prepare(double sampleRate) noexcept
{
    currentSampleRate = std::max(sampleRate, 1.0);
    updateCoefficients();
    reset();
}

void ToneFilter::reset() noexcept
{
    low = 0.0f;
}

void ToneFilter::setTilt(float newTilt) noexcept
{
    tilt = std::isfinite(newTilt) ? std::clamp(newTilt, -1.0f, 1.0f) : 0.0f;
    updateCoefficients();
}

void ToneFilter::updateCoefficients() noexcept
{
    const auto tiltDb = tilt * kMaxTiltDb;
    lowGain = std::pow(10.0f, -tiltDb / 20.0f);
    highGain = std::pow(10.0f, tiltDb / 20.0f);

    const auto radians = kTwoPi * kPivotHz / static_cast<float>(currentSampleRate);
    coefficient = 1.0f - std::exp(-radians);
}

float ToneFilter::processSample(float input) noexcept
{
    low += coefficient * (input - low);

    // Unity tilt stays bit-exact: the one-pole state still advances.
    if (tilt == 0.0f)
        return input;

    return low * lowGain + (input - low) * highGain;
}

} // namespace pulse
