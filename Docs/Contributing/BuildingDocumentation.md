# Building the Documentation <!--! {#quiet_building_documentation} -->

Quiet's documentation combines Markdown handbook pages with a generated C++ reference.

Documentation generation uses the standalone CMake project in `Docs/`. It does not configure or compile the engine.

## Requirements

- CMake 3.28 or newer.
- Ninja available on PATH.
- Doxygen 1.18.0 or newer.
- The Doxygen Awesome submodule initialized.

The navigation uses stable page labels embedded in HTML comments, verified with Doxygen 1.18.0 and the included Doxygen Awesome 2.5.0 theme.

Verify the tools from PowerShell:

```powershell
cmake --version
ninja --version
doxygen --version
```

## Initialize the theme

Run from the Quiet repository root:

```powershell
git submodule update --init --recursive
```

The parent repository records the theme revision to use.

To inspect the initialized submodule:

```powershell
git submodule status Docs/ThirdParty/DoxygenAwesome
```

## Configure

Run from the Quiet repository root:

```powershell
cmake -S Docs -B Build/Intermediate/Docs -G Ninja
```

CMake expands `Docs/Doxyfile.in` into the generated configuration:

`Build/Intermediate/Docs/Doxyfile`

## Generate

```powershell
cmake --build Build/Intermediate/Docs --target Quiet_Docs
```

The target generates HTML and copies the existing branding assets needed by the root README.

## Open the result

```powershell
Start-Process .\Build\Docs\html\index.html
```

The documentation homepage comes from `Docs/README.md`.

The root README, handbook pages, system groups, namespaces, classes, and source listings are included in the generated site.

## Review diagnostics

```powershell
Get-Content .\Build\Docs\warnings.log
```

Review configuration warnings, unresolved references, and documentation parsing errors.

The initial configuration uses `EXTRACT_ALL = YES` to expose a browsable API inventory. This suppresses undocumented-member warnings. A successful generation is not a documentation coverage result.

## Rebuild after changes

After editing handbook pages or C++ documentation comments, rerun the generation command.

After editing `Docs/CMakeLists.txt` or `Docs/Doxyfile.in`, rerun configuration and generation.

## Troubleshooting

### Ninja cannot be found

Ensure the directory containing `ninja.exe` is on PATH, then open a new terminal.

Alternatively, configure with its absolute executable path:

```powershell
cmake -S Docs -B Build/Intermediate/Docs -G Ninja -DCMAKE_MAKE_PROGRAM="C:/path/to/ninja.exe"
```

Replace the example path with the actual location of your Ninja executable.

### Doxygen cannot be found

Ensure the directory containing `doxygen.exe` is on PATH, then open a new terminal and rerun configuration.

### Theme submodule is missing

Run the submodule initialization command from the repository root, then rerun configuration.

## Source control

Commit documentation source, configuration, `.gitmodules`, and the theme submodule reference.

Generated configuration, HTML, and diagnostic logs live under `Build/` and are excluded by the repository's ignore rules.

## Theme updates

Update the theme deliberately by selecting an upstream release inside the submodule.

Regenerate and review the documentation before committing the updated submodule reference in Quiet.