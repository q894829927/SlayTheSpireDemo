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

This plan continues development on the same branch. No new implementation branch is required for this phase.

---

## 1. Purpose

The FullFidelity Portal renderer has completed its functional development gate. The renderer is now considered visually and functionally stable enough that the next task should not reopen projection/composition correctness work unless a regression proves it necessary.

The immediate next phase is a narrow **Portal Performance / VRAM P1** pass before broad Portal physics development.

The purpose of P1 is to:

```text
preserve accepted FullFidelity image correctness
preserve current projective aperture/depth behavior
preserve TSR and Lumen fidelity
remove unnecessary persistent recursion resources
stop retaining resources for inactive recursion levels
avoid submitting recursion that is too small to matter
measure resource ownership and performance before/after every change
```

The phase is deliberately conservative. It addresses resource lifetime and bounded work first. It does **not** redesign the renderer.

---

## 2. Current accepted production architecture

The accepted production path is:

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

The following facts are treated as the baseline contract:

- FullFidelity rendering owns the production remote-view path when enabled.
- `AInteriorPlayerController` bypasses legacy `PortalSystem->RenderViews()` while FullFidelity owns rendering.
- Production `Start / Stop / Dump` call the native backend directly.
- Production startup no longer uses `GEngine::Exec(...)` to dispatch lifecycle console commands.
- Console commands remain manual diagnostic aliases only.
- FullFidelity startup failure propagates as a real `bool` failure instead of being reported as success unconditionally.

P1 must preserve this ownership model.

---

## 3. Current performance and VRAM problem statement

### 3.1 ViewState allocation is currently maximum-oriented

The current producer owns two portal endpoints and supports four recursion levels.

Conceptually the maximum state surface is:

```text
Blue   L0 L1 L2 L3
Orange L0 L1 L2 L3

2 endpoints × 4 levels = 8 ViewStates
```

At startup, ViewState objects are allocated for the complete endpoint × recursion matrix rather than only the recursion depth that is currently required.

`FSceneViewState` itself is not equivalent to immediately allocating every maximum-size TSR/Lumen texture. Heavy temporal resources are materialized as views actually render. The practical problem is therefore broader:

```text
high recursion level becomes active
→ temporal renderer state/history is established
→ RecursionDepth later decreases
→ inactive deep-level state has no explicit reclaim path
```

P1 must make resource ownership follow active recursion requirements.

### 3.2 FullFidelity targets are derived from the player view size

The current FullFidelity producer derives `TargetSize` from the constrained player viewport.

Conceptually:

```text
TargetSize ≈ PlayerRect
ExpectedPrimarySize = TargetSize × PrimaryResolutionFraction
```

This means `PrimaryResolutionFraction` lowers internal primary rendering resolution, but does not inherently shrink the final portal output target dimensions.

This is an important optimization opportunity, but portal-projected target allocation is **not part of P1** because changing target extents affects projection, extraction, UV mapping, depth reconstruction and recursive composition.

### 3.3 Recursion resources can grow and remain resident

The existing target management grows resources to satisfy requested recursion depth. Reducing recursion depth does not provide an equally strong shrink/reclaim contract.

Example:

```text
RecursionDepth = 4
→ L0/L1/L2/L3 become active

RecursionDepth = 1
→ only L0 is required
→ L1/L2/L3 must no longer retain production resources
```

This is the highest-priority P1 issue.

### 3.4 Every submitted layer is a real Lit scene view

Each submitted recursion layer is a real UE scene render using the current FullFidelity path, including the accepted temporal and lighting behavior.

The cost model therefore remains approximately:

```text
main view
+ visible endpoint × visible recursion layer scene submissions
```

P1 should reduce unnecessary submissions but must not reduce visual fidelity by silently disabling FullFidelity features.

### 3.5 Main composition has an existing bounded-pass path

`portal.BoundedMainPassScissor` already exists as a bounded-work mechanism, but it is not the production default.

P1 will validate this existing path after resource lifetime is stable. It will not redesign the composition architecture.

---

## 4. P1 scope

P1 contains three implementation blocks:

```text
P1A — Resource Lifetime & Budget
P1B — Recursion Screen-Coverage Cutoff
P1C — Bounded Main-Pass Validation
```

Order is mandatory:

