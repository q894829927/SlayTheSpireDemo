# Interior Portal — Current Execution Plan

> **HISTORICAL EXECUTION RECORD — SUPERSEDED FOR CURRENT WORK**
>
> The sequencing/status language below records the Portal development state as of 2026-09-15 and is retained as historical engineering evidence. It is **not** the current resume/execution authority, even where older text below says otherwise.
>
> Current authorized Portal work is defined by:
>
> ```text
> docs/PortalPerformanceVRAMP1Plan.md
> ```
>
> Current functional closure evidence is:
>
> ```text
> docs/InteriorPortalExperiment/InteriorPortalFullFidelityProductionRegression.md
> ```
>
> Resume new Portal work from those documents. Do not resume STEP 1B.x experiments from this historical record unless a new reproduced regression explicitly reopens them.

Date: **2026-09-15**

Branch reviewed: **`portal/full-fidelity-p1`**

Review / diagnostic baseline HEAD: **`85f3685220ac9a8852ab0ab590c970459a7a16ca`**

Status:

```text
CORE IMPLEMENTATION BASELINE ESTABLISHED /
STEP 1 — FINAL RENDERER CLOSURE ACTIVE /
SCENECAPTURE RETAINED AS FALLBACK / COMPARISON PATH /
STEP 1A — A/B BASELINE + HISTORY DIAGNOSTICS IMPLEMENTED /
STEP 1B.2 — PUBLIC CUSTOM RENDER PASS SPIKE PARTIAL /
STEP 1B.3 — MAIN SCENECOLOR COMPOSITION BOUNDARY PARTIAL /
STEP 1B.4 — BASECOLOR CRP DIAGNOSTIC PASSED /
CURRENT DEPTHANDBASEPASS LIT SCENECOLOR CEILING CONFIRMED /
FULL TRANSFORMED SUB-VIEW / RENDERER-PRIVATE SPIKE NEXT /
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

This delivery does **not** establish exposure parity, Lumen parity, temporal quality, visual clipping acceptance or production renderer freeze. Those remain manual/renderer-matrix gates below. The Step 1B result is recorded separately below.

### 1.2 STEP 1B.2 Public CustomRenderPass feasibility result — 2026-09-14

**Result: `STEP 1B.2 PARTIAL`.** The public custom-pass path proves that a
transformed portal scene view can be submitted to the UE 5.8 renderer without
modifying Engine source, but it does not expose the main-view composition
bindings required for the full aperture contract.

The project now has an explicit renderer backend contract:

```text
RendererBackend = SceneCapture             (default / retained fallback)
RendererBackend = MainViewStencilSpike    (explicit feasibility selection)
RendererBackend = CustomRenderPassSpike   (explicit public CRP proof; not default)
RendererBackend = CustomRenderPassCompositionSpike (explicit pre-tonemap composition experiment)

