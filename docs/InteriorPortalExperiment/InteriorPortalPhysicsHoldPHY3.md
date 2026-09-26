# PHY-3 — bounded hold drive and feasible targets

Updated: 2026-09-26. Status: **NARROW DRIVE/TARGET PROFILE VALIDATED; PHY-4 NEXT; production migration and Core acceptance OPEN**.
Source HEAD: `b4dec6faafd19f8e234f2b7b6c4fd9fe59f91a95` (PHY-2 delivery).
Authority: [rigid-body contract](InteriorPortalRigidBodyInteractionDesign.md)
and [ordered migration](InteriorPortalPhysicsP1Execution.md).

## Implementation boundary

Implement value-only hold intent, explicit direct/single-pair route, static
obstruction snapshot, feasible-target result and bounded drive result. Reuse
registered collision shapes and the actual PHY-2 native solver fixture. Do not
attach another writer to production PhysicsHandle/gate/Recovery; PHY-4 removes
those writers together. This phase does not close the carrying defect.

The first obstruction provider is a certified convex static free region expressed
as ordered inward half-spaces. Each plane constrains the entire registered
sphere/capsule/box/convex/compound geometry. This is an explicit limited query
profile, not a general world sweep: production must supply certified region
coverage or reject. Initial overlap, invalid snapshots, stale route/body tokens
and unsupported multiple hops release/reject with zero drive.

Sweep translation from current body pose. Project target into the intersection
of shape-offset half-spaces while retaining tangential intent. Check the final
result rather than assuming iterative projection converged. Bound intermediate
rotation by `2 * collisionRadius * sin(shortestAngle / 2)`; when that conservative
envelope cannot fit, keep current orientation and project translation. No fixed
28 cm holding sphere or broad wall ignore is used. Contacts remain authoritative;
feasible-target assistance cannot authorize traversal or guarantee actual motion.

Resolve route from explicit state, never proximity. Direct and one equal-scale
pair map desired pose to body space; reject stale pair/route revisions and extra
hops. Transfer-triggered route/history reconciliation remains PHY-4. No helper
silently switches route or reuses an old-space force after transfer.

## Drive and declared numeric gates (before execution)

Read current pose, COM velocity, angular velocity, mass and local inertia at
PreIntegrate. Apply a damped spring at the local grab anchor, including
`omega cross anchorOffset` in anchor velocity. Use implicit-step spring gains
with actual dt/mass/inertia to reduce timestep instability. Combine anchor torque
with orientation torque, then cap total force and total torque independently.
No pose/velocity assignment is part of holding. Invalid state or excessive raw
anchor error returns zero force/torque and a release reason.

Fixture profile: linear stiffness 300, damping 160, maximum force 1200
kg*cm/s²; angular stiffness 600, damping 200, maximum torque 1200
kg*cm²/s²; maximum raw anchor error 300 cm, skin 0.5 cm, maximum dt 1/60 s.
Native one/two-substep TaskGraph runs use 1 kg and 20 kg boxes. Free orientation
target rotates 90 degrees on the 1 kg box. Blocked target drives toward X=-50, Y=20 from X=8;
the shape-offset wall boundary is X=7.5 cm.

Gates: each applied force/torque norm <= cap + 1e-5; unconstrained per-interval
linear velocity follows `F/m * dt` within 1e-4 cm/s; rotational impulse follows
actual local inertia within 1e-4 rad/s for the isotropic box. Free-step energy
must not exceed the work-plus-impulse bound by 0.02 kg*cm²/s². After 240 game
dispatches, blocked target error <= 2 cm, final speed <= 3 cm/s, body remains
on the blocked side X>=6.8 cm, and no transfer/recovery/contact bypass occurs.
Free rotation error <= 3 degrees and final angular speed <= 0.1 rad/s. Light/
heavy bodies may have different lag but must meet the same caps and final gates.
Unsafe near-wall rotation is projected explicitly; arbitrary contact/friction
energy, remote-half physics, CCD and joint assemblies are not accepted here.

Pure tests cover shape-specific constraints, tangential projection, rotation
envelope, initial overlap/invalid input, off-center anchor torque, stale routes
and remote pose mapping. Actual Chaos tests cover force/torque, blocked targets,
rotation and the declared substeps. Rerun affected PHY-2 fixture tests after its
extension. No manual visual gate is required for this isolated experiment;
production Core PIE gates remain required in PHY-4.

