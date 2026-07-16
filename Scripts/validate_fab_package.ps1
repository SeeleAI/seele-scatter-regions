param(
    [Parameter(Mandatory = $true)]
    [string]$PluginRoot,

    [string]$ZipPath
)

$ErrorActionPreference = 'Stop'

$PluginRoot = (Resolve-Path -LiteralPath $PluginRoot).Path
$requiredFiles = @(
    'SeeleScatterRegions.uplugin',
    'Binaries/Win64/UnrealEditor-SeeleScatterRegions.dll',
    'Binaries/Win64/UnrealEditor-SeeleScatterRegionsEditor.dll',
    'Config/DefaultSeeleScatterRegions.ini',
    'Content/Recipes/DA_Village_Demo.uasset',
    'Content/Recipes/DA_Farm_Demo.uasset',
    'Content/Recipes/DA_Cemetery_Demo.uasset',
    'Resources/Icon128.png',
    'Source/SeeleScatterRegions/SeeleScatterRegions.Build.cs',
    'Source/SeeleScatterRegionsEditor/SeeleScatterRegionsEditor.Build.cs',
    'Docs/quickstart.md',
    'Samples/CommandPayloads/village.json',
    'README.md',
    'LICENSE'
)

$missing = @(
    $requiredFiles | Where-Object {
        -not (Test-Path -LiteralPath (Join-Path $PluginRoot $_) -PathType Leaf)
    }
)
if ($missing.Count -gt 0) {
    throw "Fab plugin package is missing files: $($missing -join ', ')"
}

$descriptorPath = Join-Path $PluginRoot 'SeeleScatterRegions.uplugin'
$descriptor = Get-Content -LiteralPath $descriptorPath -Raw | ConvertFrom-Json
$descriptorIssues = @()
if ($descriptor.VersionName -ne '0.1.0') {
    $descriptorIssues += "VersionName must be 0.1.0 (found '$($descriptor.VersionName)')"
}
if ($descriptor.EngineVersion -ne '5.5.0') {
    $descriptorIssues += "EngineVersion must be 5.5.0 (found '$($descriptor.EngineVersion)')"
}
if ($descriptor.CanContainContent -ne $true) {
    $descriptorIssues += 'CanContainContent must be true'
}
if ($descriptor.Installed -ne $true) {
    $descriptorIssues += 'Installed must be true in the UAT package'
}
if ($descriptorIssues.Count -gt 0) {
    throw "Fab plugin descriptor is invalid: $($descriptorIssues -join '; ')"
}

$blockedPaths = @('.git', 'Saved', 'HostProject', 'Docs/superpowers')
$presentBlocked = @(
    $blockedPaths | Where-Object { Test-Path -LiteralPath (Join-Path $PluginRoot $_) }
)
if ($presentBlocked.Count -gt 0) {
    throw "Fab plugin package contains blocked paths: $($presentBlocked -join ', ')"
}

if ($ZipPath) {
    $ZipPath = (Resolve-Path -LiteralPath $ZipPath).Path
    if ([System.IO.Path]::GetFileName($ZipPath) -ne 'SeeleScatterRegions-v0.1.0-UE5.5-Win64.zip') {
        throw "Unexpected Fab ZIP filename: $([System.IO.Path]::GetFileName($ZipPath))"
    }

    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $archive = [System.IO.Compression.ZipFile]::OpenRead($ZipPath)
    try {
        $entries = @($archive.Entries | ForEach-Object { $_.FullName.Replace('\', '/') })
        $outsideRoot = @($entries | Where-Object {
            $_ -and -not $_.StartsWith('SeeleScatterRegions/', [System.StringComparison]::Ordinal)
        })
        if ($outsideRoot.Count -gt 0) {
            throw "Fab ZIP contains entries outside the single plugin root: $($outsideRoot -join ', ')"
        }

        $unsafeEntries = @($entries | Where-Object {
            $_ -match '(^|/)\.\.(/|$)' -or $_.StartsWith('/', [System.StringComparison]::Ordinal)
        })
        if ($unsafeEntries.Count -gt 0) {
            throw "Fab ZIP contains unsafe paths: $($unsafeEntries -join ', ')"
        }

        $blockedZipEntries = @($entries | Where-Object {
            $_.StartsWith('SeeleScatterRegions/Docs/superpowers/', [System.StringComparison]::OrdinalIgnoreCase)
        })
        if ($blockedZipEntries.Count -gt 0) {
            throw "Fab ZIP contains internal planning files: $($blockedZipEntries -join ', ')"
        }

        $missingZipEntries = @(
            $requiredFiles | Where-Object {
                $expected = "SeeleScatterRegions/$($_.Replace('\', '/'))"
                $entries -notcontains $expected
            }
        )
        if ($missingZipEntries.Count -gt 0) {
            throw "Fab ZIP is missing entries: $($missingZipEntries -join ', ')"
        }
    }
    finally {
        $archive.Dispose()
    }
}

Write-Host "Fab package validation passed for $PluginRoot"
if ($ZipPath) {
    Write-Host "Fab ZIP validation passed for $ZipPath"
}
