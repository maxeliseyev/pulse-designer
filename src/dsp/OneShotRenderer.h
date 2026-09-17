#pragma once

#include "SynthParameters.h"

#include <vector>

namespace pulse
{

std::vector<float> renderOneShot(const SynthConfig& config,
                                 double sampleRate,
                                 int numSamples,
                                 int midiNote = 60,
                                 float velocity = 1.0f);

} // namespace pulse
