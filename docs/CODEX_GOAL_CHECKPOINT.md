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
DESIGN REFINED / IMPLEMENTATION NOT AUTHORIZED

Ironclad Capability Architecture:
COUPLING REVIEW INCORPORATED / PLANNING ONLY
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

It covers 75 distinct Ironclad card definitions and CAP-00..CAP-20 plus explicit cross-cutting typed contracts.

Locked architecture rule:

```text
Primitive capabilities stay neutral.
Authored card/orchestration is the composition layer and may combine multiple public contracts.
Capabilities communicate through typed Query / Predicate / Spec / SelectionResult / CommitResult / BattleEvent contracts.
Do NOT replace this with a UniversalResultBus / UniversalContext / arbitrary key-value interpreter.
```

Coupling-review decisions now recorded in the plan:

```text
CAP-09 no longer owns Draw restriction.
Draw legality and Draw amount modification are a separate typed Draw rule surface.
Draw modifiers should use deterministic ordering homologous to existing modifier pipelines.

CAP-12 RNG is domain-neutral: ChooseIndex / ChooseOne / Shuffle only.
CAP-20 CardCatalog only returns ordered Definition candidates.
CAP-05 CardCreation materializes the chosen Definition.

Result-dependent composition uses a typed, authored, resolution-local Continuation.
Continuation is NOT BattleEvent / Dispatcher / persistent Trigger registry.
Action Execute -> commit Result -> synchronous Continuation build -> enqueue before Finish -> no queue pump.

CardExhausted is an explicit future committed Gameplay event.
Sentinel requires generic Card Trigger Runtime Source, not an Exhaust/CardId special case.
CardData owns the trigger definition; CardInstance supplies runtime identity; no per-instance Trigger UObject is required.
Card trigger ordering preserves existing Phase7 Status/Relic RuntimeSequence ordering; Card sources sort after existing non-card sources at equal Priority and use stable Card RuntimeId among cards.
Deck setup still does not consume battle RuntimeSequence.

HP mutation does not own consumer counters.
Blood for Blood-style counts live in explicit card runtime state; Status/Power counters live in their own runtime owner.

CAP-11 only supplies target sets; Damage/Status own their commits.
CAP-17 only owns Block clear/retain lifecycle.
CAP-18 only owns exact Damage outcome facts; Fatal/Heal/MaxHP follow-up belongs to authored Continuation.

Card clone snapshot is conditional: introduce FCardCloneSpec only when mutable card state and a real Copy consumer require it.

Shared predicate/query outlet is used by Dropkick / Spot Weakness-style conditional composition; do not create a second conditional system.
```

Known sealed legacy coupling hotspots are recorded but not reopened:

```text
UCardEffect compile-time A3 Preview coupling
FCardPlayContext service-bag tendency
PlayCardAction gameplay + card-face freeze + presentation snapshot duties
```

New card capabilities must not enlarge those surfaces, especially by adding more subsystem services into `FCardPlayContext`.

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

RepeatableUpgradeCapability owns only repeat policy/state (CanUpgrade / ApplyUpgrade / UpgradeCount).
It must not know Damage / Draw / Block Effect types and must not build Presentation text.

Gameplay resolves authored upgrade data into EffectiveCardFacts.
Presentation consumes a frozen FUpgradeStateView / effective DTO.
Normal upgraded title uses only "+"; repeatable presentation uses "+1", "+2", ...

Upgrade remains orthogonal to Damage / Block / Draw / Exhaust / Status / Relic.
Runtime temporary upgrade remains Action-authoritative.
```

This does not yet authorize Run Deck, campfire, save/load, reward or shop systems.

## Next exact action

```text
1. Review revised Phase 8 design.
2. Explicitly authorize Phase 8 implementation if accepted.
3. Complete/validate/seal Phase 8.
4. Then explicitly authorize Card Expansion / Upgrade Foundation as the next bounded goal.
5. Before implementing Exhaust/Sentinel/Battle Trance/Feed/Fiend Fire class mechanics, obey the newly locked Continuation / CardExhausted / Card Trigger ordering / Draw-rule decisions.

Do not start Upgrade or Ironclad capability implementation before Phase 8 seal unless the user explicitly changes this ordering.
```
