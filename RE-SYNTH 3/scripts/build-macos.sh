#!/usr/bin/env bash
# Requires macOS with Xcode CLI tools, CMake, and a local JUCE 8 source checkout.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
: "${JUCE_DIR:?Set JUCE_DIR to the JUCE 8 source directory}"
[[ -f "$JUCE_DIR/CMakeLists.txt" ]] || { echo "Invalid JUCE_DIR" >&2; exit 1; }
BUILD="$ROOT/build-macos"
DIST="$ROOT/dist-macos"
cmake -S "$ROOT" -B "$BUILD" -G Xcode -DJUCE_DIR="$JUCE_DIR" \
  -DCMAKE_OSX_ARCHITECTURES='arm64;x86_64' \
  -DCMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED=NO
cmake --build "$BUILD" --config Release --target ReSynth_VST3 ReSynth_AU --parallel 3
mkdir -p "$DIST/VST3" "$DIST/Components"
VST="$BUILD/ReSynth_artefacts/Release/VST3/RE-SYNTH.vst3"
AU="$BUILD/ReSynth_artefacts/Release/AU/RE-SYNTH.component"
[[ -d "$VST" && -d "$AU" ]] || { echo "Expected plugin bundles missing" >&2; exit 1; }
ditto "$VST" "$DIST/VST3/RE-SYNTH.vst3"
ditto "$AU" "$DIST/Components/RE-SYNTH.component"
# Ad hoc signatures are usable for LOCAL TESTS only; commercial distribution
# requires a Developer ID and notarization.
codesign --force --deep --sign - "$DIST/VST3/RE-SYNTH.vst3"
codesign --force --deep --sign - "$DIST/Components/RE-SYNTH.component"
ditto -c -k --sequesterRsrc --keepParent "$DIST" "$ROOT/RE-SYNTH-macOS-VST3-AU.zip"
echo "Prepared local-test VST3/AU archive: $ROOT/RE-SYNTH-macOS-VST3-AU.zip"
