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
- [x] Phase 6 battle events / triggers complete for the defined Phase 6 scope.
  - [x] Phase 6A TurnEnd Trigger Vertical Slice — COMPLETE; UE5.8 self-hosted CI validated at 23/23.
  - [x] Phase 6B Battle Turn Wiring — COMPLETE; expanded Queue contract suite passed at 12/12, total Phase5 + Phase6A + Phase6B gate passed 48/48, and post-hardening PIE turn-cycle validation passed.
  - [x] Phase 6C DeckShuffled Event — COMPLETE; UE5.8 Editor build + Phase6C 5/5 and total Phase5–Phase6C 53/53 Automation gate passed.
  - [x] Phase 6R Regression Gate + test-module extraction — COMPLETE; UE5.8 full 53/53 regression gate and Shipping exclusion validation passed.
- [ ] Phase 6UI-A playable Battle UI — IN PROGRESS.
  - [x] UI-A0 Playable Gameplay Boundary — COMPLETE; UE5.8 Editor build + Phase5/6 regressions + UI-A0 20/20 passed, current owner-only gate 73/73.
  - [x] UI-A1 Operable Battle HUD — COMPLETE; current Self-target Player-selection path revalidated with UE5.8 UI-A1 11/11 and manual PIE.
  - [ ] UI-A2 Basic Committed Presentation — NEXT / DESIGN LOCKED; UI-A2A infrastructure is the next implementation slice. See `docs/Phase6UIA2Implementation.md` and Section 16.
  - [ ] UI-A3 Deterministic Immediate Preview — dynamic card/status text slice implemented, DataAsset-authored and UE5.8/PIE/package revalidated with UI-A3 8/8; remaining target-specific/energy-result preview still planned.
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

Phase 6C validated:

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
- the owner-only `.github/workflows/ue-phase6c-tests.yml` passed Phase5 13 + Phase6A 23 + Phase6B 12 + Phase6C 5 = 53/53 tests.

Phase 6R validated:

- all Automation-only Phase5/6A/6B/6C sources and reflected `UPhase6ATest*` helpers were moved out of the Runtime module into the Editor-only `SlayTheSpireDemoTests` module;
- `SlayTheSpireDemoTests` depends on `SlayTheSpireDemo`; the Runtime module and Game target do not depend on or include the test module;
- `FTriggerContext` is exported from Runtime only as required for the cross-module test-helper boundary; gameplay semantics are unchanged;
- `SlayTheSpireDemoEditor` builds with the extracted test module;
- Editor Automation still discovers and passes exactly Phase5 13/13 + Phase6A 23/23 + Phase6B 12/12 + Phase6C 5/5 = 53/53;
- the normal `SlayTheSpireDemo Win64 Shipping` target builds and the Shipping exclusion gate finds no `SlayTheSpireDemoTests` / `Phase6ATest*` artifacts;
- Phase 6R adds no gameplay, UI, DataAsset or map semantics, so no additional manual PIE/asset validation is required beyond the already-passed Phase 6 gameplay evidence.

Phase 6UI-A0 validated:

- opening Hand and normal player-turn draws are authoritative gameplay lifecycle, with explicit `PlayerTurnStarting` before turn-start work and `PlayerTurn` remaining the Request-eligible state;
- formal `QueryCardPlayability` / `QueryPlayCard` / `RequestPlayCard` and EndTurn Query/Request APIs share gameplay-owned validation, and Request always revalidates current authoritative state;
- `AcceptedForResolution` means accepted into gameplay resolution, not completed effects;
- the initial DrawPile is deterministically shuffled with the battle-scoped RNG before the opening Hand, while initialization intentionally emits no `DeckShuffled` event;
- the committed Enemy Intent is the source for the corresponding EnemyTurn Actions; player-facing `CurrentResolvedDamageAmount` reuses the Damage Modifier Pipeline for the current snapshot revision but is explicitly not a guarantee of future EnemyTurn damage after intervening reactions;
- `FBattleReadSnapshot` provides one coherent `(BattleId, StateRevision)` read boundary and `OnReadStateReady` is deferred beyond the public Request call stack so it cannot fire before an accepted Request returns;
- read consumers use subscribe-then-pull initialization because `OnReadStateReady` is a non-replaying edge notification;
- healthy QueueEmpty still accepts empty batches as no-op success while rejecting direct non-empty insertion during observer notification;
- faulted resolutions publish a readable `ResolutionFaulted` snapshot through the battle-level Ready path rather than masquerading as healthy idle;
- the owner-only `.github/workflows/ue-phase6uia0-tests.yml` built `SlayTheSpireDemoEditor` and passed Phase5 13/13 + Phase6A 23/23 + Phase6B 12/12 + Phase6C 5/5 + Phase6UIA0 20/20 = 73/73 for the completed run.

Phase 6UI-A1 validated:

- the concrete Battle HUD is operable through normal UI controls without gameplay-driving debug keyboard commands;
- Enemy-target and Self-target cards both use gameplay-provided public legal-target selection and formal Request revalidation; Self-target Defend resolves by selecting the highlighted Player presentation;
- HP, Block, Energy, Hand/pile counts, Status inspection, committed Enemy Intent, card selection/cancel, target selection, End Turn, Resolving lock and terminal feedback are wired through the ViewModel/base-widget boundary;
- one full player → enemy → player PIE cycle passes through the normal UI path;
- the packaged Defend card resolves its dynamic `{Block}` description instead of displaying the literal placeholder;
- the latest UE5.8 owner gate passes Phase5 13/13 + Phase6A 23/23 + Phase6B 12/12 + Phase6C 5/5 + Phase6UIA0 20/20 + Phase6UIA1 11/11 + Phase6UIA3 8/8 = 92/92.

Phase 6UI-A3 dynamic-text slice validated:

- `BattleTextResolver` produces player-facing Card/Status description values from stable named Effect/Modifier arguments and the existing read-only Damage/Block pipelines;
- required Card/Status DataAsset description formats and argument names are authored in UE Editor and validated;
- preview resolution remains read-only and does not Commit, enqueue Actions, emit Events, consume RNG or mutate runtime state;
- UE5.8 Automation passes the current UI-A3 8/8 suite, and PIE/package validation confirms the authored descriptions resolve in runtime builds;
- target-specific exact preview and immediate Energy-result preview remain future UI-A3 work.

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

Phase 6C, Phase 6R and Phase 6UI-A0 require no new `.uasset` / `.umap` configuration.

Phase 6UI-A1 and the current UI-A3 authored asset work are validated in UE Editor, including the current Self-target Player-selection path. The concrete Battle HUD/card/combatant presentation assets and the Card/Status dynamic-description fields must remain wired to the ViewModel/player-facing snapshot path; do not replace them with Widget-side gameplay calculations.

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

Implemented a focused Unreal Automation suite now located at:

```text
Source/SlayTheSpireDemoTests/Private/Phase5RegressionTests.cpp
```

It was originally introduced in the Runtime module and was moved into the Editor-only test module by Phase 6R.

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

The hardened `.github/workflows/ue-phase5-tests.yml` workflow builds `SlayTheSpireDemoEditor` and runs `Automation RunTest SlayTheSpireDemo.Phase5` on the Windows `ue58` self-hosted runner. The current 13-test gate has passed end-to-end.

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

### Phase 6 — Battle Events and Triggers — COMPLETE

Phase 6 introduced deterministic post-commit facts and queued reactions without weakening the existing ActionQueue / Modifier / Commit boundaries.

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
6C  DeckShuffled Event                                             COMPLETE / UE5.8 5/5, TOTAL 53/53 PASSED
6R  Phase 6 Regression Gate + test-module extraction               COMPLETE / TOTAL 53/53 + SHIPPING EXCLUSION PASSED
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

Phase 6A introduced `FTurnEndedEvent`; Phase 6C adds the second real event, `FDeckShuffledEvent`. Do not predeclare speculative event alternatives.

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

#### Phase 6C — DeckShuffled Event — COMPLETE / UE5.8 53/53 PASSED

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

The owner-only `.github/workflows/ue-phase6c-tests.yml` passed:

```text
Phase 5    13/13
Phase 6A   23/23
Phase 6B   12/12
Phase 6C    5/5
Total      53/53
```

Phase 6C is complete.

#### Phase 6R — Regression Gate + Test Module Extraction — COMPLETE

Phase 6R is an engineering/regression slice. It adds no new gameplay behavior.

Final module boundary:

```text
SlayTheSpireDemo          Runtime
SlayTheSpireDemoTests     Editor-only
        ↓ depends on
SlayTheSpireDemo
```

Automation-only sources now live under:

```text
Source/SlayTheSpireDemoTests/Private/
```

including:

```text
Phase5RegressionTests.cpp
Phase6ARegressionTests.cpp
Phase6AExecutionOrderTests.cpp
Phase6ATestTypes.h/.cpp
Phase6BRegressionTests.cpp
Phase6CRegressionTests.cpp
```

The reflected test helpers use `SLAYTHESPIREDEMOTESTS_API`. The Runtime module contains no `UPhase6ATest*` reflected helper and never depends on `SlayTheSpireDemoTests`.

`SlayTheSpireDemo.uproject` declares the test module as `Type = Editor`; `SlayTheSpireDemoEditorTarget` includes it, while the normal Game target does not.

`FTriggerContext` is exported from Runtime because the extracted test trigger helpers call its runtime methods across the module/DLL boundary. This export does not alter Trigger semantics.

The owner-only `.github/workflows/ue-phase6r-tests.yml` validates:

```text
Editor build with SlayTheSpireDemoTests
Phase 5    13/13
Phase 6A   23/23
Phase 6B   12/12
Phase 6C    5/5
----------------
Total      53/53

Shipping game target build
SlayTheSpireDemoTests excluded from Shipping artifacts
Phase6ATest* excluded from Shipping artifacts
```

Phase 6R passed. Phase 6 is complete for the defined scope.

No additional PIE, Blueprint, DataAsset or map action is required for 6R because the slice only changes test/module engineering boundaries and the full gameplay regression gate remained green.

### Phase 6UI-A — Playable Battle UI — IN PROGRESS

Implementation order inside this phase:

```text
UI-A0 Playable Gameplay Boundary   COMPLETE / UE5.8 UI-A0 20/20, CURRENT TOTAL 73/73 PASSED
↓
UI-A1 Operable Battle HUD          COMPLETE / UE5.8 UI-A1 11/11 + PIE/PACKAGE VALIDATED
↓
UI-A2 Basic Committed Presentation NEXT / DESIGN LOCKED / UI-A2A IMPLEMENTATION NEXT
↓
UI-A3 Deterministic Immediate Preview — DYNAMIC TEXT SLICE VALIDATED / UE5.8 UI-A3 8/8; REMAINING TARGET/ENERGY PREVIEW PLANNED
```

UI-A0 and UI-A1 are complete. The current UI-A3 dynamic-text slice is also validated; UI-A2A is the next implementation slice. Detailed evidence lives in `docs/Phase6UIA0Implementation.md`, `docs/Phase6UIA1Implementation.md`, `docs/Phase6UIA2Implementation.md` and `docs/Phase6UIA3DynamicTextImplementation.md`.

The durable UI/MVVM/Presentation architecture and acceptance criteria are defined in Sections 15 and 16. Do not begin Phase 7 until Phase 6UI-A is playable unless the user explicitly changes the order.

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

### 4.20 Public stable-read completion is battle-level and non-reentrant

`BattleActionQueue::OnResolutionIdle` is an internal settled-resolution fact. Widgets must not bind to it or to `OnQueueEmpty` as their completion protocol. BattleManager maps settled/fault outcomes into the public `OnReadStateReady(BattleId, StateRevision)` boundary.

A public `OnReadStateReady` publication must not fire before a public `RequestPlayCard` / `RequestEndPlayerTurn` call has returned its `AcceptedForResolution` result. The current implementation defers publication through the CoreTicker. Read consumers initialize by subscribing first and then immediately pulling the current player-facing snapshot because Ready is a non-replaying edge notification.

### 4.21 Player-facing Intent values must state their time semantics

The committed Enemy Intent remains the authoritative plan from which EnemyTurn Actions are built. `EnemyIntentPlayerFacing.CurrentResolvedDamageAmount` is a gameplay-derived value produced by running the current snapshot state through the same Damage Modifier Pipeline; it is not guaranteed future EnemyTurn damage when mandatory reactions may alter state before execution.

Never let Widget/ViewModel code reimplement Strength/Weak/Vulnerable formulas or relabel a current-state value as a guaranteed future result.

### 4.22 Initial battle shuffle is setup, not DeckShuffled gameplay

`DeckRuntime::InitializeFromDefinitions` uses the battle-scoped RNG stream to deterministically shuffle the initial DrawPile before the opening Hand. This setup randomization advances the same RNG stream used by later reshuffles but emits no `FDeckShuffledEvent`. Only successful discard-to-draw gameplay reshuffles emit the event.

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

Target Runtime source areas as needed:

```text
Battle/ Combat/ Actions/ Cards/ Deck/ Status/ Modifiers/ Relics/ Events/ Enemy/ UI/ Keywords/
```

Automation-only source belongs under the Editor-only test module:

```text
Source/SlayTheSpireDemoTests/Private/
```

Do not create empty folders just to reserve future architecture. `Keywords/` is a future presentation-oriented source area and should be created only when keyword/card-text presentation work actually begins. Focused Automation regressions and test-only reflected helpers must remain in `SlayTheSpireDemoTests`, not in the Runtime module.

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

Card descriptions use semantic named templates for deterministic numeric values:

```text
Deal {Damage} damage.
Apply {VulnerableAmount} [Keyword:Vulnerable].
```

The current minimal `BattleTextResolver` resolves numeric placeholders. A future
Keyword/RichText presentation layer may additionally resolve:

```text
KeywordId → localized display name
KeywordId → tooltip description
KeywordId → presentation style
```

Keyword parsing/storage remains deferred; it is not part of the minimal numeric
FText::Format implementation.

#### Dynamic card-value preview

Card and Status `Description` properties retain their serialized names for existing
`.uasset` compatibility, but their authored values are FText::Format patterns.
Normal UI must not reimplement combat formulas such as Strength/Weak/Vulnerable checks.

Current model:

```text
Gameplay:
DamageAction / GainBlockAction
→ typed Spec
→ typed ModifierPipeline
→ ResolvedAmount
→ Commit

UI Preview:
BattleTextResolver
→ preview typed Spec
→ same read-only typed ModifierPipeline rules
→ ResolvedAmount
→ no Commit / no state mutation
```

Every concrete CardEffect must expose stable named deterministic preview values.
Every concrete Damage/Block Modifier must expose stable named Status-description
values. Future Effect/Modifier subclasses must implement these read-only contracts;
do not silently preserve numeric gameplay fields as stale hand-authored text.

Current card-face targeting policy is deliberately source-baseline:

```text
Enemy-target card → Source = Player, Target = null
Self-target card  → Source = Player, Target = Player
```

Therefore Strength/Weak affect an Attack card face while a particular Enemy's
Vulnerable does not. Target-specific exact previews remain a separate future UI
surface. Self Block previews include Dexterity/Frailty through the normal Block
pipeline.

Status descriptions resolve from the exact `UStatusInstance` and its immutable
Modifier definitions. `{Amount}` is reserved for runtime Amount. FlatAdd values
respect AmountMode. Ratio descriptions expose the configured per-application
percentage; ScaleWithAmount must use percentage plus Amount rather than claiming
an exact cumulative percentage that ignores gameplay's per-step integer flooring.

`TryBuildPlayerFacingReadSnapshot()` resolves final Card/Status descriptions for
the same `(BattleId, StateRevision)`. ViewModels copy that FText and Widgets only
render it. Preview evaluation must be explicitly read-only and must not consume
RNG, enqueue Actions, mutate status state or emit gameplay events.

Missing, duplicate or invalid arguments render `?` and log a development error.
CardData/StatusData validation treats those configurations as invalid.

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
14. Do not implement KeywordLibrary, rich-text parsing or keyword tooltips/styles merely because status mechanics have player-facing names. The explicitly approved UI-A3 numeric `BattleTextResolver` remains separate from future Keyword presentation.
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
32. Automation-only reflected `UCLASS` types and regression sources belong in the Editor-only `SlayTheSpireDemoTests` module. Never reintroduce test-only reflected classes into the Runtime module merely for convenience.
33. `FDeckShuffledEvent` is emitted only after a successful shuffle commit. Expected/no-op shuffles emit no event, and reactions to a successful shuffle resolve before the already-pending RetryDraw continuation.
34. Phase 6UI-A does not begin before Phase 6R unless the user explicitly changes the order. Phase 7 follows the playable UI-A slice, not Phase 6R directly.
35. `PlayerTurn` is an authoritative gameplay request-eligible state. Presentation may still lock the View after Gameplay enters `PlayerTurn`; animation completion must never be required to commit `BattleState = PlayerTurn`.
36. Before UI-A2 exists, presentation catch-up in UI-A0/UI-A1 is an immediate/no-op boundary followed by coherent authoritative snapshot refresh; UI-A0 must not depend on Presentation Records/Presentation Queue.
37. Formal UI/ViewModel consumers must use `OnReadStateReady` plus coherent player-facing snapshots rather than treating `QueueEmpty` or Queue-level idle as the public completion protocol.
38. `OnReadStateReady` must not fire re-entrantly before a public accepted Request returns. Read consumers bind first, then immediately pull the current snapshot because Ready is a non-replaying edge notification.
39. `EnemyIntentPlayerFacing.CurrentResolvedDamageAmount` means current-snapshot resolved damage, not guaranteed future EnemyTurn damage; never hard-code TurnEnd decay rules in UI to manufacture a future guarantee.
40. Initial battle setup shuffle consumes the battle RNG but emits no `DeckShuffled`; only successful discard-to-draw gameplay reshuffles emit that event.
41. During QueueEmpty observer notification, healthy empty Action batches remain legal no-op success; only non-empty direct insertion is rejected.
42. During UI-A2 playback, `FPresentationStateSnapshot` is the only historical display input. Historical Envelopes must never query mutable `UCardInstance`, `UStatusInstance`, `ACombatant` or `ABattleManager` state while rendering.
43. Runtime input bindings are not historical state. Refresh `RuntimeId → UCardInstance` and `TargetId → ACombatant` bindings only after presentation catches up to the newest `(BattleId, StateRevision)`; never use those bindings while playing an older Envelope.
44. When Presentation is enabled, `OnReadStateReady` must not bypass the Presentation Coordinator/Controller and directly apply a newer live state to the Battle HUD ViewModel. The Controller/Presenter owns display-state sequencing; `OnReadStateReady` remains only a stable-read edge notification.
45. Ordinary gameplay validation rejection creates no Presentation Resolution. Once validation succeeds, any framework fault during build/enqueue/processing must be represented by a fault/system Presentation Resolution so committed facts and the final fault snapshot can still be sealed together.
46. Gameplay runtime owners such as `ACombatant`, `UDeckRuntime` and `UStatusContainer` must not depend on Presentation Recorder. They return typed Commit/Mutation results; the Action or BattleManager that owns Source/Reason/Resolution context converts those results into Presentation Records.
47. Presentation backlog, playback timeout, missing Blueprint callback, skipped playback or disabled Presentation must never request a Gameplay `ResolutionFault`. Presentation failure catches up or falls back to the newest frozen snapshot.
48. Combatant Presentation IDs are resolved by one Battle-layer function and the resolved IDs, not raw authored fields, are validated as non-empty and battle-scoped unique. Snapshot, LegalTargets and Presentation Records must share that resolved identity. Invalid presentation bootstrap is a UI/Presentation failure, not a Gameplay `ResolutionFault`.

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

Current trusted Phase 5/6 self-hosted regression evidence remains:

```text
Phase 5   13/13 PASS
Phase 6A  23/23 PASS
Phase 6B  12/12 PASS
Phase 6C   5/5  PASS
Total     53/53 PASS
```

The post-hardening PIE player → enemy → player cycle also passed with no `ResolutionFault` and with observer-visible QueueEmpty states remaining `PlayerTurnEnding` then `EnemyTurnEnding` before macro progression.

Phase 6R validation is complete through `.github/workflows/ue-phase6r-tests.yml`:

```text
SlayTheSpireDemoEditor + SlayTheSpireDemoTests build  PASS
Phase5–Phase6C full regression                         53/53 PASS
SlayTheSpireDemo Win64 Shipping build                  PASS
SlayTheSpireDemoTests / Phase6ATest Shipping exclusion PASS
```

Phase 6UI-A0 validation is complete through `.github/workflows/ue-phase6uia0-tests.yml` based on the owner-confirmed successful run after the final UE5.8 ticker-handle fix:

```text
SlayTheSpireDemoEditor build  PASS
Phase 5       13/13 PASS
Phase 6A      23/23 PASS
Phase 6B      12/12 PASS
Phase 6C       5/5  PASS
Phase 6UI-A0  20/20 PASS
Current run   73/73 PASS
```

The exact `73/73` total is evidence for that completed run, not a permanent architecture acceptance constant. The durable UI-A0 gate is: Editor build passes + all existing Phase5/6 regressions pass + all currently named UI-A0 invariants pass.

Latest Phase 6UI-A1 plus current UI-A3 dynamic-text validation is owner-confirmed after the Self-target Player-selection change:

```text
SlayTheSpireDemoEditor build  PASS
Phase 5       13/13 PASS
Phase 6A      23/23 PASS
Phase 6B      12/12 PASS
Phase 6C       5/5  PASS
Phase 6UI-A0  20/20 PASS
Phase 6UI-A1  11/11 PASS
Phase 6UI-A3   8/8  PASS
Current run   92/92 PASS

Manual PIE normal UI player → enemy → player loop  PASS
Manual PIE Self-target Defend → highlighted Player PASS
Packaged Defend dynamic {Block} description         PASS
```

The exact `92/92` total is evidence for the current validated run, not a permanent architecture acceptance constant. The durable UI-A1 gate remains: Editor build passes + existing regressions pass + current UI-A1 invariants pass + the concrete Battle HUD operates one normal battle loop through UI. UI-A3 is not complete as a whole; only the currently implemented dynamic-text slice is validated.

The self-hosted workflows remain manual, owner-only and restricted to trusted `main`.

Phase 6, Phase 6UI-A0 and UI-A1 are complete. The current UI-A3 dynamic-text slice is validated. The next implementation slice is `Phase 6UI-A2A — committed-presentation infrastructure`; do not start Damage animation before the A2A contracts in Section 16 pass their focused Automation gate.

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
Source/SlayTheSpireDemoTests/Private/Phase5RegressionTests.cpp
.github/workflows/ue-phase5-tests.yml
.github/workflows/runner-smoke-test.yml

Phase 6A
Events/BattleEvent.h
Events/BattleTrigger.h/.cpp
Events/BattleEventDispatcher.h/.cpp
Events/TurnEndStatusDecayTrigger.h/.cpp
Actions/ReduceStatusAction.h/.cpp
Actions/BattleActionQueue.h/.cpp
Source/SlayTheSpireDemoTests/Private/Phase6ARegressionTests.cpp
Source/SlayTheSpireDemoTests/Private/Phase6AExecutionOrderTests.cpp
Source/SlayTheSpireDemoTests/Private/Phase6ATestTypes.h/.cpp
.github/workflows/ue-phase6a-tests.yml

Phase 6B
Actions/TurnEndedAction.h/.cpp
Battle/BattleManager.h/.cpp
Actions/BattleActionQueue.h/.cpp
Source/SlayTheSpireDemoTests/Private/Phase6BRegressionTests.cpp
.github/workflows/ue-phase6b-tests.yml

Phase 6C
Events/BattleEvent.h
Actions/ShuffleDeckAction.h/.cpp
Actions/DrawCardAction.h/.cpp
Cards/CardPlayContext.h
Cards/Effects/DrawCardEffect.cpp
Actions/PlayCardAction.h/.cpp
Battle/BattleManager.h
Source/SlayTheSpireDemoTests/Private/Phase6CRegressionTests.cpp
.github/workflows/ue-phase6c-tests.yml
docs/Phase6CImplementation.md

Phase 6R
Source/SlayTheSpireDemoTests/SlayTheSpireDemoTests.Build.cs
Source/SlayTheSpireDemoTests/Private/SlayTheSpireDemoTests.cpp
SlayTheSpireDemo.uproject
Source/SlayTheSpireDemoEditor.Target.cs
Events/BattleTrigger.h
.github/workflows/ue-phase6r-tests.yml
docs/Phase6RImplementation.md
docs/Phase6DeferredEngineering.md

Phase 6UI-A0
Battle/BattleRequestTypes.h
Battle/BattleReadSnapshot.h
Battle/BattleManager.h/.cpp
Battle/BattleManagerUIA0ReadState.cpp
Enemy/EnemyIntent.h
Deck/DeckRuntime.h/.cpp
Actions/BattleActionQueue.h/.cpp
Events/BattleEventDispatcher.h/.cpp
Source/SlayTheSpireDemoTests/Private/Phase6UIA0RegressionTests.cpp
Source/SlayTheSpireDemoTests/Private/Phase6UIA0ReviewRegressionTests.cpp
Source/SlayTheSpireDemoTests/Private/Phase6UIA0TestTypes.h/.cpp
.github/workflows/ue-phase6uia0-tests.yml
docs/Phase6UIA0Implementation.md

Phase 6UI-A1 source boundary
UI/BattleHUDTypes.h
UI/BattleHUDViewModel.h/.cpp
UI/BattleHUDWidgetBase.h/.cpp
UI/BattleHUDCombatantPresentationWidgetBase.h/.cpp
UI/BattleHUDPresenter.h/.cpp
Source/SlayTheSpireDemoTests/Private/Phase6UIA1*.cpp/.h
docs/Phase6UIA1Implementation.md
docs/Phase6UIA1CombatantInspectionSetup.md
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
- Phase 6C — PASSED: typed DeckShuffled post-commit event, shuffle reaction-before-retry ordering, Phase6C 5/5 and total Phase5–Phase6C 53/53 passed through the owner-only UE5.8 workflow.
- Phase 6R — PASSED: Automation-only tests/reflected helpers extracted into Editor-only `SlayTheSpireDemoTests`; Editor build + full 53/53 regression + Shipping build/exclusion gate passed.
- Phase 6 — PASSED: Battle Event/Trigger scope and the Phase 6R regression/test-module isolation gate are complete.
- Phase 6UI-A0 — PASSED: authoritative turn/Hand lifecycle, deterministic initial battle shuffle, formal Query/Request APIs with shared revalidation, committed Enemy Intent, current-state gameplay-derived Intent display value, coherent `(BattleId, StateRevision)` snapshots and non-reentrant battle-level `OnReadStateReady`; owner-only UE5.8 workflow passed UI-A0 20/20 with current total 73/73.
- Phase 6UI-A1 — PASSED: current character-bound target interaction is revalidated; Self-target cards expose the gameplay-provided Player through public `LegalTargets`, Defend resolves by clicking the highlighted Player, UI-A1 11/11 passes, and the manual PIE loop passes.
- Phase 6UI-A2 — NEXT / DESIGN LOCKED: `Frozen FinalSnapshot + typed CommitResult/MutationResult + Gameplay Begin/Seal + immutable Resolution Envelope + bounded Controller queue`; UI-A2A infrastructure must land before visible Damage animation. Detailed contract: `docs/Phase6UIA2Implementation.md` and Section 16.
- Phase 6UI-A3 — PARTIAL / CURRENT SLICE PASSED: dynamic Damage/Block card text and runtime Status descriptions are implemented, DataAsset-authored and UE5.8/PIE/package revalidated; UI-A3 8/8 passes, while remaining Energy-result and target-specific exact preview are still planned.
- Phase 6UI-A — IN PROGRESS: the operable-loop acceptance is satisfied by UI-A1, but UI-A2 and the remaining UI-A3 scope are still planned before closing the phase.
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

