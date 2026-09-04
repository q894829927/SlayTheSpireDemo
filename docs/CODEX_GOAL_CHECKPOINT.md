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
DESIGN REFINED / DEFERRED / NOT A BLOCKER FOR CARD EXPANSION

Card Expansion / Upgrade Foundation:
NEXT ACTIVE GOAL / DESIGN REFINED / IMPLEMENTATION NOT AUTHORIZED

Card Trigger Source Expansion:
DESIGN DRAFT / FUTURE INDEPENDENT FOUNDATION SLICE / IMPLEMENTATION NOT AUTHORIZED

Ironclad Capability Architecture:
COUPLING REVIEW REFINED / PLANNING ONLY
```

## Ordering decision — Phase 8 is deferred

The user explicitly changed the implementation order on **2026-09-04**.

Phase 8 remains a valid future integration-validation gate, but it is no longer a prerequisite for starting Card Expansion / Upgrade Foundation.

Current order is:

```text
Phase 7A–7F
COMPLETE / VALIDATED / SEALED

→ Card Expansion / Upgrade Foundation
→ first formal upgraded-card batch
→ later bounded Ironclad capability/card waves
→ Phase 8 Combo Architecture Validation may be resumed later as an integration gate
```

Do not delete Phase 8 design or its existing Production PIE evidence. Mark it deferred and preserve it for later reuse.

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

Sealed queue/event ordering precedent remains:

```text
DrawCardsAction
→ queues Draw / Shuffle / RemainingDraw continuation batch at Queue front

ShuffleDeckAction
→ commits Shuffle
→ dispatches DeckShuffled

Dispatcher
→ inserts reaction batch at Queue front

final execution order:
Shuffle commit
→ reactions
→ RemainingDraw continuation
→ previously pending work
```

This remains the precedent to generalize for future authored Result->Continuation.

## Deferred Phase 8 authority

```text
docs/Phase8ComboArchitectureDesign.md
```

Phase 8 design is retained but deferred.

Its future Automation remains decoupled from production Pommel values:

```text
Transient authored UCardData
├─ generic Damage Effect
└─ UDrawCardEffect(DrawCount=2)

→ real PlayCard / Effect / Action path
→ real Shuffle commit
→ real DeckShuffled event
→ Sundial reaction
```

Existing Draw-2 Pommel Strike + Sundial observation remains Production PIE evidence only.

Phase 8 implementation is **not authorized now**, and Phase 8 is **not a blocker** for the next Card Expansion goal.

## Card Expansion / Upgrade Foundation authority

Dedicated design:

```text
docs/CardUpgradeFoundationDesign.md
```

This is now the **next active bounded goal**, but implementation still requires explicit authorization.

Locked Upgrade rules:

```text
Each CardInstance has exactly one authoritative upgrade-state shape.

Normal definition:
→ Single state only
→ bool bUpgraded semantics
→ exactly one upgrade

Repeatable definition:
→ Repeatable state only
→ mutable RepeatCount in CardInstance/dedicated runtime state
→ no independent authoritative bUpgraded

RepeatableUpgradeCapability:
→ immutable definition policy/authored data only
→ does not store mutable RepeatCount
→ does not know Damage / Draw / Block Effect types
→ does not build Presentation text
```

Effective resolution remains typed, not a flattened universal bag:

```text
EffectiveCardView
├─ card-level typed facts
└─ EffectiveEffects[] with typed effect parameters
```

Actual gameplay, A3 preview, committed card-face freeze and Presentation snapshot building must consume the same effective card/effect values.

First Upgrade slice keeps Effect type/order/count unchanged and upgrades typed authored parameters only.

Presentation consumes a frozen `FUpgradeStateView`; normal upgraded title uses `+`, repeatable presentation uses `+1`, `+2`, ...

This does not authorize Run Deck, campfire, save/load, reward or shop systems.

## Ironclad architecture guardrails

Long-term plan:

```text
docs/IroncladCardArchitecturePlan.md
```

Locked architecture rule:

```text
Primitive capabilities stay neutral.
Authored card/orchestration is the composition layer and may combine multiple public contracts.
Capabilities communicate through typed Query / Predicate / Spec / SelectionResult / CommitResult / BattleEvent contracts.
Do NOT replace this with a UniversalResultBus / UniversalContext / arbitrary key-value interpreter.
```

Result-dependent composition uses typed, authored, resolution-local, immutable/stateless Continuations.
Mutable-state-dependent numeric/predicate resolution occurs at Execute-time.

Future same-commit ordering continues to reuse the sealed pattern:

```text
Action Execute
→ commit
→ typed Result
→ authored Continuation builds dependent batch
→ AddBatchToFrontPreserveOrder(ContinuationBatch)
→ dispatch committed BattleEvent
→ reactions naturally insert ahead of ContinuationBatch
→ Finish
```

Result:

```text
same-commit reactions
→ authored Continuation actions
→ previously pending actions
```

Failure path:

```text
Continuation build/insert failure
→ RequestResolutionFault
→ Finish
→ return immediately
→ do NOT dispatch afterward from that failed path
```

## Card Trigger Source Expansion authority

Dedicated future design:

```text
docs/CardTriggerSourceExpansionDesign.md
```

This is an independent future foundation slice before Sentinel/Card-trigger consumers.

Locked comparison key:

```text
Priority
→ SourceTier
   Status / Relic = 0
   Card           = 1
→ SequenceKey
   Status / Relic = RuntimeSequence
   Card           = RuntimeId
→ LocalTriggerIndex
```

Source discovery must come through a typed Card trigger-source provider boundary; Dispatcher must not traverse Deck zones and interpret Card semantics.

Every newly materialized `UCardInstance` receives a fresh battle-unique RuntimeId from the same authoritative allocator; clone/copy never copies RuntimeId.

## Known sealed coupling guardrails

```text
UCardEffect compile-time A3 Preview coupling
FCardPlayContext service-bag tendency
FTriggerContext service-bag tendency
PlayCardAction gameplay + card-face freeze + presentation snapshot duties
```

Do not enlarge these surfaces during Card Expansion.

## Next exact action

```text
1. Phase 8 remains DEFERRED; do not implement it now.
2. Card Expansion / Upgrade Foundation is the next active bounded goal.
3. Before writing production card code, explicitly authorize the first Upgrade Foundation implementation slice.
4. Implement/validate the Upgrade Foundation with the first formal upgraded-card batch.
5. Continue later Ironclad capabilities as separately bounded slices.
6. Resume Phase 8 later when an integration-validation gate is useful.
```
