# Codex Goal Checkpoint — Phase 6UI-A3

Last updated: **2026-09-02**

## Goal

Complete Phase 6UI-A3 under `docs/Phase6UIA3Implementation.md` without changing Gameplay authority, A2 committed-presentation semantics, Native/Legacy ownership, or phase ordering.

The locked distinction remains:

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
A3-3 Energy + Target-Aware Legality: NEXT IMPLEMENTATION SLICE
A3-4 ViewModel Transient Preview Lifecycle: NOT STARTED
A3-5 Minimal Native UMG + A2/A3 Combined PIE: NOT STARTED
```

Predecessor state remains closed:

```text
UI-A2: COMPLETE / VALIDATED / SEALED
A2N R0-R13: COMPLETE / VALIDATED
R14-A: COMPLETE / VALIDATED
R14-B: NOT REQUIRED / NOT AUTHORIZED
Native HUD: sole active production implementation
Legacy HUD/Card/Status: retained / deprecated / do not use
Production runtime Legacy HUD/Card/Status dependency count: 0
```

## Active authority

Read before active A3 work:

```text
AGENTS.md
Source/SlayTheSpireDemo/UI/AGENTS.md
docs/Phase6UIA3Implementation.md
docs/Phase6UIA3DynamicTextImplementation.md
docs/ValidationExecutionPolicy.md
```

`docs/Phase6UIA3Implementation.md` remains the dedicated implementation/ordering/acceptance authority. `docs/Phase6UIA2EImplementation.md` remains sealed A2 history and design background.

## A3-2 sealed implementation

A3-2A implementation:

```text
08f878e9f4f74b438985d187884c877d613617af  feat(ui-a3): add immediate preview effect contributions
```

A3-2B implementation:

```text
a796709034427d470881b6a7d01f2701305406b7  feat(ui-a3): expose immediate preview query
57d0cc0db45089cf025310002806f944bf59769f  feat(ui-a3): build immediate preview query
ebf3838b188b9ccf66e40df549d1fa3665fddd6a  test(ui-a3): cover immediate preview query assembly
```

The sealed A3-2 contract establishes:

```text
FImmediateCardPreview / FImmediatePreviewOperation
Damage / Block supported Operations
UCardEffect read-only contribution hook
Damage through current target-specific FDamageModifierPipeline
Self Block through current FBlockModifierPipeline
fixed multi-hit per-hit amount + authored HitCount
unsupported Effects omitted from Operations[]
ABattleManager::TryBuildImmediateCardPreview(...)
current BattleId + StateRevision + CardRuntimeId stamping
canonical SourcePresentationId + concrete TargetPresentationId stamping
immutable CardData Effects iteration in definition order
read-only Query with no BuildActions / enqueue / state mutation
```

## A3-2 validation evidence

A3-2A closed-scope Gate:

```text
Editor Build: PASS
Automation: SlayTheSpireDemo.UIA3.ImmediatePreview
Discovered: 3
Result: 3/3 Success
Exit code: 0
Manual PIE: NOT REQUIRED
```

A3-2B closed-scope Gate completed on **2026-09-02**:

```text
Editor Build: PASS (user-run UE5.8 SlayTheSpireDemoEditor build)
Automation: SlayTheSpireDemo.UIA3.ImmediatePreviewQuery
Discovered: 2
Result: 2/2 Success
Exit code: 0
Manual PIE: NOT REQUIRED
```

Focused A3-2B tests passed:

```text
IsDeterministicReadOnlyAndRejectsIncoherentInputs
StampsCurrentIdentityAndKeepsDefinitionOrder
```

No Phase6R, A2D5, Shipping, broad Scenario or Legacy parity suite was rerun.

## Next exact action — A3-3

Implement only:

```text
A3-3 — Energy + Target-Aware Legality
```

Required semantics:

```text
no concrete Target -> reuse QueryCardPlayability(Card)
concrete Target -> reuse QueryPlayCard(Card, Target)
EnergyBefore = current authoritative Energy
EffectiveCost = Card->GetCurrentCost()
EnergyAfter valid only when the current validation allows the play
insufficient Energy -> Validation=NotEnoughEnergy and EnergyAfter unavailable
normal Gameplay rejection remains DTO validation, not transport/build failure
RequestPlayCard semantics remain unchanged
```

A3-3 intentionally extends the public Preview query so a null Target becomes a coherent pre-target query state rather than `InvalidTarget`/transport failure. Because this changes one A3-2B proving assertion and edits the same query implementation, the next closed-scope Gate must rerun only the invalidated A3-2B prefix plus the new A3-3 prefix. This does not reopen A3-2A.

Still do not touch:

```text
ViewModel
UMG
PreviewTarget lifecycle
Trigger / Relic prediction
final HP prediction
HP ghost bars
Status merge prediction
draw / shuffle prediction
multi-enemy architecture
cross-revision retained selection
```
