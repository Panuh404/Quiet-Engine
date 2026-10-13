# File Formats <!--! {#quiet_format_overview} -->

This reference describes Quiet's persisted data and the behavior of
its current readers and writers.

Examples illustrate structure. Paths and asset IDs must correspond
to the project being used.

| Reference | Formats |
| --- | --- |
| [Project files](ProjectFiles.md) | Project descriptor, asset registry, and user project registry |
| [Scene files](SceneFiles.md) | Scene settings, Actors, components, and hierarchy |
| [Runtime package files](RuntimePackageFiles.md) | Runtime manifest and cook cache |
| [Binary assets](BinaryAssets.md) | QAsset container, QMesh, and QTexture |

Reader acceptance, writer output, and runtime usability are different
contracts. A structurally accepted document does not establish that
its referenced files or resources are available.

See [Assets](../Systems/Assets.md), [Scene and Actors](../Systems/Scene.md),
and [Cooking](../Systems/Cooking.md) for system behavior.