# Interior Portal — STEP 1B.14B Main-vs-Secondary View Diagnostics

Date: **2026-09-16**

State:

```text
STEP 1B.14B MAIN-VS-SECONDARY VIEW DIAGNOSTICS
= IMPLEMENTED
= USER BUILD / PIE EVIDENCE REQUIRED
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

## Runtime procedure

Close Unreal Editor and rebuild because this step adds a C++ translation unit.

Start PIE and establish the accepted secondary producer:

```text
r.AntiAliasingMethod 4
portal.FullViewFamilyTSRPrimaryFraction 0.67
portal.StartFullViewFamilyTSRSpike
```

Then start the diagnostic:

```text
portal.StartViewParityDiagnostics
```

Leave the camera fixed for roughly 2–3 seconds, then:

```text
portal.DumpViewParityDiagnostics
```

Expected main-view periodic log:

```text
PortalViewParity Main Tonemap ...
PreExposure=...
AA=...
ExposureBias=...
AutoExposureMethod=...
DynamicGI=...
Reflection=...
IndirectLightingIntensity=...
```

Expected summary log:

```text
PortalViewParity Report ...
MainPreExposure=...
SecondaryProducerPreExposure=...
Rebase=1
RebaseScale=...
SecondaryPolicyEyeAdaptation=0
```

If the secondary additional family also receives the extension, a separate
`PortalViewParity Secondary Tonemap ...` line appears and
`secondaryObservedFrames > 0` is recorded. Its absence is not a failure of this
gate because the manual full-view family historically requires explicit extension
attachment.

## Decision rule

The first follow-up will be selected from the evidence:

```text
A. Main exposure/post-process state materially differs from the configured secondary policy
   -> implement a controlled policy A/B, starting with secondary EyeAdaptation ownership.

B. Exposure/post-process state is effectively equivalent but the secondary remains dark
   -> inspect Lumen/view-family construction and lighting-cache ownership next.

C. Static state matches but mismatch is motion-only
   -> inspect velocity / TSR / screen-space history next.
```

Do not use arbitrary brightness, gamma, exposure compensation or portal-local gain as
a fix.

## Cleanup

```text
portal.StopViewParityDiagnostics
```

## Acceptance boundary

This step is diagnostic only. It does not claim visual parity. PASS means the runtime
telemetry identifies which view-family/exposure policy should be A/B tested next.
