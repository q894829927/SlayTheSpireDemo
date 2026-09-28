# Portal rigid-body interaction contract

Status: **production migration pending; PHY-0/PHY-1 complete, PHY-2/PHY-3 narrow profiles validated, PHY-4 coordinator and world-observation/transfer primitives validated/in progress**, 2026-09-26.
Execution: [Physics P1 migration](InteriorPortalPhysicsP1Execution.md). Performance
P1 is now sealed. The diagnostic and identity/geometry stages do not make existing physics
conform to this contract or establish physical acceptance.
Reviewed baseline: `9259f8e`. This document refines P5/P6/P10 in
[the implementation plan](InteriorPortalFullFidelityImplementationPlan.md).
[The performance execution plan](../PortalPerformanceVRAMP1Plan.md) owns current
work ordering before broad physics. The older
[execution record](InteriorPortalCurrentExecutionPlan.md) is historical; the
implementation plan retains the Core-before-P8/P9 dependencies. It does not authorize beginning P8/P9 before their existing gates.

## Scope and physical model

Core supports declared single rigid bodies, free or held, through static,
equal-scale portal pairs. Holding is an optional bounded force/torque constraint,
not a separate traveller type or a position teleport. Gameplay has one physical
body identity and one authoritative pose/velocity state. Chaos owns ordinary
motion and contact response; the crossing coordinator alone commits portal
transfers and exceptional recovery. Presentation consumes committed state.

P5 geometry adapters cover sphere, capsule, oriented box, convex and supported
compound collision shapes conservatively. A mesh's visual bounds are not its
collision geometry. Unsupported shapes/modes are explicitly rejected. Long
bodies require swept translation and rotation coverage, not an endpoint fit test.

Core does not claim remote-half destination contacts before transfer. Those
remain P8. Ragdolls, rope/cloth, vehicles and arbitrary constrained assemblies
require their own supported policy; P9 must not be implied by generic registration.

## Ownership and module boundaries

These are responsibilities, not a requirement for one UObject per row.

| Owner | Owned state and output | Forbidden responsibility |
|---|---|---|
| Traveller registry | Stable ID/generation, collision geometry, previous committed physics pose, passage state, lifetime | Independent copy of simulated pose/velocity |
| Hold controller | Holder/body association, local grab anchor, desired pose, bounded drive profile, portal route | Direct body relocation or support-wall filtering |
| Passage/query solver | Pure geometry and route evaluation from one physics snapshot; legal passage, obstruction and optional feasible target | Changing Chaos or gameplay state |
| Physics adapter | Apply drive settings and exact body/support contact permission at a supported simulation boundary | Independently deciding that a body crossed |
| Crossing coordinator | Commit transfer/recovery, mapping, route revision, histories and constraint rebind as one transaction | Normal per-frame position correction |

Inputs carry body generation, portal-pair generation, physics-step ID, current
pose/velocities, collision shape and intended motion. Results carry those same
identities plus route segments, mapped target, hit/normal, passage interval and
reason. Reject stale results after replacement, destruction or a committed swap.
Rendering crop, visual bias and Ping-Pong state never enter physics decisions.

## Holding contract

The current 125 cm distance may remain a configurable interaction preference.
It is an intent, not a guarantee of the body's position. Drive profiles declare
stiffness, damping, maximum force/torque, distance/error limits and release policy.
Heavy or blocked bodies may lag; movement must not inject an unbounded impulse
to force a target to be reached. The chosen UE drive implementation must prove
these bounds; using PhysicsHandle by name does not prove force limiting.

Initial selection may use a ray. Continuous holding uses the registered collision
geometry and the same aperture eligibility as free traversal, including rotation
and compound shapes. P10 supplies shared route/arbitration primitives; it must
not retain a second fixed-radius eligibility rule for held bodies.

Optional target projection reduces unreachable drive error using a sweep from
the current body pose, not only an eye-to-target ray. Preserve tangential intent
when obstructed. Actual contact remains authoritative: target projection is
interaction assistance and cannot replace the physics solver or permit tunneling.
Validate initial overlap and swept rotation explicitly. No global wall ignore is
allowed; legal support arbitration does not remove unrelated blockers.

