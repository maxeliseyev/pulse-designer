# Status

Updated: 2026-10-12

Stage: 4 — voice engine and MIDI
Branch: `feat/dsp-voice-modes`
PR: [#7](https://github.com/maxeliseyev/pulse-designer/pull/7) в `main`
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
- Добавлен выходной Tone: one-pole tilt вокруг 1 кГц, ±6 дБ на ±1.
- Добавлен output gain в децибелах, −96…+12 дБ; −96 дБ и ниже дают тишину.
- Добавлен equal-power pan. Монофонический рендер остаётся до панорамы.
- Зафиксирован линейный Mix: `0` осциллятор, `1` шум, по умолчанию `0.5`.
- Пройден aliasing/DC gate: −60 дБ ниже основного тона и ниже 5 кГц на
  максимальном Drive при 4x и 8x. Хвост удара имеет среднее около нуля.
  Half-band не потребовался.
- Все параметры голоса копируются в snapshot при `note-on`.
- `SynthEngine` — пул из восьми `Voice`. Добавлены `restart` и `overlap` с
  `voices` 1–8 и кражей самого старого голоса, а также Note Filter
  omni/нота/диапазон.
- Ретриггер возвращает огибающие к пику от текущего значения и сохраняет фазу
  осциллятора и состояние фильтра шума. Решение —
  `docs/decisions/0006-voice-modes-retrigger.md`.
- `PluginProcessor` пропускает MIDI-сообщения, которые не являются нотами.
- Добавлены DSP-тесты и plugin smoke-тесты.
- Добавлен переключатель `PULSE_DESIGNER_COPY_PLUGINS` для сред без доступа к
  системным plugin-папкам.

## Verification

- `rtk make test` — зелёный, оба таргета.
- Тесты покрывают четыре noise type, S&H period, filter morph, noise render и
  sample rates 44.1/48/96/192 kHz, pitch/velocity и burst block-size invariance.
- Тесты покрывают все drive types, oversampling factors, DC blocker и nonlinear
  one-shot render.
- Тесты покрывают обход Tone, асимптоты ±6 дБ на 44.1/48/96/192 кГц, направление
  наклона, clamp gain, equal-power pan, latch и block-size invariance стереовыхода.
- Тесты Mix проверяют середину как среднее двух источников, глушение второго
  источника на краях, зажим диапазона и latch.
- Тесты голосов проверяют Note Filter, сумму голосов в overlap, кражу, освобождение
  хвостов, возврат уровня после restart, отсутствие щелчка при ретриггере через
  5 мс на четырёх частотах дискретизации и block-size invariance перекрывающегося
  MIDI при 32/64/512/2048.
- Тесты aliasing gate проверяют чистый синус, 4x/8x на четырёх частотах
  дискретизации и всех Drive, провал 1x и нулевой хвост удара.
- VST3/AU/Standalone targets собраны через CMake с
  `PULSE_DESIGNER_COPY_PLUGINS=OFF`.
- При обычном `COPY_PLUGIN_AFTER_BUILD=ON` сборка дошла до копирования, но
  sandbox запретил запись в `~/Library/Audio/Plug-Ins/`.

## Next

Следующий срез:

1. APVTS parameter contract, latch snapshot в `processBlock` и state round-trip;
2. factory presets;
3. UI и waveform preview после sound-quality gate;
4. host QA и CLAP.

## Open

- Публичные APVTS parameters ещё не добавлены: их IDs нужно зафиксировать
  вместе с первым рабочим звуковым срезом.
- `Puls` / `Pdsn` и bundle ID пока считаются provisional до этапа identity/state
  contracts.
