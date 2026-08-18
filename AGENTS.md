# SlayTheSpireDemo Agent Instructions

## 1. Project Goal

This is an Unreal Engine 5.8 C++ learning/demo project inspired by Slay the Spire combat architecture.

Build a small, extensible and deterministic card-battle framework that supports reusable card effects/actions, deck zones, statuses, relic triggers, modifier pipelines and event-driven interactions without hard-coded combos.

Long-term flow:

```text
CardData / CardInstance
        ↓
CardEffect
        ↓
BattleAction
        ↓
BattleActionQueue
        ↓
Operation Spec
        ↓
Modifier Pipeline
        ↓
Commit
        ↓
BattleEvent
        ↓
Trigger listeners
        ↓
New BattleAction
        ↓
BattleActionQueue
```

`BattleStateMachine` controls turn flow. `BattleActionQueue` controls execution order. Modifier pipelines handle deterministic pre-commit modification. Events/triggers handle post-commit reactions.

---

## 2. Current Repository State

- [x] UE5.8 C++ project and runtime module exist.
- [x] Phase 1 minimal combat loop implemented and PIE-validated.
- [x] Phase 2 `BattleActionQueue` implemented and PIE-validated.
- [x] Phase 3 deck system implemented and PIE-validated.
- [x] Phase 4 data-driven cards implemented and PIE-validated.
- [x] Phase 5 Modifier-Based Framework / status system implemented and validated.
  - [x] Phase 5A Status Runtime + ApplyStatusAction implemented and PIE-validated.
  - [x] Phase 5B1 Damage Spec + DamageFlatAdd + Strength implemented and PIE-validated.
  - [x] Phase 5B2 Damage Ratio + Weak + Vulnerable implemented and PIE-validated.
  - [x] Phase 5C Block Spec + Dexterity + Frailty implemented and PIE-validated.
  - [x] Phase 5R regression Automation gate implemented and UE5.8 self-hosted CI validated at 13/13.
- [ ] Phase 6 battle events / triggers in progress.
  - [x] Phase 6A TurnEnd Trigger Vertical Slice — COMPLETE; UE5.8 self-hosted CI validated at 23/23.
  - [x] Phase 6B Battle Turn Wiring — COMPLETE; expanded Queue contract suite passed at 12/12, total Phase5 + Phase6A + Phase6B gate passed 48/48, and post-hardening PIE turn-cycle validation passed.
  - [ ] Phase 6C DeckShuffled Event — SOURCE IMPLEMENTED; UE5.8 Editor build + Phase6C 5/5 and total 53/53 Automation validation pending exact confirmation.
  - [ ] Phase 6R Regression Gate + deferred test-module extraction — PENDING until Phase 6C validation is confirmed.
- [ ] Phase 6UI-A playable Battle UI — PLANNED AFTER Phase 6R.
  - [ ] UI-A0 Playable Gameplay Boundary.
  - [ ] UI-A1 Operable Battle HUD.
  - [ ] UI-A2 Basic Committed Presentation.
  - [ ] UI-A3 Deterministic Immediate Preview.
- [ ] Phase 7 relic system — PLANNED AFTER Phase 6UI-A.
- [ ] Phase 8 Pommel Strike+ + Sundial architecture/presentation validation — PLANNED AFTER Phase 7.
- [ ] Phase 6UI-B advanced UX / preview / developer tooling — PLANNED AFTER Phase 8.
- [ ] Presentation Polish — PLANNED AFTER Phase 6UI-B.

### Validation summary

Phase 1 validated HP, Block, Energy, turn flow, enemy attacks, victory/defeat and command rejection after battle end.

Phase 2 validated queued Damage/Block, explicit `Finish()`, deterministic front/back ordering, one final `QueueEmpty`, and enemy-turn progression only after the queue drains.

Phase 3 validated Draw/Hand/Discard/Exhaust, DrawPile end as top, one-card draw actions, deterministic Fisher–Yates shuffle with battle-scoped `FRandomStream`, stable card identity, and queued `Shuffle → RetryDraw` with one final `QueueEmpty`.

Phase 4 validated:

- DataAsset-defined cards and independent `UCardInstance` objects;
- two instances from one `DA_Card_Strike` become `Strike#1` and `Strike#2`;
- stateless reusable card effects;
- Pommel Strike resolving `PlayCardAction → DamageAction → DrawCardAction → FinishCardPlayAction`;
- PlayArea lifecycle and resolved destination cleanup;
- Energy spending/rejection;
- Phase 3 shuffle/retry behavior after migrating from `FDeckCardToken` to `UCardInstance`.

Phase 5A validated:

- `UStatusData`, `UStatusInstance`, `UStatusContainer` and `UApplyStatusAction` compile and run in UE5.8 PIE;
- status application resolves through `BattleActionQueue` rather than direct debug mutation;
- first application creates `Strength#1 Amount=2`;
- reapplication merges into the same `Strength#1`, producing Amount=3 while candidate sequence 2 is intentionally unused;
- Enemy Weak then creates `Weak#3 Amount=2`, proving one battle-wide sequence across combatants and allowing gaps;
- repeated batches preserve runtime identities while increasing Amount;
- restarting PIE resets battle-scoped status state and sequence allocation;
- each status batch produces one final `QueueEmpty`.

Phase 5B1 validated:

- `UDamageAction` creates `FDamageSpec` at Execute-time instead of committing `BaseAmount` directly;
- `UDamageCardEffect` explicitly carries `EDamageKind` and ordinary card damage is `Attack`;
- `UStatusData` owns typed instanced `DamageModifiers` rather than concrete Strength-specific battle logic;
- `UDamageFlatAddModifier` configured on `DA_Status_Strength` resolves from current `StatusInstance.Amount`;
- the Damage pipeline collects current Source/Target status modifiers, filters scope/applicability and uses deterministic `Phase → Priority → RuntimeSequence → LocalModifierIndex` ordering;
- `Strength#1 Amount=2` changes Pommel Strike Attack damage from Base 9 to Resolved 11;
- the same Strength does not modify an `Effect` damage operation: Base 9 remains Resolved 9;
- Pommel Strike still completes `DamageAction → DrawCardAction → FinishCardPlayAction → QueueEmpty`, so modifier resolution does not break the existing card-play chain.

Phase 5B2 validated:

- `UDamageRatioModifier` supports explicit `Numerator` / `Denominator`, `SourceMultiplier` / `TargetMultiplier`, and `PresenceOnly` / `ScaleWithAmount` semantics;
- Weak configured as Source + Attack + SourceMultiplier + 3/4 + PresenceOnly reduces 11 to 8;
- Vulnerable configured as Target + Attack + TargetMultiplier + 3/2 + PresenceOnly increases 8 to 12;
- `Weak#2 Amount=3` and `Vulnerable#1 Amount=2` each apply exactly once (`Applications=1`), proving PresenceOnly does not repeat per Amount;
- status creation order was deliberately `Vulnerable#1 → Weak#2 → Strength#3`, while modifier execution remained `Strength FlatAdd → Weak SourceMultiplier → Vulnerable TargetMultiplier`, proving Phase ordering outranks RuntimeSequence;
- each ratio modifier rounds immediately through integer arithmetic: `11 * 3 / 4 = 8`, then `8 * 3 / 2 = 12`;
- Pommel Strike Attack Base 9 resolves to 12 and commits 12 damage;
- with the same statuses active, Effect damage Base 9 collects zero modifiers and remains Resolved 9.

Phase 5C validated:

- `UGainBlockAction` now creates `FBlockSpec` at Execute-time and resolves through `FBlockModifierPipeline` before committing Block;
- `UStatusData` owns typed instanced `BlockModifiers` separately from `DamageModifiers`;
- Dexterity configured as Target + FlatAdd + Value 1 + ScaleWithAmount changes Base Block 5 to 7 at Amount=2;
- Frailty configured as Target + Multiplier + 3/4 + PresenceOnly changes 7 to 5 at Amount=3 and logs `Applications=1`;
- status creation order was deliberately `Frailty#1 → Dexterity#2`, while Block modifier execution remained `Dexterity FlatAdd → Frailty Multiplier`, proving Phase ordering outranks RuntimeSequence in the Block domain;
- the direct Phase 5C test resolves Base Block 5 to 5 and commits one final 5 Block with one final `QueueEmpty`;
- the real `DA_Card_Defend` path resolves through `PlayCardAction → GainBlockAction → FBlockSpec → BlockModifierPipeline → FinishCardPlayAction`, adding another 5 Block and moving Defend to Discard correctly;
- existing Block=5 therefore becomes Block=10 after playing Defend, confirming the card path and pipeline integrate without breaking card cleanup.

Phase 5R validated:

- focused Unreal Automation Tests run against transient runtime objects and assert resolved gameplay state/results directly rather than parsing expected log text;
- Damage regression coverage includes BaseZeroCanReceiveFlatAdd, Strength ScaleWithAmount, Weak/Vulnerable PresenceOnly, per-modifier integer flooring, Phase-before-RuntimeSequence ordering and Attack-vs-Effect filtering;
- Block regression coverage includes BaseZeroCanReceiveFlatAdd, Dexterity ScaleWithAmount, Frailty PresenceOnly and Phase-before-RuntimeSequence ordering;
- Status regression coverage verifies reapplication preserves runtime identity/RuntimeSequence while merging Amount;
- Queue regression coverage verifies existing front/back insertion semantics deterministically;
- the suite contains 13 Phase 5 tests;
- all 13 tests passed through the Windows `ue58` self-hosted runner.

Phase 6A validated:

- atomic ordered front/back Action batches and transactional validation;
- ResolutionFault safety, finite action budget and no normal QueueEmpty on fault;
- typed `FTurnEndedEvent`, on-demand Status trigger collection and deterministic `Priority → RuntimeSequence → LocalTriggerIndex` ordering;
- eligibility/candidate tracing is distinct from actual Reaction execution ordering;
- exact-instance `ReduceStatusAction` semantics and snapshot eligibility vs live mutation validation;
- Execute-time nested event dispatch resolves depth-first through the Queue rather than recursive gameplay mutation;
- PlayCard and Draw shuffle continuations use atomic batch insertion where they form one logical dependent chain;
- the Phase 6A suite contains 23 tests and passed UE5.8 self-hosted CI at 23/23.

Phase 6B validated:

- `PlayerTurnEnding`, `EnemyTurnEnding` and `ResolutionFaulted` are explicit battle states;
- player turn ending and fixed enemy `[DamageAction, TurnEndedAction]` batches are atomic and state commits happen only after successful insertion;
- `TurnEndedAction` emits events at the correct post-commit point and lethal enemy actions suppress the normal enemy TurnEnded event;
- ResolutionFault transitions BattleManager to `ResolutionFaulted` and stops normal turn progression;
- player TurnEnd reactions finish before enemy actions begin;
- Weak/Vulnerable/Frailty DataAssets decay only on their owner's TurnEnded event while Strength/Dexterity remain unchanged;
- `OnQueueEmpty` is a non-reentrant observable boundary: BattleManager defers macro progression until every listener has observed the current ending state;
- the Queue keeps one pump frame alive across deferred macro continuation instead of nesting another QueueEmpty broadcast;
- the strengthened QueueEmpty regression requires observer-visible `PlayerTurnEnding` followed by `EnemyTurnEnding`;
- Queue contract coverage additionally verifies healthy empty batches remain legal no-op success during observer notification, non-empty insertion is rejected during broadcast, continuation registration is broadcast-scoped and single-owner, fault cancels deferred continuation, and empty/unbound continuations are rejected safely;
- expanded UE5.8 gates passed Phase5 13/13 + Phase6A 23/23 + Phase6B 12/12 = 48/48;
- post-hardening PIE validated `PlayerTurnEnding → QueueEmpty → EnemyTurn → EnemyTurnEnding → QueueEmpty → PlayerTurn` with no ResolutionFault.

Phase 6C source implemented; exact UE5.8 53/53 validation evidence is still pending confirmation in this document:

- `FBattleEvent` now discriminates `FTurnEndedEvent` from the second real payload, `FDeckShuffledEvent`;
- `FDeckShuffledEvent` carries the exact `UDeckRuntime*` whose shuffle committed;
- `ShuffleDeckAction` emits `FDeckShuffledEvent` only after `ShuffleDiscardIntoDrawPile()` succeeds;
- expected shuffle no-ops emit no event and do not require event wiring merely to remain no-ops;
- empty-draw continuation ordering is `Draw → Shuffle commit → DeckShuffled reactions → RetryDraw`;
- event-dispatch dependencies are propagated explicitly without a persistent Trigger Registry or actor search;
- generic `PlayCardAction` does not make event wiring a mandatory dependency for non-draw cards;
- pre-6C `Initialize(Deck)` / PlayCard initializer call shapes remain available;
- no new test-only reflected `UCLASS` was added; existing Phase6A test helpers were reused for execution-order recording;
- `SlayTheSpireDemo.Phase6C` contains 5 regressions;
- `.github/workflows/ue-phase6c-tests.yml` expects Phase5 13 + Phase6A 23 + Phase6B 12 + Phase6C 5 = 53 tests.

### Manual UE assets/configuration

Phase 4:

```text
Content/SlayTheSpireDemo/Data/Cards/Ironclad/Attacks/DA_Card_Strike
Content/SlayTheSpireDemo/Data/Cards/Ironclad/Attacks/DA_Card_PommelStrike
Content/SlayTheSpireDemo/Data/Cards/Ironclad/Skills/DA_Card_Defend
```

Phase 5/6 status assets:

