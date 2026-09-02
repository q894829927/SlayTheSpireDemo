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
A3-4 ViewModel Transient Preview Lifecycle: COMPLETE / VALIDATED / SEALED
A3-5 Minimal Native UMG + A2/A3 Combined PIE: NEXT IMPLEMENTATION SLICE
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

## A3-4 sealed implementation

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

Sealed semantics:

```text
SetPreviewTargetById(TargetId) explicitly nominates only a current gameplay-provided legal target
ClearPreviewTarget() clears target nomination and ImmediatePreview without cancelling card selection
ViewModel never calculates Effects/Damage/Block/Energy rules; it delegates to BattleManager::TryBuildImmediateCardPreview
Preview DTO must match current live binding BattleId + StateRevision + selected CardRuntimeId + Source/Target PresentationIds
normal Preview build failure fails soft by clearing Preview state
switching selected card clears previous Preview
CancelSelection clears PreviewTarget + ImmediatePreview
accepted authoritative card/end-turn submission clears Preview through interaction teardown
loss/refresh of live bindings clears Preview
ApplyPresentationSnapshot clears selection on every BattleId/StateRevision change even when bResetInteraction=false
OnReadStateReady clears selection/legal targets/PreviewTarget/Preview immediately when Gameplay revision changes, including while PresentationController still owns historical display
terminal / PresentationUnavailable paths clear Preview through existing interaction teardown
PreviewTarget state is separate from combatant/status inspection delegates and does not alter card interaction state
```

No UMG/widget rendering was added in A3-4.

## A3-4 validation evidence

```text
Editor Build: PASS (user-reported, latest A3-4 head)
Focused Automation: SlayTheSpireDemo.UIA3.ViewModelPreviewLifecycle
Expected tests: 3
Result: PASS (user-reported)
Manual PIE: NOT REQUIRED
```

Because `UBattleHUDViewModel` is a shared contract, the historical prefix was also executed:

```text
SlayTheSpireDemo.Phase6UIA1.ViewModel
Discovered: 11 tests
Directly affected interaction/revision/terminal cases: PASS
Aggregate process exit: -1 because of exactly 2 stale historical assertions unrelated to the A3-4 diff
```

The two stale historical tests are:

```text
SubscribeThenPullBuildsHUD
CardPresentationFieldsComeFromDefinition
```

They mutate Status/CardData after `FHUDTestFixture` construction has already called `StartBattle()` and frozen the opening Presentation baseline, then expect a late-subscriber ViewModel to see those later mutable changes. That expectation conflicts with the sealed A2 frozen-Presentation boundary. A3-4 did not change combatant Status or card presentation-field copying. Do not weaken frozen Presentation semantics to make these legacy assertions pass. Track them as historical test debt and repair their fixture timing separately if/when that old suite is maintained.

The same aggregate run showed the directly affected A3-4 shared-contract regressions succeeding, including:

```text
ReadyRefreshClearsStaleSelection
ResolutionFaultIsVisibleTerminalState
SelectionUsesLegalTargetsAndCancelIsPresentationOnly
SelfTargetUsesLegalPlayerSelection
NoTargetRequiresConfirmAndLocksUntilReady
TargetRequestLocksUntilReady
EndTurnLocksUntilReady
UnplayableCardSurfacesGameplayReason
```

A3 / A2 broad suites, Phase6R, Shipping and Legacy parity were intentionally not run.

A3-4 is COMPLETE / VALIDATED / SEALED.

## Next exact action — A3-5

Implement only:

```text
A3-5 — Minimal Native UMG + A2/A3 handoff
```

Required scope:

```text
Native HUD only
small dedicated transient Preview surface
PreviewTarget nomination from hover/focus through dedicated Preview events/state, separate from inspection and authoritative target submission
render current supported Operations / Energy / validation from ViewModel ImmediatePreview only
accepted authoritative submission clears A3 Preview immediately before A2 committed Presentation playback owns the visual result
no Legacy fallback/parity path
no Gameplay rule recomputation in Widget code
```

Acceptance must follow `docs/Phase6UIA3Implementation.md`: focused automated widget/lifecycle gate plus the required combined Native PIE handoff check. Do not start Phase 7 until A3-5 is sealed.