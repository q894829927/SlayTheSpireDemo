# Interior portal player traversal — P3/P4 delivery

Date: **2026-09-13**. Baseline HEAD: `7d10a8e207446085b1aeaf6edc889f903e0163e4`, including the user's existing uncommitted map, material and flashlight-clearance changes.

Authority: [full-fidelity plan](InteriorPortalFullFidelityImplementationPlan.md), [observed issues](InteriorPortalObservedIssues.md). This delivery addresses the critical player support-wall escape before the remaining visual work. It does not seal the entire P3/P4 or Core Portal Fidelity scope.

## Player passage contract

`InteriorChildCharacter` now uses `UInteriorPortalMovementComponent`. Every movement operation, including slide, step and correction submoves, enters the same aperture constraint before the engine moves the capsule. The supporting primitive is ignored only while that capsule has an active, bounded passage. Other world collision remains active.

The aperture is an inscribed 32-sided polygon inside the visible ellipse. Analytic capsule support functions determine the continuous legal interval along a requested move. This is a conservative fit; it does not rely on a few surface samples as a claim of exact arbitrary-body geometry. A lateral move that leaves the interval produces a blocking hit before leaving the opening. A move that has already cleared the entire wall can continue normally in the room.

The explicit states are `Outside`, `ApproachingEntry`, `IntersectingAperture`, `Transferred`, and `ClearingExit`. During clearance, reversal stops at the exit eye plane until the full capsule has moved in front of the support by its projected normal extent plus 2 cm. A later return is a new legal crossing. At most one commit is allowed in a game frame, including multiple CharacterMovement substeps. This is intentional hysteresis; immediate reversal while still straddling is not treated as another transfer.

Invalidating or destroying an endpoint restores collision only after recovering the capsule to a non-penetrating front-side pose, with the last clear pose as fallback. Routine lateral motion is constrained continuously rather than repaired by a later teleport. Existing move/clear requests remain rejected while passage is busy.

### Wall-stuck correction after baseline `82e2651`

The user reported becoming immobile during slow entry and while standing partway through a portal. The previous C++ constraint returned a zero movement fraction whenever the capsule footprint was initially invalid, including for outward retreat. The regression uses a rim contact followed by a 0.25 cm lateral pose correction to exercise that state. This isolates the lock-prone branch; it does not claim to identify every in-play trigger of the user's symptom.

For an already active passage, an initially violated aperture edge now permits movement parallel to it or back toward its valid side. It forbids increasing that edge's penetration. The normal direction out of the wall therefore remains available, and the hit normal allows CharacterMovement to slide toward the opening. Deeper entry still requires a strict capsule fit. Acquisition, placement and transfer checks keep the original strict aperture. A stale gate for a capsule already clear of the wall is released before the next constraint.

A normal displacement within `UE_SMALL_NUMBER` is treated as numerical noise for the invalid-footprint entry guard. Inverse rotation can otherwise turn a tangent move into an infinitesimal negative normal displacement and block centerward recovery. The native regression includes that residual; ordinary inward movement remains blocked.

This does not enlarge the authored portal or disable wall collision globally. Ordinary edge contacts remain bounded, and outward retreat is continuous. The native movement regression covers a rim contact followed by the same small pose correction, exact outward progress, rejection of deeper entry, movement back toward center, and collision/state cleanup after full retreat.

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

## Overall player presentation pass — 2026-09-13

`UInteriorPortalPresentation` now gives the local player a presentation-only
crossing path. Mesh components tagged `PortalTravellerVisual` keep their
authoritative source material while a no-collision copy is mapped through the
paired logical frame. The map owns five generated material variants under
`/Game/SlayTheSpireDemo/Interior/Portals/`, and the component binds them by
original material so unrelated character meshes are not changed.

The same component maps the attached handheld flashlight into the destination
portal. It creates destination-side movable spotlights only when the cone can
reach the legal aperture, applies `M_LF_PortalFlashlight` to restrict the light
function to that aperture, and restores temporary lighting channels on reset or
teardown. This is a supported first-person effect; it does not claim general
physical light transport.

The reproducible runtime check is
`tools/validate_portal_player_presentation.py`. In a real PIE world it moves
the pawn to the authored blue entry, enables the existing flashlight for one
second, records the presentation state, restores the original pose/light state
and writes `Saved/PortalPlayerPresentationRuntime.json`. The 2026-09-13 run
passed with five bound slice materials, three active remote player visuals and
one active remote light while the crossing state remained `Outside`. The
supplemental capture is `Saved/PortalPresentationOverall.png`; it is useful for
reviewing the portal composition but does not replace the manual visual matrix.

## Portal-aware interaction queries — 2026-09-13

