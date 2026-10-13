# Audio <!--! {#quiet_system_audio} -->

Quiet integrates miniaudio through an application-owned AudioSubsystem
and Scene-owned AudioWorld objects.

AudioSubsystem owns the playback engine, asset metadata library, editor
preview sound, and mixing groups. AudioWorld owns the runtime sounds
associated with Scene Actors.

This chapter covers ownership, initialization, source construction and
synchronization, spatial updates, playback commands, listener selection,
shared mixing, one-shot playback, editor previews, and teardown.

See [Application](Application.md) for subsystem lifetime,
[Scene and Actors](Scene.md) for runtime lifecycle, and
[Assets](Assets.md) for asset loading.

## Implementation map

| Responsibility | Source |
| --- | --- |
| Shared subsystem declaration | `Engine/src/Audio/AudioSubsystem.h` |
| Engine initialization, buses, one-shots, and preview | `Engine/src/Audio/AudioSubsystem.cpp` |
| Runtime world declaration | `Engine/src/Audio/AudioWorld.h` |
| Runtime sources, listener updates, and mix restoration | `Engine/src/Audio/AudioWorld.cpp` |
| Audio metadata representation | `Engine/src/Audio/AudioAsset.h` |
| Metadata inspection and storage | `Engine/src/Audio/AudioLibrary.h` and `.cpp` |
| Bus identifiers | `Engine/src/Audio/AudioBus.h` |
| Scene mix settings | `Engine/src/Scene/SceneAudioSettings.h` |
| Source authoring data | `Engine/src/Scene/Components/AudioSourceComponent.h` |
| Listener authoring data | `Engine/src/Scene/Components/AudioListenerComponent.h` |
| Runtime creation and orchestration | `Engine/src/Scene/Scene.cpp` |

## Ownership

Application owns AudioSubsystem.

A Scene creates AudioWorld when beginning Play or Simulate runtime.
Ending runtime or clearing the Scene releases that world.

AudioWorld borrows AudioSubsystem. The subsystem and its miniaudio
engine must outlive the world's runtime sounds.

| Owner | Owned resources |
| --- | --- |
| AudioSubsystem | miniaudio engine, AudioLibrary, preview sound, and Music/SFX/UI sound groups |
| AudioWorld | Runtime sound bindings and saved mix levels for restoration |
| AudioLibrary | AudioAsset metadata indexed by asset ID |

AudioLibrary stores source paths and inspected metadata. Loading an
asset into this library does not create a runtime playback sound or
retain the inspection decoder.

AudioWorld receives the Scene registry by reference. It does not own
the registry.

Both AudioSubsystem and AudioWorld disable copying and moving.

## Subsystem initialization

AudioSubsystem construction calls Initialize().

Initialization:

1. Allocates the miniaudio engine.
2. Obtains miniaudio's default engine configuration.
3. Creates AudioLibrary.
4. Initializes the engine.
5. Creates Music, SFX, and UI sound groups.
6. Marks the subsystem initialized.
7. Logs the selected backend, playback device, sample rate, and channels.

Quiet does not hard-code the logged playback device, backend, or
sample rate in this initialization function.

The three sound groups are created with spatialization disabled.
Master volume is controlled on the engine itself; Master does not
have a separate sound-group object.

### Reported initialization failures

A failed miniaudio engine initialization throws std::runtime_error.

If a sound-group initialization reports failure, the implementation
releases previously initialized groups, uninitializes the engine,
releases the engine and library, and throws std::runtime_error.

These paths describe reported miniaudio failures; they are not a
general exception-safety guarantee for every allocation.

## Runtime startup

AudioWorld construction stores the subsystem reference and logs world
creation. The constructor does not build sources or validate subsystem
initialization.

During Scene::BeginRuntime(), the Scene:

1. Creates AudioWorld.
2. Applies the Scene's mix settings.
3. Resolves the primary audio-listener Actor, if one exists.
4. Calls BuildSources() with the registry and listener entity.

