# Interior Portal — STEP 1B.12C-A Main SceneDepth Propagation Feasibility

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
```

## Goal

1B.12B established that the transported secondary device Z is the exact
conceptual main-view device Z for opaque remote geometry under the current rigid
portal transform and identical projection matrix.

1B.12C-A asks the next renderer question:

```text
Can project-side public RDG code write that validated remote device depth into
THE CURRENT MAIN SceneDepth resource inside the portal aperture, while preserving
real main-view foreground occluders?
```

This is a bounded feasibility spike. It does not yet claim stencil ownership,
HZB rebuild, recursion, or production-ready downstream behavior.

## Why the write uses two passes

Reading and writing the same SceneDepth texture in one raster pass would create
an SRV/DSV hazard and would also make foreground preservation ambiguous.

The implementation therefore splits the operation:

```text
PASS A — candidate build
    read main SceneDepth
    read transported secondary depth
    apply exact projective aperture
    reject real foreground geometry in front of portal plane
    reject sky / missing remote raster depth
    reject invalid remote depth in front of portal plane
    write accepted remote device Z to transient R32F candidate texture

PASS B — depth write
    read transient candidate texture
    discard zero / invalid candidate pixels
    bind current main SceneDepth as depth-stencil target
    depth test = ALWAYS
    depth write = enabled
    stencil = untouched
    write candidate as SV_Depth
```

`CF_Always` is intentional. PASS A already decided whether a pixel is safe to
replace. A normal reversed-Z depth test would reject remote portal depth against
the physical host wall / portal-plane depth, which is exactly the depth that must
be replaced inside the aperture.

## Runtime controls

The write path is off by default:

```text
portal.MainDepthPropagation 0
```

Enable it explicitly:

```text
portal.MainDepthPropagation 1
```

Debug mode 6 also requests the write path automatically:

```text
portal.CompositionDebugMode 6
```

Mode 6 executes after the queued depth-write pass and re-reads the actual main
SceneDepth resource. Colors inside the portal aperture mean:

```text
cyan   = main SceneDepth now matches transported secondary depth
         -> propagation succeeded

green  = real main-view foreground geometry remains in front of portal plane
         -> foreground preservation succeeded

yellow = no transported raster depth exists (for example sky/background)
         -> no main depth write is attempted

red    = valid transported remote depth exists but main SceneDepth does not
         contain the propagated value
         -> write / ordering failure
