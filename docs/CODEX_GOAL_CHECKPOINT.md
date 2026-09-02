# Codex Goal Checkpoint — Phase 6UI-A3

Last updated: **2026-09-02**

## Goal

Complete Phase 6UI-A3 under `docs/Phase6UIA3Implementation.md` without changing Gameplay authority, A2 committed-presentation semantics, Native/Legacy ownership, or phase ordering.

The locked A3 distinction is:

```text
A3 = pre-commit read-only current-state supported Operation values
A2 = post-commit playback of immutable facts that actually committed
```

## Current execution status

```text
UI-A3: IN PROGRESS / AUTHORIZED
A3-1 Dynamic Text: COMPLETE / VALIDATED / SEALED
A3-2 Target-Specific Current-State Preview: IN PROGRESS
A3-2A Immediate Preview DTO + Effect contribution: COMPLETE / VALIDATED / SEALED
A3-2B BattleManager Query + identity stamping: IMPLEMENTED / VALIDATION PENDING
A3-3 Energy + Target-Aware Legality: NOT STARTED
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
Legacy location: /Game/SlayTheSpireDemo/UI/Out/Legacy/
Production runtime Legacy HUD/Card/Status dependency count: 0
```

Production configuration remains:

```text
Map:
/Game/SlayTheSpireDemo/Maps/L_BattleTest

WidgetClass:
/Game/SlayTheSpireDemo/UI/Widgets/WBP_BattleHUD_Native.WBP_BattleHUD_Native_C
```

## Active authority

Read before A3 implementation:

```text
AGENTS.md
Source/SlayTheSpireDemo/UI/AGENTS.md
docs/Phase6UIA3Implementation.md
docs/Phase6UIA3DynamicTextImplementation.md
docs/ValidationExecutionPolicy.md
```

`docs/Phase6UIA2EImplementation.md` remains sealed A2 history and the original A3 follow-up design basis. `docs/Phase6UIA3Implementation.md` is the dedicated implementation/ordering/acceptance authority for active A3 work.

## A3-1 sealed predecessor

A3-1 already established read-only Dynamic Text:

```text
CardEffect / Status Modifier
-> named deterministic value
-> DamageSpec / BlockSpec + existing Pipeline where applicable
-> current description at BattleId + StateRevision
-> ViewModel / Widget
```

Do not reopen A3-1 merely to implement target-specific Preview.

The semantic distinction remains:

```text
Card-face Dynamic Text
= current source-side/self presentation value

Target-Specific Current-State Preview
= supported Operation value for one concrete current Target
  at one current BattleId/StateRevision
```

## A3-2A sealed implementation

Implementation commit:

```text
08f878e9f4f74b438985d187884c877d613617af
feat(ui-a3): add immediate preview effect contributions
```

The sealed A3-2A contract establishes:

```text
FImmediateCardPreview
FImmediatePreviewOperation
Damage / Block operation type
UCardEffect default no-op read-only contribution hook
UDamageCardEffect target-specific Damage contribution through FDamageModifierPipeline
UGainBlockCardEffect Self Block contribution through FBlockModifierPipeline
fixed multi-hit per-hit ResolvedAmount + authored HitCount semantics
unsupported effects omitted from Operations[]
```

A3-2A validation evidence:

```text
Editor Build: PASS (user-run UE5.8 SlayTheSpireDemoEditor build)
Automation prefix: SlayTheSpireDemo.UIA3.ImmediatePreview
Discovered: 3 tests
Result: 3/3 Success
Automation exit code: 0
Manual PIE: NOT REQUIRED
```

Do not rerun the sealed A3-2A Gate unless a later edit invalidates that contract or its proving tests.

## A3-2B implementation state

Implementation commits:

```text
a796709034427d470881b6a7d01f2701305406b7  feat(ui-a3): expose immediate preview query
57d0cc0db45089cf025310002806f944bf59769f  feat(ui-a3): build immediate preview query
ebf3838b188b9ccf66e40df549d1fa3665fddd6a  test(ui-a3): cover immediate preview query assembly
```

Changed boundary from the sealed A3-2A head:

```text
Source/SlayTheSpireDemo/Battle/BattleManager.h
Source/SlayTheSpireDemo/Battle/BattleManagerUIA3Preview.cpp
Source/SlayTheSpireDemoTests/Private/Phase6UIA3ImmediatePreviewQueryTests.cpp
```

Implemented behavior:

```text
ABattleManager::TryBuildImmediateCardPreview(...)
current BattleId + StateRevision stamping
CardRuntimeId stamping
canonical SourcePresentationId + TargetPresentationId stamping via TryResolveCombatantPresentationId
immutable CardData Effects iteration in definition order
BuildImmediatePreviewOperations(...) only; no BuildActions
unsupported valid effects remain absent while later supported EffectIndex values are preserved
failed/incoherent transport inputs reset OutPreview and return false
A3-3 Validation/Energy fields remain reserved and are not populated here
```

The public Query accepts `const ACombatant*`. The shared A3-1 `FCardEffectPreviewContext` still stores `ACombatant*`, so the implementation uses one narrow `const_cast` adapter when assigning the context Target. The Effect contribution contract remains read-only, and the focused test checks that the Query does not mutate battle/combatant/deck/queue state.

Focused A3-2B Automation prefix:

```text
SlayTheSpireDemo.UIA3.ImmediatePreviewQuery
```

Expected tests:

```text
StampsCurrentIdentityAndKeepsDefinitionOrder
IsDeterministicReadOnlyAndRejectsIncoherentInputs
```

A3-2B still does not touch:

```text
ViewModel
UMG
PreviewTarget lifecycle
Energy / legality evaluation
Trigger / Relic prediction
final HP prediction
HP ghost bars
Status merge prediction
draw / shuffle prediction
multi-enemy architecture
cross-revision retained selection
```

## Validation actually performed for A3-2B

No UE validation has been claimed yet for the new A3-2B code.

```text
Editor Build: NOT RUN for A3-2B
SlayTheSpireDemo.UIA3.ImmediatePreviewQuery Automation: NOT RUN
Manual PIE: NOT REQUIRED for A3-2B
Phase6R / A2D5 / Shipping / broad Scenario suites: intentionally NOT RUN
```

## Next exact action

Run only the closed-scope A3-2B Gate:

```text
1. Editor Build once.
2. Run Automation prefix exactly once:
   SlayTheSpireDemo.UIA3.ImmediatePreviewQuery
3. Confirm exactly the focused A3-2B tests pass and Automation exits successfully.
4. Record Build + Automation evidence.
5. Mark A3-2B COMPLETE / VALIDATED / SEALED.
6. Only then decide whether A3-2 is fully closed or whether the dedicated implementation document requires another A3-2 sub-slice before A3-3.
```

Do not run Phase6R, A2D5, Shipping, broad Scenario A-E, Legacy parity or unrelated historical suites unless a concrete shared-contract failure invalidates them.
