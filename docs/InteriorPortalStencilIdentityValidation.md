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
= MAIN STENCIL WRITE/READ REACHED
= POST-TONEMAP EXPOSURE-SAFE PROOF REACHED
= ZERO-VISIBLE LIFETIME FIX IMPLEMENTED / RETEST REQUIRED
```

## Goal

The accepted portal path already has exact projective aperture math, transported
remote depth, a legal main SceneDepth write and a real downstream DOF consumer.
The aperture is still identified primarily by shader-side projective math.

1B.12D-A asks:

```text
Can project-side public RDG code assign the linked physical portal apertures an
explicit identity in THE CURRENT MAIN depth/stencil attachment, then prove that
identity with a real hardware stencil comparison rather than by resampling the
projective mask in a color shader?
```

This is a bounded stencil-feasibility gate. It does **not** yet make normal portal
composition depend on stencil and does not claim a permanent engine-wide
stencil-bit reservation.

## Implementation

Writer / identity validator:

```text
Source/SlayTheSpireDemo/Interior/InteriorPortalStencilIdentityValidation.cpp
Shaders/InteriorPortalStencilIdentity.usf
```

Exposure-safe downstream proof:

```text
Source/SlayTheSpireDemo/Interior/InteriorPortalStencilTonemapValidation.cpp
docs/InteriorPortalStencilTonemapValidation.md
```

On the main player view only, `SetupView` snapshots linked portal logical frames,
rejects apertures that are not potentially visible, and builds the same exact
inverse planar homography already accepted by 1B.11A. Additional secondary view
families are ignored.

At `BeforeDOF` the writer obtains the current main `SceneDepth` and requires:

```text
Format == PF_DepthStencil
TexCreate_DepthStencilTargetable
```

With validation isolation enabled, bit `0x40` is cleared with a masked stencil
replace while preserving all other stencil bits. Every valid visible linked
aperture is then rasterized with the exact projective ellipse and writes only:

```text
Depth write  = OFF
Depth test   = ALWAYS
Stencil test = ALWAYS
Stencil op   = REPLACE on pass
Read mask    = 0x40
Write mask   = 0x40
Stencil ref  = 0x40
```

The identity is therefore physically stored in the current main depth/stencil
target rather than a sidecar texture.

## Bit-allocation boundary

The feasibility spike uses:

```text
PortalStencilBit = 0x40
```

`0x40` is **not** production-owned. Isolation is validation-only and does not
constitute an engine-wide reservation or collision policy.

## Exposure-safe hardware stencil proof

The original `AfterDOF` color overlay reached the real stencil resource but fed
pre-tonemap HDR / exposure state and could wash out the main view. That visual
proof was therefore separated from the writer.

The accepted proof path keeps:

```text
portal.StencilIdentityDebug 0
```

and runs the independent Tonemap validator. It does not recompute portal geometry
or write stencil. At the real Tonemap callback it binds current main SceneDepth as
`StencilRead` and uses:

```text
Stencil test = EQUAL
Read mask    = 0x40
Stencil ref  = 0x40
Stencil write = OFF
```

The cyan proof is authored after Tonemap, so it cannot feed main eye adaptation.

## Runtime evidence so far

The second runtime retest established the exposure-safe downstream path.
The writer repeatedly reported:

```text
VisibleApertures=1
StencilTargetable=1
Isolated=1
```

The independent Tonemap proof repeatedly reported:

```text
PassEnabled=1
StencilTargetable=1
StencilBit=0x40
SceneRect=2278x1061
```

The user explicitly reported that this retest **did not blow out / turn the scene
white**. The Tonemap validator completed 472 proof frames with targetable stencil
and the real Tonemap callback enabled.

The writer's final report for that run recorded:

```text
SetupView=1519
Writes=623
Overlays=0
VisibleApertures=0
Targetable=1
AfterDOFEnabled=0
Isolated=1
```

This is strong evidence that the main-stencil write path and a real downstream
hardware stencil read are both reachable without the previous exposure failure.

## Zero-visible lifetime defect found

The same run exposed a lifecycle edge case:

```text
SetupView ... VisibleApertures=0
```

The previous writer registered its `BeforeDOF` callback only when at least one
snapshot existed. Therefore a zero-visible frame skipped the isolate clear. A
`0x40` mark from a previous visible frame could theoretically survive and become a
stale / ghost portal identity.

That is a real code defect even without a manual ghost repro.

## Zero-visible lifetime fix

Commit:

```text
3391df2b63d5bcc9ba1c90ef3afaf32e1c42de9c
portal: clear stencil identity on zero-visible frames
```

The writer now always registers its active main-view `BeforeDOF` callback.
When isolation is enabled:

```text
every active main-view BeforeDOF frame
    -> clear only stencil bit 0x40
    -> if VisibleApertures > 0: mark current apertures
    -> if VisibleApertures == 0: stop after clear
