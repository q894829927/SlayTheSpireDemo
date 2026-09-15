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
STEP 1B.12D-A MAIN STENCIL APERTURE IDENTITY = IMPLEMENTED / NOT YET BUILT OR RUN
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

On the main player view only, `SetupView` snapshots both linked portal logical
frames and builds the same exact inverse planar homography already accepted by
1B.11A. Additional secondary view families are ignored.

At `BeforeDOF` the extension obtains the current main `SceneDepth` resource and
requires:

```text
Format == PF_DepthStencil
TexCreate_DepthStencilTargetable
```

For every valid linked aperture it draws a fullscreen rectangle whose pixel
shader discards everything outside the exact projective ellipse. Color and depth
are untouched. The depth/stencil state performs only a masked stencil replace:

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

## Bit-allocation boundary

The feasibility spike uses:

```text
PortalStencilBit = 0x40
```

`0x80` is intentionally avoided because UE renderer paths such as the SSR stencil
pre-pass use that value on supported configurations.

`0x40` is **not** declared production-owned by this project yet. The spike does
not clear this bit globally. That is intentional: if another active renderer
feature already owns `0x40`, the verification overlay can reveal unexpected cyan
outside the portal apertures instead of silently destroying somebody else's
stencil state.

Production promotion requires an explicit collision policy / renderer ownership
contract after this feasibility proof.

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

This distinction is the purpose of the gate. Projective math decides where to
write the identity; the later proof is driven by hardware stencil ownership.

Unexpected cyan outside the linked portal apertures is a stencil-bit collision or
stale-bit failure signal.

## Controls

```text
portal.StencilIdentityValidation 0/1
portal.StencilIdentityDebug 0/1
portal.StencilIdentityDiagnostics 0/1
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

Periodic expected logs:

```text
PortalStencilIdentity SetupView ... Apertures=2 ... StencilBit=0x40
PortalStencilIdentity BeforeDOF ... StencilTargetable=1 ... StencilBit=0x40
PortalStencilIdentity AfterDOF ... PassEnabled=1 ... StencilBit=0x40
```

## Validation procedure

Close Unreal Editor before rebuilding because the gate adds a new compiled source
file and new global shaders.

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

Then run the stencil gate:

```text
portal.StencilIdentityDebug 1
portal.StencilIdentityDiagnostics 1
portal.StartStencilIdentityValidation
```

Expected visual result:

1. The linked physical portal aperture(s) become solid cyan in the main view.
2. Cyan follows the exact oblique/projective ellipse while moving laterally.
3. Cyan does not fill the conservative rectangular projected bounds.
4. Cyan does not appear on unrelated walls, floor, weapon, HUD or arbitrary
   screen regions.
5. At grazing angles the cyan identity remains attached to the portal aperture
   until the projective mapping becomes genuinely singular / behind the view.
6. No D3D12/RDG depth-stencil assertion occurs.

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
2. `SetupView` reports at least one valid linked aperture; normally two mappings
   are available for the linked pair.
3. `BeforeDOF` repeatedly reports `StencilTargetable=1`.
4. `AfterDOF` repeatedly executes with `PassEnabled=1` while debug overlay is on.
5. Cyan is confined to the actual projective portal aperture(s), including at
   oblique viewing angles.
6. No unexpected cyan appears elsewhere in the main view; otherwise `0x40` is
   considered collided/contaminated for this renderer configuration.
7. Disabling `portal.StencilIdentityDebug` restores normal portal presentation
   while the validation extension remains active.
8. No render-thread/RDG/D3D12/resource-lifetime failure occurs while moving.

## What PASS proves

A PASS proves the bounded chain:

```text
linked portal logical aperture
    -> exact projective rasterization
    -> masked write into current main stencil bit
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
