# Interior Portal Experiment

This folder contains the documentation for the UE 5.8 interior-portal / full-fidelity rendering experiment on branch `portal/full-fidelity-p1`.

## Start here

- [InteriorPortalFullFidelityProductionRegression.md](InteriorPortalFullFidelityProductionRegression.md) — **current focused gate**: accepted FullFidelity behavior is being closed as a production path. Player lifecycle ownership now enters through a native API instead of console-command dispatch; run the focused FullFidelity/MainViewOwnership/ColorSampleExposure Automation set plus one compact PIE regression before marking the current renderer scope closed.
- [InteriorPortalOffscreenPublicationRetirement.md](InteriorPortalOffscreenPublicationRetirement.md) — **PASS in user PIE**: fast visible -> offscreen transitions no longer expose the default spiral after publication retirement was ordered on the render queue with generation checks.
- [InteriorPortalGrazingAndRecursionRootFix.md](InteriorPortalGrazingAndRecursionRootFix.md) — analytic world ray/portal-plane aperture and FullFidelity recursion are implemented; the user has accepted current grazing behavior and confirmed `RecursionDepth=2` renders a nested portal before crossing.
- [InteriorPortalGrazingForegroundDepthRegression.md](InteriorPortalGrazingForegroundDepthRegression.md) — previous intermediate diagnosis that separated cosmetic Surface depth from the logical traversal plane. Useful evidence, but superseded by the analytic root fix.
- [InteriorPortalMultiVisibleValidation.md](InteriorPortalMultiVisibleValidation.md) — STEP 1B.14D-MV gate: dual-visible correctness and automatic/exclusive FullFidelity lifecycle are accepted; the prior 161 MB video-memory over-budget warning disappeared once legacy SceneCapture was excluded.
- [InteriorPortalProductionParityValidation.md](InteriorPortalProductionParityValidation.md) — STEP 1B.14D-C **PASS for one visible portal**: static and dynamic TSR/exposure/composition parity accepted, including slant, leave-return and crossing.
- [InteriorPortalViewParityDiagnostics.md](InteriorPortalViewParityDiagnostics.md) — STEP 1B.14B/14C main-vs-secondary diagnostics and accepted EyeAdaptation root-cause classification.
- [InteriorPortalVisualParityIsolation.md](InteriorPortalVisualParityIsolation.md) — STEP 1B.14A visual-parity classification; Outcome B accepted.
- [InteriorPortalLateLatchedPreExposureBridge.md](InteriorPortalLateLatchedPreExposureBridge.md) — legacy diagnostic only; superseded for normal production validation by exact per-submission color metadata.
- [InteriorPortalCurrentExecutionPlan.md](InteriorPortalCurrentExecutionPlan.md) — original execution-order authority; its early renderer baseline predates later 1B.5–1B.14 evidence, so use the newer gate documents above for current renderer closure state.
- [InteriorPortalFullFidelityImplementationPlan.md](InteriorPortalFullFidelityImplementationPlan.md) — full renderer implementation plan and acceptance boundaries.
- [InteriorPortalObservedIssues.md](InteriorPortalObservedIssues.md) — observed defects and evidence.
- [InteriorPortals.md](InteriorPortals.md) — general portal feature notes.

## Full-view renderer, HDR and temporal path

