param(
    [string]$ReleaseVersion = '0.1.2',

    [string]$EngineVersion = '5.8.0',

    [string]$TargetPlatform = 'Win64',

    [string]$RunUAT,

    [string]$ArtifactRoot,

    [string]$ZipPath
)

$ErrorActionPreference = 'Stop'

$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$engineMajorMinor = ($EngineVersion -split '\.')[0..1] -join '.'
$engineLabel = "UE$engineMajorMinor"
$expectedZipName = "SeeleScatterRegions-v$ReleaseVersion-$engineLabel-$TargetPlatform.zip"
if (-not $RunUAT) {
    $RunUAT = "C:\Program Files\Epic Games\UE_$engineMajorMinor\Engine\Build\BatchFiles\RunUAT.bat"
}
if (-not $ArtifactRoot) {
    $ArtifactRoot = Join-Path $env:LOCALAPPDATA "SeeleScatterRegions\Fab\v$ReleaseVersion-$engineLabel"
}
if (-not $ZipPath) {
    $ZipPath = Join-Path $root $expectedZipName
}
elseif (-not [System.IO.Path]::IsPathRooted($ZipPath)) {
    $ZipPath = Join-Path $root $ZipPath
}
$ZipPath = [System.IO.Path]::GetFullPath($ZipPath)
$ArtifactRoot = [System.IO.Path]::GetFullPath($ArtifactRoot)

if ([System.IO.Path]::GetFileName($ZipPath) -ne $expectedZipName) {
    throw "ZipPath must use the release filename $expectedZipName"
}

$safeArtifactBase = [System.IO.Path]::GetFullPath((Join-Path $env:LOCALAPPDATA 'SeeleScatterRegions\Fab'))
if (-not $ArtifactRoot.StartsWith($safeArtifactBase + [System.IO.Path]::DirectorySeparatorChar, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "ArtifactRoot must stay under $safeArtifactBase"
}

& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'validate_public_package.ps1') `
    -Root $root `
    -ExpectedVersionName $ReleaseVersion `
    -ExpectedEngineVersion $EngineVersion `
    -ExpectedPlatform $TargetPlatform
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

$fabExcludedPaths = @(
    'Binaries',
    'Build',
    'Intermediate',
    'Saved',
    'LICENSE',
    'NOTICE.md'
)
foreach ($relativePath in $fabExcludedPaths) {
    $candidate = Join-Path $stagedPlugin $relativePath
    if (Test-Path -LiteralPath $candidate) {
        Remove-Item -LiteralPath $candidate -Recurse -Force
    }
}

if (Test-Path -LiteralPath $ZipPath) {
    Remove-Item -LiteralPath $ZipPath -Force
}
Compress-Archive -LiteralPath $stagedPlugin -DestinationPath $ZipPath -CompressionLevel Optimal

& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'validate_fab_package.ps1') `
    -PluginRoot $stagedPlugin `
    -ZipPath $ZipPath `
    -ExpectedVersionName $ReleaseVersion `
    -ExpectedEngineVersion $EngineVersion `
    -ExpectedPlatform $TargetPlatform
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

$hash = Get-FileHash -LiteralPath $ZipPath -Algorithm SHA256
Write-Host "FAB_RELEASE_ZIP=$ZipPath"
Write-Host "FAB_RELEASE_SHA256=$($hash.Hash)"
