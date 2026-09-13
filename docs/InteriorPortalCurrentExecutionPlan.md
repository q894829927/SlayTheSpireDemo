# Interior Portal — Current Execution Plan

Date: **2026-09-14**

Branch reviewed: **`portal/full-fidelity-p1`**

Review baseline HEAD: **`ab0f590d2ccde39a785837c0db6d44422ac698e8`**

Status:

```text
CORE IMPLEMENTATION BASELINE ESTABLISHED /
STEP 1 — FINAL RENDERER CLOSURE ACTIVE /
SCENECAPTURE RETAINED AS FALLBACK / COMPARISON PATH /
STEP 1A — A/B BASELINE + HISTORY DIAGNOSTICS IMPLEMENTED /
VISUAL CEILING TEST STILL OPEN /
STENCIL / MAIN-VIEW RENDERER FEASIBILITY AUTHORIZED /
FULL PHYSICS DEFERRED UNTIL CORE SEAL
```

This document is the **active execution-order authority** for the current portal branch. The durable scope, architectural constraints and final Definition of Done remain in [`InteriorPortalFullFidelityImplementationPlan.md`](InteriorPortalFullFidelityImplementationPlan.md).

When the durable plan still contains older sequencing or renderer-escalation wording, **this document wins for current execution order**. In particular, a bounded Stencil/MainView feasibility spike is already authorized in Step 1; production migration still requires the complete renderer acceptance matrix below.

The current branch is no longer a prototype that should execute every old P-stage in sequence. Most spatial, traversal, query and presentation foundations already exist. Remaining work is grouped into **three large delivery steps**.

---

## 1. Current baseline — preserve, do not redo

The following are already implementation candidates and should be evolved rather than replaced:

```text
LogicalPortalFrame separated from cosmetic surface bias
native SceneCapture clip plane + guarded oblique fallback
explicit player crossing / clearance state machine
CharacterMovement submove aperture constraint
same-submove player transfer before later floor queries
quaternion-owned portal camera and input composition
runtime traveller registration / discovery
sphere / capsule / box conservative portal fit
rigid-body position / rotation / linear / angular velocity transfer
held-object angular velocity preservation
player and rigid-body partial-crossing visual proxies
portal-aware flashlight proxy / light-function path
PortalQuery line trace + sphere sweep
focused Automation + actual-map traversal regressions
```

Current gap boundaries:

- portal visual parity is not sealed;
- rigid-body high-speed / CCD aperture safety is not sealed;
- remote-half physical contact does not exist yet;
- cross-portal multi-body constraint support does not exist yet.

The current `SCS_FinalColorHDR` + capture-owned exposure-normalization path is an **experiment/candidate**, not a final renderer decision.

### 1.1 STEP 1A first delivery status — 2026-09-14

The first STEP 1A implementation is now present in the current branch. It establishes two explicit, independently selectable SceneCapture paths without duplicating the portal system:

```text
FinalColorHDR   -> SCS_FinalColorHDR, capture EyeAdaptation ON
SceneColorLinear -> SCS_SceneColorHDRNoAlpha, capture EyeAdaptation OFF
```

`SceneColorLinear` is the explicit code default and the authored setup default. `PortalViewExposureCorrection` remains disabled by default and is only a diagnostic A/B parameter; it is not a brightness fix and does not use the player's `EyeAdaptationInverse`.

Each endpoint owns one persistent `USceneCaptureComponent2D` per recursion level. The current SceneCapture fallback therefore no longer reuses one endpoint ViewState for multiple virtual camera transforms in the same frame. A history reset is requested for placement/replacement, clear, endpoint invalidation, mode/clip/temporal/render-target/recursion changes, camera cuts and detected discontinuous virtual-camera jumps.

`Saved/PortalRendererDiagnostics.json` records the endpoint, endpoint path, recursion depth, virtual transform, capture mode/source, exposure ownership, RenderTarget format and linear-gamma flag, target path/size, ViewState key/generation, reset reason, clip/TAA state and projected bounds. For `FinalColorHDR`, the image-bound capture PreExposure remains **Unavailable / Unverified**: UE 5.8's public `CaptureScene()` path enqueues the render work, while `GetViewState(0)` is not a proof that a post-call value belongs to the image already written to the target. For `SceneColorLinear`, the capture disables EyeAdaptation; the UE 5.8 renderer contract leaves PreExposure at `1.0`, so no readback or guessed division is performed.

