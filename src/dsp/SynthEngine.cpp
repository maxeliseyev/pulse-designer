#include "SynthEngine.h"

#include <algorithm>
#include <cmath>

namespace pulse
{

namespace
{
constexpr float kCentreGain = 0.7071067811865475f;

float midiNoteFrequency(int midiNote) noexcept
{
    const float semitones = static_cast<float>(midiNote - 60);
    return 55.0f * std::pow(2.0f, semitones / 12.0f);
}

float velocityLevel(float velocity) noexcept
{
    return std::clamp(velocity, 0.0f, 1.0f);
}
} // namespace

void SynthEngine::prepare(double sampleRate, int maxBlockSize) noexcept
{
    currentSampleRate = std::max(sampleRate, 1.0);
    currentMaxBlockSize = std::max(maxBlockSize, 0);
    oscillator.prepare(currentSampleRate);
    ampEnvelope.prepare(currentSampleRate);
    reset();
}

void SynthEngine::reset() noexcept
{
    oscillator.reset();
    ampEnvelope.reset();
    voiceLevel = 1.0f;
    voiceActive = false;
}

void SynthEngine::setConfig(const SynthConfig& newConfig) noexcept
{
    config = newConfig;
}

void SynthEngine::trigger(const NoteEvent& event) noexcept
{
    if (event.type != NoteEventType::noteOn || event.velocity <= 0.0f)
        return;

    const float tracking = std::clamp(config.keyTracking, 0.0f, 1.0f);
    const float trackedFrequency = midiNoteFrequency(event.midiNote);
    const float frequency = config.pitchHz
                            * std::pow(trackedFrequency / 55.0f, tracking);

    oscillator.start(frequency, config.startPhaseDegrees, config.waveform);
    voiceLevel = velocityLevel(event.velocity);
    ampEnvelope.start(ampEnvelope.value(),
                      config.ampAttackMs,
                      config.ampDecayMs,
                      config.ampCurve);
    voiceActive = true;
}

float SynthEngine::processVoiceSample() noexcept
{
    if (!voiceActive)
        return 0.0f;

    const float sample = config.level * voiceLevel * oscillator.processSample()
                         * ampEnvelope.processSample();
    voiceActive = ampEnvelope.isActive();
    return sample;
}

void SynthEngine::processMono(const NoteEvent* events,
                              int numEvents,
                              float* output,
                              int numSamples) noexcept
{
    if (output == nullptr || numSamples <= 0)
        return;

    std::fill(output, output + numSamples, 0.0f);

    int eventIndex = 0;
    for (int sample = 0; sample < numSamples; ++sample)
    {
        while (events != nullptr && eventIndex < numEvents
               && events[eventIndex].sampleOffset <= sample)
        {
            if (events[eventIndex].sampleOffset >= 0)
                trigger(events[eventIndex]);
            ++eventIndex;
        }

        output[sample] = processVoiceSample();
    }
}

void SynthEngine::process(const NoteEvent* events,
                          int numEvents,
                          float* const* output,
                          int numChannels,
                          int numSamples) noexcept
{
    if (output == nullptr || numChannels <= 0 || numSamples <= 0)
        return;

    if (output[0] == nullptr)
    {
        for (int channel = 1; channel < numChannels; ++channel)
        {
            if (output[channel] != nullptr)
                std::fill(output[channel], output[channel] + numSamples, 0.0f);
        }
        return;
    }

    processMono(events, numEvents, output[0], numSamples);
    for (int sample = 0; sample < numSamples; ++sample)
        output[0][sample] *= kCentreGain;

    for (int channel = 1; channel < numChannels; ++channel)
    {
        if (output[channel] == nullptr)
            continue;

        std::copy(output[0], output[0] + numSamples, output[channel]);
    }
}

} // namespace pulse