This happens after physics-world setup and before script-instance
construction and script Begin callbacks.

BuildSources() destroys existing runtime sources before checking
whether the subsystem and engine are available.

When they are available, it establishes the listener before creating
sources, allowing PlayOnStart sounds to begin with the initial
listener state.

The method returns void. Completing startup does not guarantee that
every authored source acquired a runtime sound.

## Source eligibility

Creating a runtime source requires:

- An initialized AudioSubsystem with an available engine.
- A valid registry entity.
- ActorIdentityComponent.
- TransformComponent.
- AudioSourceComponent.
- A nonempty AudioAssetID present in AudioLibrary.
- No existing source binding for that entity.

AudioLibrary supplies the source file path. Runtime sound creation
opens that file through miniaudio; metadata presence alone does not
guarantee that playback initialization will succeed.

Each successful binding records the entity, Actor UUID, asset ID,
bus, spatialization mode, and owned miniaudio sound.

## Sound construction

All runtime sounds request MA_SOUND_FLAG_STREAM.

Nonspatial sources additionally request
MA_SOUND_FLAG_NO_SPATIALIZATION.

The selected bus determines the sound group:

| Bus | Playback routing |
| --- | --- |
| Master | Directly through the engine, without a dedicated group |
| Music | Music group |
| SFX | SFX group |
| UI | UI group |

An unrecognized bus value falls back to the SFX group.

### Creation settings

The following values are applied when the sound is created:

| Setting | Finite-value handling | Nonfinite fallback |
| --- | --- | --- |
| Volume | Clamp to 0–4 | 1 |
| Pitch | Clamp to 0.01–4 | 1 |
| Minimum distance | Clamp to 0.01–99999.99 | 1 |
| Maximum distance | Clamp to minimum distance + 0.01 through 100000 | 100000 |
| Rolloff | Clamp to 0–10 | 1 |
| Cone inner angle | Clamp to 0–360 degrees | 360 degrees |
| Cone outer angle | Clamp to inner angle through 360 degrees | 360 degrees |
| Cone outer gain | Clamp to 0–1 | 0 |
| Doppler factor | Clamp to 0–10 | 1 |

Loop is copied directly.

Distance, cone, and Doppler settings are applied only to spatial
sources. Spatial sounds use inverse distance attenuation.

The initial position comes from WorldTransform.Position.
Direction is derived from WorldTransform.RotationDegrees using the
engine's local forward axis, negative Z.

Initial source velocity is zero. Cone angles are converted to radians
before being passed to miniaudio.

These numeric setting checks do not provide general validation of
every transform value.

### PlayOnStart

When PlayOnStart is enabled, creation attempts to start the sound
before storing its binding.

A sound initialization failure leaves the entity unbound.

If starting a PlayOnStart sound fails, the initialized sound is
uninitialized and the entity remains unbound.

BuildSources() returns no aggregate success result. Its logged
playOnStart count describes successfully bound sources with that flag,
rather than all authored sources.

## Source synchronization

AudioWorld::Update() updates the listener, synchronizes source
membership, and then updates spatial source transforms.

SynchronizeSources() removes an existing binding when:

- Its entity is invalid.
- ActorIdentityComponent is missing.
- Its Actor UUID differs from the recorded UUID.
- AudioSourceComponent is missing.
- AudioAssetID is empty or differs from the recorded ID.
- Bus differs from the recorded bus.

Removing a binding stops and uninitializes its sound.

The method then attempts to create missing eligible sources.
Unresolved asset IDs are skipped without attempting backend sound
initialization every frame.

A loaded asset whose sound initialization fails can be attempted
again on a later update.

### Changes to existing sources

| Authored change | Existing binding behavior |
| --- | --- |
| Actor UUID | Remove and attempt recreation |
| AudioAssetID | Remove and attempt recreation |
| Bus | Remove and attempt recreation |
| Spatial | Retain the creation-time spatialization mode |
| Volume, Pitch, or Loop | Retain the creation-time value |
| Distance, cone, or Doppler settings | Retain the creation-time value |
| PlayOnStart | Does not directly start or stop an existing sound |
| Transform | Update position and direction for an existing spatial sound |

