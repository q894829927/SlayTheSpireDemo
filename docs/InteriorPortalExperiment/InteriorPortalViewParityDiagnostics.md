# Interior Portal — STEP 1B.14B Main-vs-Secondary View Diagnostics

Date: **2026-09-16**

State:

```text
STEP 1B.14B MAIN-VS-SECONDARY VIEW DIAGNOSTICS
= RUNTIME CLASSIFIED
= MAIN EXPOSURE POLICY MISMATCH CONFIRMED
= CACHED-LIGHTING PREEXPOSURE RANGE A/B DID NOT MATERIALLY IMPROVE THE DARK SECONDARY
```

## Why this gate exists

STEP 1B.14A classified the remaining one-layer visual mismatch as **Outcome B**:
the full-screen transformed secondary reference is already too dark even when
portal-only aperture, stencil, bounded main-pass scissor, main depth propagation,
depth-aware composition and secondary depth remap are disabled.

Therefore the remaining mismatch is upstream of portal composition. The next step
is to compare the real main-view Tonemap-stage exposure/post-process state against
the secondary full-view producer policy instead of continuing aperture/depth/stencil
work or adding brightness/gamma compensation.

## Implementation

Source:

```text
Source/SlayTheSpireDemo/Interior/InteriorPortalViewParityDiagnostics.cpp
```

Commands:

```text
portal.StartViewParityDiagnostics
portal.DumpViewParityDiagnostics
portal.StopViewParityDiagnostics
```

Runtime report:

```text
Saved/AutomationReports/PortalViewParityDiagnostics.json
```

The diagnostic extension subscribes to the real Tonemap boundary and records:

```text
PreExposure
AA method
Tonemap SceneColor extent
AutoExposureBias
AutoExposureMethod
DynamicGlobalIlluminationMethod
ReflectionMethod
IndirectLightingIntensity
```

It records the real main view directly. If the manually constructed additional
secondary family exposes the registered extension, it records that view directly as
well. If it does not, the report states `secondaryObserved.available=false` and uses
the accepted secondary producer's configured policy plus the measured
`portal.SecondaryPreExposure` CVar as the authoritative secondary-side evidence.

The current secondary producer policy is intentionally explicit:

```text
EyeAdaptation = false
MotionBlur = false
DepthOfField = false
TemporalAA = true
ScreenPercentage = true
SceneCaptureSource = SCS_FinalColorHDR
Dynamic GI = forced Lumen
Reflection = forced Lumen
```

This makes the deliberate `EyeAdaptation=false` difference a first-class candidate
for the dark-secondary mismatch. The diagnostic does not yet change that policy.

## Runtime evidence — 2026-09-16

The real main-view Tonemap callback stabilized at approximately:

```text
Main PreExposure ~= 0.00173 .. 0.00176
AA = TSR (4)
AutoExposureMethod = 0
ExposureBias = 0
DynamicGI = Lumen
Reflection = Lumen
IndirectLightingIntensity = 1
```

The saved report recorded:

```text
mainTonemapFrames = 1324
secondaryTonemapFramesObservedByDiagnosticExtension = 0
main.preExposure = 0.00173180806
secondaryProducerPolicy.eyeAdaptation = false
secondaryProducerPolicy.measuredPreExposureCVar = 1
preExposureRebaseEnabled = true
mainToSecondaryPreExposureScale = 0.00173180806
```

This confirms a large policy difference between the real main view and the manually
constructed secondary family. The existing RGB rebase correctly maps the two
pre-exposure domains numerically, but it does not prove that renderer-internal
lighting/history/cache behavior is equivalent when the secondary view is rendered
with EyeAdaptation disabled and pre-exposure 1.

A follow-up A/B changed:

```text
r.EyeAdaptation.CachedLightingPreExposure 4 -> 8
```

with a fixed camera. The two portal images were visually almost unchanged: the
secondary interior remained substantially darker than the real scene. Therefore the
cached-lighting supported-range setting is not accepted as the primary fix for this
mismatch. Do not promote `8` as a project workaround from this experiment.

## Next isolation

Before changing the secondary renderer, run a diagnostic main-view pre-exposure
alignment test:

```text
r.EyeAdaptation.PreExposureOverride 0
```

capture baseline, then at the same camera:

```text
r.EyeAdaptation.PreExposureOverride 1
```

This temporarily moves the real main view into the same pre-exposure domain already
used by the current secondary producer. It is a diagnostic override only, not a
production fix. If the portal/real-scene mismatch changes materially, implement a
controlled secondary EyeAdaptation ownership A/B next. If it does not, move directly
to additional-view-family / Lumen construction and lighting-history parity.

Restore after the A/B:

```text
r.EyeAdaptation.PreExposureOverride 0
```

## Decision rule

```text
A. PreExposureOverride=1 materially reduces the mismatch
   -> implement secondary EyeAdaptation / pre-exposure ownership A/B in the producer.

B. PreExposureOverride=1 does not materially reduce the mismatch
   -> exposure-domain mapping is not the main visual defect; inspect the manually
      constructed additional FSceneViewFamily and Lumen/history ownership next.
```

Do not use arbitrary brightness, gamma, exposure compensation or portal-local gain as
a fix.

## Cleanup

```text
portal.StopViewParityDiagnostics
r.EyeAdaptation.PreExposureOverride 0
```

## Acceptance boundary

This step remains diagnostic. It does not claim visual parity. The runtime evidence
has already rejected portal-only aperture/depth/stencil/scissor causes and has now
also rejected `r.EyeAdaptation.CachedLightingPreExposure 8` as a meaningful visual
fix. The next gate must distinguish exposure-domain ownership from additional-family
Lumen/view construction.