```text
Content/SlayTheSpireDemo/Data/Status/DA_Status_Strength
Content/SlayTheSpireDemo/Data/Status/DA_Status_Weak
Content/SlayTheSpireDemo/Data/Status/DA_Status_Vulnerable
Content/SlayTheSpireDemo/Data/Status/DA_Status_Dexterity
Content/SlayTheSpireDemo/Data/Status/DA_Status_Frailty
```

`DA_Status_Strength` DamageFlatAdd:

```text
Scope                  = Source
Priority               = 0
ApplicableDamageKind   = Attack
Value                   = 1
AmountMode              = ScaleWithAmount
```

`DA_Status_Weak` DamageRatio:

```text
Scope                  = Source
Priority               = 0
ApplicableDamageKind   = Attack
Phase                   = SourceMultiplier
Numerator               = 3
Denominator             = 4
AmountMode              = PresenceOnly
```

`DA_Status_Vulnerable` DamageRatio:

```text
Scope                  = Target
Priority               = 0
ApplicableDamageKind   = Attack
Phase                   = TargetMultiplier
Numerator               = 3
Denominator             = 2
AmountMode              = PresenceOnly
```

`DA_Status_Dexterity` BlockFlatAdd:

```text
Scope                  = Target
Priority               = 0
Value                   = 1
AmountMode              = ScaleWithAmount
```

`DA_Status_Frailty` BlockRatio:

```text
Scope                  = Target
Priority               = 0
Numerator               = 3
Denominator             = 4
AmountMode              = PresenceOnly
```

For the Block domain, `Target` means the recipient of Block.

Phase 6 turn-end trigger configuration:

```text
DA_Status_Weak
DA_Status_Vulnerable
DA_Status_Frailty
└── Triggers[0] = TurnEndStatusDecayTrigger
    Priority = 0
    AmountToRemove = 1
```

Do not add turn-end decay to `DA_Status_Strength` or `DA_Status_Dexterity`.

Phase 6C requires no new `.uasset` / `.umap` configuration.

Current temporary `L_BattleTest` wiring includes:

```text
BeginPlay → StartBattle
Keyboard 1 → TestAttack
Space → EndPlayerTurn
Keyboard B → TestGainBlock
Keyboard Q → TestActionQueueOrder
Keyboard D → TestDrawCard
Keyboard X → TestDiscardCard
Keyboard P → TestPlayFirstCard
Keyboard M → TestApplyPhase5AStatuses
Keyboard N → TestApplyPhase5B1Strength
Keyboard K → TestPhase5B1EffectDamage
Keyboard V → TestApplyPhase5B2DamageStatuses
Keyboard C → TestPhase5CBlockPipeline
```

These bindings are debug-only, not the future card-input architecture.

---

## 3. Development Order

Do not skip ahead unless the user explicitly requests it.

### Phase 1 — Minimal Combat Loop — COMPLETE

Implemented `ACombatant`, `ABattleManager`, HP, Block, Energy, turn state and victory/defeat.

### Phase 2 — BattleActionQueue — COMPLETE

Implemented `UBattleAction`, `UBattleActionQueue`, `UDamageAction`, `UGainBlockAction`.

Durable rules:

- one authoritative action executes at a time;
- ordering and completion are explicit;
- actions may schedule dependencies but never pump/advance the queue;
- dependent batches needed for one logical chain must be queued before the current action finishes.

### Phase 3 — Deck System — COMPLETE

Implemented `UDeckRuntime`, DrawPile, Hand, DiscardPile, ExhaustPile, `UDrawCardAction`, `UDiscardCardAction`, `UShuffleDeckAction`.

Durable rules:

- DeckRuntime owns authoritative pile truth;
- DrawPile end is top;
- one DrawAction means one draw attempt;
- draw does not synchronously shuffle;
- `ShuffleDeckAction` only shuffles;
- battle RNG is initialized once and consumed across shuffles;
- runtime card identity is stable object identity after Phase 4 migration.

### Phase 4 — Data-Driven Cards — COMPLETE

Implemented:

```text
UCardData : UPrimaryDataAsset
UCardInstance
ECardType
ECardTargetType
ECardDestination
UCardEffect
UDamageCardEffect
UGainBlockCardEffect
UDrawCardEffect
UPlayCardAction
UFinishCardPlayAction
```

Established flow:

```text
UCardData
↓
UCardInstance
↓
UPlayCardAction
↓
UCardEffect::BuildActions() const
↓
Damage / Block / Draw Actions
↓
UFinishCardPlayAction
↓
resolved destination
```

Phase 4 durable rules:

- effect subobjects in `UCardData` are immutable shared definitions;
- `BuildActions(...)` is logically const/stateless;
- effects capture base intent, not future-state-dependent resolved values;
- effects may receive neutral `ActionOuter` but must not control the queue;
- `UPlayCardAction` may temporarily use a narrow `ABattleManager` Energy interface; effects must not depend on BattleManager;
- all dependent card-play actions are queued before `PlayCardAction::Finish()`;
- `UCardInstance*` is normal runtime identity; `RuntimeId` is for stable labels/logging/replay support;
- `GetDebugLabel()` uses stable `CardId#RuntimeId`, not localized `DisplayName`;
- card destination resolves at cleanup Execute-time;
- actions fail soft and always `Finish()` on invalid execution preconditions.

### Phase 5 — Modifier-Based Framework and Status System — COMPLETE

Phase 5 was built and validated through these vertical slices:

```text
5A  Status Runtime + ApplyStatusAction                              COMPLETE
5B1 FDamageSpec + DamageFlatAdd + Strength                         COMPLETE
5B2 DamageRatio + Weak + Vulnerable                                COMPLETE
5C  FBlockSpec + BlockFlatAdd + BlockRatio + Dexterity + Frailty   COMPLETE
5R  Phase 5 Automation Regression Gate                             COMPLETE / CI PASSED 13/13
```

Phase 5 acceptance is satisfied:

```text
Phase 5C source compiles
+ Phase 5C UE5.8 PIE validation passes
+ Phase 5 Automation regression tests pass in UE5.8 CI
```

#### Phase 5A — Status Runtime — COMPLETE

Implemented and PIE-validated:

```text
UStatusData
UStatusInstance
UStatusContainer
UApplyStatusAction
```

Ownership:

```text
ACombatant
└── UStatusContainer
      └── UStatusInstance...
```

Responsibilities:

```text
UStatusData      → immutable definition
UStatusInstance  → Definition / Amount / RuntimeSequence / Owner
UStatusContainer → Apply / Find / Remove / enumerate; authoritative merge/create decision
ACombatant       → owns StatusContainer, no concrete Strength/Weak rules
```

Use `Amount`, not `Stacks`, because status values may represent magnitude, duration-like values or charges.

`UApplyStatusAction` carries `AmountToAdd`.

Phase 5 status invariants:

```text
AmountToAdd > 0
active StatusInstance.Amount > 0
application is additive: existing Amount + AmountToAdd
```

Do not use negative `AmountToAdd` for decay/removal and do not add stacking policies such as Replace/Refresh/Max until a real mechanic needs them.

Authoritative application path:

```text
request
↓
UApplyStatusAction
↓
BattleActionQueue
↓
UStatusContainer
```

Debug code must not directly mutate StatusContainer.

##### RuntimeSequence

Every newly created runtime status gets a battle-scoped `uint64 RuntimeSequence` used for deterministic ordering.

Requirements:

```text
unique within battle
monotonically increasing
deterministic
not required to be contiguous
```

Do not use separate Player/Enemy counters.

Phase 5 may temporarily expose a narrow allocator on `ABattleManager`, but the permanent architecture requirement is only that allocation is battle-scoped.

`UStatusContainer` must not discover/depend on BattleManager. It remains authoritative for merge-vs-create.

Conceptual application:

```text
ApplyStatusAction obtains/carries candidate sequence
↓
StatusContainer.ApplyStatus(...)
↓
existing → merge Amount, preserve old sequence
absent   → create instance, use candidate sequence
```

Removing and later recreating a status gets a new sequence. Sequence gaps are valid.

#### Phase 5B1 — Damage Spec + Strength — COMPLETE

Implemented and PIE-validated:

```text
EDamageKind
FDamageSpec
UDamageModifier
UDamageFlatAddModifier
FDamageModifierPipeline
```

`FDamageSpec` carries:

```text
Source
Target
DamageKind
BaseAmount
WorkingAmount
ResolvedAmount
```

Use `ResolvedAmount`, not `FinalAmount`, because Block/HP commit can still transform the resolved incoming damage.

Damage semantics currently remain deliberately small:

```cpp
enum class EDamageKind : uint8
{
    Attack,
    Effect
};
```

`DamageKind` is not hard-bound to `ECardType`.

`UDamageCardEffect` explicitly supplies DamageKind to `UDamageAction`; DamageAction must not infer it from card type.

Execute-time flow:

```text
DamageCardEffect(BaseAmount, DamageKind)
↓
DamageAction(BaseAmount, DamageKind)
↓ Execute
FDamageSpec
↓
collect current modifiers
↓
filter Scope / ApplicableDamageKind
↓
deterministic sort
↓
DamageModifierPipeline
↓
ResolvedAmount
↓
TakeCombatDamage(ResolvedAmount)
```

Damage base-input invariant:

```text
BaseAmount < 0  → invalid DamageAction input; fail soft and Finish()
BaseAmount == 0 → valid operation; enter DamageModifierPipeline
BaseAmount > 0  → valid operation; enter DamageModifierPipeline
```

Do not skip a zero-base Damage operation before modifier resolution. FlatAdd modifiers such as Strength must be able to turn Base 0 into a positive `ResolvedAmount` when their applicability rules allow it.

Strength is data-driven:

```text
Strength
└── DamageFlatAdd
    Scope = Source
    Phase = FlatAdd
    Priority = 0
    Value = +1
    AmountMode = ScaleWithAmount
    ApplicableDamageKind = Attack
```

Validated:

```text
Strength Amount=2
Pommel Strike Attack Base=9
→ FlatAdd +2
→ Resolved=11

same Strength Amount=2
Effect damage Base=9
→ modifier filtered out
→ Resolved=9
```

The same DataAsset modifier definition is shared/immutable; runtime Amount and RuntimeSequence come from `UStatusInstance`.

#### Phase 5B2 — Weak + Vulnerable — COMPLETE

Implemented and PIE-validated:

```text
UDamageRatioModifier
EDamageModifierPhase::SourceMultiplier
EDamageModifierPhase::TargetMultiplier
```

Weak/Vulnerable remain generic DataAsset composition rather than content-specific C++ modifier classes.

Configuration:

```text
Weak
└── DamageRatio
    Scope = Source
    Phase = SourceMultiplier
    Numerator = 3
    Denominator = 4
    AmountMode = PresenceOnly
    ApplicableDamageKind = Attack

Vulnerable
└── DamageRatio
    Scope = Target
    Phase = TargetMultiplier
    Numerator = 3
    Denominator = 2
    AmountMode = PresenceOnly
    ApplicableDamageKind = Attack
```

`PresenceOnly` applies once whenever Amount > 0. It does not repeat the ratio Amount times.

Validated with deliberately reversed runtime creation order:

```text
Vulnerable#1 Amount=2
Weak#2 Amount=3
Strength#3 Amount=2
```

Damage resolution still follows Phase before RuntimeSequence:

```text
Pommel Strike Base 9
→ Strength#3 FlatAdd: 9 → 11
→ Weak#2 SourceMultiplier 3/4: 11 → 8
→ Vulnerable#1 TargetMultiplier 3/2: 8 → 12
→ ResolvedAmount=12
```

Both ratio statuses logged `Applications=1` despite Amount > 1.

The same active statuses do not modify `EDamageKind::Effect`; Effect Base 9 resolves to 9 with zero applicable modifiers.

#### Phase 5C — Block Spec + Dexterity + Frailty — COMPLETE

Implemented and PIE-validated:

```text
FBlockSpec
UBlockModifier
UBlockFlatAddModifier
UBlockRatioModifier
FBlockModifierPipeline
```

Flow:

```text
GainBlockAction(BaseAmount)
↓ Execute
FBlockSpec
↓
collect current Block modifiers
↓
filter Scope
↓
Phase → Priority → RuntimeSequence → LocalModifierIndex
↓
BlockModifierPipeline
↓
ResolvedAmount
↓
GainBlock(ResolvedAmount)
```

Keep the existing `GainBlockAction(Source, Target, BaseAmount)` API. In the Block domain, `Target` is the Block recipient.

Block base-input invariant mirrors Damage:

```text
BaseAmount < 0  → invalid GainBlockAction input; fail soft and Finish()
BaseAmount == 0 → valid operation; enter BlockModifierPipeline
BaseAmount > 0  → valid operation; enter BlockModifierPipeline
```

Dexterity/Frailty are data-driven:

```text
Dexterity
└── BlockFlatAdd
    Scope = Target
    Phase = FlatAdd
    Priority = 0
    Value = +1
    AmountMode = ScaleWithAmount

Frailty
└── BlockRatio
    Scope = Target
    Phase = Multiplier
    Priority = 0
    Numerator = 3
    Denominator = 4
    AmountMode = PresenceOnly
```

Validated with deliberately reversed runtime creation order:

```text
Frailty#1 Amount=3
Dexterity#2 Amount=2
```

Resolution still follows Phase before RuntimeSequence:

```text
Base Block 5
→ Dexterity#2 FlatAdd: 5 → 7
→ Frailty#1 Multiplier 3/4: 7 → 5
→ ResolvedAmount=5
```

