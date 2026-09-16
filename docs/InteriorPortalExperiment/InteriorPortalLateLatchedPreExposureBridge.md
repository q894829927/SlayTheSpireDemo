# STEP 1B.14D-B — Late-Latched Main Tonemap PreExposure Bridge

Status: **LEGACY DIAGNOSTIC / SUPERSEDED FOR NORMAL PRODUCTION VALIDATION**

## Historical purpose

The original exposure-authority experiment captured the player-main view state during `SetupView`, but runtime data showed `GetPreExposure()` remained `1.0` there while the real main-view Tonemap callback reported about `0.00205–0.00208` in the same run.

This bridge was added as a narrow diagnostic experiment. It captures the real main-view PreExposure at Tonemap, stores it as a late-latched scalar, then publishes the latest completed value to `portal.SecondaryPreExposure` on the following game-thread tick while keeping `portal.PreExposureRebase=1`.

Commands remain available for historical A/B diagnosis:

```text
portal.LateLatchedPreExposureDiagnostics 1
portal.StartLateLatchedPreExposureBridge
portal.DumpLateLatchedPreExposureBridge
portal.StopLateLatchedPreExposureBridge
```

## Why this is no longer the production direction

Current branch HEAD has a stronger ownership contract than this bridge:

```text
secondary render submission
    -> owns one FColorSample
    -> secondary Tonemap extraction writes that exact sample's PreExposure
    -> the same FInteriorPortalRenderRequest carries that sample to main composition
    -> composition computes MainPreExposure / Sample.PreExposure
    -> an unextracted sample is rejected instead of borrowing old CVar metadata
```

This removes the architectural ambiguity that motivated the late-latched CVar path. A value published on a later game-thread tick cannot prove that it belongs to the exact HDR image currently stored in the portal target; `FColorSample` can.

The bridge therefore must **not** be used as the normal fix for portal brightness and must not be started during the production parity acceptance pass unless a future regression specifically requires historical comparison.

## Current production exposure contract

The accepted direction is:

```text
secondary full FSceneViewFamily keeps EyeAdaptation enabled
persistent secondary ViewState owns its exposure history
post-TSR / pre-tonemap extraction measures the secondary image's own PreExposure
that exact image carries its exact FColorSample metadata
main player view remains the final exposure / local-exposure / grading / tonemap authority
composition rebases only between the two measured pre-exposure domains
```

No portal-local artistic brightness multiplier, gamma correction or `EyeAdaptationInverse` workaround is part of this contract.

## Current next gate

Use:

```text
docs/InteriorPortalExperiment/InteriorPortalProductionParityValidation.md
```

The active sequence is now:

```text
1B.14D-C production TSR dynamic parity validation
    -> static / slant / retreat / leave-and-return / crossing
    -> render-thread main-vs-secondary exposure telemetry
    -> one focused manual PIE visual gate

1B.14E production merge / cleanup + affected regression
```

If 1B.14D-C telemetry is coherent but the portal is still visually wrong, investigate the classified renderer subsystem (for example Lumen/additional-family history or temporal ownership) rather than re-enabling this bridge as a permanent correction.
