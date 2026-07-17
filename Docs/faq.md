# Frequently Asked Questions

This FAQ covers the most common questions about Seele Scatter Regions, an
open-source procedural world-building plugin for Unreal Engine 5.

## What does Seele Scatter Regions do?

It generates seeded village, farm, and cemetery layouts in the Unreal Editor.
Each layout can contain region-specific roads, boundaries, buildings, fields,
tombs, and props. Reusable recipe assets define which Static Meshes to use and
how densely to place them.

## Which Unreal Engine versions are supported?

Version 0.1.0 targets and is validated with Unreal Engine 5.5. Compatibility
with other Unreal Engine versions has not yet been validated.

## Does it require Unreal Engine's PCG framework?

No. The plugin implements a standalone, recipe-driven generator and does not
depend on the Unreal Engine PCG plugin. “Procedural generation” in this project
describes the workflow, not a dependency on the PCG framework.

## Can it generate content at game runtime?

Not in version 0.1.0. The public generator belongs to the Unreal Editor module.
Generated actors and `UInstancedStaticMeshComponent` output can be saved with
the edited level, but the repository does not expose generation as a packaged-
game runtime feature.

## Do I need a C++ Unreal project?

Yes. This is a source-code plugin with runtime and editor C++ modules. Install
it in a C++ Unreal project and compile the project after enabling the plugin.

## Can I call the generator from Blueprint or automation?

Yes, inside the editor. The plugin provides a Blueprint-callable Editor
Subsystem, a C++ generator, and a JSON adapter. See the
[Generation API](generation-api.md) and editable
[sample payloads](../Samples/CommandPayloads).

## Are generated layouts deterministic?

They are seeded. The same recipe, seed, parameters, and relevant scene state
produce repeatable placement. Changing a recipe, input parameter, Landscape,
or other traced scene state can change the result.

## Is a Landscape required?

Landscape projection is supported and reported through projection hit and miss
counts. If no Landscape is hit, the generator can retain local recipe height
and returns a warning, so review the structured result before accepting output.

## Does the repository include the art shown in the preview?

No. The showcase meshes, materials, and textures are not distributed. The
repository includes lightweight recipes based on Unreal Engine Basic Shapes so
you can test installation, then duplicate a recipe and assign your own assets.

## What regions are available?

Version 0.1.0 supports `Village`, `Farm`, and `Cemetery`. Region requests and
proposals are welcome in [GitHub Discussions](https://github.com/SeeleAI/seele-scatter-regions/discussions).

## Is the plugin free and open source?

Yes. The source is available under the [MIT License](../LICENSE). Review
[NOTICE.md](../NOTICE.md) for attribution and notice information.

## Where should I report a problem?

Open a [GitHub issue](https://github.com/SeeleAI/seele-scatter-regions/issues)
and include the Unreal Engine version, region type, seed, recipe asset path,
and returned warnings or errors.

---

[English README](../README.md) · [简体中文 FAQ](faq.zh-CN.md)
