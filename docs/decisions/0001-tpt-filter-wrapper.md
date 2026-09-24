# TPT filter wrapper

## Decision

Pulse Designer keeps a small host-independent TPT state-variable filter in
`src/dsp` instead of wrapping three JUCE `StateVariableTPTFilter` instances.

The filter follows the same TPT update equations used by JUCE's
`StateVariableTPTFilter` and exposes low-pass, band-pass and high-pass outputs
from one shared state. A continuous morph blends LP→BP for `0..0.5` and BP→HP
for `0.5..1`.

## Reason

The noise voice needs all three outputs on every sample. Three independent
filters would duplicate state and make the morph less predictable during fast
cutoff modulation. Keeping the wrapper in `src/dsp` also preserves the
host-independent `SynthEngine` contract used by realtime and offline rendering.

## Consequence

The wrapper must retain the numerical stability guarantees of JUCE's TPT
implementation. Its impulse, finite-output, morph and sample-rate tests belong
to the DSP test target.
