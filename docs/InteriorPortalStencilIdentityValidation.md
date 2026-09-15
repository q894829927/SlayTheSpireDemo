# Interior Portal — STEP 1B.12D-A Main Stencil Aperture Identity Validation

Date: **2026-09-15**

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
STEP 1B.12D-A MAIN STENCIL APERTURE IDENTITY = RUNTIME REACHED / HARDENED / RETEST REQUIRED
```

## Goal

The accepted portal path already has exact projective aperture math, transported
remote depth, a legal main SceneDepth write and a real downstream DOF consumer.
The aperture is still identified primarily by shader-side projective math.

1B.12D-A asks the next renderer question:

```text
Can project-side public RDG code assign the linked physical portal apertures an
explicit identity in THE CURRENT MAIN depth/stencil attachment, then prove that
identity with a real hardware stencil comparison rather than by resampling the
projective mask in a color shader?
```

This is deliberately a bounded stencil-feasibility gate. It does **not** yet make
normal portal composition depend on stencil and does not claim a permanent
engine-wide stencil-bit reservation.

## Implementation

The validation extension lives in:

```text
Source/SlayTheSpireDemo/Interior/InteriorPortalStencilIdentityValidation.cpp
```

Shader:

```text
Shaders/InteriorPortalStencilIdentity.usf
```

On the main player view only, `SetupView` snapshots linked portal logical frames,
rejects apertures that are not potentially visible, and builds the same exact
inverse planar homography already accepted by 1B.11A. Additional secondary view
families are ignored.

At `BeforeDOF` the extension obtains the current main `SceneDepth` resource and
requires:

```text
Format == PF_DepthStencil
TexCreate_DepthStencilTargetable
```

For every valid visible linked aperture it draws a fullscreen rectangle whose
pixel shader discards everything outside the exact projective ellipse. Color and
depth are untouched. The depth/stencil state performs only a masked stencil
replace:

```text
Depth write  = OFF
Depth test   = ALWAYS
Stencil test = ALWAYS
Stencil op   = REPLACE on pass
Read mask    = 0x40
Write mask   = 0x40
Stencil ref  = 0x40
```

This means the bit is physically stored in the current main depth/stencil target,
not in a sidecar texture.

## Bit-allocation boundary and validation isolation

The feasibility spike uses:

```text
PortalStencilBit = 0x40
```

`0x80` is intentionally avoided because UE renderer paths such as the SSR stencil
pre-pass use that value on supported configurations.

`0x40` is **not** declared production-owned by this project. The first runtime
attempt proved that relying on the pre-existing value of an unreserved bit is not
a valid diagnostic assumption: the real stencil-tested overlay appeared broadly
outside the intended apertures.

The hardened validation path therefore adds:

```text
portal.StencilIdentityIsolateBit 1
```

When enabled, the validator clears **only bit 0x40** across the current main view
immediately before marking the visible portal apertures. The clear uses a masked
stencil replace with reference zero, so all other stencil bits are preserved.
This is validation isolation only; it is **not** a production engine-wide
ownership policy.

Production promotion still requires an explicit collision / renderer ownership
contract.

## Real stencil verification

When debug visualization is enabled, an `AfterDOF` pass binds the same main
SceneDepth as `StencilRead` and draws cyan with a real stencil state:

```text
Stencil test = EQUAL
Read mask    = 0x40
Stencil ref  = 0x40
Stencil write = OFF
```

The overlay shader itself does not recompute the portal shape. Therefore:

```text
cyan pixel = the real main stencil comparison passed
```

The first runtime attempt also exposed an HDR-domain problem in the proof overlay:
raw `float3(0,1,1)` was emitted into a pre-exposed HDR SceneColor domain and was
then strongly overexposed by the later main exposure / tonemap path. The hardened
overlay now scales diagnostic cyan by the current main-view pre-exposure before
writing it at `AfterDOF`.

## Controls

```text
portal.StencilIdentityValidation 0/1
portal.StencilIdentityDebug 0/1
portal.StencilIdentityDiagnostics 0/1
portal.StencilIdentityIsolateBit 0/1
```

Commands:

```text
portal.StartStencilIdentityValidation
portal.DumpStencilIdentityValidation
portal.StopStencilIdentityValidation
```

Report:

```text
Saved/AutomationReports/PortalStencilIdentityValidation.json
```

Periodic expected logs after hardening:

```text
PortalStencilIdentity SetupView ... VisibleApertures=... StencilBit=0x40 IsolateBit=1
PortalStencilIdentity BeforeDOF ... VisibleApertures=... StencilTargetable=1 ... Isolated=1
PortalStencilIdentity AfterDOF ... PassEnabled=1 ... StencilBit=0x40 OverlayPreExposure=...
```

## First runtime evidence and why it is not a PASS

The first user run successfully reached the real main depth/stencil resource and
the real downstream stencil-tested overlay for a sustained run. The final report
recorded:

```text
setupViewFrames       = 3022
stencilWriteFrames    = 1142
stencilOverlayFrames  = 994
lastApertureCount     = 2
lastStencilTargetable = true
lastAfterDOFPassEnabled = true
stencilBit            = 64 (0x40)
```

Periodic logs likewise reported `StencilTargetable=1` and `AfterDOF PassEnabled=1`.
This proves the public RDG path can bind, write and later test the main stencil.

However, the debug frame was almost entirely blown out and the real stencil-tested
overlay was not confined to the intended portal apertures. That run therefore does
**not** satisfy visual identity acceptance.

Two hardening changes were made before the required retest:

```text
af7597bad285c7940533639a9d66713ecd508438
portal: harden stencil identity debug shader

