# Interior Portal — STEP 1B.14B Main-vs-Secondary View Diagnostics

Date: **2026-09-16**

State:

```text
STEP 1B.14B MAIN-VS-SECONDARY VIEW DIAGNOSTICS
= RUNTIME CLASSIFIED
= GLOBAL PRE-EXPOSURE OVERRIDE DOES NOT FIX THE SECONDARY
= NEXT: CONTROLLED SECONDARY EYE-ADAPTATION A/B
```

## Why this gate exists

STEP 1B.14A classified the remaining one-layer visual mismatch as **Outcome B**:
the full-screen transformed secondary reference is already too dark even when
portal-only aperture, stencil, bounded main-pass scissor, main depth propagation,
depth-aware composition and secondary depth remap are disabled.

Therefore the remaining mismatch is upstream of portal composition.

## Runtime evidence

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

The existing RGB rebase correctly converts the numeric pre-exposure domains, but
that alone does not prove renderer-internal Lumen / lighting-cache / temporal-history
behavior was produced under an equivalent exposure policy.

## Cached-lighting pre-exposure A/B

A fixed-camera comparison changed:

```text
r.EyeAdaptation.CachedLightingPreExposure 4 -> 8
```

Result: no material improvement in the dark secondary view. Do not promote `8` as a
project workaround from this experiment.

## Global pre-exposure override A/B

A second fixed-camera comparison changed:

```text
r.EyeAdaptation.PreExposureOverride 0 -> 1
```

Observed result:

```text
Override=0:
  main view is normally exposed
  portal secondary remains substantially too dark

Override=1:
  the whole main scene becomes dramatically darker
  portal secondary remains comparably dark
```

The apparent relative similarity under override 1 comes from darkening the real main
view, not from repairing the secondary renderer. Therefore the remaining visual defect
is not fixed by forcing the global pre-exposure domain and `PreExposureOverride=1`
must not be used as a production workaround.

## Current highest-confidence candidate

The largest deliberate producer-side divergence now remaining is:

```text
Main view:
  normal game EyeAdaptation / exposure ownership

Secondary additional view family:
  ShowFlags.SetEyeAdaptation(false)
  persistent independent ViewState
  Lumen GI / reflections
  TSR
  post-TSR pre-tonemap extraction
```

A post-hoc `MainPreExposure / SecondaryPreExposure` RGB multiply cannot guarantee
that exposure-dependent renderer histories or Lumen/cached-lighting state were
produced under equivalent conditions.

## Next gate — controlled secondary EyeAdaptation A/B

Modify the full-view TSR producer to expose:

```text
portal.SecondaryEyeAdaptation 0/1
```

and replace the hard-coded:

```cpp
ShowFlags.SetEyeAdaptation(false);
```

with the controlled value. Also record directly from the secondary Tonemap extraction
callback:

```text
Secondary PreExposure
Secondary AutoExposureMethod
Secondary DynamicGI
Secondary Reflection
```

This direct callback is authoritative because it is already explicitly attached to
the manually constructed additional view family.

A/B procedure:

```text
0 = current baseline
1 = secondary participates in normal EyeAdaptation/exposure ownership
```

If `1` materially restores the secondary lighting/GI while retaining TSR stability,
make it the production direction and rerun the full-screen visual parity reference.
If it does not, move next to additional-view-family / Lumen lighting-history ownership.

## Do not do

Do not use arbitrary brightness, gamma, exposure compensation, portal-local gain,
`r.EyeAdaptation.CachedLightingPreExposure=8`, or
`r.EyeAdaptation.PreExposureOverride=1` as a production fix.

## Cleanup

```text
r.EyeAdaptation.PreExposureOverride 0
r.EyeAdaptation.CachedLightingPreExposure 4
portal.StopViewParityDiagnostics
```

## Acceptance boundary

This diagnostic stage has completed its classification purpose, but single-layer
visual parity is still **NOT PASS**. The next implementation gate is the controlled
secondary EyeAdaptation A/B.
