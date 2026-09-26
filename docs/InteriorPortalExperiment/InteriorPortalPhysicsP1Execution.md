# Portal Physics P1 execution

Updated: 2026-09-26.
Status: **PHY-0 baseline and PHY-1 identity/geometry foundation COMPLETE; PHY-2 NEXT; production physics migration not yet accepted**.
Source baseline: `78057a34022e0f979be887a9ad7f97038c3350bf`. This delivery
adds the diagnostics and identity/geometry foundation and records the sealed
Performance P1 acceptance. The user requested one commit after PHY-1 completion;
the pre-existing modified map is excluded.

The authority for ownership, supported geometry, holding, contacts, transfer,
recovery and cleanup remains
[the rigid-body interaction contract](InteriorPortalRigidBodyInteractionDesign.md).
This document orders its migration and records execution; it does not weaken
the Core-before-P8/P9 gates in the full-fidelity implementation plan.

## Ordered delivery

| Stage | Deliverable | Dependency / gate |
|---|---|---|
| PHY-0 | Reproduce carrying oscillation; correlate hold target, shape eligibility, support bypass, recovery and transfer | Performance P1 sealed; observe existing behavior before changing motion |
| PHY-1 | Stable traveller/generation identity and collision-shape adapters; shared P10 passage/query eligibility | PHY-0 evidence; sphere/capsule/oriented box/convex/compound support explicitly declared and tested |
| PHY-2 | Narrow UE/Chaos boundary spike with immutable snapshots and result handoff | PHY-1; prove contact timing and supported transfer interval, including substeps; reject unsupported passages safely |
| PHY-3 | Bounded hold drive and route/target solver using the same registered geometry | PHY-2; prove force/torque bounds and blocked/rotated target behavior in actual Chaos |
| PHY-4 | One passage/crossing coordinator for free and held bodies; transactional transfer/recovery/cleanup | PHY-3; remove superseded writers together; Core physical acceptance before P8/P9 |

PHY labels distinguish this execution sequence from rendering P1 and the older
feature-area P5/P6/P10 numbers. Do not introduce a second production motion path
alongside the legacy writers. Read-only PHY-0 diagnostics are not a physics fix.

## PHY-0 observation contract

`portal.PhysicsDiagnostics 1` emits compact JSON prefixed `PortalPhysicsTrace`
to the normal UE log; default is 0. Turn it off after a short capture. Records
include body path, game frame, observed pose/rotation/velocities, held/linked
state, route entry and these events:

- `HoldTarget`: mapped eye, desired pose, actual drive target, fixed sphere
  radius, query-ignored support components and blocking hit.
- `GateEvaluation`: current/predicted shape fit, wall distance, support extents
  and prediction interval at each evaluated entry.
- `ContactPermission`: selected support bypass, actual constraint presence,
  remembered exit and committed transfer count.
- `Recovery`: observed input pose, last-safe pose, resulting pose/velocity and
  the existing outside-aperture/inside-support reason.
- `Transfer`: committed entry/exit, resulting body state and transfer count.

The current implementation invokes gate evaluation from `PrePhysicsTick` and
`PostCameraTraversal`. Those names identify game-thread observation sites;
they are **not** Chaos pre/post-solve hooks. In the original PHY-0 capture,
`physicsStep`, `bodyGeneration` and `pairGeneration` are explicitly `UNAVAILABLE`
because that baseline has no such tokens. PHY-1 now supplies registry/body/pair
identities; `physicsStep` remains `UNAVAILABLE`. Paths correlate the baseline within one PIE session and
must not become the future registry's authoritative IDs. Exit-state decisions
and blocked-exit rollback still require separate investigation if implicated;
the trace does not claim full solver/contact coverage.

The observers must not alter target selection, fit, contact policy, body
position, velocity or transfer behavior. No renderer/asset/public gameplay API
changes, global wall-ignore workaround or rollback suppression belong here.

## Baseline reproduction and evidence

Use `L_Interior_LivingKitchen` in fresh PIE. Preserve the authored map and test
temporary runtime fixtures only. Compare no portal/unlinked portal/linked pair,
first holding steady and then carrying parallel to the support near the rim.
Record actual body collision, mass, hold settings, player/body transforms,
frame/substep settings and the wall/support identity. A runtime scripted holder
trajectory is a reproducible diagnostic fixture, not manual visual acceptance.

