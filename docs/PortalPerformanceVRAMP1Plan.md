# Portal Performance / VRAM P1 Execution Plan

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

## 3. Resource terminology and invariants

P1 uses four different depth concepts. They must not be conflated.

### 3.1 RequestedDepth

```text
RequestedDepth = clamp(PortalSystem->RecursionDepth, 1, MaxRecursionDepth)
```

This is the configured recursion-capacity ceiling. It defines which levels are allowed to own production resources.

### 3.2 VisibleDepth

The number of recursion requests that can currently be built for one endpoint from the accepted geometry/visibility pipeline.

```text
VisibleDepth <= RequestedDepth
```

A portal moving offscreen may make `VisibleDepth` zero without changing the configured resource-capacity budget.

### 3.3 SubmittedDepth

The number of full scene-view submissions actually issued for one endpoint in the current frame.

P1B may reduce this below `VisibleDepth`.

```text
SubmittedDepth <= VisibleDepth <= RequestedDepth
```

### 3.4 AllocatedDepth / AllocatedCapacity

The recursion levels for which this producer currently owns persistent resources such as ViewState, color/depth targets or dependent extensions.

Allocation is allowed to be lazy. P1 must **not** require unused levels to be allocated merely because `RequestedDepth` is high.

Required invariant:

```text
AllocatedDepth <= RequestedDepth
```

For a fully warmed, fully visible two-endpoint scene at requested depth `D`, the maximum expected endpoint-level ownership is approximately:

```text
2 × D
```

This is a warmed-capacity expectation / upper bound, not a mandatory equality at every frame.

### 3.5 Visibility is not lifetime

The following rule is mandatory:

```text
visibility/workload change != persistent resource-lifetime change
```

Short-term offscreen transitions and P1B screen-coverage cutoffs must not automatically destroy and recreate resources every frame.

Persistent reclaim is driven primarily by:

```text
configured RequestedDepth decreasing
renderer stop/teardown
explicitly documented capacity-policy changes
```

This prevents allocation thrash during normal camera movement.

---

## 4. Current problem statement

### 4.1 ViewState ownership starts maximum-oriented

The producer supports:

```text
Blue   L0 L1 L2 L3
Orange L0 L1 L2 L3
```

and currently allocates all eight ViewState objects at startup.

`FSceneViewState::Allocate()` does not imply that every possible TSR/Lumen texture is immediately materialized at maximum size. The important problem is lifetime: once deeper layers have rendered and accumulated temporal renderer state, reducing the configured recursion depth does not currently provide an explicit per-layer reclaim path.

### 4.2 Color targets grow without a shrink contract

`AInteriorPortal::EnsureTargets()` grows the color-target array and resizes existing targets, but does not remove surplus levels when the configured recursion-capacity ceiling decreases.

Current request creation may call it with `VisibleDepth`; P1 must not reinterpret that as permission to destroy targets merely because a portal is briefly offscreen.

### 4.3 Depth targets are lazy but lack runtime capacity reclaim

Depth targets are created when a layer is actually submitted. This behavior is useful and must remain lazy.

P1 must not replace it with eager `RequestedDepth × endpoints` allocation.

The required change is that a depth target belonging to a level that is now **outside the configured capacity ceiling** can retire safely and be reclaimed.

### 4.4 FullFidelity output targets remain full-view sized

The current FullFidelity producer derives its output size from the constrained player view. `PrimaryResolutionFraction` lowers the internal primary render size but does not inherently shrink final color/depth extraction targets.

Portal-projected RT allocation and TSR-primary-resolution depth targets are **P2**, not P1.

### 4.5 Each submitted recursion layer is expensive

Each submitted layer is a real Lit `FSceneViewFamily` using the accepted temporal and lighting path, including TSR and Lumen behavior.

P1 may reduce unnecessary submissions, but it must not silently lower quality settings.

---

## 5. Non-negotiable publication and lifetime protocol

