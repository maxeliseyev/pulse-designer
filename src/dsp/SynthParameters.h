#pragma once

namespace pulse
{

enum class Waveform
{
    sine,
    triangle,
    square
};

struct SynthConfig
{
    Waveform waveform = Waveform::sine;
    float pitchHz = 55.0f;
    float startPhaseDegrees = 90.0f;
    float ampAttackMs = 0.0f;
    float ampDecayMs = 400.0f;
    float ampCurve = 0.85f;
    float level = 1.0f;
    float keyTracking = 0.0f;
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
