#include "SynthEngine.h"

#include <algorithm>
#include <cmath>

namespace pulse
{

namespace
{
constexpr float kCentreGain = 0.7071067811865475f;
constexpr float kQuarterPi = 0.7853981633974483f;
constexpr float kMinimumOutputGainDb = -96.0f;
constexpr float kMaximumOutputGainDb = 12.0f;
constexpr std::uint32_t kNoiseBurstSeedStep = 0x6d2b79f5u;

float gainFromDecibels(float decibels) noexcept
{
    if (!std::isfinite(decibels) || !(decibels > kMinimumOutputGainDb))
        return 0.0f;

    const auto clamped = std::min(decibels, kMaximumOutputGainDb);
    return std::pow(10.0f, clamped / 20.0f);
}

int samplesForMilliseconds(float milliseconds, double sampleRate) noexcept
{
    const auto samples = 0.001 * static_cast<double>(std::max(milliseconds, 0.0f))
                          * sampleRate;
    return std::max(1, static_cast<int>(std::lround(samples)));
}

float midiNoteFrequency(int midiNote) noexcept
{
    const float semitones = static_cast<float>(midiNote - 60);
    return 55.0f * std::pow(2.0f, semitones / 12.0f);
}

float shapedVelocity(float velocity, float curve) noexcept
{
    const auto normalizedVelocity = std::clamp(velocity, 0.0f, 1.0f);
    const auto exponent = 2.0f - 1.5f * std::clamp(curve, 0.0f, 1.0f);
    return std::pow(normalizedVelocity, exponent);
}

float velocityMapping(float shaped, float amount) noexcept
{
    const auto normalizedAmount = std::clamp(amount, 0.0f, 1.0f);
    return 1.0f - normalizedAmount + normalizedAmount * shaped;
}
} // namespace

void SynthEngine::prepare(double sampleRate, int maxBlockSize) noexcept
{
    currentSampleRate = std::max(sampleRate, 1.0);
    currentMaxBlockSize = std::max(maxBlockSize, 0);
    oscillator.prepare(currentSampleRate);
    pitchEnvelope.prepare(currentSampleRate);
    ampEnvelope.prepare(currentSampleRate);
    noiseAmpEnvelope.prepare(currentSampleRate);
    filterEnvelope.prepare(currentSampleRate);
    noiseGenerator.prepare(currentSampleRate);
    noiseFilter.prepare(currentSampleRate);
    dcBlocker.prepare(currentSampleRate);
    toneFilter.prepare(currentSampleRate);
    reset();
}

void SynthEngine::reset() noexcept
{
    oscillator.reset();
    pitchEnvelope.reset();
    ampEnvelope.reset();
    noiseAmpEnvelope.reset();
    filterEnvelope.reset();
    noiseGenerator.reset();
    noiseFilter.reset();
    shapeStage.reset();
    driveStage.reset();
    dcBlocker.reset();
    toneFilter.reset();
    baseFrequencyHz = 55.0f;
    pitchEnvelopeAmountSemitones = 0.0f;
    velocityCutoffOctaves = 0.0f;
    voiceLevel = 1.0f;
    latchOutputControls();
    noiseBurstsRemaining = 0;
    burstSpacingSamples = 1;
    samplesUntilNextBurst = 0;
    burstIndex = 0;
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

    voiceConfig = config;

    const float tracking = std::clamp(voiceConfig.keyTracking, 0.0f, 1.0f);
    const float trackedFrequency = midiNoteFrequency(event.midiNote);
    baseFrequencyHz = voiceConfig.pitchHz * std::pow(trackedFrequency / 55.0f, tracking);

    oscillator.start(baseFrequencyHz,
                     voiceConfig.startPhaseDegrees,
                     voiceConfig.waveform);
    shapeStage.setAmount(voiceConfig.shape);
    shapeStage.setOversampling(voiceConfig.oversampling);
    driveStage.setAmount(voiceConfig.drive);
    driveStage.setDriveType(voiceConfig.driveType);
    driveStage.setOversampling(voiceConfig.oversampling);
    latchOutputControls();
    noiseGenerator.setType(voiceConfig.noiseType);
    noiseGenerator.setSampleAndHoldRate(voiceConfig.sampleAndHoldRateHz);
    noiseFilter.reset();

    const auto shaped = shapedVelocity(event.velocity, voiceConfig.velocityCurve);
    voiceLevel = velocityMapping(shaped, voiceConfig.velocityToLevel);
    pitchEnvelopeAmountSemitones = voiceConfig.pitchEnvelopeAmountSemitones
                                   * velocityMapping(shaped,
                                                     voiceConfig.velocityToPitchEnvelope);
    velocityCutoffOctaves = 4.0f * std::clamp(voiceConfig.velocityToCutoff, 0.0f, 1.0f)
                            * (shaped - 1.0f);

    pitchEnvelope.start(pitchEnvelope.value(),
                        0.0f,
                        voiceConfig.pitchEnvelopeDecayMs,
                        voiceConfig.pitchEnvelopeCurve);
    ampEnvelope.start(ampEnvelope.value(),
                      voiceConfig.ampAttackMs,
                      voiceConfig.ampDecayMs,
                      voiceConfig.ampCurve);
    noiseAmpEnvelope.start(noiseAmpEnvelope.value(),
                           voiceConfig.noiseAmpAttackMs,
                           voiceConfig.noiseAmpDecayMs,
                           voiceConfig.noiseAmpCurve);
    filterEnvelope.start(filterEnvelope.value(),
                         0.0f,
                         voiceConfig.filterEnvelopeDecayMs,
                         voiceConfig.ampCurve);

    noiseBurstsRemaining = std::clamp(voiceConfig.noiseBursts, 1, 4) - 1;
    burstSpacingSamples = samplesForMilliseconds(voiceConfig.burstSpacingMs,
                                                 currentSampleRate);
    samplesUntilNextBurst = burstSpacingSamples;
    burstIndex = 0;
    triggerNoiseBurst();
    voiceActive = true;
}

void SynthEngine::triggerNoiseBurst() noexcept
{
    const auto seedOffset = static_cast<std::uint32_t>(burstIndex) * kNoiseBurstSeedStep;
    noiseGenerator.reset(voiceConfig.noiseSeed + seedOffset);
    noiseFilter.reset();
    noiseAmpEnvelope.start(noiseAmpEnvelope.value(),
                           voiceConfig.noiseAmpAttackMs,
                           voiceConfig.noiseAmpDecayMs,
                           voiceConfig.noiseAmpCurve);
    ++burstIndex;
}

void SynthEngine::latchOutputControls() noexcept
{
    toneFilter.setTilt(voiceConfig.tone);
    voiceOutputGain = gainFromDecibels(voiceConfig.outputGainDb);

    const auto rawPan = std::isfinite(voiceConfig.pan) ? voiceConfig.pan : 0.0f;
    const auto pan = std::clamp(rawPan, -1.0f, 1.0f);
    if (pan == 0.0f)
    {
        voiceLeftGain = kCentreGain;
        voiceRightGain = kCentreGain;
        return;
    }

    if (pan == -1.0f)
    {
        voiceLeftGain = 1.0f;
        voiceRightGain = 0.0f;
        return;
    }

    if (pan == 1.0f)
    {
        voiceLeftGain = 0.0f;
        voiceRightGain = 1.0f;
        return;
    }

    const auto angle = (pan + 1.0f) * kQuarterPi;
    voiceLeftGain = std::cos(angle);
    voiceRightGain = std::sin(angle);
}

float SynthEngine::applyOutput(float input) noexcept
{
    const auto toned = toneFilter.processSample(dcBlocker.processSample(input));
    return voiceOutputGain * voiceLevel * toned;
}

float SynthEngine::processVoiceSample() noexcept
{
    if (!voiceActive)
    {
        const auto shaped = shapeStage.processSample(0.0f);
        const auto driven = driveStage.processSample(shaped);
        return applyOutput(driven);
    }

    if (noiseBurstsRemaining > 0 && samplesUntilNextBurst <= 0)
    {
        triggerNoiseBurst();
        --noiseBurstsRemaining;
        samplesUntilNextBurst = burstSpacingSamples;
    }

    if (noiseBurstsRemaining > 0)
        --samplesUntilNextBurst;

    const auto pitchEnvelopeValue = pitchEnvelope.processSample();
    oscillator.setFrequency(baseFrequencyHz
                            * std::pow(2.0f,
                                       pitchEnvelopeAmountSemitones * pitchEnvelopeValue
                                           / 12.0f));
    const auto oscillatorSample = oscillator.processSample() * ampEnvelope.processSample();
    const auto filterEnvelopeValue = filterEnvelope.processSample();
    const auto cutoff = voiceConfig.noiseCutoffHz
                        * std::pow(2.0f,
                                   velocityCutoffOctaves
                                       + 4.0f
                                       * std::clamp(voiceConfig.filterEnvelopeAmount, -1.0f, 1.0f)
                                       * filterEnvelopeValue);

    noiseFilter.setParameters(cutoff,
                              voiceConfig.noiseResonance,
                              voiceConfig.noiseFilterMorph);
    const auto noiseSample = noiseFilter.processSample(noiseGenerator.processSample()).morphed
                              * noiseAmpEnvelope.processSample();
    const auto noiseMix = std::clamp(voiceConfig.noiseMix, 0.0f, 1.0f);
    const auto mixedSample = oscillatorSample * (1.0f - noiseMix) + noiseSample * noiseMix;
    const auto shapedSample = shapeStage.processSample(mixedSample);
    const auto drivenSample = driveStage.processSample(shapedSample);

    voiceActive = ampEnvelope.isActive() || pitchEnvelope.isActive()
                  || noiseAmpEnvelope.isActive() || filterEnvelope.isActive()
                  || noiseBurstsRemaining > 0;
    return applyOutput(drivenSample);
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

        const auto mono = processVoiceSample();
        if (!applyPan)
        {
            left[sample] = mono;
            continue;
        }

        left[sample] = mono * voiceLeftGain;
        if (right != nullptr)
            right[sample] = mono * voiceRightGain;
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
