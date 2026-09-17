#pragma once

namespace pulse
{

class ExponentialEnvelope final
{
public:
    void prepare(double sampleRate) noexcept;
    void reset(float value = 0.0f) noexcept;

    void start(float currentValue,
               float attackMs,
               float decayMs,
               float curve) noexcept;

    float processSample() noexcept;
    float value() const noexcept { return currentValue; }
    bool isActive() const noexcept { return active; }

private:
    enum class Stage
    {
        idle,
        attack,
        decay
    };

    static float shapedProgress(float progress, float curve) noexcept;

    double currentSampleRate = 48000.0;
    Stage stage = Stage::idle;
    float currentValue = 0.0f;
    float startValue = 0.0f;
    float decayStartValue = 0.0f;
    float curve = 1.0f;
    int stagePosition = 0;
    int stageLength = 0;
    int decayLength = 0;
    bool active = false;
};

} // namespace pulse
