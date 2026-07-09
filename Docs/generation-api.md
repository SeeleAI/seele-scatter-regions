# Generation API

## C++

Use `FScatterRegionGenerator` from the editor module:

```cpp
FScatterRegionGenerator Generator;
TSharedPtr<FJsonObject> Result = Generator.GenerateFromJson(Params);
```

`Params` must contain:

- `region_type`: `village`, `farm`, or `cemetery`
- `center`: `[x, y, z]` in centimeters
- `size_m`: region size in meters
- `seed`: optional integer seed
- `recipe_asset`: object path to a `ScatterRegionRecipeDataAsset`

## JSON Adapter

Use `FScatterRegionJsonAdapter::ExecuteJsonCommand` when integrating with
automation code. The adapter accepts a command name and a JSON parameter string.

Supported command:

```text
generate_scatter_region
```

## Result Fields

Results include:

- `success`
- `command`
- `generator`
- `region_type`
- `seed`
- `warnings`
- `errors`
- `region_actor`
- `recipe_asset`
- `instance_count`
- `component_count`
- `projection_attempts`
- `projection_hits`
- `projection_misses`
- `slot_counts`
- `bounds`
