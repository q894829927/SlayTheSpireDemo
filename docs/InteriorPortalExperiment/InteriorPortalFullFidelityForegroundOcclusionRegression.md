# Interior Portal — Full-Fidelity Foreground Occlusion Regression

Date: **2026-09-16**

State:

```text
MV-C EXCLUSIVE LIFECYCLE AUTO-START = USER PIE PASS
MV-C DUAL-VISIBLE COVERAGE = USER PIE PASS
MV-C LEGACY SCENECAPTURE MUTUAL EXCLUSION = USER PIE PASS
MV-C PREVIOUS VRAM-OVER-BUDGET WARNING = NOT REPRODUCED
FOREGROUND OCCLUSION REGRESSION = FIX IMPLEMENTED / USER PIE REQUIRED
```

## Latest production-promotion evidence

The user rebuilt the lifecycle-integrated branch and launched PIE without entering a renderer start command.

The runtime automatically emitted:

```text
portal.StartFullFidelityRenderer
portal.StartMultiVisibleTSRSpike
PortalMultiVisible: START...
PortalFullFidelityRenderer: START requested through endpoint-owned multi-visible TSR path.
PortalFullFidelityRenderer: lifecycle START from InteriorPlayerController; legacy RenderViews bypassed.
```

A later full-fidelity dump while both endpoints were visible reported:

```text
VisibleCount=2
VisibleMask=0x03
SubmittedMask=0x03
PublishedMask=0x03
E0 Submitted=1021 ExtractFrame=1881 Continuous=1018 PreExposure=0.00145010708 Completed=1021
E1 Submitted=973 ...
```

The user also reported that the previous `Video memory has been exhausted` warning no longer appeared after the lifecycle integration made the full-fidelity renderer mutually exclusive with legacy SceneCapture.

Classification:

```text
MV-C lifecycle ownership = PASS
original dual-visible one-black-portal defect = CLOSED
161.059 MB duplicate-renderer memory warning = no longer reproduced in exclusive path
```

This means MV-D is no longer justified as an emergency correctness/performance blocker solely from the old 161.059 MB measurement. Further memory profiling remains useful, but should use the exclusive full-fidelity renderer as the baseline.

## Newly observed regression

The user then reproduced a first-person foreground problem:

```text
flashlight / first-person weapon overlaps the portal aperture in screen space
portal remote RGB overwrites the foreground flashlight silhouette
```

This is **not** the intended portal slicing behavior.

Intended behavior is:

```text
first-person geometry physically in front of the entry plane
    -> remains visible in front of portal composition

geometry that actually intersects / crosses the portal plane
    -> source half may be sliced and the mapped remote half may be presented at the exit
```

A portal aperture must not behave like a flat overlay card that simply covers nearer main-view geometry.

## Root cause

This exact contract was already implemented and accepted by STEP 1B.12A.

`FInteriorPortalViewExtension` supports main SceneDepth foreground rejection through:

```text
portal.ProjectiveAperture
portal.DepthAwareComposition
```

The shader compares real main-view SceneDepth against the exact physical entry-plane depth. When the main-view sample is closer than the portal plane, the original main SceneColor wins.

The accepted STEP 1B.12A visual evidence explicitly included first-person foreground geometry.

However, the renderer CVars still have validation-oriented defaults:

```text
portal.ProjectiveAperture      = 1
portal.DepthAwareComposition  = 0
```

MV-C production promotion started the endpoint-owned renderer but did not promote the accepted depth-aware composition policy. The result was the earlier RGB-only behavior: any pixel inside the aperture could overwrite the flashlight even when main SceneDepth placed it in front of the portal plane.

## Fix

Commit:

```text
fd2e43cee99716342e5cbea9b483a534a49bfb2a
portal: preserve first-person foreground in full-fidelity composition
```

`InteriorPortalFullFidelityRendererControl.cpp` now treats the accepted foreground-depth behavior as part of the full-fidelity production contract.

On full-fidelity start it applies:

```text
portal.ProjectiveAperture = 1
portal.DepthAwareComposition = 1
```

The previous values are saved once per full-fidelity lifecycle and restored when the full-fidelity renderer stops.

This is intentionally narrow. The fix does **not** alter:

```text
secondary EyeAdaptation
exact FColorSample ownership
TSR histories
multi-visible endpoint ownership
portal traversal
player/held-object slicing
brightness/gamma
main-depth propagation to downstream effects
```

It only restores the already accepted STEP 1B.12A rule that real main-view foreground depth wins over portal RGB before the physical entry plane.

## USER ACTION REQUIRED

Rebuild the latest branch and launch PIE normally. Do not enter any renderer start command.

Expected startup log includes:

```text
PortalFullFidelityRenderer: composition policy ProjectiveAperture=1 DepthAwareComposition=1; real main-view foreground depth wins in front of the entry plane.
PortalMultiVisible: START...
PortalFullFidelityRenderer: lifecycle START from InteriorPlayerController; legacy RenderViews bypassed.
```

Validation sequence:

```text
1. Stand far enough from the portal that the flashlight is clearly in front of the entry plane.
2. Aim so the flashlight overlaps the aperture in screen space.
3. Move closer slowly while maintaining overlap.
4. Move laterally / use a strong slant.
5. If possible, move close enough that part of the first-person geometry actually crosses the physical portal plane.
6. Verify both Blue and Orange still render simultaneously.
```

Acceptance:

```text
foreground flashlight silhouette remains visible over the portal while it is physically in front
no aperture-shaped clipping merely from screen-space overlap
actual portal-plane intersection may still use the designed source/remote slicing behavior
both endpoints remain rendered
no black portal regression
no renewed VRAM-over-budget warning
```

If the foreground is still overwritten after this fix, enable only:

```text
portal.CompositionDiagnostics 1
```

and inspect for:

```text
Projective=1
DepthRequested=1
DepthTextureValid=1
DepthAware=1
```

If those are all `1` but the flashlight still loses, the next question is whether that specific first-person mesh writes usable main SceneDepth. Do not compensate with aperture bias, gamma, brightness or a larger depth epsilon.
