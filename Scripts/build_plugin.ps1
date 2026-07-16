param(
    [Parameter(Mandatory = $true)]
    [string]$RunUAT,

    [string]$PackageDir = (Join-Path (Resolve-Path (Join-Path $PSScriptRoot '..')).Path 'Package')
)

$ErrorActionPreference = 'Stop'

$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$plugin = Join-Path $root 'SeeleScatterRegions.uplugin'

if (-not [System.IO.Path]::IsPathRooted($PackageDir)) {
    $PackageDir = Join-Path $root $PackageDir
}
$PackageDir = [System.IO.Path]::GetFullPath($PackageDir)

if (-not (Test-Path -LiteralPath $RunUAT)) {
    throw "RunUAT was not found: $RunUAT"
}

& $RunUAT BuildPlugin `
    -Plugin="$plugin" `
    -Package="$PackageDir" `
    -TargetPlatforms=Win64 `
    -StrictIncludes

if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}
