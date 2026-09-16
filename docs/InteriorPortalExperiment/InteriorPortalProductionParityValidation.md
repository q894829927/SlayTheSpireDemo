# Interior Portal — STEP 1B.14D-C Production TSR Parity Validation

Date: **2026-09-16**

State:

```text
STEP 1B.14A VISUAL-PARITY ISOLATION = PASS / OUTCOME B
STEP 1B.14B MAIN-VS-SECONDARY DIAGNOSTICS = PASS / ROOT CAUSE CLASSIFIED
STEP 1B.14C SECONDARY EYE ADAPTATION A/B = PASS
STEP 1B.14D EXACT PER-SUBMISSION COLOR SAMPLE OWNERSHIP = IMPLEMENTED, PUBLICATION ORDER HARDENED
STEP 1B.14D-C ATTEMPT 1 = FAIL / PIE SINGLE-FRAME FREEZE
STEP 1B.14D-C ATTEMPT 2A = PASS / FREEZE REGRESSION REMOVED
STEP 1B.14D-C ATTEMPT 2B = FAIL / IN-FLIGHT COLOR SAMPLE PUBLISHED BEFORE TONEMAP EXTRACTION
STEP 1B.14D-C ATTEMPT 3 = CODE FIX COMMITTED / USER BUILD + PIE REQUIRED
```

## Why this gate exists

The remaining portal darkness was already isolated upstream of aperture, stencil,
depth propagation and bounded main-pass composition. The decisive runtime A/B was:

```text
secondary EyeAdaptation OFF -> secondary PreExposure pinned near 1 -> remote full view nearly black
secondary EyeAdaptation ON  -> secondary PreExposure converges to the expected small HDR-domain value -> lit remote view returns
```

The accepted full-view TSR producer therefore preserves the game viewport EyeAdaptation
show flag. Each submitted portal HDR image also owns an `FColorSample`; secondary
Tonemap extraction writes the measured secondary PreExposure and the main compositor
uses that metadata for the `MainPreExposure / SecondaryPreExposure` conversion.

This gate validates the normal production path rather than introducing an alternate
renderer.

## Attempt 1 — parity observer froze PIE

The first parity probe registered a second world-scoped `FWorldSceneViewExtension`
and subscribed it to Tonemap for both main and secondary view families. Starting
`portal.StartProductionParityValidation` left PIE permanently displayed on one frame.

Classification:

```text
ATTEMPT 1 = FAIL
REGRESSION = SECONDARY VIEW-FAMILY OBSERVER INTERFERENCE / SINGLE-FRAME FREEZE
```

The fix was to make `InteriorPortalProductionParityValidation.cpp` main-view-only.
It now ignores non-main and additional view families. Secondary telemetry remains
owned by `portal.StartFullViewFamilyTSRSpike`.

## Attempt 2A runtime evidence — freeze regression fixed

With the corrected main-view-only observer, PIE continued rendering and accepting
movement after `portal.StartProductionParityValidation`.

Observed main telemetry advanced continuously from the first sample through more
than one thousand observations. Representative values:

```text
EyeAdaptation = 1
AA = AAM_TSR (4)
InvalidMainExposureFrames = 0
Main PreExposure = finite positive values around the expected 0.001x..0.004x range
```

The user also dumped the producer reports:

```text
PortalTSRSpike:
  Submitted=409
  Skipped=0
  LastExtractionFrame=4536
  CameraCuts=1
  ContinuousHistoryFrames=408
  Input=1920x900
  ObservedAA=4
  JitterObserved=1
  DepthFrame=4536
  DepthSource=1286x603
  DepthTarget=1920x900

PortalProductionParity:
  MainFrames=1609
  MainPre=0.00251182751
  Eye=1
  AA=4
  InvalidMain=0
  MainCuts=0
  Classification=MAIN_TELEMETRY_READY_CHECK_TSR_REPORT_AND_MANUAL_VISUAL_GATE
```

Therefore:

```text
ATTEMPT 2A = PASS
PARITY OBSERVER FREEZE REGRESSION = CLOSED
```

The secondary producer itself was rendering, extracting color/depth, running TSR,
carrying temporal jitter and preserving continuous history.

## Attempt 2B runtime evidence — black aperture was a publication-order bug

Despite healthy producer/main telemetry, the normal portal aperture appeared black.
`portal.CompositionDiagnostics 1` exposed the exact failure pattern.

For long runs of frames, composition produced:

```text
PortalComposition Subscribe ... RequestValid=1 HasTarget=1 HasDepthTarget=1
PortalComposition Execute ...
```

but **no `ComposeReady`** line.

Examples occurred continuously from frame 5122 onward. The request itself was valid,
the targets existed and the BeforeDOF callback executed, so this was not an aperture,
visibility, target-discovery or extension-subscription failure.

When the same request's `FColorSample` happened to have completed before the main
BeforeDOF callback, `ComposeReady` appeared immediately. Example:

```text
Frame=5233
MainPreExposure=0.00154885312
SecondaryPreExposure=0.00144923234
ExposureScale=1.06874037
Projective=1
ProjectiveValid=1
SecondaryDepthTextureValid=1
DrawQueued
```

Adjacent successful frames showed similarly sane values, including exposure scales
near `0.95..1.07`. This proves the remaining black result was not caused by an
extreme exposure multiplier.

### Root cause

Before this fix, `FTSRPortalProducer::SubmitFrame()` did this order on the game thread:

```text
1. Allocate a new FColorSample with PreExposure=0.
2. Queue the secondary FSceneViewFamily.
3. Immediately publish Request to FInteriorPortalViewExtension.
4. Later, secondary Tonemap runs and writes the exact PreExposure into FColorSample.
```

