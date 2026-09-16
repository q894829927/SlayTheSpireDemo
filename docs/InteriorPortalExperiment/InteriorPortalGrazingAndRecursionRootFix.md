# Interior Portal — Grazing Ownership + Full-Fidelity Recursion Root Fix

Date: **2026-09-16**

State:

```text
GRAZING SPIRAL REGRESSION = ROOT CAUSE CLASSIFIED
ANALYTIC RAY/PLANE APERTURE = IMPLEMENTED / USER BUILD + PIE REQUIRED
FULL-FIDELITY RECURSION 1..4 = IMPLEMENTED / USER BUILD + PIE REQUIRED
LEGACY MATERIAL FEEDBACK AS RECURSION = NOT USED
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

## 7. USER ACTION REQUIRED

Build the current branch first. No GitHub CI currently proves this C++/shader change.

Then run PIE with normal automatic FullFidelity startup; do not manually start a second renderer.

### Grazing gate

Reproduce the exact former failure:

```text
normal view
 -> increasingly oblique
 -> almost edge-on
 -> hold near the old spiral-flash angle for 10 seconds
 -> oscillate slowly across that angle
```

PASS requires no alternating remote-image / default-spiral frames.

### RecursionDepth=2 gate

Set:

```text
RecursionDepth = 2
```

Arrange the pair so the entry portal is visible inside its level-0 remote view. PASS requires:

```text
main -> level 0 portal view -> nested level 1 portal view
```

without physically crossing first.

Dump:

```text
portal.DumpFullFidelityRenderer
```

Expected report includes:

```text
RecursionDepth=2
E0Depth=2 and/or E1Depth=2 when that endpoint's nested portal is geometrically visible
PortalMultiVisible Recursive Endpoint=<n> Level=1 Submitted>0 ExtractFrame>0 Pre>0 Completed>0
```

A requested depth of 2 does not force a second render if the recursive entry aperture is genuinely outside the parent secondary view; `VisibleDepth` is visibility-gated just like the legacy recursion path.

## 8. Failure routing

```text
grazing still exposes spiral
 -> inspect analytic ray-plane shader / physical surface coverage; do not tune determinant or epsilon

RecursionDepth=2 but E*Depth stays 1
 -> request-chain visibility/build problem

E*Depth=2 and Level=1 completes, but nested portal absent
 -> child BeforeDOF targeting/scheduling problem

nested portal visible but wrong brightness
 -> child FColorSample -> parent PreExposure rebase problem

nested portal temporal ghost/cut
 -> per-level ViewState/history reset problem
```
