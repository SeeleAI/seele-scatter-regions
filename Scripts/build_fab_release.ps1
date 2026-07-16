param(
    [string]$RunUAT = 'C:\Program Files\Epic Games\UE_5.5\Engine\Build\BatchFiles\RunUAT.bat',

    [string]$ArtifactRoot = (Join-Path $env:LOCALAPPDATA 'SeeleScatterRegions\Fab\v0.1.0'),

    [string]$ZipPath
)

$ErrorActionPreference = 'Stop'

$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if (-not $ZipPath) {
    $ZipPath = Join-Path $root 'SeeleScatterRegions-v0.1.0-UE5.5-Win64.zip'
}
elseif (-not [System.IO.Path]::IsPathRooted($ZipPath)) {
    $ZipPath = Join-Path $root $ZipPath
}
$ZipPath = [System.IO.Path]::GetFullPath($ZipPath)
$ArtifactRoot = [System.IO.Path]::GetFullPath($ArtifactRoot)

$expectedZipName = 'SeeleScatterRegions-v0.1.0-UE5.5-Win64.zip'
if ([System.IO.Path]::GetFileName($ZipPath) -ne $expectedZipName) {
    throw "ZipPath must use the release filename $expectedZipName"
}

$safeArtifactBase = [System.IO.Path]::GetFullPath((Join-Path $env:LOCALAPPDATA 'SeeleScatterRegions\Fab'))
if (-not $ArtifactRoot.StartsWith($safeArtifactBase + [System.IO.Path]::DirectorySeparatorChar, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "ArtifactRoot must stay under $safeArtifactBase"
}

& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'validate_public_package.ps1') -Root $root
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

if (Test-Path -LiteralPath $ArtifactRoot) {
    Remove-Item -LiteralPath $ArtifactRoot -Recurse -Force
}
New-Item -ItemType Directory -Path $ArtifactRoot -Force | Out-Null

$pluginBuild = Join-Path $ArtifactRoot 'PluginBuild'
& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'build_plugin.ps1') `
    -RunUAT $RunUAT `
    -PackageDir $pluginBuild
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

$stagingRoot = Join-Path $ArtifactRoot 'Staging'
$stagedPlugin = Join-Path $stagingRoot 'SeeleScatterRegions'
New-Item -ItemType Directory -Path $stagedPlugin -Force | Out-Null
Get-ChildItem -LiteralPath $pluginBuild -Force | Copy-Item -Destination $stagedPlugin -Recurse -Force

if (Test-Path -LiteralPath $ZipPath) {
    Remove-Item -LiteralPath $ZipPath -Force
}
Compress-Archive -LiteralPath $stagedPlugin -DestinationPath $ZipPath -CompressionLevel Optimal

& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'validate_fab_package.ps1') `
    -PluginRoot $stagedPlugin `
    -ZipPath $ZipPath
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

$hash = Get-FileHash -LiteralPath $ZipPath -Algorithm SHA256
Write-Host "FAB_RELEASE_ZIP=$ZipPath"
Write-Host "FAB_RELEASE_SHA256=$($hash.Hash)"
