# Codex Goal Checkpoint — Phase 7 Relics

Last updated: **2026-09-04**

## Current status

```text
Phase 6UI-A / A3: COMPLETE / VALIDATED / SEALED
7A Relic Runtime: COMPLETE / VALIDATED / SEALED
7B Status + Relic Trigger Sources: COMPLETE / VALIDATED / SEALED
7C Sundial + GainEnergyAction: COMPLETE / VALIDATED / SEALED
7D Relic Read / Frozen / Native UI: COMPLETE / VALIDATED / SEALED
7E Relic Reaction Composition: COMPLETE / VALIDATED / SEALED
7F Relic Counter Metadata Unification: IMPLEMENTATION AUTHORIZED / SOURCE IMPLEMENTED / VALIDATION PENDING
```

## Phase 7F active authority

```text
docs/Phase7FCounterMetadataImplementation.md
```

Phase 7F only removes duplicate authored counter-threshold metadata. It does not reopen sealed Phase 7A–7E Gameplay or UI behavior.

## Implemented on current remote source

```text
URelicCountTrigger : UBattleTrigger
└─ RequiredCount

UDeckShuffledCountTrigger : URelicCountTrigger

URelicData
- CounterDisplayMax removed
- bShowCounter retained as Presentation choice
- TryGetCounterMax derives from the unique URelicCountTrigger
- DataValidation rejects >1 count trigger
- bShowCounter requires one valid positive count threshold

RelicPresentationSnapshot
- frozen CounterMax derives from authoritative RequiredCount
- FBattleHUDRelicView remains a value-only frozen DTO
```

Focused tests added:

```text
SlayTheSpireDemo.Phase7F.CounterMetadata.SingleSource
SlayTheSpireDemo.Phase7F.CounterMetadata.InvalidDefinitions
```

Existing Phase7D RelicPresentation and Phase7E composition fixtures have been migrated off `CounterDisplayMax`.

## Production Sundial state / pending asset save

Before this source change, production `DA_Relic_Sundial` was already saved and validated with:

```text
bShowCounter = true
Triggers[0] = UDeckShuffledCountTrigger
RequiredCount = 3
Effects[0] = UGainEnergyRelicEffect
Amount = 2
```

After the new source builds, the asset must be opened and saved once in Unreal Editor so the removed reflected `CounterDisplayMax` property is no longer carried as stale serialized data. Connected GitHub cannot author the binary `.uasset`.

## Historical Phase 7E evidence

`docs/Phase7EValidation.md` remains trusted sealed evidence for 7E. Historical 7D/7E documents that mention `CounterDisplayMax` describe the pre-7F state and are not rewritten.

## Next exact gate

```text
USER ACTION REQUIRED

1. git pull
2. regenerate UE project files because reflected URelicCountTrigger was added
3. Development Editor Build once
4. STOP and report the Build result
```

If Build passes, next validation order is:

```text
SlayTheSpireDemo.Phase7F
→ open/save DA_Relic_Sundial and confirm CounterDisplayMax is gone, RequiredCount=3 remains
→ SlayTheSpireDemo.Phase7E
→ SlayTheSpireDemo.Phase7.Sundial
→ SlayTheSpireDemo.Phase7.RelicPresentation
→ Sundial PIE smoke
```

Do not run broader unrelated Phase6R / A2D5 / Shipping aggregate gates for this bounded cleanup.
