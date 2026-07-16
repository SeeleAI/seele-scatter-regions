import json
import unreal


PACKAGE_PATH = "/SeeleScatterRegions/Recipes"
RECIPE_CLASS_PATH = "/Script/SeeleScatterRegions.ScatterRegionRecipeDataAsset"


def load_required_asset(path):
    asset = unreal.load_asset(path)
    if asset is None:
        raise RuntimeError(f"Required engine asset was not found: {path}")
    return asset


def make_mesh_recipe(mesh, scale=(1.0, 1.0, 1.0), weight=1.0):
    value = unreal.ScatterRegionMeshRecipeData()
    value.set_editor_property("mesh", mesh)
    value.set_editor_property("scale_min", unreal.Vector(*scale))
    value.set_editor_property("scale_max", unreal.Vector(*scale))
    value.set_editor_property("uniform_scale", True)
    value.set_editor_property("weight", weight)
    return value


def make_single_mesh_recipe(mesh, scale=(1.0, 1.0, 1.0)):
    value = unreal.ScatterRegionSingleMeshRecipeData()
    value.set_editor_property("mesh", mesh)
    value.set_editor_property("scale_min", unreal.Vector(*scale))
    value.set_editor_property("scale_max", unreal.Vector(*scale))
    value.set_editor_property("uniform_scale", True)
    return value


def set_village_recipe(asset, meshes):
    buildings = list(asset.get_editor_property("building_slots"))
    for index, slot in enumerate(buildings):
        slot.set_editor_property("enabled", True)
        slot.set_editor_property("weight", 1.0)
        slot.set_editor_property("main_building", make_mesh_recipe(meshes["cube"], (1.0, 1.0, 1.5)))
        slot.set_editor_property("local_props", [make_mesh_recipe(meshes["cylinder"], (0.35, 0.35, 0.7))])
        buildings[index] = slot
    asset.set_editor_property("building_slots", buildings)

    boundaries = list(asset.get_editor_property("boundary_slots"))
    for index, slot in enumerate(boundaries):
        slot.set_editor_property("enabled", True)
        slot.set_editor_property("mesh", make_single_mesh_recipe(meshes["cube"], (0.15, 1.0, 0.4)))
        boundaries[index] = slot
    asset.set_editor_property("boundary_slots", boundaries)

    road = asset.get_editor_property("road_slot")
    road.set_editor_property("enabled", True)
    road.set_editor_property("mesh", make_single_mesh_recipe(meshes["plane"], (1.0, 1.0, 1.0)))
    asset.set_editor_property("road_slot", road)

    densities = list(asset.get_editor_property("density_slots"))
    density_meshes = [meshes["cylinder"], meshes["sphere"]]
    for index, slot in enumerate(densities):
        slot.set_editor_property("enabled", True)
        slot.set_editor_property("meshes", [make_mesh_recipe(density_meshes[index], (0.25, 0.25, 0.5))])
        densities[index] = slot
    asset.set_editor_property("density_slots", densities)


def set_farm_recipe(asset, meshes):
    road = asset.get_editor_property("road_slot")
    road.set_editor_property("enabled", True)
    road.set_editor_property("mesh", make_single_mesh_recipe(meshes["plane"]))
    asset.set_editor_property("road_slot", road)

    densities = list(asset.get_editor_property("density_slots"))
    density_meshes = [meshes["cube"], meshes["cylinder"], meshes["cone"]]
    density_scales = [(0.8, 0.8, 0.1), (0.15, 0.15, 0.8), (0.3, 0.3, 0.8)]
    for index, slot in enumerate(densities):
        slot.set_editor_property("enabled", True)
        slot.set_editor_property("meshes", [make_mesh_recipe(density_meshes[index], density_scales[index])])
        densities[index] = slot
    asset.set_editor_property("density_slots", densities)


def set_cemetery_recipe(asset, meshes):
    road = asset.get_editor_property("road_slot")
    road.set_editor_property("enabled", True)
    road.set_editor_property("mesh", make_single_mesh_recipe(meshes["plane"]))
    asset.set_editor_property("road_slot", road)

    densities = list(asset.get_editor_property("density_slots"))
    density_meshes = [meshes["cube"], meshes["cone"]]
    density_scales = [(0.25, 0.12, 0.6), (0.7, 0.7, 1.0)]
    for index, slot in enumerate(densities):
        slot.set_editor_property("enabled", True)
        slot.set_editor_property("meshes", [make_mesh_recipe(density_meshes[index], density_scales[index])])
        densities[index] = slot
    asset.set_editor_property("density_slots", densities)


def main():
    recipe_class = unreal.load_class(None, RECIPE_CLASS_PATH)
    if recipe_class is None:
        raise RuntimeError(f"Recipe class was not loaded: {RECIPE_CLASS_PATH}")

    meshes = {
        "cube": load_required_asset("/Engine/BasicShapes/Cube.Cube"),
        "cylinder": load_required_asset("/Engine/BasicShapes/Cylinder.Cylinder"),
        "sphere": load_required_asset("/Engine/BasicShapes/Sphere.Sphere"),
        "cone": load_required_asset("/Engine/BasicShapes/Cone.Cone"),
        "plane": load_required_asset("/Engine/BasicShapes/Plane.Plane"),
    }

    specs = [
        ("DA_Village_Demo", unreal.ScatterRegionRecipeType.VILLAGE, set_village_recipe),
        ("DA_Farm_Demo", unreal.ScatterRegionRecipeType.FARM, set_farm_recipe),
        ("DA_Cemetery_Demo", unreal.ScatterRegionRecipeType.CEMETERY, set_cemetery_recipe),
    ]

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    created = []
    with unreal.ScopedEditorTransaction("Create Seele Scatter Regions demo recipes"):
        for asset_name, region_type, configure in specs:
            object_path = f"{PACKAGE_PATH}/{asset_name}.{asset_name}"
            if unreal.EditorAssetLibrary.does_asset_exist(object_path):
                raise RuntimeError(f"Refusing to overwrite existing asset: {object_path}")

            factory = unreal.DataAssetFactory()
            factory.set_editor_property("data_asset_class", recipe_class)
            asset = asset_tools.create_asset(asset_name, PACKAGE_PATH, recipe_class, factory)
            if asset is None:
                raise RuntimeError(f"Failed to create asset: {object_path}")

            asset.set_editor_property("region_type", region_type)
            configure(asset, meshes)
            if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
                raise RuntimeError(f"Failed to save asset: {object_path}")

            loaded = unreal.load_asset(object_path)
            if loaded is None:
                raise RuntimeError(f"Saved asset could not be loaded: {object_path}")
            created.append(
                {
                    "path": object_path,
                    "region_type": str(loaded.get_editor_property("region_type")),
                    "building_slots": len(loaded.get_editor_property("building_slots")),
                    "boundary_slots": len(loaded.get_editor_property("boundary_slots")),
                    "density_slots": len(loaded.get_editor_property("density_slots")),
                }
            )

    print("SEELE_SCATTER_DEMO_RECIPES=" + json.dumps(created, ensure_ascii=True, sort_keys=True))


if __name__ == "__main__":
    main()
