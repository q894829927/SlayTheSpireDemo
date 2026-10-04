# Interior Portal — STEP 1B.14B/1B.14C View/Exposure Parity Diagnostics

Date: **2026-09-16**

State:

```text
STEP 1B.14B MAIN-VS-SECONDARY VIEW DIAGNOSTICS
= PASS / ROOT-CAUSE CLASSIFIED

STEP 1B.14C SECONDARY EYE ADAPTATION A/B
= PASS / EYE ADAPTATION REQUIRED FOR VISUAL PARITY
```

## Why these gates exist

STEP 1B.14A proved the remaining single-layer visual mismatch is upstream of portal-only aperture/stencil/depth/scissor composition: the transformed secondary full-screen reference itself was too dark.

The next gates therefore compared the real main-view exposure state against the manually constructed additional `FSceneViewFamily`, then A/B tested the secondary view's EyeAdaptation policy directly.

## 1B.14B runtime evidence

The real main-view Tonemap callback stabilized approximately at:

```text
Main PreExposure ~= 0.00173 .. 0.00176
AA = TSR (4)
AutoExposureMethod = 0
ExposureBias = 0
DynamicGI = Lumen
Reflection = Lumen
IndirectLightingIntensity = 1
```

The secondary producer policy at that point was:

```text
EyeAdaptation = false
Secondary PreExposure = 1
MotionBlur = false
DepthOfField = false
TemporalAA = true
ScreenPercentage = true
Dynamic GI = forced Lumen
Reflection = forced Lumen
```

Two preliminary A/B tests were rejected as primary fixes:

```text
r.EyeAdaptation.CachedLightingPreExposure 4 -> 8
```

produced almost no meaningful visual improvement, and

```text
r.EyeAdaptation.PreExposureOverride 0 -> 1
```

only darkened the main scene globally; it did not repair the secondary renderer.

Therefore the mismatch was not accepted as a simple cached-lighting range problem or a simple final RGB pre-exposure conversion error.

## 1B.14C implementation

Source:

```text
Source/SlayTheSpireDemo/Interior/InteriorPortalSecondaryEyeAdaptationSpike.cpp
```

Controls:

```text
portal.SecondaryEyeAdaptation 0/1
portal.SecondaryEyeAdaptationDiagnostics 1
portal.StartSecondaryEyeAdaptationSpike
portal.DumpSecondaryEyeAdaptationSpike
portal.StopSecondaryEyeAdaptationSpike
```

The A/B changes only the secondary additional-family EyeAdaptation policy while preserving the transformed full `FSceneViewFamily`, persistent secondary `FSceneViewState`, TSR, Lumen GI/reflection, clip plane, post-TSR/pre-tonemap HDR extraction and pre-exposure rebase.

## 1B.14C runtime evidence — 2026-09-16

### Policy 0 — legacy secondary EyeAdaptation off

Observed over hundreds of frames:

```text
Policy=0
PreExposure=1
AutoExposureMethod=0
AA=4
DynamicGI=1
Reflection=1
IndirectLightingIntensity=1
```

The full-screen transformed secondary reference remained severely underexposed / nearly black.

### Policy 1 — secondary EyeAdaptation on

Observed:

```text
Policy=1
Frame 1:   PreExposure=1
Frame 60:  PreExposure=0.00139418803
Frame 120: PreExposure=0.00138348085
Frame 180: PreExposure=0.00138009491
Frame 240: PreExposure=0.00137879699
Frame 300: PreExposure=0.00137867441
Frame 360: PreExposure=0.00137837441
Frame 420: PreExposure=0.00137848745
Frame 480: PreExposure=0.00137784705
AutoExposureMethod=0
AA=4
DynamicGI=1
Reflection=1
IndirectLightingIntensity=1
```

The secondary view brightened dramatically and visually returned to the expected lit scene instead of the legacy near-black result.

This is decisive evidence that the remaining dark-portal mismatch is caused primarily by the manually constructed secondary view running with EyeAdaptation disabled / pre-exposure fixed at 1, not by portal aperture, stencil, depth propagation, main-pass scissor, cached-lighting range, or final composition gain.

The exact secondary pre-exposure does not have to equal the main-view value because the transformed virtual camera can legitimately observe a different luminance distribution. The accepted requirement is that the secondary view owns a valid exposure history and the existing main/secondary pre-exposure rebase converts between domains during composition.

## Next gate

Promote the successful exposure policy into the real `InteriorPortalFullViewFamilyTSRSpike.cpp` producer:

```text
Secondary EyeAdaptation = enabled
persistent SecondaryViewState continues to own exposure history
post-TSR/pre-tonemap extraction remains unchanged
portal.SecondaryPreExposure continues to use measured secondary pre-exposure
existing RGB rebase remains enabled
```

Before declaring final visual parity, run one normal-aperture integration check with the successful policy (not the full-screen reference):

```text
1. Keep SecondaryEyeAdaptation=1 spike running.
2. Stop portal.StartVisualParityReference.
3. Observe the normal portal aperture at the same camera.
4. Compare interior brightness/GI/material response against the directly viewed destination scene.
```

If that matches, integrate the policy into the accepted production TSR producer and perform a final static + motion parity validation.

## Acceptance boundary

`1B.14C` proves the core exposure ownership defect. It does not yet claim the production TSR producer has been updated, and it does not yet claim final static/motion visual parity. No arbitrary brightness, gamma, exposure compensation, or portal-local gain should be introduced.