The shared `InteriorPortalMath::ProjectPortalApertureToScreenBounds` helper now produces conservative normalized `MinX/MinY/MaxX/MaxY` bounds with near-clip, camera-crossing, behind-camera and viewport-clipping diagnostics. It is independent of SceneCapture and is intended for later scissor/restricted-viewport use.

This delivery does **not** establish exposure parity, Lumen parity, temporal quality, visual clipping acceptance or production renderer freeze. Those remain manual/renderer-matrix gates below. No Step 1B Stencil/MainView implementation is included in this delivery.

---

## 2. Architecture contracts

Treat the implementation as three cooperating layers:

```text
Portal Spatial Core
    InteriorPortalMath
    traversal state
    quaternion camera mapping
    PortalQuery
    traveller registry

Portal Renderer
    SceneCaptureFallback
    Stencil/MainViewRenderer candidate

Portal Physics
    current Core transfer/gate path
    future PortalPhysicsBubble + ShadowPhysicsClone
```

The following contracts are mandatory for all later work.

### 2.1 One final exposure authority

The production renderer should make the **player main view** the final owner of:

```text
Exposure
Local Exposure
Color Grading
Tone Mapping
Display conversion
```

Do not solve parity with scene-specific brightness gains. `FinalColorHDR + capture EyeAdaptation` remains a comparison path, not proof that capture-owned exposure is the final architecture.

### 2.2 Independent virtual-view histories

Portal views must not blindly share one TAA/TSR/Lumen history with the player or with other recursion levels.

When temporal accumulation is enabled, history identity is at least:

```text
Endpoint + RecursionDepth + RendererViewIdentity
```

Conceptually:

```text
PlayerViewState
Blue/Depth0 ViewState
Blue/Depth1 ViewState
Blue/Depth2 ViewState
Orange/Depth0 ViewState
Orange/Depth1 ViewState
Orange/Depth2 ViewState
```

Synchronize frame timing, jitter policy and render settings where required, but keep history ownership distinct.

Invalidate the affected virtual history on at least:

```text
portal placement / replacement / clear
camera cut / discontinuous virtual-camera transform
recursion-depth change
render-target / viewport resolution change
renderer-mode change
endpoint destruction
```

The current SceneCapture fallback must obey this contract too; it is not only a requirement for the future Stencil renderer.

### 2.3 Exact render-sample ownership

Every captured/composited portal image must have an auditable relationship:

```text
Portal image / RT
    <-> exact virtual view transform
    <-> exact recursion level
    <-> exact PreExposure or exposure-domain state used to render it
    <-> exact temporal-history identity
```

Do not assume that a `GetPreExposure()` value read after a capture necessarily belongs to that exact image unless the implementation proves the renderer timing. If the component-level API cannot prove the association, record exposure state from the actual view/render stage instead.

### 2.4 Portal-bounded rendering

The renderer should not treat a small portal as a full-screen secondary camera when the engine path allows tighter work.

Target contract:

```text
Portal aperture geometry
    -> project to screen
    -> conservative projected bounds
    -> scissor / restricted viewport
    -> off-axis or aperture-derived portal frustum
    -> render only geometry potentially visible through the aperture
```

For the Stencil/MainView path, combine the projected bounds with stencil/depth. For SceneCapture fallback, use the same bounds to reduce unnecessary RT/render work where practical without breaking projection correctness.

---

# STEP 1 — FINAL RENDERER CLOSURE

This combines the old P1/P2/P7 visual work, recursion, temporal behavior and renderer performance into one decision.

## 1A. Bound the current SceneCapture experiments

Keep both capture paths available for controlled A/B comparison:

```text
A — FinalColorHDR + capture eye adaptation; image-bound PreExposure ownership is currently unverified through the public API
B — SceneColorHDRNoAlpha + capture eye adaptation disabled; UE 5.8 capture PreExposure is 1.0 by contract, with no guessed normalization
```

Use the same portal pair, same player pose, same destination region and same frame conditions. Do not accept a solution because one scalar makes one room look correct.

SceneCapture remains eligible as the Core production renderer only if it passes the same acceptance matrix as the MainView candidate. If it cannot, freeze it as fallback/debug behavior rather than continuing unbounded material/exposure workarounds.