Correlate records by body and game frame/boundary. Determine whether observed
body jumps come from repeated `Recovery`, actual transfers, drive/contact
response or a display-only temporal trail. The earlier pull-versus-rollback
description remains a hypothesis until this capture establishes it. Do not
diagnose from source inspection alone or tune new acceptance thresholds after
seeing the outcome.

Validation for PHY-0: prescribed UE 5.8 project generation/build, the affected
existing traveller registration Automation, and actual diagnostic PIE captures.
No test should duplicate log formatting. Real contact/force/energy and Core
visual gates belong to the migration stages; diagnostics do not satisfy them.

## Validation and next action

UE 5.8 bundled .NET project generation PASS. Development Editor build PASS
after correcting the initial JSON link failure with the engine's private `Json`
module dependency. No third-party dependency, compiler policy or plugin was
added. Focused existing `SlayTheSpireDemo.Interior.Portals.TravellerRegistry`
Automation ran with NullRHI: **1/1 PASS**. This does not prove Chaos contacts.

Actual D3D12 PIE captured 1,080 ticks, 360 each for no placed portal, blue-only
unlinked portal and the linked pair. The authored cube was used: engine basic
cube mesh, uniform scale 0.4, observed mass 12.000001 kg. Legacy support extents
and fit results are recorded by the trace; the mesh's visual bounds are not
accepted as the future collision-shape adapter. Physics substepping was false;
FPS cap was 30. This is not a fixed-step solver acceptance run.

The fixture starts the cube at (1300, 845, 120.15), holder at
(1300, 780, 72.15), yaw=90/pitch=0. It grabs through the existing handle, holds
for 90 ticks, moves holder X with `1300 + 60*sin(2*pi*(n-90)/120)` for ticks
90..269, then holds X=1350 for 90 ticks. Player Y and portal poses remain fixed.
The fixture moves the holder through native runtime APIs; it does not set cube
pose or velocity during the held interval. Legacy handle stiffness/damping:
linear 750/200, angular 1500/500. All editor/map changes are confined to PIE.

| Scenario | HoldTarget records | Transfer events | Actual Recovery moves | Maximum recovery displacement | Contact-permission selection changes |
|---|---:|---:|---:|---:|---:|
| No portal | 360 | 0 | 0 | 0 cm | 0 |
| Unlinked portal | 360 | 0 | 0 | 0 cm | 0 |
| Linked pair | 360 | 1 | 98 | 46.2149 cm | 136 |

The last column counts changes between successive game-thread permission
records, including both observation boundaries; it does not count actual Chaos
contact manifold transitions. All 360 linked HoldTarget records ignored a
support in the query. No/unlinked queries ignored none. All emitted JSON
parsed successfully.

### Observed causal sequence

The linked fixture first transfers the held cube once and then carries it in
the remote exit space. At game frame 828, the legacy fit is initially legal;
the hold query ignores the exit support and targets approximately
(1779.0000, 1244.5887, 120.1500). The later gate prediction rejects continued
passage and restores support contact. At frame 829, the hold target still
advances to Y=1246.6288. The resulting body pose no longer fits the aperture,
and Recovery moves it from approximately (1773.7950, 1244.4498, 118.7844)
back to (1778.9787, 1240.1213, 118.8788). Frame 830 permits contact bypass
again at that restored legal pose, while the drive target advances to
Y=1248.5410. The contradictory target is not synchronized by Recovery.

Thus this linked, remote-held rim scenario establishes real recovery teleports
and alternating support policy competing with an unreachable drive target.
The jumps are not repeated portal transfers or merely a rendering trail.
This refines the original hypothesis: an already mapped holding route can
ignore the whole exit support independently of shape eligibility. It does not
prove the identical path in every original user camera/holding case. No fix,
Core physical stability acceptance or high-speed/substep support is claimed.

Local evidence (ignored):

