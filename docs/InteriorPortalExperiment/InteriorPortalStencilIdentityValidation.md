# Interior Portal — STEP 1B.12D-A Main Stencil Aperture Identity Validation

Date: **2026-09-16**

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
STEP 1B.12C-B REAL MAIN DOF DEPTH CONSUMER = PASS
STEP 1B.12D-A MAIN STENCIL RESOURCE / LIFETIME FEASIBILITY
= MAIN STENCIL TARGET + MASKED WRITE PATH = PASS
= ZERO-VISIBLE STENCIL LIFETIME = PASS
= POST-TONEMAP CALLBACK / TARGETABILITY = PASS
= POST-TONEMAP VISIBLE-APERTURE CONFINEMENT = INCONCLUSIVE / REJECTED AS ACCEPTANCE SURFACE
STEP 1B.12D-B SAME-BEFOREDOF STENCIL-GATED NORMAL COMPOSITION = IMPLEMENTED / NOT YET BUILT OR RUN
```

## Goal

Determine whether project-side public RDG code can use the current main
`SceneDepth` stencil plane as an explicit portal aperture identity, while keeping
the accepted projective aperture, depth transport and main-depth propagation
contracts intact.

The bounded feasibility bit remains:

```text
PortalStencilBit = 0x40
```

`0x40` is not declared production-owned or collision-free engine-wide.

## Writer implementation

Source:

```text
Source/SlayTheSpireDemo/Interior/InteriorPortalStencilIdentityValidation.cpp
Shaders/InteriorPortalStencilIdentity.usf
```

The writer runs on the real main player view. At `BeforeDOF` it requires:

```text
Format == PF_DepthStencil
TexCreate_DepthStencilTargetable
```

With `portal.StencilIdentityIsolateBit 1`, every active main-view `BeforeDOF`
frame clears only bit `0x40`, preserving all other stencil bits. Valid visible
portal apertures are then rasterized from the accepted inverse planar homography
and use a masked stencil replace with no color or depth write.

## Zero-visible lifetime defect and accepted fix

The original writer skipped the `BeforeDOF` callback when no visible aperture
snapshot existed. That could leave stale `0x40` state from a previous frame.

Fix:

```text
3391df2b63d5bcc9ba1c90ef3afaf32e1c42de9c
portal: clear stencil identity on zero-visible frames
```

The active writer now follows:

```text
every main-view BeforeDOF frame
    -> clear only 0x40
    -> if VisibleApertures > 0: mark current aperture(s)
    -> if VisibleApertures == 0: stop after clear
```

Runtime telemetry accepted this contract. During the forced zero-visible test,
`ZeroVisibleClear=1` remained active and `ZeroVisibleClears` increased through a
long sustained run. The final writer report recorded:

```text
SetupView=2296
Writes=1691
Overlays=0
ZeroVisibleClears=1511
VisibleApertures=0
Targetable=1
Isolated=1
```

Therefore stale identity cannot survive merely because the current aperture list
is empty.

## Why the Tonemap visual proof is no longer an acceptance surface

The first `AfterDOF` cyan proof contaminated main exposure and was rejected. A
later independent proof moved the cyan draw to the real Tonemap callback and
eliminated the white-screen exposure failure. Its sustained telemetry proved:

```text
Tonemap callback executes
SceneDepth/Stencil is still bindable/targetable
CF_Equal test path can be submitted
```

However, the final static screenshot did **not** show the portal interior becoming
solid cyan. The visible blue/cyan in that screenshot was the authored portal rim,
while the aperture interior remained the normal dark portal scene.

Therefore the Tonemap logs alone do not prove that portal pixels actually passed
the hardware stencil comparison. The validator did not count passing pixels, and
Tonemap operates after TSR/display-resolution processing while the stencil was
written in the earlier main render-resolution depth attachment. That makes the
post-tonemap surface a poor place to close the aperture-identity proof.

The previous documentation statement that post-tonemap visible confinement was a
PASS is superseded by this correction.

## Replacement acceptance gate: STEP 1B.12D-B

The correct next proof is to write and consume the stencil in the **same
BeforeDOF RDG chain and resolution domain** as normal portal composition:

```text
BeforeDOF main SceneDepth/Stencil
    -> clear 0x40
    -> mark exact projective portal aperture into 0x40
    -> optional main-depth propagation preserves stencil
    -> normal portal RGB composition uses hardware CF_Equal(0x40)
```

A dedicated proof switch can disable the color shader's ellipse mask. If portal
RGB remains confined to the physical aperture while that shader mask is forced to
full coverage, the hardware stencil gate is the only mechanism capable of
confining the draw.

See:

```text
docs/InteriorPortalStencilGatedCompositionValidation.md
```

## Accepted D-A claims

D-A accepts only these bounded facts:

```text
current main SceneDepth is a usable PF_DepthStencil target
project code can submit masked stencil writes at BeforeDOF
bit 0x40 can be isolated without clearing other bits
zero-visible frames can clear the bounded bit every active frame
post-tonemap callback/targetability can be reached without exposure feedback
```

D-A does **not** claim:

```text
that post-tonemap pixels visibly pass the intended portal stencil identity
permanent engine-wide ownership of bit 0x40
normal portal composition gated by stencil
HZB / SSR / Lumen integration
recursion
production performance acceptance
```
