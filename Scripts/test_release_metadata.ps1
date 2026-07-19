param(
    [string]$Root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path,
    [string]$ExpectedVersionName = '0.1.1',
    [string]$ExpectedEngineVersion = '5.8.0',
    [string]$ExpectedPlatform = 'Win64'
)

$ErrorActionPreference = 'Stop'

$Root = (Resolve-Path -LiteralPath $Root).Path
$descriptorPath = Join-Path $Root 'SeeleScatterRegions.uplugin'
$configPath = Join-Path $Root 'Config/DefaultSeeleScatterRegions.ini'

$descriptor = Get-Content -LiteralPath $descriptorPath -Raw | ConvertFrom-Json
$issues = @()

if ($descriptor.VersionName -ne $ExpectedVersionName) {
    $issues += "VersionName must be $ExpectedVersionName (found '$($descriptor.VersionName)')"
}
if ($descriptor.EngineVersion -ne $ExpectedEngineVersion) {
    $issues += "EngineVersion must be $ExpectedEngineVersion (found '$($descriptor.EngineVersion)')"
}
foreach ($module in $descriptor.Modules) {
    $platforms = @($module.PlatformAllowList)
    if ($platforms.Count -ne 1 -or $platforms[0] -ne $ExpectedPlatform) {
        $issues += "Module '$($module.Name)' must target only $ExpectedPlatform"
    }
}

$config = Get-Content -LiteralPath $configPath -Raw
if ($config -notmatch "(?m)^ReleaseVersion=$([regex]::Escape($ExpectedVersionName))\r?$") {
    $issues += "Config must declare ReleaseVersion=$ExpectedVersionName"
}
if ($config -notmatch "(?m)^SupportedEngineVersion=$([regex]::Escape($ExpectedEngineVersion))\r?$") {
    $issues += "Config must declare SupportedEngineVersion=$ExpectedEngineVersion"
}

if ($issues.Count -gt 0) {
    throw "Release metadata test failed: $($issues -join '; ')"
}

Write-Host "Release metadata test passed for version $ExpectedVersionName, engine $ExpectedEngineVersion, platform $ExpectedPlatform"
