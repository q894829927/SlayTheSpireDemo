# Portal Performance / VRAM P1 Execution Plan

**2026-09-20 current-work note:** the user's crop-display repair is tracked in
[InteriorPortalCropDisplayRegression.md](InteriorPortalExperiment/InteriorPortalCropDisplayRegression.md).
It retains Ping-Pong, corrects receiving-view coordinates and depth extraction,
and retains a code default of off. The user has now confirmed Ping-Pong=1
manual acceptance complete; restoring the default is eligible but not yet done.
The repair does not claim that this plan's resource-reclaim or performance
gates are complete. The phase descriptions below remain the performance plan;
they are not a current implementation-status checklist.

Date: **2026-09-16**

Status:

```text
AUTHORIZED / READY FOR IMPLEMENTATION
```

Working branch:

```text
portal/full-fidelity-p1
```

Validated functional baseline:

```text
6d514466639858c1fd903d0f9abf96d5aad02077
portal: finalize native full-fidelity lifecycle cleanup
```

Validation closure:

```text
2026-09-16
Editor Build = PASS
native lifecycle PIE startup = PASS
short FullFidelity smoke = PASS
portal.DumpFullFidelityRenderer = PASS
```

The authoritative functional-regression record is:

```text
docs/InteriorPortalExperiment/InteriorPortalFullFidelityProductionRegression.md
```

This plan continues development on the same branch. No new implementation branch is required for this phase.

---

## 1. Purpose

The current single-pair FullFidelity Portal renderer has completed its functional gate. P1 must not reopen accepted projection, aperture, depth, TSR, Lumen, publication-retirement or composition behavior unless a reproduced regression proves it necessary.

The immediate next phase is a narrow **Portal Performance / VRAM P1** pass before broad Portal physics work.

P1 exists to:

```text
preserve accepted FullFidelity image correctness
preserve current TSR/Lumen quality
make recursion-resource ownership explicit and measurable
reclaim resources that exceed the configured recursion-capacity budget
avoid unnecessary deep recursive submissions
complete production evidence for the existing bounded main-pass path
measure logical ownership separately from observed GPU/RHI memory
```

P1 deliberately does **not** redesign the renderer.

---

## 2. Accepted production architecture

The accepted production path remains:

```text
AInteriorPlayerController
    ↓
InteriorPortalFullFidelityRenderer
    ↓
InteriorPortalFullFidelityBackend
    ↓
InteriorPortalMultiVisibleTSRPrivate::StartMultiVisible(World)
    ↓
FMultiVisibleProducer
    ↓
endpoint × recursion renderer
```

Baseline contracts that P1 must preserve:

- FullFidelity owns the production remote-view path while active.
- `AInteriorPlayerController` bypasses legacy `PortalSystem->RenderViews()` while FullFidelity owns rendering.
- Production `Start / Stop / Dump` call native backend APIs directly.
- Production lifecycle does not dispatch portal console commands through `GEngine::Exec(...)`.
- Historical console commands remain operator/diagnostic aliases only.
- Startup failure propagates as a real failure.
- endpoint × recursion-level temporal history remains independent where a layer is active.
- render-thread-ordered publication retirement remains authoritative for visible/offscreen transitions.

---

## 3. Resource and submission terminology

The following concepts are deliberately separate and must not be conflated.

### 3.1 RequestedDepth

```text
RequestedDepth = clamp(PortalSystem->RecursionDepth, 1, MaxRecursionDepth)
```

This is the configured recursion-capacity ceiling.

A newly reusable/active lifetime may only exist for:

```text
Level < RequestedDepth
```

A level that was previously valid but becomes `Level >= RequestedDepth` does **not** need to disappear synchronously. It must immediately stop being reusable and enter ordered retirement.

### 3.2 VisibleDepth

The number of consecutive recursion requests that the accepted geometry/visibility builder can construct for one endpoint before P1B policy is applied.

```text
VisibleDepth <= RequestedDepth
```