This section is the detailed long-term specification for work after the now-complete Phase 6R. Its order must remain synchronized with Sections 2, 3 and 12; it must not be used to override stale contradictory progress text elsewhere in this file.

Phase 6UI-A0 and UI-A1 are complete. The current UI-A3 dynamic-text slice is validated. Phase 6UI-A2A is now the next implementation slice. Section 16 contains the locked A2 architecture summary; `docs/Phase6UIA2Implementation.md` is the detailed implementation contract. Do not skip to Phase 7 unless the user explicitly changes the development order.

### 15.1 Post-Phase-6 development order

```text
Phase 6R                              COMPLETE
    test-module extraction
    full Phase 5/6 regression
    Shipping/package-oriented validation
↓
Phase 6UI-A                           IN PROGRESS
    UI-A0 playable gameplay boundary COMPLETE
    UI-A1 operable Battle HUD        COMPLETE
    UI-A2 basic committed presentation NEXT / DESIGN LOCKED
    UI-A3 deterministic immediate preview — dynamic-text slice validated; remaining target/energy preview planned
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

### 15.2 Phase 6UI-A0 — Playable Gameplay Boundary — COMPLETE

UI-A0 established the non-debug gameplay/read boundary required before formal Battle Widgets begin.

Implemented and validated:

```text
Playable Turn / Hand Lifecycle
Formal gameplay Request APIs
Shared gameplay-owned validation
Advisory playability queries + authoritative Request revalidation
Read-only coherent UI state/query boundary
Minimal authoritative Enemy Intent
Deterministic initial DrawPile shuffle using battle RNG
Battle-level deferred OnReadStateReady completion boundary
```

The normal playable loop after UI-A0 does not require:

```text
TestDrawCard
TestPlayFirstCard
or equivalent rule-driving debug commands
```

#### PlayerTurn is gameplay request-eligible; Presentation may still lock the View

`PlayerTurn` means all authoritative turn-start gameplay work for that player turn has completed and normal gameplay Requests are eligible to be accepted.

It does **not** mean the View must immediately release input while historical presentation is still catching up.

UI-A0 introduced the explicit non-interactive `PlayerTurnStarting` boundary:

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

The existing `BattleStart` state serves as the equivalent non-interactive battle-opening boundary. Do not add or rename a separate `BattleStarting` state unless implementation later demonstrates a concrete need.

Opening battle resolves conceptually as:

```text
BattleStart
↓
initialize combatants / deck / battle-scoped state
↓
deterministically shuffle initial DrawPile with battle RNG
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

The initialization shuffle is setup randomization and intentionally emits no `DeckShuffled` event. Later successful discard-to-draw reshuffles continue to emit the event after commit.

#### Turn / Hand lifecycle is authoritative gameplay

UI must not compensate for missing gameplay lifecycle by directly drawing, discarding or retaining cards.

Current first playable rules are explicit/configurable:

```text
OpeningHandDrawCount = 5
PlayerTurnDrawCount  = 5
PlayerTurnEnd        = discard all remaining Hand cards
```

These are initial content rules, not permanent architecture constants.

Gameplay owns:

```text
BattleStart opening Hand setup
PlayerTurnStarting / PlayerTurnStart draw
PlayerTurn card play
PlayerTurnEnd remaining-Hand handling
transition to EnemyTurn
next PlayerTurn draw
```

Durable rule:

```text
Widget code never creates the Hand lifecycle.
Gameplay owns it.
```

#### Hand cleanup timing relative to TurnEnded

The established playable semantics use this ordering:

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

Therefore `FTurnEndedEvent(Player)` describes a player turn whose current-version remaining-Hand cleanup has already committed.

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

Public completion publication has an additional hard timing rule:

```text
public Request returns AcceptedForResolution
↓
only later may OnReadStateReady(BattleId, StateRevision) publish
```

The current implementation schedules Ready through the CoreTicker after the Queue fully settles. A ViewModel may therefore safely enter `Resolving` after seeing `AcceptedForResolution` and leave it after the later Ready/snapshot refresh without a re-entrant completion race.

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

Current concrete failure reasons include the UI-A0 needs such as:

```text
InvalidBattle
BattleEnded
ResolutionFaulted
WrongTurn
ResolutionBusy
InvalidCard
CardNoLongerInHand
NotEnoughEnergy
InvalidTarget
QueueRejected
```

Do not grow this into a speculative giant enum; add reasons when a concrete gameplay/UI need appears.

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

The implemented UI-A0 snapshot contains the current battle/read needs including:

```text
BattleId
StateRevision
BattleState
Player / Enemy combatant views
Energy
Hand / pile views and counts
Status views
committed Enemy Intent
current-state player-facing Intent damage value
```

Durable invariant:

```text
one UI refresh observes one coherent authoritative state revision
```

Any playability/legal-target information remains advisory and revision-bound; formal Requests revalidate against current authoritative gameplay state.

UI presentation caches may exist for rendering convenience but are not authoritative. Widgets must not reach into private DeckRuntime arrays, maintain a competing fake Hand, infer card zones from animation position or scan the world for gameplay actors.

`GetFirstHandCard()`-style debug/helper access is not sufficient as the formal UI read model. Expose only the minimum safe read-only Hand/pile information that gameplay rules allow the player to inspect; do not expose mutable pile containers merely for Widget convenience.

#### Read snapshot revision identity

Snapshots are identified by the coherent gameplay key:

```text
BattleId
StateRevision
```

Do not use frame number or Widget refresh time as authoritative gameplay revision identity. Publication de-duplication must compare the full `(BattleId, StateRevision)` key so a new battle is not suppressed merely because its revision number repeats a prior battle's value.

#### Read consumer initialization is subscribe-then-pull

`OnReadStateReady` is a non-replaying edge notification. A ViewModel or other read consumer may attach after the initial battle-ready notification has already published. Its mandatory initialization order is:

```text
subscribe to OnReadStateReady
↓
immediately request the current coherent player-facing snapshot
↓
render that snapshot when readable
↓
use later notifications to refresh future revisions
```

Never initialize the HUD by waiting only for the next event. Subscribe before the initial pull so a state change cannot occur between pulling and registering the listener.

`BattleActionQueue` publishes Queue-level settled/fault facts only. It must not discover or call `ABattleManager` through its UObject `Outer`. BattleManager owns the mapping from explicitly subscribed Queue signals to the deferred public `OnReadStateReady` boundary and must detach from a replaced Queue when a new battle scope begins.

Healthy Ready and fault Ready are separate paths: a faulted Queue is never reported as healthy idle, but after BattleManager commits `BattleState = ResolutionFaulted`, the public Ready path still publishes a readable fault snapshot so UI cannot remain stuck in `Resolving`.

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

UI-A0 currently supports the minimal committed Attack Intent and obeys the same source-of-truth rule.

Player-facing snapshots may additionally expose:

```text
EnemyIntentPlayerFacing.CurrentResolvedDamageAmount
```

For an Attack Intent this value is computed by building a read-only `FDamageSpec` from the committed BaseAmount and running the same Damage Modifier Pipeline against the **current snapshot state**. Widget/ViewModel code must not reimplement Strength, Weak, Vulnerable or other damage rules.

The semantic boundary is strict:

```text
CurrentResolvedDamageAmount
= current-state gameplay-derived value at this snapshot revision
≠ guaranteed future EnemyTurn damage
```

Mandatory TurnEnded reactions may alter statuses before the enemy actually executes. If a future feature requires a guaranteed execution-time prediction, it must model the mandatory pre-execution gameplay transitions through a dedicated gameplay-owned predictor rather than hard-code decay rules in UI.

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

Before UI-A2 exists, UI-A1 uses the UI-A0 stable-read boundary:

```text
Request returns AcceptedForResolution
↓
UI enters Resolving
↓
Gameplay / Trigger / turn macro work fully settles
↓
BattleManager schedules public stable-read publication
↓
OnReadStateReady(BattleId, StateRevision)
↓
ViewModel pulls one coherent player-facing snapshot
↓
presentation catch-up = immediate / no-op
↓
refresh HUD
↓
release UI input only when authoritative gameplay state is Request-eligible
```

Therefore UI-A1 may operate with instantaneous committed-state changes. It does not depend on Presentation Records or a Presentation Queue; UI-A2 later replaces the no-op catch-up boundary with historical committed-fact playback.

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

Phase 6UI-A intentionally uses a simple explicit two-stage card interaction to validate selection, cancellation, legal target handling, Request submission and Resolving lock.

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
→ query gameplay legal targets
→ expose/select a legal enemy candidate
→ RequestPlayCard(Card, selected enemy)
→ submission-time authoritative revalidation

Self-target card
select card
→ query gameplay legal targets
→ expose the gameplay-provided Player candidate through public `LegalTargets`
→ ChoosingTarget
→ select the Player presentation
→ RequestPlayCard(Card, selected Player)
→ submission-time authoritative revalidation

