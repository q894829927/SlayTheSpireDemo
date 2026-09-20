# Interior Portal — Observed Issues and Regression Targets

## Current crop-display regression — 2026-09-20

The reproduced vertical slit / distance-dependent aperture scaling has a
working-tree repair on `df09691`: bounded-pass raster NDC was incorrectly used
as receiving-camera NDC. Color/depth aperture coordinates and TSR depth
extraction were corrected. Build and focused Automation passed; actual PIE
static captures and four-layer recursive publication were checked. Continuous
motion/traversal visual acceptance remains **USER ACTION REQUIRED**; Ping-Pong
is retained but defaults off pending that gate.

See [the dedicated repair evidence](InteriorPortalCropDisplayRegression.md)
for scope, current status, failed/discarded evidence and exact remaining steps.
The 2026-09-14 entries below are historical observations; their exposure and
rendering status must be read alongside the later FullFidelity production
acceptance, not treated as a current list of reproduced failures.

Date: **2026-09-14**

Status:

```text
ACTIVE DEFECT LOG /
USED TO DRIVE THE FULL-FIDELITY PORTAL IMPLEMENTATION /
DO NOT MARK CORE PORTAL FIDELITY SEALED WHILE A BLOCKING ITEM BELOW REMAINS OPEN
```

Related authority:

- `docs/InteriorPortalFullFidelityImplementationPlan.md`
- `docs/InteriorPortals.md`
- `Source/SlayTheSpireDemo/Interior/InteriorPortalSystem.h/.cpp`
- `Source/SlayTheSpireDemo/Interior/InteriorPortal.h/.cpp`
- `Source/SlayTheSpireDemo/Interior/InteriorChildCharacter.h/.cpp`
- `tools/setup_interior_portals.py`

This document records defects observed in live PIE validation while implementing the full-fidelity portal plan. It is not a substitute for the implementation plan; it is the concrete regression list that each delivery stage must close.

---

## 1. P1 clipping defect — black support wall / giant diagonal triangle

### Observed symptom

At near or oblique portal viewing angles, the portal image could show either:

- a large black wall region; or
- a giant diagonal triangular/wedge-shaped piece of geometry.

### Current status

**Mitigated in the tested normal views, not fully sealed.**

The current branch separates the logical portal plane from the cosmetic visual surface and uses the native SceneCapture clip plane as the P1 production candidate. The originally reproduced black-wall / giant-triangle defect is no longer visible in the supplied normal PIE screenshots.

### Why it happened

The previous path relied on a custom oblique near-plane projection with a very small offset from the exit support surface. At near/grazing configurations, the support wall could intersect or leak across the custom clipping plane, and an ill-conditioned projection could produce extreme triangle distortion.

### Remaining regression matrix

P1 is not sealed until all of the following pass:

```text
centered far
centered near
left/right aperture edge
high/low aperture edge
grazing angle
camera crossing the plane
recursion depth >= 2
rapid portal replacement / clear
```

---

## 2. P2 exposure-domain mismatch — portal view too dark or over-corrected

### Observed symptom

The same destination scene has substantially different brightness when viewed:

```text
through the portal
vs
standing in the destination directly
```

Observed A/B results:

- raw portal RenderTarget path (`PortalViewExposureCorrection = 0`) is visibly too dark;
- the first `EyeAdaptationInverse` correction path (`PortalViewExposureCorrection = 1`) is visibly too bright / over-corrected;
- when eye adaptation is disabled and pre-exposure is forced to `1.0`, the direct and portal brightness broadly converge, although the whole bright scene becomes overexposed.

### Current status

**STEP 1A A/B baseline implemented — direct-vs-portal parity remains open and
continues to block P2 visual fidelity.**