- [InteriorPortalCRPBaseColorDiagnostic.md](InteriorPortalCRPBaseColorDiagnostic.md)
- [InteriorPortalFullSceneViewSpike.md](InteriorPortalFullSceneViewSpike.md)
- [InteriorPortalFullViewFamilySpike.md](InteriorPortalFullViewFamilySpike.md)
- [InteriorPortalFullViewFamilySpikeValidation.md](InteriorPortalFullViewFamilySpikeValidation.md)
- [InteriorPortalBeforeDOFExtractionSpike.md](InteriorPortalBeforeDOFExtractionSpike.md)
- [InteriorPortalBeforeDOFExtractionValidation.md](InteriorPortalBeforeDOFExtractionValidation.md)
- [InteriorPortalBeforeDOFExtractionResult.md](InteriorPortalBeforeDOFExtractionResult.md)
- [InteriorPortalFullViewFamilyMainCompositionValidation.md](InteriorPortalFullViewFamilyMainCompositionValidation.md)
- [InteriorPortalFullViewFamilyRealtimeValidation.md](InteriorPortalFullViewFamilyRealtimeValidation.md)
- [InteriorPortalPreExposureRebaseValidation.md](InteriorPortalPreExposureRebaseValidation.md)
- [InteriorPortalFullViewFamilyTemporalValidation.md](InteriorPortalFullViewFamilyTemporalValidation.md)
- [InteriorPortalFullViewFamilyTSRValidation.md](InteriorPortalFullViewFamilyTSRValidation.md)
- [InteriorPortalProductionParityValidation.md](InteriorPortalProductionParityValidation.md)
- [InteriorPortalMultiVisibleValidation.md](InteriorPortalMultiVisibleValidation.md)
- [InteriorPortalGrazingAndRecursionRootFix.md](InteriorPortalGrazingAndRecursionRootFix.md)
- [InteriorPortalOffscreenPublicationRetirement.md](InteriorPortalOffscreenPublicationRetirement.md)
- [InteriorPortalFullFidelityProductionRegression.md](InteriorPortalFullFidelityProductionRegression.md)

## Aperture, depth and downstream consumers

- [InteriorPortalNearGrazingValidation.md](InteriorPortalNearGrazingValidation.md)
- [InteriorPortalMainDepthContinuityValidation.md](InteriorPortalMainDepthContinuityValidation.md)
- [InteriorPortalSecondaryDepthTransportValidation.md](InteriorPortalSecondaryDepthTransportValidation.md)
- [InteriorPortalMainDepthPropagationValidation.md](InteriorPortalMainDepthPropagationValidation.md)
- [InteriorPortalDOFDepthConsumerValidation.md](InteriorPortalDOFDepthConsumerValidation.md)
- [InteriorPortalGrazingForegroundDepthRegression.md](InteriorPortalGrazingForegroundDepthRegression.md)
- [InteriorPortalGrazingAndRecursionRootFix.md](InteriorPortalGrazingAndRecursionRootFix.md)

## Stencil and bounded composition

- [InteriorPortalStencilIdentityValidation.md](InteriorPortalStencilIdentityValidation.md)
- [InteriorPortalStencilTonemapValidation.md](InteriorPortalStencilTonemapValidation.md)
- [InteriorPortalStencilGatedCompositionValidation.md](InteriorPortalStencilGatedCompositionValidation.md)
- [InteriorPortalBoundedMainPassValidation.md](InteriorPortalBoundedMainPassValidation.md)

## Visual validation and gameplay-side portal behavior

- [InteriorPortalVisualAutomation.md](InteriorPortalVisualAutomation.md)
- [InteriorPortalVisualParityIsolation.md](InteriorPortalVisualParityIsolation.md)
- [InteriorPortalViewParityDiagnostics.md](InteriorPortalViewParityDiagnostics.md)
- [InteriorPortalSecondaryEyeAdaptationValidation.md](InteriorPortalSecondaryEyeAdaptationValidation.md)
- [InteriorPortalProductionParityValidation.md](InteriorPortalProductionParityValidation.md)
- [InteriorPortalMultiVisibleValidation.md](InteriorPortalMultiVisibleValidation.md)
- [InteriorPortalGrazingForegroundDepthRegression.md](InteriorPortalGrazingForegroundDepthRegression.md)
- [InteriorPortalGrazingAndRecursionRootFix.md](InteriorPortalGrazingAndRecursionRootFix.md)
- [InteriorPortalOffscreenPublicationRetirement.md](InteriorPortalOffscreenPublicationRetirement.md)
- [InteriorPortalFullFidelityProductionRegression.md](InteriorPortalFullFidelityProductionRegression.md)
- [InteriorPortalPlayerTraversal.md](InteriorPortalPlayerTraversal.md)

## Folder policy

New documents created specifically for this portal experiment should be added under `docs/InteriorPortalExperiment/` rather than directly under `docs/`.

Keep project-wide documents such as `docs/Architecture.md`, `docs/DevelopmentPhases.md`, `docs/Validation.md`, and `docs/CODEX_GOAL_CHECKPOINT.md` at the `docs/` root.
