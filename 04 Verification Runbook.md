# GrainLatch Verification Runbook

## Local Commands

```sh
python3 /Users/2bit/.agents/skills/develop-juce-plugins/scripts/check_juce_project.py . --expect-product GrainLatch --expect-manufacturer-code EHL_ --expect-bundle-prefix jp.ehl. --strict
cmake --preset engine-debug
cmake --build --preset engine-debug
ctest --preset engine-debug --output-on-failure
cmake --preset plugin-release
cmake --build --preset plugin-release
ctest --preset plugin-release --output-on-failure
python3 /Users/2bit/.codex/skills/develop-ehl-plugins/scripts/check_public_text.py --history .
```

## Required Evidence

- DSP tests pass for silence, deterministic render, freeze hold, reset, and extremes.
- Integration tests pass for parameter ranges, state round-trip, editor child IDs, and finite processing.
- Hosted VST3 load test passes and reports non-silent, finite output.
- Artifact check sees staged Standalone, VST3, AU on Apple, manifest, microphone plist, and valid ad-hoc signatures.
- AgentPluginHost offline render passes when the host launcher is available.

## Artifact Paths

- `artifacts/plugin-release/macos-arm64/vst3/grainlatch_vst3_plugin.vst3`
- `artifacts/plugin-release/macos-arm64/au/grainlatch_au_plugin.component`
- `artifacts/plugin-release/macos-arm64/standalone/grainlatch_standalone_plugin.app`
- `artifacts/plugin-release/macos-arm64/ARTIFACTS.txt`