The accepted offscreen-publication fix is a P1 hard dependency:

```text
docs/InteriorPortalExperiment/InteriorPortalOffscreenPublicationRetirement.md
```

P1 resource reclaim must preserve its queue-ownership semantics.

### 5.1 Layer resource states

P1 should make layer lifetime observable using states equivalent to:

```text
UNALLOCATED
ALLOCATED
ACTIVE
RETIRING
RECLAIMABLE
```

Exact enum names may differ, but diagnostics must distinguish active ownership from pending retirement.

### 5.2 Retirement order

For a layer leaving the configured capacity budget, the required conceptual sequence is:

```text
ACTIVE / ALLOCATED
    ↓
advance lifetime/publication generation
    ↓
prevent old extraction callbacks from publishing again
    ↓
stop new submissions for that retiring lifetime
    ↓
enqueue publication retirement in render-queue order
    ↓
allow already-queued consumers to finish safely
    ↓
confirm dependent render-thread use is retired
    ↓
RECLAIMABLE
    ↓
destroy/release ViewState, depth/color resources and dependent references
```

Synchronous game-thread `ClearRequest()` followed by immediate destruction is not an accepted runtime reclaim strategy.

### 5.3 Old retirement must not clear new publication

A stale retirement command must only retire an older lifetime/generation. If a level is recreated and publishes again before an old retirement command executes, that old command must not clear the new request.

### 5.4 Runtime rebuild identity

Current startup `ResetLayer()` resets `HistoryGeneration` to `1`. That is acceptable for a new producer lifetime but must not be reused blindly for runtime destroy/recreate.

P1 must introduce an identity contract that prevents old and new layer lifetimes from sharing the same effective identity. Acceptable designs include:

```text
monotonically increasing generation across rebuilds
```

or preferably an explicit pair such as:

```text
LayerLifetimeId + PublicationGeneration
```

A runtime-recreated layer must be distinguishable from every prior lifetime of the same endpoint/level.

### 5.5 Recursive extension dependency

Recursive composition extensions hold relationships to parent ViewState/layer state. If a parent ViewState lifetime is destroyed and recreated, every extension/reference that depends on its old pointer must be rebuilt or rebound before the new layer can submit.

No dangling `SceneViewStateInterface` is permitted.

---

# 6. P1A — Resource Lifetime & Budget

P1A is the first implementation block.

## 6.1 P1A-0 — Documentation/evidence closure

Before performance code changes, keep repository evidence internally consistent.

Required state:

```text
FullFidelity functional gate = COMPLETE / VALIDATED / SEALED
baseline = 6d514466639858c1fd903d0f9abf96d5aad02077
Build = PASS
native lifecycle PIE startup = PASS
short smoke = PASS
Dump = PASS
```

No historical matrix needs to be rerun solely for documentation closure.

## 6.2 P1A-1 — Structured resource baseline and Dump instrumentation

### Goal

Before changing allocation behavior, make the current resource ownership and lifecycle observable.

### Required diagnostics

`portal.DumpFullFidelityRenderer` and/or its report must expose at least:

```text
RequestedDepth
VisibleDepth per endpoint
SubmittedDepth per endpoint
last visible/submitted/published masks

per endpoint / per level:
  lifetime id
  publication generation
  resource state (unallocated/allocated/active/retiring/reclaimable)
  ViewState allocated yes/no
  color target allocated yes/no + dimensions + format
  depth target allocated yes/no + dimensions + format
  FramesSubmitted
  last extraction/depth-extraction frame
  camera-cut count
  continuous-history frame count

shared scratch:
  allocated yes/no
  dimensions
  format
```

### Logical ownership vs real GPU memory

Dump must separate what it can prove from what it cannot prove.

Portal-owned diagnostics prove:

```text
which resources this system still owns
which resources are retiring
explicit RT dimensions/formats
theoretical explicit RT bytes
```

