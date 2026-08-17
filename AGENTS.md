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
- [ ] Phase 5 Modifier-Based Framework / status system implemented.
  - [x] Phase 5A Status Runtime + ApplyStatusAction implemented and PIE-validated.
  - [x] Phase 5B1 Damage Spec + DamageFlatAdd + Strength implemented and PIE-validated.
  - [x] Phase 5B2 Damage Ratio + Weak + Vulnerable implemented and PIE-validated.
  - [ ] Phase 5C Block Spec + Dexterity + Frailty implemented.
- [ ] Phase 6 battle events / triggers implemented.
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
- `Weak#2 Amount=3` still applies exactly once (`Applications=1`), proving PresenceOnly does not repeat per Amount;
- `Vulnerable#1 Amount=2` also applies exactly once;
- status creation order was deliberately `Vulnerable#1 → Weak#2 → Strength#3`, while modifier execution correctly remained `Strength FlatAdd → Weak SourceMultiplier → Vulnerable TargetMultiplier`, proving Phase ordering outranks RuntimeSequence;
- each ratio modifier rounds immediately through integer arithmetic: `11 * 3 / 4 = 8`, then `8 * 3 / 2 = 12`;
- Pommel Strike Attack Base 9 resolves to 12 and commits 12 damage;
- with the same statuses active, Effect damage Base 9 collects zero modifiers and remains Resolved 9.

### Manual UE assets/configuration

Phase 4:

```text
Content/SlayTheSpireDemo/Data/Cards/Ironclad/Attacks/DA_Card_Strike
Content/SlayTheSpireDemo/Data/Cards/Ironclad/Attacks/DA_Card_PommelStrike
Content/SlayTheSpireDemo/Data/Cards/Ironclad/Skills/DA_Card_Defend
```

Phase 5A/5B1/5B2:

```text
Content/SlayTheSpireDemo/Data/Status/DA_Status_Strength
Content/SlayTheSpireDemo/Data/Status/DA_Status_Weak
Content/SlayTheSpireDemo/Data/Status/DA_Status_Vulnerable
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

### Phase 5 — Modifier-Based Framework and Status System

Build Phase 5 through vertical validation:

```text
5A  Status Runtime + ApplyStatusAction                         COMPLETE
5B1 FDamageSpec + DamageFlatAdd + Strength                    COMPLETE
5B2 DamageRatio + Weak + Vulnerable                           COMPLETE
5C  FBlockSpec + BlockFlatAdd + BlockRatio + Dexterity + Frailty  NEXT
```

Do not mark Phase 5 complete until source compiles and all corresponding UE5.8 PIE validations pass.

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
collect current status modifiers
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

#### Phase 5C — Block Spec + Dexterity + Frailty

Next scope:

```text
FBlockSpec
UBlockModifier
├── UBlockFlatAddModifier
└── UBlockRatioModifier
BlockModifierPipeline
```

Flow:

```text
GainBlockAction(BaseAmount)
↓ Execute
FBlockSpec
↓
BlockModifierPipeline
↓
ResolvedAmount
↓
GainBlock(ResolvedAmount)
```

Keep the existing `GainBlockAction(Source, Target, BaseAmount)` API. For Block recipient modifiers, `Target` is the recipient.

Dexterity/Frailty are collected from `Target` unless a future modifier explicitly declares another scope.

```text
Dexterity
└── BlockFlatAdd
    Scope = Target
    Phase = FlatAdd
    Value = +1
    AmountMode = ScaleWithAmount

Frailty
└── BlockRatio
    Scope = Target
    Phase = Multiplier
    Numerator = 3
    Denominator = 4
    AmountMode = PresenceOnly
