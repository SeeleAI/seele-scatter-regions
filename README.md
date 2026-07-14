# Seele Scatter Regions for Unreal Engine

<p align="center">
  <img src="Docs/images/seele-scatter-regions-banner.png" alt="Seele Scatter Regions procedural village, farm, and cemetery generation for Unreal Engine 5" width="1600">
</p>

<p align="center">
  <a href="https://www.unrealengine.com/"><img src="https://img.shields.io/badge/Unreal%20Engine-5.5-0E1128?style=for-the-badge&amp;logo=unrealengine&amp;logoColor=white" alt="Unreal Engine 5.5"></a>
  <a href="CHANGELOG.md"><img src="https://img.shields.io/badge/Beta-0.1.0-F59E0B?style=for-the-badge" alt="Beta 0.1.0"></a>
  <a href="Source"><img src="https://img.shields.io/badge/Language-C%2B%2B-00599C?style=for-the-badge&amp;logo=cplusplus&amp;logoColor=white" alt="C++"></a>
  <a href="LICENSE"><img src="https://img.shields.io/badge/License-MIT-2EA44F?style=for-the-badge" alt="MIT License"></a>
</p>

<p align="center">
  <a href="https://www.seeles.ai/features/create/unreal-game">Try Seele</a> &middot;
  <a href="Docs/quickstart.md">Quickstart</a> &middot;
  <a href="Docs/generation-api.md">Generation API</a> &middot;
  <a href="Docs/recipe-assets.md">Recipe Assets</a> &middot;
  <a href="Samples/CommandPayloads">Samples</a> &middot;
  <a href="CHANGELOG.md">Changelog</a>
</p>

**Seele Scatter Regions** is an open-source Unreal Engine 5 plugin for
generating naturally scattered villages, farms, and cemeteries from your own
Static Mesh assets.

Define a reusable recipe, choose a location, size, and seed, then generate
landscape-projected instanced content through C++, an Editor Subsystem, or JSON
automation.

## Try Seele for Unreal Engine

Create Unreal Engine scenes with Seele AI. Explore the
[Unreal game creation workflow](https://www.seeles.ai/features/create/unreal-game).

## What It Generates

| Region | Generated content |
| --- | --- |
| **Village** | Building clusters, local props, roads, fences, and gates |
| **Farm** | Roads, dirt patches, crop fields, and scarecrows |
| **Cemetery** | Paths, tombs, and memorial buildings |

## Key Features

- **Recipe-driven placement** - configure project-owned meshes, weights,
  density, and scale ranges in a reusable `ScatterRegionRecipeDataAsset`.
- **Seeded generation** - reproduce placement with the same recipe, seed, and
  scene state.
- **Landscape-aware output** - project placement points to Landscape and report
  projection attempts, hits, and misses.
- **Instanced rendering** - group generated meshes into
  `UInstancedStaticMeshComponent` components.
- **Multiple integration paths** - generate through C++, a Blueprint-callable
  Editor Subsystem, or the JSON command adapter.
- **Structured results** - inspect the generated actor, instance and component
  counts, slot counts, bounds, warnings, and errors.

## See It in Action

![Jiangnan scatter region preview](Docs/images/jiangnan-scatter-region-preview.png)

Jiangnan-style village, farm, and cemetery regions generated with
project-provided meshes and materials. Art assets shown in the image are not
included with the plugin.

## Quick Start

### Requirements

- Unreal Engine 5.5
- A C++ Unreal project
- Static Mesh assets for the region recipes you want to generate

### 1. Install the Plugin

Clone the repository into your Unreal project's `Plugins` directory:

```powershell
cd <YourUnrealProject>\Plugins
git clone https://github.com/SeeleAI/seele-scatter-regions.git SeeleScatterRegions
```

Open the project, enable **Seele Scatter Regions**, regenerate project files if
prompted, and compile the project.

### 2. Create a Recipe

In the Content Browser, create a `ScatterRegionRecipeDataAsset`, choose
`Village`, `Farm`, or `Cemetery`, and assign your Static Mesh assets to the
available slots.

See [Recipe Assets](Docs/recipe-assets.md) for slot definitions and mesh
settings.

### 3. Generate a Region

Provide a region type, world-space center in centimeters, size in meters, seed,
and recipe asset path:

```json
{
  "region_type": "village",
  "center": [75000, 37000, 0],
  "size_m": 150,
  "seed": 76101,
  "recipe_asset": "/Game/MyRecipes/DA_Village.DA_Village"
}
```

The JSON adapter command is:

```text
generate_scatter_region
```

Use the C++ generator, Editor Subsystem, or JSON adapter described in the
[Generation API](Docs/generation-api.md). Ready-to-edit payloads are available
for [village](Samples/CommandPayloads/village.json),
[farm](Samples/CommandPayloads/farm.json), and
[cemetery](Samples/CommandPayloads/cemetery.json) regions.

### 4. Validate the Result

Treat generation as successful when:

- `success` is `true`
- `region_actor` is not empty
- `instance_count` is greater than zero
- `projection_hits` is greater than zero when projecting to Landscape
- `errors` is empty

Review `warnings` and `projection_misses` before accepting sparse or partially
projected output.

## Public API

| API | Purpose |
| --- | --- |
| `UScatterRegionRecipeDataAsset` | Editable recipe asset for region type, meshes, density, weights, and scale ranges |
| `FScatterRegionGenerationSpec` | Region type, center, size, seed, and recipe asset input |
| `FScatterRegionGenerationResult` | Success state, generated actor, counts, projection hits, warnings, and errors |
| `FScatterRegionGenerator` | Editor-side C++ generator and command handler |
| `UScatterRegionEditorSubsystem` | Blueprint-callable editor wrapper for JSON commands |
| `FScatterRegionJsonAdapter` | JSON command integration for editor automation |

## Documentation

| Guide | Purpose |
| --- | --- |
| [Quickstart](Docs/quickstart.md) | Install the plugin, create a recipe, generate a region, and validate output |
| [Recipe Assets](Docs/recipe-assets.md) | Configure shared mesh data and region-specific slots |
| [Generation API](Docs/generation-api.md) | Integrate through C++ and JSON and inspect result fields |
| [Sample Payloads](Samples/CommandPayloads) | Start from village, farm, and cemetery JSON examples |
| [Changelog](CHANGELOG.md) | Review versioned changes |

## Bring Your Own Assets

This repository ships plugin source code only. It does not include the meshes,
materials, textures, or content packs shown in the preview. Create recipe assets
inside your Unreal project and assign your own Static Mesh content.

Generated build output, private automation tools, and unreleased content assets
are intentionally excluded from this repository.

## Project Status

Seele Scatter Regions is currently **beta software at version 0.1.0**. The
public generator runs in the Unreal Editor module and targets Unreal Engine 5.5.
Source builds require a C++ Unreal project.

## Contributing

Issues and pull requests are welcome. When reporting generation behavior,
include the Unreal Engine version, region type, seed, recipe asset path, and any
returned warnings or errors.

- [Open an issue](https://github.com/SeeleAI/seele-scatter-regions/issues)
- [View the source](https://github.com/SeeleAI/seele-scatter-regions)

## License

Seele Scatter Regions is available under the [MIT License](LICENSE). See
[NOTICE.md](NOTICE.md) for attribution and notice information.