Recreation uses the current component values. A recreated source with
PlayOnStart enabled starts a new sound; the previous playback cursor
and paused state are not preserved.

Replacing library metadata under the same asset ID does not itself
recreate an existing sound.

### Missing transforms

TransformComponent is required to create a source, but removing it
does not currently remove an existing binding.

The spatial update skips a binding whose transform is missing.
Its sound remains allocated and may continue playing with its previous
spatial state.

## Spatial source updates

Spatial updates use the binding's recorded Spatial flag, rather than
the component's current value.

Position and direction are supplied from the current WorldTransform.

Velocity is estimated as:

(current position - previous position) / deltaTime

This calculation requires a finite positive delta, a previous
position sample, and finite current and previous positions.
Otherwise, velocity is set to zero.

A finite current position becomes the next previous-position sample.
A nonfinite current position invalidates that history.

An invalid delta does not skip the entire audio update. Listener
processing, membership synchronization, and spatial position and
direction updates still occur.

Position is passed to miniaudio before its finiteness is checked for
velocity estimation. Rotation-derived direction is not independently
validated here.

## Playback commands

Scene exposes playback commands using Actor UUIDs:

- PlayAudioSource.
- StopAudioSource.
- PauseAudioSource.
- ResumeAudioSource.
- SeekAudioSource.
- GetAudioSourcePlaybackState.

The Scene resolves the Actor UUID and forwards the operation to
AudioWorld. Commands return false when runtime is inactive, the world
is unavailable, or the Actor cannot be resolved.

AudioWorld commands require an existing binding with a sound.
They do not create a missing source on demand.

### Command behavior

| Command | Behavior on success |
| --- | --- |
| PlaySource | Stops, rewinds to frame zero, and starts playback |
| StopSource | Stops, clears the paused flag, and rewinds to frame zero |
| PauseSource | Stops a currently playing sound without rewinding and marks it paused |
| ResumeSource | Starts a paused sound at its retained cursor and clears the paused flag |
| SeekSource | Moves the cursor to the requested time without explicitly changing playback or paused state |

PlaySource restarts even when the sound is already playing or paused.

PauseSource returns false when the sound is not currently playing.
ResumeSource returns false unless the binding is marked paused.

SeekSource rejects negative or nonfinite seconds. Quiet does not
clamp the requested time against AudioAsset.DurationSeconds;
acceptance is delegated to miniaudio.

### Failure behavior

Backend failures are logged and return false.

Multi-operation commands are not transactional. For example, PlaySource
can successfully stop a sound and then fail while rewinding or starting
it. StopSource can stop a sound and then fail to rewind it.

A false return therefore does not guarantee that playback state and
cursor remain unchanged.

## Playback state

GetSourcePlaybackState() evaluates the binding in this order:

| Condition | Returned state |
| --- | --- |
| Missing binding or sound | Unavailable |
| miniaudio reports playback active | Playing |
| Binding is marked paused | Paused |
| Otherwise | Stopped |

Stopped does not distinguish an explicitly stopped sound from one that
naturally finished playback.

Scene::GetAudioSourcePlaybackState() also returns Unavailable when
runtime is inactive or the Actor cannot be resolved.

## Listener selection

Scene::FindPrimaryAudioListenerActor() returns the first Actor in
the Scene's Actor container with AudioListenerComponent.Primary enabled.

It does not enforce that only one listener is primary. When several
are primary, container order determines which one is selected.

The selection does not automatically follow the active camera or
choose an arbitrary Actor when no primary listener exists.

Scene resolves this selection during runtime startup and again during
each runtime audio update.

AudioWorld uses miniaudio listener index zero. Multiple authored
listeners therefore select one active listener rather than providing
multiple simultaneous listener updates.