```

Validation:

```text
Defend Base 5
Dexterity Amount=2 → 7
Frailty 3/4 → 5
ResolvedAmount=5
```

CardData, CardEffect, PlayCardAction and DeckRuntime should not require architecture changes for Phase 5 status modifiers.

#### Phase 5 exclusions

Do not implement these during Phase 5 without a concrete requirement:

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

Keyword presentation is intentionally deferred. Phase 5 establishes gameplay semantics for statuses such as Strength/Weak/Vulnerable/Dexterity/Frailty, but it must not make `Keyword` an alias for `UStatusData` or add UI keyword infrastructure merely because these mechanics have player-facing names.

### Phase 6 — Battle Events and Triggers

Introduce explicit post-commit events such as battle start, turn start/end, card played/drawn/exhausted, deck shuffled, damage events and enemy killed. Listeners normally enqueue actions rather than synchronously mutating unrelated state.

### Phase 7 — Relics

Implement relic listeners through the event/trigger architecture. First validation: Sundial; optional Abacus.

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

Instanced definition subobjects such as CardEffects/Modifiers are shared configuration and must be logically const/stateless.

### 4.13 Resolve future-state-dependent results at Execute-time

Enqueue-time captures stable intent/base inputs. Mutable-state-dependent values resolve when the action executes. Snapshot semantics must be explicit and mechanic-specific.

### 4.14 Action validation must fail soft and remain action-specific

Invalid execution dependencies must log when useful, call `Finish()`, and never wedge the queue. Do not impose a universal dead-target rule in the base action.

### 4.15 Card destination is resolved, not hard-coded

Card cleanup resolves destination at Execute-time and delegates authoritative zone movement to DeckRuntime.

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

Damage domain phases currently reserve:

```text
FlatAdd
SourceMultiplier
TargetMultiplier
FinalModifier
Override
Clamp
```

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

Example:

```text
11
→ Weak 3/4 = 8
→ Vulnerable 3/2 = 12
```

---

## 6. UE5 C++ Conventions

Use normal Unreal prefixes: `A`, `U`, `F`, `E`, `I`, and `b` for booleans.

Target source areas as needed:

```text
Battle/ Combat/ Actions/ Cards/ Deck/ Status/ Modifiers/ Relics/ Events/ Enemy/ UI/ Keywords/
```

Do not create empty folders just to reserve future architecture. `Keywords/` is a future presentation-oriented source area and should be created only when keyword/card-text presentation work actually begins.

Prefer forward declarations and small public headers. UObject runtime ownership must be GC-safe through clear Outer/`UPROPERTY`/`TObjectPtr` references. Do not enable Tick by default.

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
Strength / Weak / Vulnerable
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
DamageAction
→ FDamageSpec
→ DamageModifierPipeline
→ ResolvedAmount
→ Commit

UI Preview:
CardTextResolver
→ preview FDamageSpec
→ same read-only DamageModifierPipeline rules
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
13. Do not implement Phase 6 status decay, battle-event listeners or relic triggers during Phase 5 merely to make Weak/Vulnerable/Frailty expire.
14. Do not introduce a universal modifier context or GameplayTag-based damage taxonomy during Phase 5 without a concrete implemented need.
15. Do not implement KeywordLibrary, CardTextFormatter, RichText parsing, keyword tooltips/styles or dynamic card-value preview during Phase 5 merely because status mechanics now have player-facing keyword names.
16. Never model `Keyword = StatusData`; keyword presentation metadata must remain separate from the Status/Action/Modifier/Trigger/DeckRule that implements the gameplay mechanic.

Prefer clear architecture over clever abstractions.

---

## 9. Build and Verification Rules

After C++ changes:

- verify includes/module dependencies;
- build `SlayTheSpireDemoEditor` when a build environment is available;
- report build errors instead of masking them;
- never claim successful UE build/PIE without actually running it;
- if source tooling cannot run UE, require user-side compile/PIE before marking a phase complete.

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
```

`ABattleManager` currently owns the battle-scoped ActionQueue, DeckRuntime and temporary RuntimeSequence allocator. Each `ACombatant` owns its StatusContainer.

---

## 12. Acceptance Summary

- Phase 1 — PASSED: minimal battle loop.
- Phase 2 — PASSED: deterministic ActionQueue and queued combat.
- Phase 3 — PASSED: deterministic deck state and queued shuffle/retry.
- Phase 4 — PASSED: data-driven CardData/CardInstance/effect composition and complete card-play queue chain.
- Phase 5A — PASSED: queued status application, authoritative merge/create, Amount semantics and deterministic battle-wide RuntimeSequence behavior.
- Phase 5B1 — PASSED: Execute-time typed damage resolution, data-driven Strength FlatAdd and Attack-vs-Effect applicability filtering.
- Phase 5B2 — PASSED: integer DamageRatio resolution, PresenceOnly semantics, deterministic Phase ordering and Weak/Vulnerable Attack filtering.
- Phase 5 — NOT YET PASSED: 5C remains.

---

## 13. Architecture Validation Principle

Complex interactions must emerge from generic rules.

Pommel Strike knows only its configured damage/draw effects. DeckRuntime knows only card zones/draw/shuffle. Sundial should eventually know only shuffle events. Damage statuses modify typed Damage specs through the Modifier Pipeline.

Player-facing keywords explain mechanics but do not own those mechanics. A Status keyword such as Vulnerable may map to Status + Modifier, while a card-mechanic keyword such as Exhaust may map to DeckRuntime/Card destination/Action logic. UI presentation must not force unrelated gameplay concepts into one Status hierarchy.

If a new card/relic/status requires editing many unrelated classes or scattering concrete status checks through battle code, stop and reconsider the architecture.

---

## 14. Documentation and Progress Updates

When completing a meaningful phase:

- update Current Repository State;
- record durable architecture invariants;
- record required manual UE assets/configuration;
- keep documentation synchronized with actual source/PIE state;
- do not fill this file with daily implementation trivia.
