#pragma once

#include "dsp/SynthParameters.h"

#include <atomic>

#include <juce_audio_processors/juce_audio_processors.h>

namespace pulse
{

// Public parameter IDs. They are part of saved projects and host automation:
// never rename one or reuse it for a different meaning.
namespace param
{
inline constexpr const char* oscWaveform = "osc_waveform";
inline constexpr const char* oscPitch = "osc_pitch";
inline constexpr const char* oscPitchEnvAmount = "osc_pitch_env_amount";
inline constexpr const char* oscPitchEnvDecay = "osc_pitch_env_decay";
inline constexpr const char* oscPitchEnvCurve = "osc_pitch_env_curve";
inline constexpr const char* oscStartPhase = "osc_start_phase";
inline constexpr const char* oscAmpAttack = "osc_amp_attack";
inline constexpr const char* oscAmpDecay = "osc_amp_decay";
inline constexpr const char* oscAmpCurve = "osc_amp_curve";
inline constexpr const char* oscShape = "osc_shape";

inline constexpr const char* noiseType = "noise_type";
inline constexpr const char* noiseFilterMorph = "noise_filter_morph";
inline constexpr const char* noiseCutoff = "noise_cutoff";
inline constexpr const char* noiseResonance = "noise_resonance";
inline constexpr const char* noiseFilterEnvAmount = "noise_filter_env_amount";
inline constexpr const char* noiseFilterEnvDecay = "noise_filter_env_decay";
inline constexpr const char* noiseAmpAttack = "noise_amp_attack";
inline constexpr const char* noiseAmpDecay = "noise_amp_decay";
inline constexpr const char* noiseAmpCurve = "noise_amp_curve";
inline constexpr const char* noiseBursts = "noise_bursts";
inline constexpr const char* noiseBurstSpacing = "noise_burst_spacing";

inline constexpr const char* mix = "mix";
inline constexpr const char* driveType = "drive_type";
inline constexpr const char* drive = "drive";
inline constexpr const char* oversampling = "oversampling";
inline constexpr const char* tone = "tone";
inline constexpr const char* gain = "gain";
inline constexpr const char* pan = "pan";

inline constexpr const char* velocityToLevel = "vel_level";
inline constexpr const char* velocityToPitchEnv = "vel_pitch_env";
inline constexpr const char* velocityToCutoff = "vel_cutoff";
inline constexpr const char* velocityCurve = "vel_curve";
inline constexpr const char* noteFilterMode = "note_filter_mode";
inline constexpr const char* noteFilterNote = "note_filter_note";
inline constexpr const char* noteFilterLow = "note_filter_low";
inline constexpr const char* noteFilterHigh = "note_filter_high";
inline constexpr const char* keyTracking = "key_tracking";
inline constexpr const char* voiceMode = "voice_mode";
inline constexpr const char* voices = "voices";
} // namespace param

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

// Reads the current parameter values into a SynthConfig without locking or
// allocating, so the audio thread and the preview can both take a snapshot.
class ParameterSnapshot final
{
public:
    explicit ParameterSnapshot(const juce::AudioProcessorValueTreeState& state);

    SynthConfig read() const noexcept;

private:
    struct Pointers
    {
        const std::atomic<float>* oscWaveform = nullptr;
        const std::atomic<float>* oscPitch = nullptr;
        const std::atomic<float>* oscPitchEnvAmount = nullptr;
        const std::atomic<float>* oscPitchEnvDecay = nullptr;
        const std::atomic<float>* oscPitchEnvCurve = nullptr;
        const std::atomic<float>* oscStartPhase = nullptr;
        const std::atomic<float>* oscAmpAttack = nullptr;
        const std::atomic<float>* oscAmpDecay = nullptr;
        const std::atomic<float>* oscAmpCurve = nullptr;
        const std::atomic<float>* oscShape = nullptr;
        const std::atomic<float>* noiseType = nullptr;
        const std::atomic<float>* noiseFilterMorph = nullptr;
        const std::atomic<float>* noiseCutoff = nullptr;
        const std::atomic<float>* noiseResonance = nullptr;
        const std::atomic<float>* noiseFilterEnvAmount = nullptr;
        const std::atomic<float>* noiseFilterEnvDecay = nullptr;
        const std::atomic<float>* noiseAmpAttack = nullptr;
        const std::atomic<float>* noiseAmpDecay = nullptr;
        const std::atomic<float>* noiseAmpCurve = nullptr;
        const std::atomic<float>* noiseBursts = nullptr;
        const std::atomic<float>* noiseBurstSpacing = nullptr;
        const std::atomic<float>* mix = nullptr;
        const std::atomic<float>* driveType = nullptr;
        const std::atomic<float>* drive = nullptr;
        const std::atomic<float>* oversampling = nullptr;
        const std::atomic<float>* tone = nullptr;
        const std::atomic<float>* gain = nullptr;
        const std::atomic<float>* pan = nullptr;
        const std::atomic<float>* velocityToLevel = nullptr;
        const std::atomic<float>* velocityToPitchEnv = nullptr;
        const std::atomic<float>* velocityToCutoff = nullptr;
        const std::atomic<float>* velocityCurve = nullptr;
        const std::atomic<float>* noteFilterMode = nullptr;
        const std::atomic<float>* noteFilterNote = nullptr;
        const std::atomic<float>* noteFilterLow = nullptr;
        const std::atomic<float>* noteFilterHigh = nullptr;
        const std::atomic<float>* keyTracking = nullptr;
        const std::atomic<float>* voiceMode = nullptr;
        const std::atomic<float>* voices = nullptr;
    };

    Pointers pointers;
};

} // namespace pulse
