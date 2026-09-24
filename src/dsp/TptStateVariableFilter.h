#pragma once

namespace pulse
{

struct FilterOutputs
{
    float low = 0.0f;
    float band = 0.0f;
    float high = 0.0f;
    float morphed = 0.0f;
};

class TptStateVariableFilter final
{
public:
    void prepare(double sampleRate) noexcept;
    void reset() noexcept;

    void setParameters(float cutoffHz, float resonance, float morph) noexcept;
    FilterOutputs processSample(float input) noexcept;

private:
    void updateCoefficients() noexcept;

    double currentSampleRate = 48000.0;
    float cutoffHz = 2000.0f;
    float resonance = 0.2f;
    float morph = 0.5f;
    float g = 0.0f;
    float r2 = 5.0f;
    float h = 1.0f;
    float s1 = 0.0f;
    float s2 = 0.0f;
};

} // namespace pulse
