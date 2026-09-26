# PHY-4 slice 2 — solver-bound static clearance

Updated: 2026-09-26. Status: **NATIVE STATIC CERTIFICATE VALIDATED; general provider/cutover/Core OPEN**.
Source HEAD: `33492d01471b2025f7d068dc090c0e4b1ac2ee37` (world observations/native adapter committed).
Authority: [PHY-4 ordering](InteriorPortalPhysicsPHY4.md).

## Supported profile and ownership

Read native Chaos particles at the actual PreIntegrate, PostIntegrate and PostSolve
hooks instead of promoting a game-thread world query to authority. Bind one
body/geometry generation, equal-scale logical pair, solver and unique binding
epoch to exact support proxies/transforms. A topology/configuration mismatch
permanently retires that binding; new topology requires a new binding.
Support bounds match the frozen collision descriptor. Native geometry root
identity/type hash is latched at the first boundary and cannot change within
that binding. Actual bounds remain the containment authority; a hash alone is
not a geometric containment proof.

The first supported certificate covers one independently simulated, fixed-rotation
body translating normally through a static scene. Any other simulation-enabled
dynamic, sleeping or kinematic particle anywhere in the solver rejects this
profile. This deliberately strict whole-scene scope cannot yet be the production
provider in a world with players/other rigid bodies. Non-normal velocity or
acceleration, spin, angular acceleration and changed orientation also reject.
Persistent particle constraints (joints, etc.; not transient collisions) reject
before integration, because their solve can move a body beyond this independent
motion profile. A rotational hold drive must therefore reject even from zero
initial angular velocity. Normal static cancellation
remains the proved PHY-2 policy; arbitrary straddling recovery is still open.

Use the selected body's actual native geometry bounds, compare them to bound
registered geometry bounds, and conservatively contain all native box corners
inside each aperture. Sweep conservative world bounds through the source approach
and full mapped exit-clearance corridor. Scan every non-disabled particle with
simulation-enabled shapes, irrespective of query enablement. `PhysicsOnly`
obstacles participate; simulation-disabled query-only shapes do not. Collision
responses are conservatively over-blocked in this first profile. Unknown/unbounded
geometry, clusters or more than 4096 native particles reject rather than truncate.
Scene scan reasons use fixed priority, not discovery order.

A private certificate freezes binding, command, actual interval samples,
solver step, callback stage and issuance sequence. Consume once at that same
stage/interval; older stage, changed command/sample or repeated consumption fails.
Re-scan actual physics state at every stage; do not reuse a previous substep or
game-thread observation. The verifier reads Chaos only, never UObjects and never
writes physical state. The existing editor callback alone translates consumed
proof into its internal command's permission/certificate fields. No active map
writer is replaced or duplicated.

## Predeclared validation

Reuse the native fixture and TaskGraph one/two-substep profiles. Clear static
scene commits exactly once with preserved native material mapping and durable
fact consumption. A PhysicsOnly exit blocker must be invisible to the GT observer
but rejected at the next actual solver boundary, with zero transfer and ordinary
blocking (unchanged X > 6.8 cm threshold). Adding that blocker during partial
insertion at X=2 must cancel before affected integration and restore contacts.
No game-thread pose Recovery is permitted in the cancellation test.
The certificate case covers both free and held bodies with the existing
coordinator association; it does not simulate a production holder particle.

Reject any other dynamic/kinematic particle, non-normal/spinning motion, applied
rotation drive from zero spin, persistent world joints, body
shape mismatch and support movement; do not revive a retired binding after its
support returns. Prove stale step/stage/command/sample and duplicate certificate
consumption fail. Two substeps must issue different physical step certificates.
Current requested-input lifetime, exact contact-pair arbitration, transfer
mapping, adapter failure atomicity, held/free relation and durable fact gates
remain unchanged. Mapping tolerance 1e-6; no numeric relaxation.

Run prescribed bundled UE 5.8 project generation/build and focused affected
Automation. No manual gate applies to this isolated native profile. Production
Core manual acceptance remains required after full supported coverage and atomic
cutover; do not request acceptance of unchanged legacy gameplay.

## Evidence and next action