```text
P1A
 ↓
P1A Gate
 ↓
P1B
 ↓
P1B Gate
 ↓
P1C
 ↓
Performance / VRAM P1 Gate
```

Do not begin P1B or P1C while P1A is not stable.

---

# 5. P1A — Resource Lifetime & Budget

## 5.1 P1A-1 — Resource baseline and Dump instrumentation

### Goal

Before changing allocation behavior, establish a measurable baseline from the current renderer.

### Required diagnostics

`portal.DumpFullFidelityRenderer` or the backend report must expose enough information to answer at least:

```text
RequestedRecursionDepth
VisibleDepth per endpoint
SubmittedDepth per endpoint
Active/Allocated ViewState count
Allocated portal color-target count
Allocated depth-target count
Color-target dimensions
Depth-target dimensions
Shared scratch dimensions
last visible/submitted endpoint mask
```

Where practical, also report:

```text
per endpoint / per level allocated state
per endpoint / per level visible state
per endpoint / per level FramesSubmitted
history generation
camera-cut count
continuous-history frame count
```

### Baseline matrix

Collect baseline data for:

```text
Depth 1
Depth 2
Depth 3
Depth 4
Depth 4 → 1
Depth 1 → 4
```

Use the same map, window size, rendering settings and camera position when comparing numbers.

### Acceptance

P1A-1 passes when the report can prove what resources exist without relying on source inspection alone.

No resource-allocation behavior should change in this step unless required solely to expose correct diagnostics.

Suggested commit:

```text
portal: report full-fidelity recursion resource ownership
```

---

## 5.2 P1A-2 — Allocate ViewState on demand

### Goal

Stop treating all four recursion levels as active ownership at renderer startup.

### Required behavior

For requested depth `D`, active ViewState ownership should converge to:

```text
ActiveViewStates = ActiveEndpoints × D
```

For the normal linked two-endpoint case:

```text
Depth 1 → 2 ViewStates
Depth 2 → 4 ViewStates
Depth 3 → 6 ViewStates
Depth 4 → 8 ViewStates
```

### Rules

- Do not change ViewState identity every frame while the level remains active.
- Stable active levels must retain temporal continuity.
- Creating a newly required level must initialize it as a clean history generation.
- Recreated levels must not inherit stale TSR/Lumen state from a prior destroyed lifetime.
- ViewState allocation must happen before a view or extension references it.
- No null/dangling `SceneViewStateInterface` may reach the render path.

### Non-goals

Do not change:

```text
TSR
Lumen GI
Lumen reflections
projection matrix
portal clip plane
projective aperture math
composition shader math
```

Suggested commit:

```text
portal: allocate full-fidelity recursion view states on demand
```

---

## 5.3 P1A-3 — Reclaim inactive recursion ViewState and history

### Goal

When requested recursion depth decreases, higher recursion levels must release temporal renderer state instead of only becoming logically unused.

### Example contract

```text
Depth 4
Blue   L0 L1 L2 L3
Orange L0 L1 L2 L3

Depth 1
Blue   L0
Orange L0

L1/L2/L3 ownership must be destroyed/released.
```

### Required cleanup

For every level that becomes inactive:

```text
detach/revoke publication state
clear recursive composition references where applicable
release/destroy ViewState ownership
reset history metadata
reset target references associated only with that level
invalidate stale completed render requests
```

The exact implementation may differ, but no render-thread or game-thread object may retain a pointer/reference to a released ViewState.

### Re-expansion contract

The following sequence is mandatory:

```text
Depth 4
→ render stable
→ Depth 1
→ deep state reclaimed
→ render stable
→ Depth 4
→ deep state recreated cleanly
→ render stable
```

Re-expansion must not produce:

```text
stale remote image
one-frame spiral flash
invalid history reuse
assert/crash
render-thread use-after-free
```

Suggested commit:

```text
portal: reclaim inactive full-fidelity recursion histories
```

---

## 5.4 P1A-4 — Shrink portal color-target ownership

### Goal

Fix target arrays that grow to a maximum depth and remain there after depth decreases.

### Required behavior

A portal endpoint should own color targets only for currently required recursion levels, subject to any explicitly documented safety buffer.

Normal target count should converge to:

```text
ColorTargetsPerEndpoint = RequestedRecursionDepth
```

When depth decreases:

```text
Depth 4 → 1
```

extra targets must be removed/released rather than merely left unused.

### Requirements

