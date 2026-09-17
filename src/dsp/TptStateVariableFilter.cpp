#include "TptStateVariableFilter.h"

#include <algorithm>
#include <cmath>

namespace pulse
{

namespace
{
constexpr float kPi = 3.14159265358979323846f;
constexpr float kMinimumCutoffHz = 1.0f;
constexpr float kMinimumResonance = 0.05f;
}

void TptStateVariableFilter::prepare(double sampleRate) noexcept
{
    currentSampleRate = std::max(sampleRate, 1.0);
    updateCoefficients();
    reset();
}

void TptStateVariableFilter::reset() noexcept
{
    s1 = 0.0f;
    s2 = 0.0f;
}

void TptStateVariableFilter::setParameters(float newCutoffHz,
                                            float newResonance,
                                            float newMorph) noexcept
{
    const auto nyquist = static_cast<float>(currentSampleRate * 0.49);
    cutoffHz = std::clamp(newCutoffHz, kMinimumCutoffHz, std::max(nyquist, kMinimumCutoffHz));
    resonance = std::clamp(newResonance, kMinimumResonance, 1.0f);
    morph = std::clamp(newMorph, 0.0f, 1.0f);
    updateCoefficients();
}

void TptStateVariableFilter::updateCoefficients() noexcept
{
    const auto sampleRate = static_cast<float>(currentSampleRate);
    const auto nyquist = std::max(sampleRate * 0.49f, kMinimumCutoffHz);
    const auto safeCutoff = std::clamp(cutoffHz, kMinimumCutoffHz, nyquist);
    g = std::tan(kPi * safeCutoff / sampleRate);
    r2 = 1.0f / std::max(resonance, kMinimumResonance);
    h = 1.0f / (1.0f + r2 * g + g * g);
}

FilterOutputs TptStateVariableFilter::processSample(float input) noexcept
{
    const auto high = h * (input - s1 * (g + r2) - s2);
    const auto band = high * g + s1;
    s1 = high * g + band;
    const auto low = band * g + s2;
    s2 = band * g + low;

    if (morph <= 0.5f)
    {
        const auto amount = morph * 2.0f;
        return { low, band, high, low + (band - low) * amount };
    }

    const auto amount = (morph - 0.5f) * 2.0f;
    return { low, band, high, band + (high - band) * amount };
}

} // namespace pulse
