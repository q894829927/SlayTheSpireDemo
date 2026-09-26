# PHY-4 — one passage coordinator and production cutover

Updated: 2026-09-26. Status: **IN PROGRESS; COORDINATOR SLICE VALIDATED; actual-world provider/cutover/Core gates OPEN**.
Source HEAD: `33492d01471b2025f7d068dc090c0e4b1ac2ee37` (world observations/native adapter committed).
Native static-clearance continuation is uncommitted; general slice 2 OPEN.
Authority: [rigid-body contract](InteriorPortalRigidBodyInteractionDesign.md),
[Physics P1 ordering](InteriorPortalPhysicsP1Execution.md).

## Dependency-preserving implementation order

1. Coordinator and durable commit protocol: one solver-side owner of passage,
   crossing revision and holder/body route relation; transactional adapter commit;
   replay/acknowledgment independent of intent freshness. Prove it in the existing
   native Chaos fixture before introducing an actual-map writer.
2. Actual-world certified query/topology provider and production adapter. The
   PHY-2/PHY-3 authored empty corridor/static half-space fixtures do not certify
   arbitrary furniture, floor contacts, rotation or gravity. Implement and prove
   the supported motion and safe cancellation/recovery/rejection policies.
3. Atomic production cutover: remove PhysicsHandle hold targeting, whole-support
   free-joint arbitration, normal frame position Recovery and post-camera body
   warp together. Also remove player-first direct held-body warp/regrab. Preserve
   player traversal/presentation and expose one coherent held/free authority.
4. Actual-map Core native and manual PIE matrix. Only then close carrying defect
   and seal Physics P1. P8/P9 remain gated by Core.

These are dependent implementation slices within PHY-4, not new global phases.
No slice may imply the later production/visual gates passed. Keep new mutating
code isolated until slice 2 proves production coverage; do not run it alongside
legacy writers or switch only the hold query as a temporary fix.

## Coordinator contract

One coordinator binds body identity, pair generation, solver epoch and immutable
equal-scale endpoint frames. It owns passage/contact selection and the exact
once crossing decision for held and free bodies. Chaos still owns normal pose/
velocity; interval samples are transient reads, not a second simulated state.
Only the adapter writes a prepared transfer and must be atomic: return false
without any write, or commit the complete supplied pose/velocity transaction.
Then the coordinator publishes a committed fact and updates route/history.

Revalidate the actual solved interval, mapped aperture and certified clearance
before commit. Reject stale/mismatched frames, duplicate steps, invalid material
state and unsupported motion without creating a fact. Preserve mass, inertia,
COM and declared sleep state in the native adapter; no exit offset is allowed.
Boundary timing remains restricted to PHY-2's proved static normal approach.

Holding is an orthogonal association with a monotonic instance ID, holder identity,
local grab anchor and desired holder pose. Route direction is explicit. Body-first
and holder-first crossings advance the same relation; if both complete through
the declared pair, the route becomes direct. Unsupported crossing sequences
release rather than guessing additional hops. Holder-space intent carries its
holder-transfer revision, so a delayed old-space target cannot revive a route.
No new hold instance may reuse a retired instance ID.

## Durable facts and lifetime

Committed facts have body/solver/binding domain, transfer revision, actual solver step,
source/target endpoint, mapped state and frozen hold relation. They do not use
the current command revision or current pair generation as a consumption gate.
Advisory receipts may become stale; already committed facts cannot. Binding epoch
is separate from solver epoch and must be unique for every new binding, including
the same body/solver after topology replacement. Facts/cursors/acks carry it;
a revision restarting at 1 in a new binding cannot masquerade as an old duplicate
or let an old acknowledgment erase the new journal. The adapter owns allocation.

Retain unacknowledged facts in a bounded journal. Repeated delivery is allowed;
consumer applies only the next contiguous transfer revision and reports duplicates
or a gap without partial mutation. Ack only a consumed contiguous prefix. A full
journal rejects a new commit before the adapter writes; it never silently drops
an earlier fact. Ack is marshalled back to the solver boundary, not a game-thread
mutation of the coordinator.

Retirement is permanent for that binding. It cancels pending permission and hold
association but preserves committed facts until handoff/teardown completion.
Owner cancellation precedes native proxy deletion, with native unregister as a
second guard. New topology/body generation requires a new binding; old inputs
cannot revive it. Consumer cursors observe facts and never reapply physical pose.

## Predeclared validation for the coordinator slice

Pure tests: duplicate step exclusion; adapter failure atomicity; stale body/pair/
frame/solver rejection; blocked exit; invalid solved/material input; held/free
shared decision; body-first/holder-first route and anchor/target invariants;
old-space intent and old hold instance rejection; permanent retirement; delayed
facts despite newer intent/topology; gap/dedup/ack and bounded-journal exhaustion.

Extend existing native TaskGraph single/two-substep fixture: one commit, actual
pose/velocity mapping within existing 1e-6 tolerance; unchanged mass/inertia/COM/
sleep flags; immediate route refresh before the next drive; delayed fact delivery
and acknowledgment; normal collision cancellation/lifetime regressions. Keep
PHY-2/PHY-3 numeric gates unchanged. No manual gate applies to this isolated slice.

Required production manual gate remains `/Game/House/L_Interior_LivingKitchen`:
no/unlinked/linked walls, parallel carrying, rim/corner, partial insert/retreat,
rotating/light/heavy/spinning bodies, blocked exit, body-first/player-first,
reset/replacement/destruction and declared frame/substep settings. Observe no
ordinary Recovery teleports, no steady permission alternation, no sustained
oscillation; preserve expected collision/traversal/visuals. Record screenshots/
observations plus diagnostics. If manual execution is unavailable, label it
`USER ACTION REQUIRED`; do not substitute isolated Automation for this gate.