- Existing active targets must not be unnecessarily recreated when depth is unchanged.
- Resolution changes still resize currently active targets correctly.
- Removed targets must no longer be reachable by portal material publication state.
- Re-expanding depth must recreate targets correctly.

Suggested commit:

```text
portal: shrink inactive recursion color targets
```

---

## 5.5 P1A-5 — Depth-target lifetime

### Goal

Depth-target ownership must follow active recursion state in the same way as ViewState and color targets.

### Required behavior

For a normal linked pair:

```text
Depth 1 → 2 active depth targets
Depth 2 → 4 active depth targets
Depth 3 → 6 active depth targets
Depth 4 → 8 active depth targets
```

If implementation details justify temporary render-thread retirement rather than immediate destruction, diagnostics must distinguish:

```text
logically inactive
pending release
actively owned
```

### Important limitation

P1 does **not** change the depth-target resolution model. Changing depth target dimensions to TSR primary resolution belongs to P2.

Suggested commit:

```text
portal: reclaim inactive recursion depth targets
```

---

## 5.6 P1A-6 — Shared scratch lifecycle audit

### Goal

Verify that the shared FullFidelity scratch target is genuinely shared and has bounded lifetime.

### Required checks

- only one expected production scratch allocation exists for the producer;
- viewport-size changes resize rather than leak generations;
- stop/restart releases and recreates it correctly;
- FullFidelity renderer shutdown does not leave rooted transient resources behind;
- no legacy renderer scratch target remains alive through accidental dual-path execution.

This task is primarily an ownership audit. Do not split scratch per recursion level.

---

# 6. P1A Gate

P1A is complete only after all resource lifetime work passes the following matrix.

## Build

```text
SlayTheSpireDemoEditor Win64 Development — PASS
```

## Functional visual matrix

```text
single visible portal                         PASS
dual visible portals                         PASS
RecursionDepth = 1                           PASS
RecursionDepth = 2                           PASS
RecursionDepth = 3                           PASS
RecursionDepth = 4                           PASS
Depth 4 → 1                                  PASS
Depth 1 → 4                                  PASS
oblique/grazing view                         PASS
fast look-away / look-back                   PASS
no stale frame                               PASS
no unexpected recursion spiral flash         PASS
no renderer crash/assert                     PASS
```

## Ownership invariants

For two active endpoints:

```text
Depth 1 → expected active deep-resource levels = 2
Depth 2 → expected active deep-resource levels = 4
Depth 3 → expected active deep-resource levels = 6
Depth 4 → expected active deep-resource levels = 8
```

After:

```text
4 → 1
```

Dump must prove that inactive L1-L3 ownership has been reclaimed.

After:

```text
1 → 4
```

Dump must prove that L1-L3 were recreated as a new clean lifetime.

## Regression rule

If reclaiming resources causes temporal instability, do **not** hide the issue with arbitrary delays or permanent retention. Identify which reference or history dependency actually requires lifetime extension and document it explicitly.

---

# 7. P1B — Recursion Screen-Coverage Cutoff

P1B begins only after P1A Gate passes.

## 7.1 Goal

Avoid submitting expensive full scene views for recursion levels whose projected portal area is too small to provide meaningful visual value.

This reduces GPU scene-render count. It is not primarily a persistent-VRAM optimization.

## 7.2 Requested depth vs effective depth

Keep two separate concepts:

```text
RequestedRecursionDepth
Effective/VisibleRecursionDepth
```

Example:

```text
RequestedRecursionDepth = 4
portal projection becomes tiny after L1
EffectiveRecursionDepth = 2
```

Do not silently rewrite the configured requested depth.

## 7.3 Coverage metric

Use the existing projected portal bounds generated by the accepted portal geometry pipeline where possible.

A suitable metric is normalized conservative projected area:

```text
Coverage = ProjectedPortalPixelArea / MainViewPixelArea
```

Provide a tunable threshold, for example:

```text
portal.MinRecursionScreenCoverage
```

Initial validation values may include:

```text
0      = disabled
0.001  = 0.10%
0.0025 = 0.25%
0.005  = 0.50%
```

The final production value must be selected by measurement and visual validation, not assumed from the initial suggestion.

## 7.4 Stop rule

Before constructing/submitting the next deeper request:

```text
if next portal projection is invalid
    stop recursion

if projected coverage < threshold
    stop recursion
```

