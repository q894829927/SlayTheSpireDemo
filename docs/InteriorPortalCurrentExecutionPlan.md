# Interior Portal — Current Execution Plan

Date: **2026-09-13**

Branch reviewed: **`portal/full-fidelity-p1`**

Review HEAD: **`cd0097b6d25a538810a2658d40705601504200c5`**

Status:

```text
CORE IMPLEMENTATION BASELINE ESTABLISHED /
CORE FIDELITY ACCEPTANCE IN PROGRESS /
P2 VISUAL EXPOSURE PARITY IS THE PRIMARY CURRENT BLOCKER /
FULL PHYSICS P8/P9 NOT STARTED
```

This document is the **active execution/order authority** for the current portal branch. It does not replace the durable architecture, scope and final Definition of Done in [`InteriorPortalFullFidelityImplementationPlan.md`](InteriorPortalFullFidelityImplementationPlan.md). It replaces the older assumption that the project should still execute P0→P1→P2→P3 sequentially from a prototype baseline.

Current defect/evidence authorities:

- [`InteriorPortalObservedIssues.md`](InteriorPortalObservedIssues.md)
- [`InteriorPortalPlayerTraversal.md`](InteriorPortalPlayerTraversal.md)
- [`Validation.md`](Validation.md)
- [`CODEX_GOAL_CHECKPOINT.md`](CODEX_GOAL_CHECKPOINT.md) for resumable state only

---

## 1. Why the implementation order is being changed

The current branch is no longer the original SceneCapture + teleport prototype. Several later-stage mechanisms already exist and have focused evidence:

```text
logical portal frame separated from visual surface
native SceneCapture clip-plane production candidate
fallback guarded oblique projection
explicit player crossing state machine
CharacterMovement submove aperture constraint
same-submove player transfer before later floor queries
quaternion-owned portal camera/input state
runtime traveller registration/discovery
sphere/capsule/box geometry-aware conservative fit
rigid-body position/rotation/linear/angular velocity transfer
held-object angular velocity preservation
player partial-crossing presentation proxies
portal-aware flashlight proxy/light-function path
PortalQuery line trace + sphere sweep
focused Automation and actual-map traversal regressions
```

Continuing to treat P3/P4/P5/P7/P10 as untouched future stages would cause duplicate work and risk regressing mechanisms that are already stronger than the original plan baseline.

The correct strategy is now:

```text
freeze the working core
→ close remaining Core Fidelity blockers
→ seal Core Portal Fidelity
→ only then begin experimental/full dual-space physics
```

---

## 2. Current implementation inventory

### 2.1 Rendering / portal surface

**Implemented candidate:**

- `LogicalPortalFrame` is the actor transform used by traversal, queries and clipping;
- `SurfaceVisualBias` affects only the cosmetic surface;
- global clip-plane support is enabled;
- native `SceneCaptureComponent2D` clip plane is the default production candidate;
- guarded oblique projection remains as an explicit fallback/diagnostic path;
- the second competing custom near-clip override is disabled;
- recursive HDR render targets remain finite and deepest-first;
- temporal SceneCapture history is enabled by default;
- Lumen capture cache resolution is configurable.

**Open:**

- direct-view vs portal-view exposure parity;
- final temporal policy under motion/recursion;
- near/grazing/plane-crossing/recursion/replacement visual seal;
- RenderTarget/SceneCapture GPU and VRAM budget.

### 2.2 Player traversal

**Implemented candidate:**

- crossing states: `Outside`, `ApproachingEntry`, `IntersectingAperture`, `Transferred`, `ClearingExit`;
- every CharacterMovement submove is constrained before the ordinary world sweep;
- capsule aperture interval is continuous and conservative;
- active-passage recovery permits non-worsening outward/centerward motion instead of wall-locking;
- transfer occurs during the movement submove before later destination floor queries;
- one transfer per frame protection exists;
- exit clearance/hysteresis prevents immediate ping-pong reversal;
- last safe position/recovery data exists;
- portal replacement/clear remains blocked while traversal is busy.

**Evidence already present:**

