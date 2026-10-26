# RE-SYNTH

Native JUCE-based additive/residual resynthesis instrument — Windows VST3 and macOS VST3/AU.

## Status
The native CI build workflow is configured. **The C++ source files are not yet present in this repository.** To compile the installers, add the project files from `RE-SYNTH-v0.5.1-installer-build-project.zip` at the repository root (not nested in another RE-SYNTH directory).

Expected paths:

- `CMakeLists.txt`
- `Source/PluginProcessor.cpp`, `Source/PluginEditor.cpp` and other source files
- `tests/`
- `scripts/`
- `packaging/windows/RE-SYNTH.iss`
- `.github/workflows/native-plugins.yml` (already configured)

The GitHub Actions workflow builds the Windows VST3 and macOS VST3/AU and packages developer-test installers. It can be triggered manually under **Actions → Build RE-SYNTH installers** once the source files are present. The packages are development builds and are not commercially code-signed or notarized.
