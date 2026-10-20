# Scene Files <!--! {#quiet_format_scene} -->

Quiet stores Scene authoring data as JSON.

Implementation:

- Engine/src/Scene/SceneSerializer.h
- Engine/src/Scene/SceneSerializer.cpp
- Engine/src/Scene/Scene.cpp
- Engine/src/Scene/Components/
- Quiet::Scene::SceneSerializer

SceneDocument delegates persistence to SceneSerializer.

## Root structure

| Member | Representation | Reader behavior |
| --- | --- | --- |
| actors | Array | Required |
| audio_settings | Object | Optional; missing values retain defaults |
| render_settings | Object | Optional; missing values retain defaults |
| physics_settings | Object | Optional; missing values retain defaults |
| editor_hierarchy | Object | Optional; hierarchy can be reconstructed |

There is no Scene format Version field.

The writer emits all five root members.
Unknown fields are not retained when saving reconstructs the document.

Example containing one empty Actor:

```json
{
    "actors": [
        {
            "id": "0000000000000001-0000000000000001",
            "name": "Example",
            "transform": {
                "position": [0, 0, 0],
                "rotation": [0, 0, 0],
                "scale": [1, 1, 1]
            }
        }
    ]
}
```

This example omits optional settings, components, and hierarchy.

## Common representations

Vectors are positional numeric arrays:

- Vector2: [x, y].
- Vector3: [x, y, z].
- Vector4: [x, y, z, w].
- Colors: [r, g, b] or [r, g, b, a].

Readers extract the required array positions directly. Too-short arrays
or incompatible element types can throw. Extra elements are not consumed.

Rotation arrays contain Euler angles in degrees.

UUIDToString writes two zero-padded sixteen-digit hexadecimal values
separated by a hyphen. Use generated IDs for real Scene content.

Asset references contain string IDs, not embedded resources or file paths.

## Scene settings

### audio_settings

| Field | Type | Default |
| --- | --- | --- |
| master_volume | Number | 1 |
| music_volume | Number | 1 |
| sfx_volume | Number | 1 |
| ui_volume | Number | 1 |

### render_settings

| Field | Type | Default |
| --- | --- | --- |
| light_direction | Vector3 | [-0.35, 0.85, -0.35] |
| light_intensity | Number | 1 |
| ambient_color | Vector3 | [1, 1, 1] |
| ambient_intensity | Number | 0.25 |

Light direction follows Quiet's surface-toward-light convention.

### physics_settings

| Field | Type | Default |
| --- | --- | --- |
| gravity | Vector3 | [0, -9.81, 0] |

Loading these values does not itself establish their runtime validity.
Runtime systems apply their own validation and clamping.

## Actor identity and transform

| Field | Type | Reader behavior |
| --- | --- | --- |
| id | UUID string | Required; parse failure logs and skips the Actor |
| name | String | Defaults to Actor when absent |
| transform | Object | Required |
| transform.position | Vector3 | Required |
| transform.rotation | Vector3 | Required; degrees |
| transform.scale | Vector3 | Required |

The loader creates identity and transform components before reading
optional components.

Actor UUID parsing is not a general uniqueness check. Duplicate Actor
IDs are not rejected by this loading loop.

All following component objects are optional. Their presence causes
loading of the corresponding component, subject to the stated checks.

## Mesh and material

### static_mesh

| Field | Type | Reader behavior |
| --- | --- | --- |
| resource_id | String | Required |
| visible | Boolean | Defaults to true |
| material_slots | Array of material objects | Loaded when present as an array |

The mesh ID must already resolve in MeshLibrary.

If it does not, the loader logs a warning and does not add the mesh
component. It does not import the missing resource during Scene loading.

The writer emits static_mesh only when the component passes IsValid().
It emits material_slots only when the slot vector is nonempty.

### material

Both Actor-level material and static-mesh material slots use:

| Field | Type | Default |
| --- | --- | --- |
| base_color | Vector4 | [1, 1, 1, 1] |
| base_color_texture_asset_id | String | Empty |

Scene deserialization does not validate texture resource availability.

## Lighting, rotation, and camera

### directional_light

| Field | Type | Default |
| --- | --- | --- |
| enabled | Boolean | true |
| direction | Vector3 | [-0.35, 0.85, -0.35] |
| color | Vector3 | [1, 1, 1] |
| intensity | Number | 1 |

### rotator

| Field | Type | Default |
| --- | --- | --- |
| speed_degrees_per_second | Number | 0 |

### camera

Camera field names use different capitalization from other components.

| Field | Type | Default |
| --- | --- | --- |
| Primary | Boolean | true |
| FieldOfViewDegrees | Number | 60 |
| NearClip | Number | 0.1 |
| FarClip | Number | 1000 |

Loading does not enforce a single primary camera.

## Audio components

### audio_source

| Field | Type | Default |
| --- | --- | --- |
| audio_asset_id | String | Empty |
| bus | String | sfx |
| volume | Number | 1 |
| pitch | Number | 1 |
| play_on_start | Boolean | false |
| loop | Boolean | false |
| spatial | Boolean | true |
| min_distance | Number | 1 |
| max_distance | Number | 100000 |
| rolloff | Number | 1 |
| cone_inner_angle_degrees | Number | 360 |
| cone_outer_angle_degrees | Number | 360 |
| cone_outer_gain | Number | 0 |
| doppler_factor | Number | 1 |

Bus strings are lowercase: master, music, sfx, and ui.
Unknown bus text logs a warning and falls back to sfx.

