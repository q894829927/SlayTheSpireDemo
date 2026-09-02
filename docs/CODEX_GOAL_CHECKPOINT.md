# Codex Goal Checkpoint — Phase 6UI-A3

Last updated: **2026-09-02**

## Goal

Complete Phase 6UI-A3 under `docs/Phase6UIA3Implementation.md` without changing Gameplay authority, A2 committed-presentation semantics, Native/Legacy ownership, or phase ordering.

```text
A3 = pre-commit read-only current-state supported Operation values
A2 = post-commit playback of immutable facts that actually committed
```

## Current execution status

```text
UI-A3: IN PROGRESS / AUTHORIZED
A3-1 Dynamic Text: COMPLETE / VALIDATED / SEALED
A3-2 Target-Specific Current-State Preview: COMPLETE / VALIDATED / SEALED
A3-3 Energy + Target-Aware Legality: COMPLETE / VALIDATED / SEALED
A3-4 ViewModel Transient Preview Lifecycle: IMPLEMENTED / VALIDATION PENDING
A3-5 Minimal Native UMG + A2/A3 Combined PIE: NOT STARTED
```

UI-A2 remains complete/sealed. Native HUD remains the sole active production implementation. Legacy HUD/Card/Status remain retained/deprecated with zero production runtime dependency.

## Active authority

```text
AGENTS.md
Source/SlayTheSpireDemo/UI/AGENTS.md
docs/Phase6UIA3Implementation.md
docs/Phase6UIA3DynamicTextImplementation.md
docs/ValidationExecutionPolicy.md
```

`docs/Phase6UIA3Implementation.md` is the active implementation/ordering/acceptance authority. `docs/Phase6UIA2EImplementation.md` remains sealed A2 history and design background.

## Sealed predecessor evidence

```text
A3-2A Editor Build: PASS
A3-2A Automation SlayTheSpireDemo.UIA3.ImmediatePreview: 3/3 Success, exit 0
A3-2B Editor Build: PASS
A3-2B Automation SlayTheSpireDemo.UIA3.ImmediatePreviewQuery: 2/2 Success, exit 0

A3-3 Editor Build: PASS
A3-3 Automation SlayTheSpireDemo.UIA3.ImmediatePreviewLegality: 2/2 Success, exit 0
A3-3 compatibility rerun SlayTheSpireDemo.UIA3.ImmediatePreviewQuery: 2/2 Success (user-reported)
Manual PIE: NOT REQUIRED for A3-2/A3-3
```

A3-3 implementation commits:

```text
fa7832e07e78a2b2e0b23ce9835b95b95ea25ed2  feat(ui-a3): add preview legality and energy
fca059cd150a8f1d004068e95cbc4573e3438749  test(ui-a3): keep A3-2B gate compatible with pre-target preview
874013d7f62744a514459398e406f650d1aa2720  test(ui-a3): cover preview legality and energy
3c05c7b1967e9864229b59270c84b90ae7c13bde  docs(ui-a3): seal A3-3 focused gate
```

## A3-4 implementation state

Implementation commits:

```text
37e82b306fd47585b191c21c88f0dc4150c78c09  feat(ui-a3): add transient preview viewmodel state
260866bff2e616ab16949345d7d37a77e3d90d18  feat(ui-a3): implement preview target lifecycle
5552e382e279090a2afb7794cd85763d4434dc4a  feat(ui-a3): clear stale preview with viewmodel lifecycle
4f63de522ce1e5ade12f1dd2fd393a31553f8315  test(ui-a3): cover viewmodel preview lifecycle
```

Changed boundary:

```text
Source/SlayTheSpireDemo/UI/BattleHUDViewModel.h
Source/SlayTheSpireDemo/UI/BattleHUDViewModel.cpp
Source/SlayTheSpireDemo/UI/BattleHUDViewModelUIA3Preview.cpp
Source/SlayTheSpireDemoTests/Private/Phase6UIA3ViewModelPreviewLifecycleTests.cpp
```

Implemented semantics:

```text
SetPreviewTargetById(TargetId) explicitly nominates only a current gameplay-provided legal target
ClearPreviewTarget() clears target nomination and ImmediatePreview without cancelling card selection
ViewModel never calculates Effects/Damage/Block/Energy rules; it delegates to BattleManager::TryBuildImmediateCardPreview
Preview DTO must match current live binding BattleId + StateRevision + selected CardRuntimeId + Source/Target PresentationIds
normal Preview build failure fails soft by clearing Preview state
switching selected card clears previous Preview
CancelSelection clears PreviewTarget + ImmediatePreview
accepted authoritative card/end-turn submission clears Preview through selection teardown
loss/refresh of live bindings clears Preview
ApplyPresentationSnapshot clears selection on every BattleId/StateRevision change even when bResetInteraction=false
OnReadStateReady clears selection/legal targets/PreviewTarget/Preview immediately when Gameplay revision changes, including while PresentationController still owns historical display
terminal / PresentationUnavailable paths clear Preview through existing interaction teardown
PreviewTarget state is separate from combatant/status inspection delegates and does not alter card interaction state
```

No UMG/widget rendering was added in A3-4.

Focused A3-4 Automation prefix:

```text
SlayTheSpireDemo.UIA3.ViewModelPreviewLifecycle
```

Expected tests:

```text
TargetNominationAndClearAreTransient
CancelAndAcceptedSubmissionClearPreview
RevisionChangeClearsBeforePresentationCatchUp
```

## Validation actually performed for A3-4

No UE validation is claimed yet for the A3-4 code.

```text
Editor Build: NOT RUN for A3-4
SlayTheSpireDemo.UIA3.ViewModelPreviewLifecycle: NOT RUN
SlayTheSpireDemo.Phase6UIA1.ViewModel directly affected shared-contract regression: NOT RUN
Manual PIE: NOT REQUIRED for A3-4
A2 / Shipping / broad Scenario / Legacy parity: intentionally NOT RUN
```

## Next exact action

Run only the closed-scope A3-4 Gate:

```text
1. Editor Build once.
2. Run focused A3-4 prefix exactly once:
   SlayTheSpireDemo.UIA3.ViewModelPreviewLifecycle
   expected: 3 tests
3. Because UBattleHUDViewModel is a materially changed shared contract, run the directly affected historical prefix once:
   SlayTheSpireDemo.Phase6UIA1.ViewModel
4. Both prefixes must be all Success and Automation must exit 0.
5. Record exact evidence and seal A3-4.
```

Manual PIE is not required for A3-4. Do not run A3-2/A3-3 again, A2 suites, Phase6R, Shipping, broad Scenario suites or Legacy parity unless a concrete failure invalidates them.

Do not start A3-5 until this Gate passes.