~~~text
Saved/PortalPhysicsP1/capture.py
Saved/PortalPhysicsP1/capture.json
Saved/PortalPhysicsP1/complete.txt = COMPLETE
Saved/PortalPhysicsP1/analyze.py
Saved/PortalPhysicsP1/trace.json
Saved/PortalPhysicsP1/summary.json
Saved/Logs/PortalPhysicsP1Capture.log
Saved/Logs/PortalPhysicsP1Automation.log
Saved/AutomationReports/PortalPhysicsP1/index.json
~~~

The capture ended PIE and exited the editor normally. Diagnostics, FPS cap and
temporary editor throttling were restored. Map SHA-256 still matches the
pre-existing dirty map: `0436F682DCD867E43ECEA81E89D4916D8B8ECFB2E66CA9321997BC97038BE25C`.

**PHY-0 is COMPLETE for this baseline. Its subsequent PHY-1 task:** replace implicit list
identity and bounds-based eligibility with generation-bearing traveller records
and actual collision-geometry adapters shared by held/free/P10 queries. Establish
pure geometry/route tests first, then the PHY-2 solver-boundary spike before
switching contact or drive authority. Do not fix this finding by turning off
Recovery, widening the portal, changing the 125 cm preference or adding a
second force/position writer.

## PHY-1 delivered foundation

`InteriorPortalTravellerRegistry` owns game-thread registration/configuration
only: registry epoch, monotonically allocated body ID, generation and copied
collision geometry. A body keeps its ID across unregister/re-register with a
new generation; removing another body cannot renumber it. Reset never reuses
IDs. Separate registries have different epochs. Geometry edits and component
scale changes invalidate old snapshots immediately; refresh issues a newer
generation. Invalid/destroyed or unsupported travellers cannot authorize a
query. The system retains legacy indexed bookkeeping solely for the existing
motion path until the PHY-4 cutover; those indices are not identity.

Portal-pair generation covers endpoint/support identity, logical pose, aperture
dimensions and placement. A mutation between refresh boundaries makes the
current token unavailable (0), rejecting outstanding queries until refresh.
These are game-thread configuration generations, **not** simulation-step IDs
or transfer revisions. PHY-2 owns solver snapshots/handoff; PHY-4 must invalidate
outstanding physical results on committed transfer/recovery. The registry does
not duplicate Chaos pose, velocity or passage authority.

`InteriorPortalBodyGeometry` extracts simple collision from a root, unwelded
ShapeComponent or StaticMeshComponent's BodySetup. It never derives shape from
render bounds. Sphere/box/capsule scaling uses the engine helpers used by Chaos;
convex element transforms are baked before component scaling, matching BodySetup
cooking. Positive nonuniform component scale is supported and baked once.
Complex-as-simple, unsupported aggregate element types, mirrored/degenerate
scale, missing convex vertices and non-root/welded components are rejected.
Compound geometry requires every physics-enabled element to fit. Authored
convex hull containment is conservative; this stage does not claim measurement
of cooked contact margins or solver manifolds.

| Geometry / motion | PHY-1 eligibility rule |
|---|---|
| Oriented box / convex | All collision vertices lie in the elliptical aperture cylinder |
| Sphere | Conservative entire-ball bound in ellipse-normalized coordinates |
| Capsule | Both endpoint balls fit; convexity contains the intervening capsule |
| Compound | Every supported physics element passes |
| Linear translation with fixed orientation | Both endpoint projections fit; convexity bounds the complete segment |
| Continuous rotation | `ROTATION_SWEEP_UNSUPPORTED`; endpoint fit never proves a rotating sweep |

Sphere/capsule and clearance margins use the conservative normalized bound
`length((y/halfWidth,z/halfHeight)) + (radius+margin)/min(halfWidth,halfHeight) <= 1`.
Subtracting radius from both ellipse axes can incorrectly accept an escaping
ball and is not used. The bound may reject some geometrically legal placements;
no portal dimensions or physical opening are enlarged to compensate. The
default body-query clearance is 0.5 cm; this is a declared geometric margin,
not a tuned physical-stability acceptance threshold.

`InteriorPortalQuery::EvaluateBodyPassage` checks current body/pair identities
and calls this one geometry implementation. Held-target and free-body predicted
translation diagnostics both use it. `FITS` proves the stated aperture-cylinder
containment only: it does not prove crossing direction, wall-contact timing,
exit clearance, unrelated-world obstruction or a supported transfer interval.
An eventual route solver must restrict motion to the relevant support interval
and handle swept rotation or safely reject it.