Holding route and passage state are distinct. The route records the portal pair,
direction and mapping between holder and body; it is not a toggle inferred from
whether an entry pointer happens to be set. Core declares support for direct and
single-pair routes; unsupported additional hops release safely or reject the
operation with a documented reason, rather than silently choosing a route.

The [PHY-3 drive/target profile](InteriorPortalPhysicsHoldPHY3.md) separates
value-only intent, explicit route, certified static obstruction region, feasible
target and bounded wrench. Its free region is an intersection of inward
half-spaces with stable ordering and actual collision-shape offsets; it must
not be populated from unverified or incomplete world hits. Production providers
must certify coverage and invalidate snapshots with topology changes. Target
assistance neither disables contacts nor authorizes a portal passage.

Drive uses actual mass/inertia and solver dt, includes angular grab-anchor
velocity, and caps total torque after combining anchor and orientation torque.
The tested implicit-step gains are separate translational/rotational estimates,
not a fully coupled anchor solve. Native support proven so far is the centered,
isotropic box in zero gravity; off-center/anisotropic/gravity cases need their
own acceptance before promotion. Contact response remains Chaos authority.

## Passage lifecycle and simulation order

Passage states are Outside, Entering, Straddling and Exiting, with exceptional
Recovery. Holding is orthogonal. Acquisition requires a swept legal opening;
retreat can cancel entry without a transfer. Exiting ends after complete body
clearance. Small documented entry/clearance hysteresis prevents state chatter
without enlarging the physical opening or allowing lateral wall escape.

Use one declared authority reference, normally center of mass, for crossing.
Previous/current reference tests and transfer IDs prevent repeated transfer within
one simulation interval. Preserve physical retreat and later legitimate reversal.

Required logical ordering for each supported physics interval:

1. Consume an immutable input/portal snapshot at the physics boundary.
2. Evaluate shape-aware passage and holding route from the same body state.
3. Apply bounded drive and local contact permissions before affected contacts solve.
4. Simulate contacts and integrate; consume the resulting pose/contact state.
5. Commit any supported transfer exactly once, refreshing constraints, route and
   histories before the next affected solve; publish read-only presentation state.

Implementation must document the actual UE/Chaos hooks and game/physics-thread
handoff. An Actor Tick order is not proof of substep safety. If transfer timing
requires splitting an interval at the crossing, prove that mechanism in a narrow
spike before claiming high-speed/substep acceptance. Do not mutate UObjects from
an unsupported solver callback. Repeatable ordering is required; bitwise Chaos
determinism across platforms is not claimed.

Core may keep pairwise support bypass only while it can enforce a bounded legal
passage throughout the supported motion interval. A broad ignore followed by
next-frame rollback does not meet this contract. If the adapter cannot enforce
this, block/reject that passage safely and keep its acceptance gate open.

Cancelling bypass must remove forbidden inward motion before affected contacts
solve in the same boundary transaction; simply restoring a wall pair after
integration can eject an inserted body through its back face. PHY-2 proves a
static normal projection for its fixed-orientation approach fixture, not a
general rotating-body cancellation algorithm. See
[the boundary spike](InteriorPortalPhysicsBoundarySpike.md) for its exact limits.
The supported production profile must establish safe rejection for every
admitted motion, including bodies already straddling the opening.

## Transfer, recovery and cleanup

One transfer maps pose, linear/angular velocity, grab anchor/target and route;
preserves mass/inertia and deliberate sleep/wake state; updates previous/safe
poses, contact ownership and transfer generation together. Validate destination
clearance with actual supported geometry before committing. Rebinding must not
retain a target from the other space or derive velocity from teleport distance.
Player-first and body-first crossings update the same holder/body relation.

Recovery is reserved for invalid penetration, invalidated topology or numerical
failure. Revalidate the last safe pose against current geometry; it may no longer
be safe after a portal/world change. If valid, commit recovery with synchronized
target, constraint and history, removing only forbidden inward motion. If no
valid recovery exists, release the hold and use a documented bounded collision
recovery/failure path; never repeatedly teleport to an unchecked old position.
Repeated recovery in ordinary wall sliding fails acceptance.

Reset, replacement, body/holder destruction and EndPlay cancel pending work by
generation, release drives, restore exact collision pairs and remove derived
state. Cleanup is idempotent. No callback may revive an invalid hold or passage.

