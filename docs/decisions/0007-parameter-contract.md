# Parameter contract and state

## Decision

Публичные параметры заведены в APVTS с version hint `1`. ID ниже стабильны:
их нельзя переименовывать или переиспользовать для другого смысла.

| ID | Диапазон | По умолчанию | Шкала |
|---|---|---|---|
| `osc_waveform` | Sine / Triangle / Square | Sine | choice |
| `osc_pitch` | 20–2000 Гц | 55 Гц | log |
| `osc_pitch_env_amount` | −48…+48 st | +36 | linear |
| `osc_pitch_env_decay` | 1–500 мс | 40 мс | log |
| `osc_pitch_env_curve` | 0–1 | 0.8 | linear |
| `osc_start_phase` | 0–360° | 90° | linear |
| `osc_amp_attack` | 0–50 мс | 0 | skew, центр 10 мс |
| `osc_amp_decay` | 5–3000 мс | 400 мс | log |
| `osc_amp_curve` | 0–1 | 0.85 | linear |
| `osc_shape` | 0–1 | 0 | linear |
| `noise_type` | White / Pink / Metallic / S&H | White | choice |
| `noise_filter_morph` | 0–1 | 0.5 | linear |
| `noise_cutoff` | 20–20000 Гц | 2000 Гц | log |
| `noise_resonance` | 0–1 | 0.2 | linear |
| `noise_filter_env_amount` | −100…+100 % | 0 | linear |
| `noise_filter_env_decay` | 1–500 мс | 60 мс | log |
| `noise_amp_attack` | 0–50 мс | 0 | skew, центр 10 мс |
| `noise_amp_decay` | 2–2000 мс | 150 мс | log |
| `noise_amp_curve` | 0–1 | 0.85 | linear |
| `noise_bursts` | 1–4 | 1 | int |
| `noise_burst_spacing` | 5–40 мс | 12 мс | linear |
| `mix` | 0–1 | 0.5 | linear |
| `drive_type` | Soft / Hard / Asymmetric / Fold | Soft | choice |
| `drive` | 0–1 | 0 | linear |
| `oversampling` | 1x / 2x / 4x / 8x | 4x | choice |
| `tone` | −1…+1 | 0 | linear |
| `gain` | −96…+12 дБ | 0 дБ | skew, центр −12 дБ; −96 — `-inf` |
| `pan` | −1…+1 | 0 | linear |
| `vel_level` | 0–100 % | 70 % | linear |
| `vel_pitch_env` | 0–100 % | 20 % | linear |
| `vel_cutoff` | 0–100 % | 0 % | linear |
| `vel_curve` | 0–1 | 0.5 | linear |
| `note_filter_mode` | Omni / Note / Range | Omni | choice |
| `note_filter_note` | 0–127 | 36 | int, имя ноты (C3 = 60) |
| `note_filter_low` | 0–127 | 0 | int |
| `note_filter_high` | 0–127 | 127 | int |
| `key_tracking` | 0–100 % | 0 % | linear |
| `voice_mode` | Restart / Overlap | Restart | choice |
| `voices` | 1–8 | 1 | int |

Проценты хранятся в APVTS как 0–100 и делятся на 100 в `SynthConfig`.
Порядок пунктов choice — часть контракта: значение хранится индексом.
Новый пункт добавляется только в конец списка.

`pulse::ParameterSnapshot` один раз берёт указатели на атомарные значения
APVTS. `read()` собирает `SynthConfig` без lock и аллокаций.
`processBlock` берёт snapshot в начале каждого блока. Движок latch-ит его на
каждом `note-on`, поэтому изменение параметра не трогает звучащий голос.
Preview будет пользоваться тем же snapshot.

State — XML APVTS с атрибутом `stateVersion = 1`.

- Чужой тег или повреждённые данные игнорируются.
- Параметр, которого нет в сохранённом state, получает default. Это делает
  сама APVTS, тест фиксирует поведение.
- Неизвестные ID игнорируются.
- State без `stateVersion` читается как версия 1, потому что раньше публичных
  параметров не было.
- Будущая версия формата добавляет миграцию в `setStateInformation` до
  `replaceState`.

## Reason

Defaults совпадают со спецификацией и `SynthConfig {}`, и тест сверяет каждое
поле. Если любая из сторон разойдётся, тест упадёт. Логарифмические шкалы
времени и частоты дают одинаковую точность ручки на каждой октаве.

## Consequence

Внутренние `sampleAndHoldRateHz` и `noiseSeed` не публикуются. Коды
`Puls` / `Pdsn` всё ещё provisional, но после первого публичного релиза их
смена сломает сохранённые проекты так же, как смена ID.
