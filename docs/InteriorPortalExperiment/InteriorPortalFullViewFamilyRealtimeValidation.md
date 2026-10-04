# Interior Portal — STEP 1B.9 Per-Frame Full View-Family Producer Validation

Date: **2026-09-15**

State:

```text
STEP 1B.5 FULL TRANSFORMED VIEW-FAMILY = PASS
STEP 1B.6 BEFORE-DOF HDR EXTRACTION = PASS
STEP 1B.7 STATIC MAIN-VIEW COMPOSITION = PASS
STEP 1B.8 SECONDARY->MAIN PRE-EXPOSURE REBASE = PASS
STEP 1B.9 PER-FRAME FULL SECONDARY PRODUCER = PASS
STEP 1B.10 PERSISTENT TEMPORAL HISTORY / CAMERA-CUT POLICY = NEXT
```

Implementation commit:

```text
4beb64e41a5d8e8d4545349fd4c985a45559629c
portal: add per-frame full view-family producer spike
```

## Purpose

STEP 1B.9 promotes the proven one-shot chain into a continuously updated, single-visible-portal feasibility path:

```text
player camera each frame
    -> rebuild transformed portal request
    -> standalone full FSceneViewFamily
    -> secondary BeforeDOF lit HDR extraction
    -> endpoint RGBA16F portal target
    -> dynamic Secondary/Main pre-exposure rebase
    -> ordinary player BeforeDOF aperture composition
```

The gate asks only whether portal content and aperture tracking update correctly while the player moves and looks around.

## Scheduling and synchronization contract

The spike is command-driven and registers `FWorldDelegates::OnWorldPostActorTick` for the active PIE/Game world. This gives the player/camera actors a chance to update first, then queues the transformed secondary renderer before the ordinary viewport render for the frame.

The per-frame path deliberately does **not** call `FlushRenderingCommands()`.

The secondary render graph is queued before the main viewport render graph, and the BeforeDOF extraction transitions the endpoint target back to external/SRV access before the main compositor samples it.

Blocking synchronization is restricted to:

- stopping the spike and releasing persistent renderer resources;
- a viewport-size change that requires recreating render targets.

This is important: a per-frame `FlushRenderingCommands()` would prove visual tracking while invalidating the performance/lifetime architecture being tested.

## Persistent state

The realtime driver owns:

- one persistent secondary `FSceneViewStateReference`;
- one persistent final-output scratch `RTF_RGBA16f` target;
- one persistent main `FInteriorPortalViewExtension` compositor;
- the endpoint's existing persistent `RTF_RGBA16f` portal target;
- a fresh extraction extension attached explicitly to each manually constructed additional view family.

For this gate:

```text
EyeAdaptation = off
TemporalAA = off
MotionBlur = off
ScreenPercentage = off
bCameraCut = true for every secondary frame
```

The secondary pre-exposure is therefore expected to remain near the already measured `1.0`; the latest renderer-thread measurement is carried into the next submission and the main compositor continues to apply:

```text
PortalRGB_mainDomain = PortalRGB_secondary
                     * MainPreExposure / SecondaryPreExposure
```

Temporal fidelity is intentionally deferred.

## Commands

Controlled setup:

```text
RendererBackend = SceneCapture
portal.CompositionDebugMode 0
portal.CompositionDiagnostics 1
portal.FullViewFamilyRealtimeDiagnostics 1
```

Start:

```text
portal.StartFullViewFamilyRealtimeSpike
```

Then move and rotate the player camera normally. The portal image and analytic aperture should track without requiring another run command.

Dump the current state without stopping:

```text
portal.DumpFullViewFamilyRealtimeSpike
```

Report:

```text
Saved/AutomationReports/PortalFullViewFamilyRealtimeSpike.json
```

Stop:

```text
portal.StopFullViewFamilyRealtimeSpike
```

Starting the realtime spike first clears the old one-shot `portal.RunFullViewFamilyMainCompositionSpike` compositor if it is still armed.

## Expected diagnostics

