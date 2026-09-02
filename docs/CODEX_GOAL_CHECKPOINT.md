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
A3-2B BattleManager Query + identity stamping: NEXT IMPLEMENTATION SLICE
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

Implemented boundary:

```text
Source/SlayTheSpireDemo/Battle/BattleImmediatePreview.h
Source/SlayTheSpireDemo/Cards/Effects/CardEffect.h
Source/SlayTheSpireDemo/Cards/Effects/DamageCardEffect.h
Source/SlayTheSpireDemo/Cards/Effects/DamageCardEffect.cpp
Source/SlayTheSpireDemo/Cards/Effects/GainBlockCardEffect.h
Source/SlayTheSpireDemo/Cards/Effects/GainBlockCardEffect.cpp
Source/SlayTheSpireDemoTests/Private/Phase6UIA3ImmediatePreviewTests.cpp
```

The sealed contract establishes:

```text
FImmediateCardPreview
FImmediatePreviewOperation
Damage / Block operation type
UCardEffect default no-op read-only contribution hook
UDamageCardEffect target-specific Damage contribution through FDamageModifierPipeline
UGainBlockCardEffect Self Block contribution through FBlockModifierPipeline
fixed multi-hit per-hit ResolvedAmount + authored HitCount semantics
unsupported effects omitted from Operations[]
focused Automation prefix: SlayTheSpireDemo.UIA3.ImmediatePreview
```

A3-2A did not touch:

```text
BattleManager public Preview query construction
ViewModel
UMG
Energy / legality evaluation
PreviewTarget lifecycle
Trigger / Relic prediction
final HP prediction
HP ghost bars
Status merge prediction
draw / shuffle prediction
multi-enemy architecture
cross-revision retained selection
```

## A3-2A validation evidence

Closed-scope Gate completed on **2026-09-02** against the A3-2A implementation.

```text
Editor Build: PASS (user-run UE5.8 SlayTheSpireDemoEditor build)
Automation prefix: SlayTheSpireDemo.UIA3.ImmediatePreview
Discovered: 3 tests
Result: 3/3 Success
Automation exit code: 0
Manual PIE: NOT REQUIRED for A3-2A
Phase6R / A2D5 / Shipping / broad Scenario suites: intentionally NOT RUN
```

Focused tests passed:

```text
BlockUsesSelfPipelineAndIgnoresHoveredEnemy
DamageUsesTargetSpecificPipelineAndPreservesHits
SupportedEffectsKeepDefinitionOrderAndUnsupportedEffectsStayAbsent
```

This is sufficient to seal A3-2A under the closed-scope validation policy. Do not rerun this Gate unless a later edit invalidates the A3-2A contract or its proving tests.

## Next exact action

Implement only:

```text
A3-2B — BattleManager public Immediate Preview Query + identity stamping
```

Required boundary:

```text
ABattleManager::TryBuildImmediateCardPreview(...)
current BattleId + StateRevision stamping
CardRuntimeId stamping
SourcePresentationId + TargetPresentationId stamping
iterate immutable CardData Effects in definition order
call BuildImmediatePreviewOperations(...) only
return coherent FImmediateCardPreview without mutation
focused A3-2B Automation
```

Do not add A3-3 Energy/legality behavior yet beyond preserving the DTO fields already reserved for it.

Do not touch ViewModel / UMG / PreviewTarget lifecycle in A3-2B.

Do not run Phase6R, A2D5, Shipping, broad Scenario A-E, Legacy parity or unrelated historical suites unless a concrete shared-contract failure invalidates them.
