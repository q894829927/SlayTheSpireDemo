# Interior exploration sample

This standalone sample is scoped to `/Game/House/L_Interior_LivingKitchen`.
It does not advance the card-battle phases or change the project's startup map.

## Interaction contract

- A child-sized first-person character walks with WASD, looks with the mouse, jumps with Space, and interacts with E or the left mouse button.
- Five labeled wall switches sit next to the entrance in two rows at 105 / 153 cm above the floor.
- Interaction requires an unobstructed camera trace within 180 cm. Walls and furniture block interaction.
- Each switch owns its on/off state and exactly one explicitly assigned practical light: ENTRY, LIVING, PENDANT 1, PENDANT 2, or KITCHEN. Switches never discover all lights or share circuit targets.
- The sun, sky light and disabled window fill lights are outside this circuit. Daylight remains visible with the indoor circuit off.
- The sample's GameMode override applies only to this map. Card-battle state, input configuration and UI are not involved.
- Q toggles daytime/nighttime through exactly one level-owned `InteriorDayNightController`, resolved once by the character. The controller explicitly references the sun, sky and exposure volume, captures authored daytime values once, and restores them on return to daytime. It never changes the wall switch's indoor circuit.
- Character visuals are a simple stylized prototype assembled from primitives, not a rigged or animated child character asset.

## Controls

Open the interior map and choose Play using the map's PlayerStart. Click the viewport to capture the mouse if needed.
Use WASD / mouse / Space. Aim at the switch beside the entrance and press E or left-click.
Press Q anywhere to toggle day/night; the HUD shows the next available mode. Indoor lamps retain their current on/off state.
The crosshair prompt and physical switch indicator show the interaction/state. Escape ends PIE.

## Acceptance

### AUTOMATED GATES

- Development Editor build, including UHT for the new classes.
- Focused interior Automation: explicit light ownership, toggle and restoration, and interaction distance/occlusion.
- Saved map contains the dedicated GameMode, a safe PlayerStart, and five independent switches, each wired to exactly one indoor light.

### MANUAL PIE GATES

- Child eye height, mouse look, walking collision and jumping behave sensibly inside the furnished rooms.
- Aim at the wall switch from close range: the prompt appears; E / left-click visibly changes the switch and indoor lamps.
- A second interaction restores the lamps. Outdoor daylight remains present. No battle HUD appears.

## Validation evidence

2026-09-07:

- UE 5.8 bundled .NET project-file generation and Development Editor build passed. Final build after the HUD readability adjustment is recorded in `Saved/Logs/InteriorExplorationFinalBuild.log`.
- `SlayTheSpireDemo.Interior`: 2 tests passed, 0 failures. The circuit test has one fixture teardown warning (`DestroyActor: World has no context`) from explicitly exercising a destroyed light reference. Report: `Saved/Automation/InteriorExploration/index.json`; log: `Saved/Logs/InteriorExplorationTests.log`.
- UE Python configured and saved the actual map with `InteriorGameMode`, one `Interior_WallLightSwitch`, and exactly the five practical lights. Setup manifest: `Saved/InteriorExplorationSetup.json`.
- Focused PIE: the possessed pawn is `InteriorChildCharacter`, HUD is `InteriorHUD`, and the camera initially focuses the wall switch. Captured eye Z was 112.15 cm including floor thickness and CharacterMovement floor clearance.
- Keyboard/mouse input in PIE moved the pawn from (-160, -255, 64.15) to approximately (81.79, -373.69, 64.15), with wall collision preventing further travel through the entrance wall. The view changed in response to mouse input. Evidence: `Saved/InteriorPIE_Movement.json`.
- The indoor circuit switched off: all five practical light components became invisible while the 65,000 lux sun remained visible. The off state is recorded in `Saved/InteriorPIE_LightsOff.json`; a second nearby, unobstructed interaction restored the lights.
- Space input caused a jump from Z 64.15 to a sampled peak of Z 116.36, followed by landing. Runtime sample: `Saved/InteriorPIE_Jump.json`.
- The first PIE view exposed undersized HUD text; the font was increased and given a contrasting prompt background. This HUD-only edit does not invalidate the already-passing circuit/trace tests.
- The corrected HUD was checked in PIE: controls and the `[E / LMB] Turn lights OFF` prompt are readable. PIE was then stopped, leaving the saved interior map open for the user.

Local evidence under `Saved/` is intentionally not committed. Packaged-game validation was not performed.

### Q day/night addition (2026-09-07)

