# Interior Portal — STEP 1B.12B Secondary Depth Transport + Main-view Remap Validation

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
```

## Goal

1B.12A proved that the main BeforeDOF compositor can read the real player
SceneDepth and preserve ordinary foreground objects in front of the physical
entry portal plane.

1B.12B is deliberately narrower than a main depth-buffer write. It proves that
the same full secondary renderer sample can transport its remote SceneDepth back
to project code and that this device depth already has an exact main-view depth
interpretation for the conceptual geometry seen through the portal.

It does **not** yet mutate the renderer's main SceneDepth attachment.

## Why secondary device Z can be reused as conceptual main-view device Z

`InteriorPortalMath` uses a rigid portal mapping:

```text
P_exit = ExitOrigin + R * (P_entry - EntryOrigin)
C_exit = ExitOrigin + R * (C_main  - EntryOrigin)
```

where `R` is a normalized rotation and the virtual camera orientation is rotated
by the same `R`.

For any real exit-side point `P_exit`, transform it conceptually back through the
inverse portal into the entry-side virtual continuation `P_entry_virtual`.
Because both the camera and the point use the same rigid transform, their
camera-relative coordinates are identical:

```text
ViewSecondary(P_exit) == ViewMain(P_entry_virtual)
```

The secondary spike also uses the player's exact projection matrix. Therefore:

```text
SecondaryDeviceZ(P_exit)
    == ConceptualMainDeviceZ(P_entry_virtual)
```

No brightness control, arbitrary depth offset, or distance approximation is
required for this remap proof.

## Depth extraction path

The accepted 1B.10B secondary renderer still extracts post-TSR / pre-tonemap HDR
SceneColor at the `Tonemap` extension slot.

1B.12B additionally requests the same secondary view's `SceneDepth` at that
callback and copies it into a persistent transient `R32F` render target.

Important claim boundary:

```text
secondary color = post-TSR display-resolution linear HDR
secondary depth = current-frame primary-resolution SceneDepth
                  point-resampled into display-size R32F
```

The depth surface is **not TSR-reconstructed**. The point upsample only gives the
main compositor a one-to-one normalized-UV resource for this feasibility proof.
It does not manufacture temporal depth information that UE's TSR did not expose.

The existing TSR report records:

```text
lastDepthExtractionFrame
secondaryDepthSourceSize
secondaryDepthTargetSize
secondaryDepthFormat
```

and periodic `PortalTSRSpike` diagnostics include:

```text
DepthFrame=...
DepthSource=...x...
DepthTarget=...x...
```

## Composition proof

`FInteriorPortalRenderRequest` carries both external resources:

```text
PortalRenderTarget       # accepted HDR color
PortalDepthRenderTarget  # 1B.12B R32F device depth
```

The main BeforeDOF compositor can consume the depth target without changing
normal portal color.

Runtime switch:

```text
portal.SecondaryDepthRemap 0  # normal accepted path; no 1B.12B depth diagnostic
portal.SecondaryDepthRemap 1  # enable transported-depth remap proof
```

Debug mode 5 automatically requests the remap proof:

```text
portal.CompositionDebugMode 5
```

Inside the projective portal aperture:

```text
cyan    = valid transported remote geometry whose remapped depth is at/behind
          the physical entry portal plane
magenta = valid transported depth that unexpectedly maps in front of the entry
          portal plane; this is a failure signal except for isolated tolerance-
          scale boundary pixels