Owner cancellation must reach the affected physics boundary with teardown;
native proxy-unregister notification is an independent lifetime guard, not the
only cancellation signal. Retired bindings cannot be revived by replayed input.
Keep advisory intent freshness separate from durable committed transfer facts:
a newer command must never discard an already committed transfer or prevent
holder route/history reconciliation. Physics-step keys do not replace transfer
revisions.

Committed-fact identity includes a unique coordinator binding epoch independent
of solver epoch, body identity and mutable pair configuration. Cursors and
acknowledgments name the same domain. Topology rebinding cannot restart a fact
revision in an indistinguishable domain; drain/handoff the old domain explicitly.
Unacknowledged facts are retained with bounded backpressure before new commits,
never silently replaced by the latest intent. Retirement cancels permission/
holding but not already committed facts. See
[PHY-4 execution](InteriorPortalPhysicsPHY4.md) for implementation and open
production lifetime/cancellation gates.

World-query observations and authoritative permissions are different types and
ownership boundaries. A game-thread `ClearAtQuery` result for a geometric path
does not certify physics-only collision, future dynamic obstacles or an arbitrary
solver interval. A provider must prove coverage, topology/body revision and scene
lease correlation before authorizing bypass/transfer. The native adapter checks
the actual solved P/Q and material state before its first setter; an advisory
query cannot itself write a body. See
[world-observation/transfer evidence](InteriorPortalPhysicsWorldQueriesPHY4.md).

The [native static certificate](InteriorPortalPhysicsSolverClearancePHY4.md)
implements the boundary distinction in a restricted normal-motion/static-scene
profile: consume one private proof for its exact command, samples, physical step
and stage, with fresh scans at PreIntegrate/PostIntegrate/PostSolve. Actual native
simulation geometry includes PhysicsOnly collision. This profile rejects other
dynamic/kinematic particles in its original static profile. The subsequent
[native filter/kinematic extension](InteriorPortalPhysicsKinematicCoveragePHY4.md)
uses bilateral native simulation filters and PreIntegrate envelopes that cover
the full remaining kinematic target or one physical velocity interval, including
all intermediate rotations about the native origin. Later hooks verify actual
geometry stays inside that same envelope. Envelopes expire per physical step;
GT component transforms cannot substitute for native state. Both supports must
retain native interaction with the traveller for contact restoration; losing it
retires the binding. Filtered-out dynamics are excluded. The subsequent
[native dormant-body lease](InteriorPortalPhysicsDormantCoveragePHY4.md) admits
isolated actual Sleeping particles by freezing native pose/geometry per physical
interval and revalidating all three stages. No GT sleep claim or V*dt prediction
can substitute for a native lease. Every simulation sleeper participates in wake
analysis, including filtered-out intermediates. The subsequent
[closed sleeping contact-island lease](InteriorPortalPhysicsContactIslandsPHY4.md)
admits retained collision-only contacts only when the complete native island is
sleeping, with bounded canonical member/partner topology and fixed static-contact
pose/geometry. Cached static shape transforms must match native state; stale
manifolds cannot bootstrap a new lease. Later stages revalidate the same topology
and descriptors. No graph pointers or debug island IDs are persistent identities.
Persistent joints, pending impulses, retained kinematic contacts, resim/cache,
unknown geometry and uncertified wake reach reject. Active dynamic/awake-island
motion still requires contact/constraint-safe coverage or a bounded rejection
policy. The [bounded independent active-body profile](InteriorPortalPhysicsActiveReachPHY4.md)
now leases a remote contact-free native dynamic only with a usable speed cap,
whole-step corridor separation and preflight exclusion of possible static
contacts. Any interacting non-static partner, CCD, joint or actual contact
still rejects. A speed cap alone never certifies collision-solver displacement.
This does not certify arbitrary production scenes.
The [traveller gravity/contact preflight](InteriorPortalPhysicsGravityCancellationPHY4.md)
requires a native integration speed cap on the travelling rigid body and rejects
potential floor contact before disabling portal support contacts. The
[cap lifecycle](InteriorPortalPhysicsSpeedCapLifecyclePHY4.md) installs it via
UE's GT physics interface. Ordinary filter updates preserve it; actual body
recreation retires the binding and requires a new cap. The original cancellation
fixture had body gravity disabled; the [continuous-gravity follow-up](InteriorPortalPhysicsContinuousGravityPHY4.md)
enables it, verifies downward integration and admits bounded sideways motion
under the same whole-step reach proof. This establishes free fixed-orientation
passage and near-floor safe denial in a narrow native fixture, not successful
passage for a floor-supported or generally rotating body.
The shared rigid half-turn is an exact quaternion, preserving an axial trajectory
without float-angle lateral drift across physics and virtual-camera mapping.

