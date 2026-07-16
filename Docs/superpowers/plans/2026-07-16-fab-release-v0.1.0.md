# Fab v0.1.0 Release Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Produce, validate, publish, and expose a stable Fab review ZIP for Seele Scatter Regions v0.1.0 on Unreal Engine 5.5 Win64.

**Architecture:** Keep the generator C++ unchanged. Add only Fab-facing plugin metadata, minimal real Unreal content, package filtering, an icon, and repeatable validation/build scripts. Build with UE 5.5 UAT, test the packaged plugin in a clean host, then publish the exact verified ZIP as a GitHub Release asset.

**Tech Stack:** Unreal Engine 5.5, Unreal Automation Tool, PowerShell, Unreal Editor Python, Git, GitHub Releases.

---

### Task 1: Turn Fab package requirements into executable checks

**Files:**
- Modify: `Scripts/validate_public_package.ps1`
- Create: `Scripts/validate_fab_package.ps1`

- [ ] **Step 1: Add checks for `Content`, `Resources/Icon128.png`, content-enabled descriptor metadata, release documentation filters, and a single-plugin packaged ZIP layout.**
- [ ] **Step 2: Run the validators and confirm they fail because the required release structure is absent.**
- [ ] **Step 3: Keep the checks minimal and report every missing path or invalid descriptor field in one run.**

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\Scripts\validate_public_package.ps1
```

Expected: non-zero exit with missing `Content`, `Resources/Icon128.png`, or `CanContainContent` evidence.

### Task 2: Add Fab-facing plugin structure

**Files:**
- Modify: `SeeleScatterRegions.uplugin`
- Modify: `Config/FilterPlugin.ini`
- Create: `Resources/Icon128.png`
- Create through Unreal: `Content/Recipes/DA_Village_Demo.uasset`
- Create through Unreal: `Content/Recipes/DA_Farm_Demo.uasset`
- Create through Unreal: `Content/Recipes/DA_Cemetery_Demo.uasset`
- Modify: `README.md`
- Modify: `Docs/quickstart.md`

- [ ] **Step 1: Set `CanContainContent` to true and declare the UE 5.5 engine version.**
- [ ] **Step 2: Package README, license, notices, docs, and samples via `FilterPlugin.ini`.**
- [ ] **Step 3: Add a 128x128 plugin icon derived from the existing product visual language.**
- [ ] **Step 4: Build/load the plugin in an isolated UE workspace and create three saved recipe assets with Engine Basic Shapes so no third-party showcase content is redistributed.**
- [ ] **Step 5: Document the included demo recipes and repeat the public-package validator until it passes.**

### Task 3: Build and inspect the UE 5.5 package

**Files:**
- Modify: `Scripts/build_plugin.ps1`
- Create: `Scripts/build_fab_release.ps1`
- Generated, ignored: `Package/`

- [ ] **Step 1: Build with `C:\Program Files\Epic Games\UE_5.5\Engine\Build\BatchFiles\RunUAT.bat`.**
- [ ] **Step 2: Verify the packaged descriptor, binaries, source, config, content, resources, documentation, and samples.**
- [ ] **Step 3: Create `SeeleScatterRegions-v0.1.0-UE5.5-Win64.zip` with exactly one plugin root.**
- [ ] **Step 4: Run the Fab package validator against the ZIP and confirm zero missing or blocked paths.**

### Task 4: Clean-host validation

**Files:**
- Generated outside the repository: isolated UE 5.5 host workspace and logs

- [ ] **Step 1: Install the packaged plugin into one clean UE 5.5 C++ host workspace.**
- [ ] **Step 2: Compile/load the plugin, confirm the three recipe assets are registered, and run a small seeded generation probe.**
- [ ] **Step 3: Capture fresh build/editor log evidence and confirm no plugin load or compile errors.**

### Task 5: Publish the verified artifact

**Files:**
- Git commit/tag: `v0.1.0`
- GitHub Release asset: `SeeleScatterRegions-v0.1.0-UE5.5-Win64.zip`

- [ ] **Step 1: Review the complete diff and rerun all validators and the UAT build.**
- [ ] **Step 2: Commit the release changes, fast-forward `main`, and push the commit.**
- [ ] **Step 3: Create and push the annotated `v0.1.0` tag.**
- [ ] **Step 4: Create the GitHub Release, upload the verified ZIP, and verify the unauthenticated direct-download URL.**
- [ ] **Step 5: Report the exact Fab Version Title, <=255-character Version Notes, and Project File Link.**