The current renderer has two explicit paths. `FinalColorHDR` uses
`SCS_FinalColorHDR` with capture EyeAdaptation enabled, but the public UE 5.8
API does not prove that a `GetViewState(0)->GetPreExposure()` read immediately
after `CaptureScene()` belongs to the RenderTarget image just enqueued. The
implementation therefore records the ownership as `Unavailable / Unverified`,
does not perform a one-frame shift or magic correction, and leaves the
normalization diagnostic disabled by default. `FinalColorHDR` is a capture
post-process-domain comparison path; it is not claimed to restore original
scene-linear radiance.

`SceneColorLinear` uses `SCS_SceneColorHDRNoAlpha` and disables capture EyeAdaptation.
In the UE 5.8 renderer path, `UpdatePreExposure()` leaves capture PreExposure at
`1.0` when EyeAdaptation is not relevant, so this path does not divide by a
guessed value and does not use the player's `EyeAdaptationInverse`. The player
main view remains the intended owner of final Exposure, Local Exposure, color
grading and tone mapping. This is a contract baseline, not visual parity proof.

Both paths use the same explicit portal material and linear-gamma HDR target;
the old `PortalViewExposureCorrection` blend is available only when the system
diagnostic switch is enabled. A scene-specific brightness multiplier is not
introduced. `Saved/PortalRendererDiagnostics.json` supplies the per-sample
comparison data, while direct-vs-portal visual parity remains a PIE/manual gate.

### Why the first correction is insufficient

The earlier SceneCapture path used `SCS_SceneColorHDRNoAlpha`, which left the
portal surface in a separate capture exposure domain. Applying the player's
`EyeAdaptationInverse` to that texture did not invert the SceneCapture's own
history and produced the over-bright screenshot. A constant gain or a full
inverse-eye-adaptation transform is not a correct general solution.

### Required direction

The next P2 correction must explicitly reason about the **capture view's own pre-exposure**, not tune a scene-specific brightness multiplier. If the required image-bound ownership cannot be obtained through a supported public hook, the limitation must remain an explicit SceneCapture ceiling and feed the Stencil/MainView feasibility decision.

Target pipeline:

```text
SceneCapture FinalColorHDR (comparison path)
    -> only normalize Capture PreExposure when image ownership is proven
    -> HDR portal value in the resolved capture post-process domain
    -> emit through portal material
    -> player view applies its normal exposure/tonemap once
```

### STEP 1A renderer ceiling and history diagnostics — 2026-09-14

- The A/B enum is separate from `RenderClipMode`; no comment-edit switch is required.
- Each endpoint has persistent depth-indexed capture components/ViewStates. Their identities are recorded as endpoint/depth/ViewKey/generation and are covered by focused Automation.
- History reset requests cover portal placement/replacement, clear, endpoint invalidation, renderer mode/configuration changes, camera cuts and discontinuous virtual-camera transforms. This proves ownership separation and invalidation signaling, not temporal visual quality.
- The shared projected-aperture helper is independent of SceneCapture and handles viewport clipping, near-plane intersection, camera crossing, behind-camera rejection and grazing/partial visibility. Its bounds are normalized to the player view and are available for later scissor work.
- `FinalColorHDR` exact image PreExposure ownership is still unavailable through the inspected public UE 5.8 timing. This is an architectural limitation of the current public SceneCapture boundary, not a reason to add a frame offset.
- No exposure parity, Lumen parity, TAA/TSR quality or visual seal is claimed. Manual PIE evidence is still required for both paths.

### STEP 1B.2 Public CustomRenderPass feasibility spike — 2026-09-14

**Result: `PARTIAL`.** UE 5.8's public CustomRenderPass path can submit a real
transformed portal scene view. The CRP itself is a separate target; the
project-side main SceneColor boundary is tracked separately in STEP 1B.3.

