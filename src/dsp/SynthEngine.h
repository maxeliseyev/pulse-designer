#pragma once

#include "Envelope.h"
#include "NoiseGenerator.h"
#include "Oscillator.h"
#include "SynthParameters.h"
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
    float processVoiceSample() noexcept;

    double currentSampleRate = 48000.0;
    int currentMaxBlockSize = 0;
    SynthConfig config;
    SynthConfig voiceConfig;
    Oscillator oscillator;
    ExponentialEnvelope ampEnvelope;
    ExponentialEnvelope noiseAmpEnvelope;
    ExponentialEnvelope filterEnvelope;
    NoiseGenerator noiseGenerator;
    TptStateVariableFilter noiseFilter;
    float voiceLevel = 1.0f;
    bool voiceActive = false;
};

} // namespace pulse
