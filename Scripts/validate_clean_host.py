import json
import unreal


RECIPE_PATHS = [
    "/SeeleScatterRegions/Recipes/DA_Village_Demo.DA_Village_Demo",
    "/SeeleScatterRegions/Recipes/DA_Farm_Demo.DA_Farm_Demo",
    "/SeeleScatterRegions/Recipes/DA_Cemetery_Demo.DA_Cemetery_Demo",
]


def main():
    recipes = []
    for object_path in RECIPE_PATHS:
        asset = unreal.load_asset(object_path)
        if asset is None:
            raise RuntimeError(f"Packaged recipe asset was not loadable: {object_path}")
        recipes.append(
            {
                "path": object_path,
                "region_type": str(asset.get_editor_property("region_type")),
                "building_slots": len(asset.get_editor_property("building_slots")),
                "boundary_slots": len(asset.get_editor_property("boundary_slots")),
                "density_slots": len(asset.get_editor_property("density_slots")),
            }
        )

    subsystem = unreal.get_editor_subsystem(unreal.ScatterRegionEditorSubsystem)
    if subsystem is None:
        raise RuntimeError("ScatterRegionEditorSubsystem was not available")

    params = {
        "region_type": "village",
        "center": [0.0, 0.0, 0.0],
        "size_m": 30.0,
        "seed": 76101,
        "recipe_asset": RECIPE_PATHS[0],
    }
    command_result = subsystem.execute_scatter_region_json_command(
        "generate_scatter_region",
        json.dumps(params),
    )
    if isinstance(command_result, str):
        command_ok = True
        result_json = command_result
    elif isinstance(command_result, tuple) and len(command_result) == 2:
        command_ok, result_json = command_result
    else:
        raise RuntimeError(f"Unexpected subsystem return value: {command_result!r}")
    result = json.loads(result_json)
    if not command_ok or not result.get("success"):
        raise RuntimeError(f"Generation failed: {result_json}")
    if int(result.get("instance_count", 0)) <= 0:
        raise RuntimeError(f"Generation returned no instances: {result_json}")
    if result.get("errors"):
        raise RuntimeError(f"Generation returned errors: {result_json}")

    evidence = {
        "recipes": recipes,
        "generation": {
            "success": result.get("success"),
            "region_type": result.get("region_type"),
            "seed": result.get("seed"),
            "region_actor": result.get("region_actor"),
            "instance_count": result.get("instance_count"),
            "component_count": result.get("component_count"),
            "projection_attempts": result.get("projection_attempts"),
            "projection_hits": result.get("projection_hits"),
            "projection_misses": result.get("projection_misses"),
            "warnings": result.get("warnings"),
        },
    }
    print("SEELE_SCATTER_CLEAN_HOST=" + json.dumps(evidence, ensure_ascii=True, sort_keys=True))


if __name__ == "__main__":
    main()
