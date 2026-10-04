# Interior Portal — STEP 1B.12D-B Stencil-Gated Normal Composition

Date: **2026-09-16**

State:

```text
STEP 1B.12D-B SAME-BEFOREDOF STENCIL-GATED NORMAL COMPOSITION
= PASS
```

## Goal

Prove that normal `BeforeDOF` portal composition can be constrained by the real
main SceneDepth stencil plane, with stencil write and stencil read occurring in
the same RDG chain and same render-resolution domain.

## Accepted implementation

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

The bounded validation bit is:

```text
0x40
```

This is still validation ownership only, not a permanent engine-wide reservation.

When `portal.StencilGatedComposition=1`, the compositor performs in one
`BeforeDOF` callback:

```text
1. obtain current main SceneDepth
2. require PF_DepthStencil + DepthStencilTargetable
3. prefill the new SceneColor output from incoming main SceneColor
4. clear only stencil bit 0x40 across the active main view
5. rasterize the exact projective portal ellipse into 0x40
6. preserve the accepted main-depth propagation path
7. bind the same SceneDepth as stencil read for normal composition
8. draw with hardware CF_Equal, read mask 0x40, ref 0x40
```

Normal mode keeps the accepted shader aperture mask for its soft edge and depth
logic. The hardware stencil is an additional hard outer gate.

## Hardening retained

### Output preservation for sparse stencil coverage

`FScreenPassRenderTarget::CreateFromInput(...)` creates a compatible output but
does not guarantee the input pixels have already been copied into that output.
Because hardware stencil rejection makes the color draw sparse, the output is
explicitly prefilled first:

```text
if Output.Texture != SceneColor.Texture:
    AddCopyTexturePass(SceneColor.Texture -> Output.Texture)
```

This preserves main SceneColor on every stencil-rejected pixel.

Source commit:

```text
f819eb054218a7e1a5591428d97746b0c6c27296
portal: preserve scene color outside stencil gate
```

### Explicit pipeline stencil reference

The screen-pass pipeline states encode:

```text
clear pipeline ref = 0x00
mark pipeline ref  = 0x40
test pipeline ref  = 0x40
```

and the active composition diagnostic reports:

```text
PipelineStencilRef=0x40
```

### Two-sided stencil state

All clear / mark / test states are enabled for both front and back faces so the
screen-pass result is independent of triangle winding:

```text
clear: front/back = ALWAYS + REPLACE
mark:  front/back = ALWAYS + REPLACE
test:  front/back = EQUAL + KEEP
```

### Exposure-safe proof mode

`portal.StencilCompositionBypassShaderAperture=1` deliberately stops using the
normal Portal RGB proof surface and instead emits a bounded cyan tint from the
already-pre-exposed main SceneColor:

```text
SafeProof = MainColor * (0.25, 1.0, 1.0)
```

The shader aperture is bypassed for this proof, so only the hardware stencil test
may confine the draw.

## Runtime acceptance evidence

The accepted runtime sustained:

```text
Requested=1
Active=1
StencilTargetable=1
Format=11
StencilBit=0x40
PipelineStencilRef=0x40
OutputPrefilled=1
MainDepthPropagation=1
```

Normal mode reported:

```text
BypassShaderAperture=0
ProofMode=NormalPortalRGB
OutputPrefilled=1
```

and the main scene returned to the accepted presentation: wall / floor / weapon
remained normal, the portal showed the remote scene, and the previous white / pink /
neon undefined-output corruption was gone.

The decisive proof then switched to:

```text
portal.StencilCompositionBypassShaderAperture 1
```

with runtime diagnostics:

```text
BypassShaderAperture=1
ProofMode=SafeMainColorTint
PipelineStencilRef=0x40
OutputPrefilled=1
```

The captured frame showed that the modification remained confined to the portal
aperture while the surrounding wall, floor, weapon and HUD remained unchanged.
There was no full-screen cyan modification and no HDR exposure blowout.

The one-frame `BypassShaderAperture=0` sample immediately after the console command
is expected command-to-render-thread latency; subsequent frames reported `1` and
were used for acceptance.

## Verdict

```text
STEP 1B.12D-B = PASS
```

Accepted chain:

```text
same BeforeDOF main depth/stencil resource
    -> clear validation bit 0x40
    -> exact projective stencil mark
    -> preserve main SceneColor outside sparse coverage
    -> CF_Equal(ref=0x40, readmask=0x40)
    -> normal portal composition / safe proof confined to aperture
```

This proves that the production-relevant `BeforeDOF` composition path is genuinely
hardware-stencil gated rather than merely appearing portal-shaped because of the
shader aperture mask.

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

## Next gate

Proceed to portal-bounded / scissored resource hardening. Do not begin recursion
yet. The next goal is to stop paying full-view secondary allocation / work when the
visible portal occupies only a bounded region, while preserving the already
accepted HDR, pre-exposure, TSR/history, depth propagation and stencil semantics.