Short-term visibility does not define persistent resource lifetime.

### 3.3 EffectiveDepth

The consecutive recursion depth selected by geometry plus the active submission policy.

Before P1B exists:

```text
EffectiveDepth == VisibleDepth
```

P1A-4 capacity-backpressure refinement: if two retiring generations already
occupy a level, allocation is deferred and EffectiveDepth temporarily describes
the consecutive ready prefix, so it may be below VisibleDepth even before P1B.
The first unavailable layer reports `VIEWSTATE_OR_LIFETIME_UNAVAILABLE`;
AttemptedLayerMask still records only actual SubmitLayer calls. This bounded
fallback keeps L0 rendering without waiting or growing retirement ownership.

After P1B exists, screen-coverage policy may produce:

```text
EffectiveDepth <= VisibleDepth <= RequestedDepth
```

`EffectiveDepth` describes the intended continuous recursion chain. It does **not** prove that every corresponding `SubmitLayer()` call succeeded.

### 3.4 AttemptedLayerMask / SubmittedLayerMask / SubmissionCount

Current production code can continue attempting shallower levels after a deeper `SubmitLayer()` failure. Therefore a scalar `SubmittedDepth` is ambiguous and must not be the authoritative diagnostic.

Use:

```text
AttemptedLayerMask
    levels for which SubmitLayer() was attempted this frame

SubmittedLayerMask
    exact levels whose SubmitLayer() returned success this frame

SubmissionCount
    popcount(SubmittedLayerMask)
```

Example:

```text
EffectiveDepth      = 3
AttemptedLayerMask  = 0b111
SubmittedLayerMask  = 0b101
SubmissionCount     = 2
```

This does **not** mean a continuous submitted depth of 2.

P1A-1 reports this fact without changing current submission behavior.

### 3.5 Owned / Active / Retiring / Reclaimable resource sets

Persistent ownership must be reported as sets or masks rather than only one ambiguous `AllocatedDepth` scalar.

At minimum expose concepts equivalent to:

```text
OwnedLayerMask
ActiveLayerMask
RetiringLayerMask
ReclaimableLayerMask
```

and corresponding counts.

Lazy allocation is allowed. High `RequestedDepth` does not force eager allocation.

### 3.6 Capacity invariant with asynchronous retirement

The old unconditional invariant:

```text
AllocatedDepth <= RequestedDepth
```

is **not valid during queue-safe retirement**.

The correct contract is:

```text
new submission / reusable active lifetime:
    Level < RequestedDepth

existing lifetime with Level >= RequestedDepth:
    immediately enters RETIRING
    receives no new submissions
    cannot be reused as the new active lifetime

while retirement is incomplete:
    total owned resources may temporarily exceed RequestedDepth

once retirement reaches quiescence:
    all non-retiring reusable ownership satisfies RequestedDepth
```

Dump must expose retiring ownership explicitly rather than hiding it to make the capacity count look smaller.

### 3.7 Fully warmed capacity expectation

For a fully warmed, fully visible two-endpoint scene at configured depth `D`, the maximum expected active/reusable endpoint-level capacity is approximately:

```text
2 × D
```

This is an upper bound / warmed-state expectation, not a mandatory equality at every frame.

### 3.8 Visibility is not lifetime

Mandatory rule:

```text
visibility/workload change != persistent resource-lifetime change
```

Short-term offscreen transitions and P1B screen-coverage cutoffs do not automatically destroy and recreate persistent resources every frame.

Persistent reclaim is driven primarily by:

```text
RequestedDepth decreasing
renderer stop/teardown
explicitly documented capacity-policy changes
```

---

## 4. Current problem statement

### 4.1 ViewState ownership starts maximum-oriented

The producer supports:

```text
Blue   L0 L1 L2 L3
Orange L0 L1 L2 L3
```

and currently allocates all eight ViewState objects at startup.

`FSceneViewState::Allocate()` does not imply that every possible TSR/Lumen texture is immediately materialized at maximum size. The practical problem is lifetime: after deep levels render and establish temporal state, reducing the configured capacity does not currently provide a safe per-layer reclaim path.

