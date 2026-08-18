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
  - [x] Phase 5R regression Automation Tests implemented; the previous 12-test suite passed through UE5.8 self-hosted CI, and the updated 13-test suite passes locally pending an owner-triggered CI rerun.
- [ ] Phase 6 battle events / triggers implemented.
  - [ ] Phase 6A TurnEnd Trigger Vertical Slice — NEXT.
  - [ ] Phase 6B Battle Turn Wiring.
  - [ ] Phase 6C DeckShuffled Event.
  - [ ] Phase 6R Regression Gate.
- [ ] Phase 7 relic system implemented.
- [ ] Phase 8 Pommel Strike+ + Sundial architecture validation implemented.

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
- the suite contains 13 Phase 5 tests and passes locally in UE5.8;
- the previous 12-test suite passed through the Windows `ue58` self-hosted runner; the updated 13-test gate requires a new owner-triggered `workflow_dispatch` run before its CI evidence is recorded.

### Manual UE assets/configuration

Phase 4:

```text
Content/SlayTheSpireDemo/Data/Cards/Ironclad/Attacks/DA_Card_Strike
Content/SlayTheSpireDemo/Data/Cards/Ironclad/Attacks/DA_Card_PommelStrike
Content/SlayTheSpireDemo/Data/Cards/Ironclad/Skills/DA_Card_Defend
```

Phase 5 status assets:

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
5R  Phase 5 Automation Regression Gate                             COMPLETE
```

Phase 5 acceptance is satisfied:

```text
Phase 5C source compiles
+ Phase 5C UE5.8 PIE validation passes
+ Phase 5 Automation regression tests pass
```

Next development phase is Phase 6 Battle Events and Triggers.

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

The hardened `.github/workflows/ue-phase5-tests.yml` workflow builds `SlayTheSpireDemoEditor` and runs `Automation RunTest SlayTheSpireDemo.Phase5` on the Windows UE5.8 self-hosted runner. The previous 12-test gate passed end-to-end. The updated 13-test suite passes locally and awaits the next owner-triggered workflow run.

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

### Phase 6 — Battle Events and Triggers — NEXT

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
6A  TurnEnd Trigger Vertical Slice                                 NEXT
6B  Battle Turn Wiring
6C  DeckShuffled Event
6R  Phase 6 Regression Gate
```

Do not implement Phase 7 relics during Phase 6.

#### Phase 6A — TurnEnd Trigger Vertical Slice — NEXT

Phase 6A must be a real vertical slice rather than horizontal infrastructure built around fake triggers.

Minimum implemented mechanic:

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
FTurnEndedEvent as the only initial event payload
synchronous on-demand Status trigger collection
transient deterministic TriggerCandidate sorting
TurnEndStatusDecayTrigger
exact-instance ReduceStatusAction
queued depth-first nested reactions
focused Unreal Automation Tests
```

Do not wire the complete `ABattleManager` player/enemy turn lifecycle until Phase 6B.

##### Event representation and lifetime

Events are short-lived typed value data, not gameplay UObjects and not persistent registry entries.

Phase 6A introduces only `FTurnEndedEvent`. When Phase 6C adds a real second event, add `FDeckShuffledEvent` then. Do not predeclare speculative event alternatives.

`FBattleEvent` may use a small tagged variant such as `TVariant` or an equivalently type-safe checked representation. Requirements are more important than the exact representation:

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

When Phase 7 introduces a real `RelicContainer`, extract the smallest trigger contributor/collector boundary needed to collect both Status and Relic sources. Do not prebuild that generic boundary during Phase 6A.

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

Phase 6A sorting:

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

This distinction is required for deterministic reasoning.

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

Add ordered atomic batch APIs:

```text
AddBatchToFrontPreserveOrder(...)
AddBatchToBackPreserveOrder(...)
```

Both should share one batch validator. Empty batch is a legal no-op success.

For every non-empty batch, validate the complete batch before modifying `PendingActions`:

```text
all Actions are valid
all Actions are unfinished
all Action Outer values are this Queue
no duplicate Action pointer inside the batch
no batch Action equals CurrentAction
no batch Action already exists in PendingActions
```

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

##### Queue resolution-fault safety

Phase 6 introduces a hard safety boundary against infinite or structurally invalid reaction chains.

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

When a structural error is discovered while a current Action is still inside `Execute()` (for example Dispatcher final reaction insertion fails), do not clear `CurrentAction` from inside that Action's call stack. Instead:

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

Phase 6B will make `ABattleManager` explicitly transition to `EBattleState::ResolutionFaulted` when `OnResolutionFaulted` fires.

##### Exact-instance status decay

Do not implement turn-end decay as negative `ApplyStatusAction`.

Introduce a dedicated `ReduceStatusAction` carrying the exact expected runtime `UStatusInstance*` plus the amount to remove.

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

##### Phase 6A Automation acceptance

Required focused tests:

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

Status.TurnEndDecay
Status.TurnEndDecayRemovesExactInstanceAtZero
Status.RemovedAndRecreatedInstanceIsNotReduced
Status.OtherActorsTurnDoesNotDecay
Status.SnapshotEligibilityVsLiveActionValidation

Safety.ResolutionBudgetFaultsInsteadOfLoopingForever
```

Do not mark Phase 6A complete until these intended semantics compile and pass in UE5.8 Automation.

#### Phase 6B — Battle Turn Wiring

After Phase 6A is stable, wire the real battle turn lifecycle.

Add explicit ending/fault states as needed:

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

Current Phase 6 fixed Enemy behavior may be constructed upfront because it is a known `DamageAction`:

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

Final QueueEmpty flow:

```text
CheckBattleResult
↓
Victory/Defeat → stop
PlayerTurnEnding → StartEnemyTurn
EnemyTurnEnding  → StartPlayerTurn
ResolutionFaulted → no transition
```

