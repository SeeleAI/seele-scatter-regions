# Recipe Assets

`UScatterRegionRecipeDataAsset` defines the meshes and density settings used by
the generator.

The plugin includes editable starter recipes under
`/SeeleScatterRegions/Recipes`. They reference Unreal Engine Basic Shapes so
the package can be validated without bundling the showcase environment art.
Duplicate a starter recipe into project content before replacing its meshes.

## Shared Mesh Data

Each mesh entry contains:

- `Mesh`: Static Mesh reference.
- `ScaleMin`: minimum random scale.
- `ScaleMax`: maximum random scale.
- `bUniformScale`: use one uniform random scale value.
- `Weight`: relative selection weight when a slot has multiple meshes.

## Village

Village recipes use:

- `BuildingSlots`: main buildings plus local props.
- `BoundarySlots`: fence and gate.
- `RoadSlot`: road mesh.
- `DensitySlots`: side-road props and scatter props.

## Farm

Farm recipes use:

- `RoadSlot`
- `DensitySlots`: dirt, crops, scarecrow

## Cemetery

Cemetery recipes use:

- `RoadSlot`
- `DensitySlots`: tombs, panteon

The `panteon` slot spelling is preserved for compatibility with existing
content authored with that slot name.