## P8/P9 extension boundary

Keep the planned PortalPhysicsBubble and ShadowPhysicsClone. Visual proxies and
collision representations are separate. Remote contacts act on one authoritative
physical state with gravity once and mapped impulses once. P8A first proves
static contact timing/torque; P8B proves finite-mass dynamic contact, friction and
atomic authority swap. A kinematic clone's impulse is not a finite-mass solution.
Hold forces and contact responses must join the same declared simulation order.
P9 explicitly defines joint/assembly traversal; these are not inferred from a
single-body hold implementation. Existing go/no-go gates remain unchanged.

## Migration and acceptance

1. Capture the reported lateral oscillation with body/desired/drive target poses,
   passage/route IDs, contact permission changes, recovery/transfer counters and
   physics-step IDs where available. The PHY-0 execution record now reproduces
   real repeated recovery moves in one linked, remote-held rim fixture (98
   moves, one transfer); no/unlinked controls have none. It establishes competing
   legacy hold/recovery decisions for that fixture, not every user-reported
   scenario or solver-substep correctness. Physics-step/generation identities
   remain unavailable in the baseline. See the execution record for exact scope.
2. PHY-1 now supplies stable epoch/ID/generation registration, collision-shape
   extraction and one read-only held/free body eligibility query with tested
   stale-token rejection. Its conservative fixed-orientation translation rule
   explicitly rejects continuous rotation. Legacy drive/contact writers remain
   active; full P10 route/obstruction migration and swept rotational support are
   still required. PHY-2 now proves actual Chaos timing, capped force application,
   a restricted transfer interval and cancellation/retirement in an isolated
   editor fixture. See the execution and boundary-spike records for exact scope;
   these foundations are not Core acceptance. PHY-3 now validates bounded drive
   and shape-aware target/route solving for its certified static-region profile,
   with actual light/heavy blocked targets and free rotation/energy checks.
   PHY-4 now proves the coordinator/durable-fact slice in the editor fixture;
   actual-world volume observations and a reusable native transfer primitive now
   also pass narrow tests, including normal blocked-exit cancellation. Certified
   static solver/topology coverage now passes its strict native profile; general
   dynamic scene coverage, coordinated production cutover and Core acceptance
   remain unfinished. None of these isolated slices closes the carrying defect.
3. Route free and held bodies through the same passage coordinator. Replace the
   fixed sphere continuous-hold sweep, ad hoc whole-support ignore decision and
   independent position rollback. Remove superseded branches when switching;
   old/new systems must never both write the body's movement/contact state.
4. Validate Core held/free behavior before extending to P8/P9. Unsupported cases
   block or release with a reason; fallback must not reactivate competing writers.

Automated gates: geometry and rotated/compound fit, swept boundary eligibility,
route/target transform consistency, stale generation rejection, same-interval
transfer exclusion, synchronized recovery and idempotent teardown. Real Chaos
tests measure drive force/error, velocity continuity and bounded energy under
declared fixed/substep configurations; pure math tests cannot prove contact.

PIE matrix: no portal, unlinked portal, linked pair; wall-parallel carrying;
rim/corner grazing; rotate while held; partial insert/retreat; blocked exit;
light/heavy bodies; throw/spin; player-first/body-first crossings; reset/replacement
while held. Repeat at declared frame-rate/substep settings. Ordinary wall sliding
must have zero recovery teleports, no alternating contact permission at a steady
pose, and no sustained target/body oscillation. Define numeric force, error,
energy and discontinuity tolerances in the implementation fixture before runs;
do not tune the pass threshold after observing a failure.

Record actual implementation/build/Automation/Chaos/PIE evidence separately.
This document records no passing physical validation. Unperformed manual gates
must be labelled **USER ACTION REQUIRED** when implementation is delivered.
