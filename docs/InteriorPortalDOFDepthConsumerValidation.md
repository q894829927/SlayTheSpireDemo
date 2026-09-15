# Interior Portal — STEP 1B.12C-B Real DOF Downstream Depth-Consumer Validation

Date: **2026-09-15**

State:

```text
STEP 1B.5 FULL TRANSFORMED VIEW-FAMILY = PASS
STEP 1B.6 HDR EXTRACTION = PASS
STEP 1B.7 MAIN-VIEW COMPOSITION = PASS
STEP 1B.8 SECONDARY->MAIN PRE-EXPOSURE REBASE = PASS
STEP 1B.9 PER-FRAME FULL SECONDARY PRODUCER = PASS
STEP 1B.10A PERSISTENT SECONDARY TAA HISTORY = PASS
STEP 1B.10B TSR / TEMPORAL JITTER / SCREEN PERCENTAGE = PASS
STEP 1B.11A PROJECTIVE APERTURE / GRAZING / VIEWPORT CLIP = PASS
STEP 1B.12A MAIN SCENEDEPTH FOREGROUND OCCLUSION = PASS
STEP 1B.12B SECONDARY DEPTH TRANSPORT + MAIN-VIEW REMAP = PASS
STEP 1B.12C-A MAIN SCENEDEPTH WRITE FEASIBILITY = PASS
STEP 1B.12C-B REAL MAIN DOF DEPTH CONSUMER = IMPLEMENTED / NOT YET BUILT OR RUN
```

## Goal

1B.12C-A proved that project-side public RDG code can write validated transported
remote device depth into the current main SceneDepth attachment while preserving
real main-view foreground geometry.

1B.12C-B now asks a stronger question:

```text
Does a real downstream UE post-process pass consume that mutated main SceneDepth
and therefore treat portal-interior opaque geometry as main-view depth?
```

The chosen consumer is UE's real cinematic Depth of Field pass because the portal
composition/depth write happens at `BeforeDOF`, and UE exposes an `AfterDOF`
post-processing extension point. This makes DOF the earliest clean downstream
consumer proof for the current architecture.

## Validation harness

A dedicated world scene-view extension is implemented in:

```text
Source/SlayTheSpireDemo/Interior/InteriorPortalDOFDepthConsumerValidation.cpp
```

It is intentionally separate from the production portal compositor.

When active it affects only the real player main view. It explicitly ignores
`bAdditionalViewFamily`, so the accepted secondary full-view producer still keeps
its own DOF disabled and does not bake a second DOF pass into PortalTexture.

The harness uses `FSceneView::OverridePostProcessSettings` to force a controlled
main-view cinematic DOF setup and enables the main `DepthOfField` show flag.

Controls:

```text
portal.DOFValidationFocalDistanceCm 150
portal.DOFValidationFstop 1.2
portal.DOFValidationSensorWidthMm 36
portal.DOFValidationDiagnostics 1
```

Commands:

```text
portal.StartDOFDepthConsumerValidation
portal.DumpDOFDepthConsumerValidation
portal.StopDOFDepthConsumerValidation
```

Report:

```text
Saved/AutomationReports/PortalDOFDepthConsumerValidation.json
```

## Why the A/B proves downstream depth consumption

The secondary renderer still supplies post-TSR / pre-tonemap HDR color with
secondary DOF disabled.

The main portal compositor writes this color at `BeforeDOF`. When
`portal.MainDepthPropagation=0`, the main DOF pass sees the original physical
main depth under the portal aperture (normally the host wall / portal-plane
surface).

When `portal.MainDepthPropagation=1`, 1B.12C-A replaces valid aperture pixels with
transported remote depth before DOF executes.

Therefore a fixed-camera A/B with identical DOF settings is:

```text
Propagation OFF
    Portal RGB is present
    DOF sees physical main depth under aperture

Propagation ON
    same Portal RGB path
    DOF sees transported portal-interior depth for opaque remote geometry
```

A meaningful change in blur/focus structure inside the portal while ordinary
main-view foreground/background DOF remains otherwise consistent is evidence that
the real downstream DOF pass consumed propagated depth.

This is stronger than another shader-side diagnostic because DOF is an actual UE
post-processing consumer after the write point.

