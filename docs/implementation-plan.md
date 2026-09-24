# План реализации Pulse Designer

Статус: bootstrap, oscillator/envelope, noise/TPT, pitch/velocity/bursts,
nonlinear output и Tone/gain/pan завершены; следующий шаг — публичный Mix и APVTS

Спецификация продукта: [`drum-synth-spec.md`](drum-synth-spec.md)

## Архитектурный ориентир

За основу берём организацию `beat-equalizer`:

- C++20;
- JUCE 8.0.15 через CMake `FetchContent`;
- CMake presets и `Makefile` как основной интерфейс сборки;
- Catch2 3.8.1;
- `src/dsp` для алгоритмов;
- `src/plugin` для JUCE, APVTS, MIDI и UI;
- `tests` для синтетических и интеграционных проверок;
- `docs`, `VERSION`, `CHANGELOG.md`, feature-ветки и небольшие PR.

Копировать архитектуру буквально нельзя: `beat-equalizer` — аудиоэффект с
входным bus, а Pulse Designer — MIDI-инструмент с одним стереовыходом.

## Принцип реализации

Офлайн-рендер одного удара и realtime-обработка должны использовать один и
тот же `SynthEngine`. Отдельные реализации для waveform preview и
`processBlock` запрещены: они неизбежно начнут звучать по-разному.

Внутри одного voice хранятся:

- latched-конфигурация, полученная в момент `note-on`;
- фаза осциллятора;
- состояние pitch и amp envelopes;
- состояние шумового генератора и его RNG;
- состояние фильтра;
- состояние нелинейных блоков.

Realtime-код не аллоцирует память и не блокируется. Шум должен быть
детерминированным и продвигаться посэмплово, чтобы размер аудиоблока не влиял
на результат.

## Этап 0. Зафиксировать контракты

До написания DSP определить:

- имя плагина, namespace, manufacturer/plugin codes;
- стабильные APVTS ID и схему состояния;
- точную модель `Note Filter` — режим, нота и границы диапазона;
- семантику `restart` и `overlap`;
- seed и правила повторяемости noise;
- default oversampling — 4x;
- лицензионную и платформенную стратегию.

Короткими аудио-прототипами закрыть открытые вопросы спецификации:

1. Оставлять ли одновременно `Shape` и `Drive`.
2. Достаточен ли один осциллятор для снейра.
3. Нужны ли pink и S&H сверх белого шума с фильтром.
4. Обновлять waveform preview во время движения ручки или только после отпускания.

Рекомендуемый старт: оставить оба шейпера, начать с одного осциллятора,
реализовать все четыре типа шума и обновлять preview через debounce во время
движения ручки с немедленным пересчётом после отпускания.

## Этап 1. Скелет проекта и минимальный плагин

Создать:

- `CMakeLists.txt`;
- `CMakePresets.json`;
- `cmake/Dependencies.cmake`;
- `Makefile`;
- `src/dsp`, `src/plugin`, `tests`, `packaging`, `scripts`;
- `VERSION`, `CHANGELOG.md` и базовую документацию.

Настроить JUCE-плагин со следующими свойствами:

- `IS_SYNTH TRUE`;
- `NEEDS_MIDI_INPUT TRUE`;
- `NEEDS_MIDI_OUTPUT FALSE`;
- один стереовыход;
- без аудиовхода;
- VST3, AU, Standalone;
- инструментальная категория.

Добавить пустой `PluginProcessor`, `PluginEditor`, APVTS и `prepareToPlay` /
`processBlock` с `ScopedNoDenormals`.

**Gate:** Standalone открывается, VST3 виден в Reaper, note-on не приводит к
ошибкам, `make test` зелёный.

## Этап 2. DSP-ядро офлайн-рендера

Сначала реализовать JUCE-независимый или минимально зависящий от JUCE слой:

```text
SynthConfig + NoteEvent + sampleRate
        ↓
deterministic one-shot renderer
        ↓
audio buffer
```

Порядок блоков:

1. Экспоненциальные attack/decay envelopes с управляемой кривой.
2. Sine, triangle, square.
3. Pitch envelope в полутонах и start phase.
4. White, pink, metallic и S&H noise. **Готово.**
5. TPT/ZDF filter, LP/BP/HP morph, filter envelope и noise amp envelope.
   **Готово.**
6. Noise bursts. **Готово.**
7. Velocity mapping и key tracking. **Velocity mapping готово; key tracking
   уже поддержан базовым voice engine.**
8. Oscillator/noise mix с явной линейной семантикой. **Базовый mix готов;
   публичный контракт ещё не зафиксирован.**

Каждый примитив покрывается отдельным синтетическим тестом до подключения к
`PluginProcessor`.

## Этап 3. Фильтр, нелинейности и выходной тракт

Добавить отдельные тестируемые блоки:

- TPT/ZDF state-variable filter;
- непрерывный LP → BP → HP morph;
- filter envelope;
- `Shape`. **Готово.**
- soft, hard, asymmetric и fold drive. **Готово.**
- oversampling 1x/2x/4x/8x вокруг каждого нелинейного участка. **Готово.**
- DC blocker 10 Гц. **Готово.**
- Tone. **Готово.** Опорная частота 1 кГц, глубина ±6 дБ.
- Gain. **Готово.** −96…+12 дБ, ниже −96 дБ — тишина.
- equal-power pan. **Готово.** В монофоническом рендере не применяется.

