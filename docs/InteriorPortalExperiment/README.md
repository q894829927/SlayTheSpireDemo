# Interior Portal Experiment

This folder contains the documentation for the UE 5.8 interior-portal / full-fidelity rendering experiment on branch `portal/full-fidelity-p1`.

## Start here

- [InteriorPortalPhysicsMapNativeScenePHY4.md](InteriorPortalPhysicsMapNativeScenePHY4.md) — current PHY-4 physics boundary: an actual-map native scene check rejects an active contact safely; production ownership/cutover and Core PIE remain open.
- [InteriorPortalLightTransport.md](InteriorPortalLightTransport.md) — **current lighting work**: offline reference and installed-engine Scene UB publication/native TLAS proof. One frozen connection reaches main/additional/recursive views; 10 native PIE cases / 30 view readbacks pass move, reattachment, offscreen, clear/relink and observer restart. Cosmetic-proxy retirement cannot disconnect the logical light path. Production Lumen/shadow/cache integration and the visible color seam remain OPEN; source-engine changes are conditional.
- [InteriorPortalLumenIntegrationLT3.md](InteriorPortalLumenIntegrationLT3.md) — LT-3 的 Lumen 消费者、末段命中/累积光程数据契约、缓存代次和源码引擎接入边界；当前是设计，不是生产受光实现。
- [InteriorPortalSceneLinearHDRBoundary.md](InteriorPortalSceneLinearHDRBoundary.md) — directed receiving views fix back-wall visibility; native and TSR diagnostic producers now export actual pre-tonemap HDR with sealed exposure and shared quality flags. Build and 22/22 Automation pass; Ping-Pong 0/1 back/front/dual/actual-depth-4 static geometry verified. Normal Lumen-lit half-crossing color seam remains OPEN and is tracked in the lighting work above. Earlier simulation-disabled cube acceptance is withdrawn.
- [InteriorPortalReceivingPlaneOwnership.md](InteriorPortalReceivingPlaneOwnership.md) — support depth/identity leases and shared receiving-view mapping restore dual/recursive coverage. Continuous held-motion/traversal acceptance remains open; static coverage does not establish lighting continuity.
- [InteriorPortalFullFidelityProductionRegression.md](InteriorPortalFullFidelityProductionRegression.md) — **production baseline gate**: lifecycle/publication ownership for the FullFidelity production path; the current surface-ownership interruption above separately reopens its visual geometry acceptance. Player lifecycle ownership now enters through a native API instead of console-command dispatch; run the focused FullFidelity/MainViewOwnership/ColorSampleExposure Automation set plus one compact PIE regression before marking the current renderer scope closed.
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
