# Interior Portal — STEP 1B.14D-C Production TSR Parity Validation

Date: **2026-09-16**

State:

```text
STEP 1B.14A VISUAL-PARITY ISOLATION = PASS / OUTCOME B
STEP 1B.14B MAIN-VS-SECONDARY DIAGNOSTICS = PASS / ROOT CAUSE CLASSIFIED
STEP 1B.14C SECONDARY EYE ADAPTATION A/B = PASS
STEP 1B.14D EXACT PER-SUBMISSION COLOR SAMPLE OWNERSHIP = PRESENT IN HEAD
STEP 1B.14D-C ATTEMPT 1 = FAIL / PIE SINGLE-FRAME FREEZE
STEP 1B.14D-C ATTEMPT 2 = IMPLEMENTED / SECONDARY OBSERVER REMOVED / USER PIE REQUIRED
```

## Why this gate exists

The remaining portal darkness was already isolated upstream of aperture, stencil,
depth propagation and bounded main-pass composition. The decisive runtime A/B was:

```text
secondary EyeAdaptation OFF -> secondary PreExposure pinned near 1 -> remote full view nearly black
secondary EyeAdaptation ON  -> secondary PreExposure converges to the expected small HDR-domain value -> lit remote view returns
```

The accepted full-view TSR producer now preserves the game viewport EyeAdaptation
show flag instead of forcing it off. Each submitted portal HDR image also owns its
exact `FColorSample`; the producer-owned Tonemap extraction writes the measured
secondary PreExposure into that sample and the main compositor uses the same sample
for the `MainPreExposure / SecondaryPreExposure` rebase.

The purpose of this gate is to validate the resulting production path during
static viewing, motion, leave/return and crossing without introducing another
renderer path.

## Attempt 1 runtime failure — 2026-09-16

The first parity probe registered a second world-scoped `FWorldSceneViewExtension`
and subscribed that extension to `Tonemap` for both:

```text
player main primary view
portal secondary primary additional view
```

The user reproduced this sequence:

```text
r.AntiAliasingMethod 4
portal.FullViewFamilyTSRPrimaryFraction 0.67
portal.StartFullViewFamilyTSRSpike
portal.ProductionParityDiagnostics 1
portal.StartProductionParityValidation
```

Immediately after `portal.StartProductionParityValidation`, PIE remained displayed
on one frame and no longer advanced. This was not an input-focus symptom: the
rendered viewport itself stopped advancing.

Classification:

```text
ATTEMPT 1 = FAIL
REGRESSION = SECONDARY VIEW-FAMILY OBSERVER INTERFERENCE / SINGLE-FRAME FREEZE
```

No telemetry or visual PASS may be claimed from that run.

## Historical constraint that applies here

The earlier full-SceneView work already established that a world-scoped
`FSceneViewExtension` can see manually constructed additional portal view families.
That work therefore added explicit additional-family isolation to prevent an
ordinary main-view extension from feeding work back into the secondary renderer.

The accepted per-frame and TSR producers subsequently proved continuous operation
without a per-frame game-thread `FlushRenderingCommands()` and with extraction
frames continuing to advance across long runs.

Attempt 1 violated the spirit of that isolation by layering a new world-scoped
Tonemap observer onto the already producer-owned secondary Tonemap chain.

## Corrected implementation — Attempt 2

Source:

```text
Source/SlayTheSpireDemo/Interior/InteriorPortalProductionParityValidation.cpp
```

The parity probe is now **main-view-only**.

It no longer:

```text
subscribes to secondary Tonemap
copies secondary SceneColor
registers a parity callback into an additional portal view family
attempts to infer secondary ownership from generic bAdditionalViewFamily state
```

Instead, the parity extension observes the authoritative player main view from
`PostRenderViewFamily_RenderThread` and immediately ignores any family satisfying:

```text
!bIsMainViewFamily
OR
bAdditionalViewFamily
```

Secondary telemetry remains where it already belongs: inside the accepted
`portal.StartFullViewFamilyTSRSpike` producer and its own Tonemap extraction hook.
That producer already owns the secondary `FSceneViewState`, temporal history,
post-TSR extraction, measured secondary PreExposure, observed AA method, jitter,
depth transport and exact `FColorSample`.

This creates an explicit ownership split:

```text
PortalProductionParityValidation
    -> player main-view observer only

PortalFullViewFamilyTSRSpike
    -> secondary renderer + secondary telemetry owner
```

There is no additional parity RDG pass in the secondary view family.

## Commands

Main observer:

```text
portal.ProductionParityDiagnostics 1
portal.StartProductionParityValidation
portal.DumpProductionParityValidation
portal.StopProductionParityValidation
```

Main report:

```text
Saved/AutomationReports/PortalProductionParityValidation.json
```

Secondary producer report:

```text
portal.DumpFullViewFamilyTSRSpike
Saved/AutomationReports/PortalFullViewFamilyTSRSpike.json
```

## Main-view classification

The parity report now intentionally classifies only the main side:

```text
INSUFFICIENT_MAIN_DATA
    -> fewer than 30 authoritative main-view observations

INVALID_MAIN_PREEXPOSURE_OBSERVED
    -> at least one authoritative main-view observation had non-finite / non-positive PreExposure

MAIN_TELEMETRY_READY_CHECK_TSR_REPORT_AND_MANUAL_VISUAL_GATE
    -> main-view ownership telemetry is coherent; secondary producer report and manual PIE remain required
```

This is narrower than Attempt 1 by design. It avoids pretending that a second
observer is needed to validate data the secondary producer already owns.

## USER ACTION REQUIRED — rebuild, then one focused PIE rerun

The frozen Attempt 1 session is invalid evidence. End that PIE session. If the
Editor is truly hard-frozen and cannot stop PIE, terminate the Editor process.
Do not try to dump or preserve the frozen run.

Pull/sync the corrected branch, rebuild, then run:

```text
r.AntiAliasingMethod 4
portal.FullViewFamilyTSRPrimaryFraction 0.67
portal.StartFullViewFamilyTSRSpike
portal.ProductionParityDiagnostics 1
portal.StartProductionParityValidation
```

First acceptance condition is now simply:

```text
PIE continues rendering and accepting movement after StartProductionParityValidation
```

If it freezes again, stop this gate immediately and capture the last 30-50 log lines;
do not continue the movement matrix.

If it remains live, perform:

```text
1. Hold a static direct view through the portal for ~3 seconds.
2. Move laterally into a strong slant / grazing view and hold briefly.
3. Approach the aperture, then retreat without crossing.
4. Turn until the portal is fully outside the viewport for ~2 seconds.
5. Turn back to the same portal and hold.
6. Cross the portal once, then stop and look back through it.
```

Then run:

```text
portal.DumpProductionParityValidation
portal.DumpFullViewFamilyTSRSpike
portal.StopProductionParityValidation
```

## Automated acceptance

Main report:

```text
mainViewFamilyFrames >= 30
invalidMainExposureFrames == 0
mainAAMethod == AAM_TSR
classification == MAIN_TELEMETRY_READY_CHECK_TSR_REPORT_AND_MANUAL_VISUAL_GATE
```

Secondary TSR report must independently remain consistent with the previously
accepted producer contract:

```text
framesSubmitted continues increasing
framesSkipped remains 0 for the ordinary run
lastExtractionFrame continues increasing
observedAAMethod == AAM_TSR
temporalJitterObserved == true
extractionInputSize == full output size
secondaryPreExposure is finite and positive
continuousHistoryFrames grows substantially
camera cuts remain sparse / explainable rather than occurring every frame
```

Because the corrected parity observer does not touch the secondary family, any
secondary regression now belongs to the producer itself rather than to validation
instrumentation.

## Manual visual acceptance

At the same camera / destination relationship:

```text
portal interior brightness is materially consistent with direct destination viewing
no one-frame black / near-black flash when the portal returns to view
no old exposure-domain frame appears after leave-and-return
no obvious brightness discontinuity during approach / retreat
no large exposure pop caused specifically by physical crossing
slant / grazing views retain the accepted aperture and clipping behavior
```

Do not tune a constant portal gain to pass this gate.

## Failure routing

```text
PIE freezes immediately after parity observer start
    -> parity instrumentation is still interfering; capture tail log and do not debug exposure yet

main PreExposure invalid
    -> audit main-view observer timing / player ViewState ownership

TSR report stops advancing without parity observer touching secondary
    -> audit the production TSR producer itself

TSR observedAAMethod != TSR or jitter disappears
    -> audit secondary AA / screen-percentage setup

secondaryPreExposure remains near 1 and visual returns dark
    -> audit production secondary EyeAdaptation policy / exposure history

telemetry is healthy but view still dark
    -> investigate Lumen / reflection / additional-family lighting history next

static view matches but motion or leave-return flashes
    -> investigate temporal-history reset / exact render-sample lifetime next

only portal boundary fails while full secondary image is correct
    -> return to aperture/depth/stencil integration, not exposure
```

## Gate after PASS

If both reports and the manual visual acceptance pass, the next renderer task is:

```text
STEP 1B.14E — production merge / cleanup + focused regression
```

That gate should:

```text
retain production secondary EyeAdaptation ownership
retain exact per-submission FColorSample exposure metadata
retain the additional-view-family isolation rule
retire the late-latched CVar bridge from normal validation instructions
run the affected depth/stencil/TSR/near-grazing regressions once
record single-layer visual parity as accepted
```

Only after 1B.14E should performance work such as bounded secondary transport/view
allocation resume. Recursion >= 2 and full physics remain later gates.
