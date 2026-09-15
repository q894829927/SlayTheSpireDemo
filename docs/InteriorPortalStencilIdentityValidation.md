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
STEP 1B.12D-A MAIN STENCIL APERTURE IDENTITY
= MAIN STENCIL WRITE/READ = PASS
= POST-TONEMAP EXPOSURE-SAFE HARDWARE PROOF = PASS
= ZERO-VISIBLE STENCIL LIFETIME = PASS
= FINAL STATIC VISIBLE-APERTURE CONFINEMENT SCREENSHOT REQUIRED
```

## Goal

Prove that project-side public RDG code can assign the linked physical portal
aperture an explicit identity in the **current main depth/stencil attachment** and
verify that identity downstream with a real hardware stencil comparison rather
than by recomputing the projective mask in the proof shader.

This remains a bounded feasibility gate. It does not reserve a production stencil
bit engine-wide and normal portal composition does not yet depend on stencil.

## Implementation

Writer / identity validator:

```text
Source/SlayTheSpireDemo/Interior/InteriorPortalStencilIdentityValidation.cpp
Shaders/InteriorPortalStencilIdentity.usf
```

Exposure-safe downstream proof:

```text
Source/SlayTheSpireDemo/Interior/InteriorPortalStencilTonemapValidation.cpp
```

Zero-visible test helper:

```text
Source/SlayTheSpireDemo/Interior/InteriorPortalStencilZeroVisibleTest.cpp
```

The writer runs only on the real main player view. `SetupView` snapshots linked
portal logical frames, filters potentially visible endpoints and builds the same
exact inverse planar homography already accepted by 1B.11A.

At `BeforeDOF`, current main `SceneDepth` must satisfy:

```text
Format == PF_DepthStencil
TexCreate_DepthStencilTargetable
```

Validation uses bounded bit:

```text
PortalStencilBit = 0x40
```

With `portal.StencilIdentityIsolateBit 1`, every active main-view `BeforeDOF`
frame clears **only bit 0x40** while preserving all other stencil bits. Then each
visible aperture is rasterized with the exact projective ellipse and writes:

```text
Depth write   = OFF
Depth test    = ALWAYS
Stencil test  = ALWAYS
Stencil op    = REPLACE
Read mask     = 0x40
Write mask    = 0x40
Stencil ref   = 0x40
```

The identity therefore exists in the real main depth/stencil target, not in a
sidecar mask texture.

## Downstream hardware proof

The old `AfterDOF` cyan overlay was rejected as an acceptance surface because it
fed the pre-tonemap HDR/exposure chain and could wash the scene white.

The accepted proof keeps:

```text
portal.StencilIdentityDebug 0
```

and uses the independent Tonemap validator. It does not recompute the aperture and
does not write stencil. At the real Tonemap callback it binds current main
`SceneDepth` as `StencilRead` and uses:

```text
Stencil test  = EQUAL
Read mask     = 0x40
Stencil ref   = 0x40
Stencil write = OFF
```

The user reran this path and explicitly reported that the main scene no longer
blew out / turned white. Sustained logs reported:

```text
PortalStencilTonemapProof Tonemap ...
PassEnabled=1
StencilTargetable=1
StencilBit=0x40
```

Therefore the exposure-safe downstream hardware stencil test is accepted.

## Zero-visible lifecycle defect and fix

The original writer skipped its `BeforeDOF` callback when no visible aperture
snapshot existed. That could leave a stale `0x40` mark from the previous frame.

Fix commit:

```text
3391df2b63d5bcc9ba1c90ef3afaf32e1c42de9c
portal: clear stencil identity on zero-visible frames
```

The writer now follows:

```text
every active main-view BeforeDOF frame
    -> clear only stencil bit 0x40
    -> VisibleApertures > 0: mark current aperture(s)
    -> VisibleApertures == 0: stop after clear