The branch now separates `RendererBackend` from `CaptureColorMode` and keeps
`SceneCapture` as the default fallback. An explicit
`CustomRenderPassSpike` backend builds a copied `FInteriorPortalRenderRequest`
for one visible portal and one recursion layer, maps it to
`FSceneInterface::FCustomRenderPassRendererInput`, and submits a heap-owned
`FInteriorPortalCustomRenderPass : FCustomRenderPassBase` through
`FSceneInterface::AddCustomRenderPass`. UE 5.8 consumes the supplied
location, rotation matrix and projection matrix to construct a real
transformed `FSceneView` / `FViewInfo`, with an independent portal
`FSceneViewStateReference`. The pass requests `DepthAndBasePass` and
`SceneColorNoAlpha`, so the output is a separate HDR/pre-tonemap scene-color
target. This is a genuine transformed scene-render proof, not a SceneCapture
material wrapper. It is intentionally limited to one visible portal and one
recursion layer.

The existing `MainViewStencilSpike` remains a separate request-only
`FWorldSceneViewExtension` experiment; it is not relabeled as a CustomRenderPass
result. The public CRP output is not composited into the player's main
SceneColor. Project code cannot bind the main `FSceneTextures::Color`,
depth-stencil or stencil to the pass, and the public renderer input has neither
a scissor field nor a `GlobalClippingPlane` field. The shared projected-bounds
math and pixel scissor are therefore constructed and diagnosed but
`scissorApplied` remains false. The logical exit plane is encoded through the
existing oblique projection helper when possible; this is not equivalent to a
renderer-private global clip for every view. Lumen/reflection parity is
unavailable/unverified through this public pass contract. No independent portal
output is sent through player `EyeAdaptationInverse`, and no brightness
multiplier was added.

The remaining production escalation is a private deferred-renderer portal
aperture/depth pass in `FDeferredShadingSceneRenderer::Render`, around the
custom-render-pass phase and before the existing SceneColor resolve /
`PrePostProcessPass_RenderThread` boundary. It needs access to `FViewInfo`,
scene visibility/base-pass, main `FSceneTextures` depth-stencil/stencil,
stencil/scissor aperture state and the mapped virtual view. Engine source was
not modified.

Automation result: **16/16 PASS** for backend/configuration/request mapping,
ViewState identity, bounds/scissor math, clip contract, activation and
lifecycle contracts. This is not GPU evidence. `MANUAL VISUAL ACCEPTANCE
REQUIRED` remains for CustomRenderPass GPU output/aperture/depth and for the
retained SceneCapture bright/dark, crossing, edge, grazing and recursion
matrix. No visual parity/pass is claimed.

### STEP 1B.3 Main SceneColor composition boundary — 2026-09-14

**Result: `PARTIAL`.** The public project-side boundary is more capable than
the CRP-only result suggested, but it is not a complete stencil/depth Portal
Renderer.

`CustomRenderPassCompositionSpike` retains the real CRP transformed scene pass
and uses `FInteriorPortalViewExtension::SubscribeToPostProcessingPass` at
`EPostProcessingPass::BeforeDOF`. The callback receives the current
`FPostProcessMaterialInputs::SceneColor`, imports the CRP `FRenderTarget` with
`GetRenderTargetTexture`, and writes a new RDG screen-pass texture. In the
UE5.8 call chain this is the player's HDR/pre-tonemap SceneColor chain, before
player exposure/local exposure/color grading/tonemap. It therefore establishes
a project-side pre-tonemap composition proof without sending the portal image
through a second final-color material domain.

The aperture in this spike is an explicit analytic ellipse generated from the
logical portal projected bounds. It avoids exposing the entire conservative
rectangle, and portal-outside pixels remain the original Main SceneColor.
However, the shader does not yet compare main SceneDepth, and public
`FSceneTextureShaderParameters` does not expose the main depth-stencil stencil
binding (only CustomDepth/CustomStencil resources). The CRP request also has no
public scissor field. Therefore main-wall/depth continuity, true stencil
masking and actual scissor application are **NOT IMPLEMENTED / UNVERIFIED**.

The public API findings are:

```text
SubscribeToPostProcessingPass(BeforeDOF)  readable/replacable SceneColor: YES
PrePostProcessPass_RenderThread            direct SceneColor replacement: NO
Main SceneDepth                            visible through SceneTextures: YES
Main stencil                               public binding: NO / UNVERIFIED
CRP HDR external target                    same-RDG import: YES
Player final exposure/tonemap authority    structurally preserved: YES
```

Focused Automation is **17/17 PASS** in
`Saved/AutomationReports/PortalCompositionBoundary/index.json`. This is
contract evidence only. GPU composition, portal aperture edge stability,
wall/depth occlusion, exposure parity and visual quality require
`MANUAL VISUAL ACCEPTANCE REQUIRED` in PIE/RenderDoc. SceneCapture remains the
default fallback; no brightness gain, EyeAdaptationInverse or ObliqueFallback
change was introduced.

### Acceptance

Across both bright->dark and dark->bright portal directions:

- no obvious black crush;
- no white blowout caused by the portal material;
- no large brightness jump when the player physically crosses the portal;
- auto-exposure adaptation does not make the portal drift independently from the destination view.

---

## 3. Partial-crossing player/held-item visual discontinuity

### Observed symptom

When the player is only partially through a portal, first-person geometry such as the flashlight can disappear at the portal plane. The object is visible on the source side but the portion that should already exist on the destination side is missing.

### Current status

**Implementation candidate present — visual acceptance remains open.**

On 2026-09-13 `UInteriorPortalPresentation` added a mapped, no-collision proxy
for every player mesh tagged `PortalTravellerVisual`. The authored flashlight
body/rim/lens/grip and child body parts now use the five generated slice
materials in `/Game/SlayTheSpireDemo/Interior/Portals/`. The source and remote
materials receive the same logical portal origin/normal, so the visual plane is
independent of the cosmetic portal-surface bias.

The state-level PIE check in `Saved/PortalPlayerPresentationRuntime.json`
reported five bound slice materials and three active remote player visuals while
the pawn was placed at the blue entry. This is runtime evidence for the proxy
path; it does not close the manual near-plane, grazing-angle or held-item visual
matrix.

### Why it happens

The original partial-crossing proxy/slice implementation covered configured
rigid-body `PhysicsTravellers`. The player path now has the equivalent remote
representation, but it is still limited to mesh components whose material slots
are explicitly mapped in the map-owned presentation component.

Before the candidate path, the visual model was effectively:

```text
source first-person mesh
        |
        | portal plane
        X destination-side copy does not exist
```

The candidate now creates that destination-side copy for mapped player meshes;
the remaining question is whether the material and proxy placement satisfy the
full visual acceptance matrix in PIE.

### Required behavior

Player-owned visuals that can intersect the aperture must participate in the same portal visual contract as rigid-body travellers:

```text
source visual
    + source-side slice
    + mapped remote visual proxy
    + destination-side inverse slice
    = one continuous object across the portal
```

The implementation must define which player-owned components participate, at minimum the current first-person flashlight body/rim/lens/grip and any future held-item visual registered as a PortalTraveller visual child.

### Acceptance

- slow partial insertion does not make the flashlight/held item vanish;
- source and remote halves meet continuously at the logical portal plane;
- no duplicate full object is visible;
- the proxy is removed/authority-swapped deterministically after crossing clears;
- rapid forward/backward motion does not leave a stale proxy.

---

## 4. Flashlight illumination is not portal-aware

### Observed symptom

When the player shines the flashlight at/through the portal, illumination can appear to originate from behind the supporting wall rather than continuing cleanly through the aperture.

### Current status

**Implementation candidate present — visual acceptance remains open.**

`UInteriorPortalPresentation` now maps the attached flashlight origin,
orientation and cone into the paired endpoint. It creates up to two movable
remote spotlights, assigns `M_LF_PortalFlashlight` as a light-function mask for
the legal aperture, reserves a free lighting channel, and restores every
temporary channel assignment during reset or teardown. The source intensity is
suppressed while its emitter is crossing so the two copies do not double-light
the scene.

