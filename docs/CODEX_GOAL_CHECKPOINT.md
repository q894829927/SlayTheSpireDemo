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
A3-2 Target-Specific Current-State Preview: NEXT IMPLEMENTATION SLICE
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

`docs/Phase6UIA2EImplementation.md` remains sealed A2 history and the original A3 follow-up design basis. `docs/Phase6UIA3Implementation.md` is now the dedicated implementation/ordering/acceptance authority for active A3 work.

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

## Next exact action

Implement only:

```text
A3-2A — Immediate Preview DTO + read-only Effect contribution contract
```

Initial edit boundary:

```text
FImmediateCardPreview
FImmediatePreviewOperation
Damage / Block operation type
UCardEffect narrow read-only Preview-operation contribution hook
UDamageCardEffect contribution
UGainBlockCardEffect contribution
focused A3-2A read-only Automation
```

Do not touch ViewModel or UMG in A3-2A.

Do not implement Energy/legality lifecycle yet beyond what is structurally required by the DTO; that belongs to A3-3.

Do not add Trigger/Relic simulation, final HP prediction, HP ghost bars, Status merge prediction, draw/shuffle prediction, multi-enemy architecture or cross-revision retained selection.

## A3-2A validation scope

Validation is closed-scope.

AUTOMATED GATES

```text
1. Editor Build once.
2. Run the smallest A3-2A focused Automation prefix once.
3. Prove supported Damage/Block operation resolution is deterministic and read-only.
4. Prove fixed multi-hit retains per-hit ResolvedAmount + HitCount semantics.
5. Prove no Gameplay mutation / Action enqueue / Event emission / RNG consumption.
```

MANUAL PIE GATES

```text
none required for A3-2A
```

Do not run Phase6R, A2D5, Shipping, broad Scenario A-E, Legacy parity or unrelated historical suites unless a concrete shared-contract failure invalidates them.

After the A3-2A automated Gate passes, record evidence, commit, and STOP before the next A3 slice unless the user explicitly requests continuation.
