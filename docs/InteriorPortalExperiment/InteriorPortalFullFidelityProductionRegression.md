# Interior Portal — FullFidelity Production Cleanup + Focused Regression

Date: **2026-09-16**

State:

```text
FULLFIDELITY VISUAL CORRECTNESS GATES = ACCEPTED IN PIE
NATIVE PLAYER LIFECYCLE OWNERSHIP = IMPLEMENTED
NATIVE FULLFIDELITY BACKEND SEAM = IMPLEMENTED
CONSOLE COMMANDS = OPERATOR/DIAGNOSTIC ALIASES ONLY
HISTORICAL MULTIVISIBLE COMMAND DISPATCH IN NORMAL CONTROL FLOW = REMOVED
FOCUSED AUTOMATION:
  FullFidelity = PASS (user local UE5.8 run)
  MainViewOwnership = PASS (user local UE5.8 run)
  ColorSampleExposure = PASS (user local UE5.8 rerun after float-stable test fix)
FINAL NATIVE-SEAM BUILD + SHORT PIE SMOKE = USER ACTION REQUIRED
```

## 1. Cleanup boundary

The accepted renderer behavior is not being redesigned in this step. The goal is to remove experimental control-flow coupling from normal gameplay while preserving historical diagnostics for A/B and regression work.

The old lifecycle used console-command dispatch in normal gameplay:

```text
AInteriorPlayerController
 -> GEngine::Exec("portal.StartFullFidelityRenderer")
 -> stable console command
 -> GEngine::Exec("portal.StartMultiVisibleTSRSpike")
 -> accepted endpoint x recursion renderer
```

The production-facing lifecycle is now:

```text
AInteriorPlayerController
 -> InteriorPortalFullFidelityRenderer::ShouldOwnRendering(...)
 -> InteriorPortalFullFidelityRenderer::Start/Stop(World)
 -> InteriorPortalFullFidelityBackend::Start/Stop()
 -> accepted endpoint x recursion renderer
```

No normal player/render lifecycle code requires a portal console command string.

`portal.StartFullFidelityRenderer`, `portal.StopFullFidelityRenderer`, `portal.DumpFullFidelityRenderer`, and the older `portal.*MultiVisibleTSRSpike` commands remain only as operator/diagnostic aliases. The renderer implementation is still physically located in the historical `InteriorPortalMultiVisibleTSRSpike.cpp` translation unit; that filename is not a runtime dependency and may be renamed later as a purely mechanical cleanup.

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

Per `AGENTS.md`, this C++ lifecycle cleanup used the smallest relevant Automation set:

```text
Automation RunTests SlayTheSpireDemo.Interior.Portals.FullFidelity
Automation RunTests SlayTheSpireDemo.Interior.Portals.MainViewOwnership
Automation RunTests SlayTheSpireDemo.Interior.Portals.ColorSampleExposure
```

Observed locally by the user on 2026-09-16:

```text
SlayTheSpireDemo.Interior.Portals.FullFidelity       PASS
SlayTheSpireDemo.Interior.Portals.MainViewOwnership PASS
SlayTheSpireDemo.Interior.Portals.ColorSampleExposure PASS
```

The first `ColorSampleExposure` run exposed only a test-numerics defect. Production exposure logic was not changed. The accepted contract remains:

```text
ExposureScale = MainPreExposure / SecondaryPreExposure
RebasedSceneColor = Radiance * SecondaryPreExposure * ExposureScale
                  = Radiance * MainPreExposure
```

The test now uses an explicit `1e-6` float tolerance for that invariant and passed on rerun.

The FullFidelity prefix also covers the analytic-aperture and recursive-request contracts.

## 4. Final native-seam smoke

The final cleanup after the 3/3 Automation pass replaced the remaining stable-control -> `StartMultiVisibleTSRSpike` command dispatch with `InteriorPortalFullFidelityBackend` native calls. No renderer math, exposure logic, aperture ownership, recursion scheduling, or publication retirement logic changed.

Because this is still a C++ link/control-flow change, do one editor build and one short PIE smoke before calling the production cleanup fully closed.

Start PIE normally. Do **not** call any Start command.

Expected startup evidence includes:

```text
PortalFullFidelityRenderer: START through native lifecycle and backend APIs.
PortalMultiVisible: START. ...
```

A short smoke is sufficient:

1. one portal renders normally,
2. Blue and Orange can both appear in the viewport,
3. `RecursionDepth=2` still shows the nested portal,
4. one strong grazing angle remains stable,
5. one rapid visible -> offscreen -> visible snap does not expose the spiral,
6. no video-memory exhausted warning appears.

Finally run:

```text
portal.DumpFullFidelityRenderer
```

The command itself is only a diagnostic alias; the renderer must already be running before it is entered.

## 5. USER ACTION REQUIRED

Pull the latest branch, rebuild the UE5.8 editor target, then perform the short PIE smoke above.

The three focused Automation groups are already accepted and do not need to be rerun solely because the final backend seam removed command-string dispatch. If the build exposes a shared/link failure, fix that first; if PIE shows a visual regression, reopen only the reproduced subsystem.

GitHub currently has no CI check proving this final local C++ link step.

## 6. After PASS

After the native-seam build + short PIE smoke passes, treat the renderer as closed for the current single-pair FullFidelity scope.

Remaining work is optional/mechanical:

- optionally rename `InteriorPortalMultiVisibleTSRSpike.cpp` to a production-oriented filename,
- retain old spike/diagnostic commands only where they still provide useful A/B evidence,
- do not reopen exposure, aperture, recursion, or visibility algorithms without a new reproduced failure.