```

New telemetry:

```text
ZeroVisibleClear=0/1
ZeroVisibleClears=<count>
```

and report field:

```text
zeroVisibleClearFrames
```

This makes the zero-visible lifetime contract directly observable instead of
requiring visual inference.

## Validation procedure after zero-visible fix

Close Unreal Editor, pull and rebuild. Enter PIE and keep the legacy `AfterDOF`
overlay disabled:

```text
portal.StencilIdentityIsolateBit 1
portal.StencilIdentityDiagnostics 1
portal.StencilIdentityDebug 0
portal.StartStencilIdentityValidation
```

Start the exposure-safe downstream proof:

```text
portal.StencilIdentityTonemapDiagnostics 1
portal.StartStencilIdentityTonemapValidation
```

When returning input focus to PIE, click the game viewport after entering console
commands. The validators do not intentionally modify input mode or pause the game.

Face a portal first, then turn until all portals are outside the view. Required
zero-visible evidence is now a `BeforeDOF` line, not only a `SetupView` line:

```text
PortalStencilIdentity BeforeDOF ... VisibleApertures=0 ... Isolated=1 ZeroVisibleClear=1 ZeroVisibleClears=N
```

Keep the camera away from the portal for several frames and confirm
`ZeroVisibleClears` continues increasing. Turn back to the portal and confirm
`VisibleApertures=1` returns and the post-tonemap proof follows the aperture.

Finish with:

```text
portal.DumpStencilIdentityTonemapValidation
portal.StopStencilIdentityTonemapValidation
portal.DumpStencilIdentityValidation
portal.StopStencilIdentityValidation
```

## PASS criteria

PASS requires all of the following:

1. C++ and shaders compile successfully.
2. Visible portal frames repeatedly report `StencilTargetable=1` and `Isolated=1`.
3. The real Tonemap proof repeatedly executes with `PassEnabled=1` and
   `StencilTargetable=1` without changing main scene exposure.
4. The proof is confined to the actual projective portal aperture(s), including
   oblique viewing angles.
5. Zero-visible frames still execute `BeforeDOF` isolation and report
   `ZeroVisibleClear=1` with an increasing `ZeroVisibleClears` count.
6. No stale cyan / ghost aperture remains when `VisibleApertures=0`.
7. Returning the portal to view restores the current aperture identity normally.
8. No render-thread/RDG/D3D12/resource-lifetime failure occurs while moving.

## What PASS proves

A PASS proves the bounded chain:

```text
linked visible portal logical aperture
    -> exact projective rasterization
    -> per-frame isolated masked write into current main stencil bit
    -> zero-visible frame cleanup
    -> stencil survives downstream
    -> hardware CF_Equal stencil test after Tonemap
    -> exposure-safe visible aperture identity
```

## What PASS does NOT prove

It does not yet prove:

```text
permanent conflict-free engine-wide ownership of bit 0x40
normal portal composition gated by stencil
stencil-aware HZB / occlusion hierarchy rebuild
SSR / Lumen screen-trace integration
translucent remote depth
recursion >= 2
multiple simultaneous visible portal histories
portal-bounded resource/scissor performance acceptance
Core Portal Fidelity Seal
```

## Next gate after PASS

After 1B.12D-A is accepted, promote the stencil identity from diagnostic-only to a
bounded normal composition gate. Then move to portal-scissored / bounded resource
hardening. Do not start recursion before aperture identity and bounded rendering
are stable.
