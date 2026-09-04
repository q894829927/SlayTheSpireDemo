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
DESIGN REFINED / REVIEW PENDING / IMPLEMENTATION NOT AUTHORIZED

Card Expansion / Upgrade Foundation:
DESIGN REFINED / IMPLEMENTATION NOT AUTHORIZED

Card Trigger Source Expansion:
DESIGN DRAFT / IMPLEMENTATION NOT AUTHORIZED

Ironclad Capability Architecture:
COUPLING REVIEW REFINED / PLANNING ONLY
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

This is the precedent to generalize for future authored Result->Continuation; do not invent a second queue-ordering model.

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

That remains Production PIE evidence only.

Phase 8 Automation is now decoupled from production Pommel authored values:

```text
Transient authored UCardData
├─ generic Damage Effect
└─ UDrawCardEffect(DrawCount=2)

→ real PlayCard / Effect / Action path
→ real Shuffle commit
→ real DeckShuffled event
→ Sundial reaction
```

No persistent test-only `Pommel Strike+` asset is created. Future Upgrade Foundation may restore production Pommel Strike to Base Draw 1 / Upgraded Draw 2 without invalidating the architecture Automation.

Current Phase 8 scope is only:

```text
8A Automated Combo Integration
8B Record existing Production PIE evidence
8C Validation / Seal
```

Suggested focused tests:

```text
SlayTheSpireDemo.Phase8.Combo.Draw2Sundial
SlayTheSpireDemo.Phase8.Combo.OrderingAndContinuation
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

Coupling-review decisions:

```text
CAP-09 no longer owns Draw restriction.
Draw legality and Draw amount modification are a separate typed Draw rule surface.
Draw modifiers use deterministic ordering homologous to existing modifier pipelines.

CAP-12 RNG is domain-neutral: ChooseIndex / ChooseOne / Shuffle only.
CAP-20 CardCatalog only returns ordered Definition candidates.
CAP-05 CardCreation materializes the chosen Definition.

Result-dependent composition uses a typed, authored, resolution-local Continuation.
Continuation is NOT BattleEvent / Dispatcher / persistent Trigger registry.
Continuation authored objects are immutable/stateless.
Mutable-state-dependent numeric/predicate resolution occurs at Execute-time.
```

### Continuation ordering is not a new queue model

Future CommitResult-dependent actions must reuse the sealed Draw/Shuffle pattern:

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

If Continuation build/insert fails:

```text
RequestResolutionFault
→ Finish
→ return immediately
→ do NOT dispatch the event afterward from that failed path
```

Do not rely on `RequestResolutionFault` alone to make Dispatcher reject work before the Queue reaches its safe fault point.

## Card Trigger Source Expansion authority

Dedicated design:

```text
docs/CardTriggerSourceExpansionDesign.md
```

This is an independent future foundation slice, not part of Sentinel content implementation.

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

RuntimeId invariant:

```text
Every newly materialized UCardInstance
→ fresh battle-unique RuntimeId
→ same authoritative NextRuntimeId allocator
```

Clone/copy never copies RuntimeId.

This slice must protect sealed ordering with focused Card-source Automation plus existing Phase7 Status/Relic and Phase6 trigger-order regressions.

## Known sealed coupling guardrails

Known historical coupling hotspots remain sealed and are not reopened now:

```text
UCardEffect compile-time A3 Preview coupling
FCardPlayContext service-bag tendency
FTriggerContext service-bag tendency
PlayCardAction gameplay + card-face freeze + presentation snapshot duties
```

New card capabilities must not enlarge those surfaces:

```text
Do not add new subsystem services to FCardPlayContext or FTriggerContext.
Do not use FTriggerContext::GetBattle() as a general subsystem locator.
Prefer Event / source snapshot / typed Query boundaries.
```

## Upgrade Foundation authority

Dedicated design:

```text
docs/CardUpgradeFoundationDesign.md
```

Decision:

```text
Upgrade System WILL be implemented,
but not inside Phase 8.

It becomes the Card Expansion Foundation immediately after Phase 8 seal
and is implemented together with the first formal upgraded-card batch.
```

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

Effective resolution is typed, not a flattened universal bag:

```text
EffectiveCardView
├─ card-level typed facts
└─ EffectiveEffects[] with typed effect parameters
```

Actual gameplay, A3 preview, committed card-face freeze and Presentation snapshot building must consume the same effective card/effect values.

First Upgrade slice keeps Effect type/order/count unchanged and upgrades typed authored parameters only.

Presentation consumes a frozen `FUpgradeStateView`; normal upgraded title uses `+`, repeatable presentation uses `+1`, `+2`, ...

This does not yet authorize Run Deck, campfire, save/load, reward or shop systems.

## Planning-order clarification

The long-term Ironclad Waves are implementation/validation groupings, not dependency order.

Actual near-term sequence is:

```text
Phase 8 seal
→ Card Expansion / Upgrade Foundation
→ first normal upgraded-card batch
→ later Ironclad capability waves
→ Armaments / Searing Blow remain later special Upgrade consumers when their card batches arrive
```

Any old wording that places the generic Upgrade Foundation itself at a final "Wave 10" is stale; only special runtime-upgrade consumers may remain in that later wave.

## Next exact action

```text
1. Review the refined Phase 8 design.
2. Explicitly authorize Phase 8 implementation if accepted.
3. Complete / validate / seal Phase 8.
4. Then explicitly authorize Card Expansion / Upgrade Foundation as the next bounded goal.
5. Keep Card Trigger Source Expansion as a separate future foundation slice before Sentinel/Card-trigger consumers.

Do not start Upgrade or Ironclad capability implementation before Phase 8 seal unless the user explicitly changes this ordering.
```
