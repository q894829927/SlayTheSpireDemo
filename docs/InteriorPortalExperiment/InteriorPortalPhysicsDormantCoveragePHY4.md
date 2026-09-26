# PHY-4 slice 2 — native dormant-body interval leases

Updated: 2026-09-27. Status: **DORMANT PROFILE VALIDATED; active dynamics/cutover/Core OPEN**.
Source HEAD: `9d44f5b535ca4371a60892afbd9ba357d93d44ed` (native filters/kinematic coverage committed).
Authority: [PHY-4 ordering](InteriorPortalPhysicsPHY4.md),
[rigid-body contract](InteriorPortalRigidBodyInteractionDesign.md).

## Supported scope and rejection policy

Extend the current native verifier with an interval lease for interacting
Sleeping rigid bodies. Chaos does not integrate Sleeping particles, so this
profile avoids unproved active-force/contact extrapolation. The verifier reads
native state only and never forces an obstacle to sleep or removes its contacts.
Keep static supports, fixed-normal traveller motion, stage/step proof consumption
and current pairwise Core obstruction policy unchanged.

An admitted sleeper must have bounded native geometry, finite positive mass,
zero current velocity/acceleration/pending impulse, no persistent constraints and
no retained collision midphases. Existing contacts, constrained assemblies,
active dynamics and unsupported shapes remain rejected. Gravity on a sleeper is
not extrapolated: integration skips it while it stays Sleeping. This does not
approve sleeping furniture resting in a contact island or arbitrary dynamics.

At PreIntegrate freeze native geometry/root identity, local bounds and rigid pose
under the native unique index. Later stages require the same Sleeping state and
pose/configuration. Wake-up, new contacts, a pending impulse, changed geometry or
newly discovered sleeper cannot consume an earlier interval lease. Recompute
leases for every physical step; retirement clears them.

Prove no current-interval wake influence before authorizing bypass. Lease every
simulation-enabled native sleeper, including those filtered out against the
traveller, and scan every potential partner. This prevents an unexamined sleeper
from propagating a wake-up to an admitted obstacle. The traveller's source and mapped destination
corridors cover its possible contact reach. Other Sleeping/Static partners cannot
move in this profile. Moving kinematics use the full existing interval envelope,
including rotations and native Velocity mode. An interacting active dynamic
partner has uncertified reach and rejects regardless of distance. Unknown
kinematic bounds reject; never silently skip a possible wake source.

Pad wake influence by the actual native collision detector BoundsExpansion,
size scaling and velocity inflation times conservative native travel distance,
capped by the detector's maximum velocity expansion. The maximum is a cap, not
the actual travel distance. Position-mode kinematics cover their full target;
source and mapped destination corridors are checked separately, without treating
the space between portals as a physical path. Detector settings and size-scale
cvars are read-only
settings, frozen for the interval; changed/invalid settings reject. Both CCD and
ordinary candidates are conservatively covered. Existing midphases are rejected
rather than guessing their cached manifold reach. The wake-partner pass is bounded
to 32 admitted sleepers and 16384 pair checks; exhaustion rejects, not truncates.
Source/destination obstruction takes priority over a mere wake-risk reason;
malformed/unsupported scene still has the existing highest rejection priority.

No active-world permission owner or gameplay writer is added. This is an isolated
native provider increment. General active-dynamic reach, contact-island support,
traveller rotation/gravity/cancellation, invalid topology, shutdown handoff and
atomic production cutover remain dependent work. No P8 remote dynamic contacts.

## Predeclared automated gates

Reuse TaskGraph one/two-substep native scenes:

- A genuinely sleeping, isolated 1 kg / 20 kg body outside both corridors no
  longer rejects the scene. Held/free traveller commits exactly once with coherent
  material and durable facts; every stage consumes a current certificate.
- A sleeping PhysicsOnly obstruction inside the mapped exit blocks transfer.
  Waking an admitted sleeper during partial insertion revokes the first affected
  PreIntegrate; ordinary contacts restore the traveller with no GT pose Recovery.
