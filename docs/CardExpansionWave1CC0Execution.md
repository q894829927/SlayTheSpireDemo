# Card Expansion — Wave 1C-C0 Execution / Seal Record

Date: **2026-09-08**

Status:

```text
COMPLETE / VALIDATED / SEALED
```

This document is the final execution and acceptance record for **Wave 1C-C0 — Select-Exhaust Generalization**.

It closes the implementation authorized by:

```text
docs/CardExpansionWave1CC0SelectExhaustGeneralization.md
docs/CardExpansionWave1CC0DescriptionCompatibilityAmendment.md
```

The user explicitly confirmed on **2026-09-08** that C0 has passed and authorized recording the slice as complete.

## Final capability

`USelectExhaustHandCardEffect` remains the single reusable Hand Select-Exhaust effect and now supports:

```text
BaseSelectionMode      = Player | Random
BaseSelectionCount     = exactly N
UpgradedSelectionMode  = Player | Random
UpgradedSelectionCount = exactly N
```

Runtime rules remain:

```text
RequiredCount = Min(ConfiguredSelectionCount, CandidateCount)
count 0       = legal no-op
no candidates = legal no-op
Player        = mandatory exact-N RuntimeId selection
Random        = deterministic unique selection without replacement
both          = canonical candidate-order result
              -> UExhaustCardAction x N
```

Each selected card remains an independent exact Hand -> Exhaust commit/event/presentation fact. No bulk Exhaust mutation was introduced.

## Description contract at seal

The Effect has semantic Description argument defaults:

```text
DescriptionArgumentName              = Exhaust
SelectionModeDescriptionArgumentName = ExhaustMode
```

Selection-mode text is localized Chinese:

```text
Random -> 随机消耗
Player -> 消耗
```

Base and Upgraded descriptions resolve from their corresponding authored selection mode/count, so an upgrade may change both Gameplay and visible wording.

Example:

```text
Description = "{ExhaustMode}{Exhaust}张牌。"

Base     Random / 1 -> 随机消耗1张牌。
Upgraded Player / 1 -> 消耗1张牌。
```

Explicit `None` remains available only as an intentional legacy opt-out; it is not the default for newly-created Effect instances.

## Final validation evidence

User-confirmed C0 seal acceptance:

```text
[x] SlayTheSpireDemoEditor Win64 Development Build PASS
[x] SlayTheSpireDemo.CardExpansion.Wave1CC0 focused Automation PASS (20 / 20)
[x] existing sealed Wave 1C regression gate PASS (13 / 13)
[x] Native HUD PIE Player multi-select PASS
[x] Native HUD PIE Random multi-select PASS
[x] Native HUD PIE Burning Pact regression PASS
[x] final user validation / seal confirmation received
```

The focused C0 inventory remains:

```text
SlayTheSpireDemo.CardExpansion.Wave1CC0
20 tests
```

The user-approved PowerShell Automation invocation convention for this project is the UE 5.8 `UnrealEditor-Cmd.exe` form with `Automation RunTest <Filter>;Quit`.

## Seal consequences

Wave 1C-C0 is now closed for normal forward development.

Do not reopen or redesign C0 unless a concrete regression directly implicates the sealed contract.

The previous C0 predecessor gate on Wave 1C-C1 is satisfied. Future Wave 1C-C1 work may proceed from the sealed C0 capability without adding True Grit-specific Gameplay branches to the C0 primitive.

## Binary scope

This seal record does not itself modify or authorize automatic rewriting of user-owned production binary assets. In particular, it does not modify:

```text
Content/SlayTheSpireDemo/Data/Cards/Ironclad/Skills/DA_Card_BurningPact.uasset
Content/SlayTheSpireDemo/Maps/L_BattleTest.umap
```

Production assets remain authored/saved through Unreal when a later explicit content-authoring step requires it.
