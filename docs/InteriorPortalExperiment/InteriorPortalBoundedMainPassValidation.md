# Interior Portal — STEP 1B.13A Bounded Main-Pass Scissor Hardening

**2026-09-20 regression update:** the historical acceptance below does not
cover the now-reproduced pass-local NDC / camera-NDC mismatch. Its repair and
current acceptance boundary are recorded in
[InteriorPortalCropDisplayRegression.md](InteriorPortalCropDisplayRegression.md).

Date: **2026-09-16**

State:

```text
STEP 1B.13A BOUNDED MAIN-PASS SCISSOR HARDENING
= PASS
= USER-CONFIRMED RUNTIME ACCEPTANCE
```

## Goal

Reduce main-view portal raster work without changing any already accepted image,
depth, temporal-history or stencil semantics.

This step asks:

```text
Can the existing BeforeDOF portal work be restricted to the conservative
projected portal rectangle while preserving the accepted full-fidelity path?
```

The full secondary `FSceneViewFamily`, TSR history and transported color/depth
allocation remain unchanged in 1B.13A.

## Accepted contracts preserved

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

With `portal.BoundedMainPassScissor=1`, the compositor converts the request's
normalized conservative `ProjectedBounds` into the current main `SceneColor.ViewRect`
and expands it by the configured padding. The resulting rectangle is used for:

```text
main-depth propagation candidate build
main-depth propagation write
portal stencil mark
normal portal color composition
```

The stencil-bit clear remains full-view. STEP 1B.12D established that bit `0x40`
must still be cleared when the current portal disappears; restricting that clear
to the current rectangle would reintroduce stale stencil outside new bounds.

## Sparse-output preservation

A bounded composition draw is sparse. The separately allocated post-process output
is therefore prefilled from incoming main SceneColor whenever either hardware
stencil composition or bounded main-pass scissoring is active.

```text
inside bounded portal work -> portal pass may overwrite
outside bounded portal work -> original main SceneColor survives exactly
```

## Proof isolation

These cases intentionally remain full-view:

```text
portal.StencilCompositionBypassShaderAperture=1
CompositionDebugMode=2
```

The stencil bypass proof must not be accidentally confined by the CPU rectangle,
and full-screen magenta keeps its original diagnostic meaning.

## Runtime acceptance — 2026-09-16

The user completed the prescribed PIE A/B and reported the bounded path **passed**.
The runtime acceptance therefore records:

```text
[x] bounded main-pass path reached in PIE
[x] same-camera unbounded -> bounded transition accepted visually
[x] portal presentation remained correct
[x] no reported rectangle-edge clipping / black bars / stale pixels
[x] no reported foreground-depth or stencil regression
[x] no reported RDG / RHI / D3D12 failure
```

The exact `Coverage` value and full diagnostic log were not pasted into the chat,
so this document does **not** invent a measured pixel-reduction percentage. The
user's pass is sufficient to close the correctness gate; a production performance
seal still requires captured timings / counters later.

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

## Continuation decision

The originally planned next performance gate is **1B.13B bounded secondary
transport**. It reduces post-TSR color/depth transport work and later transport
allocation, but it is not expected to fix a visual-fidelity mismatch by itself.

Because the current user priority is eliminating the remaining portal display
inconsistency, fidelity isolation may be run before the more invasive secondary
resource-cropping work. Performance hardening remains a separate accepted follow-up.
