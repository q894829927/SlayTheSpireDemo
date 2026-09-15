# Interior Portal — Full SceneView Lit Spike

Date: **2026-09-15**

Branch: **`portal/full-fidelity-p1`**

Status:

```text
STEP 1B.5 IMPLEMENTED /
LOCAL UE 5.8 BUILD REQUIRED /
GPU LIT RESULT NOT YET ACCEPTED /
NO CORE SEAL CLAIM
```

Related evidence:

- `docs/InteriorPortalCRPBaseColorDiagnostic.md`
- `docs/InteriorPortalCurrentExecutionPlan.md`
- `Source/SlayTheSpireDemo/Interior/InteriorPortalRenderer.h/.cpp`
- `Source/SlayTheSpireDemo/Interior/InteriorPortalFullSceneViewSubsystem.h/.cpp`
- `Source/SlayTheSpireDemo/Interior/InteriorPlayerController.cpp`

---

## 1. Why this spike exists

The STEP 1B.4 BaseColor diagnostic established a narrow but useful boundary:

```text
transformed view / target-space geometry        works for the tested view
Depth/BasePass                                  works
BaseColor output                                works
portal HDR target receives scene data           works
PortalTexture + analytic aperture composition   works
current DepthAndBasePass lit SceneColor         insufficient
```

The next question is therefore not whether the portal transform or RenderTarget is alive. The next question is whether a **normal full UE scene renderer**, driven by the same transformed portal camera, can produce a lit HDR secondary view before the project pays the cost of a renderer-private depth/stencil integration.

STEP 1B.5 is intentionally the smallest discriminating experiment for that question.

---

## 2. Implementation

The new opt-in path is owned by:

```text
UInteriorPortalFullSceneViewSubsystem
```

and is driven after the existing `AInteriorPortalSystem::RenderViews()` call from `AInteriorPlayerController::UpdateCameraManager()`.

The path reuses the already-proven immutable `FInteriorPortalRenderRequest` transform/projection builder. For the first visible endpoint it:

```text
player camera + portal pair
    -> FInteriorPortalRenderRequest::Build
    -> transformed virtual location / rotation / projection
    -> require existing oblique exit clip to encode successfully
    -> standalone additional FSceneViewFamily
    -> one FSceneView at the transformed camera
    -> IRendererModule::BeginRenderingViewFamily
    -> existing portal RGBA16f RenderTarget
    -> existing BeforeDOF analytic aperture composition
    -> player main view keeps final display/exposure authority
```

Unlike the CRP `DepthAndBasePass` path, `IRendererModule::BeginRenderingViewFamily` sends the view family through the normal scene renderer rather than requesting only the CRP depth/base subset.

This first spike deliberately uses no `FSceneViewStateInterface`. Temporal AA, motion blur and final-display post-process transforms are disabled. The purpose is to isolate **spatial + lit renderer feasibility**, not to solve temporal quality at the same time.

---

## 3. Runtime controls

The feature is disabled by default.

```text
portal.FullSceneViewSpike 0
    disabled

portal.FullSceneViewSpike 1
    render one transformed full SceneView into the portal HDR target
    and compose it through the existing BeforeDOF aperture path
```

Optional metadata:

```text
portal.FullSceneViewDiagnostics 1
```

writes:

```text
Saved/PortalFullSceneViewDiagnostics.json
```

The diagnostic records the renderer hook, endpoint, target size, clip policy, disabled temporal/display stages and the still-unimplemented depth/stencil/scissor boundaries.

`Saved/` output is runtime evidence only and must not be committed.

---

## 4. Deliberate scope limits

STEP 1B.5 does **not** attempt to solve all renderer fidelity at once.

Current scope:

```text
one visible portal endpoint
one transformed secondary view
one render layer
full scene renderer feasibility
existing oblique exit-plane projection
existing HDR RenderTarget
existing BeforeDOF analytic aperture composition
```

Explicitly deferred in this spike:

```text
portal recursion >= 2
persistent portal ViewState / temporal history
TAA / TSR
motion-vector validation
Lumen history parity
exposure adaptation parity
main depth continuity
main depth-stencil / stencil aperture
true portal scissor
portal-bounded renderer visibility work
production performance acceptance
```

Both portal actors' primitive components are hidden from the secondary view so this first test cannot accidentally become an uncontrolled recursive portal render.

---

## 5. Composition isolation