- formerly failing large-timestep traversal reproduced and repaired;
- 3 Hz / 260 cm/s repeated crossing regression passed;
- 30 Hz repeated crossing and protected reversal passed;
- 60 Hz / 20 cm/s slow-entry/pause/rim-retreat matrix passed;
- corrected footprint recovery probe passed.

**Open:**

- manual arbitrary wall/floor/ceiling visual/camera matrix;
- jump-through + mouse-look matrix;
- extreme edge/corner acceptance;
- packaged-build confirmation.

### 2.3 Camera/input continuity

**Implemented candidate:**

- active camera orientation is quaternion-owned;
- transfer composes the existing active portal orientation;
- mouse yaw/pitch compose in the active rolled camera basis;
- horizon recovery waits until physical clearance;
- character capsule remains upright/world-gravity based.

**Open:**

- manual arbitrary orientation acceptance;
- confirm there is no visible roll snap, input inversion or camera discontinuity at transfer/clearance boundaries.

### 2.4 Physics travellers

**Implemented candidate:**

- explicit runtime register/unregister API;
- stable tagged-traveller discovery;
- invalid traveller cleanup;
- sphere/capsule/box support projection into portal axes;
- rigid-body position, orientation, linear velocity and angular velocity mapping;
- held-object angular velocity mapping;
- prototype pairwise support-wall collision suppression;
- partial-crossing visual proxy path for supported StaticMesh bodies.

**Open:**

- high-speed/CCD Chaos aperture gate;
- broader shape matrix and unsupported-category rejection contract;
- multi-material/general production visual slicing for all supported traveller assets;
- remote-half physical contact does not exist yet.

### 2.5 Portal-aware queries

**Implemented candidate:**

- bounded recursive `LineTrace`;
- bounded recursive `SphereSweep`;
- analytic portal-plane crossing;
- support-wall/aperture arbitration;
- unrelated nearer obstacle precedence;
- mapped remainder segment through the paired frame;
- current reuse by interaction and Physics Handle related targeting paths.

**Open:**

- segmented diagnostic/debug visualization;
- explicit projectile/beam/exploration-weapon consumers;
- any additional sweep shapes actually required by supported gameplay.

### 2.6 Player/first-person presentation

**Implemented candidate:**

- tagged `PortalTravellerVisual` meshes;
- per-material-slot source/remote slicing;
- remote no-collision player visual proxies;
- mapped flashlight spot-light proxies;
- aperture light-function restriction;
- temporary lighting-channel allocation/restoration;
- source-light suppression during emitter crossing.

**Open:**

- manual near/grazing partial-body visual acceptance;
- held-item seam quality;
- flashlight visual/illumination continuity in arbitrary directions;
- stale proxy/light cleanup stress matrix.

---

## 3. New execution strategy

The remaining work is split into two independent acceptance programs.

```text
TRACK A — Core Portal Fidelity
    finish and seal the production-useful portal system

TRACK B — Full Physics Fidelity
    add remote-half physical contact and supported constraints only after Track A is sealed
```

Do **not** mix P8/P9 experimentation into the Core closure path unless a Core requirement proves impossible without it.

---

# TRACK A — CORE PORTAL FIDELITY

## A0 — Stabilize current branch baseline

### Goal

Treat the current branch as a working integrated system rather than a collection of experimental stages.

### Required work

- keep the current logical-frame contract unchanged;
- retain native clip plane as the default candidate while P1 acceptance is open;
- retain guarded oblique fallback only for A/B/rollback;
- retain the submove traversal state machine and quaternion camera path;
- retain PortalQuery as the single reusable cross-portal query layer;
- retain traveller registry and geometry-aware fit architecture;
- ensure every new change preserves the currently passing focused regressions.

### Do not redo

Do not replace the following merely because the original master plan listed them as future work:

```text
LogicalPortalFrame separation
native clip candidate
FInteriorPortalCameraState
UInteriorPortalMovementComponent
crossing state machine
CapsuleApertureInterval
traveller registry/discovery
sphere/capsule/box support fit
PortalQuery line/sphere implementation
player presentation component
flashlight proxy/light-function path
```

---

## A1 — P2 exposure-domain closure — CURRENT HIGHEST PRIORITY

### Problem

