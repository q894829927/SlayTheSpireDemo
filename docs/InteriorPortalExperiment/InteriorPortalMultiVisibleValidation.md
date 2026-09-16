# Interior Portal — STEP 1B.14D-MV Multi-Visible Portal Gate

Date: **2026-09-16**

State:

```text
OPEN / ROOT CAUSE CLASSIFIED
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

`FTSRPortalProducer::SubmitFrame()` currently searches Blue then Orange and stops on the first visible candidate:

```text
for (Blue, Orange)
    if candidate is visible
        Entry = candidate
        Exit = paired endpoint
        EndpointIndex = candidate index
        break
```

Only one secondary `FSceneViewFamily` is therefore submitted per producer tick.

The main compositor is also single-request:

```text
FInteriorPortalViewExtension
    TOptional<FInteriorPortalRenderRequest> PublishedRequest

PublishRequest(Request)
    PublishedRequest = Request
```

A second request would overwrite the first even if the producer loop rendered both endpoints.

The producer additionally owns only one:

```text
FSceneViewStateReference SecondaryViewState
single temporal-history record
single SecondaryDepthTarget
```

Deleting the first-visible `break` is therefore explicitly rejected as an implementation strategy.

## Required ownership model

The production candidate must support at least the two linked top-level endpoints independently:

```text
Endpoint 0 (Blue)
    -> own persistent TSR ViewState
    -> own temporal validity / camera-cut state
    -> own exact FColorSample per completed submission
    -> own portal color target
    -> own secondary depth transport identity

Endpoint 1 (Orange)
    -> own persistent TSR ViewState
    -> own temporal validity / camera-cut state
    -> own exact FColorSample per completed submission
    -> own portal color target
    -> own secondary depth transport identity
```

The compositor must retain a small set of completed requests keyed by endpoint rather than one replaceable request.
A main BeforeDOF frame must compose every currently visible completed top-level request exactly once.

## Publication rules

The accepted STEP 1B.14D publication contract remains mandatory per endpoint:

```text
never publish in-flight PreExposure=0 metadata
secondary Tonemap measures exact exposure
matching color/depth extraction is queued
exact FColorSample is completed
only then publish/replace that endpoint's completed request
```

If one endpoint is temporarily not visible:

```text
remove only that endpoint's composition request
invalidate only that endpoint's temporal history
keep the other visible endpoint alive
```

A hidden endpoint returning after a visibility gap must restart with a deterministic camera cut rather than reuse stale
TSR history.

## Composition requirements

For two simultaneously visible non-overlapping apertures:

```text
Blue remote image visible inside Blue aperture
Orange remote image visible inside Orange aperture
neither endpoint remains the black fallback surface
outside both apertures main SceneColor remains unchanged
both requests retain their own PreExposure conversion
```

Depth/stencil/projective aperture behavior must remain request-local. The implementation must not use one portal's
projective mapping, depth transport or FColorSample for the other endpoint.

Portal-on-portal recursion remains outside this gate. If the remote secondary view itself sees a linked portal, recursion
>= 2 is still a later renderer gate.

## Performance constraint

The discovery run displayed:

```text
Video memory has been exhausted
```

with the process already tens of MB over the current budget. Multi-visible support therefore must avoid unnecessary
full-resolution duplication.

Correctness takes priority, but implementation should preferentially:

```text
reuse sequential scratch work when render-command ordering makes it safe
keep persistent histories per endpoint
allocate only endpoint-owned resources that must survive until main composition
preserve the existing 0.67 primary-resolution TSR policy
```

Do not reduce fidelity or introduce constant exposure gains to hide memory pressure.

## Acceptance matrix

### A. both visible, static

```text
both apertures show their correct remote scenes for >= 5 seconds
no endpoint intermittently turns black
no endpoint steals the other endpoint's image
```

### B. visibility transitions

```text
both visible -> Blue only -> both visible
both visible -> Orange only -> both visible
```

On return, the reappearing endpoint must not show a stale frame or incorrect exposure domain.

### C. motion

With both visible:

```text
lateral camera motion
approach / retreat
strong slant where both remain in frame when practical
```

No cross-endpoint temporal ghosting or exposure swapping is accepted.

### D. crossing

Cross one portal, turn around and obtain a view where both top-level endpoints can again be seen if map geometry allows.
The endpoint identities must remain deterministic after authority swaps.

## Telemetry required

Add endpoint-aware diagnostics sufficient to prove:

```text
visibleEndpointCount
submittedEndpointMask
publishedEndpointMask
per-endpoint last extraction frame
per-endpoint camera-cut count
per-endpoint continuous-history count
per-endpoint latest secondary PreExposure
per-endpoint completed submission id
```

The compositor diagnostic should log endpoint identity for each compose callback.

## Gate result

PASS only when two simultaneously visible top-level portals remain independently rendered and temporally stable without
regressing the already accepted single-visible parity path.

After PASS, continue to production cleanup / focused regression, then resume secondary transport/view bounding and later
recursion work.
