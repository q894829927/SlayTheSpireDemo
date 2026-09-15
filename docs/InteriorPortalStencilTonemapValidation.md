# Interior Portal — STEP 1B.12D-A Post-Tonemap Stencil Isolation Retest

Date: **2026-09-16**

State:

```text
STEP 1B.12D-A MAIN STENCIL APERTURE IDENTITY
= REAL STENCIL WRITE/READ REACHED
= AFTER-DOF COLOR PROOF CONTAMINATES EXPOSURE
= POST-TONEMAP ISOLATION RETEST IMPLEMENTED / NOT YET BUILT OR RUN
```

## Why this retest exists

The hardened main-stencil validator already established sustained runtime evidence that:

```text
visible aperture snapshot exists
BeforeDOF main SceneDepth is PF_DepthStencil and stencil-targetable
bit 0x40 is isolated before marking
bit 0x40 is written for visible apertures
real downstream CF_Equal stencil testing executes
```

However, the second user retest still produced a nearly white / strongly clipped main image after enabling the `AfterDOF` cyan overlay. Runtime telemetry showed the overlay callback running with a rapidly changing very small main pre-exposure (roughly 0.0017 down to 0.00019). This means the visual proof itself is not exposure-neutral enough to be used as the acceptance surface.

The stencil resource path is therefore separated from the visualization path. The existing validator remains the stencil writer, with its debug overlay disabled. A new independent extension visualizes the already-written stencil identity only at the real `Tonemap` post-processing callback.

UE 5.8's `SubscribeToPostProcessingPass` callback is an after-pass event. Using `EPostProcessingPass::Tonemap` therefore moves the cyan proof out of the pre-tonemap HDR / eye-adaptation input domain.

## Implementation

New source:

```text
Source/SlayTheSpireDemo/Interior/InteriorPortalStencilTonemapValidation.cpp
```

It does not recompute portal geometry and does not write stencil. It only:

1. subscribes to the real main-view `Tonemap` pass;
2. rebinds current main `SceneDepth` as `StencilRead`;
3. uses a real `CF_Equal` test with read mask `0x40`, stencil ref `0x40`;
4. draws display-domain cyan where the stencil test passes;
5. skips additional secondary view families.

It deliberately reuses `StencilOverlayPS` with `OverlayPreExposure=1.0`, because this callback is after Tonemap and should author display-domain diagnostic color rather than pre-exposed HDR color.

## Controls

```text
portal.StencilIdentityTonemapValidation 0/1
portal.StencilIdentityTonemapDiagnostics 0/1
```

Commands:

```text
portal.StartStencilIdentityTonemapValidation
portal.DumpStencilIdentityTonemapValidation
portal.StopStencilIdentityTonemapValidation
```

Report:

```text
Saved/AutomationReports/PortalStencilTonemapValidation.json
```

## Validation procedure

Close Unreal Editor and rebuild because this retest adds a new compiled source file.

The original stencil writer must remain active, but its exposure-contaminating `AfterDOF` debug overlay must be disabled:

```text
portal.StencilIdentityIsolateBit 1
portal.StencilIdentityDiagnostics 1
portal.StencilIdentityDebug 0
portal.StartStencilIdentityValidation
```

Wait until the original validator reports:

```text
PortalStencilIdentity BeforeDOF ... VisibleApertures=1 ... StencilTargetable=1 ... Isolated=1
```

Then start the new post-tonemap proof:

```text
portal.StencilIdentityTonemapDiagnostics 1
portal.StartStencilIdentityTonemapValidation
```

Expected periodic evidence:

```text
PortalStencilTonemapProof Tonemap ... PassEnabled=1 ... StencilTargetable=1 StencilBit=0x40
```

Expected visual result:

```text
normal scene exposure remains stable
visible portal aperture = solid cyan
unrelated wall/floor/weapon/HUD = not cyan
cyan follows oblique/projective aperture while moving
```

If the post-tonemap proof is confined to the portal while the rest of the scene remains normally exposed, then 1B.12D-A may be accepted as a bounded main-stencil aperture-identity feasibility proof.

If the post-tonemap proof still covers unrelated scene regions while bit isolation is active, the failure is a real stencil marking/test problem rather than an exposure-domain artifact and must be fixed before production promotion.

Finish with:

```text
portal.DumpStencilIdentityTonemapValidation
portal.StopStencilIdentityTonemapValidation
portal.DumpStencilIdentityValidation
portal.StopStencilIdentityValidation
```

## Claim boundary

A PASS proves only that the project can create an isolated aperture identity in current main stencil and verify it with a real hardware stencil test downstream of Tonemap. It still does not reserve `0x40` engine-wide, make normal portal composition stencil-gated, rebuild HZB, integrate SSR/Lumen screen traces, accept recursion, or accept production performance.
