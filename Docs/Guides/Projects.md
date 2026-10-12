# Projects

This guide covers creating and opening editable Quiet projects through
the Launcher, understanding their layout, and launching the Editor
directly.

## Prerequisites

Build Quiet_Launcher and Quiet_Editor using
[Getting Started](../GettingStarted.md).

Keep both executables and their Resources directory together.
Launch from the executable output directory.

The examples below use Windows and PowerShell.

## Project location

The Launcher displays its configured root under PROJECTS LOCATION.
Open Folder opens that directory in the system file manager.

On Windows, the default root is:

```text
%USERPROFILE%\Documents\QuietProjects
```

This default is constructed from the user profile path. It does not
query Windows for a redirected Documents known-folder location.

The Launcher stores its root and recent-project paths in:

```text
%USERPROFILE%\.quiet\ProjectRegistry.json
```

This user-level registry is separate from each project's AssetRegistry.json.

The current Launcher interface displays the configured root but does
not provide a control for changing it.

## Create a project

1. Open Quiet_Launcher.
2. Select + New Project.
3. Enter a project name, such as FirstProject.
4. Check the displayed destination and validation messages.
5. Select Create and Open.

Leading and trailing whitespace is trimmed from the name.

Names must be nonempty and avoid filesystem-invalid characters,
control characters, trailing dots, and reserved device names.

Creation is disabled when the destination folder already exists.
Choose another name rather than expecting the Launcher to overwrite it.

Expected result: Quiet creates the project descriptor, directories,
and asset registry, then requests Editor launch.

For FirstProject under the default root, the descriptor is:

```text
%USERPROFILE%\Documents\QuietProjects\FirstProject\FirstProject.quiet
```

Creation and Editor launch are separate operations. If launch fails
after creation, the project can remain on disk and be opened later.

## Project layout

A new project uses these default paths:

| Path | Responsibility |
| --- | --- |
| FirstProject.quiet | Project name, timestamps, directories, and startup Scene |
| AssetRegistry.json | Asset identities, types, and source paths |
| Scenes/ | Saved Scene documents |
| Assets/ | Project source assets |
| Cache/ | Project cache directory |

The descriptor defaults StartupScene to DefaultScene.json.

Creating the project does not itself write that Scene file.
When the Editor cannot load the startup Scene, it creates a default
Scene in memory and logs a warning. Save the Scene to persist it.

Project directory names and the startup Scene can be configured in the
descriptor; the table describes defaults rather than mandatory names.

## Open an existing project

1. Open the Launcher.
2. Locate the project card.
3. Use Search projects... to filter the list when needed.
4. Select Open on the card.

The search matches project paths case-insensitively.

Refresh rescans the configured projects root recursively for files
with the exact .quiet extension. Existing recent-project paths can
also appear in the list.

Right-click a project card to access:

- Open Project.
- Show in File Explorer.
- Copy Project Path.

Open Folder in the footer opens the configured root.

Discovery of a .quiet file does not prove that its descriptor, assets,
or startup Scene can be loaded successfully.

## Open a project outside the Launcher list

The Launcher has no arbitrary project-file picker in this interface.
An existing project can be passed directly to Quiet_Editor.

From the repository root:

```powershell
$projectFile = Join-Path $env:USERPROFILE `
    "Documents\QuietProjects\FirstProject\FirstProject.quiet"

Push-Location .\Build\Binaries\Debug
try {
    .\Quiet_Editor.exe $projectFile
}
finally {
    Pop-Location
}
```

Replace the example path with an existing project descriptor.

Expected result: the Editor opens and begins loading that project.
Check its loading feedback and diagnostics for completion or failure.

Moving an entire project beneath the configured root and selecting
Refresh also allows it to be discovered. Preserve the project's
associated files rather than moving only its descriptor.

## Editor handoff

The Launcher resolves Quiet_Editor beside its own executable and
passes the absolute project-file path to a detached process.

A successful process-launch request causes the Launcher to exit.
It does not establish that Editor initialization completed successfully.

Keep Launcher and Editor from the intended build configuration together.

## Troubleshooting

| Symptom | Action |
| --- | --- |
| Create and Open is disabled | Check name validation and whether the destination folder exists |
| Creation fails | Check write access and the Launcher feedback; partial project files may remain |
| Project is absent | Clear the search, check PROJECTS LOCATION, select Refresh, or launch the Editor directly |
| Selected project no longer exists | Restore its descriptor and files, or refresh the list |
| Editor could not be launched | Build Quiet_Editor beside Quiet_Launcher and check executable/resource availability |
| Editor opens but project loading fails | Check Editor feedback and diagnostics; discovery alone does not validate a project |
| Default Scene appears unexpectedly | Check the descriptor's startup path and the saved Scene; failed Scene loading falls back to an in-memory default |

See [Getting Started](../GettingStarted.md) for build, resource,
validation-layer, and runtime dependency troubleshooting.

## Verification

Confirm that:

- Creation produces the descriptor, directories, and asset registry.
- The Editor completes project startup after Create and Open.
- Saving a Scene produces its Scene document.
- Reopening the project restores the saved Scene.
- An existing project can also be opened through the direct Editor command.

See [Scene and Actors](../Systems/Scene.md) for Scene persistence and
[Application](../Systems/Application.md) for startup implementation.