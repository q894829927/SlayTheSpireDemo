# Codex Goal Checkpoint — Phase 7 / Phase 8 / Card Expansion

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

Phase 8 Combo Architecture Validation:
DESIGN REVISED / REVIEW PENDING / IMPLEMENTATION NOT AUTHORIZED

Card Expansion / Upgrade Foundation:
DESIGN DRAFT / IMPLEMENTATION NOT AUTHORIZED
```

## Sealed Phase 7 state

Phase 7A–7F remain sealed. Do not reopen them without a concrete later failure that invalidates an existing contract.

Production Sundial remains:

```text
bShowCounter = true
UDeckShuffledCountTrigger.RequiredCount = 3
UGainEnergyRelicEffect.Amount = 2
Counter 0 → 1 → 2 → 0
third real counted Shuffle → +2 Energy
```

## Revised Phase 8 authority

```text
docs/Phase8ComboArchitectureDesign.md
```

The existing Pommel Strike has already been configured to `Draw 2` and the user has manually observed the target gameplay chain in PIE:

```text
Pommel Strike
→ Damage
→ Draw 2
→ UDrawCardsAction(2)
→ real Shuffle
→ FDeckShuffledEvent
→ Sundial Counter
→ third counted Shuffle +2 Energy
→ remaining Draw continuation
```

Therefore Phase 8 no longer creates a dedicated `Pommel Strike+` test asset.

Current Phase 8 scope is only:

```text
8A Automated Combo Integration
8B Record existing Production PIE evidence
8C Validation / Seal
```

The automated path must start from real Card / Effect execution, not a manually dispatched `FDeckShuffledEvent`.

Phase 8 still requires explicit implementation authorization.

## Ironclad card planning

Long-term card inventory/capability plan:

```text
docs/IroncladCardArchitecturePlan.md
```

It covers 75 distinct Ironclad card definitions and CAP-00..CAP-20.

Locked architecture rule:

```text
Capabilities are orthogonal and composable.
They may communicate only through stable neutral contracts such as:
Query / Spec / SelectionResult / CommitResult / BattleEvent.

No capability may know concrete Card / Status / Relic consumers.
```

This remains planning reference only.

## Upgrade Foundation authority

Dedicated design draft:

```text
docs/CardUpgradeFoundationDesign.md
```

Decision:

```text
Upgrade System WILL be implemented,
but not inside Phase 8.

It becomes a Card Expansion foundation capability
and is implemented together with the first formal card-development batch.
```

Locked Upgrade rules:

```text
Default card upgrade is single-use and may use bool bUpgraded.
Normal card: false → true; a second upgrade is rejected generically.
Repeated upgrading is NOT part of every card's default model.
Repeated upgrading is an optional capability assigned by card definition.
Searing Blow is a consumer of that capability, not a CardId special case.
Normal upgraded card title uses only "+".
Repeatable upgrade presentation uses "+1", "+2", ...
Upgrade remains orthogonal to Damage / Block / Draw / Exhaust / Status / Relic.
All Gameplay / UI / A2 / A3 consume one effective-card boundary.
Runtime temporary upgrade remains Action-authoritative.
```

This does not yet authorize Run Deck, campfire, save/load, reward or shop systems.

## Next exact action

```text
1. Review revised Phase 8 design.
2. Explicitly authorize Phase 8 implementation if accepted.
3. Complete/validate/seal Phase 8.
4. Then explicitly authorize Card Expansion / Upgrade Foundation as the next bounded goal.

Do not start Upgrade implementation before Phase 8 seal unless the user explicitly changes this ordering.
```