The main BeforeDOF compositor can run between steps 3 and 4. Its exact-ownership
contract intentionally rejects an uncompleted sample:

```text
TryGetExposureScale(...) == false
-> return original main SceneColor
```

The physical portal surface underneath is black, so repeatedly rejecting the
in-flight sample presents exactly as the observed black aperture.

This also explains why occasional `ComposeReady` frames existed: scheduling sometimes
allowed secondary Tonemap to populate the sample before main composition consumed it.

Classification:

```text
ATTEMPT 2B = FAIL
ROOT CAUSE = IN-FLIGHT REQUEST PUBLICATION / SAMPLE COMPLETION ORDER
```

## Attempt 3 code fix — publish only completed secondary submissions

Commit:

```text
63a7534fcf3d8067d2c2630f1364554f2a9db40a
portal: publish TSR request only after extraction
```

Source:

```text
Source/SlayTheSpireDemo/Interior/InteriorPortalFullViewFamilyTSRSpike.cpp
```

The producer no longer publishes the request immediately from `SubmitFrame()`.
Instead:

```text
1. Build the request and assign color/depth target identities.
2. Allocate the exact FColorSample.
3. Pass the request, sample and composition extension into the producer-owned Tonemap extraction extension.
4. Queue color extraction and secondary depth transport in that secondary RDG graph.
5. Write the measured secondary PreExposure into the exact FColorSample.
6. Publish that completed request from the render thread.
```

The main compositor therefore cannot receive a newly submitted request whose exact
sample is still `PreExposure=0`.

The fix deliberately does **not** restore the legacy one-frame CVar bridge and does
not add a fallback exposure multiplier. The exact sample remains the authority;
only its publication timing changed.

## Ownership after the fix

```text
PortalProductionParityValidation
    -> player main-view telemetry only

PortalFullViewFamilyTSRSpike / FPortalTSRExtractionExtension
    -> secondary renderer
    -> secondary Tonemap measurement
    -> color extraction
    -> depth transport
    -> exact FColorSample completion
    -> completed-request publication

FInteriorPortalViewExtension
    -> consumes only the latest published completed request at main BeforeDOF
```

## USER ACTION REQUIRED — rebuild and run Attempt 3

Pull/sync the branch and rebuild first. Then start PIE and run:

```text
r.AntiAliasingMethod 4
portal.FullViewFamilyTSRPrimaryFraction 0.67
portal.StartFullViewFamilyTSRSpike
portal.ProductionParityDiagnostics 1
portal.StartProductionParityValidation
```

First observe the normal aperture. The primary acceptance condition for this patch is:

```text
portal interior is no longer persistently black
```

Then enable composition diagnostics briefly:

```text
portal.CompositionDiagnostics 1
```

Hold the portal visible for about 2 seconds, then:

```text
portal.CompositionDiagnostics 0
portal.DumpFullViewFamilyTSRSpike
portal.DumpProductionParityValidation
```

### Attempt 3 telemetry acceptance

Expected:

```text
PortalTSRSpike framesSubmitted continues increasing
framesSkipped == 0 during the ordinary visible run
lastExtractionFrame continues increasing
observedAAMethod == 4
temporalJitterObserved == true
secondaryPreExposure is finite and positive

PortalProductionParity invalidMainExposureFrames == 0
mainAAMethod == 4
classification == MAIN_TELEMETRY_READY_CHECK_TSR_REPORT_AND_MANUAL_VISUAL_GATE
```

For the brief composition diagnostic window, once the first completed secondary
submission exists, visible ordinary frames should no longer show long sequences of:

```text
Subscribe
Execute
<no ComposeReady>
```

Instead `ComposeReady` / `DrawQueued` should occur consistently while the portal is
visible and the producer is successfully submitting.

## Manual visual acceptance

After the black-aperture publication bug is gone, continue the original movement gate:

```text
1. Static direct view through the portal.
2. Strong slant / grazing view.
3. Approach then retreat without crossing.
4. Turn until the portal leaves the viewport.
5. Turn back and hold.
6. Cross once, stop and look back.
```

Accept only if:

```text
portal interior brightness is materially consistent with direct destination viewing
no repeated black / near-black frames during ordinary visibility
no old exposure-domain frame after leave-and-return
no obvious brightness discontinuity during approach / retreat
no large exposure pop caused specifically by crossing
slant / grazing behavior preserves the accepted aperture and clipping behavior
```

Do not tune a constant portal gain to pass this gate.

## Failure routing after Attempt 3

```text
build fails
    -> repair the new extraction-extension ownership signature; do not change renderer policy

PIE freezes again
    -> capture the last 30-50 lines; the main-only parity fix must be re-audited independently

TSR report stops advancing
    -> secondary producer regression

ComposeReady still missing for long visible runs while TSR extraction advances
    -> audit completed-request publication visibility / request mutex ordering

ComposeReady is continuous but portal remains black
    -> inspect actual PortalTexture RGB contents / shader sampling next

portal is lit but materially darker despite sane exposure scale
    -> investigate Lumen / reflection / additional-family lighting history

static view matches but motion or leave-return flashes
    -> investigate target/sample lifetime and temporal reset next

only aperture boundary fails while remote image is correct
    -> return to aperture/depth/stencil integration, not exposure
```

## Gate after PASS

If Attempt 3 removes the black aperture and both telemetry plus manual visual checks
pass, proceed to:

```text
STEP 1B.14E — production merge / cleanup + focused regression
```

That gate should retain:

```text
secondary EyeAdaptation ownership
exact per-submission FColorSample metadata
completed-request publication ordering
additional-view-family isolation
```

Then retire the late-latched CVar bridge from normal validation instructions and run
the affected depth/stencil/TSR/near-grazing regressions once. Performance work such
as bounded secondary transport/view allocation follows only after that closure.
