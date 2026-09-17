#pragma once

#include "SynthParameters.h"

namespace pulse
{

class NonlinearStage final
{
public:
    enum class Kind
    {
        shape,
        drive
    };

    explicit NonlinearStage(Kind stageKind) noexcept : kind(stageKind) {}

    void reset() noexcept;
    void setAmount(float newAmount) noexcept;
    void setDriveType(DriveType newType) noexcept { driveType = newType; }
    void setOversampling(int factor) noexcept;

    float processSample(float input) noexcept;

private:
    static int normalizedOversampling(int factor) noexcept;
    float processNonlinear(float input) const noexcept;
    static float foldSample(float input) noexcept;

    Kind kind;
    DriveType driveType = DriveType::soft;
    float amount = 0.0f;
    int oversampling = 4;
    float previousInput = 0.0f;
};

} // namespace pulse