CaptureColorMode = FinalColorHDR | SceneColorLinear
RenderClipMode   = NativeClipPlane | ObliqueFallback
```

The new `FInteriorPortalRenderRequest` is a copied, immutable game-thread
description containing the endpoint, recursion level, logical entry/exit
frames, mapped virtual transform, exact view matrices, player-view projected
bounds, conservative pixel scissor, exit logical clip plane and renderer
history identity. The shared math is tested independently of SceneCapture.
Backend changes invalidate the renderer history generation, and the spike owns
an independent persistent `FSceneViewStateReference` for each endpoint. The
existing project-side `FInteriorPortalViewExtension` remains a separate
request-only MainView experiment.

`CustomRenderPassSpike` uses the UE 5.8 public
`FSceneInterface::AddCustomRenderPass` API with
`FSceneInterface::FCustomRenderPassRendererInput` and the project-owned
`FInteriorPortalCustomRenderPass : FCustomRenderPassBase`. The supplied
`ViewLocation`, `ViewRotationMatrix` and `ProjectionMatrix` are consumed by
the Engine's `FSceneRenderer` to construct a real transformed `FSceneView` /
`FViewInfo`; the independent portal ViewState is passed through the same input.
The pass uses `DepthAndBasePass` and `SceneColorNoAlpha`, producing a separate
HDR scene-color target in the custom-pass pre-tonemap scene-color domain.
This is a real transformed scene-render proof, not a SceneCapture/material
composite. The pass is limited to one visible endpoint and `RecursionDepth=1`.

The CRP target is separate by itself and does not expose a direct main-view
SceneColor merge, aperture stencil or CRP scissor field. STEP 1B.3 tested the
missing project-side composition boundary separately rather than relabeling
this CRP proof as a stencil renderer. Lumen and reflection parity are not
part of the public CRP output contract, and no GPU readback was used to claim
them.

The minimum Engine escalation point is a renderer-private portal pass in
`FDeferredShadingSceneRenderer::Render`, around the existing custom-render-pass
phase and before the `AddResolveSceneColorPass` /
`PrePostProcessPass_RenderThread` boundary. That hook needs private access to
the renderer's `FViewInfo`, scene visibility/base-pass machinery, main
`FSceneTextures` SceneColor/depth-stencil, portal aperture stencil/scissor and
the mapped virtual view. It must compose the virtual pass into the main
pre-tonemap SceneColor and then allow the player's normal exposure,
local-exposure and tone-map chain to run once. No Engine source was modified.

Focused Automation covers the contract only: **16/16 PASS** in
`Saved/AutomationReports/STEP1B2_Final/index.json`. This does not prove GPU
stencil correctness, main-view composition, exposure parity, Lumen parity,
temporal quality or visual clipping. SceneCapture remains the default and
validated fallback; production renderer freeze and Core seal remain open.

### 1.3 STEP 1B.3 Main SceneColor composition boundary — 2026-09-14

**Result: `STEP 1B.3 PARTIAL`.** UE 5.8 public project-side APIs can compose
the transformed CRP HDR target into the player's post-process SceneColor chain,
but they do not provide the public main depth-stencil/stencil contract needed
for a production aperture/depth renderer.

The explicit backend is now:

```text
SceneCapture                         (default / retained fallback)
MainViewStencilSpike                 (request-only feasibility path)
CustomRenderPassSpike                (separate transformed HDR proof target)
CustomRenderPassCompositionSpike     (explicit CRP -> Main SceneColor experiment)
```

`CustomRenderPassCompositionSpike` still submits a real one-layer transformed
view through `FSceneInterface::AddCustomRenderPass`, with
`FCustomRenderPassRendererInput`, `FCustomRenderPassBase`, an independent
portal ViewState and the existing portal transform builder. It then publishes
only copied immutable request data to `FInteriorPortalViewExtension`.

The project-side composition hook is
`ISceneViewExtension::SubscribeToPostProcessingPass` for
`EPostProcessingPass::BeforeDOF`. UE 5.8 invokes the delegate from the normal
post-processing chain with `FPostProcessMaterialInputs`; the delegate reads
`EPostProcessMaterialInput::SceneColor`, imports the CRP HDR target using
`FRenderTarget::GetRenderTargetTexture`, and returns a new RDG screen-pass
SceneColor. This is before player eye adaptation/local exposure, color grading
and tonemap, so the player remains the only final display-domain authority.
`PrePostProcessPass_RenderThread` was not used to replace SceneColor: its public
inputs are const/internal and the renderer constructs the post-process
SceneColor chain after that hook.

The proof uses an explicit analytic ellipse derived from the conservative
logical portal projected bounds. It does not use the whole bounding rectangle.
`FPostProcessMaterialInputs::SceneTextures` exposes the main SceneDepth texture
for a future comparison, but this spike does not perform a depth test. Public
scene texture parameters expose CustomStencil rather than the main
depth-stencil stencil binding, and the CRP input has no scissor field. Thus:

```text
transformed CRP HDR -> BeforeDOF SceneColor composition     PASS (API boundary)
logical projected ellipse aperture                           PARTIAL (GPU/PIE)
main depth occlusion continuity                              NOT IMPLEMENTED
main stencil aperture                                         UNAVAILABLE / UNVERIFIED
actual CRP scissor                                            NOT APPLIED
```

The external target is re-imported into the same RDG graph on the render
thread; the immutable request carries the render-resource identity and no
Actor/UObject mutable state. The resource/lifetime contract is covered at the
request/configuration level, but GPU resource correctness and radiometric
parity still require PIE/RenderDoc/manual evidence.

Focused Automation: **17/17 PASS** in
`Saved/AutomationReports/PortalCompositionBoundary/index.json`. This proves
backend selection, composition activation gating, request/math/history
contracts and SceneCapture regression only. It does not prove GPU
composition, stencil/depth behavior, exposure parity, Lumen, TSR/TAA or
visual clipping. `MANUAL VISUAL ACCEPTANCE REQUIRED` remains for the
composition backend and for the retained SceneCapture A/B matrix.

The remaining production escalation is narrower than the old “no public main
composition” statement: project code can perform a pre-tonemap SceneColor
replacement, but a renderer-private hook is still required to bind the main
depth-stencil/stencil aperture, apply true portal geometry/scissor and provide
depth-continuous occlusion around the CRP view. No Engine source was modified.

### 1.4 STEP 1B.4 BaseColor CRP diagnostic — 2026-09-15

**Result: `BASECOLOR PATH PASS / CURRENT DEPTHANDBASEPASS LIT PATH INSUFFICIENT`.**

Commits `8b0b39d7703cf7b4e564405ddc9849f522a10963` and
`85f3685220ac9a8852ab0ab590c970459a7a16ca` add a bounded diagnostic without
changing the virtual camera, projection, RenderTarget ownership or BeforeDOF
composition architecture:

```text
portal.CompositionDebugMode 0 -> SceneColorNoAlpha through normal aperture
portal.CompositionDebugMode 1 -> magenta portal aperture
portal.CompositionDebugMode 2 -> full-screen magenta
portal.CompositionDebugMode 3 -> BaseColor CRP through normal aperture
```

Mode 3 keeps `FCustomRenderPassBase::ERenderMode::DepthAndBasePass` but changes
the requested output from `SceneColorNoAlpha` to `BaseColor`. The composition
shader explicitly routes Mode 3 through the ordinary `PortalTexture` sampling
and analytic aperture mask rather than the Mode 1/2 magenta paths.

The user-supplied 2026-09-15 Mode 3 `CompositionA.png` contains coherent
unlit/base-pass material information inside the portal: a gray floor/lower
surface, an olive-green band, a center character with multiple material colors
and a right-side object with a blue/cyan material region. The main view outside
the aperture remains the normal lit scene. This is manual visual evidence that
the transformed view reaches target-space geometry and that BasePass -> CRP
output -> RenderTarget -> PortalTexture -> aperture composition is alive for
the tested view.

This result rules out the following as the primary explanation for the earlier
black Mode 0 portal in the same path:

```text
all target geometry being clipped away
a completely dead transformed virtual view
an entirely unwritten portal RenderTarget
PortalTexture always sampling an empty target
the analytic aperture composition being wholly disconnected
```

It does **not** prove the complete transformed-camera edge/grazing/parallax
matrix, main depth/stencil continuity, true scissor, deferred lighting,
shadows, Lumen GI/reflections, translucency/fog/decals, motion vectors,
TAA/TSR, exposure parity or recursion.

The important renderer boundary is now narrower: the current
`DepthAndBasePass` CRP configuration can produce meaningful BaseColor but its
`SceneColorNoAlpha` result is not a usable fully lit HDR portal view. Continue
Step 1 at the renderer level rather than treating this black result as a
portal-transform/RT/aperture bug or adding brightness/material workarounds.

Detailed evidence and claim boundaries are recorded in
[`InteriorPortalCRPBaseColorDiagnostic.md`](InteriorPortalCRPBaseColorDiagnostic.md).
Keep Mode 3 as a retained layer diagnostic for future renderer work.

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

## 1B. Full transformed sub-view / Stencil / MainView feasibility spike

STEP 1B.4 closes the current `DepthAndBasePass + SceneColorNoAlpha` CRP result as
a candidate for the required fully lit portal image. Do not keep extending that
exact path with output-mode or brightness hacks. Preserve its BaseColor Mode 3
as a diagnostic while moving the next feasibility proof to a renderer path that
actually executes the required lit scene stages.

Build one narrow proof:

```text
one portal pair
one recursion level
full transformed SceneView/sub-view virtual camera
portal aperture written to stencil/depth where the chosen path supports it
exit-plane clipping
projected portal bounds / scissor
render only inside the legal aperture where feasible
produce a genuinely lit HDR portal result
single final player exposure / tone-map path
```

The first spike should keep `RecursionDepth=1` and avoid coupling recursion,
full temporal history and every render feature into the initial proof. Reuse
the existing transform/projection, portal target ownership and pre-tonemap
composition boundary where they remain valid; change the producer of the
portal lit image, not the already-proven aperture consumer without cause.

The spike only proves feasibility. It must answer:

- can a transformed Portal view be rendered into the main frame without a separate final-color composite domain?
- can the selected renderer path produce the required fully lit HDR portal result rather than BasePass-only content?
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
    SceneCapture A/B retained as fallback/comparison
    + CRP transformed-geometry/BaseColor diagnostic COMPLETE
    -> one-layer full transformed sub-view / renderer-private lit spike
    -> depth/stencil/scissor + portal-bounded rendering
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