# Portal Performance / VRAM P1 Execution Plan

Date: **2026-09-16**

Status:

```text
AUTHORIZED / READY FOR IMPLEMENTATION
```

Baseline branch:

```text
portal/full-fidelity-p1
```

Baseline validated HEAD:

```text
6d514466639858c1fd903d0f9abf96d5aad02077
portal: finalize native full-fidelity lifecycle cleanup
```

Execution branch:

```text
portal/perf-vram-p1
```

## 1. Purpose

The FullFidelity Portal renderer has passed its functional development gate. The next step is not to reopen renderer correctness work and not yet to begin broad Portal physics development.

This phase exists to remove clear VRAM/resource-lifetime waste and establish measurable performance limits while preserving the currently accepted FullFidelity visual behavior.

The phase is intentionally narrow:

```text
keep image correctness
keep current projection/composition model
keep TSR/Lumen fidelity
reduce unnecessary persistent resources
avoid rendering recursion that is no longer useful
measure every change
```

The expected outcome is a renderer whose resource ownership scales with the actually requested/visible recursion workload instead of retaining the maximum resources ever created during the session.

## 2. Current accepted architecture

The production rendering path is considered authoritative:

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

The production lifecycle no longer depends on `GEngine::Exec(...)` console dispatch. Legacy console commands remain manual diagnostics only.

FullFidelity ownership also prevents normal production execution from falling through into the legacy `PortalSystem->RenderViews()` path.

This architecture is the starting point for P1 and must remain intact unless a later approved plan explicitly replaces it.

## 3. Problem statement

The current renderer is functionally correct but has several resource/performance characteristics that are not yet production-safe.

### 3.1 ViewState allocation scales to the maximum up front

The producer currently creates state for both portal endpoints and all four recursion levels when the renderer starts:

```text
2 endpoints × 4 recursion levels = 8 ViewStates
```

The heavy TSR/Lumen history resources are not necessarily all fully materialized at the exact moment `ViewState.Allocate()` is called, but once deeper levels have actually rendered they can retain temporal renderer state beyond the depth currently required.

The problem is therefore resource lifetime, not merely object count.

### 3.2 Portal output targets are full-view sized

The FullFidelity path derives `TargetSize` from the constrained player view rectangle rather than the portal's projected screen rectangle.

`PrimaryResolutionFraction` lowers the internal primary rendering resolution, but it does not reduce the final portal color/depth target dimensions.

This is a known optimization opportunity but is **not** part of P1 because changing target extents would require projection, UV, depth, extraction and composition changes.

### 3.3 Recursion resources grow but are not reclaimed when depth decreases

The existing target-management behavior grows arrays to satisfy a requested recursion depth and resizes the targets already present, but does not shrink the resource set when the configured recursion depth is reduced.

Example:

```text
run at depth 4
→ create/use levels L0..L3

change to depth 1
→ only L0 is needed
→ L1..L3 can remain allocated
```

This is the highest-priority P1 issue.

### 3.4 Every submitted recursion layer is a real UE scene render

Each layer currently creates a Lit `FSceneViewFamily`, uses temporal AA / TSR, real-time update and FullFidelity post-process state including Lumen GI and Lumen reflections.

This is intentionally expensive for correctness. P1 will reduce unnecessary submissions but will not lower FullFidelity quality settings.

### 3.5 Main composition already has a bounded-pass implementation but it is not the default

`portal.BoundedMainPassScissor` exists and can restrict several main-view portal passes to a conservative projected portal rectangle.

P1 will validate this feature as a performance option, but it must not be confused with persistent VRAM reduction.

## 4. P1 scope

P1 contains three implementation blocks:

```text
P1A — Resource Lifetime & Budget
P1B — Recursion Screen-Coverage Cutoff
P1C — Bounded Main-Pass Validation
```

P1 does **not** redesign the renderer.

## 5. Explicit non-goals

The following items are deliberately deferred to Portal Performance P2 or later:

