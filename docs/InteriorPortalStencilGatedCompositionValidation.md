# Interior Portal — STEP 1B.12D-B Stencil-Gated Normal Composition

Date: **2026-09-16**

State:

```text
STEP 1B.12D-B SAME-BEFOREDOF STENCIL-GATED NORMAL COMPOSITION
= RUNTIME REACHED
= MAIN-STENCIL PATH ACTIVE
= OUTPUT-PRESERVATION DEFECT IDENTIFIED
= SCENECOLOR PREFILL FIX IMPLEMENTED
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

When `portal.StencilGatedComposition=1`, the compositor performs in one
`BeforeDOF` callback:

```text
1. obtain current main SceneDepth
2. require PF_DepthStencil + DepthStencilTargetable
3. prefill the new SceneColor output from the incoming main SceneColor
4. clear only stencil bit 0x40 across the active main view
5. rasterize the exact projective portal ellipse into 0x40
6. preserve the accepted main-depth propagation path
7. bind the same SceneDepth as stencil read for normal composition
8. draw with hardware CF_Equal, read mask 0x40, ref 0x40
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
PipelineStencilRef=0x40
MainDepthPropagation=1
```

but enabling `portal.StencilGatedComposition 1` produced a white/pink/corrupted
main image even while:

```text
BypassShaderAperture=0
ProofMode=NormalPortalRGB
```

This is important: the corruption therefore did **not** require the proof shader
to bypass the aperture. The failure was in the sparse stencil-gated output path
itself.

## Root cause: sparse stencil draw into an uninitialized post-process output

`FScreenPassRenderTarget::CreateFromInput(...)` creates an output compatible with
the input; it does not mean that the input SceneColor pixels have already been
copied into that output.

The original non-stencil compositor always executed the pixel shader across the
whole viewport. Outside the portal the shader explicitly wrote `MainColor`, so the
entire output became initialized every frame.

After adding a real hardware stencil test, the color pass became sparse:

```text
stencil == 0x40  -> pixel shader executes
stencil != 0x40  -> pixel shader is rejected before writing color
```

Pixels rejected by stencil therefore retained undefined contents of the newly
allocated post-process output. Because this buffer is HDR / pre-exposed, undefined
values appeared as the observed white, pink and neon-looking corruption and could
then disturb later exposure processing.

This explains why the scene could corrupt even with the shader's own aperture mask
still enabled: the rejected pixels never reached the shader at all.

## SceneColor preservation fix

Source commit:

```text
f819eb054218a7e1a5591428d97746b0c6c27296
portal: preserve scene color outside stencil gate
```

The stencil path now explicitly prefills the output before any sparse hardware
stencil draw:

```text
if Output.Texture != SceneColor.Texture:
    AddCopyTexturePass(SceneColor.Texture -> Output.Texture)
```

The subsequent composition pass keeps `ELoad`, so:

```text
outside portal / stencil reject -> copied original main SceneColor survives
inside portal / stencil pass    -> portal composition overwrites only those pixels
```

Diagnostics now include:

```text
OutputPrefilled=1
```

for the normal expected stencil-gated path where `CreateFromInput` allocated a
separate output texture.

## Other hardening retained

### Explicit pipeline stencil reference

The path encodes:

```text
clear pipeline ref = 0x00
mark pipeline ref  = 0x40
test pipeline ref  = 0x40
```

and reports:

```text
PipelineStencilRef=0x40
```

### Two-sided stencil state

All three screen-pass states remain independent of triangle winding:

```text
clear: front/back = ALWAYS + REPLACE
mark:  front/back = ALWAYS + REPLACE
test:  front/back = EQUAL + KEEP
```

### Exposure-safe proof mode

`portal.StencilCompositionBypassShaderAperture 1` no longer samples arbitrary
portal HDR across the screen. It emits a bounded cyan tint derived from the
already-pre-exposed main SceneColor:

```text
SafeProof = MainColor * (0.25, 1.0, 1.0)
```

Therefore:

```text
if stencil works:
    only the portal aperture becomes cyan-tinted

if stencil fails:
    the whole output becomes cyan-tinted
```

without the previous HDR explosion.

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
OutputPrefilled=1
```

Expected visual result:

```text
normal main scene remains normal
portal remains confined to its accepted projective aperture
no white / pink / neon garbage outside the portal
accepted foreground occlusion remains intact
```

Then, without moving the camera:

```text
portal.StencilCompositionBypassShaderAperture 1
```

Expected diagnostics:

```text
PipelineStencilRef=0x40
BypassShaderAperture=1
ProofMode=SafeMainColorTint
OutputPrefilled=1
```

Expected visual result:

```text
only the exact portal aperture is cyan-tinted
wall / floor / weapon / HUD remain unchanged
main exposure remains stable
```

If normal mode is fixed but the safe proof still tints the whole screen, the next
diagnostic must inspect actual stencil contents immediately after the mark pass.
Do not change exposure, gamma, TSR or portal brightness.

## PASS criteria

1. C++ and Global Shaders compile.
2. `Requested=1 Active=1 StencilTargetable=1 Format=11` is sustained.
3. `PipelineStencilRef=0x40` is reported.
4. `OutputPrefilled=1` is reported on the separate-output stencil path.
5. Normal `BypassShaderAperture=0` presentation matches the accepted portal path.
6. Safe proof mode tints only the exact portal aperture.
7. No unrelated screen region is modified.
8. Accepted foreground occlusion and main-depth propagation remain intact.
9. No exposure, render-thread, RDG or D3D12 failure occurs.

## What PASS proves

```text
same BeforeDOF main depth/stencil resource
    -> exact projective stencil mark
    -> preserved main SceneColor outside sparse stencil coverage
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