Frailty logs `Applications=1` despite Amount=3.

The same rule path is exercised by the real Defend card:

```text
DA_Card_Defend
→ GainBlockCardEffect(Base=5)
→ GainBlockAction
→ FBlockSpec
→ BlockModifierPipeline
→ Resolved=5
→ GainBlock
→ FinishCardPlayAction
→ DiscardPile
```

With 5 Block already present from the direct test, playing Defend increases Block to 10.

CardData, CardEffect, PlayCardAction and DeckRuntime required no architecture changes for Phase 5C.

#### Phase 5R — Automation Regression Gate — COMPLETE

Implemented a focused Unreal Automation suite at:

```text
Source/SlayTheSpireDemo/Tests/Phase5RegressionTests.cpp
```

Tests construct transient runtime objects and validate state/results directly. They do not depend on `L_BattleTest`, manual DataAsset setup or log parsing.

Validated test set:

```text
Damage.BaseZeroCanReceiveFlatAdd
Damage.StrengthScalesWithAmount
Damage.WeakPresenceOnly
Damage.VulnerablePresenceOnly
Damage.RatioFloorsPerModifier
Damage.PhaseBeforeRuntimeSequence
Damage.EffectFiltersAttackModifiers
Block.BaseZeroCanReceiveFlatAdd
Block.DexterityScalesWithAmount
Block.FrailtyPresenceOnly
Block.PhaseBeforeRuntimeSequence
Status.ReapplyPreservesRuntimeSequence
Queue.FrontBackOrdering
```

Automation Tests and PIE have separate roles:

```text
Automation Tests
→ deterministic rules and regression invariants

PIE
→ real UE object assembly, DataAsset configuration and end-to-end runtime wiring
```

The hardened `.github/workflows/ue-phase5-tests.yml` workflow builds `SlayTheSpireDemoEditor` and runs `Automation RunTest SlayTheSpireDemo.Phase5` on the Windows UE5.8 self-hosted runner. The current 13-test gate has passed end-to-end.

#### Phase 5 exclusions

These were intentionally not implemented during Phase 5:

```text
status turn-end decay
BattleEvents / Trigger listeners
Artifact interception pipeline
CardCost / Healing / Energy / Draw modifier pipelines
relics
GameplayTag-based damage taxonomy
KeywordLibrary / CardTextFormatter / RichText parsing / keyword tooltips
keyword color/style presentation
dynamic card-text preview
```

`bCancelled` is not required in Damage/Block specs until a concrete cancellation mechanic exists.

Keyword presentation remains deferred. Phase 5 establishes gameplay semantics for statuses such as Strength/Weak/Vulnerable/Dexterity/Frailty, but it does not make `Keyword` an alias for `UStatusData` or add UI keyword infrastructure merely because these mechanics have player-facing names.

### Phase 6 — Battle Events and Triggers — IN PROGRESS

Phase 6 introduces deterministic post-commit facts and queued reactions without weakening the existing ActionQueue / Modifier / Commit boundaries.

Core responsibility split:

```text
BattleEvent
= immutable-by-contract fact describing something that already committed

Trigger
= read-only rule that decides whether it reacts and builds Reaction Actions

BattleAction
= the only object that performs authoritative gameplay mutation
```

Never implement:

```text
Event → listener → direct gameplay mutation
```

Required flow:

```text
Action Execute
↓
Modifier Pipeline when applicable
↓
Commit
↓
BattleEvent
↓
collect current trigger sources
↓
filter + deterministic sort
↓
Build Reaction Actions
↓
atomic Reaction Batch insertion
↓
Current Action Finish
↓
BattleActionQueue executes reactions
```

#### Phase 6 development order

```text
6A  TurnEnd Trigger Vertical Slice                                 COMPLETE / CI PASSED 23/23
6B  Battle Turn Wiring                                             COMPLETE / CI PASSED 12/12, TOTAL 48/48 + PIE PASSED
6C  DeckShuffled Event                                             SOURCE IMPLEMENTED / UE5.8 53-TEST GATE PENDING CONFIRMATION
6R  Phase 6 Regression Gate + test-module extraction               PENDING AFTER 6C
```

Do not implement Phase 7 relics during Phase 6.

#### Phase 6A — TurnEnd Trigger Vertical Slice — COMPLETE

Implemented as a real vertical slice:

```text
transient Weak Amount=2
↓
FTurnEndedEvent(Player)
↓
TurnEndStatusDecayTrigger
↓
ReduceStatusAction(exact Weak instance, 1)
↓
Weak Amount=1
```

Phase 6A includes:

```text
atomic Queue batch insertion front/back
Queue resolution-fault safety
FTurnEndedEvent as the first event payload
synchronous on-demand Status trigger collection
transient deterministic TriggerCandidate sorting
TurnEndStatusDecayTrigger
exact-instance ReduceStatusAction
queued depth-first nested reactions
actual execution-order Automation coverage
focused Unreal Automation Tests
```

##### Event representation and lifetime

Events are short-lived typed value data, not gameplay UObjects and not persistent registry entries.

Phase 6A introduced `FTurnEndedEvent`; Phase 6C source now adds the second real event, `FDeckShuffledEvent`. Do not predeclare speculative event alternatives.

`FBattleEvent` uses a small type-safe checked representation. Requirements:

```text
UBattleTrigger receives const FBattleEvent&
Trigger accesses payload only through checked TryGet<T>()-style access
Trigger code must not use unchecked static_cast/reinterpret_cast on event payloads
Dispatcher synchronously consumes Event/Context
Dispatcher/Trigger must never cache Event* or Event& for later use
Reaction Actions copy/store every UObject/reference they need for later Execute-time work
```

An Event being `const` does not make the UObjects it points to immutable. Trigger code is read-only by architecture rule: it may inspect state and build actions, but must never call gameplay mutation methods directly.

##### Trigger definitions and on-demand collection

Do not add a persistent Trigger Registry in Phase 6.

`StatusContainer` remains the authoritative runtime membership source. Dispatch performs on-demand collection from current combatant StatusContainers:

```text
Dispatch Event
↓
enumerate current StatusInstances
↓
read StatusData Trigger definitions
↓
build transient candidates
↓
filter
↓
sort
↓
build reactions
↓
discard candidates after Dispatch returns
```

When Phase 7 introduces a real `RelicContainer`, extract the smallest trigger contributor/collector boundary needed to collect both Status and Relic sources. Do not prebuild that generic boundary during Phase 6.

`RuntimeSource` is authoritative for trigger-source metadata. For a Status candidate:

```text
RuntimeSource     = UStatusInstance*
Owner             = RuntimeSource->GetOwner()
RuntimeSequence   = RuntimeSource->GetRuntimeSequence()
Definition        = RuntimeSource->GetDefinition()
TriggerDefinition = Definition->Triggers[LocalTriggerIndex]
```

Do not let callers independently supply `Owner` or `RuntimeSequence` alongside a runtime source. Candidate metadata must be derived from the exact runtime source so inconsistent states such as `Enemy Weak + Player Owner` cannot be constructed.

Candidate `TriggerDefinition` is logically const/shared configuration. `LocalTriggerIndex` is the stable order inside its owning definition.

##### Trigger applicability and deterministic ordering

Phase 6 sorting:

```text
Priority
→ RuntimeSequence
→ LocalTriggerIndex
```

Lower values execute earlier.

`Priority` expresses real gameplay semantic ordering. `RuntimeSequence` is only a deterministic tie-break when gameplay semantics do not otherwise distinguish sources. Do not use arbitrary hidden priorities to patch missing timing semantics.

Do not create `TriggerPhase::Normal`. Add Trigger phases only after a concrete implemented mechanic requires true before/after semantic timing.

Do not use registration order, source enumeration order, UObject addresses, names, localized text, actor discovery order or unordered-container iteration as gameplay order.

For TurnEnd decay, owner applicability belongs in the Trigger rule rather than the collector. Conceptually:

```text
Event.TryGet<FTurnEndedEvent>() succeeds
&& Event.TurnOwner == Context.Owner
```

The collector may enumerate both Player and Enemy sources; `OtherActorsTurnDoesNotDecay` must be preserved by Trigger applicability.

Eligibility trace terminology is intentionally explicit:

```text
FTriggerEligibilityRecord
OutEligibilityTrace
```

It records candidates that passed `CanReact` in deterministic order; it does not claim successful reaction construction/insertion/execution. Actual execution ordering is tested separately. Do not reintroduce the old `FTriggerDispatchRecord` alias.

##### Trigger snapshot semantics

Trigger eligibility is snapshotted at Event dispatch time:

```text
Event commits
↓
collect all currently eligible runtime sources
↓
create candidate snapshot
↓
sort
↓
Build all reactions
↓
insert final Reaction Batch
```

After eligibility is snapshotted, a sibling Reaction removing a source does not retroactively cancel another already-built sibling Reaction.

However every resulting BattleAction still validates live state at its own Execute-time. Therefore:

```text
Trigger eligibility = snapshot semantics
Action mutation      = live validation
```

##### Trigger reaction building and failure semantics

Triggers build actions but never control queue execution.

Trigger code must not call:

```text
AddToFront / AddToBack
AddBatch...
StartProcessing
PumpQueue / ProcessNext
Finish current unrelated Action
```

The Dispatcher owns collection, ordering and final Reaction Batch insertion.

Each Trigger builds into its own temporary batch. Validate that temporary batch before appending it to the final reaction list:

```text
Trigger A valid [A1, A2] → append
Trigger B invalid [B1, Invalid] → discard B batch, log error, continue
Trigger C valid [C1] → append
```

Per-trigger content/build failure is fail-soft so one bad trigger does not prevent unrelated triggers from building.

After all valid Trigger batches are concatenated, perform one final atomic `AddBatchToFrontPreserveOrder(...)` validation/insertion. If this final framework-level insertion fails, request a Queue resolution fault. Do not silently drop the complete Reaction Batch and continue battle resolution.

The current Action must still follow its finish contract after requesting a fault.

##### Reaction placement and nested reactions

Default Phase 6 reaction placement:

```text
Current Action commits
↓
Event
↓
Reaction Batch [R1, R2, ...]
↓
existing Pending Actions
```

Reaction Batch is inserted immediately after the current Action and before pre-existing pending work.

Nested Event reactions use queued depth-first semantics:

```text
Original Action
↓
A1
↓
B1   (reaction generated by A1)
↓
A2
↓
original pending work
```

This is queue-driven depth-first resolution, not recursive gameplay function calls.

If a future event requires tail placement or a different explicit timing boundary, implement that as event-specific semantics rather than accidental queue behavior.

##### Atomic ActionQueue batch APIs

Preserve current individual `AddToFront()` LIFO behavior because Phase 3 deliberately relies on it.

Ordered atomic batch APIs:

```text
AddBatchToFrontPreserveOrder(...)
AddBatchToBackPreserveOrder(...)
```

Both share one batch validator. For a healthy Queue, an empty batch is always a legal no-op success, including during QueueEmpty observer notification. Faulted/fault-requested Queues remain closed to all Add/AddBatch requests.

For every non-empty batch, validate the complete batch before modifying `PendingActions`:

```text
all Actions are valid
all Actions are unfinished
all Action Outer values are this Queue
no duplicate Action pointer inside the batch
no batch Action equals CurrentAction
no batch Action already exists in PendingActions
```

During `OnQueueEmpty` observer notification, direct non-empty Action insertion is rejected so observers cannot create a nested/registration-order-dependent resolution. Macro progression must use the deferred continuation boundary.

A `TSet` may be used only for membership checks; never iterate an unordered set to determine gameplay execution order.

If any validation fails:

```text
insert nothing
leave PendingActions unchanged
return failure
```

Front insertion must preserve:

```text
Existing [X, Y]
Batch    [A, B, C]
Result   [A, B, C, X, Y]
```

Back insertion must preserve:

```text
Existing [X, Y]
Batch    [A, B, C]
Result   [X, Y, A, B, C]
```

Multiple Actions forming one inseparable logical continuation/dependent chain must use an atomic batch. `PlayCardAction` follow-ups and Draw `Shuffle → RetryDraw` are current examples.

##### Queue resolution-fault safety

Phase 6 has a hard safety boundary against infinite or structurally invalid reaction chains.

A finite action budget exists in all build configurations, including Shipping. It is a high framework safety limit, not a gameplay balance rule, and must not be exposed as normal DataAsset/designer configuration.

Automation may have a test-only way to lower the budget so fault behavior can be tested without executing thousands of Actions.

Budget is checked at the Queue safe point **before dequeuing/executing the next Action**:

```text
while next Action could execute:
    if ExecutedCount >= MaxActionsPerResolution:
        enter/request ResolutionFault
        do not dequeue another Action

    dequeue next Action
    ExecutedCount++
    Execute
```

A true completed `QueueEmpty` resets the resolution counter. Reactions and nested reactions are part of the same logical resolution chain and consume the same budget.

When a structural error is discovered while a current Action is still inside `Execute()`, do not clear `CurrentAction` from inside that Action's call stack. Instead:

```text
Queue.RequestResolutionFault(Reason)
Current Action Finish()
Execute returns to Queue
Queue reaches safe point before next dequeue
Queue enters ResolutionFaulted
```

Formal fault entry happens only at a Queue safe point. Fault behavior:

```text
mark faulted exactly once
record reason / executed count / last Action information
clear or isolate PendingActions so they cannot execute
set bIsPumping=false before broadcasting the fault
broadcast OnResolutionFaulted exactly once
never broadcast normal OnQueueEmpty for that faulted resolution
reject all subsequent Add / AddBatch / StartProcessing requests
remain faulted until a new ActionQueue is created for a new battle
```