```

The comparison tolerance is derived from the already accepted world-space depth
tolerance:

```text
portal.DepthOcclusionEpsilonCm 2.0
```

No arbitrary depth bias is introduced.

## Diagnostics

With:

```text
portal.CompositionDiagnostics 1
```

`PortalComposition ComposeReady` reports:

```text
MainDepthPropagationRequested=...
MainDepthTargetable=...
MainDepthPropagation=...
CandidateExtent=...x...
```

A depth-write-capable frame is expected to report:

```text
Projective=1
ProjectiveValid=1
DepthTextureValid=1
SecondaryDepthTextureValid=1
MainDepthPropagationRequested=1
MainDepthTargetable=1
MainDepthPropagation=1
CandidateExtent=<main scene texture extent>
```

The draw log also appends:

```text
MainDepthPropagation=1
```

## Runtime evidence — 2026-09-15

The user built and ran 1B.12C-A successfully. Repeated frames reported:

```text
Projective=1
ProjectiveValid=1
DepthAware=1
DepthTextureValid=1
SecondaryDepthRequested=1
SecondaryDepthTextureValid=1
SecondaryDepthRemap=1
MainDepthPropagationRequested=1
MainDepthTargetable=1
MainDepthPropagation=1
CandidateExtent=1544x712
```

This proves the current main SceneDepth resource is depth-stencil targetable at
this public SceneViewExtension / RDG point and that the explicit depth-write pass
is actually active, rather than merely requested.

Mode 6 visual evidence showed:

```text
cyan   over ordinary remote opaque geometry
yellow over the already-classified sky/background region
green  over preserved real main-view foreground depth
no large persistent red write-failure region
```

Normal `CompositionDebugMode 0` remained visually usable with
`portal.MainDepthPropagation 1`, so enabling the depth mutation did not
immediately regress the accepted portal color path.

During motion with main TSR enabled, the diagnostic colors could temporarily
smear or shift toward cyan depending on movement direction. The user repeated the
same motion with:

```text
r.AntiAliasingMethod 0
```

and reported that the diagnostic colors were stable while moving. Therefore the
motion-dependent color transient is classified as main-view TSR history acting on
the artificial BeforeDOF debug colors, not an instability in the underlying
SceneDepth write/classification. This is the same bounded diagnostic limitation
already isolated in 1B.12A.

## Validation procedure

Close Unreal Editor before rebuilding because a new global shader and shader
parameter layout were added.

Pull and build the branch, then enter PIE and start the already accepted secondary
producer:

```text
r.AntiAliasingMethod 4
portal.ProjectiveAperture 1
portal.DepthAwareComposition 1
portal.SecondaryDepthRemap 1
portal.CompositionDiagnostics 1
portal.FullViewFamilyTSRDiagnostics 1
portal.FullViewFamilyTSRPrimaryFraction 0.67
portal.StartFullViewFamilyTSRSpike
```

First verify the unchanged normal path with depth propagation disabled:

```text
portal.MainDepthPropagation 0
portal.CompositionDebugMode 0
```

Then enable the write verification:

```text
portal.MainDepthPropagation 1
portal.CompositionDebugMode 6
```

Move laterally and toward / away from the portal while looking at ordinary opaque
remote geometry and at the previously identified sky region.

Expected:

1. ordinary remote opaque geometry is predominantly cyan;
2. the sky/background region remains yellow;
3. a real world-space object between player and portal becomes green where it
   overlaps the aperture;
4. large persistent red regions are not acceptable;
5. no D3D12 fatal, RDG validation failure, render-target compatibility assertion,
   or resource lifetime crash occurs.

For raw diagnostic classification under movement, temporarily disabling main-view
AA is allowed because the debug colors are authored BeforeDOF and otherwise pass
through TSR history reconstruction:

```text
r.AntiAliasingMethod 0
```

Restore after the diagnostic check:

```text
r.AntiAliasingMethod 4
```

Then return to normal color while keeping propagation enabled:

```text
portal.CompositionDebugMode 0
portal.MainDepthPropagation 1
```

The accepted portal color and 1B.12A foreground occlusion must remain visually
stable. This does not yet prove downstream DOF/fog correctness; it only checks
that mutating main depth does not immediately regress the accepted color path.

Finally disable the experimental write and stop the producer:

```text
portal.MainDepthPropagation 0
portal.StopFullViewFamilyTSRSpike
```

## PASS criteria

PASS requires all of the following:

1. C++ and shaders compile successfully. **PASS**.
2. `ComposeReady` reports `MainDepthTargetable=1` and
   `MainDepthPropagation=1` while enabled. **PASS**.
3. Debug mode 6 shows cyan on ordinary remote opaque geometry. **PASS**.
4. Known sky/background remains yellow rather than being assigned fake depth. **PASS**.
5. A real main-view foreground occluder remains green / preserved. **PASS**.
6. Large persistent red regions do not occur. **PASS in supplied visual run**.
7. No render-thread assertion, RDG validation failure, D3D12 depth-target error,
   NaN/Inf, or resource-lifetime crash occurs while moving. **PASS in supplied run**.
8. Normal mode with propagation enabled does not regress accepted portal RGB or
   1B.12A foreground occlusion. **PASS in supplied normal-color evidence**.
9. Movement-dependent debug-color changes disappear with main-view AA disabled,
   proving the observed transient is TSR history on diagnostic color rather than
   unstable propagated depth. **PASS**.

## What PASS proves

A PASS proves this narrow project-side public-renderer contract:

```text
transported remote depth
    -> validated candidate mask
    -> public RDG raster depth write
    -> current main SceneDepth resource contains conceptual portal-interior depth
       for valid opaque remote geometry
```

This is materially stronger than the earlier RGB-only and read-only depth gates.

## What PASS does NOT prove

It does not yet prove:

```text
main stencil aperture ownership
HZB / occlusion hierarchy rebuild
SSR / Lumen screen-trace integration
full DOF / fog correctness
TSR-reconstructed remote depth
translucent remote depth
recursion >= 2
multiple simultaneous visible portal histories
production GPU/VRAM acceptance
Core Portal Fidelity Seal
```

## Next gate after PASS

The next bounded proof is a **real downstream depth consumer validation**. Prefer
DOF / fog first because they consume depth after the current BeforeDOF insertion
point and can directly reveal whether the mutated main SceneDepth is observed
outside the diagnostic shader itself.

Stencil ownership remains a separate gate. Add it only if a downstream pass or
later recursion architecture actually needs explicit portal-aperture identity.
