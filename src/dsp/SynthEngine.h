#pragma once

#include "Envelope.h"
#include "DcBlocker.h"
#include "NoiseGenerator.h"
#include "NonlinearStage.h"
#include "Oscillator.h"
#include "SynthParameters.h"
#include "ToneFilter.h"
#include "TptStateVariableFilter.h"

#include <cstddef>

namespace pulse
{

class SynthEngine final
{
public:
    void prepare(double sampleRate, int maxBlockSize) noexcept;
    void reset() noexcept;

    void setConfig(const SynthConfig& newConfig) noexcept;

    void process(const NoteEvent* events,
                 int numEvents,
                 float* const* output,
                 int numChannels,
                 int numSamples) noexcept;

    void processMono(const NoteEvent* events,
                     int numEvents,
                     float* output,
                     int numSamples) noexcept;

    double sampleRate() const noexcept { return currentSampleRate; }
    int maxBlockSize() const noexcept { return currentMaxBlockSize; }

private:
    void trigger(const NoteEvent& event) noexcept;
    void triggerNoiseBurst() noexcept;
    void latchOutputControls() noexcept;
    float processVoiceSample() noexcept;
    float applyOutput(float input) noexcept;
    void render(const NoteEvent* events,
                int numEvents,
                float* left,
                float* right,
                int numSamples,
                bool applyPan) noexcept;

    double currentSampleRate = 48000.0;
    int currentMaxBlockSize = 0;
    SynthConfig config;
    SynthConfig voiceConfig;
    Oscillator oscillator;
    ExponentialEnvelope pitchEnvelope;
    ExponentialEnvelope ampEnvelope;
    ExponentialEnvelope noiseAmpEnvelope;
    ExponentialEnvelope filterEnvelope;
    NoiseGenerator noiseGenerator;
    TptStateVariableFilter noiseFilter;
    NonlinearStage shapeStage { NonlinearStage::Kind::shape };
    NonlinearStage driveStage { NonlinearStage::Kind::drive };
    DcBlocker dcBlocker;
    ToneFilter toneFilter;
    float baseFrequencyHz = 55.0f;
    float pitchEnvelopeAmountSemitones = 0.0f;
    float velocityCutoffOctaves = 0.0f;
    float voiceLevel = 1.0f;
    float voiceOutputGain = 1.0f;
    float voiceLeftGain = 0.7071067811865475f;
    float voiceRightGain = 0.7071067811865475f;
    int noiseBurstsRemaining = 0;
    int burstSpacingSamples = 1;
    int samplesUntilNextBurst = 0;
    int burstIndex = 0;
    bool voiceActive = false;
};

} // namespace pulse
