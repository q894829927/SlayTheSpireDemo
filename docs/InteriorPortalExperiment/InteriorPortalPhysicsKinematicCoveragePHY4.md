# PHY-4 slice 2 — native filters and kinematic interval coverage

Updated: 2026-09-27. Status: **FILTERS/KINEMATIC COVERAGE VALIDATED; general dynamics/cutover/Core OPEN**.
Source HEAD: `12f1fe81ef11cdc1ffc3403ae142782680f041e0` (native static certificate committed).
Authority: [PHY-4 ordering](InteriorPortalPhysicsPHY4.md),
[rigid-body contract](InteriorPortalRigidBodyInteractionDesign.md).

## Contract and supported scope

Extend the existing native certificate instead of adding another world query or
permission owner. Static supports, fixed-orientation normal traveller motion and
the once-consumed per-stage proof remain unchanged. Scan all non-disabled native
particles at each actual physical stage; retain the budget and failure priority.

Apply Chaos particle-pair broad filtering with no ignore manager, then actual
simulation-enabled per-shape `FShapeFilterData::NarrowFilter`. A pair passes if
any shape pair can simulate contact. Response masks are bilateral: Ignore or
simulation Overlap may exclude a pair even if the other shape requests Block.
Ignore-manager and complex/simple geometry exclusions remain conservative
over-blocks; root bounds still cover all shapes when any shape interacts. Query
enablement is irrelevant. Filter changes must reach native state before that
boundary; a GT declaration cannot exclude a native blocking pair.
Both bound supports must retain native interaction with the traveller, because
normal cancellation depends on restoring their actual contacts. Losing that
support response retires the binding; restoring the filter cannot revive it.

For interacting kinematics, construct a complete conservative envelope at
PreIntegrate. None/Reset modes are stationary. Position mode covers the entire
remaining native frame target, independent of the unknown substep fraction.
Velocity mode covers native X through X+V*physical-dt. Use the largest local box
corner radius around the native actor origin to cover every intermediate
rotation, including off-center shapes. The 1e-4 cm envelope expansion covers
native float rotation rounding; it does not relax mapping/aperture tolerances.

Keep envelopes only for that physical interval, keyed by native unique index.
At PostIntegrate and PostSolve verify geometry identity/hash/local bounds and
containment of the actual native geometry in its original envelope; consume the
same conservative interval extent. New interacting kinematics or changed/outside
envelopes reject. Never dereference a retained particle key; retirement clears
all envelopes. No motion prediction from a GT component is authoritative.

Collidable Dynamic/Sleeping bodies still reject anywhere in the scene. Their
future contacts/constraints can move them beyond a V*dt extrapolation, so this
slice does not claim general moving dynamic coverage. Filtered-out dynamics are
permitted. Kinematic contacts remain blocking obstructions under Core policy;
no finite-mass or remote-half contact/P8 implementation is introduced. New native
writers remain attached only in editor tests, with no production cutover.

## Predeclared automated gates

Reuse the existing native fixture with TaskGraph one/two-substep profiles:

- Ignore, simulation Overlap and query-only obstructions allow exactly one
  transfer without an authored GT certificate; either side's native Ignore must
  work. Block and PhysicsOnly remain blocking. Changing a native response from
  Ignore to Block cancels at the first affected PreIntegrate with no transfer.
- Unrelated kinematics move outside the corridor without denying passage.
  Filtered dynamic bodies may remain; interacting dynamics still reject.
- A kinematic target crosses the exit corridor with both endpoints clear;
  its swept path blocks before integration. A rotating thin box likewise blocks
  when endpoint boxes are clear but its intermediate rotation intersects.
- Native velocity-mode kinematics with angular velocity actually integrate their
  target; both a crossing corridor and unrelated clear path exercise the complete
  one/two-substep stage protocol and native facts.
- Moving obstruction during partial insertion restores ordinary contacts in
  the same boundary and commits no transfer, without GT body-pose Recovery.
