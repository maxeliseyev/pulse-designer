#include "Parameters.h"

#include <cmath>
#include <memory>

namespace pulse
{

namespace
{
constexpr int kParameterVersion = 1;
constexpr float kSilentGainDb = -96.0f;

using Range = juce::NormalisableRange<float>;
using FloatAttributes = juce::AudioParameterFloatAttributes;

Range logRange(float minimum, float maximum)
{
    const auto ratio = maximum / minimum;
    return { minimum,
             maximum,
             [=](float, float, float normalized)
             { return minimum * std::pow(ratio, normalized); },
             [=](float, float, float value)
             { return std::log(value / minimum) / std::log(ratio); } };
}

Range skewedRange(float minimum, float maximum, float centre)
{
    Range range { minimum, maximum };
    range.setSkewForCentre(centre);
    return range;
}

juce::String formatHertz(float value, int)
{
    if (value >= 1000.0f)
        return juce::String(value / 1000.0f, 2) + " kHz";

    return juce::String(value, value < 100.0f ? 1 : 0) + " Hz";
}

float parseHertz(const juce::String& text)
{
    const auto value = text.getFloatValue();
    return text.containsIgnoreCase("k") ? value * 1000.0f : value;
}

juce::String formatMilliseconds(float value, int)
{
    if (value >= 1000.0f)
        return juce::String(value / 1000.0f, 2) + " s";

    return juce::String(value, value < 10.0f ? 2 : 1) + " ms";
}

float parseMilliseconds(const juce::String& text)
{
    const auto value = text.getFloatValue();
    const auto trimmed = text.trim();
    const bool seconds = trimmed.endsWithIgnoreCase("s") && !trimmed.endsWithIgnoreCase("ms");
    return seconds ? value * 1000.0f : value;
}

juce::String formatDecibels(float value, int)
{
    if (value <= kSilentGainDb)
        return "-inf dB";

    return juce::String(value, 1) + " dB";
}

float parseDecibels(const juce::String& text)
{
    if (text.containsIgnoreCase("inf"))
        return kSilentGainDb;

    return text.getFloatValue();
}

juce::String formatPercent(float value, int)
{
    return juce::String(juce::roundToInt(value)) + " %";
}

juce::String formatSemitones(float value, int)
{
    const auto rounded = juce::String(value, 1);
    return (value > 0.0f ? "+" : "") + rounded + " st";
}

juce::String formatDegrees(float value, int)
{
    const juce::String degreeSign { juce::CharPointer_UTF8(" \xc2\xb0") };
    return juce::String(juce::roundToInt(value)) + degreeSign;
}

juce::String formatUnit(float value, int)
{
    return juce::String(value, 2);
}

juce::String formatPan(float value, int)
{
    if (std::abs(value) < 0.005f)
        return "C";

    const auto amount = juce::String(juce::roundToInt(std::abs(value) * 100.0f));
    return value < 0.0f ? "L" + amount : "R" + amount;
}

juce::String formatNote(int value, int)
{
    return juce::MidiMessage::getMidiNoteName(value, true, true, 3) + " (" + juce::String(value)
           + ")";
}

juce::ParameterID id(const char* name)
{
    return { name, kParameterVersion };
}

std::unique_ptr<juce::AudioParameterFloat> floatParameter(
    const char* name,
    const juce::String& label,
    Range range,
    float defaultValue,
    juce::String (*format)(float, int),
    float (*parse)(const juce::String&) = nullptr)
{
    auto attributes = FloatAttributes().withStringFromValueFunction(format);
    if (parse != nullptr)
        attributes = attributes.withValueFromStringFunction(parse);

    return std::make_unique<juce::AudioParameterFloat>(id(name),
                                                       label,
                                                       range,
                                                       defaultValue,
                                                       attributes);
}

std::unique_ptr<juce::AudioParameterChoice> choiceParameter(const char* name,
                                                            const juce::String& label,
                                                            const juce::StringArray& choices,
                                                            int defaultIndex)
{
    return std::make_unique<juce::AudioParameterChoice>(id(name), label, choices, defaultIndex);
}

std::unique_ptr<juce::AudioParameterInt> noteParameter(const char* name,
                                                       const juce::String& label,
                                                       int defaultValue)
{
    return std::make_unique<juce::AudioParameterInt>(
        id(name),
        label,
        0,
        127,
        defaultValue,
        juce::AudioParameterIntAttributes().withStringFromValueFunction(formatNote));
}

const std::atomic<float>* pointer(const juce::AudioProcessorValueTreeState& state,
                                  const char* name)
{
    auto* raw = state.getRawParameterValue(name);
    jassert(raw != nullptr);
    return raw;
}

float valueOf(const std::atomic<float>* raw) noexcept
{
    return raw->load(std::memory_order_relaxed);
}

int indexOf(const std::atomic<float>* raw) noexcept
{
    return juce::roundToInt(valueOf(raw));
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    auto oscillator = std::make_unique<juce::AudioProcessorParameterGroup>("osc",
                                                                           "Oscillator",
                                                                           "|");
    oscillator->addChild(
        choiceParameter(param::oscWaveform, "Waveform", { "Sine", "Triangle", "Square" }, 0),
        floatParameter(param::oscPitch, "Pitch", logRange(20.0f, 2000.0f), 55.0f, formatHertz,
                       parseHertz),
        floatParameter(param::oscPitchEnvAmount, "Pitch Env Amount", Range { -48.0f, 48.0f },
                       36.0f, formatSemitones),
        floatParameter(param::oscPitchEnvDecay, "Pitch Env Decay", logRange(1.0f, 500.0f), 40.0f,
                       formatMilliseconds, parseMilliseconds),
        floatParameter(param::oscPitchEnvCurve, "Pitch Env Curve", Range { 0.0f, 1.0f }, 0.8f,
                       formatUnit),
        floatParameter(param::oscStartPhase, "Start Phase", Range { 0.0f, 360.0f }, 90.0f,
                       formatDegrees),
        floatParameter(param::oscAmpAttack, "Osc Attack", skewedRange(0.0f, 50.0f, 10.0f), 0.0f,
                       formatMilliseconds, parseMilliseconds),
        floatParameter(param::oscAmpDecay, "Osc Decay", logRange(5.0f, 3000.0f), 400.0f,
                       formatMilliseconds, parseMilliseconds),
        floatParameter(param::oscAmpCurve, "Osc Curve", Range { 0.0f, 1.0f }, 0.85f, formatUnit),
        floatParameter(param::oscShape, "Shape", Range { 0.0f, 1.0f }, 0.0f, formatUnit));

    auto noise = std::make_unique<juce::AudioProcessorParameterGroup>("noise", "Noise", "|");
    noise->addChild(
        choiceParameter(param::noiseType, "Noise Type", { "White", "Pink", "Metallic", "S&H" }, 0),
        floatParameter(param::noiseFilterMorph, "Filter Morph", Range { 0.0f, 1.0f }, 0.5f,
                       formatUnit),
        floatParameter(param::noiseCutoff, "Cutoff", logRange(20.0f, 20000.0f), 2000.0f,
                       formatHertz, parseHertz),
        floatParameter(param::noiseResonance, "Resonance", Range { 0.0f, 1.0f }, 0.2f,
                       formatUnit),
        floatParameter(param::noiseFilterEnvAmount, "Filter Env Amount",
                       Range { -100.0f, 100.0f }, 0.0f, formatPercent),
        floatParameter(param::noiseFilterEnvDecay, "Filter Env Decay", logRange(1.0f, 500.0f),
                       60.0f, formatMilliseconds, parseMilliseconds),
        floatParameter(param::noiseAmpAttack, "Noise Attack", skewedRange(0.0f, 50.0f, 10.0f),
                       0.0f, formatMilliseconds, parseMilliseconds),
        floatParameter(param::noiseAmpDecay, "Noise Decay", logRange(2.0f, 2000.0f), 150.0f,
                       formatMilliseconds, parseMilliseconds),
        floatParameter(param::noiseAmpCurve, "Noise Curve", Range { 0.0f, 1.0f }, 0.85f,
                       formatUnit),
        std::make_unique<juce::AudioParameterInt>(id(param::noiseBursts), "Bursts", 1, 4, 1),
        floatParameter(param::noiseBurstSpacing, "Burst Spacing", Range { 5.0f, 40.0f }, 12.0f,
                       formatMilliseconds, parseMilliseconds));

    auto output = std::make_unique<juce::AudioProcessorParameterGroup>("output", "Output", "|");
    output->addChild(
        floatParameter(param::mix, "Mix", Range { 0.0f, 1.0f }, 0.5f, formatUnit),
        choiceParameter(param::driveType, "Drive Type",
                        { "Soft", "Hard", "Asymmetric", "Fold" }, 0),
        floatParameter(param::drive, "Drive", Range { 0.0f, 1.0f }, 0.0f, formatUnit),
        choiceParameter(param::oversampling, "Oversampling", { "1x", "2x", "4x", "8x" }, 2),
        floatParameter(param::tone, "Tone", Range { -1.0f, 1.0f }, 0.0f, formatUnit),
        floatParameter(param::gain, "Gain", skewedRange(kSilentGainDb, 12.0f, -12.0f), 0.0f,
                       formatDecibels, parseDecibels),
        floatParameter(param::pan, "Pan", Range { -1.0f, 1.0f }, 0.0f, formatPan));

    auto playing = std::make_unique<juce::AudioProcessorParameterGroup>("playing",
                                                                        "Playing",
                                                                        "|");
    playing->addChild(
        floatParameter(param::velocityToLevel, "Vel > Level", Range { 0.0f, 100.0f }, 70.0f,
                       formatPercent),
        floatParameter(param::velocityToPitchEnv, "Vel > Pitch Env", Range { 0.0f, 100.0f },
                       20.0f, formatPercent),
        floatParameter(param::velocityToCutoff, "Vel > Cutoff", Range { 0.0f, 100.0f }, 0.0f,
                       formatPercent),
        floatParameter(param::velocityCurve, "Vel Curve", Range { 0.0f, 1.0f }, 0.5f, formatUnit),
        choiceParameter(param::noteFilterMode, "Note Filter", { "Omni", "Note", "Range" }, 0),
        noteParameter(param::noteFilterNote, "Filter Note", 36),
        noteParameter(param::noteFilterLow, "Filter Low", 0),
        noteParameter(param::noteFilterHigh, "Filter High", 127),
        floatParameter(param::keyTracking, "Key Tracking", Range { 0.0f, 100.0f }, 0.0f,
                       formatPercent),
        choiceParameter(param::voiceMode, "Voice Mode", { "Restart", "Overlap" }, 0),
        std::make_unique<juce::AudioParameterInt>(id(param::voices), "Voices", 1, 8, 1));

    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add(std::move(oscillator), std::move(noise), std::move(output), std::move(playing));
    return layout;
}

ParameterSnapshot::ParameterSnapshot(const juce::AudioProcessorValueTreeState& state)
{
    auto& p = pointers;
    p.oscWaveform = pointer(state, param::oscWaveform);
    p.oscPitch = pointer(state, param::oscPitch);
    p.oscPitchEnvAmount = pointer(state, param::oscPitchEnvAmount);
    p.oscPitchEnvDecay = pointer(state, param::oscPitchEnvDecay);
    p.oscPitchEnvCurve = pointer(state, param::oscPitchEnvCurve);
    p.oscStartPhase = pointer(state, param::oscStartPhase);
    p.oscAmpAttack = pointer(state, param::oscAmpAttack);
    p.oscAmpDecay = pointer(state, param::oscAmpDecay);
    p.oscAmpCurve = pointer(state, param::oscAmpCurve);
    p.oscShape = pointer(state, param::oscShape);
    p.noiseType = pointer(state, param::noiseType);
    p.noiseFilterMorph = pointer(state, param::noiseFilterMorph);
    p.noiseCutoff = pointer(state, param::noiseCutoff);
    p.noiseResonance = pointer(state, param::noiseResonance);
    p.noiseFilterEnvAmount = pointer(state, param::noiseFilterEnvAmount);
    p.noiseFilterEnvDecay = pointer(state, param::noiseFilterEnvDecay);
    p.noiseAmpAttack = pointer(state, param::noiseAmpAttack);
    p.noiseAmpDecay = pointer(state, param::noiseAmpDecay);
    p.noiseAmpCurve = pointer(state, param::noiseAmpCurve);
    p.noiseBursts = pointer(state, param::noiseBursts);
    p.noiseBurstSpacing = pointer(state, param::noiseBurstSpacing);
    p.mix = pointer(state, param::mix);
    p.driveType = pointer(state, param::driveType);
    p.drive = pointer(state, param::drive);
    p.oversampling = pointer(state, param::oversampling);
    p.tone = pointer(state, param::tone);
    p.gain = pointer(state, param::gain);
    p.pan = pointer(state, param::pan);
    p.velocityToLevel = pointer(state, param::velocityToLevel);
    p.velocityToPitchEnv = pointer(state, param::velocityToPitchEnv);
    p.velocityToCutoff = pointer(state, param::velocityToCutoff);
    p.velocityCurve = pointer(state, param::velocityCurve);
    p.noteFilterMode = pointer(state, param::noteFilterMode);
    p.noteFilterNote = pointer(state, param::noteFilterNote);
    p.noteFilterLow = pointer(state, param::noteFilterLow);
    p.noteFilterHigh = pointer(state, param::noteFilterHigh);
    p.keyTracking = pointer(state, param::keyTracking);
    p.voiceMode = pointer(state, param::voiceMode);
    p.voices = pointer(state, param::voices);
}

SynthConfig ParameterSnapshot::read() const noexcept
{
    const auto& p = pointers;
    SynthConfig config;

    config.waveform = static_cast<Waveform>(indexOf(p.oscWaveform));
    config.pitchHz = valueOf(p.oscPitch);
    config.pitchEnvelopeAmountSemitones = valueOf(p.oscPitchEnvAmount);
    config.pitchEnvelopeDecayMs = valueOf(p.oscPitchEnvDecay);
    config.pitchEnvelopeCurve = valueOf(p.oscPitchEnvCurve);
    config.startPhaseDegrees = valueOf(p.oscStartPhase);
    config.ampAttackMs = valueOf(p.oscAmpAttack);
    config.ampDecayMs = valueOf(p.oscAmpDecay);
    config.ampCurve = valueOf(p.oscAmpCurve);
    config.shape = valueOf(p.oscShape);

    config.noiseType = static_cast<NoiseType>(indexOf(p.noiseType));
    config.noiseFilterMorph = valueOf(p.noiseFilterMorph);
    config.noiseCutoffHz = valueOf(p.noiseCutoff);
    config.noiseResonance = valueOf(p.noiseResonance);
    config.filterEnvelopeAmount = valueOf(p.noiseFilterEnvAmount) / 100.0f;
    config.filterEnvelopeDecayMs = valueOf(p.noiseFilterEnvDecay);
    config.noiseAmpAttackMs = valueOf(p.noiseAmpAttack);
    config.noiseAmpDecayMs = valueOf(p.noiseAmpDecay);
    config.noiseAmpCurve = valueOf(p.noiseAmpCurve);
    config.noiseBursts = indexOf(p.noiseBursts);
    config.burstSpacingMs = valueOf(p.noiseBurstSpacing);

    config.noiseMix = valueOf(p.mix);
    config.driveType = static_cast<DriveType>(indexOf(p.driveType));
    config.drive = valueOf(p.drive);
    config.oversampling = 1 << juce::jlimit(0, 3, indexOf(p.oversampling));
    config.tone = valueOf(p.tone);
    config.outputGainDb = valueOf(p.gain);
    config.pan = valueOf(p.pan);

    config.velocityToLevel = valueOf(p.velocityToLevel) / 100.0f;
    config.velocityToPitchEnvelope = valueOf(p.velocityToPitchEnv) / 100.0f;
    config.velocityToCutoff = valueOf(p.velocityToCutoff) / 100.0f;
    config.velocityCurve = valueOf(p.velocityCurve);
    config.noteFilterMode = static_cast<NoteFilterMode>(indexOf(p.noteFilterMode));
    config.noteFilterNote = indexOf(p.noteFilterNote);
    config.noteFilterLow = indexOf(p.noteFilterLow);
    config.noteFilterHigh = indexOf(p.noteFilterHigh);
    config.keyTracking = valueOf(p.keyTracking) / 100.0f;
    config.voiceMode = static_cast<VoiceMode>(indexOf(p.voiceMode));
    config.voices = indexOf(p.voices);

    return config;
}

} // namespace pulse