yellow  = no rasterized remote depth sample (sky/background/transport miss)
```

Outside the aperture, ordinary main SceneColor remains visible.

`portal.CompositionDiagnostics 1` reports:

```text
SecondaryDepthRequested=1
SecondaryDepthTextureValid=1
SecondaryDepthRemap=1
SecondaryDepthExtent=<portal output size>
```

## Runtime evidence — 2026-09-15

The user built and ran the 1B.12B path successfully. Repeated main-view
composition frames reported:

```text
Projective=1
ProjectiveValid=1
DepthAware=1
DepthTextureValid=1
SecondaryDepthRequested=1
SecondaryDepthTextureValid=1
SecondaryDepthRemap=1
SecondaryDepthExtent=1920x860
```

The path remained active across repeated frames without a D3D12/RDG assertion or
resource-lifetime crash in the supplied log sequence. This accepts the runtime
**secondary-depth target creation / transport / main-compositor binding** path.

Mode 5 visual evidence was predominantly cyan and did not show a large persistent
magenta region, which supports the rigid-depth ordering proof for samples that
contain raster depth.

A large yellow region initially required classification because the corresponding
normal-color region was visibly gray. The user confirmed that region is the sky.
Disabling translucency did not change it, which is expected for sky/background
rendering and does not imply ordinary opaque SceneDepth ownership. The yellow
classification therefore matches the explicit 1B.12B contract: visible color may
exist without a rasterized remote SceneDepth sample.

The supplied normal-color run continued to report the transported secondary depth
resource as valid and bound with `DebugMode=0`, so enabling the resource did not
regress the accepted portal color path.

## Validation procedure

Use the accepted TSR producer:

```text
r.AntiAliasingMethod 4
portal.ProjectiveAperture 1
portal.DepthAwareComposition 1
portal.CompositionDiagnostics 1
portal.FullViewFamilyTSRDiagnostics 1
portal.FullViewFamilyTSRPrimaryFraction 0.67
portal.StartFullViewFamilyTSRSpike
```

First keep normal color:

```text
portal.CompositionDebugMode 0
```

Verify that 1B.12A and the accepted TSR color path do not regress.

Then enable the bounded 1B.12B diagnostic:

```text
portal.SecondaryDepthRemap 1
portal.CompositionDebugMode 5
```

Look through the portal at ordinary opaque remote geometry while moving the
player laterally and toward/away from the entry portal.

Expected result:

1. Visible opaque remote geometry is predominantly cyan.
2. Sky/background pixels may be yellow because they do not provide ordinary
   rasterized geometry depth.
3. Large persistent magenta regions are not acceptable.
4. Large yellow regions over confirmed ordinary opaque raster geometry are not acceptable.
5. Remote-geometry silhouettes should track the portal color geometry. Some
   edge mismatch is allowed because depth is current-frame primary-resolution
   data while color is post-TSR display-resolution data.
6. No render-thread assertion, RDG validation failure, stale target, NaN/Inf or
   resource-lifetime crash occurs while moving.

Capture one `PortalComposition ComposeReady` line and one periodic
`PortalTSRSpike Frame=...` line when re-running this gate.

Then run:

```text
portal.DumpFullViewFamilyTSRSpike
portal.StopFullViewFamilyTSRSpike
```

The report is:

```text
Saved/AutomationReports/PortalFullViewFamilyTSRSpike.json
```

## PASS criteria

PASS requires all of the following:

1. C++ and shader compile successfully. **PASS**.
2. The accepted normal portal color path remains visually unchanged. **PASS**.
3. `ComposeReady` reports:

```text
Projective=1
ProjectiveValid=1
SecondaryDepthRequested=1
SecondaryDepthTextureValid=1
SecondaryDepthRemap=1
```

   **PASS in repeated runtime logs**.
4. The transported depth target is valid, persistent across the tested run, and
   bound at the same portal output extent. **PASS**.
5. Mode 5 shows cyan on ordinary remote opaque geometry, yellow on the confirmed
   sky/background region, and no large persistent magenta regions. **PASS**.
6. Motion does not produce resource-lifetime/assert/RDG failures. **PASS for supplied run**.

## What PASS proves

A PASS proves this bounded project-side contract:

```text
same full secondary view
    -> post-TSR HDR color target
    + current-frame secondary SceneDepth target
    -> rigid portal depth equivalence
    -> conceptual main-view remote device depth available to composition
```

This is enough to proceed to the next renderer question: whether that remapped
remote depth can be propagated into a downstream depth consumer or safely written
to a main-view depth/stencil attachment.

## What PASS does NOT prove

It does not prove:

```text
TSR-reconstructed remote depth
sky/background synthetic depth
translucent remote depth
main SceneDepth mutation
main stencil aperture write
depth-correct DOF/fog/SSR/HZB
occlusion/HZB integration
recursion >= 2
multiple simultaneous visible portal histories
production GPU/VRAM acceptance
Core Portal Fidelity Seal
```

A current-frame depth/color edge mismatch is expected at 0.67 primary fraction
because color is temporally reconstructed at display resolution while the depth
proof is a point-resampled current-frame surface.

## Next gate after PASS

The next stage is a bounded **STEP 1B.12C main depth/stencil propagation
feasibility spike**. It must determine whether UE 5.8's public project-side RDG
surface provides a safe way to feed this remapped remote depth into downstream
main-view depth consumers. If not, that is the explicit renderer-private
escalation point. Do not hide the result with another RGB-only workaround.