## Actual progress/evidence

Coordinator slice is implemented in runtime value-only helpers and integrated
only into the existing editor Chaos fixture. The native adapter performs the
prepared physics-thread write; held and free bodies share the coordinator.
There is no active map/system reference to it. The inline fixture writer is now
replaced by a reusable runtime Chaos transfer primitive (still attached only in
tests); see [slice-2 evidence](InteriorPortalPhysicsWorldQueriesPHY4.md).
A coordinator cannot be copied
to create another commit owner. The bounded journal defaults to 32 facts; invalid
capacity (<=0 or >1024) retires the binding.

Crossing uses an explicit baked local authority reference. Native boxes use
actual zero-offset COM; pure regression proves nonzero local COM mapping and
rejects a material reference different from the command snapshot. Solved-state
revalidation freezes the interval's geometry, bounds and certificate contract,
so reusing a revision cannot change the decision after contacts solved.

Prescribed UE 5.8 project generation/build PASS. Initial combined
PhysicsBoundary + PhysicsHold + PhysicsCoordinator run: **14/14 PASS, zero
warnings**, 2026.09.26-14.18.32 UTC. A subsequent lifetime review added explicit
binding epochs and typed acknowledgments plus stale-binding tests; rebuild PASS.
Affected PhysicsCoordinator + NativeTransferAndRetirement rerun: **7/7 PASS,
zero warnings**, 2026.09.26-14.21.42 UTC. The other unchanged motion/hold evidence
is reused. These are separate runs, not a combined 21-test total.

Evidence (ignored):
`Saved/AutomationReports/PortalPhysicsPHY4Coordinator/index.json`,
`Saved/AutomationReports/PortalPhysicsPHY4Binding/index.json`,
`Saved/Logs/PortalPhysicsPHY4Coordinator{ProjectFiles,Build,FinalBuild}.log`,
`Saved/Logs/PortalPhysicsPHY4{Coordinator,Binding}Automation.log`.

Pure coordinator tests cover adapter failure atomicity, duplicate commits,
identity/frame/solver/certificate rejection, nonzero COM reference, body-first/
holder-first relation updates, old-space intent, released hold IDs, permanent
retirement, gap/dedup/typed ack, binding separation and journal backpressure.

Native TaskGraph one/two-step profiles (24 game dispatches each):

| Intervals/dispatch | Held | Samples | Transfers / consumed fact revision | Final X (cm) |
|---|---|---:|---|---:|
| 1 | no | 24 | 1 / 1 | 1040.000003 |
| 1 | yes | 24 | 1 / 1 | 1017.397075 |
| 2 | no | 48 | 1 / 1 | 1040.000003 |
| 2 | yes | 48 | 1 / 1 | 1017.563696 |

Native checks preserve actual mass/inertia/local COM/mass frame/sleep flags and
refresh the held route/target before the next drive. The free/held difference in
final position is physical spring response, not a transfer offset. The held
intent is static in this fixture; live player/holder events are still pure
protocol tests, not actual player traversal acceptance. Delayed native fact
test withholds consumption for 12 newer command revisions, consumes/replays the
single retained fact, then confirms the marshalled ack clears it.

The subsequent [world-observation/transfer record](InteriorPortalPhysicsWorldQueriesPHY4.md)
adds real-world volume queries, full exit clearance and a reusable native adapter.
Its focused run is 16/16 PASS (two existing foundation-fixture warnings); later
shape coverage is a separate 1/1 PASS with zero warnings. Native zero-gravity
normal blocked-exit/partial-insertion cancellation restores ordinary contacts
without test-side pose Recovery; runtime adapter rejection is checked before
valid commit. These narrow checks do not establish a certified production
provider, arbitrary straddling cancellation or actual-map Core acceptance.

Subsequent [solver-bound clearance evidence](InteriorPortalPhysicsSolverClearancePHY4.md)
now proves a private once-consumed certificate at PreIntegrate/PostIntegrate/
PostSolve in a strict static-scene normal-motion profile. It reads actual native
geometry and includes PhysicsOnly blockers; topology/body configuration mismatch
retires its binding. Rejection/partial-insertion cancellation, held/free transfer
and mapping regressions pass. Final affected run 29/29 PASS, zero test warnings;
the record retains earlier failed/diagnostic runs separately. A discovered shared
float-PI half-turn drift is fixed with an exact quaternion and geometry/camera
mapping gates revalidated. New writers remain attached only in tests.

Follow-up angular-acceleration/persistent-joint guards reject before integration;
actual rotation-drive and world-joint fixtures pass. Rebuild PASS and affected
PhysicsSolverClearance rerun 4/4 PASS, zero test warnings. Other unchanged gates
reuse the preceding evidence; these are separate runs.

Limitations are explicit: game-thread observations are not physics-substep
certificates. The native certificate currently rejects every other dynamic/
kinematic simulation particle and conservatively over-blocks collision responses.
General dynamic scene coverage/filtering and production adapter assembly remain
open. Native journal-full cancellation, invalid topology while
straddling, rotation/gravity and exceptional recovery are not proved by the
current fixtures. Destruction of
the final coordinator/solver must drain or explicitly hand off pending facts;
retention inside this object alone does not implement whole-world shutdown.

No manual gate applies to this isolated slice and no production fix/Core seal
is claimed. Map hash remains
`0436F682DCD867E43ECEA81E89D4916D8B8ECFB2E66CA9321997BC97038BE25C`.
**Next unfinished work: slice 2 — actual-world certified query/topology provider
and production adapter assembly**, building on the validated observation/transfer
primitives and including the unsupported cancellation/recovery cases
above. Then perform atomic cutover and the required Core manual gate.
