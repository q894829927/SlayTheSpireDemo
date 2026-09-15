# Interior Portal — STEP 1B.12D-A Post-Tonemap Stencil Isolation Retest

Date: **2026-09-16**

State:

```text
STEP 1B.12D-A MAIN STENCIL APERTURE IDENTITY
= REAL STENCIL WRITE/READ REACHED
= AFTER-DOF COLOR PROOF CONTAMINATES EXPOSURE
= POST-TONEMAP EXPOSURE-SAFE PROOF = PASS
= ZERO-VISIBLE STENCIL LIFETIME RETEST STILL REQUIRED
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

The original `AfterDOF` cyan overlay produced a nearly white / strongly clipped
main image. Runtime telemetry showed that proof running in the pre-tonemap HDR
exposure domain, so the stencil resource path was separated from visualization.

The existing validator remains the stencil writer with:

```text
portal.StencilIdentityDebug 0
```

and this independent extension visualizes the already-written identity only at
the real `Tonemap` post-processing callback.

## Implementation

Source:

```text
Source/SlayTheSpireDemo/Interior/InteriorPortalStencilTonemapValidation.cpp
```

It does not recompute portal geometry and does not write stencil. It only:

1. subscribes to the real main-view `Tonemap` pass;
2. rebinds current main `SceneDepth` as `StencilRead`;
3. uses a real `CF_Equal` test with read mask `0x40`, stencil ref `0x40`;
4. draws display-domain cyan where the stencil test passes;
5. skips additional secondary view families.

It reuses `StencilOverlayPS` with `OverlayPreExposure=1.0`, because this callback is
after Tonemap and therefore authors display-domain diagnostic color.

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

## Runtime acceptance evidence

The user reran the proof with the original writer active, bit isolation enabled,
and the legacy `AfterDOF` overlay disabled.

The writer repeatedly reported:

```text
VisibleApertures=1
StencilTargetable=1
Isolated=1
```

The Tonemap proof repeatedly reported:

```text
PortalStencilTonemapProof Tonemap ...
PassEnabled=1
StencilTargetable=1
StencilBit=0x40
SceneRect=2278x1061
```

Final Tonemap report evidence:

```text
Frames=472
Targetable=1
TonemapEnabled=1
SceneRect=2278x1061
```

The user explicitly reported that this run **did not turn the scene white / blow
out exposure**. Therefore the post-tonemap proof successfully removes the
exposure-feedback failure of the old `AfterDOF` visualization.

This establishes:

```text
main stencil identity write
    -> stencil survives downstream
    -> real hardware CF_Equal test at Tonemap
    -> exposure-safe display-domain diagnostic path
```

It does **not** by itself close the whole 1B.12D-A gate because the same run also
reached `VisibleApertures=0`, exposing a separate zero-visible lifetime concern in
the writer. That concern is fixed in commit:

```text
3391df2b63d5bcc9ba1c90ef3afaf32e1c42de9c
portal: clear stencil identity on zero-visible frames
```

A final retest must prove that zero-visible frames continue to run the isolate
clear and do not leave stale / ghost cyan identity.

## Final zero-visible retest

Keep the original writer active with:

```text
portal.StencilIdentityIsolateBit 1
portal.StencilIdentityDiagnostics 1
portal.StencilIdentityDebug 0
portal.StartStencilIdentityValidation
```

Start this Tonemap proof:

```text
portal.StencilIdentityTonemapDiagnostics 1
portal.StartStencilIdentityTonemapValidation
```

Face a portal, then move/turn until no portal is visible. Required writer evidence:

```text
PortalStencilIdentity BeforeDOF ...
VisibleApertures=0
Isolated=1
ZeroVisibleClear=1
ZeroVisibleClears=N
```

`ZeroVisibleClears` must continue increasing while no portal is visible. The
post-tonemap proof must show no stale / ghost cyan during that interval. Turning
back toward the portal must restore current aperture identity normally.

Finish with:

```text
portal.DumpStencilIdentityTonemapValidation
portal.StopStencilIdentityTonemapValidation
portal.DumpStencilIdentityValidation
portal.StopStencilIdentityValidation
```

## Claim boundary

The post-tonemap proof PASS establishes only that the project can read the already
written main-stencil identity with a real hardware test downstream of Tonemap
without feeding main exposure. It does not reserve `0x40` engine-wide, make normal
portal composition stencil-gated, rebuild HZB, integrate SSR/Lumen screen traces,
accept recursion, or accept production performance. Full 1B.12D-A acceptance still
requires the zero-visible lifetime retest documented above.
