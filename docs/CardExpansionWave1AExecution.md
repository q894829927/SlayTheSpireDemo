# Card Expansion — Wave 1A Execution Record

Date: **2026-09-06**

Status:

```text
IMPLEMENTATION IN PROGRESS
SOURCE IMPLEMENTATION PRESENT
BUILD / AUTOMATION / PRODUCTION ASSET / PIE PENDING
```

Authority:

```text
docs/CardExpansionWave1AExhaustFactSurface.md
```

This execution record does not expand Wave 1A scope.

## Implemented source surface

```text
EBattleEventType::CardExhausted
FCardExhaustedEvent
FBattleEvent::MakeCardExhausted(...)
FBattleEvent::TryGet<FCardExhaustedEvent>()
```

Producer wiring:

```text
ABattleManager::RequestPlayCard
→ UPlayCardAction receives explicit battle-scoped Dispatcher + combatants
→ UPlayCardAction passes them to UFinishCardPlayAction
→ UFinishCardPlayAction validates Exhaust event wiring before Exhaust commit
→ DeckRuntime authoritative destination commit
→ committed CardZoneChanged record when writer is available
→ FCardExhaustedEvent built from held Card + exact FCardZoneMutationResult
→ Dispatcher receives the current Presentation writer
```

Failure semantics implemented:

```text
invalid Exhaust event wiring
→ RequestResolutionFault before Exhaust commit

successful Exhaust commit + Dispatch failure
→ RequestResolutionFault
→ committed Exhaust is not rolled back
```

No `DeckRuntime -> Dispatcher` dependency was added.

## GainEnergy authored adapter

Added:

```text
UGainEnergyCardEffect
```

Contract:

```text
BaseAmount / UpgradedAmount
→ GetEffectiveAmount(bUpgraded)
→ existing UGainEnergyAction
```

Implemented preview/configuration surface:

```text
BuildActions
GetPreviewArgumentNames
BuildPreviewArguments
ValidatePreviewConfiguration
```

`BuildImmediatePreviewOperations` is intentionally not overridden because this effect currently has no target/current-state override semantic.

## Focused Automation source added

```text
SlayTheSpireDemo.CardExpansion.Wave1A.ExhaustFact.CommittedEvent
SlayTheSpireDemo.CardExpansion.Wave1A.ExhaustFact.NonExhaustDestinations
SlayTheSpireDemo.CardExpansion.Wave1A.ExhaustFact.WiringFailureBeforeCommit
SlayTheSpireDemo.CardExpansion.Wave1A.GainEnergyCardEffect
```

Coverage intent:

```text
successful self-exhaust → exactly one CardExhausted
exact runtime Card identity preserved
committed zone facts agree with CardZoneChanged presentation record
Dispatch observes already committed Exhaust state
Discard / Removed → zero CardExhausted
rejected replay → no duplicate CardExhausted
missing event wiring → fault before Exhaust commit
GainEnergy base/upgraded Gameplay amount
GainEnergy base/upgraded preview amount
```

## Validation availability at this source state

No GitHub Actions run was automatically created for the current direct-to-main Wave 1A commits. Therefore this record does **not** claim a Build or Automation PASS from remote CI.

Production `.uasset` files in this repository are tracked through Git LFS. `DA_Card_SeeingRed.uasset` must therefore be authored/saved through Unreal Editor in a workspace with Git LFS available; a text-only GitHub contents edit cannot produce the real production asset object.

## Still pending before seal

No validation pass is claimed by this file yet.

Required:

```text
[ ] SlayTheSpireDemoEditor Win64 Development Build PASS
[ ] focused Wave 1A Automation PASS
[ ] DA_Card_SeeingRed authored as production UCardData asset
[ ] Seeing Red BaseCost=1 / UpgradedCost=0
[ ] Seeing Red GainEnergy BaseAmount=2 / UpgradedAmount=2
[ ] Seeing Red DefaultDestination=Exhaust
[ ] production DataAsset validation PASS
[ ] focused production PIE PASS
[ ] final validation evidence recorded
[ ] Wave 1A status advanced to COMPLETE / VALIDATED / SEALED
```

No Wave 1B/1C/1D, Card Trigger Source Expansion, multi-enemy, reactive Exhaust Power or generic authored Continuation work is authorized by this execution record.