`IsBusy()` must not report a faulted Queue as an ordinary idle/accepting Queue.

Do not use `checkf` as the normal ResolutionFault mechanism. `ensure` alone is also insufficient because it does not stop the pump. The Queue must enter a real fault state.

##### Exact-instance status decay

Do not implement turn-end decay as negative `ApplyStatusAction`.

`ReduceStatusAction` carries the exact expected runtime `UStatusInstance*` plus the amount to remove.

Authoritative mutation remains in `UStatusContainer`. Conceptually:

```text
ReduceStatus(ExpectedInstance, AmountToRemove)
↓
validate exact instance is still a member of this Container
↓
remaining Amount > 0 → update same instance
remaining Amount <= 0 → remove that exact instance
```

Never reduce by only `(Owner, StatusId)` because an old Trigger reaction must not reduce a newly recreated status instance.

Required scenario:

```text
Weak#3 generates ReduceStatusAction(Weak#3)
↓
Weak#3 removed before reduction executes
↓
new Weak#8 applied
↓
old ReduceStatusAction executes
↓
exact Weak#3 membership validation fails
↓
Weak#8 remains unchanged
```

Runtime identity and RuntimeSequence semantics must remain intact.

##### Phase 6A Automation acceptance — PASSED

Phase 6A currently contains 23 tests. Core coverage includes:

```text
Queue.BatchFrontPreservesOrder
Queue.BatchBackPreservesOrder
Queue.BatchInsertionIsAtomic
Queue.BatchRejectsDuplicateAndAlreadyQueuedActions
Queue.FaultDoesNotBroadcastQueueEmpty
Queue.FaultRejectsFurtherMutation

Trigger.PriorityOrdering
Trigger.RuntimeSequenceOrdering
Trigger.LocalTriggerIndexOrdering
Trigger.CollectionOrderDoesNotMatter
Trigger.ReactionBeforeExistingPending
Trigger.NestedReactionDepthFirst
Trigger.ActualPriorityExecutionOrdering
Trigger.ActualRuntimeSequenceExecutionOrdering
Trigger.ActualLocalTriggerIndexExecutionOrdering
Trigger.ActualCollectionOrderDoesNotMatter
Trigger.NestedReactionDepthFirstExecuteTimeDispatch

Status.TurnEndDecay
Status.TurnEndDecayRemovesExactInstanceAtZero
Status.RemovedAndRecreatedInstanceIsNotReduced
Status.OtherActorsTurnDoesNotDecay
Status.SnapshotEligibilityVsLiveActionValidation

Safety.ResolutionBudgetFaultsInsteadOfLoopingForever
```

All 23 passed through the UE5.8 self-hosted Automation gate.

#### Phase 6B — Battle Turn Wiring — COMPLETE

Phase 6B wires the real battle turn lifecycle.

Explicit ending/fault states:

```text
PlayerTurnEnding
EnemyTurnEnding
ResolutionFaulted
```

Turn state changes that depend on queued work are transactional:

```text
Build required Action / complete turn batch
↓
validate and atomically enqueue successfully
↓
commit the corresponding BattleState transition
↓
StartProcessing
```

Never commit `PlayerTurnEnding`/`EnemyTurn` transition first and then discover that the required sentinel/batch failed to enqueue.

Player end-turn flow:

```text
PlayerTurn
↓
EndPlayerTurn request accepted
↓
build TurnEndedAction(Player)
↓
atomic enqueue succeeds
↓
BattleState = PlayerTurnEnding
↓
StartProcessing
↓
TurnEndedAction verifies ending state and combatants alive
↓
FTurnEndedEvent(Player)
↓
reactions
↓
one true final QueueEmpty
```

Current fixed Enemy behavior:

```text
Build [EnemyDamageAction, TurnEndedAction(Enemy)]
↓
atomic AddBatchToBackPreserveOrder succeeds
↓
BattleState = EnemyTurn
↓
StartProcessing
```

When the Enemy `TurnEndedAction` executes, if combatants are still alive it transitions/ensures `EnemyTurnEnding` before emitting the event. If the preceding Enemy Action was lethal, it does not emit a normal TurnEnded event.

`TurnEndedAction` checks actual combatant death directly; it must not depend on `BattleState == Victory/Defeat`, because battle-result state is finalized at the real QueueEmpty boundary.

Final QueueEmpty macro progression is non-reentrant:

```text
Resolution reaches QueueEmpty
↓
OnQueueEmpty.Broadcast begins
↓
BattleManager observes current ending state and registers one deferred authoritative continuation
↓
all other QueueEmpty listeners observe the same completed boundary state
↓
Broadcast fully returns
↓
deferred continuation starts the next turn / enqueues next batch
↓
existing PumpQueue frame continues the next resolution
```

Do not synchronously call `StartEnemyTurn`, `StartPlayerTurn` or recursively start a new Queue pump from inside the QueueEmpty multicast. Do not repair ordering by changing delegate registration order.

`UBattleActionQueue::DeferUntilAfterQueueEmptyBroadcast(...)` is the narrow boundary for this macro continuation. Registration requires a healthy Queue, an active QueueEmpty broadcast, a bound callable, and no continuation already registered for the same boundary. At most one authoritative continuation may be registered for one QueueEmpty boundary. A fault requested by any observer cancels the deferred continuation before execution.

The observer-visible boundary sequence must be:

```text
first QueueEmpty callback  → BattleState == PlayerTurnEnding
second QueueEmpty callback → BattleState == EnemyTurnEnding
then final macro state      → BattleState == PlayerTurn
```

This observer sequence describes the already-validated Phase 6B implementation. Phase 6UI-A0 later introduces explicit turn-start work and `PlayerTurnStarting`; at that point `PlayerTurn` remains the authoritative gameplay request-eligible state, while UI input release may still be delayed by presentation.

`OnResolutionFaulted` makes BattleManager enter `EBattleState::ResolutionFaulted`, zero/reject player input, and stop turn/victory progression for that resolution. Log fault reason, executed count and last Action so PIE does not appear to freeze silently.

The upfront Enemy batch rule is deliberately narrow. It applies to the current fixed Enemy behavior only. Future Enemy Intent/composite Actions may need Execute-time state to decide follow-ups and must continue to obey the project's Execute-time resolution rule.

When dynamic Enemy continuation becomes concrete, introduce an explicit continuation/barrier insertion mechanism so dynamic follow-ups can be placed before the turn-end continuation. Do not make "all future Enemy actions must be precomputed" or "dynamic actions may never AddToBack" a permanent architecture rule.

Phase 6B Automation contains exactly 12 tests:

```text
Turn.PlayerEndingStateCommitsOnlyAfterEnqueueSuccess
Turn.EnemyBatchInsertionIsAtomic
Turn.LethalEnemyActionSkipsTurnEndedEvent
Turn.OneFinalQueueEmptyPerTurnBoundary
Turn.ResolutionFaultTransitionsBattleState
Turn.TurnEndReactionCompletesBeforeNextTurn
Queue.EmptyBatchIsLegalDuringObserverNotification
Queue.ContinuationOutsideBroadcastRejected
Queue.SecondContinuationRejected
Queue.NonEmptyInsertionRejectedDuringBroadcast
Queue.FaultCancelsDeferredContinuation
Queue.EmptyContinuationRejectedSafely
```

`Turn.OneFinalQueueEmptyPerTurnBoundary` asserts observer-visible boundary state order, not only a count of two QueueEmpty callbacks.

The expanded owner-only UE5.8 workflow passed Phase6B 12/12 while Phase5 13/13 and Phase6A 23/23 also remained green, for 48/48 total.

Post-hardening PIE also passed: one clean `Space` end-turn cycle observed Player `QueueEmpty` while `BattleState == PlayerTurnEnding`, then EnemyTurn began; later Enemy `QueueEmpty` was observed while `BattleState == EnemyTurnEnding`, then PlayerTurn began. No `ResolutionFault` occurred.

#### Phase 6C — DeckShuffled Event — SOURCE IMPLEMENTED / VALIDATION PENDING CONFIRMATION

Phase 6C source adds `FDeckShuffledEvent` as the second real typed event.

`ShuffleDeckAction` emits it only after `DeckRuntime::ShuffleDiscardIntoDrawPile()` actually succeeds. Failed/no-op shuffles emit no event.

Required ordering for the existing empty-draw flow is implemented as:

```text
DrawCardAction
↓
ShuffleDeckAction commits successful shuffle
↓
FDeckShuffledEvent
↓
Shuffle reactions
↓
RetryDrawAction
```

Reaction insertion therefore occurs before the already-pending RetryDraw action.

`FBattleEvent::TryGet<T>()` now distinguishes `FTurnEndedEvent` and `FDeckShuffledEvent`; wrong-payload access returns null.

Phase 6C preserves existing public Draw/Shuffle/Play initializer shapes. Event wiring remains optional for generic card play and ordinary/non-shuffle draws. If a successful shuffle is possible, valid battle-scoped dispatcher/combatant context is required before commit. Expected Deck-level no-ops remain fail-soft without requiring event wiring.

No persistent Trigger Registry, Relic source, or Sundial implementation is added in Phase 6C. Phase 7 should be able to add Sundial as a new trigger source without rewriting DeckRuntime/ShuffleDeckAction event timing.

Phase 6C Automation source contains exactly 5 tests:

```text
Event.TypedPayloadIsolation
Shuffle.SuccessEmitsAfterCommit
Shuffle.EmptyDiscardDoesNotEmit
Shuffle.NonEmptyDrawPileDoesNotEmit
Draw.ShuffleReactionBeforeRetryDraw
```

The strongest test observes actual DrawPile state at queued Action execution time and requires:

```text
shuffle reaction sees DrawPile = 1
RetryDraw consumes the shuffled card
post-retry tail sees DrawPile = 0
one final QueueEmpty for the whole resolution
```

The owner-only `.github/workflows/ue-phase6c-tests.yml` expects:

```text
Phase 5    13/13
Phase 6A   23/23
Phase 6B   12/12
Phase 6C    5/5
Total      53/53
```

Do not mark Phase 6C complete or start Phase 6R until exact 53/53 UE5.8 evidence is confirmed.

#### Phase 6R — Regression Gate

Phase 6 completion requires:

```text
all Phase 6 Automation tests pass
+ all 13 Phase 5 regression tests still pass
+ required UE5.8 PIE turn-flow validation passes
```

Use `Trigger.CollectionOrderDoesNotMatter`, not the obsolete `RegistrationOrderDoesNotMatter`, because Phase 6 has no persistent Trigger Registry.

Phase 6R also owns the deferred engineering cleanup recorded in `docs/Phase6DeferredEngineering.md`: move Automation-only reflected test helpers out of the Runtime module into an Editor/Developer-only test module. Until that cleanup is complete, do not add new test-only `UCLASS` types to the Runtime module.

### Phase 6UI-A — Playable Battle UI — PLANNED AFTER PHASE 6R

Implementation order inside this phase:

```text
UI-A0 Playable Gameplay Boundary
↓
UI-A1 Operable Battle HUD
↓
UI-A2 Basic Committed Presentation
↓
UI-A3 Deterministic Immediate Preview
```

The durable UI/MVVM/Presentation architecture and acceptance criteria are defined in Section 15. Do not begin Phase 7 until Phase 6UI-A is playable unless the user explicitly changes the order.

### Phase 7 — Relics — PLANNED AFTER PHASE 6UI-A

Implement relic listeners through the event/trigger architecture. First validation: Sundial; optional Abacus.

Phase 5 Damage/Block modifier collection is intentionally allowed to read directly from StatusContainers while Status is the only real modifier source. When Phase 7 introduces the first non-Status modifier source, do not disguise a Relic as a Status.

At that point introduce the smallest explicit modifier contributor/collector boundary needed so typed pipelines can collect from multiple source families, conceptually:

```text
Typed Modifier Pipeline
        ↓
Modifier Collector
        ↓
Status contributors
Relic contributors
future concrete contributors only when needed
```

The collection boundary may evolve, but the typed operation specs, modifier applicability rules and deterministic sort semantics should remain stable. Do not introduce a universal modifier context merely to support Relics.

### Phase 8 — Combo Architecture Validation — PLANNED AFTER PHASE 7

Validate two upgraded Pommel Strikes + Sundial without special-case combo code. The interaction must emerge from generic card, draw, shuffle, event, modifier and action rules and should be visually understandable through the playable UI.

### Phase 6UI-B — Advanced UX / Presentation Tooling — PLANNED AFTER PHASE 8

Advanced preview, Keyword/CardText presentation, Developer Overlay, presentation timeline tooling, controller/accessibility work and responsive-layout work belong here unless required earlier for basic playability or diagnosis.

### Presentation Polish — PLANNED AFTER PHASE 6UI-B

Drag/drop, fast-play shortcuts, final hand layout, target arrows, animation refinement, VFX/SFX and speed/skip polish belong here. Presentation remains non-authoritative.

---

## 4. Core Architecture Rules

### 4.1 Deterministic gameplay

Same initial state + input sequence + RNG seed must be reproducible. Correctness must not depend on frame rate, UObject address, unordered iteration, actor discovery order or animation timing.

### 4.2 Cards describe effects; they do not own battle rules

Normal cards are data + reusable effects. Avoid unrelated battle-system logic in individual card implementations.

### 4.3 Gameplay mutations flow through actions

Preferred:

```text
request → BattleAction → BattleActionQueue → typed resolution / owning gameplay object
```

### 4.4 Events notify; actions mutate

Events announce facts. Trigger listeners normally enqueue actions.

### 4.5 Avoid recursive gameplay chains

Prefer queued resolution over deep synchronous draw/shuffle/trigger chains.

### 4.6 Separate definitions from runtime instances

```text
CardData != CardInstance
StatusData != StatusInstance
RelicData != RelicInstance
```

Never store runtime state in shared definitions.

### 4.7 Composition over content-specific inheritance

Prefer reusable effects/modifiers and data composition.

### 4.8 UI is presentation, not authority

UMG may request gameplay and display state but never owns authoritative combat/deck/status state.

### 4.9 Keep dependencies explicit and minimal

Avoid gameplay-time actor searches. Pass/store explicit references or narrow contexts.

### 4.10 No premature GAS migration

Validate the project-specific queue/deck/modifier/trigger model first.

### 4.11 Actions may schedule dependencies but do not drive the queue

Actions may enqueue follow-ups when required but never call `PumpQueue`, `ProcessNext` or equivalent execution advancement.

### 4.12 Shared definition objects are immutable at runtime

Instanced definition subobjects such as CardEffects/Modifiers/Triggers are shared configuration and must be logically const/stateless.

### 4.13 Resolve future-state-dependent results at Execute-time

Enqueue-time captures stable intent/base inputs. Mutable-state-dependent values resolve when the action executes. Snapshot semantics must be explicit and mechanic-specific.

### 4.14 Action validation must fail soft and remain action-specific

Invalid execution dependencies must log when useful, call `Finish()`, and never wedge the queue. Do not impose a universal dead-target rule in the base action.

Framework invariant failures discovered while an Action is executing may request a Queue resolution fault, but the current Action must still honor the safe Finish/return contract so the Queue can enter fault at a safe point.

### 4.15 Card destination is resolved, not hard-coded

Card cleanup resolves destination at Execute-time and delegates authoritative zone movement to DeckRuntime.

### 4.16 Turn-state transitions depending on queued work are transactional

Build and validate the required Action or full turn batch, atomically enqueue it, then commit the associated `BattleState`, then start processing. Never enter a TurnEnding state before the required queued work is successfully present.

### 4.17 Runtime trigger source is identity authority

For Trigger collection, derive Owner, RuntimeSequence and definition metadata from the exact runtime source. Do not let callers independently pair a runtime source with separately supplied identity/order metadata.

### 4.18 QueueEmpty is a non-reentrant observable boundary

`OnQueueEmpty` observers must all see the same completed boundary before authoritative macro progression mutates `BattleState` or starts the next resolution.

BattleManager must defer the one authoritative turn continuation until after the multicast returns. Do not make correctness depend on multicast registration order, and do not recursively run the next PumpQueue from inside the current QueueEmpty broadcast.

### 4.19 DeckShuffled is a post-commit fact

`FDeckShuffledEvent` may be emitted only after a real discard-to-draw shuffle commits successfully. Expected/no-op shuffle attempts emit no event. In an empty-draw continuation, DeckShuffled reactions execute before the already-pending RetryDraw action.

---

## 5. Modifier-Based Framework Architecture

MBF does not replace the action queue.

```text
ActionQueue       → execution timing/order
Modifier Pipeline → pre-commit modification/interception/override/clamp
BattleEvent       → post-commit fact
Trigger           → reaction that may enqueue new Actions
```

### 5.1 Action vs Modifier vs Trigger

Use Action for authoritative operations, Modifier for changing an operation before commit, Trigger for reacting to a completed fact.

Do not scatter concrete status checks through cards or BattleManager.

### 5.2 Deterministic modifier ordering

Do not use one global priority.

```text
Domain
→ Phase
→ Priority
→ RuntimeSequence
→ LocalModifierIndex
```

Within a typed domain:

```text
Phase → Priority → RuntimeSequence → LocalModifierIndex
```

`RuntimeSequence` comes from the runtime source instance. `LocalModifierIndex` is stable definition-internal order.

Do not use `StatusId`, names, localized text, UObject addresses, `TSet` iteration, registration order or actor discovery order as tie-breaks. Renaming content must not change gameplay.

Damage domain phases currently implemented:

```text
FlatAdd
SourceMultiplier
TargetMultiplier
```

Block domain phases currently implemented:

```text
FlatAdd
Multiplier
```

Do not pre-add unused FinalModifier/Override/Clamp phases without a concrete mechanic.

Priority only orders modifiers inside the same Domain + Phase.

### 5.3 Typed modifier pipelines

Prefer typed specs:

```text
FDamageSpec
FBlockSpec
FCardCostSpec
FStatusApplySpec
```

Avoid one universal modifier context.

Lifecycle:

```text
Action Execute
→ create typed spec from base intent
→ collect relevant modifiers
→ filter scope/applicability
→ deterministic sort
→ apply modifiers
→ cancellation/override when supported
→ produce resolved values
→ Commit
→ BattleEvent when the operation has a concrete event contract
→ Trigger Actions
```

Operation semantics and modifier applicability are distinct. `FDamageSpec.DamageKind` describes the operation; `ApplicableDamageKind` describes which operations a modifier accepts.

Use `ResolvedAmount` for pipeline output when commit may still transform the actual gameplay result.

### 5.4 Cancellation and interception

Prefer intercept-before-commit to commit-then-undo. Do not add cancellation fields to every spec speculatively; introduce them when that domain has a concrete cancellation mechanic.

### 5.5 MBF guardrail

Before adding a buff/debuff/relic/rule, determine:

```text
1. Action, Modifier or Trigger?
2. Modifier Domain?
3. Phase?
4. Scope/applicability?
5. Same-phase ordering requirements?
6. Priority?
7. Deterministic RuntimeSequence/LocalModifierIndex?
```

### 5.6 Status runtime semantics

`UStatusData` is immutable definition data. `UStatusInstance` owns runtime `Amount`, `RuntimeSequence`, `Owner` and definition reference.

`Amount` is semantic data, not universally repeated stacks. Modifier definitions decide whether to use:

```text
PresenceOnly
ScaleWithAmount
```

`UStatusContainer` owns authoritative status membership and merge/create decisions.

Reapplication preserves RuntimeSequence; removal followed by recreation gets a new one.

Do not use negative ApplyStatus deltas for lifecycle decay; use exact-instance reduction Actions.

### 5.7 Integer modifier arithmetic

Core ratio modifiers use explicit integer fields:

```text
Numerator
Denominator
```

Requirements:

```text
Numerator >= 0
Denominator > 0
```

Each Ratio Modifier resolves and floors immediately before the next modifier. Do not combine all ratios and round only once unless a future domain explicitly defines that policy.

Use `int64` intermediate arithmetic, then clamp to the supported non-negative `int32` range.

Examples:

```text
Damage: 11 → Weak 3/4 = 8 → Vulnerable 3/2 = 12
Block:   7 → Frailty 3/4 = 5
```

### 5.8 Modifier collection boundary

Current pipelines may collect directly from `StatusContainer` because Status is currently the only implemented runtime modifier source.

This direct collection is an implementation detail, not a permanent statement that every modifier source is a Status.

When the first concrete non-Status source appears, normally Phase 7 Relics, extract the smallest contributor/collector boundary required to extend collection without changing typed spec or modifier-application semantics.

Never model a Relic, Stance, battle rule or another unrelated source as a fake `UStatusInstance` merely to reuse current collection code.

---

## 6. UE5 C++ Conventions

Use normal Unreal prefixes: `A`, `U`, `F`, `E`, `I`, and `b` for booleans.

Target source areas as needed:

```text
Battle/ Combat/ Actions/ Cards/ Deck/ Status/ Modifiers/ Relics/ Events/ Enemy/ UI/ Keywords/ Tests/
```

Do not create empty folders just to reserve future architecture. `Keywords/` is a future presentation-oriented source area and should be created only when keyword/card-text presentation work actually begins. `Tests/` contains focused automation regressions and should remain small and rule-oriented.

Prefer forward declarations and small public headers. UObject runtime ownership must be GC-safe through clear Outer/`UPROPERTY`/`TObjectPtr` references. Do not enable Tick by default.

### 6.1 BattleManager and debug harness boundary

`ABattleManager` may temporarily own battle orchestration, the battle-scoped sequence allocator and PIE debug entry points while the learning/demo framework is still being validated.

Do not split it merely for aesthetic purity.

Debug hooks are not intended to become permanent production battle APIs. Automation Tests cover stabilized deterministic rules. As formal event/input paths grow, stop growing `ABattleManager` as the default place for new rule-test commands and incrementally separate debug harness responsibilities when there is a concrete maintenance benefit.

Do not combine that cleanup with unrelated gameplay feature work unless the separation is required for the feature.

---

## 7. Blueprint and Asset Rules

Prefer C++ for authoritative battle/deck/status/modifier/event logic. Prefer Blueprint/UMG/DataAssets for presentation, assembly and content configuration.

All project-owned assets live under:

```text
Content/SlayTheSpireDemo/
```

Recommended areas:

```text
Maps/
Blueprints/
Data/Cards/
Data/Enemies/
Data/Status/
Data/Relics/
UI/
Art/
Materials/
VFX/
Audio/
Dev/
```

Naming examples:

```text
BP_BattleManager
WBP_Card
DA_Card_Strike
DA_Status_Strength
DA_Relic_Sundial
L_BattleTest
```

### 7.1 Keyword presentation boundary

Keep these concepts distinct:

```text
Status
= a runtime combat state owned by StatusContainer

Modifier
= a rule that changes an operation/spec before commit

Keyword
= a player-facing rules term and presentation identity
```

A Keyword is not a gameplay implementation type and must not be equated with `UStatusData`.

A player-facing keyword may describe mechanics implemented by different systems:

```text
Strength / Weak / Vulnerable / Dexterity / Frailty
→ Status + Modifier

Exhaust
→ Card destination / DeckRuntime / Exhaust-related Action

Innate
→ future opening-hand/deck rule

Ethereal
→ future turn-end Trigger/Action rule

Block / Energy / Draw
→ combat/deck concepts or Actions
```

Therefore the relationship is:

```text
Gameplay mechanic
    ↓
semantic KeywordId
    ↓
keyword presentation metadata
```

Keyword presentation should remain a lightweight metadata layer. A future implementation may use a shared library such as `UKeywordLibrary`/`DA_KeywordLibrary` containing records conceptually similar to:

```text
KeywordId
DisplayName
Description
PresentationStyle
optional Icon
```

Do not create one gameplay UObject subclass per keyword, and do not require one DataAsset per keyword unless a concrete content/tooling need later justifies it.

Presentation style is not gameplay logic. `Buff`, `Debuff`, `CardMechanic` or similar presentation categories may drive UMG colors/icons, but battle code must not depend on UI colors.

#### Card text templates

Do not make localized card rules authoritative by storing already-colored final strings such as:

```text
"Deal 8 damage. Apply <red>2 Vulnerable</red>."
```

Future card descriptions should prefer semantic templates/tokens, conceptually:

```text
Deal {Damage} damage.
Apply {VulnerableAmount} [Keyword:Vulnerable].
```

A future `CardTextTemplate` / `CardTextFormatter` / keyword presentation layer may resolve:

```text
numeric placeholders
KeywordId → localized display name
KeywordId → tooltip description
KeywordId → presentation style
```

The exact parser/storage representation is deferred until card UI work begins.

#### Dynamic card-value preview

If card UI later shows current resolved values, UI must not reimplement combat formulas such as Strength/Weak/Vulnerable checks.

Preferred future model:

```text
Gameplay:
DamageAction / GainBlockAction
→ typed Spec
→ typed ModifierPipeline
→ ResolvedAmount
→ Commit

UI Preview:
CardTextResolver
→ preview typed Spec
→ same read-only typed ModifierPipeline rules
→ ResolvedAmount
→ no Commit / no state mutation
```

Preview and gameplay should share rule resolution wherever practical so the number shown on the card matches the number the operation would resolve from the same state. Preview evaluation must be explicitly read-only and must not consume RNG, enqueue Actions, mutate status state or emit gameplay events.

Text/source agents must never claim to create/edit `.uasset` or `.umap` assets without UE Editor access.

Do not intentionally commit generated/local files:

```text
Binaries/
DerivedDataCache/
Intermediate/
Saved/
.vs/
*.sln
*.slnx
```

---

## 8. Change-Scope Rules for Agents