### Missing listener

If the supplied listener entity is null, invalid, or lacks
TransformComponent, AudioWorld:

1. Sets listener velocity to zero.
2. Disables listener index zero.
3. Clears its recorded listener entity and position history.

Listener selection itself does not check TransformComponent.
A selected primary Actor without a transform therefore disables the
listener instead of causing selection to continue to another Actor.

Disabling the listener does not remove source bindings or explicitly
stop their sounds.

### Listener transform

For a listener with a transform, AudioWorld enables listener index zero
and supplies WorldTransform.Position.

Direction uses local negative Z transformed by the Actor's rotation.
World up uses local positive Y transformed by the same rotation.

Velocity is estimated from successive positions only when:

- The listener entity is unchanged.
- A previous position sample exists.
- Delta time is finite and positive.
- Current and previous positions are finite.

Otherwise, listener velocity is zero. Switching listeners does not
estimate velocity between the two Actors' positions.

A finite current position becomes the next history sample.
A nonfinite position invalidates the history.

Position and rotation-derived orientation are still supplied to
miniaudio without general finite-value validation. The position checks
described above protect velocity estimation.

## Mixing

AudioSubsystem controls Master volume on the miniaudio engine and
Music, SFX, and UI volume on their sound groups.

SetBusVolume() rejects nonfinite input, unavailable infrastructure,
and unrecognized buses. Finite volume is clamped to 0–4.

GetBusVolume() returns Quiet's stored level, or 1 for an unrecognized bus.

### Scene mix application

AudioWorld::ApplyMixSettings() saves the previous four bus levels on
its first application. Subsequent applications retain that original
snapshot.

Scene values are clamped to 0–4, with nonfinite values replaced by 1.
The four bus updates are independent: failure logs a warning without
rolling back successful changes.

Scene::BeginRuntime() applies the mix during startup.
AudioWorld::Update() does not continuously reapply Scene mix settings.

These buses are shared subsystem state. Changes affect other sounds
routed through the same engine and groups.

During destruction, AudioWorld attempts to restore its original
snapshot when the subsystem remains initialized. Restoration results
are ignored. This can overwrite bus edits made during runtime and
does not isolate simultaneous worlds sharing the subsystem.

## One-shot playback

AudioSubsystem::PlayOneShot() requires an initialized engine and a
nonempty file path. It delegates playback to ma_engine_play_sound()
without a dedicated sound group.

PlayOneShotAsset() resolves loaded metadata and forwards its SourcePath.
Missing metadata or a reported backend failure returns false.

These methods return no playback handle and create no Actor source
binding. AudioWorld playback commands cannot control these one-shots.

Master volume applies; Music, SFX, and UI group routing is not supplied.

## Editor previews

AudioSubsystem owns one preview sound and its recorded asset ID.

StartPreviewAsset() requires initialized infrastructure and loaded
metadata. Missing metadata preserves the current preview.

After resolving metadata, it releases the previous preview before
creating its replacement. Initialization or startup failure therefore
leaves no replacement preview.

Preview sounds request streaming, no spatialization, and no pitch
processing. They use no dedicated sound group, so Master volume applies.

Starting the same asset again replaces its sound rather than resuming it.

IsPreviewPlaying() checks backend playback.
IsPreviewingAsset() also compares the recorded asset ID.

GetPreviewAssetID() returns the recorded ID even after natural playback
completion. Completion does not automatically release the preview sound
or clear that ID through these methods.

StopPreview() stops and uninitializes the sound, releases it, and clears
the ID. Subsystem shutdown also calls StopPreview().

## Teardown

AudioWorld destruction destroys its runtime sounds and then attempts
to restore saved subsystem mix levels.

Application shutdown releases its Scenes before AudioSubsystem.

AudioSubsystem shutdown stops and releases the preview sound, releases
AudioLibrary, uninitializes sound groups, and finally uninitializes and
releases the engine.

This ordering keeps the engine alive while dependent sounds and groups
are being released.