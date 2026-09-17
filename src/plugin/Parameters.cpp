#include "Parameters.h"

namespace pulse
{

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    // Public synthesis parameters are added after their IDs and state contract
    // are frozen in the implementation plan.
    return {};
}

} // namespace pulse