### 4.2 Color targets grow without a capacity-shrink contract

`AInteriorPortal::EnsureTargets()` grows its color-target array and resizes existing targets but does not remove surplus levels when the configured capacity ceiling decreases.

Current request creation may call it with `VisibleDepth`; P1 must not reinterpret temporary invisibility as permission to destroy resources.

### 4.3 Depth targets are lazy but lack runtime capacity reclaim

Depth targets are created when a layer is actually submitted. Keep this lazy behavior.

P1 must not replace it with eager allocation. It must only add safe retirement for depth targets belonging to levels that leave the configured capacity budget.

### 4.4 FullFidelity output targets remain full-view sized

Portal color/depth outputs remain derived from the constrained player view. `PrimaryResolutionFraction` lowers internal primary resolution but does not inherently shrink final extraction targets.

Portal-projected RT allocation and TSR-primary-resolution depth targets remain **P2**, not P1.

### 4.5 Each submitted layer is expensive

Each successful recursion submission is a real Lit `FSceneViewFamily` using the accepted temporal and lighting path, including TSR/Lumen behavior.

P1 may reduce unnecessary submissions, but it must not silently reduce quality settings.

---

## 5. Non-negotiable publication and lifetime protocol

The accepted offscreen-publication fix is a hard dependency:

```text
docs/InteriorPortalExperiment/InteriorPortalOffscreenPublicationRetirement.md
```

### 5.1 Runtime resource states

P1A-2 will implement/centralize states equivalent to:

```text
UNALLOCATED
ALLOCATED
ACTIVE
RETIRING
RECLAIMABLE
```

Exact names may differ.

### 5.2 Retirement order

For a lifetime leaving the configured capacity budget:

```text
ACTIVE / ALLOCATED
    ↓
advance lifetime/publication identity
    ↓
prevent old extraction callbacks from publishing again
    ↓
stop all new submissions for that old lifetime
    ↓
enqueue publication retirement in render-queue order
    ↓
allow already queued consumers to finish
    ↓
confirm dependent render-thread use is retired
    ↓
RECLAIMABLE
    ↓
destroy/release ViewState, color/depth targets and dependent references
```

Synchronous game-thread `ClearRequest()` followed by immediate runtime destruction is not accepted.

### 5.3 Old retirement must not clear a new lifetime

A stale retirement command may only affect the lifetime/generation it was created to retire.

If a new lifetime publishes before an old retirement command executes, the old command must not clear the new publication.

### 5.4 Runtime rebuild identity

Startup `ResetLayer()` currently resets `HistoryGeneration` to `1`. This is acceptable only for a newly started producer lifetime.

Runtime destroy/recreate must use identity that cannot collide with an old in-flight lifetime. Preferred model:

```text
LayerLifetimeId + PublicationGeneration
```

A monotonic generation model is also acceptable if it proves the same property.

### 5.5 Recursive extension dependency

If a parent ViewState is destroyed/recreated, every recursive composition extension or other object that retains its old pointer/reference must be rebuilt or rebound before the new lifetime can submit.

No dangling `SceneViewStateInterface` is permitted.

### 5.6 Rapid capacity reversal: 4 -> 1 -> 4 before retirement completes

This is an explicit supported lifecycle case and must be automated.

Preferred contract is **bounded old/new lifetime coexistence**, not blocking the game thread until retirement completes:

```text
old L2 lifetime #7 -> RETIRING
RequestedDepth returns to 4
new L2 lifetime #8 -> ACTIVE when needed
old L2 lifetime #7 -> continues ordered retirement
```

Required invariants:

- old lifetime receives no new submissions;
- new lifetime has distinct identity;
- old extraction callbacks cannot publish into the new lifetime;
- old retirement commands cannot clear the new publication;
- coexistence is bounded by the retirement queue/fence, not allowed to grow without limit.