В текущем срезе используется собственный небольшой host-independent TPT-wrapper
с теми же уравнениями, что и JUCE `StateVariableTPTFilter`, потому что он сразу
отдаёт LP/BP/HP из одного общего состояния. Решение зафиксировано в
[`docs/decisions/0001-tpt-filter-wrapper.md`](decisions/0001-tpt-filter-wrapper.md).

Oversampling nonlinear stages пока использует deterministic linear interpolation
на входе и averaging при downsample. Это сохраняет realtime-контракт без
аллокаций; отдельный aliasing gate должен определить, нужен ли более сложный
half-band resampler.

**Gate:** один удар рендерится из конфигурации, нет NaN/Inf, DC близок к нулю,
алиасинг на максимальном Drive укладывается в заранее зафиксированный порог.

## Этап 4. Voice engine и MIDI

Подключить MIDI к `PluginProcessor`:

- разбивать блок по sample offset MIDI-событий;
- обрабатывать `note-on` sample-точно;
- latch всех параметров при запуске;
- реализовать `restart` с перезапуском от текущего значения огибающей;
- реализовать `overlap` с пулом до 8 voices;
- реализовать Note Filter;
- применить velocity к level, pitch envelope и cutoff;
- корректно завершать хвосты.

`processBlock` не должен аллоцировать память, ждать mutex или выполнять
офлайн-рендер waveform preview.

**Gate:** один и тот же MIDI-поток при block size 32, 64, 512 и 2048 даёт
побитово одинаковый output.

## Этап 5. Приёмочные тесты и настройка звука

Добавить тесты для:

- envelopes;
- oscillator pitch sweep;
- noise generators;
- filter morph;
- saturation и oversampling;
- DC blocker;
- retrigger через 5 мс;
- voice allocation;
- MIDI block-size independence;
- sample-rate independence;
- state round-trip;
- всех factory presets.

Обязательные технические проверки из спецификации:

- aliasing на максимальном Drive;
- среднее значение рендера близко к нулю;
- отсутствие щелчка при ретриггере через 5 мс;
- нагрузка восьми инстансов с 4x oversampling;
- одинаковое поведение на 44.1, 48, 96 и 192 кГц.

Затем провести главный sound-quality gate. Настройка выполняется в порядке:

1. kick;
2. snare;
3. tom;
4. closed/open hat;
5. clap, rim и zap.

Сравнивать при выровненной громкости с Microtonic и живыми записями. Этап
нельзя сокращать: именно здесь определяется качество продукта.

## Этап 6. APVTS, состояние и пресеты

После стабилизации DSP оформить публичный параметрный контракт:

- oscillator;
- noise;
- mix/output;
- playing;
- oversampling.

Добавить:

- `AudioParameterChoice`, `AudioParameterFloat`, `AudioParameterInt`;
- логарифмические диапазоны для времени и частот;
- единицы измерения и форматы отображения;
- versioned `ValueTree` state;
- state migration;
- девять заводских пресетов: kick deep, kick tight, snare, rim, tom, closed
  hat, open hat, clap, zap.

Каждый пресет должен пройти рендер без NaN/Inf, чрезмерного DC и неконтролируемого
пикового уровня.

## Этап 7. UI и waveform preview

Собрать одну панель:

- два столбца Oscillator / Noise;
- нижняя секция Mix / Drive / Output;
- боковая секция Playing;
- waveform preview результата.

Preview должен:

- получать snapshot параметров;
- вызывать тот же офлайн-рендер, что и тесты;
- работать на message thread или worker-е, но не в audio thread;
- использовать debounce;
- показывать атаку, хвост и результат Drive.

Добавить GUI-тесты для editor, parameter attachments, preview и восстановления
состояния.

## Этап 8. Форматы, упаковка и host QA

Сначала проверить:

- Standalone;
- VST3 в Reaper;
- AU в Logic;
- сохранение и восстановление состояния;
- automation параметров;
- CPU при восьми инстансах.

Затем добавить CLAP через `clap-wrapper` отдельным CMake-адаптером. DSP и
модель состояния не должны дублироваться для CLAP.

Упаковку macOS строить по модели `beat-equalizer`: отдельные targets,
копирование плагинов, подпись и документация по лицензии JUCE.

## Рекомендуемые PR

1. `chore: bootstrap pulse-designer build`
2. `feat(dsp): deterministic oscillator and envelopes`
3. `feat(dsp): noise generators and TPT filter`
4. `feat(dsp): saturation, oversampling and output stage`
5. `feat(dsp): voice engine and retrigger modes`
6. `feat(plugin): MIDI processor and APVTS`
7. `test: acceptance and block-size invariance`
8. `feat(plugin): presets and state persistence`
9. `feat(ui): waveform preview and control panel`
10. `feat(build): CLAP wrapper and host packaging`

Каждый PR должен оставлять `main` собираемой и иметь собственный проверяемый
gate. UI не подключается до прохождения DSP sound-quality gate.
