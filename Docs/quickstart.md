# Quickstart

## Install

Clone the plugin into a C++ Unreal project:

```powershell
cd <YourUnrealProject>\Plugins
git clone https://github.com/SeeleAI/seele-scatter-regions.git SeeleScatterRegions
```

Open the project, enable **Seele Scatter Regions**, and compile.

## Create a Recipe

Create a `ScatterRegionRecipeDataAsset` in the Content Browser. Pick one
region type:

- `Village`: building clusters, roads, fences, gates, and props.
- `Farm`: roads, dirt patches, crops, and scarecrows.
- `Cemetery`: roads, tombs, and memorial buildings.

Each slot accepts Static Mesh references plus scale and weight metadata.

Editable starter recipes are included under `/SeeleScatterRegions/Recipes`.
They use Unreal Engine Basic Shapes only, so you can verify the plugin without
redistributing the showcase environment assets. Duplicate a starter recipe
into your project content before adapting it for production use.

## Generate a Region

Use C++ or editor automation to call the generator with:

```json
{
  "region_type": "village",
  "center": [75000, 37000, 0],
  "size_m": 150,
  "seed": 76101,
  "recipe_asset": "/Game/MyRecipes/DA_Village.DA_Village"
}
```

The command name used by the JSON adapter is:

```text
generate_scatter_region
```

## Validate Output

Treat a run as complete only when:

- `success` is `true`
- `region_actor` is not empty
- `instance_count` is greater than zero
- `projection_hits` is greater than zero when projecting to Landscape
- `errors` is empty

Warnings should be reviewed because missing slots can produce sparse regions.
