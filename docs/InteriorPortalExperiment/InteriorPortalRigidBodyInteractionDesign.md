# Portal rigid-body interaction contract

Status: **design only, implementation and acceptance pending**, 2026-09-20.
Reviewed baseline: `9259f8e`. This document refines P5/P6/P10 in
[the implementation plan](InteriorPortalFullFidelityImplementationPlan.md).
[The current execution plan](InteriorPortalCurrentExecutionPlan.md) still owns
phase order. It does not authorize beginning P8/P9 before their existing gates.

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
   physics-step IDs. Current pull-versus-rollback explanation is a hypothesis,
   not a reproduced root cause. Test with unlinked and linked portals.
2. Establish P5 identity/geometry and shared P10 eligibility with pure contract
   tests. Introduce the physics adapter boundary and prove simulation timing.
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
