# Changelog

## 0.1.2

- Separates the MIT-licensed GitHub source boundary from Fab package licensing.
- Removes unused migration documentation and an unreferenced preview image.
- Replaces dangling and broad `FilterPlugin.ini` entries with an explicit Fab
  release file list.
- Extends release validation to reject repository license files, unused release
  content, invalid filter targets, and broken relative Markdown links in Fab
  packages.

## 0.1.1

- Improves GitHub discovery with a search-intent-focused README title,
  compatibility details, and direct FAQ answers.
- Adds a Simplified Chinese README and bilingual FAQ documentation.
- Expands plugin package metadata and includes localized README files in release
  packages.
- Adds Unreal Engine 5.8 and Win64 release metadata.
- Adds Fab-required platform allow lists and publisher copyright notices.
- Excludes generated Unreal build output and the repository license file from the Fab review ZIP.
- Adds validation that rejects generated binaries, unsupported platform metadata, and missing copyright notices.

## 0.1.0

- Initial public Unreal Engine plugin scaffold.
- Adds scatter-region recipe data assets for village, farm, and cemetery regions.
- Adds editor-side generation code and JSON command adapter.
- Adds editable village, farm, and cemetery starter recipes using Unreal Engine Basic Shapes.
- Adds a validated Unreal Engine 5.5 Win64 Fab release workflow and plugin package metadata.
