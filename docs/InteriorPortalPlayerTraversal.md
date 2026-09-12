# Interior portal player traversal — P3/P4 delivery

Date: **2026-09-13**. Baseline HEAD: `7d10a8e207446085b1aeaf6edc889f903e0163e4`, including the user's existing uncommitted map, material and flashlight-clearance changes.

Authority: [full-fidelity plan](InteriorPortalFullFidelityImplementationPlan.md), [observed issues](InteriorPortalObservedIssues.md). This delivery addresses the critical player support-wall escape before the remaining visual work. It does not seal the entire P3/P4 or Core Portal Fidelity scope.

## Player passage contract

`InteriorChildCharacter` now uses `UInteriorPortalMovementComponent`. Every movement operation, including slide, step and correction submoves, enters the same aperture constraint before the engine moves the capsule. The supporting primitive is ignored only while that capsule has an active, bounded passage. Other world collision remains active.

The aperture is an inscribed 32-sided polygon inside the visible ellipse. Analytic capsule support functions determine the continuous legal interval along a requested move. This is a conservative fit; it does not rely on a few surface samples as a claim of exact arbitrary-body geometry. A lateral move that leaves the interval produces a blocking hit before leaving the opening. A move that has already cleared the entire wall can continue normally in the room.

The explicit states are `Outside`, `ApproachingEntry`, `IntersectingAperture`, `Transferred`, and `ClearingExit`. During clearance, reversal stops at the exit eye plane until the full capsule has moved in front of the support by its projected normal extent plus 2 cm. A later return is a new legal crossing. At most one commit is allowed in a game frame, including multiple CharacterMovement substeps. This is intentional hysteresis; immediate reversal while still straddling is not treated as another transfer.

Invalidating or destroying an endpoint restores collision only after recovering the capsule to a non-penetrating front-side pose, with the last clear pose as fallback. Routine lateral motion is constrained continuously rather than repaired by a later teleport. Existing move/clear requests remain rejected while passage is busy.

## Timing and camera ownership

The order for the player is now:

1. PrePhysics binds the local character and checks passage lifecycle.
2. Each CharacterMovement submove computes its allowed aperture interval, then performs the ordinary world sweep.
3. A swept eye crossing commits immediately after that submove, before subsequent floor queries. Position, velocity, remaining acceleration and view basis are mapped together. `bJustTeleported` prevents deriving a huge velocity from the discontinuous world position.
4. Final controller/camera resolution uses the resulting pose. Rigid-body processing remains on its previous path.
5. Portal captures use the final current-frame player camera.

The eye is derived from the current capsule and attached camera's relative location. Reading the camera's cached world location during a deferred `FScopedMovementUpdate` can otherwise use the previous pose.

The previous end-of-frame transfer was specifically reproduced failing at a 0.333333-second timestep: the player walked past the entry room's floor edge, fell to Z=58.272, and then attempted to transfer into the destination floor. The new submove commit resolves the destination before the next floor test.

`FInteriorPortalCameraState` owns the active view quaternion. Mouse yaw/pitch compose around camera-local up/right axes. A second transfer composes the current view directly. Optional horizon recovery begins after capsule clearance and preserves look direction. The capsule remains upright under world gravity; movement intent is projected into the horizontal movement plane. This does not introduce arbitrary gravity or ragdoll support.

Held objects also preserve angular velocity when they are transferred with the player. The existing flashlight-clearance change is retained.

## Validation evidence

Bundled UE 5.8 project-file generation and Development Editor build passed: `Saved/Logs/PortalP34ProjectFiles.log`, `Saved/Logs/PortalP34Build.log`.

Focused Automation evidence consists of the five successful unaffected contracts in `Saved/AutomationReports/PortalP34/index.json`, plus the repaired `CharacterMovementGate` and affected `Interior.Flashlight` rerun in `Saved/AutomationReports/PortalP34Gate/index.json`. The initial combined report retains its historical fixture failure; it is not a clean six-test run. The failed fixture was corrected to initialize actors before controller discovery. Final rerun: **2/2 PASS**. Reused successful portal tests: rigid mapping, oblique projection, placement/lifecycle, swept capsule aperture and quaternion camera.

The actual-map replay is `tools/validate_portal_player_reversal.py`, run through the UE Python plugin in `/Game/House/L_Interior_LivingKitchen` PIE. It supplies forced movement input while temporarily masking physical keyboard/look input, restores input and speed afterward, and never saves the test world. `t.OverrideFPS` controls simulation timestep; these runs are not GPU framerate measurements.

- **3 Hz simulation, 260 cm/s:** 100/100 crossings passed; one reversal settled at the protected plane. `Saved/PortalPlayerReversal_3Hz.json`. This is the formerly failing large-timestep regression.
- **30 Hz simulation, 260 cm/s:** 20/20 crossings and 20/20 protected reversals passed. `Saved/PortalPlayerReversal_30Hz.json`.
- Further matrix evidence is recorded after each completed run below.

## Remaining acceptance

**USER ACTION REQUIRED — visual acceptance:** in `L_Interior_LivingKitchen`, slowly enter a door, reverse before clearing, then move fully clear and return; strafe at its edge and jump through while moving the mouse. Observe no wall escape, launch, abrupt roll snap or stale collision. Repeat wall/floor and ceiling configurations. The automated wall-to-wall replay does not establish these camera and arbitrary-orientation visual cells.

P4 for high-speed Chaos rigid bodies remains on the prototype gate path. Player/flashlight remote visual slicing, aperture-limited remote flashlight illumination, capture-owned pre-exposure normalization, generic travellers, PortalQuery and dual-space contact are still open. No Core or Full Physics seal is claimed.
