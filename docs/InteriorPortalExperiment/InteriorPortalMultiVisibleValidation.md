# Interior Portal — STEP 1B.14D-MV Multi-Visible Portal Gate

Date: **2026-09-16**

State:

```text
STEP 1B.14D-C SINGLE-VISIBLE PRODUCTION TSR PARITY = PASS
MV-A ENDPOINT-OWNED PRODUCER = IMPLEMENTED
MV-B DUAL-VISIBLE RUNTIME VALIDATION = PASS
MV-C-A STABLE FULL-FIDELITY CONTROL SURFACE = IMPLEMENTED
MV-C-B EXCLUSIVE SYSTEM LIFECYCLE INTEGRATION = PASS
MV-C CLEAN VRAM BASELINE = PRIOR 161 MB OVER-BUDGET WARNING NOT REPRODUCED
CURRENT FOCUSED FOLLOW-UP = GRAZING FOREGROUND-DEPTH REGRESSION
MV-D BOUNDED SECONDARY WORK = DEFERRED UNLESS CLEAN EXCLUSIVE PROFILE REQUIRES IT
```

## Closed user-visible defect

The previously reproduced failure was:

```text
Blue + Orange visible simultaneously
    -> one endpoint rendered normally
    -> the other endpoint exposed the black physical portal surface
```

Source inspection showed that the old single-visible producer stopped on the first visible portal and the compositor owned only one replaceable request. It also had only one persistent TSR ViewState/history and one secondary depth target, so deleting the first-visible `break` would have corrupted temporal ownership.

MV-A introduced independent endpoint ownership instead of sharing that state.

## MV-A implementation

Commit:

```text
1c6b3262f286f88c222c37f702c1b617af10d23c
portal: add endpoint-owned multi-visible TSR spike
```

Source:

```text
Source/SlayTheSpireDemo/Interior/InteriorPortalMultiVisibleTSRSpike.cpp
```

Ownership is now:

```text
Endpoint 0 / Blue
    -> persistent TSR ViewState
    -> temporal-validity / camera-cut state
    -> publication generation
    -> exact FColorSample per completed submission
    -> Blue portal color target
    -> own R32F secondary-depth target
    -> own main BeforeDOF composition extension

Endpoint 1 / Orange
    -> persistent TSR ViewState
    -> temporal-validity / camera-cut state
    -> publication generation
    -> exact FColorSample per completed submission
    -> Orange portal color target
    -> own R32F secondary-depth target
    -> own main BeforeDOF composition extension
```

The two endpoint submissions share only the sequential final renderer scratch. Persistent temporal state, exposure metadata, portal color targets and depth transport remain endpoint-owned.

Each extraction extension is additionally tied to its exact endpoint `FSceneViewStateInterface*`, preventing the Blue Tonemap observer from consuming the Orange additional view family and vice versa.

Per endpoint the accepted publication contract remains:

```text
secondary Tonemap
    -> measure exact PreExposure
    -> queue matching color extraction
    -> queue matching depth transport
    -> complete exact FColorSample
    -> publish only that endpoint's completed request
```

A visibility-generation guard rejects stale in-flight publications after an endpoint leaves the visible set.

## MV-B runtime evidence — PASS

The user rebuilt the branch and ran:

```text
r.AntiAliasingMethod 4
portal.FullViewFamilyTSRPrimaryFraction 0.67
portal.MultiVisibleDiagnostics 1
portal.StartMultiVisibleTSRSpike
```

The PIE screenshot shows both simultaneously visible apertures containing their remote scenes. The previous one-black / one-lit failure is no longer present.

The runtime sequence also exercised multiple visibility states before returning to dual visibility:

```text
VisibleCount=1 / VisibleMask=0x01
VisibleCount=1 / VisibleMask=0x02
VisibleCount=0 / VisibleMask=0x00
VisibleCount=2 / VisibleMask=0x03
```

Decisive dual-visible telemetry:

```text
VisibleCount=2
VisibleMask=0x03
SubmittedMask=0x03
PublishedMask=0x03

E0:
  Submitted=403
  ExtractFrame=1689
  Cuts=5
  Continuous=398
  PreExposure=0.00198942167
  Completed=403

E1:
  Submitted=362
  ExtractFrame=1689
  Cuts=4
  Continuous=358
  PreExposure=0.00165191176
  Completed=362
```

Both endpoint extraction frames reach the same current render frame, both have finite positive independent PreExposure values, and both have completed submission ids. `PublishedMask=0x03` proves neither endpoint overwrote or suppressed the other endpoint's main-view composition request.

Classification:

```text
MV-B = PASS
DUAL VISIBLE ENDPOINT COVERAGE = PASS
PER-ENDPOINT COMPLETED PUBLICATION = PASS
PER-ENDPOINT TEMPORAL OWNERSHIP = PASS
VISIBILITY TRANSITION / RETURN = PASS
ONE-BLACK-PORTAL DISPLAY REGRESSION = CLOSED
```

## VRAM finding from MV-B

The original MV-B screenshot reported:

```text
Video memory has been exhausted (161.059 MB over budget)
Expect extremely poor performance.
```

Source audit then found that the accepted MV-B validation had been started while `RendererBackend=SceneCapture`, so the endpoint-owned producer could run while `AInteriorPortalSystem::RenderViews()` still executed the legacy SceneCapture renderer from `AInteriorPlayerController::UpdateCameraManager()`.

