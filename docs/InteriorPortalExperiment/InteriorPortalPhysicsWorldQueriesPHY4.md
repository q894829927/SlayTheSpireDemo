# PHY-4 slice 2 — world observations and native transfer primitive

Updated: 2026-09-26. Status: **OBSERVATION/TRANSFER PRIMITIVES VALIDATED; full slice 2 and production cutover OPEN**.
Source HEAD: `f6ed5dfd332c4ad083ba4439da411e7dc720033e` (coordinator committed).
Authority: [PHY-4 ordering](InteriorPortalPhysicsPHY4.md).

## Implemented scope and authority

Separate a game-thread collision observation from a physics-substep permission.
The world observer uses registered, baked collision geometry, exact component
exclusions and actual body collision responses. It checks the requested linear
interval and a normal extension to the COM crossing plane, mapped interval,
and full destination shape-clearance corridor plus one bounded interval budget.
Both endpoint ellipses must contain the entire compound shape before excluding
the corresponding support in that query. Initial overlap blocks too.

Spheres/capsules retain authored centers and orientations. Boxes and convex
hulls use conservative per-element local bounding boxes for world queries;
aperture containment still uses actual vertices. A conservative query may reject
an otherwise legal path; it cannot authorize an uncovered vertex. Traveller
root/unwelded extraction remains unchanged. Static supports may be non-root
components and use the same collision extraction, never render bounds.

`ClearAtQuery` is advisory only. Query-disabled physics obstacles, future dynamic
obstacle motion, scene synchronization, support/topology revisions and lease
correlation with a solver interval still require a certified provider. The
observer never changes `PermitSupportBypass` or `ExitCorridorCertified`, collision
filters, poses or velocities. Fixed-orientation translation only is supported;
rotation, changed geometry and invalid/static-support profiles reject.

The reusable runtime Chaos transfer primitive binds proxy/body/pair/solver and
unique binding domain. The caller arms it only at the actual PostSolve boundary
and retires it before proxy removal. It compares fact `Before` with native
solved P/Q, V/W and material state, checks finite `After`, preserves mass/inertia/
COM/mass frame/sleep and excludes duplicate writes. Every validation occurs
before the first setter. It owns no ordinary integration and discovers no
UObjects. It replaces only the inline adapter in the existing editor fixture;
production gameplay remains on the legacy path until atomic cutover.

## Predeclared automated gates

Reuse the existing native scene; do not add another simulated-world fixture.
Observe clear paths and actual collision thickness; reject source initial overlap,
destination blockers across the complete clearance corridor, dynamic observed
blockers, other colliders on the held body's owner, undersized destination,
rotation and stale geometry/identity. Prove non-root static support extraction
without relaxing traveller policy. Query checks use full volumes rather than
center rays or a fixed-radius approximation.
Include actual sphere, rotated capsule and offset compound-sphere travellers;
the distal mapped primitive must detect an obstacle that misses the root path.

Native TaskGraph one/two-interval profiles: query-observed blocked exit prevents
entry; introducing a blocker during partial insertion and then revoking the
fixture's authored permission restores normal contacts with no transfer. Use
the unchanged PHY-2 blocking threshold (body X > 6.8 cm), no test pose Recovery
and no relaxed tolerances. This narrow zero-gravity normal case does not prove
arbitrary straddling topology cancellation or exceptional recovery.

Native adapter rejects wrong binding/step, mismatched solved state, changed
material and nonfinite output before any write; the same interval's valid fact
can then commit once. Replaying it performs no write. Existing held/free,
material mapping, delayed facts, cancellation and identity geometry regressions
must pass. Position/mapping tolerance stays 1e-6. Run prescribed bundled UE 5.8
project generation/build then focused affected Automation.

No manual gate applies to these isolated primitives. Actual-map Core manual
acceptance remains in the parent document after certified coverage and atomic
production cutover; no carrying-fix claim is made by this slice.

## Evidence and next action

Prescribed bundled UE 5.8 project generation PASS. First build exposed a
pre-existing unity collision between the hold solver/coordinator's anonymous
`RigidFrame` helper names after committed files re-entered the unity build.
Renamed the coordinator helper; rebuilt PASS without changing its behavior or
build settings. The subsequent shape-test build also PASS.

Initial focused NullRHI run: **16/16 PASS**, 2026.09.26-14.54.24 UTC.
Scope: PhysicsFoundation (4), PhysicsCoordinator (6), PhysicsWorldQuery (4),
NativeContactAndSubsteps (1), NativeTransferAndRetirement (1). One existing
foundation test passes with **two warnings** (`UWorld::DestroyActor: World has
no context`, GenerationAndSharedQuery fixture); the other 15 tests have none.
These warnings are retained and are not claimed as zero-warning evidence.
Added primitive coverage, rebuilt, ran only PrimitivesAndCompound:
**1/1 PASS, zero warnings**, 2026.09.26-14.57.19 UTC. The preceding 16 unchanged
tests reuse their evidence; these are separate runs, not a 17-test aggregate.

Native TaskGraph results, 12 blocked dispatches after the observation/revocation:

| Intervals/dispatch | Partial insertion | X before cancellation (cm) | Final X (cm) | Transfers |
|---|---|---:|---:|---:|
| 1 | no | 8.000000 | 13.610278 | 0 |
| 1 | yes | 2.000000 | 7.000000 | 0 |
| 2 | no | 8.000000 | 13.917461 | 0 |
| 2 | yes | 2.000000 | 7.000001 | 0 |

Zero-duration registration flush leaves the selected body at the same pose;
original wall contacts, rather than a game-thread recovery setter, produce the
outward response. This is narrow normal-approach cancellation, not an arbitrary
straddling, energy or steady-state oscillation acceptance gate. The test-only
bridge revokes the existing fixture's authored permission; it does not certify
the game-thread observation for production physics.

Native runtime-adapter checks reject wrong binding/step/solved state, changed
mass and nonfinite velocity without any pose/velocity write; valid commit in
that same interval succeeds once and duplicate commit fails. Existing native
held/free material mapping, fact acknowledgment and retirement regressions pass.
Sphere, rotated capsule and offset compound-sphere probes pass using actual
registered bodies; the offset destination blocker identifies primitive 1.
General convex-hull query performance/precision and rotating traversal are not
native acceptance from these cases.

Evidence (ignored):
`Saved/AutomationReports/PortalPhysicsPHY4World{,Shapes}/index.json`,
`Saved/Logs/PortalPhysicsPHY4World{,Shapes}Automation.log`,
`Saved/Logs/PortalPhysicsPHY4World{ProjectFiles,InitialBuild,Build,ShapeBuild}.log`.
No map/system cutover or manual PIE was performed. Pre-existing map hash remains
`0436F682DCD867E43ECEA81E89D4916D8B8ECFB2E66CA9321997BC97038BE25C`.

Next: correlate topology/support/body revisions and scene coverage with the
actual solver interval; include query-disabled physics collision or explicitly
reject it, and revalidate dynamic obstacles at the authoritative boundary.
Prove supported gravity/rotation or bounded safe rejection, topology changes
while straddling, exceptional recovery and final journal handoff. Only after
those slice-2 gates pass may the old production writers be removed together.
