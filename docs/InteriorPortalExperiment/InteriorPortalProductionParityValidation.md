# Interior Portal — STEP 1B.14D-C Production TSR Parity Validation

Date: **2026-09-16**

State:

```text
STEP 1B.14A VISUAL-PARITY ISOLATION = PASS / OUTCOME B
STEP 1B.14B MAIN-VS-SECONDARY DIAGNOSTICS = PASS / ROOT CAUSE CLASSIFIED
STEP 1B.14C SECONDARY EYE ADAPTATION A/B = PASS
STEP 1B.14D EXACT PER-SUBMISSION COLOR SAMPLE OWNERSHIP = PASS
STEP 1B.14D-C SINGLE-VISIBLE PRODUCTION TSR PARITY = PASS
STEP 1B.14D-MV MULTI-VISIBLE PORTAL COMPOSITION = OPEN / SEPARATE GATE
```

## Accepted scope

This gate closes the production **single-visible-portal, recursion-level-0** TSR exposure/composition path.
It does not claim simultaneous composition of both linked portal endpoints and does not claim recursion >= 2.

Accepted ownership:

```text
player main FSceneView
    -> owns final player exposure / tone mapping

portal secondary FSceneViewFamily
    -> persistent temporal ViewState
    -> TSR + temporal jitter
    -> EyeAdaptation enabled
    -> post-TSR / pre-tonemap HDR extraction at Tonemap
    -> exact per-submission FColorSample::PreExposure

main BeforeDOF composition
    -> consumes only a completed request
    -> rebases with MainPreExposure / SecondaryPreExposure
```

No constant portal brightness gain is part of the accepted path.

## Failure history and fixes

### Attempt 1 — parity observer freeze

The first production-parity observer subscribed to both the player main family and the manually constructed
secondary additional family. Starting the validator froze PIE on one rendered frame.

Fix:

```text
PortalProductionParityValidation
-> main-view-only observer
-> secondary telemetry remains producer-owned
```

Result: later runs advanced for thousands of main frames with movement/input intact.

### Attempt 2 — in-flight FColorSample publication

The secondary producer rendered correctly, but the portal aperture could remain black. Composition diagnostics showed:

```text
Subscribe
Execute
<no ComposeReady>
```

The old order published a request while its exact `FColorSample` still had `PreExposure=0`. The main compositor correctly
rejected that unfinished sample and returned the original main SceneColor; the black physical portal surface underneath
then became visible.

Fix commit:

```text
63a7534fcf3d8067d2c2630f1364554f2a9db40a
portal: publish TSR request only after extraction
```

Accepted order:

```text
build request
-> queue secondary view
-> secondary Tonemap measures PreExposure
-> queue matching color/depth extraction
-> complete exact FColorSample
-> publish completed request
-> main BeforeDOF consumes it
```

The legacy late-latched CVar bridge is not restored.

## Attempt 3A — static/runtime evidence PASS

The corrected producer produced the following accepted evidence:

```text
PortalTSRSpike:
  Submitted=726
  Skipped=0
  LastExtractionFrame=1324
  CameraCuts=2
  ContinuousHistoryFrames=724
  ObservedAA=4
  JitterObserved=1
  DepthFrame=1324

PortalProductionParity:
  MainFrames=725
  Eye=1
  AA=4
  InvalidMain=0
```

A complete composition-diagnostic window contained:

```text
Subscribe    = 158
Execute      = 158
ComposeReady = 158
DrawQueued   = 158
ComposeSkip  = 0
Execute without ComposeReady = 0
```

The persistent black-aperture regression was therefore closed.

## Attempt 3B — dynamic visual matrix PASS

The user restarted PIE and repeated the dynamic matrix with the accepted TSR producer and main-only parity observer.
Reported results:

```text
strong slant / grazing: normal
approach then retreat: normal
leave viewport then return: normal
first crossing: normal
post-crossing look-back: normal
reverse crossing: normal
```

During the run, main telemetry continued advancing through at least 1800 samples with:

```text
EyeAdaptation=1
AA=4
finite positive Main PreExposure (representative values around 0.0013..0.0015)
no single-frame freeze recurrence
```

Classification:

```text
STEP 1B.14D-C = PASS
SINGLE-VISIBLE DYNAMIC EXPOSURE / TSR / COMPOSITION = ACCEPTED
```

## Newly exposed independent defect — simultaneous visibility

The same PIE run exposed a separate limitation:

```text
when both linked portals are simultaneously visible in the player main view
-> one portal shows the remote scene
-> the other can remain the black physical portal surface

when only one portal remains visible
-> that visible portal renders normally
```

This is not classified as an exposure regression. Current source explicitly has single-request ownership:

```text
FTSRPortalProducer::SubmitFrame
    -> iterate Blue / Orange candidates
    -> choose the first visible candidate
    -> break

FInteriorPortalViewExtension
    -> TOptional<FInteriorPortalRenderRequest> PublishedRequest
    -> each PublishRequest replaces the previous request
```

The TSR producer also owns only one `FSceneViewStateReference`, one temporal-history record and one secondary-depth target.
Therefore simply deleting the `break` would be incorrect: the two virtual cameras would alias temporal history and the
second request would overwrite the first.

This defect is moved to the dedicated `STEP 1B.14D-MV` multi-visible gate.

## Performance observation

The dynamic PIE screenshots also showed UE's VRAM warning:

```text
Video memory has been exhausted
```

Observed over-budget amounts were on the order of tens of MB. This did not invalidate the completed single-visible
correctness gate, but it is a constraint on the multi-visible implementation. Do not solve simultaneous visibility by
blindly duplicating every full-resolution transient resource. Secondary view/transport bounding remains a later
performance requirement.

## Closure boundary

The following are now accepted for one visible portal endpoint:

```text
secondary EyeAdaptation ownership
persistent secondary temporal history
TSR + temporal jitter
post-TSR / pre-tonemap HDR extraction
exact per-submission FColorSample metadata
completed-request publication ordering
main-view-only parity instrumentation
leave/return temporal reset behavior
crossing exposure continuity
strong-slant projective aperture behavior
```

Still open:

```text
STEP 1B.14D-MV simultaneous Blue + Orange visibility
recursion >= 2
secondary renderer / transport bounding and VRAM optimization
full later physics/recursion gates
```

Do not reopen exposure tuning for the simultaneous-visibility defect unless new evidence shows an exposure-domain failure.