1. Inspect relevant existing files first.
2. Make the smallest coherent change.
3. Do not refactor unrelated code.
4. Do not rename public APIs/assets without need.
5. Do not silently change plugins, engine association or build settings.
6. Do not add third-party dependencies without approval.
7. Do not add multiplayer architecture unless requested.
8. Do not add map/shop/meta-progression before core combat validation unless requested.
9. Preserve explainability for learning.
10. Do not skip development phases without explicit approval.
11. Do not implement future mechanisms merely because they are documented.
12. Do not introduce `FInstancedStruct`, StructUtils or another representation dependency without a concrete requirement.
13. Do not introduce a universal modifier context or GameplayTag-based damage taxonomy without a concrete implemented need.
14. Do not implement KeywordLibrary, CardTextFormatter, RichText parsing, keyword tooltips/styles or dynamic card-value preview merely because status mechanics have player-facing keyword names.
15. Never model `Keyword = StatusData`; keyword presentation metadata must remain separate from the Status/Action/Modifier/Trigger/DeckRule that implements the gameplay mechanic.
16. Do not introduce a generic modifier-contributor framework while Status is the only real modifier source; introduce the smallest collector boundary when a concrete non-Status source such as a Relic actually arrives.
17. Never implement a Relic or another unrelated modifier source as a fake Status merely to reuse StatusContainer collection.
18. Phase 6 Trigger/Event execution must not depend on multicast delegate registration order, UObject address, actor discovery order, source enumeration order or unordered-container iteration; listener order and reaction placement must be explicit.
19. Do not keep adding permanent rule-test entry points to `ABattleManager` when Automation Tests can cover the same deterministic regression; preserve PIE hooks only where end-to-end editor validation still adds value.
20. Phase 6 must not introduce a persistent Trigger Registry while StatusContainer is the only real trigger-source membership authority; collect Status triggers on demand.
21. Turn state changes occur only after the required turn Action/batch is atomically enqueued successfully.
22. Resolution budget is checked before dequeuing/executing the next Action. A fault broadcasts once, rejects further queue mutation, and never broadcasts a normal `QueueEmpty` for the faulted resolution.
23. RuntimeSource is authoritative for Trigger Owner, RuntimeSequence and exact identity; TriggerCandidate metadata must be derived rather than independently supplied.
24. Event and Trigger Context references are synchronous dispatch-lifetime values and must not be cached for later execution.
25. Trigger definitions are read-only rule builders; they must not directly mutate gameplay state or drive the ActionQueue.
26. Per-trigger reaction-build failure is fail-soft for that trigger batch, but failure of the final framework-level atomic Reaction Batch insertion requests a Queue ResolutionFault.
27. `ReduceStatusAction` must target an exact runtime StatusInstance and must never reduce a replacement instance found only by StatusId.
28. Do not add `TriggerPhase` until a concrete same-event before/after timing mechanic requires it.
29. Current fixed Enemy behavior may use an upfront turn batch ending in `TurnEndedAction`; do not generalize this into a requirement to precompute future dynamic Enemy follow-ups. Add an explicit continuation/barrier mechanism only when a real dynamic Enemy mechanic needs it.
30. ResolutionFault is framework safety, not gameplay balance. Keep a high safety budget in all builds and expose any lower configurable budget only through test-specific code.
31. `OnQueueEmpty` is an observable non-reentrant boundary. BattleManager must defer macro turn progression until all observers return; never repair QueueEmpty ordering by relying on multicast registration order.
32. Until Phase 6R extracts an Editor/Developer-only test module, do not add new test-only reflected `UCLASS` types to the Runtime module.
33. `FDeckShuffledEvent` is emitted only after a successful shuffle commit. Expected/no-op shuffles emit no event, and reactions to a successful shuffle resolve before the already-pending RetryDraw continuation.
34. Phase 6UI-A does not begin before Phase 6R unless the user explicitly changes the order. Phase 7 follows the playable UI-A slice, not Phase 6R directly.
35. `PlayerTurn` is an authoritative gameplay request-eligible state. Presentation may still lock the View after Gameplay enters `PlayerTurn`; animation completion must never be required to commit `BattleState = PlayerTurn`.
36. Before UI-A2 exists, presentation catch-up in UI-A0/UI-A1 is an immediate/no-op boundary followed by coherent authoritative snapshot refresh; UI-A0 must not depend on Presentation Records/Presentation Queue.

Prefer clear architecture over clever abstractions.

---

## 9. Build and Verification Rules

After C++ changes:

- verify includes/module dependencies;
- build `SlayTheSpireDemoEditor` when a build environment is available;
- report build errors instead of masking them;
- never claim successful UE build/PIE without actually running it;
- if source tooling cannot run UE, require user-side compile/PIE before marking new source changes validated.

For deterministic core rules, prefer focused Unreal Automation Tests once the rule has stabilized. Tests should validate state/results directly where practical instead of relying only on expected log text.

Current trusted self-hosted regression evidence before exact Phase 6C 53/53 confirmation in this document:

```text
Phase 5   13/13 PASS
Phase 6A  23/23 PASS
Phase 6B  12/12 PASS
Total     48/48 PASS
```

The post-hardening PIE player → enemy → player cycle also passed with no `ResolutionFault` and with observer-visible QueueEmpty states remaining `PlayerTurnEnding` then `EnemyTurnEnding` before macro progression.

Phase 6C source is implemented. Its owner-only workflow is `.github/workflows/ue-phase6c-tests.yml` and must remain manual, owner-only and restricted to trusted `main`.

Required confirmation gate:

```text
Phase 5    13/13
Phase 6A   23/23
Phase 6B   12/12
Phase 6C    5/5
Total      53/53
```

Phase 6R must not start until exact 53/53 evidence is confirmed. Phase 6R must later rerun the complete Phase 5 and Phase 6 suites and perform the deferred test-module extraction/package check recorded in `docs/Phase6DeferredEngineering.md`.

When UE Editor work is required, label it `USER ACTION REQUIRED` and give exact steps.

---

## 10. User-Action Boundary

User performs UE Editor work that text-only tools cannot safely represent, including creating/configuring DataAssets, Blueprints/UMG, level actors/references, visual Blueprint wiring, saving `.uasset`/`.umap`, and PIE validation.

Instructions must include exact path/menu, asset/class, property values, expected result and what logs/screenshots to return on failure.

---

## 11. Implemented Core Classes

```text
Phase 1
Battle/BattleManager.h/.cpp
Combat/Combatant.h/.cpp

Phase 2
Actions/BattleAction.h/.cpp
Actions/BattleActionQueue.h/.cpp
Actions/DamageAction.h/.cpp
Actions/GainBlockAction.h/.cpp

Phase 3
Deck/DeckRuntime.h/.cpp
Actions/DrawCardAction.h/.cpp
Actions/DiscardCardAction.h/.cpp
Actions/ShuffleDeckAction.h/.cpp

Phase 4
Cards/CardTypes.h
Cards/CardData.h/.cpp
Cards/CardInstance.h/.cpp
Cards/CardPlayContext.h
Cards/Effects/CardEffect.h
Cards/Effects/DamageCardEffect.h/.cpp
Cards/Effects/GainBlockCardEffect.h/.cpp
Cards/Effects/DrawCardEffect.h/.cpp
Actions/PlayCardAction.h/.cpp
Actions/FinishCardPlayAction.h/.cpp

Phase 5A
Status/StatusData.h/.cpp
Status/StatusInstance.h/.cpp
Status/StatusContainer.h/.cpp
Actions/ApplyStatusAction.h/.cpp

Phase 5B1
Modifiers/ModifierTypes.h
Modifiers/Damage/DamageSpec.h
Modifiers/Damage/DamageModifier.h/.cpp
Modifiers/Damage/DamageFlatAddModifier.h/.cpp
Modifiers/Damage/DamageModifierPipeline.h/.cpp
Actions/DamageAction.h/.cpp
Cards/Effects/DamageCardEffect.h/.cpp

Phase 5B2
Modifiers/Damage/DamageRatioModifier.h/.cpp
Modifiers/ModifierTypes.h
Battle/BattleManager.h/.cpp

Phase 5C
Modifiers/Block/BlockSpec.h
Modifiers/Block/BlockModifier.h/.cpp
Modifiers/Block/BlockFlatAddModifier.h/.cpp
Modifiers/Block/BlockRatioModifier.h/.cpp
Modifiers/Block/BlockModifierPipeline.h/.cpp
Status/StatusData.h
Actions/GainBlockAction.cpp
Battle/BattleManager.h/.cpp

Phase 5R
Tests/Phase5RegressionTests.cpp
.github/workflows/ue-phase5-tests.yml
.github/workflows/runner-smoke-test.yml

Phase 6A
Events/BattleEvent.h
Events/BattleTrigger.h/.cpp
Events/BattleEventDispatcher.h/.cpp
Events/TurnEndStatusDecayTrigger.h/.cpp
Actions/ReduceStatusAction.h/.cpp
Actions/BattleActionQueue.h/.cpp
Tests/Phase6ARegressionTests.cpp
Tests/Phase6AExecutionOrderTests.cpp
Tests/Phase6ATestTypes.h/.cpp
.github/workflows/ue-phase6a-tests.yml

Phase 6B
Actions/TurnEndedAction.h/.cpp
Battle/BattleManager.h/.cpp
Actions/BattleActionQueue.h/.cpp
Tests/Phase6BRegressionTests.cpp
.github/workflows/ue-phase6b-tests.yml

Phase 6C source
Events/BattleEvent.h
Actions/ShuffleDeckAction.h/.cpp
Actions/DrawCardAction.h/.cpp
Cards/CardPlayContext.h
Cards/Effects/DrawCardEffect.cpp
Actions/PlayCardAction.h/.cpp
Battle/BattleManager.h
Tests/Phase6ATestTypes.h/.cpp
Tests/Phase6CRegressionTests.cpp
.github/workflows/ue-phase6c-tests.yml
docs/Phase6CImplementation.md
```

`ABattleManager` currently owns the battle-scoped ActionQueue, EventDispatcher, DeckRuntime and temporary RuntimeSequence allocator. Each `ACombatant` owns its StatusContainer. BattleManager debug entry points are temporary validation infrastructure, not the intended long-term formal input API.

---

## 12. Acceptance Summary

- Phase 1 — PASSED: minimal battle loop.
- Phase 2 — PASSED: deterministic ActionQueue and queued combat.
- Phase 3 — PASSED: deterministic deck state and queued shuffle/retry.
- Phase 4 — PASSED: data-driven CardData/CardInstance/effect composition and complete card-play queue chain.
- Phase 5A — PASSED: queued status application, authoritative merge/create, Amount semantics and deterministic battle-wide RuntimeSequence behavior.
- Phase 5B1 — PASSED: Execute-time typed damage resolution, data-driven Strength FlatAdd and Attack-vs-Effect applicability filtering.
- Phase 5B2 — PASSED: integer DamageRatio resolution, PresenceOnly semantics, deterministic Phase ordering and Weak/Vulnerable Attack filtering.
- Phase 5C — PASSED: Execute-time typed Block resolution, data-driven Dexterity/Frailty, PresenceOnly semantics, deterministic Phase ordering and real Defend integration.
- Phase 5R — PASSED: 13/13 focused Unreal Automation tests passed UE5.8 self-hosted CI.
- Phase 5 — PASSED: Modifier-Based Framework and Status System complete for the defined Phase 5 scope.
- Phase 6A — PASSED: 23/23 UE5.8 Automation; typed TurnEnded event/trigger vertical slice, exact-instance decay, deterministic candidate and actual execution ordering.
- Phase 6B — PASSED: expanded 12/12 UE5.8 Automation, total Phase5 + Phase6A + Phase6B 48/48, real Status DataAsset PIE validation, QueueEmpty non-reentrancy PIE validation, and all six Queue continuation/broadcast contracts green.
- Phase 6C — SOURCE IMPLEMENTED / VALIDATION PENDING EXACT CONFIRMATION: typed DeckShuffled post-commit event, shuffle reaction-before-retry ordering, 5 Automation tests and a 53-test owner-only UE5.8 workflow are on `main`.
- Phase 6 — NOT YET COMPLETE: exact 6C validation confirmation and 6R remain.
- Phase 6UI-A0 — PLANNED AFTER 6R: authoritative playable turn/hand lifecycle, formal Request APIs, shared gameplay validation, coherent read snapshot and minimal authoritative Enemy Intent.
- Phase 6UI-A1 — PLANNED: first operable Battle HUD without gameplay-driving debug keyboard commands.
- Phase 6UI-A2 — PLANNED: committed Presentation Records and playback separated from `BattleActionQueue`.
- Phase 6UI-A3 — PLANNED: deterministic immediate Damage / Block / Energy preview only.
- Phase 6UI-A — PASSED only when the normal player battle loop is operable through UI without `TestDrawCard`, `TestPlayFirstCard` or equivalent gameplay-driving debug commands.
- Phase 7 — PLANNED AFTER Phase 6UI-A: Relic runtime, Sundial first validation and Relic presentation.
- Phase 8 — PLANNED AFTER Phase 7: Pommel Strike+ / Sundial generic gameplay and presentation-legibility validation.
- Phase 6UI-B — PLANNED AFTER Phase 8: advanced Outcome Preview, Keyword/CardText presentation, developer tooling, accessibility/input expansion and responsive layout.
- Presentation Polish — PLANNED LAST: interaction-speed shortcuts, animations, VFX/SFX and visual refinement; presentation remains non-authoritative.

---

## 13. Architecture Validation Principle

Complex interactions must emerge from generic rules.

Pommel Strike knows only its configured damage/draw effects. Defend knows only its configured Block effect. DeckRuntime knows only card zones/draw/shuffle. Sundial should eventually know only shuffle events. Damage and Block statuses modify typed operation specs through their respective Modifier Pipelines.

Player-facing keywords explain mechanics but do not own those mechanics. A Status keyword such as Vulnerable may map to Status + Modifier, while a card-mechanic keyword such as Exhaust may map to DeckRuntime/Card destination/Action logic. UI presentation must not force unrelated gameplay concepts into one Status hierarchy.

Relics and other future rule sources should contribute through explicit mechanisms appropriate to their domain; they must not be forced into Status purely for implementation convenience.

If a new card/relic/status requires editing many unrelated classes or scattering concrete checks through battle code, stop and reconsider the architecture.

---

## 14. Documentation and Progress Updates

When completing a meaningful phase:

