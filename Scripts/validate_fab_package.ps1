param(
    [Parameter(Mandatory = $true)]
    [string]$PluginRoot,

    [string]$ZipPath,

    [string]$ExpectedVersionName = '0.1.1',

    [string]$ExpectedEngineVersion = '5.8.0',

    [string]$ExpectedPlatform = 'Win64'
)

$ErrorActionPreference = 'Stop'

$PluginRoot = (Resolve-Path -LiteralPath $PluginRoot).Path
$requiredFiles = @(
    'SeeleScatterRegions.uplugin',
    'Config/DefaultSeeleScatterRegions.ini',
    'Content/Recipes/DA_Village_Demo.uasset',
    'Content/Recipes/DA_Farm_Demo.uasset',
    'Content/Recipes/DA_Cemetery_Demo.uasset',
    'Resources/Icon128.png',
    'Source/SeeleScatterRegions/SeeleScatterRegions.Build.cs',
    'Source/SeeleScatterRegionsEditor/SeeleScatterRegionsEditor.Build.cs',
    'Docs/quickstart.md',
    'Samples/CommandPayloads/village.json',
    'README.md'
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
if ($descriptor.VersionName -ne $ExpectedVersionName) {
    $descriptorIssues += "VersionName must be $ExpectedVersionName (found '$($descriptor.VersionName)')"
}
if ($descriptor.EngineVersion -ne $ExpectedEngineVersion) {
    $descriptorIssues += "EngineVersion must be $ExpectedEngineVersion (found '$($descriptor.EngineVersion)')"
}
if ($descriptor.CanContainContent -ne $true) {
    $descriptorIssues += 'CanContainContent must be true'
}
if ($descriptor.Installed -ne $true) {
    $descriptorIssues += 'Installed must be true in the UAT package'
}
foreach ($module in $descriptor.Modules) {
    $platforms = @($module.PlatformAllowList)
    if ($platforms.Count -ne 1 -or $platforms[0] -ne $ExpectedPlatform) {
        $descriptorIssues += "Module '$($module.Name)' must set PlatformAllowList to $ExpectedPlatform"
    }
}
if ($descriptorIssues.Count -gt 0) {
    throw "Fab plugin descriptor is invalid: $($descriptorIssues -join '; ')"
}

$releaseConfig = Get-Content -LiteralPath (Join-Path $PluginRoot 'Config/DefaultSeeleScatterRegions.ini') -Raw
if ($releaseConfig -notmatch "(?m)^ReleaseVersion=$([regex]::Escape($ExpectedVersionName))\r?$") {
    throw "Fab plugin config must declare ReleaseVersion=$ExpectedVersionName"
}
if ($releaseConfig -notmatch "(?m)^SupportedEngineVersion=$([regex]::Escape($ExpectedEngineVersion))\r?$") {
    throw "Fab plugin config must declare SupportedEngineVersion=$ExpectedEngineVersion"
}

$blockedPaths = @(
    '.git',
    'Binaries',
    'Build',
    'Intermediate',
    'Saved',
    'HostProject',
    'Docs/superpowers',
    'LICENSE'
)
$presentBlocked = @(
    $blockedPaths | Where-Object { Test-Path -LiteralPath (Join-Path $PluginRoot $_) }
)
if ($presentBlocked.Count -gt 0) {
    throw "Fab plugin package contains blocked paths: $($presentBlocked -join ', ')"
}

$generatedExtensions = @('.dll', '.pdb', '.lib', '.exp', '.obj')
$generatedFiles = @(
    Get-ChildItem -LiteralPath $PluginRoot -Recurse -File | Where-Object {
        $_.Extension -in $generatedExtensions
    } | ForEach-Object {
        $_.FullName.Substring($PluginRoot.TrimEnd('\').Length + 1)
    }
)
if ($generatedFiles.Count -gt 0) {
    throw "Fab plugin package contains generated files: $($generatedFiles -join ', ')"
}

if ($ZipPath) {
    $ZipPath = (Resolve-Path -LiteralPath $ZipPath).Path
    $engineLabel = "UE$(($ExpectedEngineVersion -split '\.')[0..1] -join '.')"
    $expectedZipName = "SeeleScatterRegions-v$ExpectedVersionName-$engineLabel-$ExpectedPlatform.zip"
    if ([System.IO.Path]::GetFileName($ZipPath) -ne $expectedZipName) {
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
            $_.StartsWith('SeeleScatterRegions/Docs/superpowers/', [System.StringComparison]::OrdinalIgnoreCase) -or
            $_ -match '^SeeleScatterRegions/(Binaries|Build|Intermediate|Saved)/' -or
            $_ -ieq 'SeeleScatterRegions/LICENSE'
        })
        if ($blockedZipEntries.Count -gt 0) {
            throw "Fab ZIP contains files rejected by Fab review: $($blockedZipEntries -join ', ')"
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
