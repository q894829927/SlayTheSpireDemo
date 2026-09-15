# Interior Portal — STEP 1B.13A Bounded Main-Pass Scissor Hardening

Date: **2026-09-16**

State:

```text
STEP 1B.13A BOUNDED MAIN-PASS SCISSOR HARDENING
= IMPLEMENTED
= NOT YET BUILT OR RUN
```

## Goal

Reduce main-view portal raster work without changing any already accepted image,
depth, temporal-history or stencil semantics.

This step is deliberately narrower than secondary-view resource cropping. It asks:

```text
Can the existing BeforeDOF portal work be restricted to the conservative
projected portal rectangle while preserving the accepted full-fidelity path?
```

The full secondary `FSceneViewFamily`, TSR history and transported color/depth
allocation remain unchanged in 1B.13A. Those are follow-up gates after this main
pass scissor proves that the conservative bounds can safely own raster work.

## Accepted contracts that must remain unchanged

The following already passed gates are not reopened by this change:

```text
1B.8    secondary -> main PreExposure rebase
1B.10A  persistent secondary temporal history / camera-cut semantics
1B.10B  post-TSR secondary HDR extraction
1B.11A  exact projective aperture + near/grazing hardening
1B.12A  real main foreground depth occlusion
1B.12B  secondary depth transport/remap
1B.12C  main SceneDepth propagation + real DOF consumer
1B.12D  main stencil lifecycle + stencil-gated normal composition
```

## Implementation

Source:

```text
Source/SlayTheSpireDemo/Interior/InteriorPortalRenderer.cpp
```

Source commit:

```text
9379484428b18023564fc92af81512382597b076
portal: bound main portal passes to projected scissor
```

Controls:

```text
portal.BoundedMainPassScissor 0/1
portal.BoundedMainPassPaddingPixels <0..64>
```

Default state remains `0` until runtime validation completes.

With `portal.BoundedMainPassScissor=1`, the compositor converts the request's
normalized conservative `ProjectedBounds` into the current main `SceneColor.ViewRect`
and expands it by the configured padding. The resulting rectangle is then used as
the raster viewport for:

```text
main-depth propagation candidate build
main-depth propagation write
portal stencil mark
normal portal color composition
```

The stencil-bit clear remains full-view on purpose. STEP 1B.12D established that
bit `0x40` must be cleared even when the current portal disappears; restricting the
clear to only the current rectangle could reintroduce stale stencil outside the new
bounds.

## Sparse-output preservation

A bounded composition draw is sparse for the same reason as the accepted hardware
stencil path: pixels outside the raster rectangle do not execute the color shader.
Therefore a separately allocated post-process output is explicitly prefilled from
incoming main SceneColor whenever either of these is active:

```text
hardware stencil composition
bounded main-pass scissor
```

This preserves the accepted 1B.12D output contract:

```text
inside bounded portal work -> portal pass may overwrite
outside bounded portal work -> original main SceneColor survives exactly
```

## Proof isolation

Two cases intentionally disable the bounded rectangle:

```text
portal.StencilCompositionBypassShaderAperture=1
CompositionDebugMode=2 (full-screen magenta)
```

The stencil bypass proof must stay full-view so a CPU rectangle cannot masquerade
as hardware `CF_Equal` success. The full-screen magenta diagnostic must also retain
its original full-screen meaning.

## Diagnostics

With `portal.CompositionDiagnostics=1`, a new line is emitted:

```text
PortalComposition BoundedPass ...
Requested=1
Active=1
Rect=(MinX,MinY)-(MaxX,MaxY)
Pixels=<bounded>/<full>
Coverage=<0..1>
Padding=<pixels>
```

`Coverage` is the raster-area ratio relative to the current main SceneColor view
rect. A portal that occupies a small part of the screen should report a value well
below `1.0`. A near/crossing portal may legitimately approach `1.0`; correctness is
more important than forcing a saving.

## Build / runtime procedure

Close Unreal Editor, pull the branch, regenerate project files if required, then
build the editor target.

Enter PIE and establish the accepted full portal path:

```text
portal.StopStencilIdentityTonemapValidation
portal.StopStencilIdentityValidation
r.AntiAliasingMethod 4
portal.CompositionDebugMode 0
portal.ProjectiveAperture 1
portal.DepthAwareComposition 1
portal.SecondaryDepthRemap 1
portal.MainDepthPropagation 1
portal.StencilGatedComposition 1
portal.StencilCompositionBypassShaderAperture 0
portal.FullViewFamilyTSRPrimaryFraction 0.67
portal.StartFullViewFamilyTSRSpike
portal.CompositionDiagnostics 1
```

Baseline:

```text
portal.BoundedMainPassScissor 0
```

Capture one stable view.

Then enable the bounded path without moving the camera:

```text
portal.BoundedMainPassPaddingPixels 4
portal.BoundedMainPassScissor 1
```

Expected telemetry:

```text
PortalComposition BoundedPass ... Requested=1 Active=1 ... Coverage=<1 for a partially sized portal
PortalComposition DrawQueued ... BoundedScissor=1
```

## Visual acceptance

A/B with the same camera must preserve:

```text
portal RGB and exposure
projective aperture shape
foreground weapon / world occlusion
remote depth ordering
portal edge alignment
TSR temporal stability
stencil confinement
HUD and all unrelated main-view pixels
```

Then test representative motion / geometry cases:

```text
front-on portal
oblique portal
portal clipped by one screen edge
close portal
foreground weapon crossing the aperture
```

No rectangle-edge clipping, missing depth, black bars, stale pixels or exposure
change is acceptable.

## PASS criteria

1. C++ compiles and PIE reaches the bounded path.
2. `Requested=1 Active=1` is sustained while the portal is visible.
3. `Coverage < 1.0` is observed for at least one partially sized portal.
4. Same-camera `Scissor=0` versus `Scissor=1` is visually equivalent apart from
   expected binary stencil / subpixel edge tolerance already accepted by 1B.12D.
5. Foreground occlusion and propagated main depth remain correct.
6. Oblique / clipped / close views do not expose the CPU rectangle edge.
7. No RDG, RHI, D3D12, exposure or temporal-history regression occurs.

## What PASS proves

```text
projected conservative portal bounds
    -> current main render-resolution rectangle
    -> bounded main portal raster work
    -> accepted full-fidelity output preserved
```

## What PASS does not prove

```text
secondary full-view renderer is scissored
secondary TSR render resolution is cropped
secondary persistent color/depth allocations are reduced
Lumen / shadow / reflection work is portal-bounded
multi-portal batching
recursion >= 2
production performance acceptance
Core Portal Fidelity Seal
```

After PASS, the next gate is **1B.13B bounded secondary transport resources**:
post-TSR color and current-frame depth extraction targets can be cropped to the
padded portal region while retaining full secondary temporal-history coordinates.
Only after that should the renderer attempt a more invasive bounded secondary
view-family / primary-render contract.
