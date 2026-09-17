# Status

Updated: 2026-09-17

Stage: 3 — nonlinear output stage
Branch: `feat/dsp-noise-filter`
PR: нет
Blockers: нет

## Done

- Добавлены CMake, presets, Makefile и FetchContent-зависимости.
- Подключены JUCE 8.0.15 и Catch2 3.8.1.
- Создан MIDI-инструмент без audio input с одним стереовыходом.
- Добавлены targets VST3, AU и Standalone.
- Добавлен host-independent `pulse::SynthEngine`; silent output заменён первым
  рабочим oscillator/envelope трактом.
- Добавлены sine/triangle/square oscillator и посэмпловая amp envelope.
- Добавлен детерминированный офлайн-рендер одного удара.
- Подключён sample-accurate MIDI note-on к `PluginProcessor`.
- Добавлены deterministic white/pink/metallic/S&H noise generators.
- Добавлены noise amp envelope и bipolar filter envelope.
- Добавлен host-independent TPT state-variable filter с LP/BP/HP morph.
- Добавлены pitch envelope и sample-accurate oscillator frequency sweep.
- Добавлены velocity mappings для level, pitch envelope и cutoff.
- Добавлены до четырёх deterministic noise bursts с burst spacing.
- Добавлены Shape и soft/hard/asymmetric/fold Drive.
- Добавлено переключаемое 1x/2x/4x/8x oversampling вокруг обоих nonlinear stages.
- Добавлен постоянно включённый DC blocker на 10 Гц.
- Все параметры голоса копируются в snapshot при `note-on`.
- Добавлены DSP-тесты и plugin smoke-тесты.
- Добавлен переключатель `PULSE_DESIGNER_COPY_PLUGINS` для сред без доступа к
  системным plugin-папкам.

## Verification

- `rtk make test` — зелёный, 2 test targets и 2 CTest tests.
- Тесты покрывают четыре noise type, S&H period, filter morph, noise render и
  sample rates 44.1/48/96/192 kHz, pitch/velocity и burst block-size invariance.
- Тесты покрывают все drive types, oversampling factors, DC blocker и nonlinear
  one-shot render.
- VST3/AU/Standalone targets собраны через CMake с
  `PULSE_DESIGNER_COPY_PLUGINS=OFF`.
- При обычном `COPY_PLUGIN_AFTER_BUILD=ON` сборка дошла до копирования, но
  sandbox запретил запись в `~/Library/Audio/Plug-Ins/`.

## Next

Следующий срез DSP:

1. Tone, output gain и equal-power pan;
2. oscillator/noise mix с зафиксированной публичной семантикой;
3. измерительный aliasing/DC gate;
4. APVTS parameter contract и state round-trip;
5. factory presets.

## Open

- Публичные APVTS parameters ещё не добавлены: их IDs нужно зафиксировать
  вместе с первым рабочим звуковым срезом.
- Внутренний `noiseMix` по умолчанию остаётся `0.0`, чтобы не менять характер
  первого oscillator-среза до появления публичного parameter contract; default
  продуктового Mix из спецификации зафиксируем вместе с APVTS.
- Текущий oversampling использует deterministic linear interpolation и
  averaging при downsample; aliasing threshold нужно измерить отдельным gate и
  при необходимости заменить внутренний resampler на half-band вариант.
- `Puls` / `Pdsn` и bundle ID пока считаются provisional до этапа identity/state
  contracts.
