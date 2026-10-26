#!/usr/bin/env bash
# Build an unsigned developer-test .pkg after a successful macOS JUCE VST3+AU build.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VERSION="0.5.1"
VST="$ROOT/build-macos/ReSynth_artefacts/Release/VST3/RE-SYNTH.vst3"
AU="$ROOT/build-macos/ReSynth_artefacts/Release/AU/RE-SYNTH.component"
OUT="$ROOT/installers"
STAGE="$ROOT/build-macos/installer-stage"
[[ "$(uname -s)" == "Darwin" ]] || { echo 'macOS is required for pkgbuild' >&2; exit 1; }
[[ -d "$VST" && -d "$AU" ]] || { echo 'Compiled VST3 and AU bundles missing. Run scripts/build-macos.sh first.' >&2; exit 1; }
[[ -d "$VST/Contents" && -d "$AU/Contents" ]] || { echo 'A compiled plugin bundle has no Contents directory.' >&2; exit 1; }
command -v pkgbuild >/dev/null || { echo 'pkgbuild is required (Xcode command-line tools).' >&2; exit 1; }
rm -rf "$STAGE"
mkdir -p "$STAGE/Library/Audio/Plug-Ins/VST3" "$STAGE/Library/Audio/Plug-Ins/Components" "$OUT"
ditto "$VST" "$STAGE/Library/Audio/Plug-Ins/VST3/RE-SYNTH.vst3"
ditto "$AU" "$STAGE/Library/Audio/Plug-Ins/Components/RE-SYNTH.component"
# Ad-hoc signing of the plugin bundles is used only for local development builds.
# Not a substitute for Developer ID signing and Apple notarization.
codesign --force --deep --sign - "$STAGE/Library/Audio/Plug-Ins/VST3/RE-SYNTH.vst3"
codesign --force --deep --sign - "$STAGE/Library/Audio/Plug-Ins/Components/RE-SYNTH.component"
PKG="$OUT/RE-SYNTH-Installer-macOS.pkg"
rm -f "$PKG"
pkgbuild --root "$STAGE" \
  --install-location / \
  --identifier com.resynth.instrument.installer \
  --version "$VERSION" \
  --ownership recommended \
  "$PKG"
[[ -s "$PKG" ]] || { echo 'pkgbuild did not create the expected installer.' >&2; exit 1; }
pkgutil --check-signature "$PKG" || true
printf 'Created unsigned macOS development installer: %s\n' "$PKG"