The deepest actually submitted level must still terminate recursion visually according to the accepted recursion-limit behavior.

## 7.5 Temporal stability

Coverage cutoff must not chatter frame-to-frame around the threshold.

If necessary, add a small hysteresis rule such as separate enter/exit thresholds. Do not add a large arbitrary time cooldown.

## 7.6 Diagnostics

Dump/report should include:

```text
RequestedDepth
EffectiveDepth per endpoint
cutoff reason
last projected coverage per level
number of scene submissions
```

Suggested commit:

```text
portal: bound recursion by projected screen coverage
```

---

# 8. P1B Gate

Validate:

```text
threshold disabled reproduces P1A behavior
large portal still reaches requested depth
distant/tiny portal stops earlier
oblique portal remains stable
camera movement across threshold does not flicker
look-away/look-back remains stable
recursion terminator remains correct
no stale child publication survives a cutoff
```

Record scene submissions per frame for representative cases.

Expected direction:

```text
small portal
→ fewer recursive scene submissions
→ lower GPU cost
```

Do not claim a specific percentage reduction until measured.

---

# 9. P1C — Bounded Main-Pass Validation

P1C begins only after P1B Gate passes.

## 9.1 Goal

Validate the already implemented:

```text
portal.BoundedMainPassScissor
```

as a production-safe way to reduce raster work outside the conservative projected portal rectangle.

## 9.2 Test modes

Compare:

```text
portal.BoundedMainPassScissor=0
portal.BoundedMainPassScissor=1
```

Use identical camera positions and recursion settings.

## 9.3 Visual requirements

With bounded pass enabled there must be no:

```text
edge clipping
missing portal pixels
portal border holes
depth mismatch at aperture boundary
grazing-angle truncation
camera-jitter leakage
one-pixel temporal seams
```

Validate with the existing padding setting and specifically test:

```text
front-on portal
oblique portal
near screen edge
partially off-screen portal
rapid camera motion
TSR jitter
```

## 9.4 Performance interpretation

Bounded scissor primarily targets:

```text
pixel/raster work
shader invocations
bandwidth
main composition GPU time
```

Do not count it as a major persistent-VRAM reduction unless measurement demonstrates one.

## 9.5 Production-default decision

Only after the full visual matrix passes may P1 change the production default to bounded scissor enabled.

If any correctness issue remains, keep the default disabled and record the blocker rather than forcing the optimization.

Suggested commit if validated:

```text
portal: enable bounded full-fidelity main composition
```

---

# 10. Performance measurement protocol

Optimization claims must use repeatable measurements.

## 10.1 Fixed conditions

Keep constant where possible:

```text
same map
same viewport resolution
same graphics settings
same renderer backend
same portal transforms
same camera transforms
same RecursionDepth
same PrimaryResolutionFraction
```

## 10.2 Required scenarios

At minimum measure:

```text
A. no portal visible
B. one portal visible, Depth 1
C. two portals visible, Depth 1
D. two portals visible, Depth 2
E. two portals visible, Depth 4
F. Depth 4 → 1 after deep histories have been exercised
G. small/distant recursive portal with P1B cutoff
```

## 10.3 Record

Record, where available:

```text
GPU frame time
portal-related GPU timing
scene-view submissions per frame
active ViewState count
color-target count and dimensions
depth-target count and dimensions
scratch dimensions
reported VRAM / render-target memory
```

Use Unreal Insights / GPU Visualizer / RHI memory diagnostics as appropriate, but keep the resource Dump as the authoritative ownership-level report for resources directly controlled by the Portal system.

---

# 11. Explicit P1 non-goals

The following are intentionally deferred.

## 11.1 Portal-sized render targets

Do not crop the FullFidelity output RT to the portal projected rectangle in P1.

That work affects:

```text
projection
view rect
screen-to-target mapping
extraction
projective composition UVs
depth reconstruction
recursive publication
TSR history dimensions
```

It belongs to Performance P2.

## 11.2 TSR-primary-resolution depth targets

Do not independently shrink the depth target to primary resolution in P1.

Color/depth coordinate-space assumptions must be redesigned and validated together.

## 11.3 Reduced FullFidelity quality tiers

Do not disable or lower:

```text
Lumen GI
Lumen reflections
TSR
shadow quality
post-process fidelity
```

inside deeper recursion as part of P1.

