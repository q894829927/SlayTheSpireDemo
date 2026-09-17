# Interior Portal — Grazing Ownership + Full-Fidelity Recursion Root Fix

Date: **2026-09-16**

State:

```text
GRAZING SPIRAL REGRESSION = ROOT CAUSE CLASSIFIED
ANALYTIC RAY/PLANE APERTURE = IMPLEMENTED / CURRENT PIE BEHAVIOR ACCEPTED
FULL-FIDELITY RECURSION 1..4 = PASS FOR RECURSIONDEPTH=2 USER PIE
LEGACY MATERIAL FEEDBACK AS RECURSION = NOT USED
OFFSCREEN PUBLICATION RETIREMENT = PASS (SEE InteriorPortalOffscreenPublicationRetirement.md)
```

## 1. Runtime evidence that ruled out the renderer pipeline

The reported grazing failure was not a missing request, a stopped TSR producer, or a missing depth texture. During the visible spiral failure the same frames continued to report:

```text
Subscribe / Execute / ComposeReady / DrawQueued
Projective=1
ProjectiveValid=1
DepthAware=1
DepthRequested=1
DepthTextureValid=1
ForegroundDepthRef Active=1 Valid=1
```

The failing endpoint was already a very narrow projection and reported approximately:

```text
ProjectiveQuality ~= 0.000982
ForegroundDepthRefQuality ~= 0.000943
```

This classified the problem as pixel ownership at grazing incidence, not renderer liveness.

## 2. Root cause: inverse screen homography is the wrong production primitive at grazing

The previous production composition asked:

```text
screen pixel
 -> multiply by inverse 3x3 portal homography H^-1
 -> divide homogeneous xy by z
 -> test ellipse
```

As a planar portal becomes edge-on, its 2D projection approaches a line. The forward planar homography therefore becomes singular by geometry, not by implementation accident. Its inverse becomes increasingly ill-conditioned before the exact singularity.

Changing determinant thresholds, depth epsilon, or cosmetic bias only moves the angle at which the failure appears. Those are not root fixes.

## 3. Root fix: analytic world ray / portal plane intersection

The production full-fidelity request now transports immutable world-space portal geometry:

```text
logical center
logical +X normal
logical +Y width axis
logical +Z height axis
1 / HalfWidth
1 / HalfHeight
SurfaceVisualBias
```

The production shader evaluates each main/parent-view pixel as:

```text
screen position
 -> View.ScreenToTranslatedWorld ray
 -> analytic ray / logical portal plane intersection
 -> project hit onto portal Y/Z basis
 -> ellipse test u^2 + v^2 <= 1
```

The cosmetic-surface depth reference uses the same ray and same normal with only `SurfaceVisualBias` added to the plane center.

Therefore:

```text
logical plane -> aperture + traversal + remote-depth ordering
biased cosmetic plane -> main foreground depth reference only
```

There is no inverse homography in the production pixel ownership decision. A truly parallel ray has no finite plane intersection, so the aperture naturally contracts to zero rather than alternating between the remote image and the fallback spiral.

The historical inverse homography remains only for old diagnostics/stencil compatibility while those paths are migrated separately.

During the later offscreen-lifetime investigation an implementation inconsistency was also removed: the producer had accidentally rebuilt `ForegroundDepthReference` with the legacy homography after `Build()` created the analytic representation. It now preserves the analytic center/basis/extents and updates only `Row2.W = SurfaceVisualBias`.

## 4. Why RecursionDepth=2 previously did nothing in FullFidelity

`AInteriorPortalSystem::RecursionDepth` belonged to the legacy SceneCapture renderer. That renderer explicitly:

1. builds repeated transformed views,
2. determines how many recursive entry views remain visible,
3. renders deepest -> shallowest,
4. binds the next render target to the portal surface while capturing the parent.

The promoted FullFidelity producer did none of those things. It always built:

```text
RecursionLevel = 0
```

and hid both portal actors from every secondary view. Thus `RecursionDepth=2` could not produce a nested portal. Physically crossing only made the formerly remote scene become the new main view, which explains the observed "it appears only after crossing" behavior.

This was an architectural gap, not a bad editor setting.

## 5. Full-fidelity recursion architecture

The FullFidelity producer now clamps and consumes:

```text
PortalSystem->RecursionDepth in [1,4]
```

For each Blue/Orange endpoint it builds a chain of visible requests. Each endpoint x recursion level owns:

```text
persistent TSR FSceneViewStateReference
camera-cut/history generation
R32F secondary depth target
exact FColorSample
color render target for that recursion level
```

Rendering order is:

```text
deepest visible level
 -> extract exact color/depth + PreExposure
 -> publish completed child request
parent secondary view BeforeDOF
 -> compose child with the same FInteriorPortalViewExtension compositor
 -> parent Tonemap/extraction
 -> publish completed parent request
...
level 0
 -> compose into player main BeforeDOF
```

This avoids previous-frame texture feedback and gives every layer an explicit exposure/history owner.

The exit portal is hidden from each secondary view because the transformed camera sits at that exit. The entry portal is no longer globally hidden: a deeper completed request composes over it when recursion continues; at the configured deepest level, the normal spiral surface is the explicit recursion-limit terminator.

## 6. Focused automated contract

Added:

```text
SlayTheSpireDemo.Interior.Portals.FullFidelity.AnalyticApertureGeometry
SlayTheSpireDemo.Interior.Portals.FullFidelity.RecursiveRenderRequest
```

These cover the CPU-side immutable geometry/recursion request contract. They do not replace PIE visual validation.

## 7. PIE acceptance

User PIE validation confirmed the important recursion behavior:

```text
RecursionDepth = 2
main view
 -> level 0 remote portal view
 -> nested level 1 portal visible
```

The nested portal is visible without requiring the player to physically cross first. This closes the original FullFidelity recursion gap for the tested depth-2 case.

A subsequent fast visible/offscreen regression exposed a separate publication-lifetime race. That issue was fixed with render-thread-ordered request retirement and is recorded as PASS in `InteriorPortalOffscreenPublicationRetirement.md`.

Current accepted behavior includes:

- FullFidelity recursion visible before crossing at depth 2,
- dual-visible endpoint rendering remains functional,
- rapid visible -> offscreen transitions no longer expose the fallback spiral,
- analytic aperture representation remains intact in the producer instead of being overwritten by the legacy homography.

## 8. Remaining validation boundary

The architecture supports recursion levels 1..4, but only the requested `RecursionDepth=2` path has explicit user PIE acceptance in this gate. Depth 3/4 remain supported-by-code rather than separately visually certified.

Keep focused regression coverage for:

```text
near-grazing / almost edge-on viewing
fast visible <-> offscreen transitions
RecursionDepth=2 nested portal visibility
Blue + Orange simultaneously visible
crossing and looking back
```