An implementation may instead wait for retirement before reactivation only if it can prove that this does not introduce an unacceptable runtime stall. The non-blocking bounded-coexistence model is the preferred P1 design.

---

# 6. P1A — Resource Lifetime & Budget

## 6.1 P1A-0 — Documentation/evidence closure

Repository status must remain internally consistent:

```text
FullFidelity functional gate = COMPLETE / VALIDATED / SEALED
baseline = 6d514466639858c1fd903d0f9abf96d5aad02077
Build = PASS
native lifecycle PIE startup = PASS
short smoke = PASS
Dump = PASS
```

Historical execution documents must clearly point to this plan as the current authority.

No historical validation matrix needs to be rerun solely for documentation closure.

## 6.2 P1A-1 — Structured resource baseline and Dump instrumentation

### Goal

Observe the current system accurately before changing allocation/reclaim behavior.

### Required frame-level diagnostics

At minimum:

```text
RequestedDepth
VisibleDepth per endpoint
EffectiveDepth per endpoint
AttemptedLayerMask per endpoint
SubmittedLayerMask per endpoint
SubmissionCount per endpoint
PublishedLayerMask per endpoint
last endpoint visible/submitted/published masks
```

Where current code can identify it, also report per-level submission failure reason.

### Required resource diagnostics

Per endpoint / per level:

```text
lifetime id
publication generation
resource state
ViewState allocated yes/no
color target allocated yes/no + dimensions + format
depth target allocated yes/no + dimensions + format
FramesSubmitted
last extraction/depth-extraction frame
camera-cut count
continuous-history frame count
```

Aggregate:

```text
OwnedLayerMask / OwnedCount
ActiveLayerMask / ActiveCount
RetiringLayerMask / RetiringCount
ReclaimableLayerMask / ReclaimableCount
shared scratch allocated yes/no + dimensions + format
```

### P1A-1 observation-status rule

P1A-1 must not implement P1A-2 merely to make every report field look complete.

Every lifetime/state field may explicitly report one of:

```text
IMPLEMENTED
INFERRED
NOT_IMPLEMENTED
```

Examples:

```text
LayerLifetimeId = NOT_IMPLEMENTED
ResourceState = INFERRED_FROM_CURRENT_OWNERSHIP
Retiring = NOT_IMPLEMENTED
```

This is preferable to inventing semantics that production code does not yet have.

### Logical ownership vs real GPU memory

Portal diagnostics can prove:

```text
which objects/resources the Portal system still owns
which known resources are active/retiring/reclaimable
explicit RT dimensions/formats
estimated explicit RT bytes
```

They do **not** by themselves prove that D3D12/RHI has already returned the same bytes to the global memory budget.

Record separately:

```text
Portal logical ownership counters
estimated explicit RT bytes
observed RHI/GPU memory
GPU frame time
capacity-transition peak frame time
```

### Baseline matrix

Capture at least:

```text
RequestedDepth 1
RequestedDepth 2
RequestedDepth 3
RequestedDepth 4
4 -> 1
1 -> 4
rapid 4 -> 1 -> 4 without waiting for old retirement
portal visible
portal offscreen
Stop -> Restart
```

P1A-1 must not alter production allocation/reclaim behavior except where strictly necessary to expose accurate diagnostics.

Suggested commit:

```text
portal: report full-fidelity recursion resource lifetimes
```

## 6.3 P1A-2 — Explicit lifetime state and identity

Implement/centralize the state and identity model from Section 5.

Required properties:

- stable active level retains ViewState identity and temporal continuity;
- runtime-recreated level receives a new lifetime identity;
- old extraction callbacks cannot publish into a new lifetime;
- old retirement commands cannot clear a new publication;
- diagnostics expose active versus retiring versus reclaimable;
- rapid `4 -> 1 -> 4` is bounded and race-safe.

Suggested commit:

```text
portal: model recursion layer lifetime and retirement state
```

## 6.4 P1A-3 — Lazy/on-demand ViewState ownership