A future performance tier may intentionally trade quality for cost, but it requires its own visual contract.

## 11.4 Portal-specific frustum/PVS architecture

Do not introduce in P1:

```text
portal-edge frustum replacement
room/sector visibility graph
PVS/areaportal-style scene partitioning
custom primitive render lists
renderer-level stencil recursion architecture
```

These are renderer-architecture projects, not cleanup tasks.

## 11.5 Physics/traversal expansion

P1 must not expand into new Portal gameplay physics.

Portal Physics P1 starts only after this performance phase is sealed.

---

# 12. Risk register

## Risk A — ViewState release invalidates render-thread references

Mitigation:

```text
revoke publication first
remove extensions/references
wait for normal engine ownership boundary where required
then destroy state
```

Do not perform unsafe raw destruction merely to make a counter decrease.

## Risk B — Re-expansion produces stale history

Mitigation:

```text
new lifetime → new history generation
camera cut / history invalidation as required
clear completed requests
```

## Risk C — Target shrink leaves portal materials referencing removed textures

Mitigation:

Ensure publication is switched to a valid active level or unlinked state before target destruction.

## Risk D — Coverage cutoff causes recursion popping

Mitigation:

Use conservative projected bounds and, if measurement proves necessary, small hysteresis around the cutoff.

## Risk E — Bounded scissor clips jittered edges

Mitigation:

Use conservative bounds plus validated padding. Keep the optimization disabled by default if correctness cannot be proven.

---

# 13. Rollback policy

Every logical optimization should be committed independently.

Preferred sequence:

```text
1. diagnostics/baseline
2. ViewState on-demand allocation
3. inactive ViewState/history reclaim
4. color-target shrink
5. depth-target reclaim
6. scratch ownership cleanup if needed
7. screen-coverage cutoff
8. bounded-main-pass production decision
9. final validation record
```

If a step causes a regression:

```text
revert only that optimization
preserve earlier validated steps
record the failed assumption
```

Do not bundle unrelated renderer changes into one commit.

---

# 14. Completion criteria for Portal Performance / VRAM P1

P1 can be marked:

```text
COMPLETE / VALIDATED / SEALED
```

only when all of the following are true:

```text
[ ] P1A-1 resource baseline/report complete
[ ] P1A-2 ViewState allocation follows required depth
[ ] P1A-3 inactive ViewState/history is reclaimed safely
[ ] P1A-4 inactive color RTs are reclaimed
[ ] P1A-5 inactive depth RTs are reclaimed
[ ] P1A-6 shared scratch lifetime is verified
[ ] Depth 4 → 1 → 4 passes without stale history or flash
[ ] P1A full visual regression matrix passes
[ ] P1B projected-coverage cutoff implemented and stable
[ ] P1B reports effective recursion depth/submissions
[ ] P1B visual regression matrix passes
[ ] P1C bounded scissor has been measured and visually validated
[ ] final Build PASS
[ ] final PIE smoke PASS
[ ] final resource Dump captured
[ ] before/after performance measurements recorded
[ ] no FullFidelity functional regression
```

No percentage VRAM or GPU target is declared up front. The first objective is eliminating objectively unnecessary ownership. Performance targets may be set only after the P1A-1 baseline exists.

---

# 15. Phase handoff after P1

After Performance / VRAM P1 is sealed, development proceeds to:

```text
Portal Physics P1
```

Planned physics order:

```text
Physics P1A — rigid-body traversal primitive
Physics P1B — player/character traversal hardening
Physics P1C — partial crossing / visual proxy / clipping
Physics P1D — complex contact, held objects and edge cases
```

Deeper renderer optimization remains a separate later phase:

```text
Portal Performance P2
```

Potential P2 subjects:

```text
portal-projected render-target sizing
TSR-primary depth storage strategy
recursion quality tiers
Lumen/reflection/shadow policy per recursion level
portal-clipped frustum
room/sector visibility
renderer-integrated portal visibility architecture
```

---

# 16. Immediate next authorized task

The next implementation task is strictly:

```text
P1A-1 — FullFidelity resource baseline + Dump instrumentation
```

Implementation should begin by measuring the current state, not by changing resource allocation.

The first code change must make resource ownership observable enough that later P1 optimizations can be proven quantitatively.

Until P1A-1 baseline data exists, do not claim that any specific optimization reduces VRAM by a particular amount.
