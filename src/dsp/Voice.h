#pragma once

#include "DcBlocker.h"
#include "Envelope.h"
#include "NoiseGenerator.h"
#include "NonlinearStage.h"
#include "Oscillator.h"
#include "SynthParameters.h"
#include "ToneFilter.h"
#include "TptStateVariableFilter.h"

namespace pulse
{

// One latched drum hit with its own source, filter, nonlinear and output state.
class Voice final
{
public:
    void prepare(double sampleRate) noexcept;
    void reset() noexcept;

    // Retriggering a busy voice restarts every envelope from its current value and
    // keeps the oscillator phase and noise filter state, so it does not click.
    void trigger(const SynthConfig& config, int midiNote, float velocity) noexcept;

    float processSample() noexcept;

    bool isBusy() const noexcept { return busy; }
    float leftGain() const noexcept { return voiceLeftGain; }
    float rightGain() const noexcept { return voiceRightGain; }

private:
    void triggerNoiseBurst(bool resetFilter) noexcept;
    void latchOutputControls() noexcept;
    float applyOutput(float input) noexcept;

    double currentSampleRate = 48000.0;
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
    int quietSamples = 0;
    bool envelopesActive = false;
    bool busy = false;
};

} // namespace pulse
