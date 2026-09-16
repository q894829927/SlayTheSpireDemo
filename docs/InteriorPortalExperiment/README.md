# Interior Portal Experiment

This folder contains the documentation for the UE 5.8 interior-portal / full-fidelity rendering experiment on branch `portal/full-fidelity-p1`.

## Start here

- [InteriorPortalFullFidelityForegroundOcclusionRegression.md](InteriorPortalFullFidelityForegroundOcclusionRegression.md) — **current gate**: MV-C exclusive lifecycle auto-start and dual-visible rendering are user-validated, the duplicate-renderer VRAM warning is no longer reproduced, and the newly observed first-person flashlight foreground-occlusion regression has a narrow depth-aware composition fix awaiting one PIE check.
- [InteriorPortalMultiVisibleValidation.md](InteriorPortalMultiVisibleValidation.md) — STEP 1B.14D-MV history and endpoint-owned Blue + Orange TSR renderer validation. The original one-black-portal defect is closed.
- [InteriorPortalMainDepthContinuityValidation.md](InteriorPortalMainDepthContinuityValidation.md) — STEP 1B.12A accepted main SceneDepth foreground-occlusion contract; real foreground geometry, including first-person geometry, must win in front of the physical entry plane.
- [InteriorPortalProductionParityValidation.md](InteriorPortalProductionParityValidation.md) — STEP 1B.14D-C **PASS for one visible portal**: static and dynamic TSR/exposure/composition parity accepted, including slant, leave-return and crossing.
- [InteriorPortalViewParityDiagnostics.md](InteriorPortalViewParityDiagnostics.md) — STEP 1B.14B/14C main-vs-secondary diagnostics and the accepted EyeAdaptation root-cause classification.
- [InteriorPortalVisualParityIsolation.md](InteriorPortalVisualParityIsolation.md) — STEP 1B.14A visual-parity classification; Outcome B accepted.
- [InteriorPortalLateLatchedPreExposureBridge.md](InteriorPortalLateLatchedPreExposureBridge.md) — legacy diagnostic only; superseded for normal production validation by exact per-submission color metadata.
- [InteriorPortalCurrentExecutionPlan.md](InteriorPortalCurrentExecutionPlan.md) — original execution-order authority; its early renderer baseline predates the later 1B.5–1B.14 implementation evidence, so use the newer gate documents above for the current renderer closure state.
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
- [InteriorPortalFullFidelityForegroundOcclusionRegression.md](InteriorPortalFullFidelityForegroundOcclusionRegression.md)

## Aperture, depth and downstream consumers

- [InteriorPortalNearGrazingValidation.md](InteriorPortalNearGrazingValidation.md)
- [InteriorPortalMainDepthContinuityValidation.md](InteriorPortalMainDepthContinuityValidation.md)
- [InteriorPortalSecondaryDepthTransportValidation.md](InteriorPortalSecondaryDepthTransportValidation.md)
- [InteriorPortalMainDepthPropagationValidation.md](InteriorPortalMainDepthPropagationValidation.md)
- [InteriorPortalDOFDepthConsumerValidation.md](InteriorPortalDOFDepthConsumerValidation.md)

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
- [InteriorPortalFullFidelityForegroundOcclusionRegression.md](InteriorPortalFullFidelityForegroundOcclusionRegression.md)
- [InteriorPortalPlayerTraversal.md](InteriorPortalPlayerTraversal.md)

## Folder policy

New documents created specifically for this portal experiment should be added under `docs/InteriorPortalExperiment/` rather than directly under `docs/`.

Keep project-wide documents such as `docs/Architecture.md`, `docs/DevelopmentPhases.md`, `docs/Validation.md`, and `docs/CODEX_GOAL_CHECKPOINT.md` at the `docs/` root.
