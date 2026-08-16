# GrainLatch Validation Evidence

Validated on 2026-08-17 in `/Users/2bit/prog/juce/GrainLatch` on macOS arm64 with local JUCE source `/Users/2bit/prog/juce/Plitch/build/release/_deps/juce-src`.

## Commands Run

```sh
cmake --preset engine-debug
cmake --build --preset engine-debug
ctest --preset engine-debug --output-on-failure

cmake --preset plugin-release -DEHL_JUCE_SOURCE_DIR=/Users/2bit/prog/juce/Plitch/build/release/_deps/juce-src -DEHL_COPY_PLUGIN_AFTER_BUILD=OFF
cmake --build --preset plugin-release
ctest --preset plugin-release --output-on-failure

python3 /Users/2bit/.agents/skills/develop-juce-plugins/scripts/check_juce_project.py /Users/2bit/prog/juce/GrainLatch --expect-product GrainLatch --expect-manufacturer-code EHL_ --expect-bundle-prefix jp.ehl. --strict
python3 /Users/2bit/.codex/skills/develop-ehl-plugins/scripts/check_public_text.py --history /Users/2bit/prog/juce/GrainLatch

codesign --verify --deep --strict artifacts/plugin-release/macos-arm64/vst3/grainlatch_vst3_plugin.vst3
codesign --verify --deep --strict artifacts/plugin-release/macos-arm64/au/grainlatch_au_plugin.component
codesign --verify --deep --strict artifacts/plugin-release/macos-arm64/standalone/grainlatch_standalone_plugin.app

/Users/2bit/Library/Caches/AgentPluginTester/releases/v0.1.0/macos-arm64/install/app/agent_plugin_host.app/Contents/MacOS/AgentPluginHost --inspect-plugin artifacts/plugin-release/macos-arm64/vst3/grainlatch_vst3_plugin.vst3
/Users/2bit/Library/Caches/AgentPluginTester/releases/v0.1.0/macos-arm64/install/app/agent_plugin_host.app/Contents/MacOS/AgentPluginHost --mode offline --no-gui --plugin artifacts/plugin-release/macos-arm64/vst3/grainlatch_vst3_plugin.vst3 --source sine --frequency 440 --level-db -18 --sample-rate 48000 --block-size 256 --run-seconds 2 --timeout-seconds 30 --record /private/tmp/grainlatch-host.wav --report /private/tmp/grainlatch-host.json --events /private/tmp/grainlatch-host.ndjson --fail-on non-finite,load-error
```

## Results

- Engine CTest: `1/1` passed.
- Plugin CTest: `4/4` passed: DSP, plugin integration, hosted VST3, artifact checks.
- JUCE metadata guard: passed for `GrainLatch`, manufacturer code `EHL_`, bundle prefix `jp.ehl.`.
- Public text guard: passed.
- Codesign verification: staged VST3, AU, and Standalone bundles passed `--deep --strict`.
- AgentPluginHost inspect: loaded one VST3 effect named `GrainLatch`, manufacturer `EsionHsrahLatigid`, version `0.1.0`, category `Fx`, not an instrument.
- AgentPluginHost offline render: passed, wrote `/private/tmp/grainlatch-host.wav` plus JSON/NDJSON reports. Output RMS `0.081765621900558`, peak `0.22214113175869`, DC offset `0.000007090801318554441`, zero crossings `42652`, nonfinite count `0`, clipped samples `0`.

## Release Artifacts

- `artifacts/plugin-release/macos-arm64/vst3/grainlatch_vst3_plugin.vst3`
- `artifacts/plugin-release/macos-arm64/au/grainlatch_au_plugin.component`
- `artifacts/plugin-release/macos-arm64/standalone/grainlatch_standalone_plugin.app`
- `artifacts/plugin-release/macos-arm64/ARTIFACTS.txt`

`build/` and `artifacts/` are intentionally gitignored generated outputs.