Stop allocating all eight ViewStates at producer startup.

Policy:

```text
RequestedDepth defines the maximum level eligible for a new active lifetime.
A permitted level allocates lazily when first needed.
Short-term invisibility does not force destruction.
RequestedDepth decrease retires every old owned lifetime with Level >= new depth.
```

After a fully warmed two-endpoint run:

```text
RequestedDepth=4 -> up to 8 active/reusable ViewStates may exist
RequestedDepth=1 -> old L1/L2/L3 enter RETIRING and eventually disappear
```

During asynchronous retirement, total owned ViewStates may temporarily be higher than the new configured capacity.

Do not change TSR, Lumen, projection, clipping, aperture or composition math.

Suggested commit:

```text
portal: allocate recursion view states within configured capacity
```

## 6.5 P1A-4 — Queue-safe runtime reclaim

Implementation update, 2026-09-21: queue-safe retirement and P1A-3's remaining
depth-decrease integration are implemented in the working tree. Evidence and
remaining gates: [P1A-3/P1A-4 capacity validation](InteriorPortalExperiment/InteriorPortalCapacityReclaimValidation.md).
CPU contracts and actual D3D12 ownership transitions pass; transition visual
acceptance and performance closure remain open. P1A-5/P1A-6/P1A-7 target capacity
work is not complete. Do not interpret this as a full P1A seal.

Implement runtime reclaim for old lifetimes that leave the configured capacity budget.

Do not use repeated unconditional `FlushRenderingCommands()` as the normal capacity-change mechanism. `Stop()` may remain a synchronous teardown boundary.

Normal gameplay capacity changes should use queue-ordered retirement plus an appropriate completion/fence mechanism.

Measure transition hitch/peak frame time.

Suggested commit:

```text
portal: retire recursion lifetimes without runtime flush stalls
```

## 6.6 P1A-5 — Color-target capacity shrink

Implementation update, 2026-09-21: fallback/legacy per-level color-target
retirement is implemented in `6404baf08f0fcfce0a3dd21ed1aa5fb94da541dd`.
Out-of-budget color targets leave the active `RenderTargets` array with their
exact retiring recursion lifetime and are released only after that lifetime's
existing RHI-thread-depth fence completes. Stop also releases active fallback
color targets. Report schema v4 exposes active versus retiring color ownership.

Validation is complete: the user confirmed the UE 5.8 Development Editor build,
focused FullFidelity Automation, actual D3D12 fallback `4 -> 1 -> 4` capacity
shrink/regrowth, offscreen no-shrink behavior, Stop zero ownership, and rapid
`4 -> 1 -> 4` visual stability. **P1A-5 = COMPLETE / VALIDATED.** See
[the P1A-5 validation record](InteriorPortalExperiment/InteriorPortalColorTargetCapacityValidation.md).

Policy:

```text
RequestedDepth decrease
-> color resources above the new ceiling enter retirement/release

portal briefly offscreen
-> do not shrink solely because VisibleDepth became 0

P1B lowers EffectiveDepth
-> do not shrink solely because fewer layers are selected/submitted
```

A target needed by an old retiring lifetime remains alive until its consumers are safe. It may therefore temporarily contribute to owned memory above the new capacity ceiling.

Suggested commit:

```text
portal: shrink color targets beyond recursion capacity
```

## 6.7 P1A-6 — Depth-target capacity reclaim

Depth target creation remains lazy on submission.

Policy:

```text
never eagerly allocate unused depth targets
in-budget owned level may retain its depth target across short visibility changes
out-of-budget old lifetime enters retirement and releases the depth target only when safe
```

P1 does not change depth-target resolution.

Suggested commit:

```text
portal: reclaim depth targets beyond recursion capacity
```

## 6.8 P1A-7 — Shared scratch lifecycle audit

Verify:

- one expected producer scratch target;
- viewport-size changes do not leak old generations;
- Stop/Restart releases and recreates correctly;
- no legacy renderer scratch survives accidental dual-path execution;
- scratch replacement does not depend on unnecessary repeated runtime flushes.