## Runtime telemetry

`SetupView` periodically logs:

```text
PortalDOFValidation SetupView ...
FocalDistanceCm=...
Fstop=...
SensorWidthMm=...
AdditionalFamily=0
```

The real `AfterDOF` callback periodically logs:

```text
PortalDOFValidation AfterDOF ...
PassEnabled=...
SceneRect=...
FocalDistanceCm=...
Fstop=...
SensorWidthMm=...
```

The report records:

```text
setupViewFrames
afterDOFFrames
lastAfterDOFFrame
lastAfterDOFPassEnabled
focalDistanceCm
fstop
sensorWidthMm
```

## Validation procedure

Close Unreal Editor before rebuilding because this adds a new compiled source
file. Pull/build the branch, enter PIE, and start the already accepted secondary
producer and main-depth path:

```text
r.AntiAliasingMethod 4
r.DepthOfFieldQuality 2

portal.ProjectiveAperture 1
portal.DepthAwareComposition 1
portal.SecondaryDepthRemap 1
portal.CompositionDebugMode 0
portal.CompositionDiagnostics 1
portal.FullViewFamilyTSRPrimaryFraction 0.67
portal.StartFullViewFamilyTSRSpike

portal.MainDepthPropagation 0
portal.StartDOFDepthConsumerValidation
```

Keep the camera fixed and choose a portal view containing at least two remote
opaque surfaces at noticeably different depths.

If the default focal distance does not put the physical portal plane near focus,
adjust only:

```text
portal.DOFValidationFocalDistanceCm <value>
```

A strong validation setup is normally:

```text
portal.DOFValidationFstop 1.2
portal.DOFValidationSensorWidthMm 36
```

### Screenshot A — propagation OFF

```text
portal.MainDepthPropagation 0
```

Wait several frames for temporal settling and capture the portal.

### Screenshot B — propagation ON

Do not move the camera:

```text
portal.MainDepthPropagation 1
```

Wait several frames and capture again.

Expected:

1. Portal color/geometry remains the same accepted scene content.
2. Blur/focus distribution inside the aperture changes according to remote
   geometry depth when propagation is enabled.
3. Remote opaque surfaces at different depths should not all behave as one flat
   portal-plane depth surface.
4. Main foreground geometry that physically blocks the portal still obeys its own
   main depth and remains spatially coherent.
5. The sky/background may not provide transported raster depth and is not used as
   proof either way.
6. No D3D12/RDG/assert/resource-lifetime failure occurs.

Then run:

```text
portal.DumpDOFDepthConsumerValidation
portal.StopDOFDepthConsumerValidation
```

Restore any temporary quality setting afterward if desired.

## PASS criteria

PASS requires all of the following:

1. Build succeeds.
2. `PortalDOFValidation SetupView` reports the forced focal distance/f-stop/sensor
   values on the main family.
3. `PortalDOFValidation AfterDOF` executes repeatedly and reports
   `PassEnabled=1`.
4. Fixed-camera propagation OFF vs ON produces a depth-structured DOF difference
   inside the portal consistent with remote opaque geometry rather than one flat
   portal-plane depth.
5. Normal portal composition and real foreground occlusion do not regress.
6. No render-thread/RDG/D3D12/resource-lifetime failure occurs.

## What PASS proves

A PASS proves the following end-to-end chain:

```text
secondary opaque SceneDepth
    -> transport
    -> rigid main-depth equivalence
    -> main SceneDepth write at BeforeDOF
    -> real UE main-view DOF pass
    -> portal-interior depth affects downstream presentation
```

That directly closes one of the major reasons the portal previously looked like
an independent RGB layer rather than part of the main spatial scene.

## What PASS does NOT prove

It does not prove:

```text
stencil aperture identity
HZB rebuild / occlusion hierarchy integration
SSR / Lumen screen-trace integration
fog correctness
TSR-reconstructed remote depth
translucent remote depth
recursion >= 2
multiple simultaneous visible portal histories
production GPU/VRAM acceptance
Core Portal Fidelity Seal
```

## Next gate after PASS

After DOF acceptance, validate another downstream depth-dependent effect only if
it materially changes fidelity. Then harden aperture identity/stencil ownership
and portal-bounded resource/scissor behavior before recursion.
