# Interior Portal — FullFidelity Production Cleanup + Focused Regression

Date: **2026-09-16**

State:

```text
FULLFIDELITY VISUAL CORRECTNESS GATES = ACCEPTED IN PIE
NATIVE PLAYER LIFECYCLE OWNERSHIP = IMPLEMENTED
NATIVE FULLFIDELITY BACKEND SEAM = IMPLEMENTED
CONSOLE COMMANDS = OPERATOR/DIAGNOSTIC ALIASES ONLY
HISTORICAL MULTIVISIBLE COMMAND DISPATCH IN NORMAL CONTROL FLOW = REMOVED
FOCUSED AUTOMATION = 3/3 PASS
FINAL NATIVE-SEAM BUILD = PASS
FINAL NATIVE-LIFECYCLE PIE SMOKE = PASS
FINAL DUMP = PASS
SINGLE-PAIR FULLFIDELITY FUNCTIONAL DEVELOPMENT GATE = COMPLETE / VALIDATED / SEALED
```

Validated production baseline:

```text
6d514466639858c1fd903d0f9abf96d5aad02077
portal: finalize native full-fidelity lifecycle cleanup
```

Validation date:

```text
2026-09-16
```

The next authorized engineering plan is:

```text
docs/PortalPerformanceVRAMP1Plan.md
```

---

## 1. Cleanup boundary

This phase did not redesign accepted renderer math. Its purpose was to remove experimental control-flow coupling from normal gameplay while preserving historical commands for operator diagnostics and A/B investigation.

The old lifecycle routed production startup through command strings:

```text
AInteriorPlayerController
 -> GEngine::Exec("portal.StartFullFidelityRenderer")
 -> stable console command
 -> GEngine::Exec("portal.StartMultiVisibleTSRSpike")
 -> endpoint × recursion renderer
```

The accepted production-facing lifecycle is now:

```text
AInteriorPlayerController
 -> InteriorPortalFullFidelityRenderer::ShouldOwnRendering(...)
 -> InteriorPortalFullFidelityRenderer::Start/Stop(World)
 -> InteriorPortalFullFidelityBackend::Start/Stop()
 -> InteriorPortalMultiVisibleTSRPrivate::StartMultiVisible(World)
 -> accepted endpoint × recursion renderer
```

No normal player/render lifecycle code requires a portal console-command string.

`portal.StartFullFidelityRenderer`, `portal.StopFullFidelityRenderer`, `portal.DumpFullFidelityRenderer`, and the older `portal.*MultiVisibleTSRSpike` commands remain only as operator/diagnostic aliases.

The implementation still physically resides in the historical `InteriorPortalMultiVisibleTSRSpike.cpp` translation unit. That filename is not part of runtime semantics and may be renamed later as mechanical cleanup.

---

## 2. Accepted production invariants

The sealed functional baseline retains all of the following:

- automatic FullFidelity startup from normal player/portal lifecycle;
- legacy SceneCapture rendering is bypassed while FullFidelity owns rendering;
- one or both endpoints may be visible in the same main view;
- endpoint × recursion-level temporal histories remain independent;
- exact per-submission `FColorSample` / PreExposure ownership is preserved;
- secondary-to-main exposure rebasing remains accepted;
- analytic world-ray / logical portal-plane aperture remains accepted at grazing incidence;
- real main-view foreground depth preserves first-person foreground occluders;
- transported portal depth and accepted recursive composition remain functional;
- `RecursionDepth=2` can show the nested portal before physically crossing;
- render-thread-ordered publication retirement remains accepted for fast visible/offscreen transitions;
- fast visible -> offscreen -> visible no longer exposes the one-frame default spiral caused by synchronous publication clearing;
- production lifecycle no longer starts the legacy path and FullFidelity path in parallel;
- no video-memory exhausted warning was reported in the final short production smoke.

These are baseline contracts for later performance work. A performance change must not silently invalidate them.

---

## 3. Focused automated regression

Per project guidance, the lifecycle cleanup used the smallest relevant Automation set:

```text
Automation RunTests SlayTheSpireDemo.Interior.Portals.FullFidelity
Automation RunTests SlayTheSpireDemo.Interior.Portals.MainViewOwnership
Automation RunTests SlayTheSpireDemo.Interior.Portals.ColorSampleExposure
```

Observed locally by the user on 2026-09-16:

```text
SlayTheSpireDemo.Interior.Portals.FullFidelity        PASS
SlayTheSpireDemo.Interior.Portals.MainViewOwnership  PASS
SlayTheSpireDemo.Interior.Portals.ColorSampleExposure PASS
```