- update Current Repository State;
- record durable architecture invariants;
- record required manual UE assets/configuration;
- keep Current Repository State, Development Order, Acceptance Summary and detailed phase sections synchronized;
- do not rely on a later section to silently override a stale earlier development order;
- keep documentation synchronized with actual source/PIE/Automation state;
- do not fill this file with daily implementation trivia.

---

## 15. Planned Playable UI, MVVM and Presentation Architecture

This section is the detailed long-term specification for work after Phase 6R. Its order must remain synchronized with Sections 2, 3 and 12; it must not be used to override stale contradictory progress text elsewhere in this file.

Do not implement these UI systems before Phase 6R unless the user explicitly changes the development order.

### 15.1 Post-Phase-6 development order

```text
Phase 6R
    test-module extraction
    full Phase 5/6 regression
    Shipping/package-oriented validation
↓
Phase 6UI-A
    UI-A0 playable gameplay boundary
    UI-A1 operable Battle HUD
    UI-A2 basic committed presentation
    UI-A3 deterministic immediate preview
↓
Phase 7
    Relics
    Relic UI / trigger feedback
↓
Phase 8
    Pommel Strike+ / Sundial gameplay + presentation-legibility validation
↓
Phase 6UI-B
    advanced preview
    Keyword / CardText presentation
    Developer Overlay
    presentation timeline tooling
    controller/accessibility/responsive layout
↓
Presentation Polish
```

Phase 6UI-A is the first formal playable presentation/input vertical slice. Its goal is not final visual polish. Its acceptance target is that a normal battle loop can be understood and operated through the game UI without gameplay-driving debug keyboard commands such as `TestDrawCard` or `TestPlayFirstCard`.

UI-A0 through UI-A3 are implementation slices inside Phase 6UI-A, not separate top-level project phases.

### 15.2 Phase 6UI-A0 — Playable Gameplay Boundary

Do not begin formal Battle Widgets before gameplay exposes enough non-debug behavior for one complete normal player turn loop.

UI-A0 must establish:

```text
Playable Turn / Hand Lifecycle
Formal gameplay Request APIs
Shared gameplay-owned validation
Advisory playability queries + authoritative Request revalidation
Read-only coherent UI state/query boundary
Minimal authoritative Enemy Intent
```

The normal playable loop after UI-A0 must not require:

```text
TestDrawCard
TestPlayFirstCard
or equivalent rule-driving debug commands
```

#### PlayerTurn is gameplay request-eligible; Presentation may still lock the View

`PlayerTurn` means all authoritative turn-start gameplay work for that player turn has completed and normal gameplay Requests are eligible to be accepted.

It does **not** mean the View must immediately release input while historical presentation is still catching up.

Phase 6UI-A0 should introduce an explicit non-interactive `PlayerTurnStarting` boundary:

```text
EnemyTurnEnding
↓
choose/commit next authoritative Enemy Intent at the defined boundary
↓
PlayerTurnStarting
↓
restore turn resources
↓
atomically enqueue required turn-start work
↓
Draw / Shuffle / Trigger reactions resolve
↓
final turn-start gameplay resolution boundary
↓
BattleState = PlayerTurn
↓
presentation catch-up policy
↓
coherent authoritative snapshot refresh
↓
UI releases normal input
```

Durable invariants:

```text
PlayerTurn = authoritative gameplay Request-eligible state
UI input release = presentation policy layered on top
```

Do not enter `PlayerTurn` merely because turn-start Actions were scheduled. Conversely, do not delay `BattleState = PlayerTurn` until animation playback completes.

The existing `BattleStart` state may serve as the equivalent non-interactive battle-opening boundary. Do not add or rename a separate `BattleStarting` state unless implementation later demonstrates a concrete need.

Opening battle should conceptually resolve as:

```text
BattleStart
↓
initialize combatants / deck / battle-scoped state
↓
choose and commit initial Enemy Intent
↓
enqueue opening-Hand gameplay work
↓
Draw / Shuffle / Trigger gameplay resolution completes
↓
BattleState = PlayerTurn
↓
presentation catch-up policy
↓
coherent authoritative snapshot refresh
↓
UI input enabled
```

#### Turn / Hand lifecycle is authoritative gameplay

UI must not compensate for missing gameplay lifecycle by directly drawing, discarding or retaining cards.

Before Phase 6UI-A is considered playable, gameplay must own explicit rules for:

```text
BattleStart opening Hand setup
PlayerTurnStarting / PlayerTurnStart draw
PlayerTurn card play
PlayerTurnEnd remaining-Hand handling
transition to EnemyTurn
next PlayerTurn draw
```

The exact content rule is intentionally deferred until UI-A0 implementation. Examples include drawing N cards, discarding all remaining cards, retaining selected cards, or using a distinct opening-Hand count.

Durable rule:

```text
Widget code never creates the Hand lifecycle.
Gameplay owns it.
```

#### Hand cleanup timing relative to TurnEnded

For the initial playable semantics, player turn ending should use this ordering:

```text
RequestEndPlayerTurn
↓
final authoritative validation
↓
build required end-turn Action batch
↓
atomic insertion succeeds
↓
BattleState = PlayerTurnEnding
↓
current-version Hand cleanup Actions
↓
TurnEndedAction(Player)
↓
FTurnEndedEvent(Player)
↓
TurnEnded reactions
↓
QueueEmpty
↓
EnemyTurn progression
```

Therefore the initial `FTurnEndedEvent(Player)` describes a player turn whose current-version remaining-Hand cleanup has already committed.

Do not silently change this established timing when future mechanics such as Retain, Ethereal, end-of-turn Hand triggers or Relics that inspect Hand arrive.

If a future concrete mechanic must observe Hand state before normal cleanup, add an explicit earlier timing boundary/event for that real mechanic. Do not pre-implement speculative pre-cleanup events, and do not overload or silently reinterpret `TurnEnded`.

### 15.3 Formal gameplay Request boundary

Normal UI must not construct or enqueue authoritative BattleActions directly.

Forbidden permanent UI path:

```text
Widget
→ NewObject<UPlayCardAction>
→ ActionQueue.Add...
```

Temporary debug helpers such as `TestPlayFirstCard()` and `TestDrawCard()` must not become the permanent UI interface.

Preferred conceptual player command API:

```text
UI / ViewModel
↓
RequestPlayCard(CardInstance, RequestedTarget)
↓
final authoritative validation
↓
Rejected(reason)
or
AcceptedForResolution
```

and:

```text
UI / ViewModel
↓
RequestEndPlayerTurn()
↓
final authoritative validation
↓
Rejected(reason)
or
AcceptedForResolution
```

After acceptance, gameplay owns creation/insertion of the required BattleActions and queue sequencing.

#### AcceptedForResolution is not completed gameplay

`AcceptedForResolution` means only:

```text
the Request passed final authoritative validation
the required initial Action / Action batch was accepted by the gameplay resolution system
gameplay is responsible for resolving it
```

It does not mean:

```text
all card effects committed
the target definitely lost HP
all Trigger reactions completed
ResolutionFault cannot occur
the battle cannot end early
```

On `AcceptedForResolution`, the UI enters its `Resolving`/presentation-locked policy. Widget callbacks must not directly mutate or assume final HP/Block/Energy/zone results. Final results come from authoritative gameplay state plus committed Presentation Records when UI-A2 exists.

Before UI-A2 exists, the same rule still holds: UI-A0/UI-A1 learn final results by refreshing one coherent authoritative snapshot after gameplay resolution; there is simply no historical Presentation Record playback yet.

### 15.4 Query and Request share one gameplay validator

Playability Query is advisory presentation data. Request validation is authoritative.

Preferred structure:

```text
QueryPlayCard(Card, Target)
↓
common gameplay-owned validator
↓
current validation result
```

and:

```text
RequestPlayCard(Card, Target)
↓
same common gameplay-owned validator
↓
re-evaluate against current authoritative state
↓
if valid:
    enqueue authoritative work
    return AcceptedForResolution
else:
    return Rejected + structured reason
```

Durable rule:

```text
one gameplay validator
multiple callers
fresh validation at Request time
```

Never treat a previous Query as a capability token or permanent permission. At the same time, Query and Request must not duplicate independent rules for turn state, Energy, card zone, target validity, battle terminal state or resolution availability.

Structured failure reasons may eventually include concepts such as:

```text
NotEnoughEnergy
WrongTurn
InvalidTarget
NoValidTarget
BattleEnded
ResolutionBusy
CardNoLongerInHand
```

Do not freeze a large permanent enum until the concrete UI implementation requires it.

### 15.5 MVVM-style responsibility split

Phase 6UI should follow an MVVM-style boundary even if the first implementation does not require Unreal's full MVVM plugin/tooling.

Do not enable a new plugin or create a large ViewModel hierarchy merely to satisfy the term “MVVM”. Adopt the responsibility boundaries first; introduce UE MVVM tooling only if the concrete implementation benefits from it.

Conceptual ownership:

```text
MODEL / authoritative gameplay runtime

BattleManager / battle orchestration
Combatants
DeckRuntime
Status runtime
Enemy Intent
BattleActionQueue
```

```text
VIEWMODEL / read model

coherent read-only presentation-facing state
formal gameplay Request forwarding
presentation-only selection/focus state
formatted display state
immediate preview state
```

```text
VIEW

UMG Widgets
```

Preferred read direction:

```text
Authoritative Gameplay Model
↓
coherent read snapshot
↓
Battle ViewModel / Read Model
↓
UMG View
```

Player commands flow back only through formal gameplay Requests:

```text
UMG View
↓
ViewModel command
↓
Gameplay Request API
↓
authoritative validation / resolution
```

The ViewModel may own presentation-only concepts such as:

```text
selected Card
focused/inspected UI element
target-selection UI state
presentation-lock state
formatted text
display ordering
temporary deterministic preview result
```

It must not become a second gameplay authority or reimplement damage, Block, Energy, Status, card legality, target legality or Enemy behavior rules.

### 15.6 Coherent read snapshot boundary

UI must not assemble one displayed battle state from unrelated live reads that may belong to different gameplay commit points.

Avoid:

```text
read HP
↓ gameplay advances
read Energy
↓ gameplay advances
read Hand
read Status
read BattleState
```

Preferred boundary:

```text
Authoritative Gameplay State
↓
capture one coherent read snapshot
↓
Battle ViewModel
↓
UI refresh
```

A mature snapshot may conceptually contain:

```text
Battle identity
State revision
BattleState
Combatant views
Energy
Hand view
Draw / Discard / Exhaust views
Status views
committed Enemy Intent view
advisory playability / legal-target data where useful
```

Not every field must exist in UI-A0. Durable invariant:

```text
one UI refresh observes one coherent authoritative state revision
```

Any playability/legal-target information embedded in a snapshot remains advisory and revision-bound; formal Requests revalidate against current authoritative gameplay state.

UI presentation caches may exist for rendering convenience but are not authoritative. Widgets must not reach into private DeckRuntime arrays, maintain a competing fake Hand, infer card zones from animation position or scan the world for gameplay actors.

`GetFirstHandCard()`-style debug/helper access is not sufficient as the formal UI read model. Expose only the minimum safe read-only Hand/pile information that gameplay rules allow the player to inspect; do not expose mutable pile containers merely for Widget convenience.

#### Read snapshot revision identity

A mature snapshot should be identifiable as belonging to one coherent gameplay revision, conceptually through:

```text
BattleId
StateRevision
```

UI-A0 does not need a complex transactional state database. The first implementation may advance revision identity only at meaningful gameplay snapshot boundaries. Do not use frame number or Widget refresh time as authoritative gameplay revision identity.

### 15.7 Authoritative Enemy Intent

Enemy Intent is player-visible gameplay information and must be authoritative gameplay state, not a UI-only prediction.

Forbidden split source:

```text
Enemy logic decides Attack 5
├── UI stores Intent = Attack 5
└── unrelated code independently builds DamageAction(5)
```

Preferred flow:

```text
Enemy chooses Intent
↓
Intent becomes committed authoritative gameplay state
↓
UI reads that exact committed Intent
↓
EnemyTurn builds/consumes Actions from that same committed Intent
↓
current Intent resolves
↓
next Intent is chosen and committed at the defined timing boundary
↓
next PlayerTurn displays the new committed Intent
```

Durable invariant:

```text
The displayed committed Enemy Intent is the authoritative source
from which the corresponding EnemyTurn Actions are built.
```

Phase 6UI-A0 may start with only a minimal `Attack 5` Intent, but it must obey the same source-of-truth rule.

Intent representation should remain extensible to future single attack, multi-hit attack, Block, Buff, Debuff, combined, unknown and conditional behavior.

Widget code must not infer Intent from `EnemyTestAttackDamage`, pending `DamageAction`s or other debug/internal implementation details.

Do not overwrite the current Intent with the next Intent while the current EnemyTurn still needs its committed data for execution or presentation. A concrete implementation may split current/resolving/next intent state if a real need appears.

### 15.8 Gameplay and Presentation are separate timelines

`BattleActionQueue` is the authoritative gameplay-resolution timeline. It is not the animation timeline.

Forbidden coupling:

```text
BattleAction
↓
wait for animation to finish
↓
allow next gameplay Action
```

Also avoid reconstructing historical visuals by reading only the latest mutable runtime state after gameplay has already advanced.

Preferred conceptual boundary:

```text
Gameplay Action
↓
Commit
↓
produce Presentation Record / committed presentation snapshot
↓
gameplay may continue deterministically

Presentation Record
↓
Presentation Queue / presentation layer
↓
visual playback at normal / accelerated / skipped speed
```

