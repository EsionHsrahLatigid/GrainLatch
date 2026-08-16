# GrainLatch Design

GrainLatch uses the shared `juce-ehl-design-module` pinned as `modules/juce-ehl-design-module`. The compact header renders the canonical short `ehl` mark through `EHL::JuceDesign`; product title and effect class remain clean operational text.

The editor is a 512 x 320 monochrome 8-bit surface. Its main readout is a functional 40 x 12 grain-field matrix sourced from the audio core through atomic snapshots.

Controls: Size, Density, Jitter, Reverse, Stutter, Damage, Mix, Output, Freeze, Retrigger. No chromatic accents, glow, fake hardware, waveform logo, or decorative glitch text are used.
