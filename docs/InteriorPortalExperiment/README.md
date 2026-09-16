# Interior Portal Experiment

This folder contains the documentation for the UE 5.8 interior-portal / full-fidelity rendering experiment on branch `portal/full-fidelity-p1`.

## Start here

- [InteriorPortalOffscreenPublicationRetirement.md](InteriorPortalOffscreenPublicationRetirement.md) — **current focused regression gate**: a fast visible -> offscreen transition exposed the spiral because game-thread visibility synchronously cleared a publication that an older queued render-thread view still needed. Publications now retire in render-queue order with generation checks; the same change also preserves analytic aperture geometry instead of accidentally rebuilding it as a homography. Local build + fast pan PIE validation is required.
- [InteriorPortalGrazingAndRecursionRootFix.md](InteriorPortalGrazingAndRecursionRootFix.md) — grazing pixel ownership is moved from an ill-conditioned inverse screen homography to analytic world ray/portal-plane intersection; the promoted FullFidelity producer now consumes `RecursionDepth` with independent endpoint x level ViewState/depth/FColorSample and deepest-to-shallowest BeforeDOF composition. Recursion depth 2 has now been observed by the user to render recursively before crossing; keep it in regression coverage.
- [InteriorPortalGrazingForegroundDepthRegression.md](InteriorPortalGrazingForegroundDepthRegression.md) — previous intermediate diagnosis that separated cosmetic Surface depth from the logical traversal plane. Useful evidence, but it did not remove inverse-homography degeneration and is superseded by the root-fix gates above.
- [InteriorPortalMultiVisibleValidation.md](InteriorPortalMultiVisibleValidation.md) — STEP 1B.14D-MV gate: MV-B dual-visible correctness is PASS; the automatic/exclusive FullFidelity lifecycle has been observed in PIE, and the prior 161 MB video-memory over-budget warning was not reproduced with legacy SceneCapture excluded.
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
- [InteriorPortalGrazingAndRecursionRootFix.md](InteriorPortalGrazingAndRecursionRootFix.md)
- [InteriorPortalOffscreenPublicationRetirement.md](InteriorPortalOffscreenPublicationRetirement.md)

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
- [InteriorPortalPlayerTraversal.md](InteriorPortalPlayerTraversal.md)

## Folder policy

New documents created specifically for this portal experiment should be added under `docs/InteriorPortalExperiment/` rather than directly under `docs/`.

Keep project-wide documents such as `docs/Architecture.md`, `docs/DevelopmentPhases.md`, `docs/Validation.md`, and `docs/CODEX_GOAL_CHECKPOINT.md` at the `docs/` root.