### audio_listener

| Field | Type | Default |
| --- | --- | --- |
| primary | Boolean | true |

Loading does not enforce a single primary listener or create audio sounds.

## Physics components

### rigid_body

| Field | Type | Default |
| --- | --- | --- |
| motion_type | String | dynamic |
| friction | Number | 0.2 |
| restitution | Number | 0 |
| linear_damping | Number | 0.05 |
| angular_damping | Number | 0.05 |
| gravity_factor | Number | 1 |
| allow_sleeping | Boolean | true |

Motion strings are static, kinematic, and dynamic.
Unknown text logs a warning and falls back to dynamic.

### Colliders

| Component | Field | Type | Default |
| --- | --- | --- | --- |
| box_collider | size | Vector3 | [1, 1, 1] |
| box_collider | center | Vector3 | [0, 0, 0] |
| sphere_collider | radius | Number | 0.5 |
| sphere_collider | center | Vector3 | [0, 0, 0] |
| capsule_collider | radius | Number | 0.5 |
| capsule_collider | height | Number | 2 |
| capsule_collider | center | Vector3 | [0, 0, 0] |

The loader can add multiple collider components.
It does not enforce PhysicsWorld's exactly-one-supported-collider
requirement or construct runtime bodies.

## Text component

An Actor's optional text object stores authored content and appearance.

| Field | Type | Default |
| --- | --- | --- |
| content | UTF-8 string | "Text" |
| pixel_height | Number | 32 |
| color | RGBA array | [1, 1, 1, 1] |
| visible | Boolean | true |

Example Actor member:

```json
"text": {
  "content": "Score: 100",
  "pixel_height": 32,
  "color": [1, 1, 1, 1],
  "visible": true
}
```

Absence of the text object leaves the Actor without TextComponent.
Missing fields within the object retain component defaults.

Pixel height is intended to be finite and positive. Color uses
straight-alpha linear RGBA. These values are stored without clamping.
The existing typed JSON reads and exception behavior apply.

This component contains no position, font asset reference, GPU resource,
or layout cache. The renderer uses the default font at shared
screen-space origin (24, 24). Inspector supports addition, removal, and
multiline content editing; appearance controls are not implemented yet.

## Script component

| Field | Type | Default |
| --- | --- | --- |
| script_asset_id | String | Empty |
| properties | Array of property objects | Empty when absent/non-array |

Each property contains name, type, and value.

Non-object entries, empty names, and unrecognized property types are
skipped. This loader does not reject duplicate property names.

| Type string | Value representation | Missing-value default |
| --- | --- | --- |
| float | Number | 0 |
| int | Integer | 0 |
| bool | Boolean | false |
| string | String | Empty |
| vector2 | Two-number array | [0, 0] |
| vector3 | Three-number array | [0, 0, 0] |
| vector4 | Four-number array | [0, 0, 0, 0] |
| color3 | Three-number array | [1, 1, 1] |
| color4 | Four-number array | [1, 1, 1, 1] |
| actor | UUID string | Unset UUID |
| mesh_asset | Asset ID string | Empty |
| texture_asset | Asset ID string | Empty |
| audio_asset | Asset ID string | Empty |
| script_asset | Asset ID string | Empty |

Actor-property parsing does not require that the referenced Actor exists.
Asset-property loading does not validate registry identity or resource
availability.

The writer omits properties with empty names and writes an unset Actor
reference as an empty string. It omits properties entirely when the
component's property vector is empty.

No Lua environment, callback state, or playback/simulation state is stored.

## Editor hierarchy

editor_hierarchy contains:

| Field | Type |
| --- | --- |
| root | Ordered array of node objects |
| folders | Array of folder objects |

A node has type and id:

```json
{
    "type": "actor",
    "id": "0000000000000001-0000000000000001"
}
```

Type folder selects a folder node. Other type text falls back to actor.

A folder has:

| Field | Type | Default |
| --- | --- | --- |
| id | UUID string | Invalid/missing IDs are skipped |
| name | String | Folder |
| expanded | Boolean | true |
| children | Ordered array of nodes | Empty |

Hierarchy nodes require parsed, valid UUIDs; invalid IDs are logged
and skipped.

Loaded hierarchy is passed through Scene::SetHierarchy() and sanitation.
It is not accepted as an unchecked structure.

When hierarchy is missing, non-object, or entirely empty while Actors
exist, Scene reconstructs root nodes from the Actors.

Folders organize Editor presentation rather than transform parenting.

## Loading boundaries

File-open failure returns false.

The loader parses JSON without a local exception handler.
Malformed JSON, missing required fields, and incompatible types can
propagate exceptions.

It checks actors before clearing the existing Scene.
After that check, Scene::Clear() resets Actors, settings, hierarchy,
runtime worlds, and pending runtime requests.

Later failure can therefore leave an empty or partially loaded Scene.
Loading does not use a temporary Scene or roll back prior state.

Missing mesh resources and skipped entries can coexist with a true
return value.

## Saving boundaries

Saving reconstructs the known fields and writes indented JSON directly
to the destination.

The serializer does not create the parent directory or use atomic
replacement.

It checks whether the output opened, but does not check stream status
after writing before logging success and returning true.

See [Scene and Actors](../Systems/Scene.md) for persistence contracts
and [Physics](../Systems/Physics.md), [Audio](../Systems/Audio.md), and
[Scripting](../Systems/Scripting.md) for runtime validation.