No-target card
select card
→ ReadyToConfirm
→ confirm
→ RequestPlayCard(Card, nullptr)
→ submission-time authoritative revalidation
```

Public `LegalTargets` contain gameplay-provided candidates the player actually chooses, including the Player for a Self-target card and enemies for an Enemy-target card. Widget code must match candidates by presentation identity and must not hard-code Player/Enemy target legality. A public target entry remains advisory presentation state rather than permanent authorization: `RequestPlayCard` always re-runs the authoritative gameplay validator at submission time.

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

Phase 6UI-A1 covers the minimum coherent playable surface:

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

Combatant `PresentationId` is a presentation-mapping key used to associate a
visible combatant presentation with a gameplay-provided legal-target view. It is
not authoritative gameplay identity, a legal-target capability token or an
ordering key. The View must still submit the current gameplay-provided `TargetId`,
and the formal Request must revalidate it.

Phase 6UI-A may initially contain one Enemy, but UI architecture must not permanently assume a single fixed Enemy slot. Conceptually prefer:

```text
EnemyArea
└── EnemyPresentation[]
```

Target selection should operate on a legal target set rather than hard-coding `BattleManager.Enemy`.

The first layout does not need final responsive polish, but data/API assumptions must not require rewriting gameplay/UI ownership when multiple enemies, many Status effects, many Relics, near-maximum Hand size or different aspect ratios arrive.

`ResolutionFaulted` must have visible development-facing presentation. A fault must not appear merely as buttons no longer responding or the battle silently freezing.

### 15.14 UI-A implementation slices

#### UI-A0 — Playable Gameplay Boundary — COMPLETE

Acceptance requires all of the following, all now validated:

```text
opening Hand comes from authoritative gameplay lifecycle
initial DrawPile uses deterministic battle-RNG setup shuffle without DeckShuffled event
PlayerTurn is entered only after turn-start gameplay work finishes
PlayerTurn transition does not depend on presentation completion
normal player card play uses formal RequestPlayCard
normal end turn uses formal RequestEndPlayerTurn
Query and Request share gameplay-owned validation rules
Request revalidates current state
accepted Requests mean AcceptedForResolution, not completed effects
public ReadStateReady cannot fire before an accepted Request returns
read consumers initialize subscribe-then-pull
remaining-Hand cleanup timing relative to TurnEnded is explicit
Enemy Intent is authoritative and drives corresponding EnemyTurn Actions
current player-facing Intent damage value uses shared pipeline but is current-state, not guaranteed-future semantics
UI can obtain one coherent (BattleId, StateRevision) read snapshot
ResolutionFaulted publishes a readable battle-level Ready snapshot
normal playable flow does not require TestDrawCard or TestPlayFirstCard
UI-A0 does not require Presentation Records / Presentation Queue
```

Before UI-A2 exists, every `presentation catch-up` label below means an immediate/no-op presentation boundary following the battle-level stable-read publication and coherent snapshot refresh.

Authoritative playable flow:

```text
BattleStart
↓
initialize battle scope
↓
deterministic initial DrawPile shuffle using battle RNG
↓
commit initial Enemy Intent
↓
opening-Hand gameplay batch
↓
turn-opening gameplay resolution completes
↓
BattleState = PlayerTurn
↓
Queue fully settles
↓
BattleManager schedules stable-read publication
↓
OnReadStateReady(BattleId, StateRevision)
↓
coherent player-facing snapshot refresh
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
Request returns to caller
↓
Resolving presentation/input policy
↓
Gameplay Actions / Events / Triggers / macro work settle
↓
authoritative BattleState/result is current
↓
OnReadStateReady(BattleId, StateRevision)
↓
coherent snapshot refresh
↓
presentation catch-up (no-op before UI-A2)
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
Queue fully settles
↓
OnReadStateReady(BattleId, StateRevision)
↓
coherent snapshot refresh
↓
UI releases input
```

Validation evidence for the completed UI-A0 run:

```text
SlayTheSpireDemoEditor build  PASS
Phase5       13/13
Phase6A      23/23
Phase6B      12/12
Phase6C       5/5
Phase6UIA0   20/20
current run  73/73
```

The numeric total is run evidence, not the permanent acceptance definition.

#### UI-A1 — Operable Battle HUD — COMPLETE

UI-A1 is validated against the UI-A0 gameplay/read boundary.

Validated scope:

```text
HP / Block / Energy / Status
Hand / pile views
Enemy Intent
End Turn
card selection / cancel / target selection
playability / rejection feedback
Resolving lock
Victory / Defeat / ResolutionFaulted
combatant inspection / PresentationId target mapping
```

Acceptance is restored for the current Self-target Player-selection policy. The UI-A1 source gate passes and PIE validates Defend by selecting the card, entering `ChoosingTarget`, clicking the highlighted Player presentation, gaining Block, spending Energy and resolving the card destination. A normal player → enemy → player UI loop also passes. Until UI-A2, committed state changes still appear immediately after coherent snapshot refresh rather than through historical animation playback.

Latest combined validation evidence:

```text
SlayTheSpireDemoEditor build  PASS
Phase5       13/13
Phase6A      23/23
Phase6B      12/12
Phase6C       5/5
Phase6UIA0   20/20
Phase6UIA1   11/11
Phase6UIA3    8/8
current run  92/92
PIE normal UI loop PASS
PIE Self-target Defend → Player selection PASS
packaged Defend dynamic Block text PASS
```

#### UI-A2 — Basic Committed Presentation — NEXT / DESIGN LOCKED

Do not start visible Damage/Block animation before UI-A2A establishes the locked transport/state contracts in Section 16 and `docs/Phase6UIA2Implementation.md`.

Implementation order:

```text
UI-A2A
Resolution lifecycle + frozen presentation state + immutable Envelope + coordinator/controller catch-up
↓
UI-A2B
Damage + Block committed presentation
↓
UI-A2C
CardPlayed + CardZoneChanged + Draw/Discard/Exhaust/Shuffle
↓
UI-A2D
Status changes + Victory/Defeat/ResolutionFault
```

The first player-facing record scope remains:

```text
Damage
Block change
Card played / card zone change
Card draw
Discard / Exhaust / Removed
Shuffle
Status amount change
Victory
Defeat
ResolutionFault
```

Goal is gameplay legibility, not final animation quality. Historical presentation is driven by committed facts and the exact frozen final state for that Resolution; it must never reconstruct history by comparing a later mutable state. Gameplay/BattleState timing remains independent of presentation playback.

#### UI-A3 — Deterministic Immediate Preview

Initial preview scope:

```text
immediate Damage
immediate Block
Energy cost / immediate Energy result
```

The first dynamic-text source slice is implemented and validated:

```text
Card/Status Description format
→ Effect/Modifier named read values
→ read-only Damage/Block pipeline where applicable
→ player-facing snapshot at one StateRevision
→ ViewModel/UMG final FText
```

Current dynamic-text slice status is `VALIDATED / UE5.8 UI-A3 8/8 + DATAASSET + PIE/PACKAGE REVALIDATED`.
Do not mark UI-A3 complete as a whole yet. Enemy-target card faces still show the source-side baseline; target-specific exact results and immediate Energy-result preview remain future UI-A3 work.

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

---

## 16. Locked Phase 6UI-A2 Architecture Summary

This section is the compact agent-facing contract for UI-A2. The detailed design lives in `docs/Phase6UIA2Implementation.md`. If this section and an implementation shortcut conflict, this section wins unless the user explicitly revises the design.

### 16.1 Required closed loop

```text
Request / System operation
↓
BeginResolution(Origin)
↓
BattleActionQueue
↓
Gameplay Commit
↓
typed CommitResult / MutationResult
↓
Action or BattleManager adds Source / Reason / Resolution context
↓
append deterministic Presentation Record
↓
Gameplay continues independently
↓
macro flow fully stabilizes
↓
Build exact raw read snapshot
↓
Freeze exact FPresentationStateSnapshot
↓
Seal immutable FPresentationResolutionEnvelope
↓
Presentation Coordinator / Controller
↓
bounded Envelope queue
↓
play Records in PresentationSequence order
↓
Finished / Skip / timeout / fail-safe
↓
Apply that Envelope.FinalSnapshot
↓
when caught up to newest BattleId/Revision, refresh live input bindings
↓
unlock input only if authoritative gameplay is request-eligible
```

`BattleActionQueue` must never wait for presentation playback.

### 16.2 Frozen display state is the one display model

Maintain one frozen player-facing display model rather than three competing copies:

```text
FBattleReadSnapshot
= current Gameplay-facing raw read state
= may contain weak runtime references

FPresentationStateSnapshot
= completely frozen player-facing display values for one exact revision
= used by both Envelope.FinalSnapshot and the immediate latest HUD baseline

BattleHUDViewModel
= copies/applies FPresentationStateSnapshot
```

`FPresentationStateSnapshot` may keep immutable presentation assets such as `UTexture2D*`, but must not retain mutable Gameplay runtime identities such as:

```text
UCardInstance
UStatusInstance
ACombatant
ABattleManager
```

Applying a frozen snapshot must not call current Gameplay APIs to rebuild card metadata, status text, playability or target presentation.

### 16.3 Historical display and live input bindings are separate

The frozen display model intentionally cannot replace current Request object identity. Keep a narrow live interaction-binding cache for only the newest readable revision:

```text
Card RuntimeId → TWeakObjectPtr<UCardInstance>
TargetId       → TWeakObjectPtr<ACombatant>
```

Rules:

```text
playing historical Envelope
→ input locked
→ do not query or use runtime bindings