With realtime diagnostics enabled, a periodic line is emitted on the game thread:

```text
PortalRealtimeSpike Frame=...
Submitted=...
Endpoint=...
Player=(...)
Virtual=(...)
Bounds=(...)-(...)
SecondaryPreExposure=...
LastExtractionFrame=...
```

The existing main-view diagnostics should continue to emit:

```text
PortalComposition Subscribe
PortalComposition Execute
PortalComposition ComposeReady
PortalComposition DrawQueued
```

The `Bounds` values and virtual camera location should change as the player looks/moves. `LastExtractionFrame` should continue advancing rather than freezing at the frame on which the spike started.

## Runtime result — PASS

The controlled rerun completed without recurring assertion, stale-target crash, or per-frame game-thread flush. After ordinary movement and look input, the final report recorded:

```text
status = STOPPED
framesSubmitted = 1210
framesSkipped = 0
lastExtractionFrame = 3590
lastEndpointIndex = 0
targetSize = 1419 x 882
secondaryPreExposure = 1
playerLocation = (1100.111606, 720.645600, 120.150001)
virtualLocation = (1963.354417, 1000.111621, 120.150001)
projectedBounds = (0.206271, 0.143670) - (0.419089, 1.000000)
```

Main-view diagnostics continued to execute on consecutive frames with valid requests and targets:

```text
PortalComposition Subscribe
PortalComposition Execute
PortalComposition ComposeReady
PortalComposition DrawQueued
```

The observed `MainPreExposure` remained around `0.00158`, `SecondaryPreExposure` remained `1.0`, and the applied `ExposureScale` matched `MainPreExposure / SecondaryPreExposure` on consecutive frames.

The visible portal content and projected aperture changed with camera movement, while the report shows 1210 successfully submitted secondary frames and zero skipped frames. `lastExtractionFrame = 3590` proves that the BeforeDOF extraction callback continued advancing long after startup rather than remaining at the initial frame.

Therefore STEP 1B.9 passes the realtime producer tracking gate.

## Acceptance

PASS requires all of the following:

1. after `portal.StartFullViewFamilyRealtimeSpike`, moving/turning the camera updates the visible target-space portal content continuously;
2. the aperture follows the portal's current projected bounds rather than the frozen STEP 1B.7/1B.8 bounds;
3. `framesSubmitted` rises over time;
4. `lastExtractionFrame` continues to advance;
5. the main compositor remains active and samples the current endpoint HDR target;
6. no per-frame game-thread `FlushRenderingCommands()` is required;
7. no recurring assertion, resource lifetime crash, or stale-target use is observed during ordinary movement.

All seven conditions were satisfied by the controlled run above.

## Architectural consequence — STEP 1B.10

The next gate is no longer spatial tracking. The realtime producer already owns a persistent secondary `FSceneViewStateReference`, but STEP 1B.9 deliberately forces every secondary frame to:

```text
bCameraCut = true
AntiAliasingMethod = AAM_None
TemporalAA show flag = off
```

STEP 1B.10 should therefore validate temporal ownership and history continuity in this order:

1. keep the persistent secondary `FSceneViewStateReference` alive across frames;
2. change `bCameraCut` from always-true to a deterministic cut policy driven by first frame, endpoint changes, renderer-history invalidation and genuine discontinuities;
3. enable the project's intended temporal AA path for the secondary view;
4. preserve the existing transformed camera, clipping, BeforeDOF extraction and pre-exposure rebase unchanged;
5. verify that history converges while stationary and remains stable during ordinary camera motion;
6. explicitly reset history after portal relocation, endpoint switch, resolution change or teleport discontinuity;
7. defer recursion and depth/stencil continuity until temporal history is proven.

## Claim boundary

Still out of scope:

```text
secondary TAA/TSR acceptance
secondary motion-vector acceptance
perfect Lumen temporal convergence
main depth/stencil continuity
portal-bounded secondary renderer scissor
more than one simultaneously visible portal
recursion >= 2
production GPU cost
Core Portal Fidelity Seal
```