f805a65c6192c746b35cee52e9c4b62d3a2e0e42
portal: isolate and cull main stencil identity validation
```

The retest must establish that masked isolation removes stray stencil matches and
that pre-exposure-correct diagnostic cyan no longer blows out the scene.

## Validation procedure

Close Unreal Editor before rebuilding because the hardening changed both C++
shader parameters and a Global Shader.

Pull and build the branch, enter PIE, make sure both portals are linked, and
restore normal display state from the previous DOF validation:

```text
show VisualizeDOF
portal.StopDOFDepthConsumerValidation
r.AntiAliasingMethod 4
portal.CompositionDebugMode 0
```

If the accepted full portal producer is not already running, start it normally:

```text
portal.ProjectiveAperture 1
portal.DepthAwareComposition 1
portal.SecondaryDepthRemap 1
portal.MainDepthPropagation 1
portal.StartFullViewFamilyTSRSpike
```

Then run the hardened stencil gate:

```text
portal.StencilIdentityIsolateBit 1
portal.StencilIdentityDebug 1
portal.StencilIdentityDiagnostics 1
portal.StartStencilIdentityValidation
```

Expected visual result:

1. Only the visible linked physical portal aperture(s) become solid cyan.
2. Cyan follows the exact oblique/projective ellipse while moving laterally.
3. Cyan does not fill the conservative rectangular projected bounds.
4. Cyan does not appear on unrelated walls, floor, weapon, HUD or arbitrary
   screen regions.
5. The scene remains normally exposed; enabling the overlay must not wash the
   whole main view toward white.
6. At grazing angles the cyan identity remains attached to the portal aperture
   until the projective mapping becomes genuinely singular / behind the view.
7. No D3D12/RDG depth-stencil assertion occurs.

The normal portal texture is intentionally hidden by the cyan proof overlay while
`portal.StencilIdentityDebug=1`. Disable only the overlay while keeping stencil
writes active with:

```text
portal.StencilIdentityDebug 0
```

Normal accepted portal RGB/depth behavior should then return without restarting
the validator.

Finally:

```text
portal.DumpStencilIdentityValidation
portal.StopStencilIdentityValidation
```

## PASS criteria

PASS requires all of the following:

1. C++ and shaders compile successfully.
2. `SetupView` reports at least one **visible** linked aperture.
3. `BeforeDOF` repeatedly reports `StencilTargetable=1` and `Isolated=1` for the
   isolated proof run.
4. `AfterDOF` repeatedly executes with `PassEnabled=1` while debug overlay is on.
5. The overlay remains in the normal exposure domain.
6. Cyan is confined to the actual projective portal aperture(s), including at
   oblique viewing angles.
7. No unexpected cyan appears elsewhere after bit isolation.
8. Disabling `portal.StencilIdentityDebug` restores normal portal presentation
   while the validation extension remains active.
9. No render-thread/RDG/D3D12/resource-lifetime failure occurs while moving.

## What PASS proves

A PASS proves the bounded chain:

```text
linked visible portal logical aperture
    -> exact projective rasterization
    -> isolated masked write into current main stencil bit
    -> stencil survives to AfterDOF
    -> hardware CF_Equal stencil test
    -> visible aperture identity
```

This is the first stage where the main renderer owns an explicit portal-aperture
identity independent of RGB content and independent of re-evaluating the mask in
the verification shader.

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

If 1B.12D-A passes, promote the identity from diagnostic-only to a bounded normal
composition gate, then move to portal-scissored/bounded resource hardening. Do not
start recursion before aperture identity and bounded rendering are stable.
