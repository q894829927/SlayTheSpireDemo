# Interior Portal — STEP 1B.10A Persistent TAA History Validation

Date: **2026-09-15**

State:

```text
STEP 1B.5 FULL TRANSFORMED VIEW-FAMILY = PASS
STEP 1B.6 BEFORE-DOF HDR EXTRACTION = PASS
STEP 1B.7 STATIC MAIN-VIEW COMPOSITION = PASS
STEP 1B.8 SECONDARY->MAIN PRE-EXPOSURE REBASE = PASS
STEP 1B.9 PER-FRAME FULL SECONDARY PRODUCER = PASS
STEP 1B.10A PERSISTENT SECONDARY TAA HISTORY = IMPLEMENTED / NOT YET BUILT OR RUN
```

Implementation commit:

```text
05be3cc7f6cd07a6443e6b7c31e14b418977f020
portal: add persistent TAA history policy to realtime full view spike
```

## Purpose

STEP 1B.9 proved asynchronous per-frame tracking, but deliberately forced every
secondary frame to be a camera cut with anti-aliasing disabled. STEP 1B.10A keeps
the already-persistent `FSceneViewStateReference`, enables `AAM_TemporalAA`, and
adds an explicit history-reset policy instead of allowing history to survive
structural discontinuities accidentally.

This is intentionally TAA-first. TSR remains a separate follow-up gate because
TSR adds its own screen-percentage/upscaling contract. The goal here is to prove
history ownership and camera-cut semantics before adding the extra TSR variables.

## Temporal ownership contract

One running realtime producer owns exactly one secondary view state for the
single currently visible endpoint proof. Ordinary movement and looking around do
not cut history.

A camera cut is forced when any of these conditions occurs:

```text
first visible frame / invalid history
visible endpoint changes
render-target size changes
entry logical portal frame changes
exit logical portal frame changes
portal becomes unavailable/hidden and later becomes visible again
```

The following are **not** camera cuts by themselves:

```text
ordinary player translation
ordinary player rotation
ordinary transformed virtual-camera motion
```

This allows TAA history to accumulate during normal portal viewing while still
preventing one endpoint or portal placement from inheriting another endpoint's
history.

## Renderer settings for this gate

When `portal.FullViewFamilyTemporalAA=1` before the producer starts:

```text
persistent FSceneViewStateReference = enabled
TemporalAA show flag = enabled
FSceneView::AntiAliasingMethod = AAM_TemporalAA
FSceneView::bCameraCut = policy-driven
EyeAdaptation = off
MotionBlur = off
ScreenPercentage = off / 1.0 legacy driver
secondary BeforeDOF HDR extraction = unchanged
main pre-exposure rebase = unchanged
```

The validated STEP 1B.8 exposure contract remains authoritative:

```text
PortalRGB_mainDomain = PortalRGB_secondary
                     * MainPreExposure / SecondaryPreExposure
```

## UE 5.8 public API audit

The implementation only uses public view fields/APIs already exposed in UE 5.8:

- `EAntiAliasingMethod::AAM_TemporalAA`;
- `FSceneView::AntiAliasingMethod`;
- `FSceneView::bCameraCut`;
- the existing persistent `FSceneViewStateReference`.

No renderer-private temporal-history object is accessed directly.

## Validation procedure

Use the same controlled map/setup as STEP 1B.9 and keep:

```text
RendererBackend = SceneCapture
```

Before starting the producer:

```text
portal.CompositionDebugMode 0
portal.CompositionDiagnostics 1
portal.FullViewFamilyRealtimeDiagnostics 1
portal.FullViewFamilyTemporalAA 1
portal.StartFullViewFamilyRealtimeSpike
```

Move/look normally for at least 10–20 seconds. Include slow lateral motion and
slow rotation while looking through high-contrast portal geometry; these motions
make temporal instability easier to see.

Then, **before stopping**, run:

```text
portal.DumpFullViewFamilyRealtimeSpike
```

Report:

```text
Saved/AutomationReports/PortalFullViewFamilyRealtimeSpike.json
```

Finally:

```text
portal.StopFullViewFamilyRealtimeSpike
```

## Expected telemetry

The JSON should include:

```text
temporalAAEnabled = true
temporalHistoryValid = true
cameraCutCount >= 1
continuousHistoryFrames > 0
lastCameraCut = false during an ordinary continuous run
lastCameraCutReason = "continuous history"
```

For a run with no portal relocation, endpoint switch, visibility loss or viewport
resize, `cameraCutCount` should normally stay at the initial cut while
`continuousHistoryFrames` grows every subsequent submitted frame.

Periodic diagnostics should contain:

```text
TemporalAA=1
CameraCut=0
CameraCuts=<small stable count>
ContinuousHistoryFrames=<steadily increasing count>
CutReason=continuous history
```

If `CameraCut=1` repeats every ordinary frame, the gate fails even if the picture
looks acceptable; temporal history would not actually be accumulating.

## Visual acceptance

PASS requires:

1. realtime portal tracking from STEP 1B.9 remains correct;
2. ordinary movement does not repeatedly trigger camera cuts;
3. `continuousHistoryFrames` grows substantially;
4. no recurring assertion/resource lifetime crash occurs;
5. slow motion does not show obvious persistent ghost trails or stale portal
   imagery caused by wrong history ownership;
6. the portal does not inherit stale history after an intentional structural
   discontinuity such as portal replacement or endpoint switch.

This gate does not require TSR yet and does not claim perfect final temporal
quality. It proves that secondary temporal history has a correct owner/reset
policy on the full-renderer path.

## Next gate after PASS

```text
STEP 1B.10B — TSR / temporal jitter + screen-percentage contract
```

Only after temporal history and TSR are accepted should the renderer move on to
near/grazing clipping hardening, depth/stencil continuity and recursion.

## Claim boundary

Still out of scope:

```text
TSR acceptance
secondary dynamic screen percentage
recursion >= 2
main depth/stencil continuity
portal-bounded renderer scissor
production GPU cost
Core Portal Fidelity Seal
```
