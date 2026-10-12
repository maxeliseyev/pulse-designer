#include "SynthEngine.h"

#include <algorithm>

namespace pulse
{

void SynthEngine::prepare(double sampleRate, int maxBlockSize) noexcept
{
    currentSampleRate = std::max(sampleRate, 1.0);
    currentMaxBlockSize = std::max(maxBlockSize, 0);
    for (auto& voice : voices)
        voice.prepare(currentSampleRate);

    reset();
}

void SynthEngine::reset() noexcept
{
    for (auto& voice : voices)
        voice.reset();

    voiceTriggerOrder.fill(0);
    triggerCounter = 0;
}

void SynthEngine::setConfig(const SynthConfig& newConfig) noexcept
{
    config = newConfig;
}

int SynthEngine::busyVoiceCount() const noexcept
{
    return static_cast<int>(std::count_if(voices.begin(), voices.end(), [](const Voice& voice)
                                          { return voice.isBusy(); }));
}

bool SynthEngine::acceptsNote(int midiNote) const noexcept
{
    switch (config.noteFilterMode)
    {
        case NoteFilterMode::omni:
            return true;
        case NoteFilterMode::note:
            return midiNote == config.noteFilterNote;
        case NoteFilterMode::range:
        {
            const auto low = std::min(config.noteFilterLow, config.noteFilterHigh);
            const auto high = std::max(config.noteFilterLow, config.noteFilterHigh);
            return midiNote >= low && midiNote <= high;
        }
    }

    return true;
}

int SynthEngine::allocateVoice() const noexcept
{
    if (config.voiceMode == VoiceMode::restart)
        return 0;

    const auto poolSize = std::clamp(config.voices, 1, kMaxVoices);
    for (int index = 0; index < poolSize; ++index)
    {
        if (!voices[static_cast<std::size_t>(index)].isBusy())
            return index;
    }

    // Every voice is sounding: the oldest one restarts from its current envelope
    // values, which is the same click-free path as restart mode.
    int oldest = 0;
    for (int index = 1; index < poolSize; ++index)
    {
        if (voiceTriggerOrder[static_cast<std::size_t>(index)]
            < voiceTriggerOrder[static_cast<std::size_t>(oldest)])
            oldest = index;
    }

    return oldest;
}

void SynthEngine::trigger(const NoteEvent& event) noexcept
{
    if (event.type != NoteEventType::noteOn || event.velocity <= 0.0f)
        return;

    if (!acceptsNote(event.midiNote))
        return;

    const auto index = static_cast<std::size_t>(allocateVoice());
    voices[index].trigger(config, event.midiNote, event.velocity);
    voiceTriggerOrder[index] = ++triggerCounter;
}

void SynthEngine::render(const NoteEvent* events,
                         int numEvents,
                         float* left,
                         float* right,
                         int numSamples,
                         bool applyPan) noexcept
{
    if (left == nullptr || numSamples <= 0)
        return;

    std::fill(left, left + numSamples, 0.0f);
    if (right != nullptr)
        std::fill(right, right + numSamples, 0.0f);

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

        float leftSum = 0.0f;
        float rightSum = 0.0f;
        for (auto& voice : voices)
        {
            if (!voice.isBusy())
                continue;

            // Pan gains are read before processing: a voice freed on this sample
            // resets them to centre, but its last sample still belongs to its pan.
            const auto leftGain = applyPan ? voice.leftGain() : 1.0f;
            const auto rightGain = voice.rightGain();
            const auto mono = voice.processSample();
            leftSum += mono * leftGain;
            rightSum += mono * rightGain;
        }

        left[sample] = leftSum;
        if (applyPan && right != nullptr)
            right[sample] = rightSum;
    }
}

void SynthEngine::processMono(const NoteEvent* events,
                              int numEvents,
                              float* output,
                              int numSamples) noexcept
{
    render(events, numEvents, output, nullptr, numSamples, false);
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

    float* right = numChannels > 1 ? output[1] : nullptr;
    render(events, numEvents, output[0], right, numSamples, true);

    for (int channel = 2; channel < numChannels; ++channel)
    {
        if (output[channel] == nullptr)
            continue;

        std::copy(output[0], output[0] + numSamples, output[channel]);
    }
}

} // namespace pulse