The destination can still appear at a materially different brightness through the portal than when viewed directly. Existing evidence shows:

```text
raw SceneColor HDR portal path -> too dark
EyeAdaptationInverse correction -> too bright / over-corrected
forced no-eye-adaptation + PreExposure 1 -> broad convergence but unacceptable scene exposure
```

This indicates an exposure-domain problem, not a scene-specific scalar tuning problem.

### Required implementation direction

Establish an explicit capture/display color-domain contract:

```text
SceneCapture scene color
→ identify capture PreExposure actually applied to that capture
→ normalize capture sample into scene-linear radiance domain
→ store/sample RenderTarget without unintended sRGB conversion
→ portal surface emits normalized scene-linear value
→ player's ordinary exposure/local exposure/tonemap applies exactly once
```

### Required diagnostics

Record per endpoint / recursion layer:

```text
CapturePreExposure
player view PreExposure
RenderTarget format / force-linear-gamma state
capture source
portal material correction mode
recursion depth
```

Do not solve this with a constant brightness multiplier accepted from one room.

### Acceptance matrix

- bright room → dark room;
- dark room → bright room;
- static view;
- camera approaching portal;
- physical crossing;
- auto-exposure settling after crossing;
- recursion depth 1 and >=2;
- flashlight off/on.

Acceptance requires no obvious brightness step attributable to the portal surface itself.

---

## A2 — Close P1/P3/P7 visual acceptance as one integrated matrix

These systems are already coupled in the player's eye and should no longer be accepted separately from isolated screenshots.

### Matrix

For both portal directions, test:

```text
centered far
centered near
left edge
right edge
top edge
bottom edge
grazing left/right
camera crossing the plane
slow partial crossing
fast crossing
reverse before clear
wall→wall
wall→floor
floor→wall
floor→ceiling / ceiling→floor where placement policy allows
mouse-look during crossing
recursion depth >= 2
rapid clear/replacement outside active passage
flashlight off/on
held object present/absent
```

### Must observe

- no black support-wall leak;
- no giant diagonal triangle;
- no one-frame unclipped frame;
- no camera jump or roll snap;
- no duplicate/missing player or held-object half;
- no flashlight double-light/pop;
- no stale remote visual or remote light after clearance/reset.

A screenshot from a centered normal view is not sufficient evidence.

---

## A3 — Finish P4/P5/P6 Chaos rigid-body hardening

Player traversal is no longer the main collision risk. The remaining collision risk is the rigid-body path.

### Required work

1. Move rigid-body support bypass toward the same bounded aperture principle used by the player.
2. Add high-speed and CCD-oriented crossing tests.
3. Prove a rigid body cannot escape laterally through the support while its wall contact is suppressed.
4. Expand supported-shape acceptance for:
   - sphere;
   - capsule;
   - box;
   - conservative StaticMesh fallback;
   - unsupported/multi-body category rejection.
5. Verify ordinary and held traversal preserve:
   - orientation;
   - linear velocity;
   - angular velocity;
   - stable Physics Handle reacquisition.
6. Verify blocked exits fail safely without leaving constraints/ignore state behind.

### Core boundary

Do **not** add remote-half physical contact in A3. That belongs to Track B.

---

## A4 — Finish PortalQuery as production infrastructure

### Required work

- add optional segmented query diagnostics showing each source/mapped segment and chosen portal event;
- add exact hop-limit behavior tests;
- add a simple exploration projectile/beam consumer if the current exploration mode uses one;
- route any remaining supported interaction traces that should traverse portals through PortalQuery;
- keep portal placement behavior explicit: only traverse a portal for placement if the game design intentionally permits it.

### Acceptance

- nearest unrelated obstacle always wins;
- support wall inside aperture is analytically replaced by the portal event;
- outside aperture the wall remains solid;
- recursive query terminates deterministically;
- mapped hit point/normal/distance semantics are documented for consumers.

---

## A5 — Lifecycle and failure-state closure

### Required matrix

Test cleanup while each transient state is active:

```text
portal reset
portal replacement
endpoint destruction
traveller destruction
traveller unregister
held-object release
player death/despawn/map transition
PIE EndPlay
render-target resize
recursion-depth change
```

