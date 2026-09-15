# Interior Portal — STEP 1B.8 Pre-Exposure Rebase Validation

Date: **2026-09-15**

State:

```text
STEP 1B.5 FULL TRANSFORMED VIEW-FAMILY = PASS
STEP 1B.6 BEFORE-DOF HDR EXTRACTION = PASS
STEP 1B.7 STATIC MAIN-VIEW COMPOSITION = PASS
STEP 1B.8 SECONDARY->MAIN PRE-EXPOSURE REBASE = IMPLEMENTED / NOT YET BUILT OR RUN
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

The one-shot full-view composition spike now measures the exact secondary view's
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

## Validation procedure

Use the same controlled setup as STEP 1B.7:

```text
RendererBackend = SceneCapture
portal.CompositionDiagnostics 1
portal.CompositionDebugMode 0
portal.RunFullViewFamilyMainCompositionSpike
```

Keep the camera still after running because this remains a one-shot static
secondary image and frozen projected-bounds proof.

Expected JSON:

```text
Saved/AutomationReports/PortalFullViewFamilyMainCompositionSpike.json

status = PREEXPOSURE_COMPOSITION_ARMED
beforeDOFCallbackExecuted = true
mainCompositionArmed = true
```

The JSON detail should contain the measured `SecondaryPreExposure`.

With composition diagnostics enabled, each main-frame `ComposeReady` log should
now also contain:

```text
Rebase=1
MainPreExposure=<value>
SecondaryPreExposure=<value>
ExposureScale=<Main/Secondary>
```

## Acceptance

PASS requires:

1. the full-renderer target-space content remains visible inside the aperture;
2. `SecondaryPreExposure` is finite and positive;
3. `MainPreExposure` is finite and positive;
4. `ExposureScale` numerically equals Main/Secondary within normal float error;
5. the severe STEP 1B.7 brightness mismatch is substantially reduced without an
   arbitrary brightness, gamma or tone-map multiplier.

Perfect visual parity is not yet required. If the measured ratio is correct but a
large mismatch remains, the next investigation must identify another renderer
domain difference rather than tune the ratio by eye.

When finished:

```text
portal.ClearFullViewFamilyMainCompositionSpike
```

This disables the dedicated compositor and resets the pre-exposure rebase CVars.

## Claim boundary

Still out of scope:

```text
per-frame full secondary producer
persistent secondary temporal history
TAA/TSR jitter and motion vectors
main depth/stencil continuity
portal-bounded renderer scissor
recursion >= 2
full Lumen/translucency/fog acceptance
production GPU cost
Core Portal Fidelity Seal
```