The first `ColorSampleExposure` run exposed only a test-numerics defect. Production exposure logic was not changed. The accepted invariant remains:

```text
ExposureScale = MainPreExposure / SecondaryPreExposure
RebasedSceneColor = Radiance * SecondaryPreExposure * ExposureScale
                  = Radiance * MainPreExposure
```

The focused FullFidelity coverage also includes accepted analytic-aperture and recursive-request contracts.

---

## 4. Final native-backend lifecycle cleanup

After the 3/3 Automation pass, the final cleanup removed the remaining production command dispatch and made the backend seam return the real producer startup result.

Final baseline commit:

```text
6d514466639858c1fd903d0f9abf96d5aad02077
portal: finalize native full-fidelity lifecycle cleanup
```

Important final behavior:

```text
AInteriorPlayerController
    ↓
InteriorPortalFullFidelityRenderer
    ↓
InteriorPortalFullFidelityBackend
    ↓
StartMultiVisible(World) -> bool
    ↓
FMultiVisibleProducer
```

The production startup path does not call:

```text
portal.StartMultiVisibleTSRSpike
portal.StopFullViewFamilyTSRSpike
portal.StopFullViewFamilyRealtimeSpike
portal.ClearFullViewFamilyMainCompositionSpike
```

through `GEngine::Exec(...)`.

Historical console commands remain manual diagnostics only.

---

## 5. Final Build + PIE smoke acceptance

The previously pending final native-seam validation has now been completed by the user.

Accepted result:

```text
SlayTheSpireDemoEditor Win64 Development Build = PASS
normal PIE startup without manual Start command = PASS
native FullFidelity startup = PASS
single-portal presentation = PASS
dual-portal presentation = PASS
RecursionDepth=2 nested portal = PASS
strong oblique/grazing view = PASS
rapid visible -> offscreen -> visible = PASS
no reported one-frame spiral regression = PASS
no reported video-memory exhausted warning = PASS
portal.DumpFullFidelityRenderer = PASS
```

Expected native startup evidence was observed sufficiently to accept the lifecycle path; no additional historical focused Automation rerun is required solely for this cleanup.

This closes the action item that previously read:

```text
FINAL NATIVE-SEAM BUILD + SHORT PIE SMOKE = USER ACTION REQUIRED
```

It is no longer pending.

---

## 6. Publication-retirement dependency

The accepted fast-camera fix is documented separately in:

```text
docs/InteriorPortalExperiment/InteriorPortalOffscreenPublicationRetirement.md
```

Its queue-order ownership model is now a functional baseline contract:

```text
advance generation
→ block stale extraction publication
→ retire old publication in render-queue order
→ preserve already-queued consumers
→ stale retirement cannot clear a newer generation
```

Later resource-lifetime work must extend this model rather than replace it with synchronous visibility-time clearing.

---

## 7. Bounded main-pass evidence already accepted

The existing bounded main-pass correctness evidence is recorded in:

```text
docs/InteriorPortalExperiment/InteriorPortalBoundedMainPassValidation.md
```

That record already proves the bounded main-pass path can preserve the accepted image in user PIE A/B testing.

It does **not** yet prove:

```text
production GPU-time benefit
dual-portal production evidence
recursion >= 2 production evidence
secondary transport/RT reduction
persistent VRAM reduction
```

Those remaining questions are intentionally carried into the Performance / VRAM P1 plan instead of reopening the old correctness gate.

---

## 8. Functional gate result

The current single-pair FullFidelity functional development gate is closed:

```text
FULLFIDELITY FUNCTIONAL DEVELOPMENT
= COMPLETE
= VALIDATED
= SEALED
```

This statement is limited to the accepted single-pair FullFidelity rendering scope. It does not claim that Portal physics, partial-body interaction, generic traveler handling or final performance optimization are complete.

Do not reopen exposure, aperture, recursion, publication retirement or native lifecycle work without a reproduced regression.

---

## 9. Authorized continuation

The next phase is not another FullFidelity correctness redesign.

Continue with:

```text
docs/PortalPerformanceVRAMP1Plan.md
```

That plan begins by separating:

```text
RequestedDepth
VisibleDepth
SubmittedDepth
AllocatedDepth
```

and by making resource lifetime, retirement, ownership and actual GPU-memory evidence explicit before reclaim behavior is changed.

The first implementation task remains:

```text
P1A-1 — structured resource baseline and Dump instrumentation
```

No Portal physics work should be mixed into that performance/lifetime phase.