- AUTOMATED GATES: regenerate project files and build passed; `SlayTheSpireDemo.Interior.DayNight` passed (1 test, 0 warnings/errors). The test covers character routing, repeated night application, exact daytime restoration including exposure override flags, and independence from the wall switch. Evidence: `Saved/Logs/InteriorDayNightBuild.log`, `Saved/Automation/InteriorDayNight/index.json`.
- The actual map has exactly one `Interior_DayNightController` with explicit sun, sky and exposure references; setup/save succeeded. Manifest: `Saved/InteriorDayNightSetup.json`.
- MANUAL PIE GATE passed: native Q keyboard input switched the actual PIE map to night and back to day; the HUD changed between Switch to DAY and Switch to NIGHT. Night retained usable warm kitchen lighting and dark windows; daylight restored the bright windows and sunlight. Runtime captures in Saved/InteriorDayNightPIE_Night.json and Saved/InteriorDayNightPIE_Day.json show sun 0.35 -> 65000 lux, sky 0.08 -> 1, exposure EV 4.5 -> 9, and all five practical lights remaining on. PIE was stopped with the saved map open.


### Individual light switches (2026-09-07)

- Replaced the single five-light circuit with five one-light switch instances on the entrance wall, with physical labels. Sun/sky remain controlled by Q; disabled window fills are not practical lamps.
- AUTOMATED GATE: direct PIE assertions passed for all five unique assignments, unobstructed interaction traces from child eye height, isolated light-off states, and restoration. Evidence: `Saved/InteriorIndividualSwitchesSetup.json`, `Saved/InteriorIndividualSwitchesPIE.json`.
- MANUAL PIE GATE: verified the two-row layout, readable labels, and the aimed switch HUD prompt. Initial horizontal placement intersected the existing television; the final two-row layout avoids it and all five trace checks pass.
- Asset-only configuration reused the existing compiled interaction implementation; no C++ rebuild or historical Automation rerun was required. Map saved; PIE stopped.

### Visible moonlight (2026-09-07)

- Increased this map instance's night directional light from 0.35 to 30 lux, retaining the controller's cool night color, sky intensity 0.08 and EV 4.5. This is an artistic visibility setting, not a physically calibrated full moon. Daytime values and independent indoor switches are unchanged.
- Direct PIE checks verified night intensity switching, daytime restoration to 65000 lux, and independence of the five switches. Visual inspection with all practical lights off confirmed window-shaped moonlight patches and furniture shadows while the room remains dark. Initial captures retained daytime lighting history; final inspection was performed after rendering settled.
- Final map settings saved in Unreal; no native code changes or rebuild required. Setup evidence: `Saved/InteriorMoonlightSetup.json`. PIE stopped.

### Moon disk, shafts and Moonlight Sonata (2026-09-07)

- Night now reveals an explicitly assigned lunar mesh and volumetric haze. The moon uses a procedural mottled emissive material. The directional light changes to a low north-facing moon angle and cool tint; daytime restores its authored direction, temperature setting and atmosphere disk. The atmosphere disk is suppressed at night when the custom moon exists to prevent a doubled moon.
- A single controller-owned, non-spatial AudioComponent plays the complete first movement of Beethoven's Moonlight Sonata, performed by Paul Pitman for Musopen. Night fades in the looping recording, day stops it, and EndPlay stops it. Reapplying night does not spawn another component or restart an already playing recording. The map explicitly references the SoundWave; no runtime network access is needed.
- Audio credits and source/license provenance: [InteriorNightAudioCredits.md](InteriorNightAudioCredits.md). A night HUD footer also credits the work, performer and musopen.org.
- AUTOMATED GATES: Development Editor build and focused DayNight Automation; direct PIE assertions for moon/fog visibility and real imported SoundWave playback, repeated night requests, stopping on day and restarting on night. Final build: Saved/Logs/InteriorNightExperienceBuild.log. Focused DayNight Automation: 1 passed, 0 failures, 0 warnings (Saved/Automation/InteriorNightExperience/index.json). Music/visibility checks: Saved/InteriorNightExperiencePIE.json (335.77-second recording).
- MANUAL PIE GATE: verify one visible moon through the kitchen window, moonlit room surfaces and atmospheric depth. Audio playback state is checked through the running AudioComponent; speaker output has not been independently auditioned.

- Final visual check confirmed a single moon through the window and moonlight across the kitchen surfaces with indoor lamps off. Lunar material contrast was refined and saved after PIE. Map and audio assets are saved; PIE stopped. Packaged-game and physical speaker audition were not performed.