They do **not** alone prove that D3D12/RHI has already returned the same number of bytes to the global GPU-memory budget.

Performance evidence must therefore record separately:

```text
Portal logical ownership counters
estimated explicit RT bytes
observed RHI/GPU memory
GPU frame time
peak frame time during depth/capacity transitions
```

### Baseline matrix

Use one fixed map, viewport, graphics settings, portal transforms and camera where possible:

```text
RequestedDepth 1
RequestedDepth 2
RequestedDepth 3
RequestedDepth 4
4 → 1
1 → 4
portal visible
portal offscreen
Stop → Restart
```

P1A-1 must not change production allocation/reclaim semantics except where strictly necessary to report them correctly.

Suggested commit:

```text
portal: report full-fidelity recursion resource lifetimes
```

## 6.3 P1A-2 — Explicit lifetime state and identity

Before destroying runtime resources, implement/centralize the layer state machine and monotonic lifetime identity described in Section 5.

Required properties:

- stable active level retains its ViewState identity and temporal continuity;
- runtime-recreated level receives a new lifetime identity;
- old extraction callbacks cannot publish into the new lifetime;
- old retirement commands cannot clear a new publication;
- diagnostics expose `ACTIVE` versus `RETIRING` versus `RECLAIMABLE`.

Suggested commit:

```text
portal: model recursion layer lifetime and retirement state
```

## 6.4 P1A-3 — Lazy/on-demand ViewState ownership

Stop allocating all eight ViewStates at producer startup.

Policy:

```text
RequestedDepth defines the maximum allowed level.
A permitted level may allocate lazily when it is first needed.
Short-term invisibility does not force destruction.
RequestedDepth decrease retires every owned level >= new depth.
```

Example after a fully warmed two-endpoint run:

```text
RequestedDepth=4 → up to 8 ViewStates may be owned
RequestedDepth=1 → L1/L2/L3 must retire and be reclaimed
```

After re-expansion to depth 4, deep levels allocate again only when needed and start a new lifetime.

Do not change TSR, Lumen, projection, clipping, aperture or composition math.

Suggested commit:

```text
portal: allocate recursion view states within configured capacity
```

## 6.5 P1A-4 — Queue-safe runtime reclaim

Implement runtime reclaim for levels that exceed the configured capacity budget.

Do not use frequent unconditional `FlushRenderingCommands()` as the normal capacity-change mechanism. `Stop()` may remain a synchronous teardown boundary; normal gameplay capacity changes should retire in queue order and use an appropriate completion/fence strategy.

Acceptance includes measuring transition hitch/peak frame time so a lower steady-state ownership count is not purchased with an unacceptable game-thread stall.

Suggested commit:

```text
portal: retire inactive recursion lifetimes without runtime flush stalls
```

## 6.6 P1A-5 — Color-target capacity shrink

Color-target ownership must obey the configured capacity ceiling without thrashing on visibility.

Required policy:

```text
RequestedDepth decrease
→ targets above the new ceiling retire/release

portal briefly offscreen
→ do not shrink solely because VisibleDepth became 0

P1B per-frame cutoff
→ do not shrink solely because SubmittedDepth is lower
```

Existing in-budget targets should remain stable when resolution/depth policy has not changed.

Re-expansion must recreate missing levels correctly when they become needed.

Suggested commit:

```text
portal: shrink color targets beyond recursion capacity
```

## 6.7 P1A-6 — Depth-target capacity reclaim

Depth target allocation remains lazy on submission.

Required policy:

```text
never eagerly allocate unused depth targets
owned level < RequestedDepth may retain its depth target across short visibility changes
owned level >= new RequestedDepth must retire/release safely
```

Diagnostics must distinguish owned, retiring and released depth targets.

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
- no legacy renderer scratch survives through accidental dual-path execution;
- scratch replacement does not use unnecessary repeated runtime flushes.

Do not split scratch per recursion level.

---

# 7. P1A Gate