Prescribed bundled UE 5.8 project generation and initial/final editor builds PASS.
Initial focused run: **18/20 PASS**, 2026.09.26-15.23.05 UTC. NativeBindingRetirement
failed because the component API refused to move a registered Static component
(two warnings); NativeStaticAndCertificate detected post-transfer rejection.
The test now moves the actual static physics body through BodyInstance, rather
than pretending the refused component move changed topology. Diagnostic rerun:
**1/2 PASS**, 2026.09.26-15.26.19 UTC, zero test warnings; retirement passed,
static certificate still failed with UnsupportedMotion after transfer.

Diagnosis: the shared mapping helper built its half-turn from single-precision
`PI` promoted to double. A -120 cm/s inward vector acquired approximately
0.00001049 cm/s of lateral velocity, outside the declared 1e-6 normal-motion
test. The helper now uses the exact quaternion `(0,0,1,0)`, shared by physics,
holding routes, world observations and virtual cameras. No separate physics
mapper or relaxed tolerance was added. RigidMapping now asserts exact axial
momentum and its repeated half-turn restoration. The hold route's independent
reference is the exact ideal point, replacing its historical float-PI reference.

Rebuild PASS. Final affected run **29/29 PASS, zero test warnings**,
2026.09.26-15.31.28 UTC: PhysicsBoundary (4), PhysicsHold (4), PhysicsCoordinator
(6), PhysicsSolverClearance (4), PhysicsWorldQuery (5), RigidMapping,
QuaternionCamera, VirtualViewRequest, FullFidelity.RecursiveRenderRequest,
FullFidelity.AnalyticApertureGeometry and PortalAwareQuery (one each). Shared
mapping changed, so these directly affected geometry/route/camera gates were
revalidated; initial/diagnostic failures remain recorded as separate runs.

Subsequent supported-motion review added native angular-acceleration and
persistent-constraint rejection before integration. Tests apply real bounded
rotation-drive torque from zero initial spin and bind a real world joint; both
reject before support bypass. Rebuild PASS; affected PhysicsSolverClearance
**4/4 PASS, zero test warnings**, 2026.09.26-15.41.14 UTC. The other 25 unchanged
gates reuse the preceding evidence. This separate rerun is not a 33-test aggregate
or a repeated full 29-test run.

Native static certificate results after 12 game dispatches:

| Intervals/dispatch | Held | Native samples | Scans/sample | Transfers / consumed fact revision | Final X (cm) |
|---|---|---:|---:|---|---:|
| 1 | no | 12 | 3 | 1 / 1 | 1016.000001 |
| 1 | yes | 12 | 3 | 1 / 1 | 1007.158179 |
| 2 | no | 24 | 3 | 1 / 1 | 1016.000001 |
| 2 | yes | 24 | 3 | 1 / 1 | 1007.305337 |

All three stages certify fresh physics state; wrong command/sample/step/stage,
previous-stage proof and duplicate consumption fail. Native material and facts
remain coherent. PhysicsOnly exit obstruction is absent from the GT query but
detected at the first affected PreIntegrate. No transfer occurs. Partial insertion
at X=2 restores through original contacts to X=7.000000 / 7.000001 (one/two-step).
Without prior insertion, ordinary collision produces X=13.610278 / 13.917461.
No test-side body-pose Recovery is used. These transient normal cancellations
do not prove general straddling energy/steady-state oscillation acceptance.
Other dynamic/kinematic particles, tangent velocity and spin reject. Native
support movement, body extent mutation and unversioned aperture changes retire
the binding; returning the wall to its old native pose does not revive it.

Evidence (ignored):
`Saved/AutomationReports/PortalPhysicsPHY4Solver{,Diagnostic,Final,Motion}/index.json`,
`Saved/Logs/PortalPhysicsPHY4Solver{,Diagnostic,Final,Motion}Automation.log`,
`Saved/Logs/PortalPhysicsPHY4Solver{ProjectFiles,InitialBuild,Build,FinalBuild,MotionBuild}.log`.
No new active gameplay writer or map/asset edits; the shared rigid mapping's
half-turn precision is corrected. No manual PIE was performed. Map hash remains
`0436F682DCD867E43ECEA81E89D4916D8B8ECFB2E66CA9321997BC97038BE25C`.

Next: replace strict whole-scene rejection with proven native simulation filtering
and conservative moving-obstacle coverage at the actual interval, so unrelated
dynamic/kinematic bodies need not deny a legal static-support passage. Retain
Core's blocked-exit policy; do not imply P8 remote-half contact support. Prove
gravity/rotation/tangential cancellation, topology changes while straddling,
exceptional recovery and final pending-fact handoff before production cutover.