### Exterior PCG forest, glazing and drifting clouds (2026-09-07)

- Source sample resolved to `E:/UE_DEMO/ElectricDreamsEnv`. Migrated three European Hornbeam SimpleWind meshes (Forest 01, Forest 03, Sapling 03) and their recursively enumerated Unreal asset dependencies. During the initial migration, imported third-party assets used the original `/Game/Megascans`, `/Game/MSPresets`, `/Game/Custom` and `/Game/SmartAssets` package paths; the map-specific groups were later organized under `/Game/House` with references fixed up. Existing destination files were hash-checked, never overwritten with conflicting content. Source content was not modified. Migration manifest: `Saved/ExteriorForestMigration.json` (59 packages, about 493 MiB, including the available cloud texture).
- Enabled the engine PCG plugin for the explicitly requested PCG workflow. At the time of creation, the project-authored assets were under `/Game/SlayTheSpireDemo/Maps/ExteriorForest`; they are now organized under `/Game/House/ExteriorForest`. `PCG_WindowForest` uses Create Points Grid -> Transform Points -> weighted Static Mesh Spawner -> Output. Two non-partitioned PCG volumes use fixed seeds 20260907 and 20260908; random yaw, offsets and uniform scale vary the trees. The central gap preserves the moon/window light corridor.
- Generated and saved 16 instances. Direct runtime assertions confirm all 16 are outside the house (Y > 2000 cm, |X| > 1500 cm), rooted on the existing ground at Z -24 cm. Evidence: `Saved/ExteriorForestGenerated.json`, `Saved/ExteriorForestPIE.json`. PCG generated output and graph references persist in the map.
- Added four low-opacity, two-sided glass panes inside the existing window frames. Panes block movement but do not cast opaque shadows across the room. No opening-window interaction was added.
- Added a distant translucent cloud plane in front of the lunar mesh. Four procedural noise octaves, soft edge fades and material Time produce slow continuous drift without Actor Tick. Clouds remain present during day/night; Q, moon visibility and music retain their existing ownership.
- AUTOMATED GATES: Unreal commandlet successfully loaded migrated assets, created/saved the PCG graph, materials and map; direct PIE assertions passed for instance count, exterior bounds, grounding and four panes. This asset/configuration change required no C++ rebuild or historical battle Automation rerun.
- MANUAL PIE GATE: viewed trees through the glass beside the visible moon; two fixed-view captures confirmed clouds changed position over time. Evidence: `Saved/ExteriorNight.png`, `Saved/ExteriorCloudsLater.png`. Restored player input after inspection, stopped PIE, and saved the final cloud material and map. Packaged validation not performed.

### Surface Sampler exterior exclusion (2026-09-07)

- Rebuilt `PCG_WindowForest` so two outside rectangles are converted to spline surfaces and fed into `PCGSurfaceSamplerSettings`; the graph is now `Create Points Grid (2 rectangles) -> Transform Points -> Create Spline -> Create Surface From Spline -> Surface Sampler -> Transform Points -> weighted Static Mesh Spawner -> Output`.
- The two sampling rectangles are `x=-4000..-1600` and `x=1600..4000`, `y=1700..6900`, leaving a 1600 cm gap around the house. Trees therefore cannot be sampled from the interior floor. The final generated points were all grounded at `Z=-24`, with `|X| >= 1930.54` and `Y >= 1700`.
- The tree density control is `Points Per Squared Meter = 0.008` on the Surface Sampler. Raise it for more trees or lower it for fewer trees. To move the forest boundary, adjust the rectangle centers/extents in the graph while keeping the gap outside the house.
- Two fixed-seed PCG volumes regenerated successfully after the graph change, producing 7 trees in the exterior rectangles. Map and graph are saved; no C++ rebuild was required. Result: `Saved/ExteriorEditorResult.json`.
- Fixed the inverted-tree orientation: the Transform Points node had a 360-degree value in the Pitch field because the Python Rotator was created positionally. `TransformPoints_16.rotationMax` is now `pitch=0, yaw=360, roll=0`; the regenerated point transforms have zero pitch/roll and yaw-only random rotation.

### Exterior forest ring around the house (2026-09-07)