P1A closes only after Build, automation and visual validation pass.

## 7.1 Build

```text
SlayTheSpireDemoEditor Win64 Development = PASS
```

## 7.2 Lifecycle automation requirements

Add focused automated coverage for at least:

```text
4 → 1 → 4
retained L0 lifetime/identity remains stable
recreated L1-L3 receive new lifetime identities
old extraction callback cannot publish after lifetime retirement
old retirement command cannot clear a newly published lifetime
depth reduction while endpoint is offscreen still reclaims levels above the configured ceiling
short offscreen transition does not cause allocation churn within the unchanged capacity budget
P1B-style lower SubmittedDepth does not imply immediate resource destruction
Stop → Restart leaves no old resource ownership/publication alive
```

Automation should prove deterministic ownership/lifetime facts. It does not replace visual PIE validation.

## 7.3 PIE visual matrix

```text
single visible portal                         PASS
dual visible portals                         PASS
RequestedDepth = 1                           PASS
RequestedDepth = 2                           PASS
RequestedDepth = 3                           PASS
RequestedDepth = 4                           PASS
4 → 1                                        PASS
1 → 4                                        PASS
oblique/grazing view                         PASS
fast visible → offscreen → visible           PASS
no stale frame                               PASS
no unexpected spiral flash                   PASS
no renderer crash/assert                     PASS
```

The accepted offscreen retirement regression must remain PASS.

## 7.4 Ownership/capacity invariants

Required:

```text
SubmittedDepth <= VisibleDepth <= RequestedDepth <= MaxRecursionDepth
AllocatedDepth <= RequestedDepth
```

After a fully warmed `RequestedDepth=4` scene, `4 → 1` must eventually prove that L1-L3 ownership is no longer active/retained after queue-safe retirement completes.

After `1 → 4`, recreated deep levels must have new lifetime identities and clean temporal histories.

## 7.5 Performance-transition invariant

Record transition peak frame time. Do not accept a design that obtains lower steady-state ownership by introducing a large avoidable `FlushRenderingCommands()` hitch during normal gameplay configuration changes.

---

# 8. P1B — Recursion Screen-Coverage Cutoff

P1B begins only after P1A passes.

## 8.1 Goal

Reduce expensive deep recursive scene submissions when the next nested portal contributes very little projected area.

P1B is a **submission/workload policy**, not a persistent-resource reclaim policy.

### Mandatory decoupling

```text
P1B lowers SubmittedDepth.
P1B does not automatically lower AllocatedDepth.
```

This prevents allocation/release oscillation as the camera moves around the threshold.

## 8.2 L0 policy

L0 is never removed by the P1B screen-coverage threshold when the endpoint itself is valid and visible.

Coverage cutoff applies only to:

```text
L1 and deeper recursion
```

This keeps the primary portal presentation FullFidelity and limits P1B to nested recursion reduction.

## 8.3 Coverage coordinate space

The accepted recursion builder constructs each deeper request from the previous `ParentView` / virtual view.

Therefore the simple P1 metric is explicitly:

```text
RecursiveParentViewCoverage
= conservative projected portal area
  / current parent virtual-view area
```

This is **not** claimed to equal the nested portal's final main-screen contribution.

P1 intentionally accepts this conservative, local metric rather than introducing a new cross-recursion projection algorithm.

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

The deepest actually submitted layer must preserve the accepted recursion terminator behavior.

## 8.6 Stability

Coverage decisions must not create visible threshold flicker. If necessary, use small enter/exit hysteresis. Do not use a broad time cooldown and do not trigger immediate resource destruction from threshold crossings.

## 8.7 Diagnostics

Report:

```text
RequestedDepth
VisibleDepth
SubmittedDepth
cutoff reason
parent-view coverage per tested level
scene submissions/frame
```

## 8.8 P1B Gate

Validate:

