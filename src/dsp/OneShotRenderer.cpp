#include "OneShotRenderer.h"

#include "SynthEngine.h"

#include <algorithm>

namespace pulse
{

std::vector<float> renderOneShot(const SynthConfig& config,
                                 double sampleRate,
                                 int numSamples,
                                 int midiNote,
                                 float velocity)
{
    const int length = std::max(numSamples, 0);
    std::vector<float> rendered(static_cast<size_t>(length), 0.0f);
    if (length == 0)
        return rendered;

    SynthEngine engine;
    engine.prepare(sampleRate, length);
    engine.setConfig(config);

    const NoteEvent event { NoteEventType::noteOn, 0, midiNote, velocity };
    engine.processMono(&event, 1, rendered.data(), length);
    return rendered;
}

} // namespace pulse
