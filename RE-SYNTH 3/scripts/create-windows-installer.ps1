# Build the Windows VST3 setup EXE from a successfully compiled JUCE bundle.
# Requirements: Windows, Inno Setup 6 (ISCC.exe), and the project's Release VST3 output.
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$version = '0.5.1'
$bundle = Join-Path $root 'build-windows/ReSynth_artefacts/Release/VST3/RE-SYNTH.vst3'
if (-not (Test-Path -LiteralPath $bundle -PathType Container)) {
    throw "Windows x64 VST3 bundle is missing: $bundle. Compile with scripts/build-windows.ps1 first."
}
$payload = Get-ChildItem -LiteralPath $bundle -File -Recurse -ErrorAction Stop
if (-not $payload -or -not ($payload | Where-Object { $_.Extension -eq '.vst3' })) {
    throw "VST3 bundle contains no compiled .vst3 binary: $bundle"
}

$paths = @(
    (Get-Command ISCC.exe -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source -ErrorAction SilentlyContinue),
    "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
    "$env:ProgramFiles\Inno Setup 6\ISCC.exe"
) | Where-Object { $_ -and (Test-Path -LiteralPath $_ -PathType Leaf) }
$iscc = $paths | Select-Object -First 1
if (-not $iscc) {
    throw 'Inno Setup ISCC.exe was not found. Install Inno Setup 6 (choco install innosetup -y).'
}
$out = Join-Path $root 'installers'
New-Item -ItemType Directory -Force -Path $out | Out-Null
$iss = Join-Path $root 'packaging/windows/RE-SYNTH.iss'
& $iscc "/DPluginSource=$bundle" "/DOutDir=$out" "/DReleaseVersion=$version" $iss
if ($LASTEXITCODE -ne 0) { throw "Inno Setup failed: $LASTEXITCODE" }
$exe = Join-Path $out 'RE-SYNTH-Setup-Windows.exe'
if (-not (Test-Path -LiteralPath $exe -PathType Leaf)) { throw "Expected installer not found: $exe" }
Write-Host "Windows installer created: $exe"
