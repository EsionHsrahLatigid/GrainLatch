# GrainLatch Research and Decision Foundation

## Sources Consulted

- Barry Truax, "Real-Time Granular Synthesis with a Digital Signal Processor," Computer Music Journal 12(2), 1988, pp. 14-26.
- Barry Truax POD bibliography: https://www.sfu.ca/~truax/pod.html
- Simon Fraser microsound tutorial: https://www.sfu.ca/sonic-studio-webdav/cmns/Handbook%20Tutorial/Microsound.html
- Curtis Roads, `Microsound`, granular synthesis chapters and corpus framing.
- JUCE 8.0.15 CMake/plugin behavior from the local EHL JUCE convention and the JUCE FetchContent pin in `CMakeLists.txt`.

## Decisions

- Use live sampled-sound granulation, not offline grain cloud rendering.
- Keep capture in a fixed four-second ring allocated inside `GranularCore`.
- Use a fixed 64-voice grain pool with deterministic oldest-voice stealing.
- Grain emission is sample-count based and seeded, so offline tests are repeatable.
- Freeze captures the current ring anchor and only emits if source material was captured first.
- Recovery is an explicit state when live input exists but wet energy has fallen too low.
- Extreme settings are intentionally harsh but guarded by denormal cleanup, DC blocking, tanh shaping, and a hard ceiling.

## Parameter Contract

- `grainMs`, `density`, `pitch`, `position`, `dispersion`
- `latch`, `freeze`
- `feedback`, `mix`, `output`

These IDs are versioned APVTS parameters and should be treated as stable after public release.

## Realtime Contract

`processBlock` calls only bounded sample processing. The core uses fixed arrays, no heap growth, no locks, no logging, no filesystem/network access, and no unbounded history scans on the audio path.
