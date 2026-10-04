# Interior Portal — STEP 1B.12A Main SceneDepth Continuity Validation

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
NEXT = STEP 1B.12B SECONDARY DEPTH TRANSPORT + MAIN-VIEW DEPTH REMAP PROOF
```

## Why this gate exists

The accepted portal path composes a secondary full-renderer HDR image into the
player's BeforeDOF SceneColor. Before this step the composition mask was purely
geometric: if a pixel was inside the projected aperture, portal RGB won. That
meant a real main-view object located between the player camera and the physical
entry portal could be overwritten by the portal image even though the main scene
depth said that object was closer.

STEP 1B.12A is the first bounded depth-continuity gate. It does **not** claim
that remote portal depth has been injected into the main SceneDepth buffer.
Instead it proves that the public BeforeDOF path can consume the real main
SceneDepth and use it to preserve correct foreground occlusion at the physical
portal plane.

## Exact portal-plane depth

STEP 1B.11A computes an inverse planar homography:

```text
H^-1 * [screenU, screenV, 1] = [u, v, 1] / clipW
```

The same three portal-plane basis points also define clip-space Z as:

```text
clipZ(u,v) = Du.z * u + Dv.z * v + Center.z
```

Therefore main-view device depth of the physical portal plane can be evaluated
per pixel without reconstructing world position:

```text
portalDeviceZ =
    Du.z * (u / clipW)
  + Dv.z * (v / clipW)
  + Center.z * (1 / clipW)
```

`InteriorPortalProjectiveAperture::FScreenToPortalMapping` carries this
`ClipZRow` next to the inverse homography rows.

## Main SceneDepth contract

The implementation intentionally avoids reflecting the whole deferred
SceneTextures uniform buffer into the custom screen shader. That path produced
D3D12 uniform-buffer slot failures in the SceneViewExtension callback.

Instead, when the depth gate is requested, project code creates the current main
view's SceneDepth-only scene-texture parameters, extracts the RDG SceneDepth
texture, and binds that texture directly as `MainSceneDepthTexture`. The shader
also declares and binds the current `FSceneView` View uniform buffer because
`ConvertFromDeviceZ()` depends on it.

For aperture pixels the shader reads the real main SceneDepth, converts both the
main sample and physical portal-plane device depth to world-space depth, and
applies:

```text
mainDepthCm + epsilon < portalPlaneDepthCm
    -> real main-scene foreground wins
otherwise
    -> portal composition is allowed
```

The default comparison tolerance is:

```text
portal.DepthOcclusionEpsilonCm 2.0
```

This tolerance is only for coplanar host-wall / portal-rim depth equality. It is
not a visual depth offset and must not be tuned to hide unrelated renderer bugs.

## Runtime controls

```text
portal.DepthAwareComposition 0   # retained RGB-only comparison path
portal.DepthAwareComposition 1   # STEP 1B.12A main-depth gate
portal.ProjectiveAperture 1      # required by exact portal-plane depth mapping
portal.CompositionDebugMode 4    # green=open, red=main foreground occluder
```

`portal.CompositionDiagnostics 1` records:

```text
DepthAware=...
DepthRequested=...
DepthTextureValid=...
DepthEpsilonCm=...
```

A normal depth-enabled frame reports:

```text
Projective=1
ProjectiveValid=1
DepthRequested=1
DepthTextureValid=1
DepthAware=1
```

## Runtime evidence — 2026-09-15

The shader bring-up exposed two binding failures and both were resolved:

```text
1. SceneTextures uniform-buffer slot was not guaranteed in this callback.
2. ConvertFromDeviceZ() reflected the View uniform buffer, but the shader
   parameter struct initially did not declare/bind View.
```

After binding SceneDepth as a direct RDG texture and explicitly binding
`InView.ViewUniformBuffer`, repeated main-view BeforeDOF frames reported:

```text
DebugMode=4
Projective=1
ProjectiveValid=1
DepthAware=1
DepthRequested=1
DepthTextureValid=1
DepthEpsilonCm=2.0000
```

Measured frames 2767-2773 used a 1557x733 main SceneColor, a 1920x904 portal
target, valid projective quality around 0.055-0.058, and a main PreExposure
around 0.00149-0.00152. A later stationary sequence at frames 2900-2913
repeatedly preserved the same contract without D3D12 uniform-buffer failures.

Visual validation then used a real opaque world-space object placed between the
player camera and the physical portal plane. In composition debug mode 4:

```text
open aperture region                 -> green
foreground world-space object overlap -> red
first-person foreground geometry      -> red where its main depth wins
```

This demonstrates that the main SceneDepth comparison follows real foreground
silhouettes instead of blindly replacing the aperture with portal RGB.

During camera motion with the main view using TSR (`r.AntiAliasingMethod=4`),
the red/green debug visualization could briefly appear almost fully red. This was
isolated by temporarily setting:

```text
r.AntiAliasingMethod 0
```

With main-view AA/TSR disabled, the transient full-red event disappeared. The
secondary portal TSR producer remained active. Therefore the transient red frame
was a temporal-history artifact of feeding synthetic red/green diagnostic color
into the main view's later TSR reconstruction, not evidence that the raw
SceneDepth classifier had classified the whole aperture as foreground.

This debug-only artifact is not a failure of the normal depth-aware composition
contract. Restore the project's normal main-view AA mode after the diagnostic
is complete.

## PASS result

STEP 1B.12A is accepted because:

1. The shader runs without the earlier View/SceneTextures uniform-buffer
   failures.
2. `Projective=1`, `ProjectiveValid=1`, `DepthRequested=1`,
   `DepthTextureValid=1`, and `DepthAware=1` are observed continuously.
3. Debug mode 4 shows the unobstructed aperture as depth-visible and real
   foreground world geometry as foreground-occluded with the expected
   silhouette.
4. The transient full-red motion artifact disappears when main-view TSR is
   disabled, isolating it to diagnostic-color temporal reconstruction rather
   than the underlying main-depth comparison.
5. No NaN/Inf, RDG validation failure, renderer assertion, or stale portal
   target was observed in the accepted depth-enabled path.

## What this proves

Project-side public renderer APIs are sufficient for this bounded contract:

```text
main SceneDepth read
    + exact physical entry-plane device depth
    + projective aperture
    -> preserve real main-view foreground occluders
```

This removes one major source of the portal looking like an RGB card pasted over
the real scene.

## What this does NOT prove

STEP 1B.12A does not write portal/remote depth into the main SceneDepth texture.
Therefore downstream passes still cannot automatically treat the portal interior
as ordinary main-view geometry for:

```text
Depth of Field
Depth Fog / depth-based post effects
SSR / other main-view depth consumers
HZB / occlusion structures
true main depth-stencil aperture ownership
remote-object depth continuity
```

The next gate is:

```text
STEP 1B.12B — SECONDARY DEPTH TRANSPORT + MAIN-VIEW DEPTH REMAP PROOF
```

That stage should extract secondary depth belonging to the same full secondary
renderer sample, remap it through the portal transform into the main-view depth
domain, and establish whether project-side RDG can safely propagate that depth
far enough for downstream effects. If a true main depth-stencil write cannot be
made safely through public APIs, that result becomes the explicit renderer-
private escalation point rather than being hidden by another RGB workaround.

## Claim boundary

Still out of scope:

```text
remote secondary depth injection into main SceneDepth
main stencil aperture write
portal-depth-aware DOF/fog/SSR/HZB
recursion >= 2
multiple simultaneous visible portal histories
production GPU/VRAM acceptance
Core Portal Fidelity Seal
```
