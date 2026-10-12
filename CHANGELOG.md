# Changelog

All notable changes to Pulse Designer are documented here.

## [0.1.0] - Unreleased

- Added the initial CMake/JUCE/Catch2 project skeleton.
- Added a minimal MIDI instrument plugin with VST3, AU and Standalone targets.
- Added the first host-independent `SynthEngine` with oscillator, amp envelope,
  offline one-shot rendering and sample-accurate MIDI events.
- Added an eight-voice pool with restart and overlap voice modes and a Note
  Filter.
- Fixed retrigger so every hit returns to full level without a click.
- Added the public parameter set with stable IDs, a lock-free snapshot per
  block and versioned plugin state.
