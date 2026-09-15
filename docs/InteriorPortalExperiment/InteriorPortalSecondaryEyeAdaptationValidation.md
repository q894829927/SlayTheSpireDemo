# Interior Portal — STEP 1B.14C Secondary EyeAdaptation A/B

Date: **2026-09-16**

State:

```text
STEP 1B.14C SECONDARY EYEADAPTATION A/B
= STATIC EXPOSURE ROOT CAUSE CONFIRMED
= PRODUCTION PROMOTION COMPLETE
= DYNAMIC / GRAZING VALIDATION FAILED
= NEXT GATE: EXPOSURE AUTHORITY ACROSS SECONDARY CAMERA CUTS
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

## Runtime evidence — static A/B

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
interior while the isolation producer remained stable.

## Initial root-cause conclusion

The dominant static visual-parity defect was caused by the production TSR secondary
family forcibly disabling EyeAdaptation:

```cpp
ShowFlags.SetEyeAdaptation(false);
```

That forced the persistent secondary exposure state to remain at pre-exposure 1.0,
while the real game view operated around ~0.0015–0.0018 in the same scene. The later
RGB pre-exposure rebase was numerically correct but could not make renderer-internal
lighting/history behavior equivalent after the secondary view had already rendered in
the wrong exposure policy.

## Production promotion

The static fix was promoted into:

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

## Integrated dynamic validation — failure refinement

The full production chain was then exercised while rotating obliquely and backing away
from the portal. The portal can become dark again even though the static view initially
converges correctly.

The decisive telemetry is not a loss of TSR or depth transport. Instead, the secondary
camera-cut count increases as visibility / selected endpoint changes:

```text
Frame 1409: Endpoint=0 CameraCuts=1
Frame 1469: Endpoint=0 CameraCuts=3
Frame 1599: Endpoint=1 CameraCuts=4
Frame 1660: Endpoint=0 CameraCuts=5
```

The production producer currently owns one secondary `FSceneViewStateReference`, and
its camera-cut policy explicitly cuts when the visible endpoint changes. Frames that
leave the visible set also invalidate the same temporal history. With EyeAdaptation now
owned by that same secondary view state, these cut / re-entry events can restart the
secondary exposure history and recreate the near-black warm-up state that was observed
at the start of Policy=1.

Therefore the refined root cause is:

```text
static mismatch:
  EyeAdaptation forcibly disabled -> fixed

dynamic mismatch:
  secondary TSR/Lumen temporal cuts and secondary exposure history share one owner
  -> oblique / retreat visibility changes can reset exposure together with TSR history
```

## Next gate — 1B.14D Exposure authority decoupling

Do not remove required TSR camera cuts merely to keep exposure alive. Endpoint changes,
re-entry after visibility loss and true portal-frame discontinuities still require
safe temporal invalidation.

The next implementation must separate the two responsibilities:

```text
Secondary temporal ViewState
  owns TSR / velocity / Lumen temporal history
  may still camera-cut when required

Exposure authority
  must remain continuous across those secondary-only cuts
  should be sourced from the player-main exposure state rather than a portal-local
  independently resetting adaptation history
```

UE 5.8 exposes `FSceneViewInitOptions::ExposureSceneViewStateInterface`, which is the
preferred public integration point to test for this separation while preserving the
secondary `SceneViewStateInterface` for TSR/history ownership.

A successful 1B.14D run must keep the portal exposure stable when:

```text
rotating to a grazing angle
backing away
briefly leaving / re-entering the visible set
switching the selected visible endpoint
```

while retaining the accepted camera-cut semantics required by TSR.

## Acceptance boundary

`STEP 1B.14C` remains accepted only as the proof that enabling EyeAdaptation fixes the
static secondary exposure defect. The integrated production visual-parity seal is NOT
accepted yet because exposure is still coupled to secondary camera-cut lifetime.
