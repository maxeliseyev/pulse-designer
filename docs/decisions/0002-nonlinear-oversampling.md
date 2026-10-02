# Nonlinear output oversampling

## Decision

`Shape` и `Drive` реализованы как независимые host-independent
`NonlinearStage`. Каждый stage поддерживает 1x, 2x, 4x и 8x. Для факторов выше
1 вход между соседними base-rate samples линейно интерполируется, nonlinear
функция считается на sub-samples, а результат усредняется обратно к base rate.

Оба stage получают один и тот же latched oversampling factor из `SynthConfig`.
При amount `0` stage делает точный bypass, поэтому выключенный Shape/Drive не
меняет фазу и амплитуду существующего oscillator/noise тракта.

## Reason

Нелинейность должна находиться внутри oversampled участка, а realtime DSP не
должен зависеть от JUCE buffer types, аллокаций или message thread. Небольшой
фиксированный resampler подходит для первого проверяемого среза и одинаково
работает в `SynthEngine` и offline renderer.

## Consequence

Порог алиасинга измерен в
[`0005-aliasing-dc-gate.md`](0005-aliasing-dc-gate.md). Linear interpolation и
averaging на 4x и 8x укладываются в −60 дБ ниже основного тона и ниже 5 кГц.
Half-band не требуется и публичную модель `SynthConfig` не меняет.
