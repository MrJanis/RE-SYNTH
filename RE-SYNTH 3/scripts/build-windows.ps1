# PowerShell 7, Visual Studio 2022 C++ Build Tools, CMake, local JUCE 8 required.
$ErrorActionPreference = 'Stop'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if (-not $env:JUCE_DIR -or -not (Test-Path (Join-Path $env:JUCE_DIR 'CMakeLists.txt'))) {
    throw 'Set JUCE_DIR to an extracted JUCE 8 source directory.'
}
$build = Join-Path $project 'build-windows'
$dist = Join-Path $project 'dist-windows'
cmake -S $project -B $build -G "Visual Studio 17 2022" -A x64 "-DJUCE_DIR=$env:JUCE_DIR"
if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed' }
cmake --build $build --config Release --target ReSynth_VST3 --parallel 3
if ($LASTEXITCODE -ne 0) { throw 'VST3 build failed' }
$bundle = Join-Path $build 'ReSynth_artefacts/Release/VST3/RE-SYNTH.vst3'
if (-not (Test-Path $bundle)) { throw "Plugin bundle missing: $bundle" }
New-Item -ItemType Directory -Force (Join-Path $dist 'VST3') | Out-Null
Copy-Item $bundle (Join-Path $dist 'VST3') -Recurse -Force
$zip = Join-Path $project 'RE-SYNTH-Windows-VST3.zip'
if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path (Join-Path $dist 'VST3') -DestinationPath $zip -CompressionLevel Optimal
Write-Host "Prepared installable VST3 bundle: $zip"
