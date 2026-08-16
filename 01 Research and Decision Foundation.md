# GrainLatch Research and Decision Foundation

Sources: Barry Truax real-time granular synthesis, Simon Fraser microsound tutorial, Curtis Roads `Microsound`, and JUCE 8.0.15 CMake/plugin behavior from local EHL JUCE convention.

Decisions: fixed four-second capture ring, fixed 64-voice grain pool, deterministic sample-count scheduling, Freeze requires prior captured material, Recovery reports weak wet output under live input, and extremes are bounded by denormal cleanup, DC blocking, tanh shaping, and hard ceiling.

Parameter contract: `grainMs`, `density`, `jitter`, `reverse`, `stutter`, `freeze`, `retrigger`, `damage`, `mix`, `output`.
