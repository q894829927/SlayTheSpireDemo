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
A3-2A Immediate Preview DTO + Effect contribution: COMPLETE / VALIDATED / SEALED
A3-2B BattleManager Query + identity stamping: COMPLETE / VALIDATED / SEALED
A3-3 Energy + Target-Aware Legality: COMPLETE / VALIDATED / SEALED
A3-4 ViewModel Transient Preview Lifecycle: IN PROGRESS / AUTHORIZED
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

## A3-2 sealed evidence

```text
A3-2A implementation:
08f878e9f4f74b438985d187884c877d613617af  feat(ui-a3): add immediate preview effect contributions

A3-2B implementation:
a796709034427d470881b6a7d01f2701305406b7  feat(ui-a3): expose immediate preview query
57d0cc0db45089cf025310002806f944bf59769f  feat(ui-a3): build immediate preview query
ebf3838b188b9ccf66e40df549d1fa3665fddd6a  test(ui-a3): cover immediate preview query assembly
```

Validation:

```text
A3-2A Editor Build: PASS
A3-2A Automation SlayTheSpireDemo.UIA3.ImmediatePreview: 3/3 Success, exit 0
A3-2B Editor Build: PASS
A3-2B Automation SlayTheSpireDemo.UIA3.ImmediatePreviewQuery: 2/2 Success, exit 0
Manual PIE: NOT REQUIRED
```

## A3-3 sealed implementation

Implementation commits:

```text
fa7832e07e78a2b2e0b23ce9835b95b95ea25ed2  feat(ui-a3): add preview legality and energy
fca059cd150a8f1d004068e95cbc4573e3438749  test(ui-a3): keep A3-2B gate compatible with pre-target preview
874013d7f62744a514459398e406f650d1aa2720  test(ui-a3): cover preview legality and energy
```

Sealed semantics:

```text
Target == nullptr -> QueryCardPlayability(Card), TargetPresentationId=None
Target != nullptr -> QueryPlayCard(Card, Target)
EnergyBefore = current authoritative Energy
EffectiveCost = Card->GetCurrentCost()
EnergyAfter exists only for an allowed current play
NotEnoughEnergy / InvalidTarget remain authoritative Gameplay validation outcomes
RequestPlayCard semantics remain unchanged
```

A3-3 validation completed on **2026-09-02**:

```text
Editor Build: PASS (user-run against current A3-3 code)
Automation SlayTheSpireDemo.UIA3.ImmediatePreviewLegality: 2/2 Success, exit 0
Automation SlayTheSpireDemo.UIA3.ImmediatePreviewQuery compatibility rerun: 2/2 Success (user-reported)
Manual PIE: NOT REQUIRED
Phase6R / A2D5 / Shipping / broad Scenario / Legacy parity: intentionally NOT RUN
```

A3-3 is COMPLETE / VALIDATED / SEALED.

## Next exact action — A3-4

Implement only:

```text
A3-4 — ViewModel Transient Preview Lifecycle
```

Required semantics:

```text
explicit SetPreviewTargetById(TargetId) / ClearPreviewTarget()
ViewModel transiently owns selected Card identity + LegalTargets + PreviewTarget identity + ImmediatePreview
Preview data comes only from BattleManager::TryBuildImmediateCardPreview(...)
Preview valid only for exact current live binding BattleId + StateRevision
BattleId or StateRevision change clears selection, LegalTargets, PreviewTarget and ImmediatePreview
CancelSelection clears PreviewTarget + ImmediatePreview
accepted authoritative card request clears Preview before A2 committed playback
terminal / PresentationUnavailable / lost live binding clears Preview
normal Preview query failure fails soft and never mutates Gameplay
inspection and PreviewTarget lifecycles remain separate
```

Do not touch UMG in A3-4. Do not add Preview rendering yet.

A3-4 Gate after implementation:

```text
1. Editor Build once.
2. Run one focused ViewModel Preview-lifecycle Automation prefix once.
3. Prove selection/target/cancel/accepted-request/revision clear behavior.
4. Prove stale Preview is never retained across StateRevision.
5. Manual PIE: NOT REQUIRED.
```
