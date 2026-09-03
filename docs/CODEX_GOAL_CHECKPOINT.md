# Codex Goal Checkpoint — Phase 7 / Phase 8

Last updated: **2026-09-04**

## Current status

```text
Phase 6UI-A / A3: COMPLETE / VALIDATED / SEALED
7A Relic Runtime: COMPLETE / VALIDATED / SEALED
7B Status + Relic Trigger Sources: COMPLETE / VALIDATED / SEALED
7C Sundial + GainEnergyAction: COMPLETE / VALIDATED / SEALED
7D Relic Read / Frozen / Native UI: COMPLETE / VALIDATED / SEALED
7E Relic Reaction Composition: COMPLETE / VALIDATED / SEALED
7F Relic Counter Metadata Unification: COMPLETE / VALIDATED / SEALED
Phase 8 Combo Architecture Validation: DESIGN DRAFTED / REVIEW PENDING / IMPLEMENTATION NOT AUTHORIZED
```

## Final Phase 7 authority / evidence

```text
docs/Phase7RelicsImplementation.md

docs/Phase7AValidation.md
docs/Phase7BValidation.md
docs/Phase7CValidation.md
docs/Phase7DValidation.md
docs/Phase7ERelicCompositionDesign.md
docs/Phase7EImplementation.md
docs/Phase7EValidation.md
docs/Phase7FCounterMetadataImplementation.md
docs/Phase7FValidation.md
```

Phase 7A–7F are sealed and must not be reopened without a concrete Phase 8 failure that invalidates an existing contract.

## Phase 7 final durable state

```text
Relics are first-class immutable-definition + mutable-runtime Gameplay sources.
Status and Relic triggers share deterministic dispatcher ordering.
Sundial is composed from UDeckShuffledCountTrigger + RelicEffects.
GainEnergyAction is reusable and queued.
Relic HUD state flows through read/frozen Presentation DTOs.
CounterDisplayMax no longer exists as authored metadata.
URelicCountTrigger::RequiredCount is the sole authored counter threshold.
```

Production Sundial final state:

```text
bShowCounter = true
Triggers[0] = UDeckShuffledCountTrigger
RequiredCount = 3
Effects[0] = UGainEnergyRelicEffect
Amount = 2
```

## Phase 8 design authority

Design draft:

```text
docs/Phase8ComboArchitectureDesign.md
```

Phase 8 goal:

```text
2 × data-driven Pommel Strike+
+ Sundial

Card
→ Effects
→ DrawCardsAction(2)
→ real Shuffle
→ FDeckShuffledEvent
→ Sundial reaction
→ every third counted shuffle +2 Energy
```

Locked draft direction:

```text
Pommel Strike+ is an immutable UCardData content variant.
No generic Upgrade runtime in Phase 8.
No bUpgraded / UpgradeLevel / UpgradeCardAction / UpgradeDelta.
No Pommel/Sundial identity special case.
Do not redesign sealed Draw / Shuffle semantics.
A3 still does not predict Draw/Shuffle/Relic reactions.
```

Planned Phase 8 slices:

```text
8A Upgraded Pommel Content Variant
8B Automated Combo Integration
8C Production PIE Acceptance
8D Validation / Seal
```

The proposed dedicated Automation prefix is:

```text
SlayTheSpireDemo.Phase8
```

The automated combo must start from real Card / Effect execution rather than manually dispatching `FDeckShuffledEvent` as its primary path.

## Next exact action

```text
REVIEW docs/Phase8ComboArchitectureDesign.md.

Phase 8 implementation is NOT authorized yet.
Do not modify Card / Draw / Shuffle / Relic runtime.
Do not create the production Pommel Strike+ asset yet.
Explicit user authorization is required after design review.
```