MV-C-B made the two renderer paths explicitly mutually exclusive. In the subsequent automatic FullFidelity PIE run, the user reported that the prior video-memory warning was gone. Therefore the 161.059 MB measurement is classified as a contaminated dual-renderer measurement, not a clean FullFidelity budget result.

This does **not** prove final production performance. It means MV-D is no longer an emergency correctness follow-up. Bounded secondary rendering should resume only from a clean exclusive-renderer profile if performance or memory still justifies it.

Do not reduce memory by sharing endpoint temporal ownership. Blue and Orange must retain independent ViewStates, exact FColorSamples and persistent target identities.

## MV-C-A — stable full-fidelity control surface

Commit:

```text
7ec91507a10cb5e8c67c7d2e6aaaf621dc5132b7
portal: add full-fidelity renderer control surface
```

Stable commands front the accepted implementation:

```text
portal.StartFullFidelityRenderer
portal.DumpFullFidelityRenderer
portal.StopFullFidelityRenderer
```

The historical `portal.StartMultiVisibleTSRSpike` remains available for A/B validation.

## MV-C-B — exclusive lifecycle integration — PASS

Implementation commits:

```text
81b91da9f688b9ed2fce153d1a8acdb52177a4c7
portal: add full-fidelity lifecycle opt-in

52ef41ad0d7e29cbe538b449009867975fb12e31
portal: own full-fidelity renderer lifecycle in player controller

03ab99361dde762047acd184f2ff57969629f508
portal: make full-fidelity renderer exclusive with legacy capture
```

### Lifecycle switch

`AInteriorPortalSystem` exposes:

```text
bUseFullFidelityRenderer = true
```

The serialized `RendererBackend=SceneCapture` value remains the compatibility/fallback backend. This avoids a UENUM/map migration during production promotion.

When both are true:

```text
bUseFullFidelityRenderer == true
RendererBackend == SceneCapture
```

`AInteriorPlayerController` starts `portal.StartFullFidelityRenderer` automatically and records the renderer as lifecycle-owned.

### Mutual exclusion

While the full-fidelity renderer is active, `UpdateCameraManager()`:

```text
keeps PortalSystem->UpdateTraversal(this)
keeps PlayerPresentation->Update(...)
DOES NOT call PortalSystem->RenderViews(this)
DOES NOT invoke UInteriorPortalFullSceneViewSubsystem::Render(...)
```

Therefore the legacy recursive SceneCapture renderer and older FullSceneView experiment cannot run alongside the endpoint-owned TSR renderer.

If `bUseFullFidelityRenderer` is disabled or `RendererBackend` is changed away from SceneCapture, the controller stops the full-fidelity renderer and returns to the existing legacy/backend-specific `RenderViews()` path.

Controller `EndPlay` also stops the lifecycle-owned renderer, so endpoint ViewStates/depth targets/final scratch are released on PIE/world teardown.

### Runtime acceptance — 2026-09-16

A subsequent PIE run required no manual start command and logged:

```text
PortalMultiVisible: START. Endpoints=2 PrimaryFraction=0.670 SharedFinalScratch=1 PerEndpointViewState=1 PerEndpointDepth=1.
PortalFullFidelityRenderer: START requested through endpoint-owned multi-visible TSR path.
PortalFullFidelityRenderer: lifecycle START from InteriorPlayerController; legacy RenderViews bypassed.
```

The user again observed both portals rendering and reported that the prior VRAM over-budget warning had disappeared. The renderer dump still reported the dual-visible endpoint masks as submitted/published.

Classification:

```text
AUTO LIFECYCLE START = PASS
LEGACY RENDERVIEWS BYPASS = PASS
DUAL-VISIBLE OUTPUT = PASS
CLEANER EXCLUSIVE VRAM BASELINE = PASS / PRIOR WARNING NOT REPRODUCED
MV-C-B = PASS
```

## Current focused follow-up

Promoting the accepted main-foreground depth policy exposed a separate grazing-angle regression where the biased physical portal Surface could be mistaken for foreground and reveal its default spiral material. That regression is tracked in:

```text
docs/InteriorPortalExperiment/InteriorPortalGrazingForegroundDepthRegression.md
```

It is not a rollback of MV-B/MV-C endpoint ownership. Keep the accepted automatic lifecycle and dual-visible producer while fixing the depth-reference split.

## MV-D — conditional bounded secondary work

If a later clean exclusive-renderer profile still shows unacceptable memory or GPU cost, continue with the deferred STEP 1B.13B class of work:

```text
1. measure persistent allocations attributable to each secondary TSR view
2. bound secondary render/output allocation to conservative portal coverage where feasible
3. preserve independent endpoint ViewStates
4. reduce full-frame transient/persistent surfaces rather than sharing temporal identity
5. re-run dual-visible correctness after each optimization
```

Keep the accepted 0.67 TSR primary fraction until profiling proves a different policy is necessary.

## Remaining distance

For the original visible defect "two portals on screen but one becomes black":

```text
SOLVED / MV-B PASS
```

For production lifecycle ownership:

```text
MV-C-B = PASS
```

For current correctness:

```text
GRAZING FOREGROUND-DEPTH REGRESSION = USER BUILD + PIE VALIDATION REQUIRED
```

For production performance:

```text
MV-D = CONDITIONAL ON CLEAN EXCLUSIVE PROFILE
```

Portal-on-portal recursion >= 2 remains a later feature gate and is not required to close the current top-level display consistency work.