## Actual results

Prescribed bundled UE 5.8 project generation and Development Editor build PASS.
After correcting only a route-test expectation, the editor rebuild also PASS.
Evidence: `Saved/Logs/PortalPhysicsPHY3{ProjectFiles,Build,FinalBuild}.log`.

Initial combined PhysicsBoundary + PhysicsHold run: **7/8 PASS, one failed,
zero warnings**. All four existing boundary regressions and all three new
wrench/native tests passed. The single failure was GeometryAndRoutes: its ideal
rounded position `(990,-20,0)` differed at the declared 1e-6 tolerance from the
existing portal math's half-turn quaternion built with engine `PI`. The test now
uses an independent half-turn reference at the actual declared frame precision;
mapping tolerance, physical acceptance thresholds and runtime math are unchanged.
Affected GeometryAndRoutes rerun: **1/1 PASS, zero warnings**. The other seven
passing tests were unchanged and their evidence is reused; these are separate
runs, not a claim that one combined 8/8 run passed.

Reports: `Saved/AutomationReports/PortalPhysicsPHY3/index.json`
(2026.09.26-13.22.07 UTC), `PortalPhysicsPHY3Geometry/index.json`
(2026.09.26-13.27.06 UTC). Logs:
`Saved/Logs/PortalPhysicsPHY3{,Geometry}Automation.log`. The initial failed
report/log are also retained in `Saved/PortalPhysicsPHY3/initial-run.{json,log}`.

Native TaskGraph blocked targets (240 game dispatches per profile):

| Physics intervals/dispatch | Mass (kg) | Samples | Final target error (cm) | Speed (cm/s) | X (cm) |
|---|---:|---:|---:|---:|---:|
| 1 | 1 | 240 | 0.015443 | 0.021059 | 7.500386 |
| 1 | 20 | 240 | 0.078037 | 0.017214 | 7.501950 |
| 2 | 1 | 480 | 0.019450 | 0 | 7.500486 |
| 2 | 20 | 480 | 0.144566 | 0 | 7.503613 |

Every sample respects force/torque caps, retains ordinary contacts, has zero
transfer/bypass and stays at X>=6.8 cm. Unsafe rotation is explicitly projected;
all four profiles settle within the predeclared error/speed limits. This fixture
has no recovery writer, rather than hiding an existing Recovery count.

Native free rotation/translation uses the 1 kg isotropic box. One/two-interval
profiles have final rotation error 0.003253/0.009990 degrees and angular speed
0.000051/0 rad/s. Every sample passes applied wrench caps, actual linear/angular
impulse and free-step work-plus-impulse energy bounds. Position error is <=2 cm;
no transfer occurs. These are real Chaos scenes under NullRHI, not pure math
stand-ins. Heavy-body rotation, gravity compensation, nonzero moving-target
velocity, contact friction/energy and arbitrary world obstruction are not proved.

Pure tests additionally cover all supported shape families, compound support,
tangential projection, initial overlap, rotation envelope, explicit single-pair
mapping and stale body/pair/route/region rejection. Off-center anchor force
contributes to total capped torque, and damping includes angular anchor velocity.
The current spring denominator accounts for translational mass and diagonal
rotational inertia separately; it is not a fully coupled implicit anchor solve.
Native off-center holding and anisotropic-body torque remain future acceptance.

All holding is editor-fixture-only. No production PhysicsHandle/contact/Recovery
branch, renderer default, asset or gameplay API changed. The pre-existing map
SHA-256 remains `0436F682DCD867E43ECEA81E89D4916D8B8ECFB2E66CA9321997BC97038BE25C`.
No manual visual gate applies to this nonproduction experiment; production
carrying behavior, arbitrary-world query providers and Core PIE remain open.

## Next action: PHY-4

Integrate one coordinator and physics adapter for held/free bodies. Supply
actual certified obstruction/topology snapshots or reject unsupported world
queries; explicitly define supported rotation/cancellation and blocked exits.
Reconcile durable transfer facts with holder routes/anchors/history, retire
bindings with teardown and remove old hold/gate/Recovery writers together.
Do not attach the new drive beside the old PhysicsHandle. Run Core native and
manual PIE gates; if required manual work cannot be performed, mark it
`USER ACTION REQUIRED`. Do not begin P8/P9 before Core acceptance.
