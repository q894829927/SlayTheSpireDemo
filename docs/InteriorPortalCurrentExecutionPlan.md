# Interior Portal — Current Execution Plan

Date: **2026-09-14**

Branch reviewed: **`portal/full-fidelity-p1`**

Review HEAD: **`b9e5eb75b8451bd8d944591da3ed832fe0cd4d83`**

Status:

```text
CORE IMPLEMENTATION BASELINE ESTABLISHED /
STEP 1 — FINAL RENDERER CLOSURE ACTIVE /
SCENECAPTURE PATH RETAINED AS FALLBACK / COMPARISON PATH /
STENCIL / MAIN-VIEW RENDERER FEASIBILITY NOW AUTHORIZED /
FULL PHYSICS REMAINS DEFERRED UNTIL CORE SEAL
```

This document is the **active execution-order authority** for the current portal branch. The durable scope, architecture constraints and final Definition of Done remain in [`InteriorPortalFullFidelityImplementationPlan.md`](InteriorPortalFullFidelityImplementationPlan.md).

The current branch is no longer a prototype that should execute every old P-stage in sequence. Most spatial, traversal, query and presentation foundations already exist. The remaining work is therefore grouped into **three large delivery steps** instead of many small stages.

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

Current known gap boundaries:

- portal visual parity is not sealed;
- rigid-body high-speed / CCD aperture safety is not sealed;
- remote-half physical contact does not exist yet;
- cross-portal multi-body constraint support does not exist yet.

The latest `SCS_FinalColorHDR` + capture-owned exposure normalization path is an **experiment/candidate**, not a final P2 seal.

---

## 2. Architecture adjustment

The implementation should now be treated as three cooperating layers:

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

Two rules are now explicit:

1. **One final exposure authority.** The player main view should own the final exposure / local exposure / tone mapping for the production renderer. Do not keep adding scene-specific brightness compensation.
2. **Independent virtual-view history.** Portal views must not blindly share one TAA/Lumen history with the player or with other recursion levels. Synchronize frame/jitter/render policy where needed, but keep distinct virtual-view histories when temporal accumulation is used.

---

# STEP 1 — FINAL RENDERER CLOSURE

This step combines the old P1/P2/P7 visual work, recursion, temporal behavior and rendering performance into one decision.

## 1A. Time-box the current SceneCapture path

Keep both current capture experiments available for controlled A/B comparison:

```text
A — FinalColorHDR + capture eye adaptation + measured PreExposure normalization
B — SceneColorHDRNoAlpha + capture eye adaptation disabled + measured PreExposure normalization
```

Use the same portal pair, same player pose, same destination region and same frame conditions.

Do not accept a solution based on tuning one scalar until one room looks correct.

Required comparison matrix:

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
```

Record at minimum:

```text
capture source
CapturePreExposure
player PreExposure
RenderTarget format / linear-gamma state
recursion level
GPU cost
RenderTarget / VRAM cost
visible temporal artifacts
```

If one SceneCapture path passes the full visual and performance matrix, it may remain the production Core renderer.

## 1B. Run a stencil / main-view renderer feasibility spike

Do not wait until every SceneCapture workaround is exhausted. Build one narrow proof:

```text
one portal pair
one recursion level
main-view / SceneView based virtual camera
portal aperture written to stencil/depth
exit-plane clipping
render only inside the aperture
single final player exposure / tone-map path
```

The spike only needs to answer:

- can the virtual Portal view be rendered into the main frame without a separate final-color texture/composite domain?
- can depth/stencil prevent support-wall leakage and preserve ordinary world occlusion?
- can the transformed camera preserve the current projection/parallax contract?
- is the integration cost acceptable in UE 5.8 for this project?

If the spike succeeds, extend it to finite recursion and make it the production target. The existing SceneCapture implementation remains a fallback/debug path.

If the spike fails for an engine-level reason, record the exact limitation and keep the best validated SceneCapture path instead of accumulating unbounded hacks.

## Step 1 acceptance

The selected production renderer must pass:

```text
no black support-wall leak / giant triangle
no unexplained portal-vs-direct exposure shift
no crossing brightness pop
no obvious portal-only temporal smear / shimmer
correct near / grazing / edge parallax
stable finite recursion
acceptable GPU + VRAM cost
```

After this step the renderer architecture is frozen for Core Seal.

---

# STEP 2 — CORE GAMEPLAY CLOSURE + CORE SEAL

This step merges the old player/camera, rigid-body, PortalQuery, presentation, lifecycle and performance cleanup stages.

## Player / camera

Keep the existing submove aperture gate, crossing state machine and quaternion camera ownership. Finish the manual matrix for:

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

Harden the existing Core transfer path without adding remote-half contact yet:

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

No stale constraints, ignores, proxies, slice materials, remote lights, camera state, traveller records or linked views may remain.

## Core Seal

Core may be sealed only when the selected renderer from Step 1 and all supported Core gameplay paths pass Automation, actual-map PIE/manual acceptance and packaged-build smoke.

Required status wording:

```text
CORE PORTAL FIDELITY — SEALED
FULL PHYSICS FIDELITY — NOT YET SEALED
```

---

# STEP 3 — FULL PHYSICS FIDELITY

Only start this after Step 2 Core Seal, except for an explicitly isolated feasibility experiment.

The target is a Valve-style local hybrid physics model rather than globally cutting the level collision or trying to solve two worlds as one monolithic exact solver.

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

The current support-wall bypass can remain the Core fallback, but the Full Physics path must prevent lateral wall escape using local aperture geometry rather than a broad temporary hole.

## ShadowPhysicsClone

A partially crossed supported rigid body gets one destination-side collision representation:

```text
Source authoritative body
        |
        | portal mapping
        v
ShadowPhysicsClone
```

Rules:

- exactly one authoritative gameplay body;
- clone never becomes an accidental second authority;
- source and clone cannot self-collide;
- clone only collides with the legal destination-side set;
- visual proxy and physics clone are separate concepts;
- reset/destruction removes the clone deterministically.

## Contact response / authority swap / constraints

Implement in increasing complexity:

```text
remote half vs static world
    -> mapped contact point / normal / impulse / torque

then supported dynamic-vs-dynamic cases
    -> finite-mass response, no kinematic infinite-mass shortcut

then authority swap when the supported centre crosses
    -> no double impulse / energy spike

then the explicitly supported cross-portal constraint policy
```

The acceptance target is behavioral fidelity and stability:

```text
no wall escape
no duplicate gravity
no duplicate impulse
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
    + single-layer stencil/main-view feasibility spike
    -> choose/freeze production renderer

STEP 2
    player/camera + rigid-body/held + PortalQuery + presentation + cleanup
    -> Core Portal Fidelity Seal

STEP 3
    PortalPhysicsBubble + ShadowPhysicsClone + mapped contacts + supported constraints
    -> Full Physics Fidelity Seal
```

This three-step order supersedes the older A0-A7 / B0-B6 execution breakdown. Those older labels may still be used as historical references in logs, but new work should be planned and reported against **Step 1 / Step 2 / Step 3**.