```text
threshold=0 reproduces P1A submission behavior
L0 always survives coverage cutoff
large nested portal reaches requested depth
tiny/distant nested portal stops earlier
bright/high-contrast small nested portal is visually checked, not assumed negligible
oblique portal remains stable
threshold crossing does not flicker
look-away/look-back remains stable
no stale child publication survives cutoff
AllocatedDepth does not churn merely because SubmittedDepth changes
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

P1C only fills the remaining production evidence gaps after P1A/P1B.

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

## 9.2 Accepted implementation contracts that must remain

Do not shrink these merely to increase scissor savings:

```text
full-view stencil-bit clear
SceneColor prefill required for sparse bounded output
existing proof/debug full-view behavior documented by the validation record
```

P1C primarily targets pixel/raster work, shader invocations and bandwidth. It is not counted as a major persistent-VRAM optimization unless actual measurement proves otherwise.

## 9.3 Production-default decision

The default may be changed only if the expanded production matrix and timing evidence justify it.

Keeping the default disabled remains valid if correctness or performance evidence is incomplete.

Suggested commit if enabled:

```text
portal: enable bounded full-fidelity main composition
```

---

# 10. Measurement protocol

Use repeatable conditions:

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

Minimum scenarios:

```text
A. portal not visible
B. one endpoint visible, shallow recursion
C. both endpoints visible
D. recursion depth 2
E. recursion depth 4 / fully warmed
F. 4 → 1 capacity transition
G. 1 → 4 re-expansion
H. small nested portal with P1B cutoff on/off
I. bounded-main-pass scissor 0/1 at fixed camera
```

Record separately:

```text
Portal logical ownership
retiring/reclaimable counts
explicit RT estimated bytes
observed GPU/RHI memory
GPU frame time
scene submissions/frame
capacity-transition peak frame time
```

Do not claim a percentage improvement without measured data.

---

# 11. Explicit P1 non-goals

The following remain out of P1:

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

These may be considered in Performance P2 or later work after P1 is sealed.

---

# 12. Commit and regression policy

Prefer narrow commits aligned with one contract at a time. Suggested sequence:

```text
docs(portal): align full-fidelity and VRAM P1 evidence
portal: report full-fidelity recursion resource lifetimes
portal: model recursion layer lifetime and retirement state
portal: allocate recursion view states within configured capacity
portal: retire inactive recursion lifetimes without runtime flush stalls
portal: shrink color targets beyond recursion capacity
portal: reclaim depth targets beyond recursion capacity
portal: add recursion lifetime automation coverage
portal: bound deep recursion by parent-view screen coverage
portal: record bounded main-pass production evidence
```

If a step breaks accepted FullFidelity correctness, revert/reopen only that step. Do not mask a lifetime bug with arbitrary delays, permanent maximum retention or unconditional runtime flushes.

---

# 13. Final P1 Gate

Portal Performance / VRAM P1 is sealed only when:

```text
[ ] P1A-0 documentation evidence aligned
[ ] P1A-1 structured resource baseline captured
[ ] lifetime/publication identity is explicit and monotonic across runtime rebuilds
[ ] ViewState ownership is lazy and bounded by RequestedDepth
[ ] resources above a reduced RequestedDepth retire safely
[ ] color targets above capacity are reclaimed
[ ] depth targets above capacity are reclaimed
[ ] shared scratch lifecycle audited
[ ] lifecycle-focused Automation PASS
[ ] FullFidelity visual PIE regression PASS
[ ] no fast offscreen/return spiral regression
[ ] P1B L1+ cutoff validated without resource thrash
[ ] P1C bounded-main-pass production evidence recorded
[ ] logical ownership and real GPU/RHI measurements are reported separately
[ ] no unacceptable capacity-transition hitch introduced
[ ] no FullFidelity quality reduction was used to manufacture the result
```

After this gate closes, proceed to:

```text
Portal Physics P1
```

More invasive render-target cropping, quality-tiering, portal-specific frustum and room/PVS work belong to later performance phases.
