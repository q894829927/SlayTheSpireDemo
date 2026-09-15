# Interior Portal — STEP 1B.14C / 1B.14D Exposure Parity

Date: **2026-09-16**

State:

```text
STEP 1B.14C SECONDARY EYEADAPTATION A/B
= STATIC EXPOSURE ROOT CAUSE CONFIRMED
= PRODUCTION PROMOTION COMPLETE

STEP 1B.14D EXPOSURE AUTHORITY DECOUPLING
= IMPLEMENTED
= USER BUILD / DYNAMIC PIE EVIDENCE REQUIRED
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
only darkened the real main view rather than fixing the secondary image.

## STEP 1B.14C — static EyeAdaptation result

Isolation source:

```text
Source/SlayTheSpireDemo/Interior/InteriorPortalSecondaryEyeAdaptationSpike.cpp
```

CVars / commands:

```text
portal.SecondaryEyeAdaptation 0/1
portal.SecondaryEyeAdaptationDiagnostics 0/1
portal.StartSecondaryEyeAdaptationSpike
portal.DumpSecondaryEyeAdaptationSpike
portal.StopSecondaryEyeAdaptationSpike
```

Policy 0 kept the persistent secondary pre-exposure at 1.0 and the transformed view
was severely dark. Policy 1 kept AA, Lumen GI/reflections and indirect-lighting
intensity unchanged while secondary pre-exposure converged to approximately
0.00145 and the static transformed view became normally exposed.

The static production defect was therefore the forced:

```cpp
ShowFlags.SetEyeAdaptation(false);
```

Production was changed to inherit the real game viewport EyeAdaptation show flag:

```text
118a9b426847c4f098d2cb79ca850446ffae822f
portal: inherit main eye adaptation in TSR producer
```

## Dynamic validation — remaining failure

The full production chain was then exercised while rotating obliquely and backing away.
The portal can become dark again after initially converging correctly.

Telemetry shows the secondary camera-cut count increasing and the selected endpoint
changing while this motion occurs:

```text
Frame 1409: Endpoint=0 CameraCuts=1
Frame 1469: Endpoint=0 CameraCuts=3
Frame 1599: Endpoint=1 CameraCuts=4
Frame 1660: Endpoint=0 CameraCuts=5
```

The production producer owns a single secondary `FSceneViewStateReference`. That state
was simultaneously acting as:

```text
TSR / velocity / Lumen temporal history owner
EyeAdaptation / exposure history owner
```

This coupling is unsafe for a portal. The secondary temporal state must still be cut on
real discontinuities, endpoint changes and visibility re-entry, but those portal-local
cuts must not reset the player's exposure authority.

## STEP 1B.14D — main-view exposure authority spike

Source:

```text
Source/SlayTheSpireDemo/Interior/InteriorPortalExposureAuthoritySpike.cpp
```

Implementation commit:

```text
0b97b322206501e90a6cce111689553bb947c6ba
portal: add main-view exposure authority spike
```

UE 5.8 exposes two separate view-state inputs in `FSceneViewInitOptions`:

```text
SceneViewStateInterface
ExposureSceneViewStateInterface
```

The spike uses that split deliberately:

```text
SceneViewStateInterface
  = persistent secondary state
  = owns TSR / velocity / Lumen temporal history
  = keeps the accepted camera-cut policy

ExposureSceneViewStateInterface
  = captured real player-main ViewState when Policy=1
  = remains independent of secondary endpoint/re-entry cuts
```

A world scene-view extension captures only non-additional main views and records the
player-main ViewState plus its current pre-exposure. The manual transformed secondary
family is never allowed to replace that capture because it is an additional view
family.

CVars:

```text
portal.SecondaryExposureAuthority 0/1
portal.SecondaryExposureAuthorityDiagnostics 0/1
```

Commands:

```text
portal.StartExposureAuthoritySpike
portal.DumpExposureAuthoritySpike
portal.StopExposureAuthoritySpike
```

Report:

```text
Saved/AutomationReports/PortalExposureAuthoritySpike.json
```

Periodic telemetry records:

```text
Policy
MainAuthorityActive
MainCaptureFrame
MainPreExposure
SceneColorPreExposure
SecondaryTemporalPreExposure
Endpoint
CameraCut / CameraCuts
CutReason
```

The important proof is that with Policy=1, `SceneColorPreExposure` should track the
captured player-main exposure even when the secondary temporal state camera-cuts or the
selected endpoint changes. `SecondaryTemporalPreExposure` is allowed to reset because
it is no longer the exposure authority.

## Runtime procedure

Use a fresh PIE session. Restore exposure diagnostics first:

```text
r.EyeAdaptation.PreExposureOverride 0
r.EyeAdaptation.CachedLightingPreExposure 4
r.AntiAliasingMethod 4
```

Disable other secondary producers, then isolate color/exposure from the already proven
depth/stencil stages:

```text
portal.StopFullViewFamilyTSRSpike
portal.StopSecondaryEyeAdaptationSpike
portal.StopVisualParityReference

portal.CompositionDebugMode 0
portal.ProjectiveAperture 1
portal.DepthAwareComposition 0
portal.SecondaryDepthRemap 0
portal.MainDepthPropagation 0
portal.StencilGatedComposition 0
portal.BoundedMainPassScissor 0

portal.FullViewFamilyTSRPrimaryFraction 0.67
portal.SecondaryExposureAuthority 1
portal.SecondaryExposureAuthorityDiagnostics 1
portal.StartExposureAuthoritySpike
```

Wait several seconds. Confirm telemetry contains:

```text
Policy=1
MainAuthorityActive=1
MainPreExposure ~= SceneColorPreExposure
```

Then deliberately reproduce the previous failure:

```text
rotate to a grazing portal angle
back away until the portal is small
briefly let it leave/re-enter the visible set
move so endpoint selection can change if possible
```

CameraCuts are allowed and expected. The portal must not become near-black merely due
to those cuts.

Finally:

```text
portal.DumpExposureAuthoritySpike
portal.StopExposureAuthoritySpike
```

## Decision rule

```text
PASS:
  MainAuthorityActive=1
  MainPreExposure and SceneColorPreExposure remain aligned
  CameraCuts may increase
  portal exposure remains stable while grazing / retreating / re-entering

FAIL-A:
  MainAuthorityActive=0
  -> main ViewState capture/lifetime is not reaching the producer

FAIL-B:
  MainAuthorityActive=1 and exposure values remain aligned, but portal still goes dark
  -> exposure is no longer the cause; inspect visibility selection / clip plane / target freshness

FAIL-C:
  assigning the main exposure state visibly perturbs the real main-view exposure
  -> do not promote; use a dedicated persistent exposure state seeded from main instead
```

Do not remove required TSR camera cuts merely to keep brightness stable, and do not use
portal-local brightness/gamma/exposure compensation as a substitute.

## Acceptance boundary

`STEP 1B.14C` is accepted only for the static EyeAdaptation defect. The integrated
production visual-parity seal remains open until 1B.14D proves exposure remains stable
across the dynamic portal camera-cut / visibility lifecycle.
