# Documentation Conventions <!--! {#quiet_documentation_conventions} -->

These conventions apply to Quiet's Markdown handbook and C++ documentation comments. They establish how documentation is written and reviewed. Local generation is described in [Building the documentation](BuildingDocumentation.md).

## Audience and language

Write in English for developers who can read C++ but may be unfamiliar with engine architecture, Vulkan, EnTT, Jolt, miniaudio, or Lua integration.

Explain unfamiliar concepts before relying on them. Use Quiet's current identifiers and terminology consistently. Define abbreviations at first use.

The technical handbook receives the most depth. Practical guides describe reproducible tasks, while the API reference records precise symbol contracts.

## Source of truth

Verify documentation against the current source being reviewed. An older README, previous snapshot, or planned design is not evidence of current behavior.

For each claim, distinguish:

- **Implemented behavior:** supported by the current implementation.
- **Observed consequence:** a conclusion drawn from that implementation; state the reasoning when it matters.
- **Planned behavior:** explicitly future work, kept separate from current contracts.

Explain design rationale only when supported by code, a recorded decision, or an explicit project requirement. Do not invent the original author's motivation.

If an implementation detail is unavailable, leave the claim unresolved rather than supplying a plausible substitute.

## Documentation responsibilities

| Location | Responsibility |
| --- | --- |
| [Root README](https://github.com/Panuh404/Quiet-Engine/blob/main/README.md) | Project introduction, main capabilities, maturity, quick start, and documentation entry points |
| [Documentation index](../README.md) | Handbook navigation and documentation progress |
| `Docs/Guides/` | Task-oriented instructions and user-visible behavior |
| `Docs/Systems/` | Concepts, architecture, ownership, lifecycle, and implementation flows |
| `Docs/Reference/` | Exact formats and lookup material |
| `Docs/Roadmap.md` | Engine direction and future work |
| C++ declarations | API contracts close to the symbols they describe |

Give each detailed topic one authoritative home. Other pages may summarize and link to it. Keep planned document paths as plain text until the target exists.

## System chapter template

Use this order unless the system needs a different sequence to be understood:

1. **Purpose and concepts:** what the system does and the terminology it introduces.
2. **Implementation map:** exact repository-relative paths and principal qualified symbols.
3. **Ownership and lifetime:** who creates, owns, updates, and destroys the objects and resources.
4. **Execution flow:** initialization, normal operation, transitions, and shutdown.
5. **Data and contracts:** persistent versus runtime state, units, validation, invariants, and failure behavior.
6. **Worked example:** a concrete operation traced through the current implementation.
7. **Limitations and extension points:** current constraints and relevant places to introduce a future change.

Document actual dependencies and boundaries, including existing coupling. Do not present a preferred future architecture as the current implementation.

## Practical guide template

State the goal, prerequisites, exact steps, expected result, and relevant failure cases. Use current build presets and executable names. Label placeholders clearly and make the working directory explicit when it affects a command.

Examples should be small enough to understand and complete enough to exercise the documented behavior. Explain when sample content is illustrative rather than directly runnable.

The Lua-facing API must be documented explicitly in the scripting guide. A generated reference for the C++ binding implementation does not by itself describe how to use those bindings from Lua.

## C++ comment style

Use `/** ... */` for class and function documentation and `///<` for concise field descriptions. Use `@` commands consistently within documentation blocks.

| Tag | Use |
| --- | --- |
| `@brief` | One concrete sentence describing the responsibility or operation |
| `@param` | Meaning, units, constraints, or ownership of a parameter when useful |
| `@tparam` | Requirements or restrictions on a template parameter |
| `@return` | Meaning of the result, including failure values and ownership where relevant |
| `@pre` | A condition the caller must satisfy |
| `@post` | A meaningful condition established by successful execution |
| `@note` | Lifetime, ordering, or context that helps a caller use the API correctly |
| `@warning` | A concrete misuse hazard supported by the implementation |
| `@ingroup` | Membership in the system group established by the Doxygen configuration |

Do not add tags merely to fill a template. Prefer a useful contract over a comment that restates the function name or parameter type.

Document declarations once rather than duplicating their contracts in definitions. Implementation comments should explain non-obvious algorithms, ordering, platform behavior, or tradeoffs.

## Contract priorities

Prioritize the following where they apply:

- Object and resource ownership.
- Pointer, handle, and reference validity, including invalidation conditions.
- Required component presence and permitted template types.
- Editor state versus runtime state.
- Immediate versus deferred effects.
- Units, coordinate spaces, ranges, and normalization requirements.
- Error results, exceptions, assertions, and partial-success behavior.
- Thread restrictions only when established by the implementation or a documented contract.

For example, component access documentation should explain the required Actor validity and component presence instead of merely saying that a component is returned. A lifetime guarantee must be checked against storage and mutation behavior before it is stated.

## System groups

Assign C++ declarations to the following system groups:

| Group identifier | Scope |
| --- | --- |
| `quiet_core` | Application orchestration and shared core utilities |
| `quiet_platform` | Window and platform shell integration |
| `quiet_input` | Input state and codes |
| `quiet_project` | Project descriptors, registry, context, and loading |
| `quiet_scene` | Actors, components, settings, scene lifecycle, and serialization |
| `quiet_editor` | Editor controllers, workflows, commands, history, and tools |
| `quiet_renderer` | Vulkan rendering and editor presentation integration |
| `quiet_assets` | Asset representations, importers, readers, and libraries |
| `quiet_physics` | Physics subsystem and runtime world |
| `quiet_audio` | Audio assets, subsystem, and runtime world |
| `quiet_scripting` | Script runtime, metadata, and bindings |
| `quiet_launcher` | Standalone project launcher |
| `quiet_cooker` | Cooking and runtime-package construction |
| `quiet_sandbox` | Standalone runtime entry point |

Group definitions live in `Docs/Groups.dox`. Their category parents are `quiet_foundations`, `quiet_runtime`, `quiet_content`, and `quiet_tools`, beneath `quiet_systems`. Assign declarations to the appropriate system group using `@ingroup`.

When adding or moving a system group, update its definition in `Docs/Groups.dox` and its sidebar entry in `Docs/DoxygenLayout.xml`.

Groups describe responsibilities rather than forcing a one-to-one mapping to directories. The reference includes relevant internal headers under `Engine/src/` as well as `Engine/include/Quiet/`.

## Links and formatting

Use relative Markdown links for handbook navigation. Refer to source using exact repository-relative paths and qualified symbols; avoid line numbers because they change as code evolves.

Each handbook page uses a unique ID in its first heading, except for the
landing page selected by `USE_MDFILE_AS_MAINPAGE`. For example:

```markdown
# Example Guide <!--! {#quiet_guide_example} -->
```

The HTML comment keeps the metadata hidden in GitHub's rendered Markdown.
Doxygen 1.18.0 uses it as the page ID. Keep IDs stable when changing page
titles or moving files.

Add the page to `Docs/Navigation.dox` with `@subpage` under one parent.
Use `@ref` for other Doxygen cross-links that do not assign a parent.
Keep ordinary handbook links relative to the referring Markdown file.

Introduce tables for exact mappings and comparisons. Use fenced code blocks with a language identifier for examples. Keep formatting readable both in the repository and in generated HTML; verify Doxygen-specific links and formatting when the toolchain exists.

No generated imagery is part of this documentation workflow. Explanations, tables, source references, and code examples are sufficient for the initial chapters.

## Verification and completion

For each chapter or system substep:

1. Inspect the current implementation, including relevant callers and serialization paths.
2. Write the handbook explanation and meaningful declaration comments.
3. Check source paths, qualified symbols, units, state transitions, and failure behavior.
4. Check relative links and planned-versus-available status.
5. Generate and inspect the affected documentation pages and warnings.
6. Exercise executable examples and commands in a suitable environment, or explicitly record what could not be verified.
7. Review each increment. Commit at the agreed chapter or system checkpoint
   after its related increments and required corrections are complete.

An automatically extracted declaration is not evidence that its behavior has been documented. Documentation warnings should be reviewed meaningfully; extraction settings must not hide missing contracts and be mistaken for full coverage.

Generated documentation belongs under `Build/Docs/` and is not committed. Documentation generation remains optional for normal builds.
Update the root README when a completed project milestone changes capabilities, setup, or documentation entry points.