- Preserve certificate replay/stage/step binding, geometry retirement, native
  transfer/material/fact coherence and unsupported traveller-motion gates.

Use prescribed bundled UE 5.8 generation/build and the affected
PhysicsSolverClearance Automation suite. No manual gate applies to this isolated
provider slice; actual-map Core manual acceptance remains after atomic cutover.

## Evidence and next action

Prescribed bundled UE 5.8 generation and editor builds PASS. Initial affected
PhysicsSolverClearance run **6/6 PASS, zero test warnings**, 2026.09.26-16.06.09
UTC. The separately added native velocity-mode gate initially **0/1 PASS**,
16.08.40 UTC (four endpoint assertions, zero warnings): BodyInstance's GT
transform did not reflect velocity-mode PT motion. The test now enqueues a native
read after the frame and waits through a zero-dt flush. It verifies actual
physics state rather than accepting a predicted/component endpoint.

After native support-response retirement guard and fixture correction, rebuild
PASS. Final affected **7/7 PASS, zero test warnings**, 2026.09.26-16.11.22 UTC:
NativeStaticAndCertificate, NativePhysicsOnlyAndCancellation,
UnsupportedSceneAndMotion, NativeBindingRetirement, NativeSimulationFilters,
KinematicIntervalCoverage and KinematicVelocityCoverage. These are separate
initial/failed/final runs, not cumulative acceptance totals. Unchanged hold,
coordinator, world-query and shared-mapping evidence from the prior delivery is
reused; this slice changes no mapping/drive/coordinator logic.

TaskGraph one/two-step results:

- Ignore from either side, simulation Overlap, query-only and filtered dynamic
  obstructions each permit exactly one transfer and consumed fact revision 1.
  Block response restored at partial insertion X=2 cancels at the first affected
  PreIntegrate, zero transfers, final source X>6.8 cm via ordinary contacts.
- Unrelated kinematic target Y=40 to 80 allows one coherent transfer. Position
  target Y=-30 to +30 across the exit corridor rejects before integration even
  though its endpoint boxes are clear. Both normal and partial entry restore
  contacts without test-side Recovery; a stationary final blocker maintains
  obstruction for the 12-frame restoration check.
- Thin box at (1013,17,0), half-extents (.25,20,.25), rotates +90 to -90 degrees:
  endpoint boxes clear but the full rotation envelope rejects. Native target
  orientation is reached; zero transfer in the affected frame.
- Native velocity 3600 cm/s with angular velocity 5 rad/s reaches PT endpoint
  Y=100.000003 (unrelated clear path) or 30.000003 (crossing path), in both step
  profiles. Every clear-path stage stays certified and commits exactly once;
  crossing rejects at PreIntegrate and creates no fact in that affected frame.
- Native support Ignore retires permission and restoring Block cannot revive
  it. Existing PhysicsOnly, replay, wrong-stage/step/sample, material/fact,
  persistent-joint and unsupported traveller-motion gates remain passing.

Ignored evidence:
`Saved/AutomationReports/PortalPhysicsPHY4Kinematic{,Velocity,Final}/index.json`,
`Saved/Logs/PortalPhysicsPHY4Kinematic{,Velocity,Final}Automation.log`,
`Saved/Logs/PortalPhysicsPHY4Kinematic{ProjectFiles,Build,VelocityBuild,FinalBuild,NativeReadBuild}.log`.
No manual PIE performed; no manual gate applies to this isolated provider slice.
The existing map is untouched and excluded from commit, SHA256
`0436F682DCD867E43ECEA81E89D4916D8B8ECFB2E66CA9321997BC97038BE25C`.

Next exact action: establish conservative interval coverage or an explicit
bounded rejection policy for interacting Dynamic/Sleeping obstacles, including
contacts/constraints that can invalidate a V*dt prediction. Then broaden admitted
traveller motion/cancellation, topology recovery and journal shutdown handoff
before production assembly and atomic cutover. No production carrying fix or
Core seal is claimed.
