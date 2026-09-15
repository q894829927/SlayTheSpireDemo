# Interior Portal — STEP 1B.12D-A Post-Tonemap Stencil Isolation Retest

Date: **2026-09-16**

State:

```text
REAL MAIN STENCIL WRITE/READ = PASS
AFTER-DOF COLOR PROOF = REJECTED AS EXPOSURE-CONTAMINATING
POST-TONEMAP EXPOSURE-SAFE HARDWARE PROOF = PASS
ZERO-VISIBLE STENCIL LIFETIME = PASS
FINAL STATIC VISIBLE-APERTURE CONFINEMENT SCREENSHOT REQUIRED
```

## Purpose

The original stencil identity writer successfully reached the current main
Depth/Stencil resource, but its `AfterDOF` cyan proof altered the pre-tonemap HDR
exposure chain and could wash the view white.

The independent Tonemap validator separates resource proof from visualization. It
keeps the real stencil writer active with:

```text
portal.StencilIdentityDebug 0
```

and visualizes the already-written bit only at the real Tonemap callback.

## Implementation

Source:

```text
Source/SlayTheSpireDemo/Interior/InteriorPortalStencilTonemapValidation.cpp
```

The validator:

1. skips additional secondary view families;
2. subscribes to `EPostProcessingPass::Tonemap`;
3. rebinds current main SceneDepth as `StencilRead`;
4. uses a real `CF_Equal` hardware test on bit `0x40`;
5. emits display-domain cyan without recomputing portal geometry.

The test state is:

```text
Stencil test  = EQUAL
Read mask     = 0x40
Stencil ref   = 0x40
Stencil write = OFF
```

## Exposure-safe runtime evidence

The user reran the proof with the writer active, bit isolation enabled and the
legacy `AfterDOF` overlay disabled. The main scene no longer blew out / turned
white.

Sustained telemetry reported:

```text
PortalStencilTonemapProof Tonemap ...
PassEnabled=1
StencilTargetable=1
StencilBit=0x40
```

The earlier accepted Tonemap report completed 472 proof frames. The later
zero-visible lifecycle run completed:

```text
Frames=1690
Targetable=1
TonemapEnabled=1
SceneRect=2283x1065
```

Therefore the post-tonemap hardware stencil proof is accepted as exposure-safe.

## Zero-visible lifecycle acceptance

The stencil writer was fixed so bit `0x40` is isolated on **every** active main
`BeforeDOF` frame, including frames with no visible aperture.

The final runtime test first naturally reached:

```text
VisibleApertures=0
ZeroVisibleClear=1
ZeroVisibleClears=3
```

Then `portal.BeginStencilZeroVisibleTest` forced both endpoints unplaced without
requiring player movement. The counter increased continuously:

```text
ZeroVisibleClears=60
ZeroVisibleClears=120
...
ZeroVisibleClears=1080
```

Final writer report:

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

This closes the stale-bit lifecycle concern. `portal.EndStencilZeroVisibleTest`
restored endpoint placement with `Blue=1 Orange=1`; a later
`VisibleApertures=0` only means no endpoint was visible in the current view.

## Remaining evidence

The whole 1B.12D-A gate still needs one static visual proof while an aperture is
visible:

```text
portal aperture = cyan
wall / floor / weapon / HUD = normal
no whole-screen exposure shift
```

No camera movement is required.

Minimal commands:

```text
portal.StencilIdentityIsolateBit 1
portal.StencilIdentityDebug 0
portal.StartStencilIdentityValidation
portal.StartStencilIdentityTonemapValidation
```

Take one screenshot with a portal visible, then stop both validators.

## Claim boundary

The accepted sub-gates prove that the project can write an isolated identity into
current main stencil, clear it on zero-visible frames and read it with a real
hardware stencil test after Tonemap without feeding main exposure.

They do not reserve `0x40` engine-wide, make normal composition stencil-gated,
rebuild HZB, integrate SSR/Lumen screen traces, accept recursion or accept
production performance. Normal-composition promotion starts only after the final
static confinement screenshot is accepted.
