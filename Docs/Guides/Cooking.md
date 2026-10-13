# Cooking Guide <!--! {#quiet_guide_cooking} -->

This guide covers preparing an editable project, cooking its runtime
package, and launching that package through Quiet_Sandbox.

Commands use PowerShell from the repository root unless stated otherwise.

## Prerequisites

- An editable project with AssetRegistry.json.
- A saved startup Scene.
- Available source files for registered project assets.
- Cooker and Sandbox builds.
- The runtime resources described in
  [Getting Started](../GettingStarted.md).

See [Projects](Projects.md), [Assets](Assets.md), and [Editor](Editor.md)
for project preparation.

## Prepare the project

1. Open the project in the Editor.
2. Stop Play or Simulate.
3. Save the intended startup Scene.
4. Open File → Project Manager....
5. Under Project Scenes, select the intended Scene.
6. Select Set As Startup.
7. Check asset diagnostics and resolve missing sources.
8. Save external Lua edits and verify runtime behavior in the Editor.

The Cooker uses the descriptor's startup Scene, not whichever Scene
happens to be open in the Editor.

It processes registered project meshes, textures, audio, and scripts,
rather than collecting only the assets referenced by that Scene.

Script packaging does not validate Lua syntax. Test scripts before
packaging.

## Build Cooker and Sandbox

```powershell
cmake --preset debug-cooker
cmake --build --preset build-debug-cooker

cmake --preset debug-sandbox
cmake --build --preset build-debug-sandbox
```

Expected outputs:

- Build/Binaries/Debug/Quiet_Cooker.exe
- Build/Binaries/Debug/Quiet_Sandbox.exe
- Runtime Resources beside the executables.

Initial Cooker configuration includes its conversion dependencies.

## Choose input and output paths

The Cooker accepts exactly two arguments:

```text
Quiet_Cooker <project.quiet> <output-directory>
```

Use a dedicated package output directory, separate from the editable
project and unrelated files.

Cooking overwrites outputs and can prune unreferenced files with
recognized packaged-asset extensions.

Example paths for FirstProject:

```powershell
$projectFile = Join-Path $env:USERPROFILE `
    "Documents\QuietProjects\FirstProject\FirstProject.quiet"

$packageRoot = Join-Path $env:USERPROFILE `
    "Documents\QuietPackages\FirstProject"
```

Replace these values with your project's descriptor and intended package
directory. Keep both variables available for the following commands.

## Run the Cooker

```powershell
& .\Build\Binaries\Debug\Quiet_Cooker.exe $projectFile $packageRoot

if ($LASTEXITCODE -ne 0) {
    throw "Cooking failed. Review the Cooker diagnostics before launching."
}
```

Expected result: the CLI reports successful asset stages and finishes
with Asset cooking completed successfully.

Review warnings even when exit code is zero. Stale-file cleanup or cache
saving can fail without failing package finalization.

A failed invocation can leave outputs behind. Directory presence alone
does not establish a completed package.

## Package contents

| Path | Contents |
| --- | --- |
| RuntimeManifest.json | Project name, startup Scene path, and runtime asset records |
| Scenes/ | Copied startup Scene |
| Content/Meshes/ | QMesh files |
| Content/Textures/ | BC7 sRGB QTexture files |
| Content/Audio/ | Copied WAV, FLAC, or MP3 files |
| Content/Scripts/ | Copied Lua source files |
| .quiet-cook-cache.json | Metadata used by subsequent Cooker invocations |

Output names are based on asset IDs.

Audio and scripts retain native source formats. They are not QAudio
files or compiled Lua bytecode.

The pipeline copies the startup Scene, not every Scene in the project.

The package does not include the Sandbox executable, engine Resources,
or a complete executable distribution.

## Launch the package

Quiet_Sandbox receives the package directory, not the editable .quiet
descriptor or RuntimeManifest.json filename.

Run it from the executable directory:

```powershell
Push-Location .\Build\Binaries\Debug
try {
    .\Quiet_Sandbox.exe $packageRoot
}
finally {
    Pop-Location
}
```

Expected result: Sandbox loads the manifest, packaged resources, and
startup Scene, then begins runtime.

Check startup diagnostics for asset or Scene loading failures.
A successful cook does not guarantee that all runtime initialization
will succeed on the launching machine.

Built-in runtime assets come from engine support loading rather than
project cooking.

## Repeat cooking and cache reuse

Run the same Cooker command again with unchanged inputs and output root.

Eligible unchanged assets should report cache hits.
Reuse requires matching source fingerprints, conversion signatures,
runtime paths, and validated output files.

Mesh and texture cache reuse includes reader validation.
Audio reuse includes metadata inspection.
Script reuse checks fingerprints without parsing Lua.

Changed, missing, or invalid cached outputs are rebuilt or recopied.

The cache is for cooking only. Sandbox does not require it to load
the runtime package.

## Cleanup behavior

After writing the manifest, the Cooker removes eligible packaged files
that are no longer referenced by that manifest.

Cleanup targets QMesh, QTexture, native audio, and Lua files in their
Content directories.

It does not remove stale Scene files, unrelated extensions, or empty
directories.

A cleanup failure can leave partial cleanup and produce a warning.
Review diagnostics before treating the output as a clean distribution.

## Current conversion limits

All cooked textures use BC7 sRGB. The stage does not choose linear
formats for normal maps or masks.

Mesh cache fingerprints cover the registered source file rather than
automatically tracking all external dependencies. Verify results when
changing files referenced by a mesh source.

Audio is copied without transcoding. Inspection does not validate
every decoded frame.

Lua is copied without syntax or callback validation.

See [Cooking internals](../Systems/Cooking.md) for validation and failure
boundaries.

## Troubleshooting

| Symptom | Check |
| --- | --- |
| Usage is printed | Supply the project descriptor and output directory |
| Codec validation fails | Review Compressonator diagnostics and Cooker build configuration |
| Startup Scene is missing | Save it and check the descriptor's selected startup Scene |
| Asset stage fails | Check source paths, supported formats, dependencies, and validation errors |
| Lua packages but fails at runtime | Test source loading and callbacks in the Editor |
| Cache is ignored | Inspect its warning; the Cooker can continue with an empty cache |
| Cleanup or cache-save warning appears | Review output permissions and remaining files |
| Sandbox cannot load the manifest | Pass the package directory and confirm cooking completed |
| Sandbox fails during resource startup | Check runtime diagnostics, driver support, Resources, and native runtime dependencies |

## Verification

1. Cook into the dedicated output directory.
2. Review final status and warnings.
3. Launch that directory through Sandbox.
4. Confirm the intended startup Scene and gameplay behavior.
5. Close Sandbox and cook again.
6. Confirm eligible unchanged assets report cache hits.
7. Change a disposable registered source and confirm its output updates.

These steps verify package production and consumption on the test
machine; they do not validate deployment to another machine.