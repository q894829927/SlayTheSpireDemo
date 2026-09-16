# Interior Portal — Grazing Foreground-Depth Regression

Date: **2026-09-16**

State:

```text
MV-C FULL-FIDELITY AUTO LIFECYCLE = USER-ACCEPTED
MV-C LEGACY SCENECAPTURE EXCLUSION = USER-ACCEPTED
MV-C PREVIOUS 161 MB VRAM OVER-BUDGET WARNING = NOT REPRODUCED AFTER EXCLUSION
FULL-FIDELITY MAIN-FOREGROUND DEPTH POLICY = PROMOTED
GRAZING SPIRAL FLASH REGRESSION = FIX IMPLEMENTED / USER BUILD + PIE REQUIRED
```

## Reproduced symptom

After promoting `portal.DepthAwareComposition=1` into the normal FullFidelity lifecycle,
the user observed a new failure at sufficiently oblique entry angles:

```text
remote portal image visible
    -> camera reaches a grazing threshold
    -> aperture repeatedly flashes back to the default spiral surface
    -> small camera-angle changes cause continuous remote/spiral alternation
```

This is not classified as a multi-visible endpoint publication failure. The same endpoint
remains visible, and the earlier projective-aperture grazing gate had already reached an
almost edge-on view without projective invalidation.

## Root cause

The portal actor intentionally separates two planes:

```text
logical entry plane
    actor transform
    authoritative for aperture, crossing, traversal and remote-depth ordering

cosmetic visible surface plane
    logical plane + SurfaceVisualBias * portal +X
    default SurfaceVisualBias = 0.6 cm
    avoids host-surface Z fighting
```

Before the production depth policy was enabled, the main compositor did not consume
main SceneDepth, so this separation could not affect portal color ownership.

The promoted depth-aware shader compared main SceneDepth against the **logical** portal
plane. The visible spiral Surface itself writes main-scene depth from the biased cosmetic
plane. At a frontal view the resulting line-of-sight depth separation is within the normal
2 cm coplanar tolerance. At grazing incidence, a small normal-space offset corresponds to
a much larger line-of-sight depth separation. The portal Surface can therefore satisfy:

```text
mainDepth + epsilon < logicalPortalPlaneDepth
```

and be incorrectly classified as a real foreground occluder. When this happens the
compositor preserves `MainColor`; `MainColor` contains the default spiral Surface, so the
failure presents exactly as a remote/spiral flash.

Increasing `portal.DepthOcclusionEpsilonCm` is rejected as the production fix because it
would trade this bug for incorrect foreground-object occlusion and would remain angle
dependent.

## Implemented fix

The render request now carries two independent projective mappings:

```text
ProjectiveAperture
    -> logical portal plane
    -> aperture ownership
    -> traversal semantics
    -> secondary/remote depth ordering

ForegroundDepthReference
    -> actual cosmetic entry Surface plane
    -> logical frame translated by SurfaceVisualBias along portal +X
    -> main SceneDepth foreground comparison only
```

The endpoint-owned FullFidelity producer builds `ForegroundDepthReference` from the
actual `AInteriorPortal::SurfaceVisualBias` for that endpoint. No hard-coded 0.6 cm
renderer constant is introduced.

Both the normal composition shader and the optional main-depth propagation candidate use
the cosmetic surface plane when deciding whether **main-view geometry** is in front of the
portal. Secondary depth is still checked against the logical portal plane. This preserves
the existing crossing/depth contract instead of moving the real portal plane to hide a
rendering defect.

If the cosmetic mapping is invalid, the renderer falls back to the logical mapping rather
than accepting an unbounded or NaN depth reference.

`portal.CompositionDiagnostics 1` now emits:

```text
PortalComposition ForegroundDepthRef ... Active=1 Valid=1 Quality=...
```

for the endpoint-owned production path.

## Code changes

```text
Source/SlayTheSpireDemo/Interior/InteriorPortalRenderer.h
    FInteriorPortalRenderRequest::ForegroundDepthReference

Source/SlayTheSpireDemo/Interior/InteriorPortalMultiVisibleTSRSpike.cpp
    builds the reference from Entry->SurfaceVisualBias

Source/SlayTheSpireDemo/Interior/InteriorPortalRenderer.cpp
    binds the independent reference to composition/depth shaders

Shaders/InteriorPortalComposition.usf
    main foreground comparison uses cosmetic-surface depth
    logical depth remains remote-depth authority

Shaders/InteriorPortalDepthPropagation.usf
    same split for the downstream depth candidate
```

## USER ACTION REQUIRED

This branch has no GitHub CI capable of building UE 5.8, so the fix is not accepted until
a local build and focused PIE gate complete.

Build the current branch with the normal UE 5.8 editor build. Then start PIE normally.
Do **not** issue a manual renderer start command; lifecycle ownership remains part of the
gate.

Run this short visual sequence:

```text
1. Front view: overlap the first-person flashlight with the aperture.
   Expected: flashlight remains in front while physically before the portal Surface.

2. Rotate/strafe to the exact oblique angle that previously triggered spiral flashing.
   Hold for at least 5 seconds.
   Expected: remote image remains continuous; no default-spiral flashes.

3. Slowly oscillate the camera across the previous threshold angle for 5-10 seconds.
   Expected: no remote/spiral alternation and no one-frame fallback.

4. Put both Blue and Orange in view once.
   Expected: both endpoints remain published and lit.

5. Confirm the video-memory over-budget warning remains absent.
```

For one diagnostic pass only:

```text
portal.CompositionDiagnostics 1
```

At the failing/grazing pose, capture one `PortalComposition ForegroundDepthRef` line and
one nearby `ComposeReady` line. Expected production values include:

```text
Projective=1
ProjectiveValid=1
DepthAware=1
DepthRequested=1
DepthTextureValid=1
ForegroundDepthRef Active=1 Valid=1
```

Then disable the verbose diagnostic:

```text
portal.CompositionDiagnostics 0
```

Focused Automation after the build:

```text
SlayTheSpireDemo.Interior.Portals.PlacementAndLifecycle
SlayTheSpireDemo.Interior.Portals.ProjectedScreenBounds
```

The first guards the logical-frame / `SurfaceVisualBias` separation; the second guards the
projective visibility/bounds path that previously passed grazing validation.

## Acceptance

Accept this regression only when all of the following hold:

```text
first-person foreground preservation                  PASS
previous grazing-angle spiral flash                  NOT REPRODUCED
slow threshold-angle oscillation                     STABLE
ForegroundDepthRef Active/Valid                      1 / 1
single-visible remote image                          STABLE
dual-visible Blue + Orange                           STABLE
new VRAM over-budget warning                         ABSENT
```

Do not close this gate from compilation alone; the defect is angle-dependent and requires
PIE visual evidence.