```text
portal-sized / cropped render targets
TSR-primary-sized final depth target
projection-matrix cropping for portal sub-rects
UV remapping caused by cropped render targets
changing projective aperture math
changing transported depth semantics
lower-quality Lumen for recursion views
turning off Lumen reflections for deeper views
shadow-quality tiers by recursion level
portal-specific frustum construction
room / zone / PVS visibility graphs
renderer-integrated stencil recursion architecture
Nanite-specific portal visibility work
multi-pair portal scaling
physics traversal changes
partial-crossing collision architecture
```

If implementation of P1 appears to require one of these changes, stop that subtask and treat it as a scope violation instead of silently expanding the work.

## 6. P1A — Resource Lifetime & Budget

### 6.1 P1A-1: Baseline instrumentation

Before changing allocation behavior, extend the existing FullFidelity diagnostic/report path so that a dump can show the resource state that matters for this phase.

Required fields:

```text
RequestedRecursionDepth
VisibleDepth per endpoint
ActiveViewStateCount
AllocatedColorTargetCount
AllocatedDepthTargetCount
FinalScratchSize
ColorTargetSize(s)
DepthTargetSize(s)
PrimaryResolutionFraction
SubmittedLayerCount for the current/last frame
```

Where practical, distinguish:

```text
allocated
active
visible
submitted
```

These concepts must not be collapsed into one number.

The report must allow a developer to prove that resources are actually reclaimed rather than merely unused.

### 6.2 Required baseline measurements

Capture the pre-optimization state at:

```text
RecursionDepth = 1
RecursionDepth = 2
RecursionDepth = 3
RecursionDepth = 4
```

Also capture this transition in one PIE session:

```text
1 → 4 → 1
```

Record at minimum:

```text
ViewState count
Color RT count
Depth RT count
RT resolution
visible depth
submitted layer count
GPU frame time if available
VRAM / GPU memory observation if available
```

The exact external VRAM number may vary by driver and editor state, so internal resource counts are the authoritative P1 correctness measurement.

### 6.3 P1A-2: Demand-driven ViewState allocation

Change layer state ownership so that recursion ViewStates are allocated only for levels required by the current recursion budget.

Target logical behavior:

```text
Depth 1 → Blue L0 + Orange L0 = 2 active ViewStates
Depth 2 → Blue L0/L1 + Orange L0/L1 = 4
Depth 3 → 6
Depth 4 → 8
```

Constraints:

```text
no change to TSR method
no ViewState sharing between endpoint/level identities
no sharing histories across different recursion transforms
no fallback to one global ViewState
no weakening of camera-cut/history invalidation semantics
```

Each `(Endpoint, Level)` remains a distinct temporal identity while it exists.

### 6.4 P1A-3: Reclaim resources when RecursionDepth decreases

When the effective requested depth is reduced, levels outside the budget must be retired deliberately.

For each retired layer, clean up all state owned specifically by that layer, including where applicable:

```text
ViewState
Depth target
published request/sample state
history-valid flags
last-frame visibility state
camera-cut/history bookkeeping
recursive composition extension relationship
```

The cleanup must make a later re-expansion safe:

```text
Depth 4 → Depth 1 → Depth 4
```

The newly recreated deeper layers must begin with valid camera-cut/history semantics and must not consume stale temporal history from the previous generation.

### 6.5 P1A-4: Shrink portal color targets

Update `AInteriorPortal::EnsureTargets()` ownership semantics so the color-target array reflects the required recursion depth rather than the maximum depth ever requested.

Expected behavior:

```text
current target count < requested depth
→ allocate missing targets

current target count > requested depth
→ detach / release / remove excess targets

resolution changed
→ resize only the targets that remain required
```

The implementation must not leave a dynamic material sampling a removed target.

### 6.6 P1A-5: Depth target lifetime

Apply the same principle to FullFidelity per-layer depth targets.

A level that is no longer part of the recursion budget must not keep a full-resolution depth target alive indefinitely.

This task is about **lifetime only**. It must not change depth resolution or transported-depth interpretation.