Do not split scratch per recursion level.

---

# 7. P1A Gate

## 7.1 Build

```text
SlayTheSpireDemoEditor Win64 Development = PASS
```

## 7.2 Lifecycle automation requirements

Add focused automated coverage for at least:

```text
stable 4 -> 1 -> 4
rapid 4 -> 1 -> 4 before old retirement completes
retained L0 lifetime/identity remains stable
recreated L1-L3 receive new lifetime identities
old extraction callback cannot publish after lifetime retirement
old retirement command cannot clear a newly published lifetime
out-of-budget lifetime receives no new submissions while RETIRING
depth reduction while endpoint is offscreen still retires levels above the configured ceiling
short offscreen transition does not cause allocation churn within unchanged capacity
P1B-style lower EffectiveDepth does not imply immediate resource destruction
Stop -> Restart leaves no old resource ownership/publication alive
```

Automation proves deterministic lifetime/ownership facts. It does not replace PIE visual acceptance.

## 7.3 Submission-failure diagnostic contract

P1A-1 does not have to change existing `SubmitLayer()` failure behavior, but the report must expose it accurately.

Before P1A/P1B is sealed, define and test the production rule for child submission failure:

```text
failed child submission
-> current-frame child result is not valid
-> parent must not accidentally consume a stale child publication as if it were current
```

The implementation may terminate recursion at that point or render the parent with an explicit recursion terminator. It must not silently reuse stale child output.

## 7.4 PIE visual matrix

```text
single visible portal                         PASS
dual visible portals                         PASS
RequestedDepth = 1                           PASS
RequestedDepth = 2                           PASS
RequestedDepth = 3                           PASS
RequestedDepth = 4                           PASS
stable 4 -> 1                                PASS
stable 1 -> 4                                PASS
rapid 4 -> 1 -> 4                            PASS
oblique/grazing view                         PASS
fast visible -> offscreen -> visible         PASS
no stale frame                               PASS
no unexpected spiral flash                   PASS
no renderer crash/assert                     PASS
```

The accepted offscreen publication-retirement regression must remain PASS.

## 7.5 Capacity invariants

Always:

```text
EffectiveDepth <= VisibleDepth <= RequestedDepth <= MaxRecursionDepth
SubmissionCount == popcount(SubmittedLayerMask)
```

For active/reusable lifetimes:

```text
Level < RequestedDepth
```

During async retirement:

```text
owned RETIRING lifetimes may temporarily exist at Level >= RequestedDepth
```

After queue-safe retirement reaches quiescence:

```text
no non-retiring reusable lifetime exists at Level >= RequestedDepth
```

## 7.6 Performance-transition invariant

Record transition peak frame time. Do not accept lower steady-state ownership if it requires a large avoidable game-thread/render-thread flush hitch during normal capacity changes.

---

# 8. P1B — Recursion Screen-Coverage Cutoff

P1B begins only after P1A passes.

## 8.1 Goal

Reduce expensive deep recursive scene submissions when the next nested portal contributes very little projected area.

P1B is a **submission/workload policy**, not a persistent-resource reclaim policy.

```text
P1B lowers EffectiveDepth.
P1B does not automatically lower persistent resource capacity.
```

## 8.2 L0 policy

L0 is never removed by the P1B screen-coverage threshold while the endpoint itself is valid and visible.

Coverage cutoff applies only to:

```text
L1 and deeper
```

## 8.3 Coverage coordinate space

Each deeper request is built from the previous `ParentView` / virtual view.

Therefore P1 uses:

```text
RecursiveParentViewCoverage
= conservative projected portal area
  / current parent virtual-view area
```

This is **not** claimed to equal final main-screen contribution.

## 8.4 Threshold

Expose a tunable control such as:

```text
portal.MinRecursionScreenCoverage
```

Candidate validation values:

```text
0      = disabled
0.001  = 0.10%
0.0025 = 0.25%
0.005  = 0.50%
```