- Measured the authored house footprint as `x=-620..620`, `y=-420..420` and moved the PCG layout close to that footprint. Four rectangular grid regions now cover the west, east, south and north sides with a 500 cm clearance from the walls.
- The graph feeds the four jittered grid regions into the shared random yaw/scale transform and tree spawner. This keeps the Surface Sampler settings available for later tuning while avoiding the old oversized rear-only sampling area.
- The active PCG volume is centered on the house at `(0,0,-24)` with a `5000 x 5000 cm` XY coverage. The second graph instance and its old ground strip were moved out of the playable area to avoid duplicate trees.
- Regeneration produced 128 tree points around all four sides (`42` west, `40` east, `23` south, `23` north). Point bounds were `x=-2164.60..2160.17`, `y=-1953.02..1962.61`, `z=-24`; no tree bounds overlap the house footprint. The exterior ground plane was centered under the ring at `z=-29`.
- Map and graph were saved after regeneration. This asset-only adjustment required no C++ rebuild.

### Movable PCG forest ring and fixed house exclusion (2026-09-07)

- Removed the disconnected spline/surface-sampler leftovers that were still wired beside the final grid path. The graph is now four `Create Points Grid -> Transform Points` strips, `Get Actor Data (Self) -> Copy Points` for the PCG volume translation, `Difference` against the fixed house exclusion box, `To Point -> Transform Points -> Static Mesh Spawner -> Output`.
- Added the editor-only `TriggerBox` actor `PCG_HouseExclusion`, tagged `PCG_HouseExclusion`, with bounds `x=-640..640`, `y=-440..440`, `z=-90..390`. The `Get Actor Data` node selects that tag without requiring overlap with the moving PCG volume, so the subtraction remains in world space.
- Kept one active graph instance, `PCG_ExteriorForest_1` / `PCGVolume_0`; removed the duplicate `PCGVolume_1` that made movement appear ineffective. The four grids remain local forest strips, while `Copy Points` applies the current PCG actor location before the exclusion test.
- Validation through the PCG graph: at `(0,0,-24)` the output contained 128 points with zero points inside the exclusion bounds; after moving the PCG volume to `(500,300,-24)` and regenerating, the output shifted by `(500,300)`, contained 125 points, and still had zero points inside the exclusion bounds. The volume was restored to `(0,0,-24)` with seed `20260907` and the map/graph were saved.
- This is an editor asset change; no C++ rebuild or PIE pass was performed for this graph cleanup. After moving the PCG volume in the editor, use `Force Regen` if the editor has not regenerated the graph yet.

### Single-grid PCG house subtraction (2026-09-07)

- Simplified `PCG_WindowForest` from four manually positioned strips to one `Create Points Grid` (`x=2200`, `y=2000`, cell size `350 x 400`) followed by jitter, actor translation, and the existing `Difference` node. The ring shape is now determined directly by the house exclusion data rather than by four separate source grids.
- Enlarged the tagged `PCG_HouseExclusion` box to `x=-800..800`, `y=-600..600` so tree point bounds have clearance from the authored walls while the grid still leaves a broad exterior area.
- Movement validation with the user's current PCG location `(820,580,-24)`: 98 points, zero inside the exclusion bounds; moving to `(1120,780,-24)` translated the same point pattern by `(300,200)` with zero points inside; the volume was restored to `(820,580,-24)` and saved. Generated meshes total 98 instances (`39` Forest 01, `39` Forest 03, `20` Saplings).

### House asset folder organization (2026-09-07)

- Moved the interior map to `/Game/House/L_Interior_LivingKitchen` (`Content/House/L_Interior_LivingKitchen.umap`).
- Moved the map-owned resources while preserving their working subfolders: `/Game/House/ExteriorForest` contains the forest PCG graph and exterior materials, `/Game/House/InteriorMaterials` contains the interior material set, and `/Game/House/InteriorNight` contains the moon and Moonlight Sonata SoundWave.
- Moved the imported forest dependencies from `/Game/Custom`, `/Game/Megascans`, `/Game/SmartAssets`, and `/Game/MSPresets` under matching `/Game/House` subfolders, preserving their relative folders. The map and PCG graph now serialize the `/Game/House/Megascans` tree paths, and the foliage materials resolve their `/Game/House/MSPresets` functions. Engine/script/plugin dependencies remain in their original shared locations.
- Removed the old map package and cleaned the obsolete redirectors after verifying the moved map's dependency list and PCG instance.
- Updated `InteriorChildCharacter.cpp`'s three hardcoded avatar material references to `/Game/House/InteriorMaterials` and verified the Development Editor build passed. No build settings changed; the code edit only keeps the child avatar's materials valid after the move.
- The editor now has `/Game/House/L_Interior_LivingKitchen` loaded. No PIE pass was repeated for this organization-only change.