The existing `FInteriorPortalViewExtension` is world-scoped, so a newly created standalone secondary `FSceneViewFamily` can also be visible to that extension.

STEP 1B.5 therefore adds an explicit guard:

```text
if View.Family is AdditionalViewFamily
    do not register BeforeDOF portal composition
```

This prevents the secondary portal renderer from sampling the portal target and compositing it back into itself. The composition request remains intended for the ordinary player/main view family only.

---

## 6. Exposure / output policy for this experiment

The secondary full-render family keeps ordinary scene lighting enabled, but disables final display-domain stages for this feasibility test:

```text
TemporalAA      OFF
MotionBlur      OFF
EyeAdaptation   OFF
LocalExposure   OFF
Tonemapper      OFF
ColorGrading    OFF
Bloom           OFF
```

This is intentional. The target should remain an HDR/linear intermediate suitable for the already-proven pre-tonemap `BeforeDOF` composition boundary. The player main view remains the only intended final display-domain authority.

Do not interpret this first output as final exposure or temporal parity.

---

## 7. Required local validation

The connected GitHub environment cannot run the user's local UE 5.8 installation. Therefore this commit is **implementation evidence only**, not a build or GPU pass.

First regenerate project files:

```powershell
& "E:\Unreal engine\UE_5.8\Engine\Binaries\ThirdParty\DotNet\10.0\win-x64\dotnet.exe" `
  "E:\Unreal engine\UE_5.8\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.dll" `
  -ProjectFiles `
  -project="E:\UE_DEMO\SlayTheSpireDemo\SlayTheSpireDemo.uproject" `
  -game -engine -2022
```

Then compile:

```powershell
& "E:\Unreal engine\UE_5.8\Engine\Build\BatchFiles\Build.bat" `
  SlayTheSpireDemoEditor Win64 Development `
  -Project="E:\UE_DEMO\SlayTheSpireDemo\SlayTheSpireDemo.uproject" `
  -WaitMutex -FromMsBuild -2022 -architecture=x64
```

Do not claim the spike is build-valid until both commands actually pass.

For the first GPU run, keep the test narrow:

```text
portal.FullSceneViewSpike 1
portal.FullSceneViewDiagnostics 1
portal.CompositionDebugMode 0
```

Use the same portal pair / camera pose that produced the successful Mode 3 BaseColor image when practical.

---

## 8. What counts as a useful result

### Result A — clearly lit portal content

If the portal now contains recognizable lit target-space geometry with lighting/shadow/material response rather than the previous black `SceneColorNoAlpha` result, then the main feasibility question passes:

```text
same transformed portal camera
    + normal full FSceneViewFamily renderer
    -> usable lit secondary image
```

That would justify keeping project-side composition and moving next to:

```text
main depth/stencil continuity
true aperture/scissor
persistent independent portal histories
Lumen/temporal parity
recursion
exposure parity
performance
```

### Result B — base/direct lighting appears but Lumen/temporal features do not

This is still useful positive evidence. The first spike is intentionally stateless and has no persistent `FSceneViewStateInterface`; history-dependent renderer features are not yet a pass/fail requirement.

### Result C — portal remains black

Do not immediately conclude that the full renderer approach is impossible. The next diagnostics would distinguish:

```text
secondary view family actually submitted?
render target resolved?
show-flag/output-domain mismatch?
command ordering / external target ownership?
oblique projection invalid for full renderer?
composition sampling the expected full-render target?
```

Mode 3 remains the retained comparison path. If Mode 3 still shows BaseColor while STEP 1B.5 is black, geometry/RT/aperture are still independently known-good and the investigation remains in the full-render output boundary.

---

## 9. Acceptance state

Current classification at commit time:

```text
STEP 1B.4 CRP BaseColor diagnostic            PASS FOR TESTED VIEW
STEP 1B.5 source implementation                PRESENT
STEP 1B.5 UE 5.8 compile                       USER VALIDATION REQUIRED
STEP 1B.5 full-render GPU submission            USER VALIDATION REQUIRED
STEP 1B.5 lit portal visual result              USER VALIDATION REQUIRED
main depth/stencil/scissor                      NOT IMPLEMENTED
persistent temporal/Lumen histories             NOT IMPLEMENTED
Core Portal Fidelity Seal                       OPEN
Full Physics Fidelity Seal                      DEFERRED
```

Do not advance Step 2 or Step 3 from this record. STEP 1 remains the active delivery until the renderer candidate passes its visual/performance matrix.