**Production boundary:** old hold SphereSweep (28 cm), whole-support query
arbitration, legacy BodyFits gate and Recovery remain the active compatibility
path. PHY-1 adds no force, pose or contact writer and does not use a diagnostic
result to override those decisions. P10's body eligibility foundation is shared;
its full route/obstruction arbitration is not yet migrated. Swapping only one
legacy predicate now would mix authorities before the PHY-2 timing proof.

### PHY-1 actual validation

Prescribed UE 5.8 bundled .NET project generation PASS; Development Editor
build PASS. Initial failures were the missing private engine `PhysicsCore`
dependency and a test type ambiguity with Slate's `FGeometry`; both were fixed.
No plugin, third-party dependency or public gameplay/Blueprint API changed.

Focused NullRHI Automation ran
`SlayTheSpireDemo.Interior.Portals.PhysicsFoundation+SlayTheSpireDemo.Interior.Portals.TravellerRegistry`:
**5/5 PASS**, zero warnings. Tests cover rotated/long/corner/compound/convex fit,
an independent unsafe-sphere counterexample, transformed logical frames,
fixed-orientation sweep samples, explicit rotational rejection, actual BodySetup
scaling, non-render-bound geometry, unsupported collision, registration/reset
identity, geometry mutation and stale body/pair rejection. This is geometry and
lifecycle validation; it does not prove Chaos contact/force limits.

Final lifetime review added rejection of a pending-destroy system owner and
extended the existing GenerationAndSharedQuery test with owner/body destruction.
The editor target was rebuilt successfully; only this affected test was rerun:
**1/1 PASS**, zero warnings, in
`Saved/AutomationReports/PortalPhysicsPHY1Lifetime/index.json` and
`Saved/Logs/PortalPhysicsPHY1LifetimeAutomation.log`. Unchanged geometry tests
remain valid; these run totals are not combined. The subsequent owner-validity
guard does not change the valid PIE path captured below, so that evidence is
reused rather than repeating the fixture.

Actual D3D12 PIE ran the existing linked fixture for 360 ticks on
`L_Interior_LivingKitchen`, without saving the map. The 1,896 diagnostic records
all parsed and carried one stable body ID/generation, one pair generation and
one extracted collision primitive; physics-step remained explicitly unavailable.
Shared held-target eligibility: 177 `FITS`, 183 `OUTSIDE_APERTURE`. Shared
predicted-translation eligibility: 399 `FITS`, 318 `OUTSIDE_APERTURE`. The legacy
path still performed one transfer and 98 recovery moves, maximum 46.2149 cm,
with 136 observed permission-selection changes. This validates runtime adapter
and diagnostics integration and confirms that the physical fix has not yet
been applied. It is not manual visual acceptance or substep testing.

The scripted shutdown issued `QUIT_EDITOR` immediately after requesting PIE
end, producing the editor's "currently in a play mode" utility error; teardown
nevertheless ended PIE and exited normally. Capture status is COMPLETE,
diagnostics/FPS/throttling were restored, and no editor process remains. This
shutdown message is not a physical validation failure. Map SHA-256 remains
`0436F682DCD867E43ECEA81E89D4916D8B8ECFB2E66CA9321997BC97038BE25C`.

Local evidence (ignored): `Saved/PortalPhysicsPHY1/{capture.py,capture.json,
complete.txt,analyze.py,trace.json,summary.json,foundation.json}`,
`Saved/Logs/PortalPhysicsPHY1{Capture,Automation}.log`, and
`Saved/AutomationReports/PortalPhysicsPHY1/index.json`.

No manual visual gate is required for this read-only foundation. Future Core
physical/visual gates remain required; no user acceptance of the oscillation
fix is claimed.

## Next action: PHY-2

Run the narrow UE 5.8 / Chaos boundary spike. Establish immutable physics input
and result handoff carrying body/pair/step identities; prove when exact
body/support contact permissions and bounded drives can apply relative to solve,
including declared substep configurations. Establish the supported transfer
interval and fail closed outside it. Do not promote game-thread Tick labels
into solver callbacks, introduce remote-half contacts (P8), or switch only one
of the legacy competing motion/contact branches before the timing proof.
