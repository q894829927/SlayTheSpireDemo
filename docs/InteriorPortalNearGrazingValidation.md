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
STEP 1B.11A PROJECTIVE APERTURE / NEAR-GRAZING HARDENING = IMPLEMENTED / NOT YET BUILT OR RUN
```

Implementation commits:

```text
23b18f861705016feacb3d55baac1b63948d2185  portal: add projective aperture homography helper
e3f184f3cd36d71d02394b140df7e4af83cc39d8  portal: carry exact projective aperture mapping in render request
c8452cdddafad2260b7a762923ef96bb88b3dd02  portal: compose aperture with projective portal-plane mapping
3e54bc3cbcb94da468e131c3f231c4cdc1ad306b  portal: carry main near plane for projective aperture clipping
06a31bcaa9e0fea103a047faff4ae13584b6f60f  portal: clip projective aperture against main near plane
384417144cc0eebe51d3e4994a21cee06710a1ed  portal: use exact projective aperture mask at grazing angles
be3d75e860b528068f20c09c3eb6a231b6907913  portal: add near and grazing aperture diagnostics
```

## Why this gate exists

The previous compositor used the conservative projected portal **bounding box**
and drew an axis-aligned ellipse inside that box. That is acceptable for a
head-on diagnostic, but it is not the projection of the authored planar ellipse
under perspective. At an oblique angle a planar ellipse becomes a projective
conic, so the old mask can visibly bulge outside or cut inside the physical
portal rim even when the secondary camera itself is correct.

This mismatch is one direct reason the portal can look different from standing
at the corresponding real scene position.

STEP 1B.11A keeps `FPortalScreenBounds` as the conservative visibility/scissor
contract but replaces the actual composition aperture with an exact portal-plane
mapping when that mapping is non-singular.

## Projective aperture contract

Portal local coordinates are normalized as:

```text
u = local Y / HalfWidth
v = local Z / HalfHeight
```

The authored aperture is therefore exactly:

```text
u^2 + v^2 <= 1
```

For the current main player view, three portal-plane basis points are projected
to clip space. Their linear homogeneous mapping forms a 3x3 plane-to-screen
homography `H`. The game thread computes `H^-1` and stores its three rows in the
immutable `FInteriorPortalRenderRequest`.

The BeforeDOF composition shader evaluates:

```text
[u_h, v_h, w_h] = H^-1 * [screenU, screenV, 1]
[u, v]           = [u_h, v_h] / w_h
mask             = (u^2 + v^2 <= 1)
```

The edge uses screen-space derivatives (`fwidth`) so a very thin grazing-angle
aperture remains anti-aliased instead of using a fixed normalized-box feather.

The inverse mapping also recovers the original perspective clip `W` because:

```text
w_h = 1 / clipW
```

For perspective views the shader therefore rejects aperture intersections whose
`clipW` is behind the player or before the main near plane. This is stricter than
using only the conservative near-clipped bounding box.

At an exact edge-on / camera-in-plane singularity the normalized homography
determinant becomes too small. The request marks the projective mapping invalid
and the compositor retains the previous bounds-ellipse path as a bounded
fallback rather than emitting NaN/Inf coordinates.

## Comparison switch

The new renderer CVar is:

```text
portal.ProjectiveAperture 1   # new exact projective mask; default
portal.ProjectiveAperture 0   # retained old bounds-ellipse A/B path
```

This is a geometric comparison switch, not an artistic control.

## Telemetry

`InteriorPortalNearGrazingDiagnostics.cpp` adds a monitor independent of the
secondary producer:

```text
portal.NearGrazingDiagnostics 1
portal.StartNearGrazingDiagnostics
portal.DumpNearGrazingDiagnostics
portal.StopNearGrazingDiagnostics
```

It writes:

```text
Saved/AutomationReports/PortalNearGrazingDiagnostics.json
```

The report records:

```text
framesObserved / framesSkipped
last + minimum camera-to-portal-plane distance
last + minimum abs(cameraForward dot portalNormal)
near-clip intersection count
camera-crossing count
viewport-clipped count
grazing-frame count
close-frame count
projective-invalid count
last projective determinant quality
last conservative projected bounds
```

`abs(cameraForward dot portalNormal)` approaches `0` at a grazing view and `1`
at a head-on view. `projectiveInvalidFrameCount` is allowed to increase only at
true singular/degenerate frames; it must not rise continuously during ordinary
oblique viewing.

The existing `portal.CompositionDiagnostics 1` log now also reports:

```text
Projective=...
ProjectiveValid=...
ProjectiveQuality=...
NearClipW=...
NearClip=...
Crossing=...
ViewportClip=...
```

## Validation procedure

Use the already accepted full-renderer TSR producer so this gate tests the real
current visual path rather than SceneCapture output:

```text
RendererBackend = SceneCapture
portal.CompositionDebugMode 0
portal.CompositionDiagnostics 1
portal.ProjectiveAperture 1
portal.FullViewFamilyTSRDiagnostics 1
portal.FullViewFamilyTSRPrimaryFraction 0.67
portal.NearGrazingDiagnostics 1
portal.StartFullViewFamilyTSRSpike
portal.StartNearGrazingDiagnostics
```

Exercise all of these views:

1. Head-on baseline.
2. Move very close to the portal plane, as close as collision/traversal allows.
3. Strafe sideways and rotate until the aperture is a very thin oblique conic.
4. Move/rotate until the portal is partially clipped by the left/right/top/bottom
   screen edge.
5. If normal traversal permits it, cross the portal while keeping the camera
   near the aperture for the frames immediately before and after transfer.

At one stable oblique view, do an explicit A/B without moving the camera:

```text
portal.ProjectiveAperture 0
# capture old bounds-ellipse appearance
portal.ProjectiveAperture 1
# capture exact projective appearance
```

Then, before stopping:

```text
portal.DumpNearGrazingDiagnostics
portal.DumpFullViewFamilyTSRSpike
```

Finally:

```text
portal.StopNearGrazingDiagnostics
portal.StopFullViewFamilyTSRSpike
```

## PASS criteria

PASS requires:

1. `Projective=1` and `ProjectiveValid=1` on ordinary head-on and oblique frames.
2. The projective aperture visually follows the physical portal rim at oblique
   angles; it must not remain an axis-aligned ellipse inside the projected box.
3. The portal stays stable when partially off screen.
4. Near-plane intersection must not expose portal pixels that belong behind the
   main camera/near plane.
5. No NaN/Inf, renderer assertion, shader failure or stale target occurs during
   close/grazing motion.
6. `projectiveInvalidFrameCount` must not grow continuously during ordinary
   viewing. A small number at true singular crossing/edge-on frames is allowed.
7. The already accepted TSR path continues to submit/extract frames with sparse
   camera cuts.

A run does not need to hit every diagnostic counter to prove the implementation
works, but final near/grazing acceptance should intentionally exercise at least
one strong oblique view and one screen-edge clip. If traversal geometry allows a
near-plane/crossing event, that should also be included.

## What this fixes — and what it does not

This gate fixes the **2D aperture projection** and main-view near-plane rejection
of the composition mask. It does not yet make the secondary image write correct
main-scene depth/stencil.

Therefore the next major gate after PASS remains:

```text
STEP 1B.11B / STEP 1C — MAIN DEPTH / STENCIL CONTINUITY
```

That stage is responsible for depth-aware occlusion and downstream effects that
need the portal interior to participate as real main-view geometry/depth rather
than only RGB SceneColor.

## Claim boundary

Still out of scope:

```text
main depth/stencil continuity
depth-correct DOF across the portal
portal-bounded secondary renderer scissor
multi-visible-portal history ownership
recursion >= 2
dynamic resolution controller
production GPU/VRAM acceptance
Core Portal Fidelity Seal
```
