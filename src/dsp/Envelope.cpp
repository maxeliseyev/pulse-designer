#include "Envelope.h"

#include <algorithm>
#include <cmath>

namespace pulse
{

namespace
{
constexpr float kEnvelopeEndLevel = 0.001f;
constexpr float kExponentialShape = 8.0f;

int samplesForMilliseconds(float milliseconds, double sampleRate) noexcept
{
    if (milliseconds <= 0.0f || sampleRate <= 0.0)
        return 0;

    return std::max(1, static_cast<int>(std::lround(
                         0.001 * static_cast<double>(milliseconds) * sampleRate)));
}
} // namespace

void ExponentialEnvelope::prepare(double sampleRate) noexcept
{
    currentSampleRate = std::max(sampleRate, 1.0);
    reset();
}

void ExponentialEnvelope::reset(float newValue) noexcept
{
    stage = Stage::idle;
    currentValue = std::clamp(newValue, 0.0f, 1.0f);
    startValue = currentValue;
    decayStartValue = currentValue;
    curve = 1.0f;
    stagePosition = 0;
    stageLength = 0;
    decayLength = 0;
    active = false;
}

void ExponentialEnvelope::start(float newCurrentValue,
                                float attackMs,
                                float decayMs,
                                float newCurve) noexcept
{
    currentValue = std::clamp(newCurrentValue, 0.0f, 1.0f);
    startValue = currentValue;
    decayStartValue = currentValue > 0.0f ? currentValue : 1.0f;
    curve = std::clamp(newCurve, 0.0f, 1.0f);
    stagePosition = 0;
    stageLength = samplesForMilliseconds(attackMs, currentSampleRate);
    decayLength = samplesForMilliseconds(decayMs, currentSampleRate);
    active = true;

    if (stageLength > 0)
    {
        stage = Stage::attack;
        return;
    }

    currentValue = decayStartValue;
    startValue = currentValue;
    stagePosition = 0;
    stageLength = decayLength;
    stage = stageLength > 0 ? Stage::decay : Stage::idle;
    active = stage != Stage::idle;
}

float ExponentialEnvelope::shapedProgress(float progress, float newCurve) noexcept
{
    const float linear = std::clamp(progress, 0.0f, 1.0f);
    const float exponential =
        (1.0f - std::exp(-kExponentialShape * linear))
        / (1.0f - std::exp(-kExponentialShape));
    return linear + (exponential - linear) * std::clamp(newCurve, 0.0f, 1.0f);
}

float ExponentialEnvelope::processSample() noexcept
{
    if (!active)
        return currentValue;

    if (stage == Stage::attack)
    {
        const float progress = static_cast<float>(stagePosition)
                               / static_cast<float>(stageLength);
        const float shaped = shapedProgress(progress, curve);
        currentValue = startValue + (1.0f - startValue) * shaped;
        ++stagePosition;

        if (stagePosition >= stageLength)
        {
            currentValue = 1.0f;
            decayStartValue = currentValue;
            stage = Stage::decay;
            stagePosition = 0;
            stageLength = decayLength;
            if (stageLength <= 0)
            {
                currentValue = 0.0f;
                stage = Stage::idle;
                active = false;
            }
        }

        return currentValue;
    }

    if (stage == Stage::decay)
    {
        const float progress = static_cast<float>(stagePosition)
                               / static_cast<float>(stageLength);
        const float shaped = shapedProgress(progress, curve);
        currentValue = decayStartValue
                       + (kEnvelopeEndLevel - decayStartValue) * shaped;
        ++stagePosition;

        if (stagePosition >= stageLength)
        {
            currentValue = 0.0f;
            stage = Stage::idle;
            active = false;
        }

        return currentValue;
    }

    active = false;
    currentValue = 0.0f;
    return currentValue;
}

} // namespace pulse
