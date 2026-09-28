# PHY-4 slice 2 — closed sleeping contact-island coverage

Updated: 2026-09-27. Status: **CLOSED SLEEPING CONTACT PROFILE VALIDATED; active dynamics/cutover/Core OPEN**.
Source HEAD: `fd825aca848de01f120bf346badd0eab6f161bc8` (dormant leases committed).
Authority: [PHY-4 ordering](InteriorPortalPhysicsPHY4.md),
[rigid-body contract](InteriorPortalRigidBodyInteractionDesign.md).

## Scope and proof

The preceding increment rejects every retained midphase. Extend its existing
native lease to ordinary sleeping contact islands, such as furniture resting on
a fixed floor, without adding a permission owner or changing gameplay writers.
Active dynamics still reject: velocity alone cannot bound future forces, contact
impulses or joint projection. This increment proves the closed sleeping subset
of the remaining contact-island coverage; it does not seal the general provider.

A sleeper with retained contacts must belong to an actually sleeping native
island. Reject resimulation/cache use, partially awake islands, persistent joints,
non-collision graph constraints, disabled/unknown members and budget exhaustion.
Every dynamic member must satisfy the existing native zero-motion/zero-pending-
impulse profile. Every retained midphase must be valid and sleeping, with only
Static or Sleeping endpoints. Kinematic contacts remain unsupported even when
currently stationary; their cached manifolds can transmit motion later.

Canonical native member and partner IDs, constraint counts and static-contact
pose/geometry descriptors form the per-interval topology lease. Read island and
midphase pointers only during the scan; retain no graph/node/midphase pointers or
debug island IDs. Revalidate the same topology and descriptors at PostIntegrate
and PostSolve. Cached static-contact shape transforms must match actual native
static pose, so an already stale manifold cannot bootstrap a new certificate.
New/removed contacts, changed support pose/geometry, waking members or altered
topology deny the interval. Existing all-sleeper wake analysis prevents external
direct/indirect influence, including partners filtered out against the traveller.

Keep 32 sleepers / 16384 wake checks and the 4096 scene cap. Limit each contact
scan to 128 retained midphases, 256 native island constraints and 256 visited
static-contact constraints. Limits reject complete certification rather than
silently truncating. Source/destination obstruction priorities remain unchanged.
The verifier never forces sleep, changes constraints or writes native body pose.

## Predeclared gates

Reuse the editor TaskGraph one/two-substep fixture, with actual native contact
generation followed by synchronized sleep. Verify native island sleep and retained
contacts before admitting fixtures; do not infer them from GT component state.

- Held/free transfer beside an unrelated one-body floor contact and two-body
  sleeping contact island: every stage clear, exactly one committed fact,
  unchanged obstacle native pose/sleep/material.
- A sleeping member in the mapped exit still blocks transfer.
- Wake a member during partial insertion: first affected interval rejects;
  normal wall blocking returns without GT Recovery or transfer facts.
- Move a static contact participant after certification, including within an
  interval: old lease rejects. Test an initially stale cached static transform.
- Retained kinematic contact, a real persistent joint and partially awake island
  remain rejected. Existing pending-impulse, pose-mutation, indirect wake, filter,
  kinematic, PhysicsOnly and proof-lifecycle regressions remain passing.

Prescribed bundled UE 5.8 generation/editor build, then affected
PhysicsSolverClearance Automation. No manual gate applies to this isolated
increment. Actual-map Core manual acceptance remains after provider/cutover and
is `USER ACTION REQUIRED` if unavailable then.

## Evidence and next action

Implementation is uncommitted on source HEAD above. Prescribed bundled UE 5.8
project generation and editor builds PASS. Initial affected PhysicsSolverClearance
run **14/15 PASS, zero test warnings**, 2026.09.26-17.34.26 UTC. All 11 previous
regressions passed; the new lease test failed four next-interval reason assertions
after restored wall contacts had rotated the fixture traveller. All first affected
interval rejection assertions passed. Restore only the traveller's authored
normal approach before checking a fresh contact lease; no runtime motion tolerance
or rejection rule was weakened. Add explicit graph-container/node validity and
constraint-count guards. Rebuild PASS; final affected ContactIsland rerun **4/4
PASS, zero test warnings**, 2026.09.26-17.37.00 UTC. Existing 11 regressions reuse
the preceding unchanged-profile evidence. These are separate runs, not a combined
19-test total or a final full 15-test rerun.

Actual TaskGraph one/two-substep fixtures generate contacts with gravity, then
synchronize native sleep before certification. They verify native island size
(one or two dynamic members), island sleeping state and retained midphases. Eight
held/free/floor/stack combinations consume clear proofs at all three stages for
12 dispatches, commit exactly once with fact revision 1 and preserve traveller
material plus furniture pose/sleep. The floor is filtered out against the
traveller but still contributes its static-contact descriptor.

A small sleeping contact-island member in the mapped exit blocks transfer.
Waking the top stacked member during partial insertion rejects the first affected
PreIntegrate, restores original wall blocking (source X>6.8 cm), and creates no
transfer/fact without GT Recovery. Native static support movement before the
first scan, after integration or after solve rejects its current certificate;
an unchanged stale cached static shape transform cannot bootstrap a fresh lease.
Retained kinematic support and a real persistent joint inside a contact island
reject before support-pair suppression. The previous retained-contact rejection
fixture now uses a kinematic endpoint: fixed sleeping contact islands are the
explicitly supported extension, not a blanket retained-midphase failure.

Ignored evidence:
`Saved/AutomationReports/PortalPhysicsPHY4ContactIsland{,Final}/index.json`,
`Saved/Logs/PortalPhysicsPHY4ContactIsland{,Final}Automation.log`,
`Saved/Logs/PortalPhysicsPHY4ContactIslandProjectFiles.log` and
`PortalPhysicsPHY4ContactIsland{,Guard,Final}Build.log`.
No manual PIE performed; no manual gate applies to this isolated provider.
No active gameplay writer/public interface/map changes. Existing map SHA256:
`0436F682DCD867E43ECEA81E89D4916D8B8ECFB2E66CA9321997BC97038BE25C`.

The next [bounded independent active-body increment](InteriorPortalPhysicsActiveReachPHY4.md)
now admits only contact-free, capped remote motion; general interacting dynamics
remain OPEN. Awake contact/joint assemblies, retained
kinematic contacts, cache/resim and unknown geometry still reject. Then broaden
traveller motion/cancellation, topology recovery and final fact handoff. Only
after slice 2 passes remove all legacy writers atomically and run Core; carrying
defect and P8/P9 remain OPEN.