The same PIE state-level check reported one active remote light with the
flashlight enabled at the blue entry (`Saved/PortalPlayerPresentationRuntime.json`).
The direct and portal exposure matrix, arbitrary light directions and manual
visual quality gate remain open.

### Current implementation constraint

The authoritative light remains the ordinary movable `USpotLightComponent`
attached to the first-person `FlashlightRig`; the presentation component adds a
destination-side proxy only while the linked portal aperture can receive the
cone. General world-light transport is still outside the supported scope.

This means the existing light behaves as an ordinary world-space spotlight even while the player's visual/camera state is transitioning between two portal spaces.

### Why this matters even though full physical light transport is not a V1 goal

The full implementation plan currently excludes physically simulated general light transport through portals. That non-goal should remain for arbitrary world lights, GI and physically exact radiance transport.

However, the player's flashlight is a direct gameplay-facing attached tool. Its visible beam must obey portal-space continuity or it visibly contradicts the portal traversal. Therefore this issue is narrower than general portal light transport and should be treated as a **supported first-person traveller effect**, not as a promise to simulate arbitrary lights through portals.

### Required behavior

Preferred staged solution:

```text
source flashlight
    -> determine whether the cone intersects the legal portal aperture
    -> map source location + direction through Entry->Exit
    -> enable/update a destination-side remote SpotLight proxy
    -> prevent the source light from visually leaking through the support wall
    -> constrain remote illumination to light that legitimately passes the aperture
```

The first implementation may use a mapped remote spotlight proxy plus an aperture/light-function restriction. A later renderer-specific solution may improve the aperture clipping if necessary.

### Acceptance

- shining at ordinary wall area outside the aperture never lights the remote side;
- shining through the aperture lights the destination side from the mapped direction;
- the beam does not appear to originate from behind the support wall;
- crossing while the flashlight is on does not cause one-frame double lighting or a light pop;
- turning the flashlight off removes both authoritative and remote lighting immediately.

---

## 5. Critical traversal defect — rapid enter/exit can place the player behind the support wall

### Observed symptom

Repeatedly moving back and forth through a portal can, with non-deterministic-looking timing, leave the player on the wrong side of the supporting wall / behind the wall rather than cleanly inside one of the two valid portal spaces.

### Severity

**Critical gameplay correctness bug.**

This defect takes priority over additional visual polish because it allows the player to escape the intended collision topology.

### Current contributing behavior

The original traversal prototype temporarily ignored the entire support primitive when the capsule was near the portal and appeared to fit the aperture. The exit-side path also kept support ignore active while `LastPlayerExit` considered the player to still be clearing the exit.

**2026-09-13 player repair candidate implemented; full issue matrix remains open.** `UInteriorPortalMovementComponent` now checks a swept capsule aperture interval on every movement submove, and explicit clearance state prevents reversal behind the exit before full clearance. Transfer occurs before subsequent floor queries. This also fixes the reproduced 3 Hz failure where entry-side simulation dropped the player below the destination floor before transfer. Actual-map repeated traversal and focused test evidence: [InteriorPortalPlayerTraversal.md](InteriorPortalPlayerTraversal.md). The remaining jump/edge/arbitrary-orientation visual matrix and Chaos body gates must not be inferred from these player tests.

Conceptually, the original prototype behavior was too close to:

```text
near valid aperture
    -> ignore entire support wall
    -> CharacterMovement runs
    -> restore later
```

rather than:

```text
inside legal swept aperture prism
    -> bypass only the portal opening
    -> immediately restore wall collision on lateral/invalid escape
```

The transfer path also performs a direct teleport placement rather than making collision correctness depend on a swept post-transfer movement step. Exit-overlap validation intentionally ignores the exit support component, which is necessary to pass through the aperture but increases the importance of a correct aperture-local state machine.

### Why the bug appears probabilistic

The defect depends on frame timing and input reversal:

```text
frame N: transfer commits; exit support is temporarily ignored
frame N+1: player reverses while still considered ClearingExit
frame N+2: CharacterMovement advances while the whole support is still ignored
frame N+3: collision is restored after the capsule may already be on the invalid side
```

Different frame rates, movement speeds and reversal timing therefore make the issue look random even though the underlying state transition is deterministic.

### Follow-up: slow entry / partial-crossing wall lock — 2026-09-13

User report at baseline `82e2651`: slow entry or stopping midway can leave the player unable to move. The aperture constraint returned zero movement in every direction when the current capsule footprint became slightly invalid. The native regression now exercises a rim contact followed by a 0.25 cm pose correction, then checks that retreat and motion toward the center remain available.

The correction permits non-worsening recovery against initially violated aperture edges while retaining strict acquisition/transfer checks and blocking deeper wall entry. Portal Automation and actual-map slow passage/pause/rim-retreat replay each passed 6/6. The corrected actual-map pose probe confirms 8 cm outward retreat, 10 cm movement toward center, blocked deeper entry and restored passage state. Regression evidence and the discarded direction-mutating probe are recorded in [InteriorPortalPlayerTraversal.md](InteriorPortalPlayerTraversal.md). Keep the broader jump/orientation/physics acceptance matrix open; the original centered continuous-traversal replay alone does not prove freedom from wall sticking.

### Required fix

P3/P4 must replace the broad wall-ignore prototype with an explicit traveller crossing state and a swept aperture-local collision gate.

Required player states:

```text
Outside
ApproachingEntry
IntersectingAperture
Transferred
ClearingExit
```

Required rules:

- at most one transfer commit per traveller per simulation interval;
- `ClearingExit` cannot immediately be treated as a new entry crossing;
- exit hysteresis must require real clearance before the portal is eligible again;
- support bypass remains valid only while the swept capsule occupies the legal portal prism;
- lateral escape from the aperture restores support collision immediately;
- any restore must resolve to a safe non-penetrating pose;
- repeated forward/backward input cannot accumulate stale ignore state;
- portal clear/replacement/destruction always restores support collision.

### Acceptance / regression tests

At minimum test:

```text
slow forward -> backward before fully clear
sprint forward -> immediate backward
hold alternating W/S at the plane
jump through -> reverse in air
approach aperture edge -> reverse/lateral strafe
30/60/120+ FPS
low and high CharacterMovement speeds
100 repeated A<->B traversals
```

Expected result:

- player never ends behind the support wall;
- player never exits through wall area outside the aperture;
- no ping-pong transfer loop;
- no permanent support-ignore state;
- no visible depenetration launch/teleport pop after collision is restored.

---

## 6. Implementation priority resulting from these observations

The newly observed defects change the immediate implementation order.

Recommended order from the current branch state:

```text
1. Keep P1 clipping regression coverage active.
2. Fix P3/P4 traversal-state + aperture-local collision safety.
3. Finish manual acceptance for player/first-person partial-crossing visuals.
4. Finish manual acceptance for the portal-aware flashlight path.
5. Return to P2 capture-pre-exposure normalization and finish direct-vs-portal exposure parity.
6. Complete PortalQuery segmented diagnostics and exploration weapon/projectile coverage, then continue rigid-body / dual-space physics work from the main plan.
```

Reason for moving traversal safety ahead of the remaining exposure work: an exposure mismatch is visually wrong, but support-wall escape breaks world topology and can invalidate later physics/query validation. The collision state must therefore become trustworthy before more advanced crossing behavior is layered on top.

---

## 7. Definition of closed for this defect log

An issue may be marked closed only when:

1. the implementation exists on the active portal branch;
2. relevant focused Automation tests are added where practical;
3. the corresponding PIE regression scenario passes;
4. no new workaround contradicts `LogicalPortalFrame`, frame-order, or PortalTraveller contracts in the main plan;
5. the result is recorded in the PR / validation notes.

Do not close an issue solely because a single screenshot looks correct.
