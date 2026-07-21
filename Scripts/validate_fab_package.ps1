param(
    [Parameter(Mandatory = $true)]
    [string]$PluginRoot,

    [string]$ZipPath,

    [string]$ExpectedVersionName = '0.1.2',

    [string]$ExpectedEngineVersion = '5.8.0',

    [string]$ExpectedPlatform = 'Win64'
)

$ErrorActionPreference = 'Stop'

$PluginRoot = (Resolve-Path -LiteralPath $PluginRoot).Path
$requiredFiles = @(
    'SeeleScatterRegions.uplugin',
    'Config/DefaultSeeleScatterRegions.ini',
    'Config/FilterPlugin.ini',
    'Content/Recipes/DA_Village_Demo.uasset',
    'Content/Recipes/DA_Farm_Demo.uasset',
    'Content/Recipes/DA_Cemetery_Demo.uasset',
    'Resources/Icon128.png',
    'Source/SeeleScatterRegions/SeeleScatterRegions.Build.cs',
    'Source/SeeleScatterRegionsEditor/SeeleScatterRegionsEditor.Build.cs',
    'Docs/faq.md',
    'Docs/faq.zh-CN.md',
    'Docs/generation-api.md',
    'Docs/quickstart.md',
    'Docs/recipe-assets.md',
    'Docs/images/seele-scatter-regions-banner.png',
    'Samples/CommandPayloads/village.json',
    'Samples/CommandPayloads/farm.json',
    'Samples/CommandPayloads/cemetery.json',
    'README.md',
    'README.zh-CN.md',
    'CHANGELOG.md'
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
    'Docs/plans',
    'LICENSE',
    'NOTICE.md',
    'Docs/migration-from-irregular.md',
    'Docs/images/jiangnan-scatter-region-preview.png'
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

$filterPath = Join-Path $PluginRoot 'Config/FilterPlugin.ini'
$filterEntries = @(
    Get-Content -LiteralPath $filterPath | ForEach-Object { $_.Trim() } | Where-Object {
        $_ -and -not $_.StartsWith(';') -and -not $_.StartsWith('#') -and -not $_.StartsWith('[')
    }
)
$requiredFilterEntries = @(
    '/README.md',
    '/README.zh-CN.md',
    '/CHANGELOG.md',
    '/Config/DefaultSeeleScatterRegions.ini',
    '/Config/FilterPlugin.ini',
    '/Docs/faq.md',
    '/Docs/faq.zh-CN.md',
    '/Docs/generation-api.md',
    '/Docs/quickstart.md',
    '/Docs/recipe-assets.md',
    '/Docs/images/seele-scatter-regions-banner.png',
    '/Samples/...'
)
$filterIssues = @()
$filterIssues += @(
    $requiredFilterEntries | Where-Object { $_ -notin $filterEntries } | ForEach-Object {
        "missing entry $_"
    }
)
$filterIssues += @(
    $filterEntries | Where-Object { $_ -notin $requiredFilterEntries } | ForEach-Object {
        "unexpected entry $_"
    }
)
foreach ($entry in $filterEntries) {
    $relativePath = $entry.TrimStart('/').Replace('/', [System.IO.Path]::DirectorySeparatorChar)
    if ($relativePath.EndsWith([System.IO.Path]::DirectorySeparatorChar + '...')) {
        $relativePath = $relativePath.Substring(0, $relativePath.Length - 4)
        $directory = Join-Path $PluginRoot $relativePath
        if (-not (Test-Path -LiteralPath $directory -PathType Container) -or
            -not (Get-ChildItem -LiteralPath $directory -Recurse -File -ErrorAction SilentlyContinue)) {
            $filterIssues += "directory entry matches no files: $entry"
        }
    }
    elseif (-not (Test-Path -LiteralPath (Join-Path $PluginRoot $relativePath) -PathType Leaf)) {
        $filterIssues += "file entry does not exist: $entry"
    }
}
if ($filterIssues.Count -gt 0) {
    throw "Config/FilterPlugin.ini is invalid for Fab: $($filterIssues -join '; ')"
}

$brokenMarkdownLinks = @()
$markdownLinkPattern = '(?:\[[^\]]*\]\(([^)]+)\)|<(?:a|img)\b[^>]+(?:href|src)="([^"]+)")'
foreach ($markdownFile in Get-ChildItem -LiteralPath $PluginRoot -Recurse -File -Filter '*.md') {
    $content = Get-Content -LiteralPath $markdownFile.FullName -Raw
    foreach ($match in [regex]::Matches($content, $markdownLinkPattern, [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)) {
        $target = if ($match.Groups[1].Success) { $match.Groups[1].Value } else { $match.Groups[2].Value }
        $target = $target.Trim().Trim('<', '>')
        if (-not $target -or $target -match '^(?:https?://|mailto:|#)') {
            continue
        }
        $targetPath = ($target -split '[#?]', 2)[0]
        if ($targetPath -match '\s+["'']') {
            $targetPath = ($targetPath -split '\s+', 2)[0]
        }
        $resolvedTarget = [System.IO.Path]::GetFullPath((Join-Path $markdownFile.DirectoryName $targetPath))
        if (-not $resolvedTarget.StartsWith($PluginRoot + [System.IO.Path]::DirectorySeparatorChar, [System.StringComparison]::OrdinalIgnoreCase) -or
            -not (Test-Path -LiteralPath $resolvedTarget)) {
            $relativeMarkdown = $markdownFile.FullName.Substring($PluginRoot.TrimEnd('\').Length + 1)
            $brokenMarkdownLinks += "$relativeMarkdown -> $target"
        }
    }
}
if ($brokenMarkdownLinks.Count -gt 0) {
    throw "Fab plugin package contains broken relative Markdown links: $($brokenMarkdownLinks -join '; ')"
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
            $_.StartsWith('SeeleScatterRegions/Docs/plans/', [System.StringComparison]::OrdinalIgnoreCase) -or
            $_ -match '^SeeleScatterRegions/(Binaries|Build|Intermediate|Saved)/' -or
            $_ -ieq 'SeeleScatterRegions/LICENSE' -or
            $_ -ieq 'SeeleScatterRegions/NOTICE.md' -or
            $_ -ieq 'SeeleScatterRegions/Docs/migration-from-irregular.md' -or
            $_ -ieq 'SeeleScatterRegions/Docs/images/jiangnan-scatter-region-preview.png'
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
