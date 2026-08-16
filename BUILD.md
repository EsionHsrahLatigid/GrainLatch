# Build Notes

The fast DSP path does not require JUCE:

```sh
cmake --preset engine-debug
cmake --build --preset engine-debug --parallel
ctest --preset engine-debug --output-on-failure
```

The plug-in path builds VST3 and Standalone everywhere, plus AU on Apple:

```sh
cmake --preset plugin-release -DEHL_JUCE_SOURCE_DIR=/Users/2bit/prog/juce/Plitch/build/release/_deps/juce-src
cmake --build --preset plugin-release --parallel
ctest --preset plugin-release --output-on-failure
```

Artifacts are staged under `artifacts/plugin-release/<platform>`.

