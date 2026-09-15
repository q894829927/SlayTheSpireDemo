# Interior Portal — STEP 1B.8 Pre-Exposure Rebase Validation

Date: **2026-09-15**

State:

```text
STEP 1B.5 FULL TRANSFORMED VIEW-FAMILY = PASS
STEP 1B.6 BEFORE-DOF HDR EXTRACTION = PASS
STEP 1B.7 STATIC MAIN-VIEW COMPOSITION = PASS
STEP 1B.8 SECONDARY->MAIN PRE-EXPOSURE REBASE = PASS
STEP 1B.9 PER-FRAME FULL SECONDARY PRODUCER = NEXT
```

## Why this step exists

STEP 1B.7 proved the complete static composition chain, but the portal aperture
was strongly over-exposed while the ordinary player scene remained normally
exposed. The renderer, transformed camera, HDR target, aperture sampling and
main-view BeforeDOF callback were all active, so arbitrary brightness/gamma
correction is not allowed.

UE pre-exposure scales SceneColor before later post processing. A secondary HDR
SceneColor must therefore be converted into the main view's current pre-exposed
domain before it is blended into main SceneColor:

```text
PortalRGB_mainDomain = PortalRGB_secondary
                     * MainPreExposure / SecondaryPreExposure
```

This is a renderer-domain conversion, not an artistic brightness control.

## Implementation

The one-shot full-view composition spike measures the exact secondary view's
PreExposure from the `FSceneView` that supplies the BeforeDOF SceneColor:

```cpp
View.State->GetPreExposure()
```

The measured value is transferred to the renderer-side composition diagnostic
through:

```text
portal.SecondaryPreExposure
portal.PreExposureRebase
```

These CVars are diagnostic transport only. The run command owns them and the
clear command restores rebasing to disabled.

At each ordinary player-view BeforeDOF composition callback, the compositor reads
that frame's main view PreExposure from `InView.State->GetPreExposure()`, computes
`Main / Secondary`, and passes the ratio to the composition shader as
`PortalExposureScale`.

Only normal composition (`portal.CompositionDebugMode 0`) applies the scale.
Modes 1/2/3 remain unscaled so the existing magenta and BaseColor diagnostics do
not change meaning.

The secondary SceneColor alpha remains ignored; main SceneColor alpha is still
preserved.

## Runtime result — PASS

The validation run reports:

```text
status = PREEXPOSURE_COMPOSITION_ARMED
targetSize = 1752 x 880
beforeDOFCallbackExecuted = true
mainCompositionArmed = true
SecondaryPreExposure = 1.0
```

Representative main-view composition frames report:

```text
Frame 1638
MainPreExposure      = 0.00154326879
SecondaryPreExposure = 1.0
ExposureScale         = 0.00154326879

Frame 1639
MainPreExposure      = 0.00154329173
SecondaryPreExposure = 1.0
ExposureScale         = 0.00154329173

Frame 1640
MainPreExposure      = 0.00154323562
SecondaryPreExposure = 1.0
ExposureScale         = 0.00154323562
```

Therefore the runtime relation is numerically exact for the tested frames:

```text
ExposureScale = MainPreExposure / SecondaryPreExposure
```

The severe STEP 1B.7 white blow-out is removed. The aperture now remains in the
same pre-exposure domain as the player/main SceneColor and continues through the
main player's normal post-processing chain.

The portal interior is substantially darker than the bright player-side room in
this validation image. That is not, by itself, evidence that the rebase is wrong:
this test intentionally keeps the **main player view as the only exposure / tone-map
authority**. With a main PreExposure near `0.001543` and a secondary PreExposure of
`1.0`, a large reduction is the expected domain conversion. Do not compensate it
with a hand-tuned brightness, gamma, or portal-local auto-exposure multiplier.

If later visual acceptance requires proving that the remaining darkness is a
lighting-domain problem rather than the intended shared-exposure result, compare
the same target-space surfaces rendered by the ordinary main camera under the
same player exposure. That comparison is separate from this pre-exposure gate.

## Acceptance result

PASS criteria:

1. full-renderer target-space content remains visible inside the aperture — **PASS**;
2. SecondaryPreExposure is finite and positive — **PASS (1.0)**;
3. MainPreExposure is finite and positive — **PASS (~0.001543)**;
4. ExposureScale equals Main/Secondary within float error — **PASS**;
5. the STEP 1B.7 severe over-exposure is substantially reduced without an
   arbitrary brightness/gamma/tone-map multiplier — **PASS**.

Accordingly STEP 1B.8 is closed as a renderer-domain feasibility pass.

## Architectural consequence — STEP 1B.9

The next task is no longer exposure correction. The remaining major limitation of
the proven path is that the secondary full-renderer image and projected bounds are
one-shot/static.

STEP 1B.9 should promote the proven producer to a per-frame one-layer path while
preserving all already verified contracts:

```text
player camera each frame
    -> immutable transformed portal request
    -> full secondary FSceneViewFamily
    -> secondary BeforeDOF HDR extraction
    -> measured SecondaryPreExposure
    -> persistent portal RGBA16F target
    -> main BeforeDOF composition
    -> MainPreExposure / SecondaryPreExposure rebase
    -> player-owned final exposure + tone map
```

The first per-frame implementation should remain deliberately narrow:

```text
one visible portal
recursion depth = 0
no TAA/TSR history promotion yet
no stencil/depth continuity yet
no portal-bounded renderer scissor yet
no recursion >= 2
```

The main acceptance criterion for STEP 1B.9 is spatial tracking: moving or rotating
the player camera must update both the transformed secondary image and aperture
bounds every frame without blocking readback/export work and without reintroducing
SceneCapture as the portal image producer.

## Validation command retained for the static gate

```text
RendererBackend = SceneCapture
portal.CompositionDiagnostics 1
portal.CompositionDebugMode 0
portal.RunFullViewFamilyMainCompositionSpike
```

When finished:

```text
portal.ClearFullViewFamilyMainCompositionSpike
```

This disables the dedicated compositor and resets the pre-exposure rebase CVars.

## Claim boundary

Still out of scope:

```text
persistent secondary temporal history
TAA/TSR jitter and motion vectors
main depth/stencil continuity
portal-bounded renderer scissor
recursion >= 2
full Lumen/translucency/fog acceptance
production GPU cost
Core Portal Fidelity Seal
```
