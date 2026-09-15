# Interior Portal — STEP 1B.12D-B Stencil-Gated Normal Composition

Date: **2026-09-16**

State:

```text
STEP 1B.12D-B SAME-BEFOREDOF STENCIL-GATED NORMAL COMPOSITION
= FIRST RUNTIME REACHED
= BYPASS PROOF FAILED WITH ONE-SIDED STENCIL STATE
= TWO-SIDED STENCIL FIX IMPLEMENTED / RETEST REQUIRED
```

## Goal

Close the stencil identity proof in the actual production-relevant domain rather
than at post-tonemap display resolution.

The gate asks:

```text
Can the normal BeforeDOF portal composition itself be constrained by the real
main SceneDepth stencil plane, with stencil write and stencil read occurring in
the same RDG chain and same render-resolution domain?
```

## Implementation

Files:

```text
Source/SlayTheSpireDemo/Interior/InteriorPortalRenderer.cpp
Shaders/InteriorPortalStencilComposition.usf
Shaders/InteriorPortalComposition.usf
```

Controls:

```text
portal.StencilGatedComposition 0/1
portal.StencilCompositionBypassShaderAperture 0/1
```

The bounded feasibility bit remains:

```text
0x40
```

This is still validation ownership only, not a permanent engine-wide reservation.

When `portal.StencilGatedComposition=1`, the normal compositor does the following
inside the same `BeforeDOF` callback:

```text
1. obtain current main SceneDepth
2. require PF_DepthStencil + DepthStencilTargetable
3. clear only stencil bit 0x40 across the active main view
4. rasterize the current request's exact projective ellipse into 0x40
   - same inverse planar homography
   - same near-plane ClipW rejection
5. preserve the accepted main-depth propagation path
6. bind the same SceneDepth as stencil read for normal portal color composition
7. submit normal portal RGB with hardware CF_Equal, read mask 0x40, ref 0x40
```

Normal mode still keeps the shader aperture mask for its soft edge and existing
projective/depth logic. The hardware stencil therefore acts as an additional hard
outer gate.

## Decisive proof switch

`portal.StencilCompositionBypassShaderAperture=1` is effective only while the
hardware stencil composition gate is active.

It deliberately changes the composition shader's aperture mask to full coverage:

```text
ApertureMask = 1
```

but does not disable projective intersection/depth calculations. Therefore:

```text
if hardware stencil is NOT working:
    portal RGB tends toward full-screen composition

if hardware stencil IS working:
    portal RGB remains confined to the exact stencil-marked portal aperture
```

## First runtime result

The first user run reached the intended code path and repeatedly reported:

```text
Requested=1
Active=1
StencilTargetable=1
Format=11
StencilBit=0x40
BypassShaderAperture=1
MainDepthPropagation=1
```

However, with the shader aperture deliberately bypassed, the main image became
strongly full-screen / overexposed instead of remaining portal-shaped. This is a
valid **FAIL** for the hardware-gate proof: the normal composition draw was not
actually being confined by the expected stencil comparison.

The main render target/depth-stencil binding and `StencilRef=0x40` were already
present, so the next issue was narrowed to the depth-stencil face state used by
`AddDrawScreenPass`.

## Root-cause hardening

UE 5.8 `TStaticDepthStencilState` carries independent front-face and back-face
stencil enable/test/op fields. The initial proof enabled stencil only for the
front face while leaving back-face stencil disabled.

A screen-pass triangle/quad must not depend on a particular winding for stencil
correctness. Therefore commit:

```text
829b2cac4afc643d7e8c58dd9fbecd0e9c6cfef5
portal: make stencil composition state two-sided
```

changes all three same-BeforeDOF stencil states to explicit two-sided behavior:

```text
clear:
    front = enabled, ALWAYS, REPLACE
    back  = enabled, ALWAYS, REPLACE

mark:
    front = enabled, ALWAYS, REPLACE
    back  = enabled, ALWAYS, REPLACE

test:
    front = enabled, EQUAL, KEEP
    back  = enabled, EQUAL, KEEP
```

Masks remain bounded to bit `0x40`; the test path still uses write mask `0x00`.

## Important isolation rule

Do not run the old standalone stencil validators during this test. They also
manipulate/read bit `0x40`, and their ordering relative to the normal compositor is
not the contract being tested.

Before testing, stop them if necessary:

```text
portal.StopStencilIdentityTonemapValidation
portal.StopStencilIdentityValidation
```

## Retest procedure

Close Unreal Editor, pull the branch and rebuild the editor target.

Enter PIE with a linked portal visible and establish the accepted full portal path:

```text
r.AntiAliasingMethod 4
portal.CompositionDebugMode 0
portal.ProjectiveAperture 1
portal.DepthAwareComposition 1
portal.SecondaryDepthRemap 1
portal.MainDepthPropagation 1
portal.FullViewFamilyTSRPrimaryFraction 0.67
portal.StartFullViewFamilyTSRSpike
```

Enable diagnostics and the new gate:

```text
portal.CompositionDiagnostics 1
portal.StencilGatedComposition 1
portal.StencilCompositionBypassShaderAperture 0
```

Expected log:

```text
PortalComposition StencilGate ...
Requested=1
Active=1
StencilTargetable=1
Format=11
StencilBit=0x40
BypassShaderAperture=0
```

The portal should look like the accepted normal composition.

Then, without moving the camera, run:

```text
portal.StencilCompositionBypassShaderAperture 1
```

Expected visual result after the two-sided fix:

```text
portal RGB remains confined to the physical/projective portal aperture
wall / floor / weapon / HUD do not become portal RGB
main scene exposure stays unchanged
real foreground occluders still remain in front of the portal
```

A slightly harder edge is acceptable in this proof because stencil is binary.

## Failure interpretation

Fail if any of the following occurs:

```text
Active=0 while Requested=1
StencilTargetable=0
format is not PF_DepthStencil
bypass=1 still causes full-screen or rectangular portal RGB
foreground occlusion regresses
main depth propagation stops working
RDG / D3D12 depth-stencil validation error
shader compilation failure
```

If the two-sided retest still fails, do not alter exposure or shader aperture. The
next diagnostic should verify actual stencil contents immediately after the mark
pass in the same BeforeDOF render-resolution domain.

## PASS criteria

1. C++ and Global Shaders compile.
2. `Requested=1 Active=1 StencilTargetable=1 Format=11` is sustained.
3. Normal `BypassShaderAperture=0` presentation matches the accepted portal path.
4. `BypassShaderAperture=1` still confines portal RGB to the exact portal aperture.
5. No unrelated screen region receives portal RGB.
6. Accepted main foreground occlusion remains correct.
7. Accepted main-depth propagation remains enabled without stencil corruption.
8. No exposure, render-thread, RDG or D3D12 failure occurs.

## What PASS proves

```text
same BeforeDOF main depth/stencil resource
    -> exact projective stencil mark
    -> normal portal composition
    -> real hardware CF_Equal aperture gate
```

## What PASS does not prove

```text
permanent engine-wide stencil-bit ownership/collision policy
multi-portal simultaneous identity values
stencil-aware HZB rebuild
SSR / Lumen screen-trace integration
translucent remote depth
recursion >= 2
portal-bounded secondary render resolution/performance acceptance
Core Portal Fidelity Seal
```

After PASS, the next renderer gate is portal-bounded/scissored resource hardening,
not recursion.
