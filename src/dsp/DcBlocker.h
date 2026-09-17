#pragma once

namespace pulse
{

class DcBlocker final
{
public:
    void prepare(double sampleRate) noexcept;
    void reset() noexcept;

    float processSample(float input) noexcept;

private:
    double currentSampleRate = 48000.0;
    float coefficient = 0.0f;
    float previousInput = 0.0f;
    float previousOutput = 0.0f;
};

} // namespace pulse
