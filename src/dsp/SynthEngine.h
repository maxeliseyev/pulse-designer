#pragma once

#include "Constants.h"
#include "SynthParameters.h"
#include "Voice.h"

#include <array>
#include <cstdint>

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
    int busyVoiceCount() const noexcept;

private:
    bool acceptsNote(int midiNote) const noexcept;
    void trigger(const NoteEvent& event) noexcept;
    int allocateVoice() const noexcept;
    void render(const NoteEvent* events,
                int numEvents,
                float* left,
                float* right,
                int numSamples,
                bool applyPan) noexcept;

    double currentSampleRate = 48000.0;
    int currentMaxBlockSize = 0;
    SynthConfig config;
    std::array<Voice, kMaxVoices> voices;
    std::array<std::uint64_t, kMaxVoices> voiceTriggerOrder {};
    std::uint64_t triggerCounter = 0;
};

} // namespace pulse