controller catches up to newest (BattleId, StateRevision)
→ apply newest Frozen Snapshot
→ rebuild bindings through current formal read/query boundary
→ verify BattleId/Revision still match
→ only then unlock input
```

Do not expand UI-A2A into a new `RequestPlayCardByRuntimeId()` API unless a later concrete need justifies that gameplay API change.

### 16.4 One display-state owner

`OnReadStateReady` remains a non-replaying edge meaning that a stable current state can be read. It is not a historical payload and does not guarantee that a later pull still returns the Revision named in an earlier callback.

When presentation is enabled:

```text
raw stable state
↓
freeze exact FPresentationStateSnapshot
↓
Seal active Envelope
↓
hand Envelope to Presentation Coordinator / Controller
↓
ordinary OnReadStateReady observers may still be notified
```

But `UBattleHUDViewModel::HandleReadStateReady()` must no longer independently pull/apply a newer live state around an active PresentationController. The Presenter/Coordinator owns HUD display sequencing.

When presentation is disabled or no Controller is present, the same frozen latest baseline may be applied immediately. Gameplay must remain fully functional without a presentation consumer.

### 16.5 Resolution lifecycle and fault exception

Resolution must exist before any action can synchronously execute or any framework preparation step can fault after gameplay validation succeeds.

Normal validation rejection:

```text
validation rejected
→ no Presentation Resolution
```

After validation succeeds:

```text
Begin builder
↓
prepare / build / enqueue
├── success → normal processing
├── framework fault → keep/create active fault/system Resolution, append ResolutionFault, Seal
└── truly side-effect-free non-framework failure → Abort builder only when no committed fact/fault exists
```

`BattleStart` begins its Resolution before any opening-hand setup operation that can produce a framework fault.

Initial Origins are deliberately small:

```text
BattleStart
PlayCard
EndTurn
System
```

Do not pre-add AI/Relic/Replay origins until a real caller needs them.

### 16.6 Immutable Envelope contract

A sealed Envelope conceptually owns:

```text
BattleId
ResolutionId
Origin
FinalStateRevision
Records[]
FinalSnapshot : FPresentationStateSnapshot
```

Records and FinalSnapshot must be frozen as one unit. The Controller never receives a ResolutionId and then asks the Recorder what records currently exist.

The Recorder is only the active Resolution builder:

```text
Begin
→ Append
→ Append
→ Seal Envelope
→ clear builder
```

It is not a history database and must not require consumer acknowledgements for Gameplay correctness.

### 16.7 Controller owns bounded backlog and fail-safe playback

The PresentationController owns queued sealed Envelopes. Each Envelope applies its own FinalSnapshot after its records finish or are skipped.

The queue must be bounded. If presentation falls materially behind:

```text
collapse / skip obsolete presentation work
↓
apply newest available FinalSnapshot
↓
refresh only newest live input bindings
```

Never let presentation storage overflow request a Gameplay `ResolutionFault`.

Blueprint playback uses a tokenized completion contract:

```text
PlayPresentationRecord(Record, PlaybackToken)
↓
NotifyPresentationFinished(PlaybackToken)
```

Controller must ignore duplicate, stale, previous-Battle and post-Skip callbacks. Missing Blueprint handling, Widget destruction or timeout causes presentation catch-up/fallback only; it never changes authoritative Gameplay.

### 16.8 Typed Commit/Mutation result ownership

Gameplay Runtime owns mutation truth but not presentation context.

Required pattern:

```text
ACombatant / UDeckRuntime / UStatusContainer
→ perform authoritative mutation
→ return typed CommitResult / MutationResult

Action / BattleManager
→ owns Source, Reason and current Resolution context
→ convert successful result into Presentation Record
```

Do not inject Recorder dependencies into Gameplay runtime owners.

Examples:

```text
FDamageCommitResult
HPBefore / HPAfter
BlockBefore / BlockAfter
BlockedDamage
HPDamage

FBlockCommitResult
BlockBefore / BlockAfter
ChangedAmount

FCardZoneMutationResult
CardRuntimeId / CardId
FromZone / ToZone

FStatusMutationResult
StatusId / RuntimeSequence
AmountBefore / AmountAfter
bCreated / bRemoved
```

`ChangeReason` and Source belong to the Action/BattleManager record layer, not to StatusContainer/DeckRuntime's generic mutation truth.

### 16.9 Specific mutation boundaries

Damage and gained Block are recorded by the corresponding Actions after typed Combatant commit results return.

Current direct `ClearBlock()` calls remain direct in UI-A2B unless a real gameplay rule requires migrating them into Actions. Use the returned Block result as follows:

```text
Battle opening normalization ClearBlock
→ no player-visible Record

StartPlayerTurn / StartEnemyTurn ClearBlock
→ BlockChanged
→ Reason = TurnStartClear
```

Deck presentation must cover real zone transitions, not only `TryDiscardCard()`:

```text
Hand → PlayArea
PlayArea → Discard
PlayArea → Exhaust
PlayArea → Removed
DrawPile → Hand
Hand → Discard
```

Use a generic committed `CardZoneChanged` fact and map it to player-facing `CardPlayed`, `CardDrawn`, `CardDiscarded`, `CardExhausted` or `CardRemoved` as appropriate.

Successful gameplay shuffle is recorded by `ShuffleDeckAction` after commit and before `FDeckShuffledEvent` reactions. Initial setup shuffle remains non-presented.

Status `ApplyStatus`, exact-instance `ReduceStatus` and `RemoveStatusById` should return one mutation-result shape. Direct test calls do not auto-generate Presentation Records.

### 16.10 Unified resolved PresentationId

Do not create a PresentationId registry/validator subsystem.

Provide one Battle-layer resolver conceptually shaped like:

```cpp
bool ABattleManager::TryResolveCombatantPresentationId(
    const ACombatant* Combatant,
    FName& OutPresentationId
) const;
```

It resolves explicit authored IDs first and applies current battle-role fallback internally. Callers must not pass a `bPlayer` flag that can disagree with BattleManager identity.

Validate the resolved results, not raw authored values:

```text
all participating combatants resolve successfully
resolved ID is non-empty
resolved IDs are unique within the battle
```

Snapshot, LegalTargets and Presentation Records all use this one resolved value. Presentation IDs are treated as stable for the battle lifetime.

Invalid resolved identity causes `PresentationUnavailable`: HUD/presentation initialization fails visibly, normal player input remains disabled and a clear development-facing error is shown. It does not become Gameplay `ResolutionFaulted`. Recorder may remain disabled for that battle while headless Gameplay correctness remains intact.

### 16.11 UI-A2A Automation gate

Before UI-A2B visible Damage/Block playback begins, focused tests must cover at least:

```text
Accepted Request establishes Resolution before execution
ordinary validation rejection creates no Resolution
post-validation enqueue/framework fault seals a fault/system Resolution
BattleStart creates a Resolution before opening-hand fault-capable work
System Resolution can be created explicitly
empty-record Resolution can Seal safely
fault retains already-committed Records and appends ResolutionFault last
battle restart resets builder/identity without record leakage
late presentation subscriber does not replay a previous battle
no Controller / presentation disabled leaves Gameplay result unchanged
Frozen FinalSnapshot does not require mutable runtime reads when applied
historical Envelope cannot use live input bindings
input bindings refresh only at newest matching BattleId/Revision
OnReadStateReady cannot bypass active presentation sequencing
Controller backlog is bounded
stale/duplicate PlaybackToken completion is ignored
skip/missing callback/timeout/widget loss catch up without Gameplay fault
resolved PresentationId is shared by Snapshot, LegalTargets and Records
invalid resolved PresentationId enters visible PresentationUnavailable, not Gameplay ResolutionFaulted
```

Only after this infrastructure gate is green should UI-A2B begin visible Damage and Block playback.
