# Interior Portal Experiment

This folder contains the documentation for the UE 5.8 interior-portal / full-fidelity rendering experiment on branch `portal/full-fidelity-p1`.

## Start here

- [InteriorPortalVisualParityIsolation.md](InteriorPortalVisualParityIsolation.md) — STEP 1B.14A visual-parity classification; Outcome B accepted.
- [InteriorPortalViewParityDiagnostics.md](InteriorPortalViewParityDiagnostics.md) — STEP 1B.14B main-vs-secondary view/exposure diagnostics.
- [InteriorPortalSecondaryEyeAdaptationValidation.md](InteriorPortalSecondaryEyeAdaptationValidation.md) — current STEP 1B.14C controlled secondary EyeAdaptation A/B.
- [InteriorPortalCurrentExecutionPlan.md](InteriorPortalCurrentExecutionPlan.md) — current execution plan / stage navigation.
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
- [InteriorPortalPlayerTraversal.md](InteriorPortalPlayerTraversal.md)

## Folder policy

New documents created specifically for this portal experiment should be added under `docs/InteriorPortalExperiment/` rather than directly under `docs/`.

Keep project-wide documents such as `docs/Architecture.md`, `docs/DevelopmentPhases.md`, `docs/Validation.md`, and `docs/CODEX_GOAL_CHECKPOINT.md` at the `docs/` root.