### 6.7 P1A-6: Scratch lifetime

Keep the existing shared final scratch model for P1.

Only verify that:

```text
there is one intended shared scratch resource
its lifetime follows the producer lifecycle
its size follows the current full-fidelity target size
it is released on FullFidelity Stop
```

Do not redesign scratch reuse in P1.

## 7. P1A acceptance gate

P1A is not complete until all of the following pass:

```text
[ ] SlayTheSpireDemoEditor Win64 Development build PASS
[ ] normal PIE auto-start PASS; no manual Start command required
[ ] single visible portal PASS
[ ] both endpoints visible PASS
[ ] RecursionDepth 1 PASS
[ ] RecursionDepth 2 PASS
[ ] RecursionDepth 3 PASS
[ ] RecursionDepth 4 PASS
[ ] runtime 1 → 4 transition PASS
[ ] runtime 4 → 1 transition PASS
[ ] runtime 4 → 1 → 4 transition PASS
[ ] oblique / grazing-angle view PASS
[ ] rapid look-away / look-back PASS
[ ] no one-frame spiral ownership hole
[ ] no stale deep-recursion frame after shrink/re-expand
[ ] no invalid render-target sampling
[ ] no new renderer warnings/errors
[ ] diagnostic dump reports correct active resource counts
```

Expected count invariants after stabilization:

```text
Depth 1 → 2 active endpoint-level ViewStates
Depth 2 → 4
Depth 3 → 6
Depth 4 → 8
```

If implementation architecture requires a small persistent shell object per level, that is acceptable. The important condition is that heavyweight renderer/RT resources for inactive levels are not retained.

## 8. P1B — Recursion Screen-Coverage Cutoff

P1B begins only after P1A is sealed.

### 8.1 Goal

Avoid submitting deeper complete scene renders when the recursively projected portal has become too small to justify another FullFidelity layer.

### 8.2 New budget concept

Introduce a configurable minimum projected portal coverage threshold, for example:

```text
portal.MinRecursionScreenCoverage
```

The exact default must be determined from measurements rather than permanently hard-coded from the planning document.

An initial test value such as `0.25%` of the parent view area may be used for experiments.

### 8.3 Requested depth versus visible/submitted depth

The renderer must preserve the distinction:

```text
RequestedRecursionDepth = user/system maximum
VisibleDepth = geometrically visible useful depth
SubmittedDepth = layers actually rendered this frame
```

Example:

```text
RequestedRecursionDepth = 4
projected portal becomes tiny at L2
SubmittedDepth = 2
```

The cutoff must not rewrite the configured maximum recursion depth.

### 8.4 Cutoff constraints

The cutoff must not introduce:

```text
flickering depth count around the threshold
one-frame spiral flashes
stale deep-level publication
rapid allocate/free thrashing
history identity confusion
```

Use hysteresis or a conservative policy if needed.

Do not optimize allocation so aggressively that a portal hovering around the threshold reallocates renderer histories every frame.

## 9. P1B acceptance gate

Required validation:

```text
[ ] Build PASS
[ ] no visual difference while portal coverage is above threshold
[ ] deeper recursion stops when projected coverage is below threshold
[ ] no visible threshold flicker during slow camera motion
[ ] no spiral flash during rapid threshold crossings
[ ] SubmittedLayerCount decreases as expected
[ ] requested recursion configuration remains unchanged
[ ] performance measurement shows reduced scene submissions in qualifying views
```

## 10. P1C — Bounded Main-Pass Validation

P1C validates the existing conservative main-view scissor path.

Compare:

```text
portal.BoundedMainPassScissor=0
portal.BoundedMainPassScissor=1
```

Keep all other renderer settings equal.

### 10.1 Correctness validation

Test at minimum:

```text
front-facing portal
large portal near screen edges
grazing angle
portal partially off-screen
camera rapidly entering/leaving portal bounds
both portals visible
recursive view present
foreground occluder intersecting portal boundary
```

### 10.2 Performance validation

