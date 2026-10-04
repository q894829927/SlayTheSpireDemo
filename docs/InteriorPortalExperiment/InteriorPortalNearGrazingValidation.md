# Interior Portal — STEP 1B.11A Projective Aperture / Near-Grazing Validation

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
STEP 1B.12A MAIN SCENEDEPTH FOREGROUND OCCLUSION = NEXT
```

## Accepted implementation

The compositor no longer treats the conservative projected portal bounding box
as the true aperture shape. `InteriorPortalProjectiveAperture` constructs an
inverse 3x3 planar homography that maps normalized constrained-view screen UV
back to normalized portal-local coordinates:

```text
[u_h, v_h, w_h] = H^-1 * [screenU, screenV, 1]
[u, v]           = [u_h, v_h] / w_h
mask             = (u^2 + v^2 <= 1)
```

The authored ellipse therefore remains the actual aperture under oblique
perspective. `FPortalScreenBounds` remains only the conservative visibility /
future-scissor bound.

For perspective views `w_h = 1 / clipW`, so the composition shader also rejects
projective intersections behind the camera or before the player's near plane.
At a true singularity the projective mapping is marked invalid and the previous
bounds-ellipse path remains a bounded fallback.

Runtime comparison switch:

```text
portal.ProjectiveAperture 1   # exact projective aperture; accepted path
portal.ProjectiveAperture 0   # retained bounds-ellipse A/B path
```

## Runtime acceptance evidence

User PIE run on 2026-09-15:

```text
framesObserved                 = 1296
framesSkipped                  = 13
minPlaneDistance               = 1.48430585
minFacingAbsDot                = 0.00124449318
nearClipFrameCount             = 0
cameraCrossingFrameCount       = 0
viewportClippedFrameCount      = 1065
grazingFrameCount              = 49
closeFrameCount                = 5
projectiveInvalidFrameCount    = 0
lastProjectiveApertureValid    = true
lastProjectiveDeterminantQuality = 0.0739796832
```

The strongest evidence is the combination of:

```text
minFacingAbsDot ~= 0.00124
49 grazing frames
1065 viewport-clipped frames
5 close-range frames
0 projective-invalid frames
```

The view reached an almost edge-on portal orientation while the inverse
homography remained finite and valid. The portal also remained stable while
partially clipped by the viewport.

A same-camera `portal.ProjectiveAperture 0` / `1` visual A/B showed no new
aperture breakage, jumping, missing wedge or gross rim mismatch. At the tested
pose the old and new shapes are visually close, which is expected because that
particular projected conic remains approximately vertical and elliptical. The
telemetry provides the stronger grazing/edge stability evidence.

## Bounded PASS decision

```text
Projective inverse-homography aperture     PASS
Oblique / grazing stability               PASS
Viewport partial clipping                 PASS
Very-close projective stability           PASS
Projective invalid fallback                NOT TRIGGERED (0 invalid frames)
Near-plane intersection                    NOT EXERCISED
Camera-plane crossing                      NOT EXERCISED
Overall STEP 1B.11A                        PASS
```

The run did not intentionally hit conservative `bIntersectsNearClip` or
`bCameraCrossing`, so those counters remain zero. This does not block the
bounded 1B.11A acceptance because close, grazing and viewport-edge behavior were
strongly exercised and no projective instability occurred. A later traversal
acceptance pass may still revisit the exact crossing frame if needed.

## Retained diagnostics

```text
portal.NearGrazingDiagnostics 1
portal.StartNearGrazingDiagnostics
portal.DumpNearGrazingDiagnostics
portal.StopNearGrazingDiagnostics
```

Report:

```text
Saved/AutomationReports/PortalNearGrazingDiagnostics.json
```

`portal.CompositionDiagnostics 1` also reports:

```text
Projective
ProjectiveValid
ProjectiveQuality
NearClipW
NearClip
Crossing
ViewportClip
```

## What this fixes — and what it does not

This step fixes the 2D physical aperture projection and hardens it for strong
oblique / grazing and screen-edge views. It does not make secondary portal
content participate in the player's main depth-stencil buffer.

The next gate is therefore:

```text
STEP 1B.12A — MAIN SCENEDEPTH FOREGROUND OCCLUSION
```

That gate uses the real main SceneDepth to preserve foreground main-view objects
that are physically in front of the entry portal plane. Remote depth transport,
true main depth injection and main stencil ownership remain later boundaries.

## Claim boundary

Still out of scope:

```text
remote secondary depth injection into main SceneDepth
main stencil aperture write
depth-correct DOF / fog / SSR / HZB through portal
portal-bounded secondary renderer scissor
multi-visible-portal history ownership
recursion >= 2
dynamic resolution controller
production GPU/VRAM acceptance
Core Portal Fidelity Seal
```
