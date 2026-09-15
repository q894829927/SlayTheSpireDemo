# Interior Portal — STEP 1B.12D-B Stencil-Gated Normal Composition

Date: **2026-09-16**

State:

```text
STEP 1B.12D-B SAME-BEFOREDOF STENCIL-GATED NORMAL COMPOSITION
= IMPLEMENTED
= NOT YET BUILT OR RUN
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

New controls:

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

This avoids the ambiguity of the earlier post-tonemap cyan proof: the normal
portal image itself becomes the stencil-test evidence.

## Important isolation rule

Do not run the old standalone stencil validators during this test. They also
manipulate/read bit `0x40`, and their ordering relative to the normal compositor is
not the contract being tested.

Before testing, stop them if necessary:

```text
portal.StopStencilIdentityTonemapValidation
portal.StopStencilIdentityValidation
```

## Build / runtime procedure

Close Unreal Editor before rebuilding because this change adds Global Shader
registrations and a new `.usf` file.

After pulling the branch, rebuild the editor target. Enter PIE with a linked portal
visible, then establish the accepted full portal path:

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

Then perform the decisive proof:

```text
portal.StencilCompositionBypassShaderAperture 1
```

Expected log changes only to:

```text
BypassShaderAperture=1
```

Expected visual result:

```text
portal RGB is still confined to the physical/projective portal aperture
wall / floor / weapon / HUD do not become portal RGB
main scene exposure stays unchanged
real foreground occluders still remain in front of the portal
```

A slightly harder edge is acceptable in this proof because stencil is binary;
the purpose is confinement, not final anti-aliased edge quality.

For an A/B capture, use the same camera:

```text
A: portal.StencilCompositionBypassShaderAperture 0
B: portal.StencilCompositionBypassShaderAperture 1
```

Both must remain portal-shaped. B is the decisive hardware-gate evidence.

## Failure interpretation

Fail if any of the following occurs:

```text
Active=0 while Requested=1
StencilTargetable=0
format is not PF_DepthStencil
bypass=1 causes full-screen or rectangular portal RGB
foreground occlusion regresses
main depth propagation stops working
RDG / D3D12 depth-stencil validation error
shader compilation failure
```

If build fails, use the first real compiler/shader error rather than cascade errors.

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

This is stronger and more production-relevant than the earlier post-tonemap color
proof because write and consume happen in the actual composition domain.

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
