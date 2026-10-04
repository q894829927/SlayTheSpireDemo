# Interior Portal GPU Visual Automation

Date: **2026-09-15**

Status:

```text
EDITOR-ONLY GPU/PIE AUTOMATION HARNESS IMPLEMENTED /
LOCAL BUILD + FIRST EXECUTION STILL REQUIRED /
DOES NOT REPLACE MANUAL VISUAL ACCEPTANCE
```

Related renderer authority:

- `docs/InteriorPortalCurrentExecutionPlan.md`
- `docs/InteriorPortalFullFidelityImplementationPlan.md`
- `docs/InteriorPortalObservedIssues.md`
- `Source/SlayTheSpireDemo/Interior/InteriorPortalRenderer.*`
- `Source/SlayTheSpireDemo/Interior/InteriorPortalSystem.*`

The focused test is:

```text
SlayTheSpireDemo.Interior.PortalVisualGPU.CompositionPipeline
```

Implementation:

```text
Source/SlayTheSpireDemoTests/Private/InteriorPortalVisualGPUTests.cpp
```

This test is deliberately separate from `SlayTheSpireDemo.Interior.Portals` so the existing contract-test total is not silently reinterpreted as GPU visual evidence.

## Preconditions

Open the editor on:

```text
/Game/House/L_Interior_LivingKitchen
```

and stop any existing PIE/SIE session before launching the test. The automation does not load or save the map because the map may contain user-owned uncommitted edits that must remain untouched.

The test starts ordinary PIE, locates the authored `AInteriorPortalSystem`, temporarily forces a valid one-layer pair, creates a transient camera, and restores the previous backend, recursion depth, portal placed flags, view target and exposure CVars during cleanup.

## Automated probes

### 1. Aperture composition

The same pose is captured first with `CustomRenderPassSpike` and then with `CustomRenderPassCompositionSpike`.

The image readback verifies two properties:

```text
inside the analytic portal ellipse:
    composition produces a measurable image change

outside the ellipse but inside its projected bounding rectangle:
    pixels remain the main-view result
```

This is evidence for the current 1B.3 analytic-ellipse proof only. It does not claim a production stencil/depth aperture.

### 2. Fixed-exposure direct-vs-portal parity

For two deterministic camera poses the test compares the portal-composited result against a direct camera placed at the corresponding `BuildVirtualViewTransform` pose.

For this diagnostic only it temporarily fixes:

```text
r.EyeAdaptationQuality = 0
r.EyeAdaptation.PreExposureOverride = 1.0
```

The test reports inner-aperture RGB mean-absolute error and relative luminance error. This isolates the composition/radiometric path from time-varying auto exposure. It does not prove the later automatic-exposure bright-to-dark/dark-to-bright acceptance matrix.

### 3. Current-frame synchronization

The camera moves from pose A to pose B. The B portal view is requested immediately after the new CRP request is submitted, then captured again after settling.

When the authored scene provides enough visual separation between A and B, the test requires the first B result to be closer to direct pose B than to direct pose A. This is intended to catch a one-frame-old external CRP target being consumed by the `BeforeDOF` composition callback.

If the selected authored region is visually too uniform to distinguish A from B, the test reports the synchronization metric as inconclusive rather than treating weak image evidence as a PASS.

## Evidence output

Runtime artifacts are intentionally written under `Saved/` and must not be committed:

```text
Saved/AutomationReports/PortalVisualGPU/BaselineA.png
Saved/AutomationReports/PortalVisualGPU/CompositionA.png
Saved/AutomationReports/PortalVisualGPU/DirectA.png
Saved/AutomationReports/PortalVisualGPU/PortalBFirst.png
Saved/AutomationReports/PortalVisualGPU/PortalBSettled.png
Saved/AutomationReports/PortalVisualGPU/DirectB.png
Saved/AutomationReports/PortalVisualGPU/metrics.json
```

`metrics.json` records aperture, fixed-exposure parity and synchronization metrics.

## Acceptance boundary

This harness may establish repeatable GPU regression evidence for the STEP 1B.3 composition path. It does **not** replace manual PIE judgment for:

```text
Lumen GI / reflections
translucency and fog
TAA / TSR shimmer
portal-edge quality at grazing angles
crossing-frame pop
bright -> dark and dark -> bright adaptation behavior
subjective final visual continuity
```

The renderer remains `STEP 1B.3 PARTIAL` until its existing manual/renderer acceptance gates are satisfied. No Core Portal Fidelity seal is implied by this test.

## Required validation after implementation

Because this change adds Editor-only C++ Automation, run the repository-standard sequence on the final HEAD:

1. Generate Project Files.
2. Development Editor Build.
3. Run `SlayTheSpireDemo.Interior.PortalVisualGPU.CompositionPipeline` with `L_Interior_LivingKitchen` already open and no PIE session active.
4. Inspect the Automation result plus `Saved/AutomationReports/PortalVisualGPU/metrics.json` and screenshots.

Do not record PASS until those steps have actually completed on UE 5.8.
