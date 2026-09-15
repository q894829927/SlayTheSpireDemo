# Interior Portal — STEP 1B.12D-B Stencil-Gated Normal Composition

Date: **2026-09-16**

State:

```text
STEP 1B.12D-B SAME-BEFOREDOF STENCIL-GATED NORMAL COMPOSITION
= RUNTIME REACHED
= BYPASS PROOF STILL FAILS
= TWO-SIDED STENCIL STATE IMPLEMENTED
= EXPLICIT SCREEN-PASS STENCIL REF IMPLEMENTED
= EXPOSURE-SAFE PROOF MODE IMPLEMENTED
= RETEST REQUIRED
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

When `portal.StencilGatedComposition=1`, the normal compositor performs in one
`BeforeDOF` callback:

```text
1. obtain current main SceneDepth
2. require PF_DepthStencil + DepthStencilTargetable
3. clear only stencil bit 0x40 across the active main view
4. rasterize the exact projective portal ellipse into 0x40
5. preserve the accepted main-depth propagation path
6. bind the same SceneDepth as stencil read for normal composition
7. draw with hardware CF_Equal, read mask 0x40, ref 0x40
```

Normal mode keeps the accepted shader aperture mask for its soft edge and depth
logic. The stencil path is an additional hard outer gate.

## Runtime failure evidence

The user repeatedly reached:

```text
Requested=1
Active=1
StencilTargetable=1
Format=11
StencilBit=0x40
MainDepthPropagation=1
```

but enabling the bypass proof still produced large full-screen / corrupted-looking
HDR changes instead of a portal-confined result.

The second run after enabling both front and back stencil faces still failed. This
means the one-sided state was a valid hardening change but was **not sufficient**
to close the gate.

The strange white / pink / neon-green presentation is not treated as source-asset
corruption. The old bypass proof disabled the shader aperture and then sampled the
secondary HDR portal texture across the full screen. If the hardware stencil test
did not confine that draw, arbitrary portal HDR values changed the main luminance
histogram and drove Eye Adaptation / PreExposure to extreme values. The visual
explosion was therefore a consequence of the intentionally unsafe proof surface,
not a valid normal portal presentation.

## Hardening after the second failure

### 1. Explicit `FScreenPassPipelineState::StencilRef`

The previous code relied on `RHICmdList.SetStencilRef(...)` inside the screen-pass
setup lambda while the `FScreenPassPipelineState` itself retained its default
stencil reference.

The hardened path now encodes the reference directly in every relevant screen-pass
pipeline state:

```text
clear pipeline ref = 0x00
mark pipeline ref  = 0x40
test pipeline ref  = 0x40
```

The dynamic `SetStencilRef` calls remain as redundant guards. Diagnostics now print:

```text
PipelineStencilRef=0x40
```

for the active composition gate.

Source commit:

```text
0169f42be9d05f416b54326d41f0d9b5008899cb
portal: harden screen-pass stencil refs
```

### 2. Exposure-safe bypass proof

The old proof did this:

```text
ApertureMask = 1
sample portal HDR across the full screen
```

That made a failed stencil test catastrophically perturb Eye Adaptation.

The new proof does **not** sample the portal texture or secondary depth at all. It
uses the same full-screen composition draw and the same hardware stencil state,
but emits a bounded cyan tint derived from the already-pre-exposed main SceneColor:

```text
SafeProof = MainColor * (0.25, 1.0, 1.0)
```

Therefore:

```text
if stencil works:
    only the portal aperture becomes cyan-tinted

if stencil fails:
    the whole screen becomes cyan-tinted
```

Either outcome is obvious, but neither path feeds arbitrary portal HDR energy into
Eye Adaptation.

Shader commit:

```text
ca00c2231b6d0d7f04c4b9630a8600e067fb8b8b
portal: make stencil bypass proof exposure-safe
```

## Existing two-sided hardening

Commit:

```text
829b2cac4afc643d7e8c58dd9fbecd0e9c6cfef5
portal: make stencil composition state two-sided
```

keeps all three stencil states independent of screen-pass winding:

```text
clear: front/back = ALWAYS + REPLACE
mark:  front/back = ALWAYS + REPLACE
test:  front/back = EQUAL + KEEP
```

Masks remain bounded to `0x40`; the composition test uses write mask `0x00`.

## Important isolation rule

Do not run the old standalone stencil validators during this test. They manipulate
the same validation bit `0x40` and are outside this gate's ordering contract.

Before testing, stop them if needed:

```text
portal.StopStencilIdentityTonemapValidation
portal.StopStencilIdentityValidation
```

## Retest procedure

Close Unreal Editor, pull the branch and rebuild the editor target.

Enter PIE with a linked portal visible and establish the accepted portal path:

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

Enable the gate in normal mode:

```text
portal.CompositionDiagnostics 1
portal.StencilGatedComposition 1
portal.StencilCompositionBypassShaderAperture 0
```

Expected diagnostics:

```text
Requested=1
Active=1
StencilTargetable=1
Format=11
StencilBit=0x40
PipelineStencilRef=0x40
BypassShaderAperture=0
ProofMode=NormalPortalRGB
```

The portal should match the previously accepted normal presentation.

Then, without moving the camera:

```text
portal.StencilCompositionBypassShaderAperture 1
```

Expected diagnostics:

```text
PipelineStencilRef=0x40
BypassShaderAperture=1
ProofMode=SafeMainColorTint
```

Expected visual result:

```text
only the exact portal aperture is cyan-tinted
wall / floor / weapon / HUD remain unchanged
no full-screen white / pink / neon HDR blowout
main exposure remains stable
```

If the whole screen is cyan-tinted, the hardware stencil test still is not
confining the draw. That is now a clean stencil failure without exposure noise.

## Failure interpretation

Fail if any of the following occurs:

```text
Active=0 while Requested=1
StencilTargetable=0
format is not PF_DepthStencil
PipelineStencilRef is not 0x40
safe proof tints the whole screen
normal mode regresses foreground occlusion or main-depth propagation
RDG / D3D12 depth-stencil validation error
shader compilation failure
```

If the safe proof still fails after this hardening, the next diagnostic must inspect
actual stencil contents immediately after the mark pass in the same BeforeDOF
render-resolution domain. Do not change exposure, gamma, TSR or portal brightness.

## PASS criteria

1. C++ and Global Shaders compile.
2. `Requested=1 Active=1 StencilTargetable=1 Format=11` is sustained.
3. `PipelineStencilRef=0x40` is reported for the active composition test.
4. Normal `BypassShaderAperture=0` presentation matches the accepted portal path.
5. Safe proof mode tints only the exact portal aperture.
6. No unrelated screen region is modified by the proof.
7. Accepted foreground occlusion and main-depth propagation remain intact.
8. No exposure, render-thread, RDG or D3D12 failure occurs.

## What PASS proves

```text
same BeforeDOF main depth/stencil resource
    -> exact projective stencil mark
    -> normal portal composition draw
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