- A kinematic obstacle filtered out against the traveller sweeps across a sleeper
  and prevents a false stationary certificate. A filtered-out active dynamic
  partner still rejects if it can interact with the sleeper; filtered against
  both, it cannot influence the lease. An active partner filtered out against
  both the traveller and the first sleeper still rejects through a second
  simulation sleeper; no unexamined wake chain can authorize passage.
- Pending native impulse, persistent joint, retained contact and native pose
  mutation cannot reuse a dormant lease. A subsequent interval may acquire a new
  lease only from actual newly valid native state.
- Retain current PhysicsOnly, filter, moving/rotating kinematic, replay, binding
  retirement and unsupported traveller-motion regressions.

Run prescribed bundled UE 5.8 generation/build and affected PhysicsSolverClearance
Automation. No manual gate applies to this isolated increment; actual-map Core
manual gates remain after full provider coverage and atomic production cutover.

## Evidence and next action

Implementation is uncommitted on the source HEAD above. Prescribed bundled UE 5.8
project generation and editor builds PASS. Final affected
`SlayTheSpireDemo.Interior.Portals.PhysicsSolverClearance` run **11/11 PASS,
zero test warnings**, 2026.09.26-17.14.46 UTC. This includes the four new dormant
gates and all seven existing native static/filter/kinematic gates. Unchanged
coordinator/hold/world-query/shared-mapping evidence is reused.

Failed runs are retained separately, not combined into acceptance totals:

| Report suffix | UTC on 2026.09.26 | Actual result | Investigation/correction |
|---|---|---|---|
| Dormant | 16.50.52 | 7/11 PASS; 1 test warning | Typed console data lookup cannot read engine FAutoConsoleVariableRef settings; use FindConsoleVariable/GetFloat |
| DormantSettings | 16.52.58 | 0/4 PASS; zero warnings | Maximum velocity expansion was incorrectly used as actual distance; compute native travel inflation and apply the cap, check both portal corridors separately |
| DormantInflation | 16.55.50 | 3/4 PASS; 2 test warnings | Cache console-variable pointers; contact warm-up also collided/rotated the traveller and masked the intended sleep gate |
| DormantDiagnostic | 17.11.32 | 0/1 PASS; zero warnings | Explicit reason identified UnsupportedMotion in the warmed traveller; reset only fixture motion and verify native retained-midphase count |
| DormantFinal | 17.14.46 | 11/11 PASS; zero warnings | Final native wake-chain guard and retained-contact fixture pass with all affected regressions |

Actual TaskGraph one/two-substep scenes prove every clear-stage certificate for
held/free travellers beside isolated 1 kg / 20 kg sleepers, exactly one transfer
and fact revision 1, and preserved material/sleep state. A sleeping PhysicsOnly
exit blocker creates no transfer. Waking during partial insertion rejects the
first affected PreIntegrate and restores ordinary contacts (source X>6.8 cm)
without GT Recovery; a later actual valid sleeping interval can reacquire a lease.
Direct and indirect wake influence reject before support-pair suppression.
Injected native pose changes at PostIntegrate/PostSolve invalidate the original
interval. Pending native impulses, a real persistent joint and a verified retained
native contact deny the profile. These tests do not prove general island motion.

Ignored evidence paths:
`Saved/AutomationReports/PortalPhysicsPHY4Dormant{,Settings,Inflation,Diagnostic,Final}/index.json`,
`Saved/Logs/PortalPhysicsPHY4Dormant{,Settings,Inflation,Diagnostic,Final}Automation.log`,
`Saved/Logs/PortalPhysicsPHY4DormantProjectFiles.log` and
`PortalPhysicsPHY4Dormant{,Settings,Inflation,Diagnostic,Final}Build.log`.
No manual PIE performed; no manual gate applies to this isolated provider slice.
Actual-map Core manual acceptance remains dependent on coverage/cutover and will
be `USER ACTION REQUIRED` if it cannot be executed then. The pre-existing map
is untouched, SHA256
`0436F682DCD867E43ECEA81E89D4916D8B8ECFB2E66CA9321997BC97038BE25C`.

Next prove active dynamic/contact-island reach or a native bounded rejection
policy that can coexist with production scene contacts, then complete broader
traveller cancellation/topology/shutdown gates before atomic production cutover.
The carrying defect and Core/P8/P9 remain OPEN.