No production default is selected without measurement and visual acceptance.

## 8.5 Stop rule

For L1+ only:

```text
if next request is invalid
    stop deeper recursion

if RecursiveParentViewCoverage < threshold
    stop deeper recursion
```

This selects `EffectiveDepth`; actual success remains represented by `SubmittedLayerMask` and `SubmissionCount`.

## 8.6 Stability

Coverage decisions must not produce threshold flicker. Small enter/exit hysteresis is allowed if needed. Do not use a broad time cooldown and do not trigger immediate persistent-resource destruction from threshold crossings.

## 8.7 Diagnostics

Report:

```text
RequestedDepth
VisibleDepth
EffectiveDepth
AttemptedLayerMask
SubmittedLayerMask
SubmissionCount
PublishedLayerMask
cutoff reason
parent-view coverage per tested level
```

## 8.8 P1B Gate

Validate:

```text
threshold=0 reproduces P1A policy
L0 always survives coverage cutoff
large nested portal reaches requested depth
tiny/distant nested portal stops earlier
bright/high-contrast small nested portal is visually checked
oblique portal remains stable
threshold crossing does not flicker
look-away/look-back remains stable
no stale child publication survives cutoff
persistent ownership does not churn merely because EffectiveDepth changes
```

Record scene submissions and GPU time for fixed-camera scenarios.

Suggested commit:

```text
portal: bound deep recursion by parent-view screen coverage
```

---

# 9. P1C — Bounded Main-Pass Production Evidence

P1C is not a from-zero correctness implementation.

Existing accepted evidence:

```text
docs/InteriorPortalExperiment/InteriorPortalBoundedMainPassValidation.md
```

already records user-confirmed PIE A/B correctness for `portal.BoundedMainPassScissor`.

P1C fills the remaining production evidence gaps after P1A/P1B.

## 9.1 Required additional evidence

Validate at least:

```text
dual visible portal scenario
recursion >= 2
P1A/P1B regression interaction
fixed-camera GPU timing: scissor 0 vs 1
near-screen-edge / partial offscreen portal
oblique/grazing portal
rapid camera motion / TSR jitter
```

## 9.2 Contracts that remain full-view

Do not shrink these merely to increase scissor savings:

```text
full-view stencil-bit clear
SceneColor prefill required for sparse bounded output
existing full-view proof/debug behavior documented by the validation record
```

P1C primarily targets raster work, shader invocations and bandwidth. Do not count it as major persistent-VRAM reduction without measurement.

## 9.3 Production-default decision

The default may change only if the expanded production matrix and timing evidence justify it.

Keeping the default disabled remains valid if correctness/performance evidence is incomplete.

---

# 10. Measurement protocol

## 10.1 Fixed conditions

Keep constant:

```text
same map
same viewport/window resolution
same graphics settings
same renderer backend
same portal transforms
same camera transforms
same RequestedDepth
same PrimaryResolutionFraction
```

## 10.2 Cold versus warm measurements

Report cold and warm behavior separately.

### Cold start

Measure separately from:

```text
PIE/native renderer startup
-> first stabilized portal frame
```

Do not mix cold allocation/shader/history establishment into warm steady-state numbers.

### Warm steady state

For comparison runs:

1. reach the fixed test camera and requested configuration;
2. allow portal ViewStates, TSR/Lumen histories and render targets to warm;
3. use the same warm-up duration for baseline and optimized builds;
4. after warm-up, sample a fixed window.

Recommended initial protocol:

```text
warm-up >= 300 frames
sample >= 300 frames
```

A longer window is acceptable, but baseline and comparison must use the same rule.

### Capacity transition

For `4 -> 1`:

```text
fully warm RequestedDepth=4
record pre-transition steady state
change to RequestedDepth=1
record transition peak frame time / memory peak
observe RETIRING ownership
wait for retirement completion
record post-retirement steady state
```

For rapid reversal:

