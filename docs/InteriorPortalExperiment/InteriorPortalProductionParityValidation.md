# Interior Portal — STEP 1B.14D-C Production TSR Parity Validation

Date: **2026-09-16**

State:

```text
STEP 1B.14A VISUAL-PARITY ISOLATION = PASS / OUTCOME B
STEP 1B.14B MAIN-VS-SECONDARY DIAGNOSTICS = PASS / ROOT CAUSE CLASSIFIED
STEP 1B.14C SECONDARY EYE ADAPTATION A/B = PASS
STEP 1B.14D EXACT PER-SUBMISSION COLOR SAMPLE OWNERSHIP = PRESENT IN HEAD
STEP 1B.14D-C PRODUCTION TSR DYNAMIC PARITY VALIDATION = IMPLEMENTED / USER PIE REQUIRED
```

## Why this is the next gate

The remaining portal darkness was already isolated upstream of aperture, stencil,
depth propagation and bounded main-pass composition. The decisive runtime A/B was:

```text
secondary EyeAdaptation OFF -> secondary PreExposure pinned near 1 -> remote full view nearly black
secondary EyeAdaptation ON  -> secondary PreExposure converges to the expected small HDR-domain value -> lit remote view returns
```

The accepted full-view TSR producer now preserves the game viewport EyeAdaptation
show flag instead of forcing it off. The current HEAD also associates each submitted
portal HDR image with its own `FColorSample`; the Tonemap extraction writes that
sample's measured secondary PreExposure and the main compositor uses the same sample
for the `MainPreExposure / SecondaryPreExposure` rebase. This removes the old
one-frame CVar ownership ambiguity from the normal path.

Therefore the next useful work is not another exposure multiplier and not more
portal-edge tuning. It is one dynamic integration gate proving that the accepted
production TSR path keeps those contracts while the camera moves, the portal leaves
the visible set, returns, and the player crosses.

## Implementation

Source:

```text
Source/SlayTheSpireDemo/Interior/InteriorPortalProductionParityValidation.cpp
```

Commands:

```text
portal.ProductionParityDiagnostics 1
portal.StartProductionParityValidation
portal.DumpProductionParityValidation
portal.StopProductionParityValidation
```

Report:

```text
Saved/AutomationReports/PortalProductionParityValidation.json
```

The probe observes the actual render-thread `Tonemap` callbacks for:

```text
player main primary view
portal secondary primary non-capture additional view
```

It records:

```text
main / secondary Tonemap frame counts
main / secondary PreExposure
main / secondary EyeAdaptation show-flag state
main / secondary AA method
invalid PreExposure frame counts
main / secondary camera-cut counts
last observed main and secondary renderer frames
main / secondary exposure-domain scale
```

It does not write exposure, gamma, brightness, EyeAdaptation, temporal state,
portal depth, stencil, aperture, clip plane or renderer settings.

## Automated telemetry classification

The final JSON classification is intentionally narrow.

```text
INSUFFICIENT_DATA
    -> fewer than 30 main or secondary Tonemap observations

INVALID_PREEXPOSURE_OBSERVED
    -> at least one main or secondary Tonemap sample had non-finite / non-positive PreExposure

SECONDARY_EYE_ADAPTATION_DISABLED
    -> the accepted secondary additional view was observed with EyeAdaptation off

SECONDARY_NOT_TSR
    -> the accepted secondary view was not observed as AAM_TSR

TELEMETRY_READY_MANUAL_VISUAL_GATE_REQUIRED
    -> renderer ownership telemetry is coherent; visual parity still requires PIE judgement
```

`TELEMETRY_READY_MANUAL_VISUAL_GATE_REQUIRED` is not itself a visual PASS.

## USER ACTION REQUIRED — one focused PIE pass

Build the branch, open the existing interior portal test map and keep the accepted
full-view TSR producer running:

```text
r.AntiAliasingMethod 4
portal.FullViewFamilyTSRPrimaryFraction 0.67
portal.StartFullViewFamilyTSRSpike
portal.ProductionParityDiagnostics 1
portal.StartProductionParityValidation
```

Perform only this sequence:

```text
1. Hold a static direct view through the portal for ~3 seconds.
2. Move laterally into a strong slant / grazing view and hold briefly.
3. Approach the aperture, then retreat without crossing.
4. Turn until the portal is fully outside the viewport for ~2 seconds.
5. Turn back to the same portal and hold.
6. Cross the portal once, then stop and look back through it.
```

Then run:

```text
portal.DumpProductionParityValidation
portal.StopProductionParityValidation
portal.DumpFullViewFamilyTSRSpike
```

## Automated acceptance

The telemetry side passes only if:

```text
mainTonemapFrames >= 30
secondaryTonemapFrames >= 30
invalidMainExposureFrames == 0
invalidSecondaryExposureFrames == 0
secondaryEyeAdaptationOffFrames == 0
secondaryEyeAdaptation == 1
secondaryAAMethod == AAM_TSR
classification == TELEMETRY_READY_MANUAL_VISUAL_GATE_REQUIRED
```

Secondary camera cuts do not need to be zero. They must remain explainable by the
existing structural reset policy rather than occurring continuously during ordinary
motion.

## Manual visual acceptance

At the same camera / destination relationship:

```text
portal interior brightness is materially consistent with direct destination viewing
no one-frame black / near-black flash when the portal returns to view
no old exposure-domain frame appears after leave-and-return
no obvious brightness discontinuity during approach / retreat
no large exposure pop caused specifically by physical crossing
slant / grazing views retain the accepted aperture and clipping behavior
```

Do not tune a constant portal gain to pass this gate.

## Failure routing

```text
secondaryEyeAdaptationOffFrames > 0
    -> production TSR ViewFamily policy regressed; repair ShowFlags ownership

invalidSecondaryExposureFrames > 0
    -> audit secondary ViewState lifetime / Tonemap extraction / sample completion

secondaryAAMethod != TSR
    -> audit AA setup / screen-percentage producer setup

telemetry passes but view still dark
    -> investigate Lumen / reflection / additional-family lighting history next

telemetry passes, static view matches, motion or return flashes
    -> investigate temporal-history reset / exact render-sample lifetime next

only portal boundary fails while full secondary image is correct
    -> return to aperture/depth/stencil integration, not exposure
```

## Gate after PASS

If both telemetry and manual visual acceptance pass, the next renderer task is:

```text
STEP 1B.14E — production merge / cleanup + focused regression
```

That gate should:

```text
retain production secondary EyeAdaptation ownership
retain exact per-submission FColorSample exposure metadata
retire the late-latched CVar bridge from normal validation instructions
run the affected depth/stencil/TSR/near-grazing regressions once
record single-layer visual parity as accepted
```

Only after 1B.14E should performance work such as bounded secondary transport/view
allocation resume. Recursion >= 2 and full physics remain later gates.