`InteriorPortalQuery::LineTrace` and `InteriorPortalQuery::SphereSweep` now
share the logical frame and mapping math with traversal. Each bounded query
compares the nearest ordinary world hit with analytic crossings of both linked
portals. A hit on the matching support primitive is replaced only when its
distance is within a tolerance derived from support thickness and the query
radius; an unrelated nearer object still blocks. The remaining segment is then
mapped through the paired frame with a finite hop budget.

`TryGrab`, light-switch focus and entrance-door interaction use the line-query
path. The query regression also covers a destination object through a blocking
support, a sphere sweep, an outside-aperture wall hit and an unrelated object
before the portal.

## Validation evidence

Bundled UE 5.8 project-file generation and Development Editor build passed: `Saved/Logs/PortalP34ProjectFiles.log`, `Saved/Logs/PortalP34Build.log`.

Focused Automation evidence consists of the five successful unaffected contracts in `Saved/AutomationReports/PortalP34/index.json`, plus the repaired `CharacterMovementGate` and affected `Interior.Flashlight` rerun in `Saved/AutomationReports/PortalP34Gate/index.json`. The initial combined report retains its historical fixture failure; it is not a clean six-test run. The failed fixture was corrected to initialize actors before controller discovery. Final rerun: **2/2 PASS**. Reused successful portal tests: rigid mapping, oblique projection, placement/lifecycle, swept capsule aperture and quaternion camera.

The actual-map replay is `tools/validate_portal_player_reversal.py`, run through the UE Python plugin in `/Game/House/L_Interior_LivingKitchen` PIE. It supplies forced movement input while temporarily masking physical keyboard/look input, restores input and speed afterward, and never saves the test world. `t.OverrideFPS` controls simulation timestep; these runs are not GPU framerate measurements.

- **3 Hz simulation, 260 cm/s:** 100/100 crossings passed; one reversal settled at the protected plane. `Saved/PortalPlayerReversal_3Hz.json`. This is the formerly failing large-timestep regression.
- **30 Hz simulation, 260 cm/s:** 20/20 crossings and 20/20 protected reversals passed. `Saved/PortalPlayerReversal_30Hz.json`.
- Further matrix evidence is recorded after each completed run below.

Wall-stuck correction: final bundled project-file generation and Development Editor build passed (`Saved/Logs/PortalWallStuckFinalProjectFiles.log`, `Saved/Logs/PortalWallStuckFinalBuild.log`). Final focused `SlayTheSpireDemo.Interior.Portals` Automation: **6/6 PASS**, 0 failed / notRun (`Saved/AutomationReports/PortalWallStuckFinal/index.json`, `Saved/Logs/PortalWallStuckFinalTests.log`). This includes the tangent-direction residual correction and supersedes the earlier movement-gate result for the changed recovery contract.

Actual-map replay `tools/validate_portal_player_wall_stuck.py`, at **60 Hz simulation / 20 cm/s: 6/6 PASS** (`Saved/PortalPlayerWallStuck.json`). For each authored endpoint it checks entry pause/resume plus exit pause/resume, partial-entry retreat, and rim strafe followed by retreat. All cases leave the passage state `Outside`; there are no unexpected transfers, stuck timeouts or floor-loss assertions. Input masking and the original 260 cm/s walk speed were restored; the simulation timestep override was reset afterward.

The corrected actual-map footprint probe passed separately (`Saved/PortalWallStuckFixedAcceptance.json`): after a 0.25 cm rim correction, outward retreat advances 8 cm, deeper wall motion advances 0 cm, movement toward center advances 10 cm, and full retreat restores `Outside`. These are state/movement checks through MCP and UE Python, not manual visual acceptance.

Discard the earlier ad hoc `Saved/PortalWallStuckBaseline.json` and `Saved/PortalWallStuckFixed.json` probe results: UE Python unary vector negation mutated the reused direction variable, so they mislabeled inward blocking as outward blocking. The corrected probe constructs deltas from immutable scalar coordinates. The native C++ regression and the six-case replay do not reuse a negated direction in later calculations and remain valid.

## Remaining acceptance

**USER ACTION REQUIRED — visual acceptance:** in `L_Interior_LivingKitchen`, slowly enter a door, reverse before clearing, then move fully clear and return; strafe at its edge and jump through while moving the mouse. Observe no wall escape, launch, abrupt roll snap or stale collision. Repeat wall/floor and ceiling configurations. The automated wall-to-wall replay does not establish these camera and arbitrary-orientation visual cells.

P4 for high-speed Chaos rigid bodies remains on the prototype gate path. Player/flashlight remote visual slicing, aperture-limited remote flashlight illumination and PortalQuery now have implementation candidates with state-level evidence; their manual matrix, capture-owned pre-exposure normalization, generic travellers and dual-space contact remain open. No Core or Full Physics seal is claimed.
