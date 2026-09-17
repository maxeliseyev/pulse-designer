#include "Oscillator.h"

#include <algorithm>
#include <cmath>

namespace pulse
{

void Oscillator::prepare(double sampleRate) noexcept
{
    currentSampleRate = std::max(sampleRate, 1.0);
    reset();
}

void Oscillator::reset() noexcept
{
    phase = 0.0f;
    phaseIncrement = 0.0f;
    waveform = Waveform::sine;
}

void Oscillator::start(float frequencyHz,
                       float phaseDegrees,
                       Waveform newWaveform) noexcept
{
    const float sampleRate = static_cast<float>(currentSampleRate);
    const float clampedFrequency = std::clamp(frequencyHz, 0.0f, 0.49f * sampleRate);
    const float normalizedPhase = phaseDegrees / 360.0f;

    phase = normalizedPhase - std::floor(normalizedPhase);
    phaseIncrement = clampedFrequency / sampleRate;
    waveform = newWaveform;
}

float Oscillator::processSample() noexcept
{
    const float currentPhase = phase;
    phase += phaseIncrement;
    phase -= std::floor(phase);

    switch (waveform)
    {
        case Waveform::sine:
            return std::sin(kTwoPi * currentPhase);

        case Waveform::triangle:
            return 1.0f - 4.0f * std::abs(currentPhase - 0.5f);

        case Waveform::square:
            return currentPhase < 0.5f ? 1.0f : -1.0f;
    }

    return 0.0f;
}

} // namespace pulse