```

Telemetry:

```text
ZeroVisibleClear=0/1
ZeroVisibleClears=<count>
zeroVisibleClearFrames=<report field>
```

## Zero-visible runtime acceptance evidence

The final user run proved the lifecycle fix under sustained runtime.
Before the explicit helper was even started, the view naturally reached:

```text
VisibleApertures=0
ZeroVisibleClear=1
ZeroVisibleClears=3
```

After `portal.BeginStencilZeroVisibleTest`, both endpoints were temporarily forced
unplaced and the writer repeatedly reported zero-visible clears:

```text
ZeroVisibleClears=60
ZeroVisibleClears=120
ZeroVisibleClears=180
...
ZeroVisibleClears=1080
```

The final report recorded:

```text
SetupView=2296
Writes=1691
Overlays=0
ZeroVisibleClears=1511
VisibleApertures=0
Targetable=1
AfterDOFEnabled=0
Isolated=1
```

The independent Tonemap proof remained active throughout and completed:

```text
Frames=1690
Targetable=1
TonemapEnabled=1
SceneRect=2283x1065
```

This accepts the zero-visible lifetime contract: the bounded stencil bit is
actively cleared on frames with no visible portal and cannot remain stale merely
because the aperture snapshot is empty.

After `portal.EndStencilZeroVisibleTest`, the helper restored endpoint placement
with `Blue=1 Orange=1`. The subsequent `VisibleApertures=0` lines are not evidence
of restoration failure: this counter means *currently visible in the current
view*, and the same run had already naturally entered zero-visible state before
the helper began.

## Remaining acceptance item

Only one visual acceptance item remains before declaring the whole 1B.12D-A gate
PASS:

```text
while one portal is visible and the post-tonemap proof is running,
cyan must be confined to the exact projective portal aperture;
wall / floor / weapon / HUD must remain non-cyan.
```

No movement is required. A single static screenshot is sufficient because the
projective/grazing geometry itself was already accepted in 1B.11A and the
zero-visible lifetime has now been independently accepted by telemetry.

Minimal final visual procedure:

```text
portal.StencilIdentityIsolateBit 1
portal.StencilIdentityDiagnostics 1
portal.StencilIdentityDebug 0
portal.StartStencilIdentityValidation

portal.StencilIdentityTonemapDiagnostics 1
portal.StartStencilIdentityTonemapValidation
```

Take one screenshot with a visible portal. Expected:

```text
portal aperture = cyan
unrelated scene = normal color
no whole-screen exposure shift
```

Then stop:

```text
portal.StopStencilIdentityTonemapValidation
portal.StopStencilIdentityValidation
```

## PASS criteria

1. Main SceneDepth is stencil-targetable.
2. Visible aperture frames isolate and write bit `0x40`.
3. Real downstream Tonemap `CF_Equal` stencil testing executes.
4. Tonemap proof does not alter main exposure.
5. Zero-visible frames execute the isolate clear and `ZeroVisibleClears` rises.
6. No stale bit survives solely because `VisibleApertures=0`.
7. Static Tonemap proof cyan is confined to the actual portal aperture.
8. No RDG / D3D12 / resource-lifetime failure occurs.

Items 1–6 and 8 are accepted. Item 7 is the only remaining evidence request.

## What PASS will prove

```text
linked portal aperture
    -> exact projective rasterization
    -> isolated masked write into current main stencil
    -> zero-visible per-frame cleanup
    -> stencil survives downstream
    -> real hardware CF_Equal test after Tonemap
    -> exposure-safe explicit aperture identity
```

## What PASS will not prove

```text
permanent conflict-free engine-wide ownership of bit 0x40
normal portal composition gated by stencil
stencil-aware HZB rebuild
SSR / Lumen screen-trace integration
translucent remote depth
recursion >= 2
multiple simultaneously visible portal histories
portal-bounded resource/scissor performance acceptance
Core Portal Fidelity Seal
```

## Next gate after PASS

Promote the accepted stencil identity from diagnostic-only to a **bounded normal
portal composition gate**, default-off behind a validation CVar first. After that,
move to portal-scissored / bounded resource hardening. Recursion remains later.