## 1B. Stencil / MainView feasibility spike

Build one narrow proof:

```text
one portal pair
one recursion level
main-view / SceneView based transformed virtual camera
portal aperture written to stencil/depth
exit-plane clipping
projected portal bounds / scissor
render only inside the legal aperture
single final player exposure / tone-map path
```

The spike only proves feasibility. It must answer:

- can a transformed Portal view be rendered into the main frame without a separate final-color composite domain?
- can depth/stencil preserve ordinary occlusion and prevent support-wall leakage?
- can current projection/parallax and clip-plane contracts be preserved?
- can a portal-bounded viewport/frustum be derived robustly?
- is the UE 5.8 integration cost acceptable for this project?

**A successful single-layer spike does not become production automatically.** Promotion is:

```text
Feasibility Spike
    -> Renderer Candidate
    -> finite recursion + temporal/Lumen/render-feature implementation
    -> full visual/performance acceptance matrix
    -> Production Renderer Freeze
```

If the spike fails for an engine-level reason, record the exact limitation and keep the best validated SceneCapture path instead.

## 1C. Unified renderer acceptance matrix

Run the selected candidates through one matrix rather than separate ad-hoc screenshots.

### View / exposure / traversal cells

```text
bright -> dark
dark -> bright
static view
camera approaching portal
physical crossing
left/right/top/bottom aperture edge
grazing angle
recursion depth 1 and >= 2
flashlight off/on
slow camera motion
fast camera motion
```

### Render-feature cells

```text
opaque static geometry
moving object / velocity-dependent rendering
translucent material
fog / volumetric contribution
decal
emissive + bloom
Lumen GI
Lumen / supported reflections
TAA / TSR temporal stability
recursive portal facing portal
```

### Required telemetry

For SceneCapture paths record at minimum:

```text
capture source
exact CapturePreExposure ownership per image
player PreExposure
RenderTarget format / linear-gamma state
virtual-view history identity
recursion level
projected portal screen bounds
GPU cost
RenderTarget / VRAM cost
visible temporal artifacts
```

For the MainView/Stencil candidate record equivalent virtual-view, history, GPU and memory data even if no portal RT exists.

## Step 1 acceptance

The production renderer may be frozen only when it passes:

```text
no black support-wall leak / giant triangle
no unexplained portal-vs-direct exposure or color shift
no crossing brightness pop
no portal-only temporal smear / shimmer / reset flash
correct near / grazing / edge parallax
correct depth / occlusion around the aperture
stable finite recursion
acceptable translucency / fog / decal / emissive behavior
acceptable Lumen / reflection behavior for the documented scope
acceptable GPU + memory cost
```

After this gate the renderer architecture is frozen for Core Seal. SceneCapture may remain as fallback/debug even if Stencil/MainView becomes production.

---

# STEP 2 — CORE GAMEPLAY CLOSURE + CORE SEAL

This merges the old player/camera, rigid-body, PortalQuery, presentation and lifecycle work.

## Player / camera

Keep the existing submove aperture gate, crossing state machine and quaternion camera ownership. Finish the matrix for:

```text
slow enter / stop / retreat
fast enter / immediate reverse
jump + mouse-look
wall -> wall
wall -> floor
floor -> wall
ceiling orientations where allowed
aperture edge / corner cases
```

No wall escape, roll snap, input inversion, ping-pong transfer or stale ignore state is allowed.

## Rigid bodies / held objects

Harden the existing Core transfer path without adding remote-half physical contact yet:

- high-speed / CCD crossing;
- lateral aperture escape prevention;
- sphere / capsule / box / conservative StaticMesh acceptance;
- unsupported category rejection;
- blocked-exit safety;
- position / orientation / linear velocity / angular velocity preservation;
- Physics Handle reacquisition stability;
- partial visual source/remote seam quality.

## PortalQuery / interaction

Finish the current query layer as shared infrastructure:

- optional segmented debug visualization;
- deterministic hop-limit tests;
- remaining supported interaction consumers;
- projectile / beam consumer only if required by the exploration mode;
- explicit hit-point / normal / distance semantics after a portal hop.

## Lifecycle / cleanup

Validate reset/replacement/destruction while temporary state is active:

```text
portal reset / replacement
endpoint destruction
traveller unregister / destruction
held-object release
player despawn / map transition
PIE EndPlay
renderer target/history reset
```

No stale constraints, ignores, proxies, slice materials, remote lights, camera state, traveller records, linked views or virtual-view histories may remain.

## Core Seal

Core may be sealed only when the production renderer from Step 1 and all supported Core gameplay paths pass Automation, actual-map PIE/manual acceptance, performance profiling and packaged-build smoke.

Required status wording:

```text
CORE PORTAL FIDELITY — SEALED
FULL PHYSICS FIDELITY — NOT YET SEALED
```

---

# STEP 3 — FULL PHYSICS FIDELITY

Only start this after Step 2 Core Seal, except for an explicitly isolated feasibility experiment.

The target is a Valve-style local hybrid physics model rather than globally cutting level collision or trying to solve two worlds as one monolithic exact solver.

## PortalPhysicsBubble

Create a bounded local crossing environment around each active aperture:

```text
PortalPhysicsBubble
    aperture / rim collision collar
    source-side valid collision set
    destination-side valid collision set
    traveller crossing state
    explicit collision-pair filtering
```

The current support-wall bypass can remain the Core fallback, but Full Physics must prevent lateral wall escape using local aperture geometry rather than a broad temporary hole.

## ShadowPhysicsClone

A partially crossed supported rigid body gets one destination-side collision representation:

```text
Source authoritative body
        |
        | rigid portal mapping
        v
ShadowPhysicsClone
```

Rules:

- exactly one authoritative gameplay body at all times;
- clone never becomes an accidental second authority;
- source and clone cannot self-collide;
- clone only collides with the legal destination-side set;
- visual proxy and physics clone are separate concepts;
- gravity is applied exactly once;
- reset/destruction removes the clone deterministically.

## Contact response, atomic authority swap and constraints

Implement in increasing complexity:

```text
remote half vs static world
    -> mapped contact point / normal / impulse / torque

then supported dynamic-vs-dynamic cases
    -> finite-mass response, no kinematic infinite-mass shortcut

then authority swap
    -> no double impulse / no missing contact / no energy spike

then explicitly supported cross-portal constraints
```

Authority swap must have a single explicit rule. For each supported traveller define an **authority reference point** (normally the authoritative rigid body's centre of mass unless another reference is documented). Transfer ownership only when that reference crosses the `LogicalPortalPlane` with configured hysteresis.

The swap is atomic within one physics step:

```text
before swap: Source authoritative, Shadow clone derived
swap condition reached once
same physics step: mapped pose/velocities/contact ownership committed
       -> Destination authoritative, old source becomes derived/removed
```

There must never be a step where both bodies are authoritative or neither body owns gravity/contact response.

The acceptance target is behavioral fidelity and stability:

```text
no wall escape
no duplicate gravity
no duplicate impulse
no missing-contact frame at authority swap
bounded energy
correct visible push / torque
stable crossing and reversal
repeatable cleanup
```

Do not claim support for ragdolls, GeometryCollection, vehicles, cloth/rope or arbitrary multi-body assemblies unless they receive their own documented policy and validation.

When this step passes its static/dynamic contact, authority-swap, supported-constraint, performance and packaged-build matrix, the final status may become:

```text
CORE PORTAL FIDELITY — SEALED
FULL PHYSICS FIDELITY — SEALED FOR DOCUMENTED SUPPORTED CATEGORIES
```

---

## 3. Immediate execution order

```text
STEP 1
    SceneCapture A/B ceiling test
    + single-layer Stencil/MainView feasibility spike
    + portal-bounded frustum/scissor + virtual-history contract
    -> compare Renderer Candidates
    -> full renderer matrix
    -> freeze production renderer

STEP 2
    player/camera + rigid-body/held + PortalQuery + presentation + cleanup
    -> Core Portal Fidelity Seal

STEP 3
    PortalPhysicsBubble + ShadowPhysicsClone + mapped contacts
    + atomic authority swap + supported constraints
    -> Full Physics Fidelity Seal
```

This three-step order supersedes the older A0-A7 / B0-B6 execution breakdown. Older labels may remain as historical references in logs, but new work should be planned and reported against **Step 1 / Step 2 / Step 3**.
