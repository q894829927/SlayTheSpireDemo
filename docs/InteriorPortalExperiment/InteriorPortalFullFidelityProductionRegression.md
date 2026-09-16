# Interior Portal — FullFidelity Production Cleanup + Focused Regression

Date: **2026-09-16**

State:

```text
FULLFIDELITY VISUAL CORRECTNESS GATES = ACCEPTED IN PIE
NATIVE PLAYER LIFECYCLE OWNERSHIP = IMPLEMENTED
CONSOLE COMMANDS = OPERATOR/DIAGNOSTIC ALIASES ONLY AT PLAYER LIFECYCLE
HISTORICAL MULTIVISIBLE BACKEND COMMAND SEAM = RETAINED TEMPORARILY
FOCUSED AUTOMATION:
  FullFidelity = PASS (user local UE5.8 run)
  MainViewOwnership = PASS (user local UE5.8 run)
  ColorSampleExposure = NUMERIC TEST TOLERANCE FIX COMMITTED / RERUN REQUIRED
FOCUSED PIE REGRESSION = USER ACTION REQUIRED
```

## 1. Cleanup boundary

The accepted renderer behavior is not being redesigned in this step. The goal is to remove experimental control-flow coupling from normal gameplay while preserving historical diagnostics for A/B and regression work.

Before this cleanup the normal player lifecycle did this:

```text
AInteriorPlayerController
 -> GEngine::Exec("portal.StartFullFidelityRenderer")
 -> stable console command
 -> GEngine::Exec("portal.StartMultiVisibleTSRSpike")
 -> accepted endpoint x recursion renderer
```

The player controller therefore depended on command registration and command strings even though FullFidelity had already been accepted as the normal renderer path.

The new lifecycle is:

```text
AInteriorPlayerController
 -> InteriorPortalFullFidelityRenderer::ShouldOwnRendering(...)
 -> InteriorPortalFullFidelityRenderer::Start/Stop(World)
 -> production composition policy
 -> accepted endpoint x recursion backend
```

`portal.StartFullFidelityRenderer`, `portal.StopFullFidelityRenderer`, and `portal.DumpFullFidelityRenderer` remain as operator aliases. They are no longer called by `AInteriorPlayerController`.

The historical backend still lives in `InteriorPortalMultiVisibleTSRSpike.cpp`, and the stable control currently retains one internal command-dispatch seam to that translation unit. That seam is deliberately documented rather than hidden: removing it is a later mechanical backend-export cleanup and must not be mixed with renderer-correctness changes.

## 2. Production invariants already accepted

The current renderer must retain all of the following:

- automatic FullFidelity startup from the portal/player lifecycle,
- legacy SceneCapture mutually excluded while FullFidelity owns rendering,
- one or both endpoints visible in the same main view,
- exact per-submission `FColorSample` / PreExposure ownership,
- independent endpoint x recursion-level TSR history,
- analytic world ray / portal-plane aperture ownership at grazing incidence,
- real main-view foreground depth preserving the first-person flashlight/portal gun,
- `RecursionDepth=2` nested portal rendering without crossing first,
- render-thread-ordered publication retirement on fast visible/offscreen transitions,
- no return of the previous video-memory over-budget warning caused by running legacy capture in parallel.

## 3. Focused automated regression

Per `AGENTS.md`, this C++ lifecycle change requires one editor build and the smallest relevant Automation set.

Run:

```text
Automation RunTests SlayTheSpireDemo.Interior.Portals.FullFidelity
Automation RunTests SlayTheSpireDemo.Interior.Portals.MainViewOwnership
Automation RunTests SlayTheSpireDemo.Interior.Portals.ColorSampleExposure
```

Observed locally by the user on 2026-09-16:

```text
SlayTheSpireDemo.Interior.Portals.FullFidelity       PASS
SlayTheSpireDemo.Interior.Portals.MainViewOwnership PASS
SlayTheSpireDemo.Interior.Portals.ColorSampleExposure
    initial run FAIL at the radiance-invariance assertion
```

The `ColorSampleExposure` failure was classified as a test-numerics issue rather than a production exposure regression. The production contract remains:

```text
ExposureScale = MainPreExposure / SecondaryPreExposure
RebasedSceneColor = Radiance * SecondaryPreExposure * ExposureScale
                  = Radiance * MainPreExposure
```

For the exercised float values, the mathematically identical result can evaluate to approximately `0.20000002` instead of exactly `0.2`. The test previously relied on `FMath::IsNearlyEqual` without an explicit tolerance. It now uses a documented `1e-6` tolerance around the same invariant. Production `FColorSample::TryGetExposureScale()` was not changed. Rerun `ColorSampleExposure` after pulling the fix.

The FullFidelity prefix currently includes the focused analytic-aperture and recursive-request contract tests.

Do not rerun unrelated battle/card suites for this renderer-only cleanup unless the build or focused tests expose a shared-module failure.

## 4. Focused PIE regression

Start PIE normally. Do **not** call any Start command.

Expected startup evidence includes the FullFidelity lifecycle START log and the endpoint-owned renderer START log.

Run one compact sequence:

1. look through one portal normally,
2. put Blue and Orange in the same viewport,
3. use a strong grazing angle,
4. overlap the portal with the first-person flashlight while the flashlight remains physically in front,
5. with `RecursionDepth=2`, confirm the nested portal is visible before crossing,
6. rapidly snap the camera visible -> offscreen -> visible at least 10 times,
7. cross once and look back through the portal.

PASS requires:

```text
no black aperture
no default-spiral flash except the intentional recursion-limit surface
no one-visible/one-black dual-portal failure
no foreground flashlight being overwritten while in front of the portal plane
nested level-1 portal visible when geometrically in view
no stale-frame flash on offscreen return
no exposure pop specific to crossing
no video-memory exhausted warning caused by parallel legacy SceneCapture
```

Finally run:

```text
portal.DumpFullFidelityRenderer
```

For a dual-visible RecursionDepth=2 framing, the report should show the applicable endpoint bits published and level-1 submitted/completed work when the nested portal is actually visible.

## 5. USER ACTION REQUIRED

Pull the latest branch and rerun only:

```text
Automation RunTests SlayTheSpireDemo.Interior.Portals.ColorSampleExposure
```

If that passes, the focused Automation gate is fully green. Then run the compact PIE regression above.

This document must not be marked fully PASS solely because the source was committed. GitHub currently has no CI check proving the local UE5.8 C++/shader build.

## 6. After PASS

Once this regression passes, the renderer can be treated as closed for the current single-pair FullFidelity scope. Remaining cleanup should be mechanical rather than visual:

- export the endpoint x recursion backend as a native service and remove the last internal `StartMultiVisibleTSRSpike` command-dispatch seam,
- optionally rename the historical `*Spike.cpp` implementation file after the native service boundary exists,
- retain old spike/diagnostic commands only where they still provide useful A/B evidence,
- do not reopen exposure, aperture, recursion, or visibility algorithms without a new reproduced failure.
