#pragma once

#include "SynthParameters.h"

namespace pulse
{

class Oscillator final
{
public:
    void prepare(double sampleRate) noexcept;
    void reset() noexcept;
    void start(float frequencyHz, float phaseDegrees, Waveform newWaveform) noexcept;

    float processSample() noexcept;

private:
    static constexpr float kTwoPi = 6.28318530717958647692f;

    double currentSampleRate = 48000.0;
    float phase = 0.0f;
    float phaseIncrement = 0.0f;
    Waveform waveform = Waveform::sine;
};

} // namespace pulse