Gameplay must produce the same authoritative result when presentation is normal-speed, accelerated, skipped or disabled.

Presentation may lock normal player input for readability, but it never becomes authoritative gameplay state.

### 15.9 Presentation Records are deterministic historical facts

Presentation Records are not gameplay mutation objects. The direction is one-way:

```text
Gameplay State
↓
Commit
↓
Presentation Record
↓
UI / animation
```

Never:

```text
Presentation Record
↓
modify authoritative gameplay state
```

A committed Presentation Record should conceptually belong to:

```text
Battle identity
Resolution identity
PresentationSequence
```

and preserve enough historical data for later rendering, potentially including:

```text
presentation operation type
stable Source visual identity
stable Target visual identity
Before values
After values
resolved values / reason needed for presentation
```

Example damage presentation snapshot:

```text
HPBefore = 50
HPAfter = 41
BlockBefore = 3
BlockAfter = 0
BlockedDamage = 3
HPDamage = 9
```

Not every field needs to be implemented in the first slice. Durable requirements:

```text
records cannot leak across battles
records from separate resolutions remain distinguishable
presentation order is deterministic
historical playback does not depend on mutable latest gameplay state
```

Presentation order must not depend on UObject address, delegate registration order, Widget creation order, animation start time, frame timing or unordered-container iteration.

Historical presentation may outlive the exact runtime moment that produced it. Do not assume every referenced UObject will still be valid when playback occurs. Prefer stable visual identity, snapshot values and safe/weak live references when live lookup is optional. If a live Actor no longer exists, presentation should skip/collapse/render safely without affecting gameplay.

Do not force every low-level internal Action to have a visible animation. Player-facing records should represent meaningful facts such as card played, damage dealt, Block gained, shuffle occurred, Relic triggered, Energy gained or card drawn. Internal class names such as `UDamageAction` belong in future developer tooling, not normal player UI.

### 15.10 Presentation catch-up and input release

Gameplay may resolve ahead of presentation, but normal player-visible decision sequences must not overlap unpredictably.

Conceptual flow once UI-A2 exists:

```text
Player input-ready
↓
Request accepted for resolution
↓
UI enters Resolving / presentation-locked state
↓
Gameplay resolution completes deterministically
↓
authoritative BattleState/result is already current
↓
Presentation Records accumulate/play
↓
presentation catches up
↓
capture latest coherent authoritative snapshot
↓
refresh Battle ViewModel as one boundary
↓
if gameplay state is Request-eligible, release normal player input
```

This is a UI/input policy. It does not make BattleActionQueue or BattleState wait for animations.

Before UI-A2 exists:

```text
presentation catch-up = immediate / no-op boundary
↓
capture coherent authoritative snapshot
↓
refresh ViewModel / HUD immediately
↓
release UI input only when authoritative gameplay state is Request-eligible
```

Therefore UI-A0 validates gameplay completion and coherent snapshot semantics only. UI-A1 may operate with instantaneous state changes. Neither UI-A0 nor UI-A1 depends on Presentation Records or a Presentation Queue; UI-A2 later replaces the no-op catch-up boundary with historical committed-fact playback.

Do not allow the player to submit a new normal card-play Request while presentation from the previous player-visible resolution is materially behind once UI-A2 exists.

Skip / fast-forward should conceptually:

```text
consume or collapse remaining presentation records
↓
discard obsolete transitional display state
↓
refresh from latest coherent authoritative snapshot
↓
release input when gameplay is Request-eligible
```

Presentation backlog must remain bounded by UX policy. High-volume/low-importance records may eventually be accelerated, coalesced, collapsed or omitted without changing gameplay results.

### 15.11 Phase 6UI-A interaction policy

Phase 6UI-A intentionally uses a simple explicit two-stage card interaction to validate selection, cancellation, legal target highlighting, Request submission and Resolving lock.

Conceptual UI state:

```text
Idle
↓
CardSelected
↓
ChoosingTarget / ReadyToConfirm
↓
PlayRequested
↓
Resolving
↓
Idle or terminal state
```

Initial examples:

```text
Enemy-target card
select card
→ select legal enemy
→ RequestPlayCard

Self-target card
select card
→ select self
→ RequestPlayCard

No-target card
select card
→ clear explicit confirmation affordance
→ RequestPlayCard
```

Cancellation must be explicit and discoverable. Exact controls may vary by input device, but the player should have a clear cancel path such as reselecting the card, right click, Escape/equivalent focus action or a visible cancel affordance.

The explicit two-stage flow is a Phase 6UI-A interaction policy, not a permanent gameplay rule or API requirement. Do not encode mandatory two-stage behavior into `CardData`, `PlayCardAction`, `BattleActionQueue` or the formal `RequestPlayCard` API.

Later presentation work may add single-target fast play, automatic target shortcuts, drag/drop, double-click/quick-cast or controller-specific confirmation without changing authoritative gameplay semantics.

### 15.12 UI-A only previews trustworthy immediate outcomes

Phase 6UI-A3 previews deterministic immediate results only.

Initial suitable scope:

```text
immediate Damage
immediate Block
Energy cost / immediate Energy result
```

Preview should reuse the same read-only gameplay rule resolution where practical:

```text
Damage preview
→ build preview FDamageSpec
→ run same read-only Damage Modifier Pipeline
→ show ResolvedAmount
→ no Commit
```

UI must not independently reimplement Strength / Weak / Vulnerable / Dexterity / Frailty formulas.

Do not present a complete future battle result as certain when it depends on TurnEnd reactions, Status decay, Enemy multi-action resolution, Relic triggers, RNG, conditional branches, future draws/shuffles or unknown Intent branches.

Future preview should distinguish conceptually between:

```text
deterministic immediate preview
conditional preview based on currently known information
uncertain/random outcome
```

Do not build a full future-turn battle simulator merely to support first-pass card inspection.

### 15.13 Operable HUD and scalability guardrails

Phase 6UI-A1 should cover the minimum coherent playable surface:

```text
Player HP / Block / Energy / Status
Enemy HP / Block / committed Intent / Status
Hand
Draw / Discard / Exhaust information
End Turn control
card playable/unplayable state + reason
card selection / cancellation
legal-target selection
Resolving input lock
Victory / Defeat / ResolutionFaulted
```

Core information must not depend on color or mouse hover alone.

Status presentation should be able to communicate through a combination of icon shape, amount, optional abbreviation/name, focus/inspect text and color. Energy numeric value is authoritative; dots/orbs may be auxiliary but must not scale into arbitrarily many icons when Energy grows.

Any information available through mouse Hover must also have a path for keyboard focus, gamepad focus or touch selection. Prefer one logical focused/inspected element model rather than mouse-specific information access.

Phase 6UI-A may initially contain one Enemy, but UI architecture must not permanently assume a single fixed Enemy slot. Conceptually prefer:

```text
EnemyArea
└── EnemyPresentation[]
```

Target selection should operate on a legal target set rather than hard-coding `BattleManager.Enemy`.

The first layout does not need final responsive polish, but data/API assumptions must not require rewriting gameplay/UI ownership when multiple enemies, many Status effects, many Relics, near-maximum Hand size or different aspect ratios arrive.

`ResolutionFaulted` must have visible development-facing presentation. A fault must not appear merely as buttons no longer responding or the battle silently freezing.

### 15.14 UI-A implementation slices

#### UI-A0 — Playable Gameplay Boundary

Acceptance requires all of the following:

```text
opening Hand comes from authoritative gameplay lifecycle
PlayerTurn is entered only after turn-start gameplay work finishes
PlayerTurn transition does not depend on presentation completion
normal player card play uses formal RequestPlayCard
normal end turn uses formal RequestEndPlayerTurn
Query and Request share gameplay-owned validation rules
Request revalidates current state
accepted Requests mean AcceptedForResolution, not completed effects
remaining-Hand cleanup timing relative to TurnEnded is explicit
Enemy Intent is authoritative and drives corresponding EnemyTurn Actions
UI can obtain one coherent authoritative read snapshot
normal playable flow does not require TestDrawCard or TestPlayFirstCard
UI-A0 does not require Presentation Records / Presentation Queue
```

Before UI-A2 exists, every `presentation catch-up` label below means an immediate/no-op presentation boundary followed by coherent snapshot refresh.

Authoritative playable flow:

```text
BattleStart
↓
initialize battle + commit initial Enemy Intent
↓
opening-Hand gameplay batch
↓
turn-opening gameplay resolution completes
↓
BattleState = PlayerTurn
↓
presentation catch-up (no-op before UI-A2)
↓
coherent snapshot refresh
↓
UI releases input
```

```text
PlayerTurn
↓
QueryPlayCard
↓
shared validator
↓
RequestPlayCard
↓
shared validator re-runs on current state
↓
Rejected(reason)
or AcceptedForResolution
↓
Resolving presentation/input policy
↓
Gameplay Actions / Events / Triggers commit
↓
authoritative BattleState/result is current
↓
presentation catch-up (no-op before UI-A2)
↓
coherent snapshot refresh
↓
UI releases input only if authoritative state is Request-eligible
```

```text
RequestEndPlayerTurn
↓
shared authoritative validation
↓
Hand-cleanup + TurnEnded batch
↓
atomic enqueue
↓
PlayerTurnEnding
↓
Hand cleanup
↓
TurnEndedAction(Player)
↓
TurnEnded reactions
↓
EnemyTurn
```

```text
EnemyTurn
↓
build authoritative Actions from committed Enemy Intent
↓
execute current Intent
↓
TurnEndedAction(Enemy)
↓
EnemyTurnEnding
↓
TurnEnded reactions
↓
QueueEmpty
↓
choose/commit next Enemy Intent at defined boundary
↓
PlayerTurnStarting
↓
restore turn resources
↓
turn-start Draw work
↓
Draw / Shuffle / Trigger gameplay resolution completes
↓
BattleState = PlayerTurn
↓
presentation catch-up (no-op before UI-A2)
↓
coherent snapshot refresh
↓
UI releases input
```

#### UI-A1 — Operable Battle HUD

Begin only after UI-A0 provides the playable gameplay boundary.

Scope:

```text
HP / Block / Energy / Status
Hand / pile views
Enemy Intent
End Turn
card selection / cancel / target selection
playability / rejection feedback
Resolving lock
Victory / Defeat / ResolutionFaulted
```

Acceptance: a normal battle turn can be operated without `TestDrawCard`, `TestPlayFirstCard` or other gameplay-driving debug keyboard commands. Until UI-A2, committed state changes may appear immediately after coherent snapshot refresh rather than through historical animation playback.

#### UI-A2 — Basic Committed Presentation

Introduce the first Presentation Record playback surface with deliberately small scope:

```text
Damage
Block change
Card draw
Discard
Shuffle
Status amount change
Victory
Defeat
ResolutionFault
```

Goal is gameplay legibility, not final animation quality. Use committed snapshots rather than reconstructing historical results from current mutable state. UI-A2 replaces the UI-A0/UI-A1 no-op presentation catch-up boundary with real committed-fact playback while leaving Gameplay/BattleState timing unchanged.

#### UI-A3 — Deterministic Immediate Preview

Initial preview scope:

```text
immediate Damage
immediate Block
Energy cost / immediate Energy result
```

Do not block Phase 6UI-A completion on advanced conditional/random prediction.

### 15.15 Phase 7, Phase 8, UI-B and Presentation Polish

After Phase 6UI-A is playable:

```text
Phase 7
→ Relic runtime
→ Sundial first validation
→ Relic UI / trigger feedback
```

Phase 8 validates two upgraded Pommel Strikes + Sundial as both a generic gameplay interaction and a visually understandable interaction through the playable UI.

Phase 6UI-B is the advanced UX/tooling pass. Potential scope:

```text
advanced / conditional Outcome Preview
Keyword presentation
CardText formatting
Developer Overlay
Action / Event / Presentation inspection
Presentation Timeline tooling
gamepad navigation
accessibility improvements
responsive layout work
```

Do not move these features into UI-A unless they become necessary to make the basic battle playable or diagnosable.

Presentation Polish comes after gameplay architecture and playable UI are validated. Potential work:

```text
drag-and-drop card play
single-target fast play
automatic target shortcuts
fan-shaped hand layout
target arrows
card motion
damage reactions
floating combat text
Status / Relic trigger animation
VFX
SFX
animation speed controls
skip / fast-forward presentation
```

These features must not change authoritative gameplay semantics.

### 15.16 UI/MVVM architecture summary

```text
                     GAMEPLAY / MODEL
                           │
             ┌─────────────┴─────────────┐
             │                           │
      Gameplay Validator          Authoritative State
             │                           │
 Query ──────┤                           │
 Request ────┘                           │
      │                                  │
      ▼                                  │
AcceptedForResolution                    │
      │                                  │
      ▼                                  │
BattleActionQueue                        │
      │                                  │
      ▼                                  │
Modifier / Action / Event / Trigger      │
      │                                  │
      ├──────────── Commit ──────────────┤
      │                                  │
      ▼                                  ▼
Presentation Records              Coherent Read Snapshot
      │                                  │
      ▼                                  ▼
Presentation Queue              VIEWMODEL / READ MODEL
      │                                  │
      └──────────────┬───────────────────┘
                     ▼
                    VIEW
                     │
                    UMG
                     │
                     ▼
              Player Interaction
                     │
                     ▼
               Formal Request
```

Durable interpretation:

```text
Gameplay
= what is true

ViewModel / Read Model
= what coherent authoritative state should be exposed to presentation now

Presentation
= how already-committed facts are played back and explained
```

The three responsibilities must remain distinct.