### Required postconditions

- no lingering support ignore;
- no stale free constraint;
- no stale proxy mesh;
- no stale player slice material state;
- no stale remote spotlight/light-channel mutation;
- no stale camera portal state;
- no stale traveller registry entry;
- no stale RenderTarget bound as a valid linked view after unlink.

Failure handling must restore topology first; cosmetic cleanup is secondary.

---

## A6 — Performance / temporal / memory gate

Core Fidelity is not sealed until the current SceneCapture architecture is measured.

### Required profiles

At minimum measure:

```text
recursion depth 1
recursion depth 2
recursion depth 3
ResolutionScale production value
1080p-equivalent viewport
portal not visible
one portal visible
portal facing portal
flashlight remote light active
```

Record:

- game-thread portal cost;
- render-thread/GPU SceneCapture cost;
- RenderTarget memory estimate/observed pool impact;
- Lumen/history cost;
- visible temporal ghosting/jitter/flicker.

### Decision gate

If SceneCapture cannot meet the visual/performance target after bounded corrections, choose explicitly between:

```text
A. accept/document the SceneCapture Core ceiling
or
B. authorize a heavier custom SceneView / RenderGraph / stencil-style renderer
```

Do not accumulate unbounded material workarounds without that decision.

---

## A7 — Core Portal Fidelity Seal

Core can be sealed only when all of the following are true:

```text
P1 clipping visual matrix PASS
P2 exposure parity PASS
P3 camera/input matrix PASS
P4 player aperture safety PASS
P4/P6 rigid-body high-speed safety PASS for supported categories
P5 traveller registration/shape matrix PASS
P6 ordinary + held transfer PASS
P7 player/held partial visual matrix PASS
P10 production PortalQuery coverage PASS
lifecycle cleanup matrix PASS
performance/temporal/VRAM gate ACCEPTED
packaged-build smoke PASS
```

Required status wording after this point:

```text
CORE PORTAL FIDELITY — SEALED
FULL PHYSICS FIDELITY — NOT YET SEALED
```

Do not claim “full Portal physics” at A7.

---

# TRACK B — FULL PHYSICS FIDELITY

Track B begins only after A7 unless a specifically authorized feasibility spike is isolated from production behavior.

## B0 — P8A dual-space contact feasibility spike

### Goal

Prove one narrow case before designing a general bridge:

```text
one dynamic box
partially through portal
remote half contacts one static wall
mapped response affects authoritative source body
```

### Questions to answer

- where can reliable Chaos contact data be observed without double-applying solver impulses?
- what contact point/normal/penetration data is available at the needed time?
- can the response be mapped back before visible divergence becomes large?
- can the remote representation remain collision-only without becoming a second authority?
- how is one contact uniquely correlated to one authoritative traveller?

No production claim is allowed until this spike is deterministic.

---

## B1 — Remote collision representation and ownership state

Introduce an explicit physical crossing state separate from visual proxies:

```text
SourceAuthoritative
DualSpaceContact
TransferPending
DestinationAuthoritative
Clearing
```

The remote collision proxy must never become an accidental second simulation authority.

Required invariants:

- exactly one authoritative rigid body;
- remote proxy has explicit mass/kinematic policy;
- source/remote filters prevent self-contact and duplicate support contact;
- cleanup is deterministic on reset/destruction;
- visual proxy ownership and collision proxy ownership are not conflated.

---

## B2 — Static-environment contact mapping

For static destination geometry, map:

```text
contact point
contact normal
normal impulse
friction/tangent contribution where supported
lever arm
```

back through the inverse portal transform and apply equivalent linear/angular response to the authoritative body.

Acceptance begins with low-speed single-contact cases before stacking/multiple contacts.

---

## B3 — Dynamic-vs-dynamic finite-mass contacts

A kinematic remote proxy is not physically sufficient for dynamic-vs-dynamic interaction.

Before claiming this stage, define finite-mass contact behavior using the two authoritative bodies' masses/inertias and prevent the kinematic proxy from injecting an infinite-mass response.

Required scenarios:

- moving remote half vs resting box;
- two moving boxes;
- oblique contact producing torque;
- frictional slide;
- repeated contact while the source center approaches the portal plane.

---

## B4 — Authority transfer through active contact

The center-plane transfer must not produce:

- duplicate impulse;
- lost impulse;
- one-frame free penetration;
- contact normal sign inversion;
- energy explosion;
- stale remote proxy contact after transfer.

The authority swap must be atomic from the portal system's point of view.

---

## B5 — P9 constraint policy

Do not promise arbitrary cross-portal constraints by default.

First define the supported policy:

```text
single rigid body — required
Physics Handle — required
simple two-body rigid constraint — optional targeted support
ragdoll / vehicle / cloth / GeometryCollection — deferred unless separately authorized
```

Only implement cross-portal constraint solving after B0-B4 are stable.

---

## B6 — Full Physics Fidelity Seal

Full Physics is sealed only after:

```text
remote-half static contact PASS
dynamic-vs-dynamic finite-mass contact PASS
torque/friction behavior ACCEPTED
authority swap under contact PASS
cleanup/reset/destruction PASS
supported constraint policy PASS
stress/performance matrix ACCEPTED
```

Final status may then say:

```text
CORE PORTAL FIDELITY — SEALED
FULL PHYSICS FIDELITY — SEALED FOR THE DOCUMENTED SUPPORT MATRIX
```

It must still not imply support for deferred solver categories.

---

## 4. Revised priority order from the current branch

The practical order is now:

```text
A0 preserve current integrated baseline
→ A1 solve P2 capture/pre-exposure parity
→ A2 integrated clipping/camera/presentation visual matrix
→ A3 rigid-body Chaos high-speed + shape/held acceptance
→ A4 finish PortalQuery consumers/diagnostics
→ A5 lifecycle cleanup matrix
→ A6 performance/temporal/VRAM gate
→ A7 CORE PORTAL FIDELITY SEAL

then

B0 P8A static-contact feasibility spike
→ B1 remote physical representation/ownership
→ B2 static contact impulse/torque mapping
→ B3 finite-mass dynamic contacts
→ B4 authority swap under contact
→ B5 supported cross-portal constraint policy
→ B6 FULL PHYSICS FIDELITY SEAL
```

This order intentionally moves exposure/visual acceptance ahead of new physics architecture because the current player/traversal/query foundation is already materially implemented and tested, while P2 remains a visible blocker to the Core seal.

---

## 5. Work explicitly deferred from the current critical path

Unless separately authorized, do not expand the current delivery with:

- multiplayer replication/prediction;
- portal-aware navmesh or AI perception;
- arbitrary world-light transport/GI transport;
- portal-aware audio/reverb;
- Niagara migration;
- vehicles;
- ragdoll multi-body traversal;
- GeometryCollection/destructible traversal;
- cloth/rope/cable simulation;
- scale-changing portals;
- arbitrary moving portal support surfaces.

These can use the same long-term architecture later, but they must not delay the current Core seal.

---

## 6. Immediate next implementation slice

The next code slice should be **A1 only** unless validation exposes a higher-severity traversal regression.

### A1 deliverable

1. instrument actual capture PreExposure per endpoint/recursion layer;
2. establish the RenderTarget/material color-domain contract;
3. normalize the captured scene into the player's expected scene-linear domain without a scene-specific gain;
4. run bright→dark and dark→bright direct-vs-portal A/B validation;
5. verify physical crossing has no portal-induced brightness jump;
6. keep the known-good material graph recoverable during experimentation;
7. update `InteriorPortalObservedIssues.md` and `Validation.md` with only measured results.

Do not begin P8 contact bridging in the same slice.

---

## 7. Definition of progress reporting

Future progress reports should use these states rather than a single percentage:

```text
IMPLEMENTED — code path exists
AUTOMATED VALIDATED — focused repeatable tests pass
MANUAL VISUAL VALIDATED — required PIE visual matrix observed
PERFORMANCE ACCEPTED — measured cost accepted
SEALED — all required gates for that acceptance level passed
DEFERRED — intentionally outside current critical path
```

A feature being `IMPLEMENTED` must not be reported as `SEALED` merely because its unit tests pass.
