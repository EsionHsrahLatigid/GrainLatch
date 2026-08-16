# GrainLatch

GrainLatch is a JUCE CMake EHL effect plugin for live granular latch, freeze, and buffer fracture.

## Identity

- Product: `GrainLatch`
- Bundle ID: `jp.ehl.grainlatch`
- Manufacturer: `EsionHsrahLatigid`
- Manufacturer code: `EHL_`
- Plug-in code: `GrLt`
- Formats: VST3, Standalone, AU on Apple

## Working Set

- DSP core: `Source/dsp/GranularCore.*`
- JUCE wrapper: `Source/PluginProcessor.*`
- Editor: `Source/PluginEditor.*`
- Tests: `Tests/`
- Release staging: `artifacts/plugin-release/<platform>/`
- Shared design module: `modules/juce-ehl-design-module`

## Current Status

Implemented as a bounded live capture ring with a fixed grain voice pool. Silence remains silent unless Freeze holds captured grain material. The editor uses the shared monochrome EHL chrome and a functional grain-field display.
