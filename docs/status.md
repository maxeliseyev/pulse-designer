# Status

Updated: 2026-09-17

Stage: 2 — oscillator и amp envelope
Branch: `main`
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
- Добавлены DSP-тесты и plugin smoke-тесты.
- Добавлен переключатель `PULSE_DESIGNER_COPY_PLUGINS` для сред без доступа к
  системным plugin-папкам.

## Verification

- `rtk make test` — зелёный, 2 test targets и 2 CTest tests.
- VST3/AU/Standalone targets собраны через CMake с
  `PULSE_DESIGNER_COPY_PLUGINS=OFF`.
- При обычном `COPY_PLUGIN_AFTER_BUILD=ON` сборка дошла до копирования, но
  sandbox запретил запись в `~/Library/Audio/Plug-Ins/`.

## Next

Следующий срез DSP:

1. deterministic white/pink/S&H noise;
2. metallic noise как отдельный генератор;
3. TPT/ZDF filter и LP/BP/HP morph;
4. filter envelope и noise amp envelope;
5. тесты sample-rate independence и filter stability.

## Open

- Публичные APVTS parameters ещё не добавлены: их IDs нужно зафиксировать
  вместе с первым рабочим звуковым срезом.
- `Puls` / `Pdsn` и bundle ID пока считаются provisional до этапа identity/state
  contracts.
