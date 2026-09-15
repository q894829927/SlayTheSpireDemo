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
STEP 1B.12A MAIN SCENEDEPTH FOREGROUND OCCLUSION = IMPLEMENTED / NOT YET BUILT OR RUN
```

## Why this gate exists

The accepted portal path currently composes a secondary full-renderer HDR image
into the player's BeforeDOF SceneColor. Until this step the composition mask was
purely geometric: if a pixel was inside the projected aperture, portal RGB won.
That means a real main-view object located between the player camera and the
physical entry portal could be overwritten by the portal image even though the
main scene depth says that object is closer.

STEP 1B.12A is the first bounded depth-continuity gate. It does **not** claim
that remote portal depth has been injected into the main SceneDepth buffer.
Instead it proves that the public BeforeDOF path can consume the real main
SceneDepth and use it to preserve correct foreground occlusion at the physical
portal plane.

## Exact portal-plane depth

STEP 1B.11A already computes an inverse planar homography:

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

`InteriorPortalProjectiveAperture::FScreenToPortalMapping` now carries this
`ClipZRow` next to the inverse homography rows.

## Main SceneDepth contract

The BeforeDOF global shader now binds UE's public
`FSceneTextureShaderParameters` with `ESceneTextureSetupMode::SceneDepth`.
For aperture pixels it reads the real main SceneDepth, converts both the main
sample and physical portal-plane device depth to world-space depth, and applies:

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

The new switch is deliberately off by default so the already accepted RGB-only
path remains available for A/B testing:

```text
portal.DepthAwareComposition 0   # retained RGB-only path
portal.DepthAwareComposition 1   # STEP 1B.12A main-depth gate
```

`portal.ProjectiveAperture 1` is required because exact portal-plane device
Z comes from the accepted projective mapping.

Composition debug mode 4 visualizes the gate:

```text
portal.CompositionDebugMode 4
```

Inside the physical aperture:

```text
green = main depth is at/behind the portal plane; portal may be visible
red   = real main-scene geometry is in front of the portal plane; main wins
```

Outside the aperture the ordinary main SceneColor remains visible.

`portal.CompositionDiagnostics 1` now records:

```text
DepthAware=...
DepthRequested=...
DepthEpsilonCm=...
```

A normal depth-enabled frame should show:

```text
Projective=1
ProjectiveValid=1
DepthRequested=1
DepthAware=1
```

## Validation procedure

Use the accepted TSR producer:

```text
RendererBackend = SceneCapture
portal.ProjectiveAperture 1
portal.DepthOcclusionEpsilonCm 2.0
portal.CompositionDiagnostics 1
portal.FullViewFamilyTSRDiagnostics 1
portal.FullViewFamilyTSRPrimaryFraction 0.67
portal.StartFullViewFamilyTSRSpike
```

First verify the diagnostic mask:

```text
portal.DepthAwareComposition 1
portal.CompositionDebugMode 4
```

Put a **world-space opaque object** between the player camera and part of the
portal aperture. Prefer a movable cube/character/world prop. Do not use only a
HUD element; a first-person weapon can also be rendered in a special foreground
path and is not the strongest proof of ordinary SceneDepth behavior.

Expected mode-4 result:

1. Open aperture pixels are green.
2. The portion covered by the real foreground object is red.
3. Moving the object across the aperture moves the red region with its real
   silhouette/depth.
4. The portal host wall/rim does not turn the entire aperture red merely because
   it is coplanar or nearly coplanar.

Then return to normal color:

```text
portal.CompositionDebugMode 0
```

Do a fixed-camera A/B while the world object overlaps the aperture:

```text
portal.DepthAwareComposition 0
# old behavior: portal RGB can overwrite the foreground object inside aperture

portal.DepthAwareComposition 1
# expected: foreground object remains visible where it is physically in front
```

Keep the player/camera and object stationary while toggling the CVar so this is
a true depth-gate A/B rather than a different camera sample.

Finally stop the producer:

```text
portal.StopFullViewFamilyTSRSpike
```

## PASS criteria

PASS requires all of the following:

1. Project compiles and the shader compiles without SceneTextures uniform-buffer
   binding errors.
2. `ComposeReady` reports `Projective=1`, `ProjectiveValid=1`,
   `DepthRequested=1`, `DepthAware=1`.
3. Debug mode 4 shows a stable green aperture where unobstructed.
4. A real opaque world object between camera and portal produces a red region
   matching the object's actual foreground overlap.
5. In normal mode with `DepthAwareComposition=1`, that foreground object is no
   longer overwritten by portal RGB.
6. `DepthAwareComposition=0` reproduces the previous accepted RGB-only path.
7. No NaN/Inf, renderer assertion, RDG validation failure or stale secondary
   target appears while moving camera/object across the aperture.

## What this proves

If PASS, project-side public renderer APIs are sufficient for this bounded
contract:

```text
main SceneDepth read
    + exact physical entry-plane device depth
    + projective aperture
    -> correct preservation of main-view foreground occluders
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

The public post-process scene-texture contract exposes readable main SceneDepth
(and CustomDepth/CustomStencil when requested), but this is not the same as a
safe public binding for mutating the renderer's main depth-stencil attachment.

The next gate after 1B.12A is therefore deliberately narrower than "finish all
stencil" in one jump:

```text
STEP 1B.12B — SECONDARY DEPTH TRANSPORT + MAIN-VIEW DEPTH REMAP PROOF
```

That stage should extract the secondary view depth owned by the same full
secondary renderer sample, remap it through the portal transform into main-view
depth, and establish whether project-side RDG can safely propagate that depth
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
