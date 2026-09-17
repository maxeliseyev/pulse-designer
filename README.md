# Pulse Designer

Pulse Designer — планируемый MIDI-инструментальный плагин для генерации
электронной перкуссии: kick, snare, tom, hat, clap, rim и zap.

Один инстанс — один инструмент и один звук. Сложные партии собираются в хосте
несколькими дорожками, а не встроенным секвенсором или мультитембральностью.

## Текущее состояние

Bootstrap-каркас этапа 1 и первый звуковой срез этапа 2 готовы. Проект собирает
VST3, AU и Standalone; MIDI note-on запускает детерминированный oscillator с amp
envelope, а noise-ветка уже поддерживает white/pink/metallic/S&H и TPT-фильтр.
Следующий шаг — noise bursts, pitch envelope и выходной nonlinear тракт.

- [Спецификация](docs/drum-synth-spec.md)
- [План реализации](docs/implementation-plan.md)
- Ориентир по стеку и организации — локальный репозиторий `beat-equalizer`.

## Идея инструмента

Два источника — осциллятор и шум — имеют независимые огибающие, смешиваются и
проходят через нелинейный выходной тракт:

```text
MIDI note-on
    ├── oscillator → pitch envelope → Shape → amp envelope ─┐
    └── noise → TPT filter → filter envelope → amp envelope ─┤
                                                            mix
                                                              ↓
                                                     Drive → DC → output
```

Параметры фиксируются в момент `note-on`. Обработка MIDI должна быть
сэмплово-точной, а огибающие — посэмпловыми и экспоненциальными.

## Запланированные форматы

- VST3;
- AU;
- CLAP через `clap-wrapper`;
- Standalone для разработки и проверки звука.

Плагин принимает MIDI и выдаёт стереоаудио. Аудиовходов, MIDI-выхода,
секвенсора, воспроизведения сэмплов, LFO, матрицы модуляций и choke-групп в
первой версии нет.

## Технологический стек

- C++20;
- JUCE 8.0.15;
- CMake 3.22+ и Ninja;
- Catch2 3.8.1;
- CMake `FetchContent` для внешних зависимостей;
- `Makefile` как короткая оболочка для сборки.

Структура проекта планируется такой:

```text
src/dsp/       синтез, voices, envelopes, filters, saturation
src/plugin/    JUCE AudioProcessor, APVTS, MIDI, editor
tests/         синтетические DSP-тесты и host/plugin smoke-тесты
docs/          спецификация, план, решения и handoff
packaging/     ресурсы и права для сборки macOS
scripts/       сборочные и упаковочные утилиты
```

Realtime-обработка и офлайн-preview waveform используют один `SynthEngine`.
В `processBlock` запрещены аллокации, блокировки и рендер интерфейса.

## План

Работа идёт от ядра к интерфейсу:

1. Зафиксировать parameter/state contracts и спорные звуковые решения.
2. Создать CMake/JUCE/Catch2-каркас и пустой MIDI-плагин.
3. Реализовать детерминированный офлайн-рендер.
4. Добавить фильтр, Shape, Drive, oversampling и выходной тракт.
5. Подключить voices и сэмплово-точную MIDI-обработку.
6. Пройти технические и звуковые acceptance gates.
7. Добавить APVTS, состояние, пресеты и waveform UI.
8. Проверить форматы и хосты, затем подключить CLAP.

Подробные deliverables и границы PR находятся в
[`docs/implementation-plan.md`](docs/implementation-plan.md).

## Проверки

Главные приёмочные свойства:

- идентичный output при размерах блока 32, 64, 512 и 2048;
- отсутствие заметного алиасинга при максимальном Drive;
- DC близок к нулю на заводских пресетах;
- отсутствие щелчка при ретриггере через 5 мс;
- одинаковое поведение на 44.1, 48, 96 и 192 кГц;
- приемлемая нагрузка при восьми инстансах и 4x oversampling.

Основные команды:

```bash
make              # Release: тесты и плагины
make debug        # Debug-сборка
make test         # DSP и интеграционные тесты
make standalone   # Standalone
make vst3         # VST3
make au           # AU на macOS
```

В ограниченной среде, где нельзя писать в системные plugin-папки, targets можно
собрать без копирования артефактов:

```bash
rtk cmake --preset release -DPULSE_DESIGNER_COPY_PLUGINS=OFF
rtk cmake --build --preset release --target PulseDesigner_VST3 PulseDesigner_AU PulseDesigner_Standalone
```

## Разработка

Ориентир по CMake, JUCE, тестам, упаковке и Git-процессу — локальный проект
`beat-equalizer`. Аудиозаписи для сравнений и реальные киты не коммитятся;
в репозитории должны оставаться только синтетические фикстуры и измеренные
числа.

До решения вопроса с лицензией JUCE проект не предназначен для распространения
закрытых бинарных сборок.