```text
fully warm 4
-> set 1
-> before retirement completes set 4
-> record old/new lifetime coexistence, peak ownership and frame-time spike
```

## 10.3 Required scenarios

```text
A. portal not visible
B. one endpoint visible, shallow recursion
C. both endpoints visible
D. recursion depth 2
E. recursion depth 4 / fully warmed
F. 4 -> 1 capacity transition
G. stable 1 -> 4 re-expansion
H. rapid 4 -> 1 -> 4
I. small nested portal with P1B cutoff on/off
J. bounded-main-pass scissor 0/1 at fixed camera
```

## 10.4 Required statistics

Record separately where tooling permits:

```text
Portal logical ownership
Active / Retiring / Reclaimable counts
explicit RT estimated bytes
observed GPU/RHI memory before / peak / after
GPU frame time average
GPU frame time median
GPU frame time P95
GPU frame time P99
transition maximum frame time
AttemptedLayerMask / SubmittedLayerMask / SubmissionCount
```

If a metric is unavailable, mark it unavailable rather than substituting a different quantity.

Do not claim a percentage improvement without measured data.

---

# 11. Explicit P1 non-goals

The following remain outside P1:

```text
portal-projected/cropped color RT allocation
TSR-primary-resolution depth-target redesign
Lumen GI quality reduction
Lumen reflection quality reduction
shadow quality reduction
post-process quality reduction
TSR replacement
projection-matrix redesign
portal-specific frustum/PVS/room visibility
stencil architecture redesign
physics traversal changes
partial-body physics/contact bridging
```

These belong to Performance P2 or later work after P1 is sealed.

---

# 12. Commit and regression policy

Prefer narrow commits aligned with one contract at a time:

```text
docs(portal): align P1 authority and lifecycle contracts
portal: report full-fidelity recursion resource lifetimes
portal: model recursion layer lifetime and retirement state
portal: allocate recursion view states within configured capacity
portal: retire recursion lifetimes without runtime flush stalls
portal: shrink color targets beyond recursion capacity
portal: reclaim depth targets beyond recursion capacity
portal: add recursion lifetime automation coverage
portal: bound deep recursion by parent-view screen coverage
portal: record bounded main-pass production evidence
```

If a step breaks accepted FullFidelity correctness, reopen/revert only that step. Do not mask lifetime bugs with arbitrary delays, permanent maximum retention or unconditional runtime flushes.

---

# 13. Final P1 Gate

Portal Performance / VRAM P1 is sealed only when:

```text
[ ] P1A-0 documentation authority aligned
[ ] P1A-1 structured baseline captured without inventing unimplemented state
[ ] EffectiveDepth and exact submission masks/counts are reported separately
[ ] lifetime/publication identity is explicit across runtime rebuilds
[ ] rapid 4 -> 1 -> 4 is race-safe and bounded
[ ] old callbacks/retirement cannot mutate a new lifetime
[ ] ViewState ownership is lazy within configured capacity
[ ] out-of-budget old lifetimes retire safely and receive no new submissions
[ ] retirement completes without hidden long-term over-budget ownership
[ ] color targets above capacity are reclaimed after retirement
[ ] depth targets above capacity are reclaimed after retirement
[ ] shared scratch lifecycle audited
[ ] child submission failure cannot expose stale child publication as current
[ ] lifecycle-focused Automation PASS
[ ] FullFidelity visual PIE regression PASS
[ ] no fast offscreen/return spiral regression
[ ] P1B L1+ cutoff validated without persistent-resource thrash
[ ] P1C bounded-main-pass production evidence recorded
[ ] cold/warm/transition measurement protocol followed
[ ] logical ownership and real GPU/RHI measurements are reported separately
[ ] no unacceptable capacity-transition hitch introduced
[ ] no FullFidelity quality reduction was used to manufacture the result
```

After this gate closes, proceed to:

```text
Portal Physics P1
```

More invasive render-target cropping, quality-tiering, portal-specific frustum and room/PVS work belong to later performance phases.
