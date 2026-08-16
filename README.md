# GrainLatch

GrainLatch is an EsionHsrahLatigid live granular latch, freeze, and buffer-fracture effect for VST3, AU, and Standalone hosts.

It captures incoming audio into a bounded preallocated ring and emits short grains from the live, latched, or held source. The public control set is grain size, density, pitch, position, dispersion, latch, freeze, feedback, mix, and output. Silence remains silent unless Freeze is holding captured material.

## Research Basis

The design follows the live-computation turn in Barry Truax, "Real-Time Granular Synthesis with a Digital Signal Processor," Computer Music Journal 12(2), 1988, pp. 14-26, and the Simon Fraser POD bibliography at https://www.sfu.ca/~truax/pod.html. The grain controls also use Curtis Roads' `Microsound` corpus framing: grain duration, density, waveform/envelope, start-time scatter, and fill factor jointly define whether grains read as rhythm, flutter, texture, or continuous mass. The SFU microsound tutorial notes the historical distinction between synthetic granular synthesis and sampled-sound granulation: https://www.sfu.ca/sonic-studio-webdav/cmns/Handbook%20Tutorial/Microsound.html.

GrainLatch is differentiated from generic offline granular tools by its source-aware live capture state: it distinguishes empty silence, active capture, held freeze, and recovery from weak wet output.

## Parameters

- `grainMs`: 3.0..220.0 ms, default 38.0
- `density`: 1.0..220.0 grains/s, default 42.0
- `pitch`: -24..24 semitones, default 0.0
- `position`: 0..1 capture offset, default 0.35
- `dispersion`: 0..1 timing/pitch/stride spread, default 0.22
- `latch`: hold the current traversal anchor, default off
- `freeze`: live capture hold, default off
- `feedback`: 0..0.92, default 0.18
- `mix`: 0..1, default 0.75
- `output`: -24..12 dB, default 0.0

## Build

```sh
cmake --preset engine-debug
cmake --build --preset engine-debug
ctest --preset engine-debug --output-on-failure

cmake --preset plugin-release
cmake --build --preset plugin-release
ctest --preset plugin-release --output-on-failure
```

Set `EHL_JUCE_SOURCE_DIR=/absolute/path/to/JUCE` to use a local JUCE checkout. Local macOS plugin builds default `EHL_COPY_PLUGIN_AFTER_BUILD=ON` for VST3/AU developer install; CI and non-macOS default it off. Staged release products are written under `artifacts/plugin-release/<platform>/`.

## License Notes

JUCE 8 modules are dual-licensed under AGPLv3 and the commercial JUCE license. Review the JUCE 8.0.15 license and choose a compatible distribution path before shipping binaries. This repository does not vendor JUCE or grant a JUCE commercial license.