`OnResolutionFaulted` makes BattleManager enter `EBattleState::ResolutionFaulted`, zero/reject player input, and stop turn/victory progression for that resolution. Log fault reason, executed count and last Action so PIE does not appear to freeze silently.

The upfront Enemy batch rule is deliberately narrow. It applies to the current fixed Enemy behavior only. Future Enemy Intent/composite Actions may need Execute-time state to decide follow-ups and must continue to obey the project's Execute-time resolution rule.

When dynamic Enemy continuation becomes concrete, introduce an explicit continuation/barrier insertion mechanism so dynamic follow-ups can be placed before the turn-end continuation. Do not make "all future Enemy actions must be precomputed" or "dynamic actions may never AddToBack" a permanent architecture rule, and do not implement a speculative continuation API in Phase 6.

Phase 6B Automation should include:

```text
Turn.PlayerEndingStateCommitsOnlyAfterEnqueueSuccess
Turn.EnemyBatchInsertionIsAtomic
Turn.LethalEnemyActionSkipsTurnEndedEvent
Turn.OneFinalQueueEmpty
```

PIE should validate real Weak/Vulnerable/Frailty turn-end decay for both Player and Enemy without growing new permanent `ABattleManager` rule-test entry points.

#### Phase 6C — DeckShuffled Event

Add `FDeckShuffledEvent` only when implementing this slice.

`ShuffleDeckAction` emits the event only after `DeckRuntime::ShuffleDiscardIntoDrawPile()` actually succeeds. Failed/no-op shuffles emit no event.

Required ordering for the existing empty-draw flow:

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

Do not implement Sundial in Phase 6C. Phase 7 should be able to add Sundial as a new trigger source without rewriting DeckRuntime/ShuffleDeckAction event timing.

#### Phase 6R — Regression Gate

Phase 6 completion requires:

```text
all Phase 6 Automation tests pass
+ all 13 Phase 5 regression tests still pass
+ required UE5.8 PIE turn-flow validation passes
```

Use `Trigger.CollectionOrderDoesNotMatter`, not the obsolete `RegistrationOrderDoesNotMatter`, because Phase 6 has no persistent Trigger Registry.

### Phase 7 — Relics

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

### Phase 8 — Combo Architecture Validation

Validate two upgraded Pommel Strikes + Sundial without special-case combo code. The interaction must emerge from generic card, draw, shuffle, event, modifier and action rules.

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
→ future BattleEvent
→ future Trigger Actions
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

During Phase 5, `AmountToAdd > 0` and active Amount > 0. Do not use negative ApplyStatus deltas for lifecycle decay.

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

Current Phase 5 pipelines may collect directly from `StatusContainer` because Status is currently the only implemented runtime modifier source.

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

Do not create empty folders just to reserve future architecture. `Keywords/` is a future presentation-oriented source area and should be created only when keyword/card-text presentation work actually begins. `Tests/` now contains focused automation regressions and should remain small and rule-oriented.

Prefer forward declarations and small public headers. UObject runtime ownership must be GC-safe through clear Outer/`UPROPERTY`/`TObjectPtr` references. Do not enable Tick by default.

### 6.1 BattleManager and debug harness boundary

`ABattleManager` may temporarily own battle orchestration, the battle-scoped sequence allocator and PIE debug entry points while the learning/demo framework is still being validated.

Do not split it merely for aesthetic purity.

Debug hooks are not intended to become permanent production battle APIs. Automation Tests now cover the stabilized Phase 5 deterministic rules. As Phase 6 introduces formal event/input paths, stop growing `ABattleManager` as the default place for new rule-test commands and incrementally separate debug harness responsibilities when there is a concrete maintenance benefit.

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

Presentation style is not gameplay logic. `Buff`, `Debuff`, `CardMechanic` or similar presentation categories may drive UMG colors/icons, but battle code must not depend on red/gold/etc. UI colors.

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

Prefer clear architecture over clever abstractions.

---

## 9. Build and Verification Rules

After C++ changes:

- verify includes/module dependencies;
- build `SlayTheSpireDemoEditor` when a build environment is available;
- report build errors instead of masking them;
- never claim successful UE build/PIE without actually running it;
- if source tooling cannot run UE, require user-side compile/PIE before marking a phase complete.

For deterministic core rules, prefer adding focused Unreal Automation Tests once the rule has stabilized. Tests should validate state/results directly where practical instead of relying only on expected log text.

The Phase 5 regression gate is automated by `.github/workflows/ue-phase5-tests.yml` on the Windows `ue58` self-hosted runner. Keep the workflow manually triggered and restricted to trusted `main` execution unless the self-hosted security model is deliberately changed.

Phase 6A must be Automation-validated before Phase 6B editor wiring. Phase 6R must rerun all 13 Phase 5 tests in addition to the Phase 6 suite.

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
```

`ABattleManager` currently owns the battle-scoped ActionQueue, DeckRuntime and temporary RuntimeSequence allocator. Each `ACombatant` owns its StatusContainer. BattleManager debug entry points are temporary validation infrastructure, not the intended long-term formal input API.

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
- Phase 5R — LOCAL PASSED / CI RERUN REQUIRED: all 13 focused Unreal Automation regression tests pass locally; the previous 12-test suite passed through UE5.8 self-hosted CI, and the updated gate awaits an owner-triggered run.
- Phase 5 — PASSED: Modifier-Based Framework and Status System complete for the defined Phase 5 scope.
- Phase 6 — NOT YET PASSED: Phase 6A is the next implementation slice.

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
- keep documentation synchronized with actual source/PIE/Automation state;
- do not fill this file with daily implementation trivia.