Record GPU timing when practical.

This feature primarily targets:

```text
raster work
pixel shader work
bandwidth
main composition cost
```

It must not be reported as a major persistent-VRAM optimization unless measurements show a real memory change.

### 10.3 Production decision

Only change the production default to enabled if:

```text
all correctness tests PASS
and
measurement shows a repeatable benefit
```

Otherwise keep it an opt-in diagnostic/performance feature and record the reason.

## 11. Measurement matrix

Use the same representative scene and camera conditions when comparing before/after states.

Minimum matrix:

| Case | Recursion | Portal visibility | Purpose |
|---|---:|---|---|
| A | 1 | one endpoint dominant | minimum FullFidelity cost |
| B | 2 | both visible | normal target case |
| C | 4 | both visible / facing recursion | stress case |
| D | 4 → 1 | same PIE session | resource reclamation |
| E | 1 → 4 → 1 | same PIE session | reallocation correctness |
| F | 4 configured, tiny projected recursion | P1B cutoff | submission reduction |

For each case record, where available:

```text
active ViewStates
color RT count
color RT dimensions
active depth RT count
depth RT dimensions
scratch dimensions
submitted recursion layers
GPU frame time
observed VRAM
visual result
warnings/errors
```

## 12. Commit strategy

Keep implementation commits independently reviewable.

Recommended sequence:

```text
docs(portal): add performance and VRAM P1 execution plan

portal: report full-fidelity recursion resource budget
portal: allocate full-fidelity view states on demand
portal: reclaim inactive recursion render targets
portal: harden recursion shrink and re-expand lifecycle
portal: add recursion screen-coverage budget
portal: validate bounded main-pass production policy

docs(portal): seal performance and VRAM P1 gate
```

Do not combine P1A resource lifetime work with P1B cutoff logic in the same first implementation commit.

## 13. Failure and rollback policy

The functional FullFidelity baseline is frozen at:

```text
6d514466639858c1fd903d0f9abf96d5aad02077
```

If a P1 optimization causes a correctness regression that cannot be fixed locally without changing projection/composition architecture:

```text
revert the optimization
retain the measurement/instrumentation if safe
record the failure mode
move the architectural change to P2
```

Do not sacrifice the accepted visual correctness merely to lower the resource count.

Priority order is:

```text
1. correctness
2. deterministic lifecycle
3. resource reclamation
4. performance
5. architectural elegance
```

## 14. P1 completion criteria

Portal Performance / VRAM P1 can be marked:

```text
COMPLETE / VALIDATED / SEALED
```

only when:

```text
[ ] P1A resource baseline is recorded
[ ] inactive recursion resources are reclaimed
[ ] depth shrink/re-expand lifecycle is stable
[ ] P1A regression matrix passes
[ ] P1B coverage cutoff is implemented and validated, or explicitly rejected with measurements
[ ] P1C bounded scissor is validated and its production policy is recorded
[ ] before/after resource counts are documented
[ ] before/after GPU/VRAM observations are documented where available
[ ] FullFidelity visual behavior remains accepted
[ ] no production console-command dependency is reintroduced
```

## 15. Work authorized after P1

Once P1 is sealed, the next authorized development phase is:

```text
Portal Physics P1
```

Proposed order:

```text
Physics P1A — rigid-body traversal primitive
Physics P1B — player / Character traversal
Physics P1C — partial crossing visual/proxy architecture
Physics P1D — complex physical interaction and stress cases
```

Portal Performance P2 is intentionally deferred until there is measured evidence that P1 is insufficient.

Potential P2 topics include:

```text
portal-projected cropped render targets
cropped projection / UV remapping
lower-resolution depth ownership
recursion quality tiers
portal-clipped frusta
room / region visibility
more renderer-native portal integration
```

## 16. Immediate next task

The first implementation task after this document is accepted is:

```text
P1A-1 — FullFidelity resource baseline and Dump instrumentation
```

No allocation policy should be changed until the baseline report can prove the before/after resource state.