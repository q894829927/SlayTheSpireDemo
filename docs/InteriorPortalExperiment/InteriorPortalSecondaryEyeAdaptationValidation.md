# Interior Portal — STEP 1B.14C Secondary EyeAdaptation A/B

Date: **2026-09-16**

State:

```text
STEP 1B.14C SECONDARY EYEADAPTATION A/B
= PASS
= ROOT CAUSE CONFIRMED
= PROMOTED INTO MAIN TSR PRODUCER
```

## Why this gate exists

STEP 1B.14A proved the remaining dark-image mismatch exists in the transformed
secondary full-view renderer itself, before portal aperture/stencil/depth/scissor
integration.

STEP 1B.14B then showed:

```text
main PreExposure ~= 0.00173
secondary producer PreExposure = 1
secondary EyeAdaptation = false
```

Changing `r.EyeAdaptation.CachedLightingPreExposure` from 4 to 8 did not materially
improve the dark secondary, and globally forcing `r.EyeAdaptation.PreExposureOverride=1`
only darkened the real main view rather than fixing the secondary image. Therefore the
next controlled test was secondary EyeAdaptation ownership itself.

## Implementation

Isolation source:

```text
Source/SlayTheSpireDemo/Interior/InteriorPortalSecondaryEyeAdaptationSpike.cpp
```

CVars:

```text
portal.SecondaryEyeAdaptation 0/1
portal.SecondaryEyeAdaptationDiagnostics 0/1
```

Commands:

```text
portal.StartSecondaryEyeAdaptationSpike
portal.DumpSecondaryEyeAdaptationSpike
portal.StopSecondaryEyeAdaptationSpike
```

Report:

```text
Saved/AutomationReports/PortalSecondaryEyeAdaptationSpike.json
```

The spike keeps the accepted transformed full `FSceneViewFamily`, persistent
`FSceneViewState`, TSR, Lumen GI/reflections, clipping plane, post-TSR/pre-tonemap
HDR extraction and pre-exposure rebase structure, but makes the secondary
`EyeAdaptation` show flag controllable.

The secondary Tonemap extraction callback directly records:

```text
secondary PreExposure
AutoExposureMethod
AA method
SceneRect
DynamicGI
Reflection
IndirectLightingIntensity
```

This avoids relying on a global extension being automatically attached to the manual
additional view family.

## Runtime evidence — 2026-09-16

### Policy 0 — legacy forced EyeAdaptation off

The secondary remained severely dark and its persistent pre-exposure stayed pinned at:

```text
Policy=0
PreExposure=1
AA=4
DynamicGI=1
Reflection=1
IndirectLightingIntensity=1
```

This remained stable for hundreds of frames.

### Policy 1 — EyeAdaptation enabled

With no changes to AA, Lumen GI, reflections or indirect-lighting intensity, the
secondary exposure state rapidly converged away from 1.0:

```text
Frame ~60:  PreExposure ~= 0.001468
Frame ~120: PreExposure ~= 0.001460
steady:     PreExposure ~= 0.001457
AA=4
DynamicGI=1
Reflection=1
IndirectLightingIntensity=1
```

The full-screen transformed secondary changed from near-black to normally exposed,
and the normal portal aperture likewise stopped exhibiting the prior near-black
interior. Re-applying normal composition controls such as:

```text
portal.CompositionDebugMode 0
portal.ProjectiveAperture 1
portal.StencilCompositionBypassShaderAperture 0
```

produced essentially no additional visual change, which is expected: those controls
were not the source of the exposure defect.

The runtime dump while the A/B producer remained active recorded approximately:

```text
Policy=1
Submitted=1756
Skipped=0
ExtractionFrames=1756
PreExposure=0.00145737
AutoExposureMethod=0
AA=4
SceneRect=1920x900
DynamicGI=1
Reflection=1
IndirectLightingIntensity=1
```

## Root-cause conclusion

The dominant single-layer visual-parity defect was caused by the production TSR
secondary family forcibly disabling EyeAdaptation:

```cpp
ShowFlags.SetEyeAdaptation(false);
```

That forced the persistent secondary exposure state to remain at pre-exposure 1.0,
while the real game view operated around ~0.0015–0.0018 in the same scene. The later
RGB pre-exposure rebase was numerically correct but could not make renderer-internal
lighting/history behavior equivalent after the secondary view had already rendered in
the wrong exposure policy.

## Production promotion

The accepted fix was promoted into:

```text
Source/SlayTheSpireDemo/Interior/InteriorPortalFullViewFamilyTSRSpike.cpp
```

Production no longer forces EyeAdaptation off. Instead, the secondary family inherits
the EyeAdaptation show flag from the real game viewport, while MotionBlur and DOF
remain disabled so the main view remains owner of those final presentation effects.

Promotion commit:

```text
118a9b426847c4f098d2cb79ca850446ffae822f
portal: inherit main eye adaptation in TSR producer
```

## Final validation still required

The production TSR producer now needs one final PIE validation with the accepted full
feature chain enabled:

```text
portal.StartFullViewFamilyTSRSpike
portal.ProjectiveAperture 1
portal.DepthAwareComposition 1
portal.SecondaryDepthRemap 1
portal.MainDepthPropagation 1
portal.StencilGatedComposition 1
portal.StencilCompositionBypassShaderAperture 0
portal.BoundedMainPassScissor 1
```

Validate at minimum:

```text
static camera
camera translation / rotation
oblique portal view
partial viewport clipping
foreground weapon occlusion
portal approach / close range
```

The portal interior should remain exposure-consistent and should not regress the
already accepted TSR/history/depth/stencil/scissor behavior.

## Acceptance boundary

`STEP 1B.14C = PASS` establishes the dominant dark-secondary root cause and promotes
the corrected exposure-policy ownership into the main TSR producer. It does not yet
claim the final production visual-parity seal until the integrated full-chain PIE
validation passes.
