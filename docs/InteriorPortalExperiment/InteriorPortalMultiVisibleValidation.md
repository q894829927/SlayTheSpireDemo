# Interior Portal — STEP 1B.14D-MV Multi-Visible Portal Gate

Date: **2026-09-16**

State:

```text
ROOT CAUSE CLASSIFIED
MV-A ENDPOINT-OWNED PRODUCER = IMPLEMENTED
USER BUILD + PIE REQUIRED
```

Prerequisite:

```text
STEP 1B.14D-C SINGLE-VISIBLE PRODUCTION TSR PARITY = PASS
```

## Runtime symptom

PIE visual evidence shows:

```text
both Blue and Orange portals visible at the same time
    -> one portal contains the expected remote scene
    -> the other portal remains the black physical portal surface

camera turns so that only one endpoint is visible
    -> the remaining portal renders normally
```

All previously validated single-visible dynamic checks remained normal:

```text
strong slant / grazing
approach / retreat
leave viewport / return
crossing
post-crossing look-back
reverse crossing
```

Therefore this gate is independent from the closed exposure/parity defect.

## Source root cause

The accepted single-visible `FTSRPortalProducer::SubmitFrame()` searches Blue then Orange and stops on the first visible candidate. Only one secondary `FSceneViewFamily` is therefore submitted per producer tick.

The existing `FInteriorPortalViewExtension` also owns one `TOptional<FInteriorPortalRenderRequest>`, so simply deleting that `break` would still cause one request to replace the other. The single-visible producer additionally owns one persistent ViewState, one temporal-history record and one secondary depth target. Reusing those objects for two different virtual cameras would corrupt TSR history.

Deleting the first-visible `break` is therefore explicitly rejected as an implementation strategy.

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

The new producer is intentionally separate from the already accepted single-visible producer so the new ownership model can be validated without destabilizing the closed 1B.14D-C path.

### Endpoint ownership

```text
Endpoint 0 (Blue)
    -> persistent TSR ViewState
    -> temporal-validity / camera-cut state
    -> publication generation
    -> exact FColorSample per completed submission
    -> Blue portal color target
    -> own R32F secondary-depth target
    -> own main BeforeDOF composition extension

Endpoint 1 (Orange)
    -> persistent TSR ViewState
    -> temporal-validity / camera-cut state
    -> publication generation
    -> exact FColorSample per completed submission
    -> Orange portal color target
    -> own R32F secondary-depth target
    -> own main BeforeDOF composition extension
```

Each endpoint therefore retains the already accepted exposure contract independently.

### Secondary extension isolation

The discovery work that closed Attempt 1 proved that arbitrary world-scoped extensions must not accidentally observe the portal additional view family. With two secondary families in the same world this becomes more important.

Each MV extraction extension is therefore bound to the exact endpoint `FSceneViewStateInterface*` and subscribes to secondary Tonemap only when:

```text
InView.Family->bAdditionalViewFamily == true
InView.State == ExpectedEndpointViewState
```

This prevents the Blue extraction extension from consuming the Orange family, and vice versa.

### Completed-request publication remains mandatory

Per endpoint:

```text
secondary Tonemap
    -> measure exact PreExposure
    -> queue color extraction
    -> queue matching depth transport
    -> complete exact FColorSample
    -> publish that endpoint's request
```

No in-flight `PreExposure=0` request is exposed to main composition.

### Visibility-generation guard

When one endpoint leaves the visible set:

```text
clear only that endpoint's main composition request
invalidate only that endpoint's temporal history
advance only that endpoint's publication generation
```

An older in-flight extraction compares its request generation to the current endpoint generation before publishing. It is therefore rejected after a visibility transition and cannot republish a stale request after the game thread has cleared it.

### VRAM policy

The user-observed discovery run already displayed `Video memory has been exhausted` while only the single-visible renderer was running.

MV-A therefore does **not** allocate two persistent full-resolution final scratch render targets. The two endpoint secondary render submissions share one sequential final scratch. Resources that must survive until main composition remain endpoint-owned:

```text
persistent TSR ViewState: per endpoint
portal color target: per endpoint
secondary R32F depth target: per endpoint
final renderer scratch: shared / sequentially reused
```

