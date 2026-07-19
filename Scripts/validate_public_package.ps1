param(
    [string]$Root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path,

    [string]$ExpectedVersionName = '0.1.1',

    [string]$ExpectedEngineVersion = '5.8.0',

    [string]$ExpectedPlatform = 'Win64'
)

$ErrorActionPreference = 'Stop'

$requiredFiles = @(
    'README.md',
    'README.zh-CN.md',
    'LICENSE',
    'NOTICE.md',
    'CHANGELOG.md',
    'Config/DefaultSeeleScatterRegions.ini',
    'Resources/Icon128.png',
    '.gitignore',
    '.gitattributes',
    'SeeleScatterRegions.uplugin',
    'Source/SeeleScatterRegions/SeeleScatterRegions.Build.cs',
    'Source/SeeleScatterRegions/Public/ScatterRegionTypes.h',
    'Source/SeeleScatterRegions/Public/ScatterRegionRecipeDataAsset.h',
    'Source/SeeleScatterRegions/Public/ScatterRegionResult.h',
    'Source/SeeleScatterRegions/Private/SeeleScatterRegionsModule.cpp',
    'Source/SeeleScatterRegions/Private/ScatterRegionRecipeDataAsset.cpp',
    'Source/SeeleScatterRegionsEditor/SeeleScatterRegionsEditor.Build.cs',
    'Source/SeeleScatterRegionsEditor/Public/ScatterRegionGenerator.h',
    'Source/SeeleScatterRegionsEditor/Public/ScatterRegionEditorSubsystem.h',
    'Source/SeeleScatterRegionsEditor/Public/ScatterRegionJsonAdapter.h',
    'Source/SeeleScatterRegionsEditor/Private/SeeleScatterRegionsEditorModule.cpp',
    'Source/SeeleScatterRegionsEditor/Private/ScatterRegionGenerator.cpp',
    'Source/SeeleScatterRegionsEditor/Private/ScatterRegionEditorSubsystem.cpp',
    'Source/SeeleScatterRegionsEditor/Private/ScatterRegionRecipeCompiler.cpp',
    'Source/SeeleScatterRegionsEditor/Private/ScatterRegionPlacement.cpp',
    'Source/SeeleScatterRegionsEditor/Private/ScatterRegionRoadGraph.cpp',
    'Source/SeeleScatterRegionsEditor/Private/ScatterRegionLandscapeProjection.cpp',
    'Source/SeeleScatterRegionsEditor/Private/ScatterRegionActorBuilder.cpp',
    'Source/SeeleScatterRegionsEditor/Private/ScatterRegionJsonAdapter.cpp',
    'Docs/quickstart.md',
    'Docs/recipe-assets.md',
    'Docs/generation-api.md',
    'Docs/faq.md',
    'Docs/faq.zh-CN.md',
    'Docs/migration-from-irregular.md',
    'Content/Recipes/DA_Village_Demo.uasset',
    'Content/Recipes/DA_Farm_Demo.uasset',
    'Content/Recipes/DA_Cemetery_Demo.uasset',
    'Samples/CommandPayloads/village.json',
    'Samples/CommandPayloads/farm.json',
    'Samples/CommandPayloads/cemetery.json',
    'Scripts/build_plugin.ps1'
)

$missing = @()
foreach ($relativePath in $requiredFiles) {
    if (-not (Test-Path -LiteralPath (Join-Path $Root $relativePath))) {
        $missing += $relativePath
    }
}

if ($missing.Count -gt 0) {
    throw "Missing required public package files: $($missing -join ', ')"
}

$pluginDescriptorPath = Join-Path $Root 'SeeleScatterRegions.uplugin'
$pluginDescriptor = Get-Content -LiteralPath $pluginDescriptorPath -Raw | ConvertFrom-Json
$descriptorIssues = @()
if ($pluginDescriptor.CanContainContent -ne $true) {
    $descriptorIssues += 'SeeleScatterRegions.uplugin must set CanContainContent to true'
}
if ($pluginDescriptor.VersionName -ne $ExpectedVersionName) {
    $descriptorIssues += "SeeleScatterRegions.uplugin must set VersionName to $ExpectedVersionName"
}
if ($pluginDescriptor.EngineVersion -ne $ExpectedEngineVersion) {
    $descriptorIssues += "SeeleScatterRegions.uplugin must set EngineVersion to $ExpectedEngineVersion"
}
if (-not $pluginDescriptor.Modules -or $pluginDescriptor.Modules.Count -lt 1) {
    $descriptorIssues += 'SeeleScatterRegions.uplugin must declare at least one code module'
}
foreach ($module in $pluginDescriptor.Modules) {
    $platforms = @($module.PlatformAllowList)
    if ($platforms.Count -ne 1 -or $platforms[0] -ne $ExpectedPlatform) {
        $descriptorIssues += "Module '$($module.Name)' must set PlatformAllowList to $ExpectedPlatform"
    }
}
if ($descriptorIssues.Count -gt 0) {
    throw "Invalid Fab plugin descriptor: $($descriptorIssues -join '; ')"
}

