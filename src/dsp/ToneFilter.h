#pragma once

namespace pulse
{

class ToneFilter final
{
public:
    void prepare(double sampleRate) noexcept;
    void reset() noexcept;

    void setTilt(float newTilt) noexcept;
    float processSample(float input) noexcept;

private:
    void updateCoefficients() noexcept;

    double currentSampleRate = 48000.0;
    float tilt = 0.0f;
    float lowGain = 1.0f;
    float highGain = 1.0f;
    float coefficient = 0.0f;
    float low = 0.0f;
};

} // namespace pulse