Correctness still takes priority. If UE render scheduling proves that the shared scratch requires additional synchronization, fix the scheduling rather than silently reducing visual fidelity.

## Commands

After pulling/rebuilding this branch, start PIE and run:

```text
r.AntiAliasingMethod 4
portal.FullViewFamilyTSRPrimaryFraction 0.67
portal.MultiVisibleDiagnostics 1
portal.StartMultiVisibleTSRSpike
```

`portal.StartMultiVisibleTSRSpike` stops the old `portal.StartFullViewFamilyTSRSpike` producer if it is running.

Optional main-view parity telemetry may remain enabled with:

```text
portal.ProductionParityDiagnostics 1
portal.StartProductionParityValidation
```

Do not run the old single-visible TSR producer at the same time.

## First runtime gate — static dual visibility

Move to the previously reproduced camera position where both Blue and Orange are visible at once.

Required visual result for >= 5 seconds:

```text
Blue aperture shows its remote scene
Orange aperture shows its remote scene
neither aperture remains black
neither endpoint steals the other endpoint's image
both images update continuously during camera motion
```

Then execute:

```text
portal.DumpMultiVisibleTSRSpike
```

Expected telemetry while both are visible:

```text
VisibleCount=2
VisibleMask=0x03
SubmittedMask=0x03
PublishedMask=0x03

E0 Submitted > 0
E1 Submitted > 0
E0 ExtractFrame advancing
E1 ExtractFrame advancing
E0 PreExposure finite positive
E1 PreExposure finite positive
E0 Completed > 0
E1 Completed > 0
```

Both endpoints should report `ObservedAA=4` and temporal jitter after a sustained run.

## Second runtime gate — visibility transitions

Exercise:

```text
both visible -> Blue only -> both visible
both visible -> Orange only -> both visible
```

On return, the reappearing endpoint must restart cleanly without a stale remote frame, incorrect exposure domain or cross-endpoint TSR history.

The returning endpoint is expected to record a camera cut because its history was intentionally invalidated while hidden. The endpoint that remained visible should preserve its own continuous history.

## Motion / crossing gate

With both visible where practical:

```text
lateral camera motion
approach / retreat
strong slant
```

Then cross one portal and inspect the pair again if map geometry permits.

Reject:

```text
cross-endpoint temporal ghosting
one endpoint using the other endpoint's exposure
one endpoint intermittently becoming the black fallback surface
stale frame after visibility return
endpoint identity swap after crossing
```

## Telemetry output

`portal.DumpMultiVisibleTSRSpike` writes:

```text
Saved/AutomationReports/PortalMultiVisibleTSRSpike.json
```

It includes:

```text
visibleEndpointCount
visibleEndpointMask
submittedEndpointMask
publishedEndpointMask
per-endpoint framesSubmitted / framesSkipped
per-endpoint last extraction frame
per-endpoint depth extraction frame
per-endpoint camera-cut count
per-endpoint continuous-history count
per-endpoint latest secondary PreExposure
per-endpoint observed AA / jitter
per-endpoint completed submission id
per-endpoint history generation
```

## Current claim boundary

MV-A is code-complete but **not runtime accepted** until the user's UE 5.8 build and PIE prove both endpoint render families can run in the same frame with the shared final scratch and independent endpoint histories.

No GitHub CI currently proves the UE build.

Portal-on-portal recursion remains outside this gate. If a remote secondary view itself sees a linked portal, recursion >= 2 remains a later renderer gate.

## Remaining steps after MV-A runtime PASS

```text
MV-B: validate static dual visibility + visibility transitions + endpoint telemetry
MV-C: promote the accepted multi-visible ownership into the normal production start path and run single-visible regression once
MV-D: VRAM/performance cleanup if the dual-view path materially exceeds the current budget
```

MV-D is conditional: correctness can close before optimization, but the existing VRAM-over-budget warning means performance cannot be ignored before this renderer is considered production-ready.

## Gate result

PASS only when two simultaneously visible top-level portals remain independently rendered and temporally stable without regressing the already accepted single-visible parity path.

After PASS, continue to production cleanup / focused regression, then resume secondary transport/view bounding and later recursion work.