$copyrightNotice = '// Copyright 2026 QUANLING SHENZHEN NETWORK CO., LTD. All Rights Reserved.'
$codeFiles = Get-ChildItem -LiteralPath (Join-Path $Root 'Source') -Recurse -File |
    Where-Object { $_.Extension -in @('.h', '.cpp', '.cs') }
$missingCopyright = @(
    $codeFiles | Where-Object {
        (Get-Content -LiteralPath $_.FullName -TotalCount 1) -ne $copyrightNotice
    } | ForEach-Object {
        $_.FullName.Substring($Root.TrimEnd('\').Length + 1)
    }
)
if ($missingCopyright.Count -gt 0) {
    throw "Source files missing the Fab copyright notice: $($missingCopyright -join ', ')"
}

$releaseConfig = Get-Content -LiteralPath (Join-Path $Root 'Config/DefaultSeeleScatterRegions.ini') -Raw
if ($releaseConfig -notmatch "(?m)^ReleaseVersion=$([regex]::Escape($ExpectedVersionName))\r?$") {
    throw "Config/DefaultSeeleScatterRegions.ini must declare ReleaseVersion=$ExpectedVersionName"
}
if ($releaseConfig -notmatch "(?m)^SupportedEngineVersion=$([regex]::Escape($ExpectedEngineVersion))\r?$") {
    throw "Config/DefaultSeeleScatterRegions.ini must declare SupportedEngineVersion=$ExpectedEngineVersion"
}

$filterPath = Join-Path $Root 'Config/FilterPlugin.ini'
$filterContent = Get-Content -LiteralPath $filterPath -Raw
$requiredFilterEntries = @(
    '/README*.md',
    '/LICENSE',
    '/NOTICE.md',
    '/CHANGELOG.md',
    '/Config/...',
    '/Docs/*.md',
    '/Docs/images/...',
    '/Samples/...'
)
$missingFilterEntries = @(
    $requiredFilterEntries | Where-Object { $filterContent -notmatch [regex]::Escape($_) }
)
if ($missingFilterEntries.Count -gt 0) {
    throw "Config/FilterPlugin.ini is missing release entries: $($missingFilterEntries -join ', ')"
}

$blockedDirectories = @(
    'Binaries',
    'Intermediate',
    'Saved',
    '.claude',
    'skills-server',
    'Content/ExternalAssets',
    'EmbeddedContent',
    'Package'
)

$presentBlocked = @()
foreach ($relativePath in $blockedDirectories) {
    if (Test-Path -LiteralPath (Join-Path $Root $relativePath)) {
        $presentBlocked += $relativePath
    }
}

if ($presentBlocked.Count -gt 0) {
    throw "Blocked private/generated directories found: $($presentBlocked -join ', ')"
}

$textFiles = Get-ChildItem -LiteralPath $Root -Recurse -File |
    Where-Object {
        $_.FullName -notmatch '\\.git\\' -and
        $_.FullName -ne $PSCommandPath -and
        $_.Extension -notin @('.png', '.uasset', '.umap', '.dll', '.pdb', '.lib', '.exe', '.zip')
    }

$blockedPatterns = @(
    'skills-server',
    '\.claude',
    'asset-retrieval',
    'ue_tools\.py',
    'unreal-mcp-status',
    'MCPUnrealProject',
    'SimpleGame',
    'http://127\.0\.0\.1',
    'gh[pousr]_[A-Za-z0-9_]+'
)

$hits = @()
foreach ($file in $textFiles) {
    $content = Get-Content -LiteralPath $file.FullName -Raw
    foreach ($pattern in $blockedPatterns) {
        if ($content -match $pattern) {
            $relative = Resolve-Path -LiteralPath $file.FullName -Relative
            $hits += "$relative matches /$pattern/"
        }
    }
}

if ($hits.Count -gt 0) {
    throw "Blocked internal/private references found: $($hits -join '; ')"
}

Write-Host "Public package validation passed for $Root"
