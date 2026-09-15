# Interior Portal — STEP 1B.14C Secondary EyeAdaptation A/B

Date: **2026-09-16**

State:

```text
STEP 1B.14C SECONDARY EYEADAPTATION A/B
= IMPLEMENTED
= USER BUILD / PIE EVIDENCE REQUIRED
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
next controlled test is secondary EyeAdaptation ownership itself.

## Implementation

Source:

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

## Runtime procedure

Reset diagnostic globals first:

```text
r.EyeAdaptation.PreExposureOverride 0
r.EyeAdaptation.CachedLightingPreExposure 4
```

### A — legacy policy

```text
portal.SecondaryEyeAdaptation 0
portal.StartSecondaryEyeAdaptationSpike
portal.StartVisualParityReference
```

Keep the camera fixed for several seconds, capture the full-screen secondary image,
then:

```text
portal.DumpSecondaryEyeAdaptationSpike
portal.StopVisualParityReference
portal.StopSecondaryEyeAdaptationSpike
```

### B — EyeAdaptation enabled

At the same player camera:

```text
portal.SecondaryEyeAdaptation 1
portal.StartSecondaryEyeAdaptationSpike
portal.StartVisualParityReference
```

Wait at least several seconds so the persistent secondary exposure state can settle,
capture the same full-screen transformed view, then:

```text
portal.DumpSecondaryEyeAdaptationSpike
portal.StopVisualParityReference
portal.StopSecondaryEyeAdaptationSpike
```

## Decision rule

```text
A. Policy=1 materially restores secondary brightness/GI and secondary PreExposure
   moves away from 1 toward the real main-view exposure domain
   -> promote EyeAdaptation ownership into the main TSR producer and run final visual
      parity validation.

B. Policy=1 changes secondary PreExposure but image remains materially too dark
   -> inspect additional-view-family Lumen/history/cache ownership next.

C. Policy=1 leaves secondary PreExposure at 1 and image essentially unchanged
   -> the show flag alone is insufficient; next gate must copy/seed the main-view
      exposure/post-process ownership more completely in the manual view family.
```

Do not use arbitrary brightness, gamma, exposure compensation or portal-local gain.

## Acceptance boundary

This is an A/B isolation producer, not the production portal renderer. It intentionally
omits secondary depth transport because STEP 1B.14A already proved the visual mismatch
exists upstream of portal depth/stencil integration. A PASS means the A/B identifies
whether secondary EyeAdaptation ownership is the remaining dominant visual-parity
cause.
