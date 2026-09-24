#pragma once

#include <cstdint>

namespace pulse
{

enum class Waveform
{
    sine,
    triangle,
    square
};

enum class NoiseType
{
    white,
    pink,
    metallic,
    sampleAndHold
};

enum class DriveType
{
    soft,
    hard,
    asymmetric,
    fold
};

struct SynthConfig
{
    Waveform waveform = Waveform::sine;
    float pitchHz = 55.0f;
    float startPhaseDegrees = 90.0f;
    float pitchEnvelopeAmountSemitones = 36.0f;
    float pitchEnvelopeDecayMs = 40.0f;
    float pitchEnvelopeCurve = 0.8f;
    float ampAttackMs = 0.0f;
    float ampDecayMs = 400.0f;
    float ampCurve = 0.85f;
    float keyTracking = 0.0f;
    float shape = 0.0f;
    DriveType driveType = DriveType::soft;
    float drive = 0.0f;
    int oversampling = 4;
    float tone = 0.0f;
    float outputGainDb = 0.0f;
    float pan = 0.0f;

    NoiseType noiseType = NoiseType::white;
    float noiseMix = 0.0f;
    float noiseCutoffHz = 2000.0f;
    float noiseResonance = 0.2f;
    float noiseFilterMorph = 0.5f;
    float filterEnvelopeAmount = 0.0f;
    float filterEnvelopeDecayMs = 60.0f;
    float noiseAmpAttackMs = 0.0f;
    float noiseAmpDecayMs = 150.0f;
    float noiseAmpCurve = 0.85f;
    int noiseBursts = 1;
    float burstSpacingMs = 12.0f;
    float sampleAndHoldRateHz = 800.0f;
    std::uint32_t noiseSeed = 0x9e3779b9u;

    float velocityToLevel = 0.7f;
    float velocityToPitchEnvelope = 0.2f;
    float velocityToCutoff = 0.0f;
    float velocityCurve = 0.5f;
};

enum class NoteEventType
{
    noteOn,
    noteOff
};

struct NoteEvent
{
    NoteEventType type = NoteEventType::noteOn;
    int sampleOffset = 0;
    int midiNote = 60;
    float velocity = 1.0f;
};

} // namespace pulse
