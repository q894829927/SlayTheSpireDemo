# SlayTheSpireDemo Agent Instructions

## 1. Project Goal

This repository is an Unreal Engine 5.8 C++ learning/demo project inspired by the combat architecture of Slay the Spire.

The immediate goal is not to recreate the full game. Build a small, extensible, deterministic card-battle framework that can eventually support reusable card effects/actions, deck zones, buffs/debuffs, relic triggers, deterministic resolution, modifier pipelines, and event-driven interactions such as two upgraded Pommel Strikes + Sundial without hard-coding the combo.

Long-term architecture direction:

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

`BattleStateMachine` controls large-scale turn flow.

`BattleActionQueue` controls fine-grained gameplay execution order.

`Modifier Pipeline` controls deterministic pre-commit modification, interception, override and clamping.

`BattleEvent / Trigger` handles post-commit reactions that may generate additional actions.

---

## 2. Current Repository State

- [x] Unreal Engine 5.8 C++ project created.
- [x] Runtime module `SlayTheSpireDemo` exists.
- [x] GitHub repository connected.
- [x] Project-level agent constraints defined in this `AGENTS.md`.
- [x] Phase 1 minimal combat loop implemented and validated in UE5.8 PIE.
- [x] Phase 2 `BattleActionQueue` implemented and validated in UE5.8 PIE.
- [x] Phase 3 deck / hand / discard / exhaust system implemented and validated in UE5.8 PIE.
- [x] Phase 4 data-driven card system implemented and validated in UE5.8 PIE.
- [ ] Phase 5 Modifier-Based Framework / status system implemented.
- [ ] Phase 6 battle event / trigger system implemented.
- [ ] Phase 7 relic system implemented.
- [ ] Phase 8 Sundial + Pommel Strike architecture validation implemented.

### Phase 1 validation record

Validated manually in UE5.8 PIE:

- battle initializes player and enemy correctly;
- player starts with 3 energy;
- test attack costs 1 energy and deals 6 damage;
- attack is rejected at 0 energy;
- end turn enters enemy turn;
- enemy test attack deals 5 damage;
- next player turn restores energy to 3;
- block absorbs damage before HP;
- enemy reaching 0 HP enters `Victory`;
- player reaching 0 HP enters `Defeat`;
- commands are rejected after victory/defeat.

### Phase 2 validation record

Validated manually in UE5.8 PIE:

- player damage resolves through `UDamageAction` and `UBattleActionQueue`;
- `Back(7), Back(8), Front(6)` resolves exactly `6 → 7 → 8`;
- `QueueEmpty` occurs only after the queued batch resolves;
- `UGainBlockAction` grants block through the same queue;
- enemy damage resolves through the queue before the next player turn starts;
- block absorption, victory, defeat and post-battle command rejection remain correct.

### Phase 3 validation record

Validated manually in UE5.8 PIE before and after the Phase 4 runtime-card migration:

- `DrawPile` array end is the top of the pile;
- one `UDrawCardAction` represents one draw attempt;
- drawing with empty DrawPile + non-empty DiscardPile inserts `ShuffleDeckAction` and retry draw through the queue;
- because `AddToFront` is stack-like, retry is inserted first and shuffle second so execution is `Shuffle → RetryDraw`;
- the original draw, shuffle and retry form one queue chain with one final `QueueEmpty`;
- drawing with both DrawPile and DiscardPile empty terminates safely;
- discard targets stable runtime card identity rather than a hand-array position;
- seeded Fisher–Yates shuffle remains deterministic across repeated PIE runs;
- Phase 4 regression validated the same `Shuffle → RetryDraw` behavior using `UCardInstance` objects.

### Phase 4 validation record

Validated manually in UE5.8 PIE:

- `UDeckRuntime` initializes from DataAsset definitions and creates independent runtime `UCardInstance` objects;
- the same `DA_Card_Strike` definition creates distinct `Strike#1` and `Strike#2` runtime instances;
- stable debug labels use `CardId#RuntimeId` rather than localized display text;
- `DA_Card_PommelStrike` composes `DamageCardEffect(9)` followed by `DrawCardEffect(1)` without a Pommel-Strike-specific C++ class;
- playing Pommel Strike resolves `PlayCardAction → DamageAction → DrawCardAction → FinishCardPlayAction` in that order;
- Pommel Strike moves `Hand → PlayArea`, spends 1 Energy, deals 9 damage, draws Defend, then moves `PlayArea → DiscardPile`;
- the complete Pommel Strike play chain emits only one final `QueueEmpty`;
- `DA_Card_Defend` targets Self and produces `GainBlockAction(5)` through reusable effect composition;
- `DA_Card_Strike` targets Enemy and produces `DamageAction(6)`;
- Energy changes `3 → 2 → 1 → 0` across validated card plays;
- attempting to play `Strike#1` at 0 Energy is rejected before moving the card out of Hand or dealing damage;
- Phase 3 draw/discard/shuffle behavior remains valid after replacing `FDeckCardToken` with `UCardInstance`;
- runtime deck zones now include DrawPile, Hand, DiscardPile, ExhaustPile, PlayArea and a Removed zone for destination resolution.

Editor assets/configuration created manually for Phase 4:

```text
Content/SlayTheSpireDemo/Data/Cards/Ironclad/Attacks/DA_Card_Strike
Content/SlayTheSpireDemo/Data/Cards/Ironclad/Attacks/DA_Card_PommelStrike
Content/SlayTheSpireDemo/Data/Cards/Ironclad/Skills/DA_Card_Defend
```

Current temporary `L_BattleTest` Level Blueprint debug wiring:

```text
BeginPlay → StartBattle
Keyboard 1 → TestAttack
Space → EndPlayerTurn
Keyboard B → TestGainBlock
Keyboard Q → TestActionQueueOrder
Keyboard D → TestDrawCard
Keyboard X → TestDiscardCard
Keyboard P → TestPlayFirstCard
```

Do not treat these temporary key bindings as the future card input architecture.

---

## 3. Development Order

Do not skip ahead unless the user explicitly requests it.

### Phase 1 — Minimal Combat Loop — COMPLETE

Implemented:

- `ACombatant`;
- `ABattleManager`;
- HP / block / player Energy;
- battle state and turn flow;
- victory / defeat checks.

### Phase 2 — BattleActionQueue — COMPLETE

Implemented and PIE-validated:

- `UBattleAction`;
- `UBattleActionQueue`;
- `UDamageAction`;
- `UGainBlockAction`.

Durable invariants:

- one authoritative action executes at a time;
- ordering is explicit and deterministic;
- completion is explicit through `Finish()`;
- normal back insertion and explicit front insertion are supported;
- actions may finish synchronously while remaining compatible with future asynchronous presentation;
- actions may enqueue dependent follow-ups but must never pump/advance the queue themselves;
- related batches must be present before the current action finishes when one logical chain must preserve a single `QueueEmpty`.

### Phase 3 — Deck System — COMPLETE

Implemented and PIE-validated:

- `UDeckRuntime`;
- DrawPile;
- Hand;
- DiscardPile;
- ExhaustPile;
- `UDrawCardAction`;
- `UDiscardCardAction`;
- `UShuffleDeckAction`.

The temporary Phase 3 `FDeckCardToken` representation was intentionally replaced by `UCardInstance` in Phase 4.

Durable invariants:

- `UDeckRuntime` is authoritative; UI never owns pile truth;
- pile ordering is explicit and DrawPile end is the top;
- normal runtime card identity is stable object identity;
- one draw action is one draw attempt;
- draw does not synchronously shuffle itself;
- `ShuffleDeckAction` only shuffles;
- shuffle uses one battle-scoped `FRandomStream` initialized once and consumed across future shuffles;
- do not reinitialize RNG per shuffle;
- deterministic Fisher–Yates ordering is used;
- empty draw + empty discard terminates safely.

### Phase 4 — Data-Driven Cards — COMPLETE

Implemented and PIE-validated:

- `UCardData : UPrimaryDataAsset`;
- `UCardInstance`;
- stable `CardId` plus presentation `DisplayName`;
- `ECardType`;
- `ECardTargetType`;
- `ECardDestination`;
- BaseCost;
- instanced reusable `UCardEffect` definitions;
- `UDamageCardEffect`;
- `UGainBlockCardEffect`;
- `UDrawCardEffect`;
- `UPlayCardAction`;
- `UFinishCardPlayAction`;
- PlayArea and destination cleanup;
- definition-driven runtime deck initialization.

Required flow is established:

```text
UCardData
    ↓
UCardInstance
    ↓
UPlayCardAction
    ↓
UCardEffect::BuildActions() const
    ↓
DamageAction / GainBlockAction / DrawCardAction
    ↓
UFinishCardPlayAction
    ↓
resolved card destination
```

Normal cards should be configured from DataAssets and reusable effects. Avoid one C++/Blueprint class per ordinary card.

#### Phase 4 durable constraints

Effect subobjects owned by `UCardData` are shared definition objects. Treat them as immutable runtime configuration.

`UCardEffect::BuildActions(...)` is logically const/stateless. Never cache battle targets, calculated final damage, counters, or other runtime state in definition effects.

`BuildActions` captures stable intent/base inputs, not future-state-dependent final results. For example `DamageCardEffect(BaseAmount=6)` creates a `DamageAction` carrying Source, Target and BaseAmount=6; future Strength/Weak/Vulnerable resolution belongs at action Execute-time in Phase 5.

`FCardPlayContext` exposes only build dependencies. Effect definitions may receive a neutral `ActionOuter` to create actions but must not receive/control the full queue.

`UPlayCardAction` may temporarily depend on `ABattleManager` only through narrow battle-owned resource orchestration because Energy still belongs to `ABattleManager`. Card effects must never depend on BattleManager.

Player card-play requests are currently accepted only while the queue is idle, so Phase 4 appends effect actions with `AddToBack`; do not add an unused batch-front helper without a concrete nested use case.

`UPlayCardAction` must completely build and validate its dependent action list before committing it. All effect actions plus `UFinishCardPlayAction` must be queued before `UPlayCardAction::Finish()`.

`UCardInstance*` object identity is the normal runtime identity for deck movement. `RuntimeId` remains for deterministic logs and potential replay/serialization support.

`GetDebugLabel()` uses stable `CardId#RuntimeId`, e.g. `Strike#1`, and must not use localized `DisplayName` as stable identity.

Card-play cleanup must resolve destination at cleanup Execute-time rather than hard-code all cards to DiscardPile. The current destination model supports Discard, Exhaust and Removed; future rules may override definition defaults.

Every action validates its own Execute-time preconditions. Invalid dependencies fail soft, log when useful, call `Finish()`, and never leave the queue stuck. Do not create a universal base-class rule that all dead targets are invalid.

`FInstancedStruct` / `TInstancedStruct` remains only a future representation option if profiling, asset scale, cook size or editor workflow justifies it.

### Phase 5 — Modifier-Based Framework and Status System

Introduce the first production modifier pipelines and status-driven modifiers.

Expected scope:

- modifier interfaces/contracts;
- modifier domains;
- modifier phases;
- deterministic modifier ordering;
- typed operation specs;
- damage modifier pipeline;
- block modifier pipeline;
- status runtime integration.

Initial validation effects:

- Strength;
- Weak;
- Vulnerable;
- Dexterity;
- Frailty.

Possible later validation:

- Artifact;
- damage clamp effects;
- cost override effects.

Do not implement every gameplay mechanism as a Modifier. Statuses that react to completed events and create new gameplay belong to Phase 6 events/triggers.

### Phase 6 — Battle Events and Triggers

Introduce explicit events such as battle start, turn start/end, card played/drawn/exhausted, deck shuffled, damage events where semantically required, and enemy killed.

Listeners should normally enqueue actions instead of recursively mutating unrelated gameplay state.

### Phase 7 — Relics

Implement relic listeners through the battle-event/trigger architecture.

First validation relics:

- Sundial;
- optionally Abacus.

### Phase 8 — Combo Architecture Validation

Validate:

```text
Pommel Strike+
Pommel Strike+
Sundial
```

Forbidden:

```text
if player has two Pommel Strikes and Sundial:
    enable infinite combo
```

The interaction must emerge from generic card, draw, shuffle, event, modifier and action rules.

---

## 4. Core Architecture Rules

### 4.1 Deterministic gameplay

For the same initial state, input sequence and RNG seed, gameplay results should be reproducible.

Correctness must not depend on frame rate, animation timing, UObject addresses, unordered collection iteration or unordered listener execution.

### 4.2 Cards describe effects; they do not own battle rules

Preferred:

```text
Pommel Strike+
  - DamageEffect(10)
  - DrawEffect(2)
```

Avoid unrelated battle-system logic inside individual card implementations.

### 4.3 Gameplay mutations flow through actions after Phase 2

Direct authoritative mutation is suspicious outside the owning low-level system/action.

Preferred:

```text
request
→ BattleAction
→ BattleActionQueue
→ typed resolution / owning gameplay object
```

### 4.4 Events notify; actions mutate

Events announce facts. Listeners should normally enqueue actions rather than perform deep synchronous mutation chains.

### 4.5 Avoid recursive gameplay chains

Prefer queued resolution over synchronous chains such as draw → shuffle → trigger → gain energy → draw → ... .

### 4.6 Separate definitions from runtime instances

```text
CardData   != CardInstance
StatusData != StatusInstance
RelicData  != RelicInstance
```

Never mutate shared definition assets with temporary cost, stacks, counters or other runtime state.

### 4.7 Composition over card-specific inheritance

Prefer reusable effects/components/data composition. Avoid one class per ordinary card/effect combination.

### 4.8 UI is presentation, not authority

UMG may request gameplay actions and display state, but must not own HP, block, Energy, deck contents, status stacks, relic counters or turn state.

### 4.9 Keep dependencies explicit and minimal

Avoid repeated `GetAllActorsOfClass` / `GetActorOfClass` during gameplay. Core systems receive/store explicit references or narrow contexts.

### 4.10 No premature GAS migration

Understand and validate the card-battle-specific queue, deck, modifier and trigger model first.

### 4.11 Actions may schedule dependencies but do not drive the queue

Actions may receive the current queue explicitly when they genuinely need to create dependent actions, but they must not call `PumpQueue`, `ProcessNext` or otherwise advance execution themselves.

### 4.12 Shared definition objects are immutable at runtime

Objects inside definition assets, including instanced `UCardEffect` subobjects, may be shared by many runtime instances. Treat them as immutable configuration.

### 4.13 Resolve future-state-dependent results at Execute-time

Action creation/enqueue time captures stable intent and base inputs. Final values that depend on mutable battle state are normally resolved when the action executes.

Only use explicit snapshot semantics when a mechanic specifically requires a snapshot.

### 4.14 Action validation must fail soft and remain action-specific

Every action validates required dependencies when it executes. If preconditions fail, it must terminate cleanly with `Finish()` so the queue cannot become stuck.

Do not centralize a universal dead-target rejection in the base action class.

### 4.15 Card destination is resolved, not hard-coded

Card-play completion must resolve destination through card definition/runtime rules at cleanup Execute-time, then let `UDeckRuntime` perform the authoritative zone move.

---

## 5. Modifier-Based Framework Architecture

MBF does not replace the battle action queue.

```text
BattleActionQueue
→ when gameplay operations execute and in what order

Modifier Pipeline
→ pre-commit modification / interception / override / clamp

Battle Event / Trigger
→ post-commit reactions that may enqueue new BattleActions
```

### 5.1 Action vs Modifier vs Trigger

Use a `BattleAction` for authoritative operations such as Damage, GainBlock, Draw, Shuffle, GainEnergy and ApplyStatus.

Use a `Modifier` when an existing operation must be changed before commit, such as Strength, Weak, Vulnerable, Dexterity, Frailty, Artifact, damage caps, cost overrides or operation cancellation.

Use a `Trigger` when gameplay reacts to a completed fact, such as Sundial reacting to DeckShuffled or Thorns reacting to damage received.

Triggers should normally enqueue actions instead of mutating unrelated state directly.

### 5.2 Modifier ordering

Do not use one global priority integer.

Use:

```text
Domain
  ↓
Phase
  ↓
Priority
  ↓
StableOrder
```

Priority is meaningful only within the same Domain + Phase.

A future damage domain may resemble:

```text
Base
↓
FlatAdd
↓
SourceMultiplier
↓
TargetMultiplier
↓
FinalModifier
↓
Override
↓
Clamp
↓
Commit
```

Never rely on `TSet` iteration, UObject address, actor discovery order or unordered registration order for StableOrder.

### 5.3 Typed modifier pipelines

Prefer typed specs such as:

```text
FDamageSpec
FBlockSpec
FCardCostSpec
FStatusApplySpec
```

Avoid one universal context containing every possible mechanism.

Preferred lifecycle:

```text
Create operation spec
↓
Collect relevant modifiers
↓
Filter
↓
Sort by Domain / Phase / Priority / StableOrder
↓
Apply modifiers
↓
Check cancellation / override
↓
Commit
↓
Emit BattleEvent
↓
Triggers enqueue new BattleActions
```

### 5.4 Cancellation and interception

Pending operations may be cancelled before commit. Prefer intercept-before-commit to commit-then-undo.

Example:

```text
ApplyStatusAction
↓
FStatusApplySpec
↓
Artifact modifier
↓
bCancelled = true
↓
no status commit
```

### 5.5 MBF guardrail

When adding a buff/debuff/relic/rule, determine:

```text
1. Action, Modifier or Trigger?
2. If Modifier, what Domain?
3. Which Phase?
4. Does same-phase ordering matter?
5. Then assign Priority.
6. Define deterministic StableOrder.
```

Do not scatter concrete status checks through cards or `ABattleManager`.

---

## 6. UE5 C++ Conventions

Follow Unreal naming conventions: `A` Actor, `U` UObject/ActorComponent, `F` struct, `E` enum, `I` interface, and `b` prefix for booleans.

Target source organization as systems are introduced:

```text
Source/SlayTheSpireDemo/
├── Battle/
├── Combat/
├── Actions/
├── Cards/
├── Deck/
├── Status/
├── Modifiers/
├── Relics/
├── Events/
├── Enemy/
└── UI/
```

Do not create empty folders merely to reserve future architecture.

Prefer forward declarations, keep public headers small, and use include paths valid for the actual module layout.

Any UObject that must survive GC must have clear ownership and an appropriate `UPROPERTY` / `TObjectPtr` reference.

Do not enable Tick by default. Card-battle gameplay should primarily be event/action driven.

---

## 7. Blueprint and Asset Rules

Prefer C++ for battle state, action queue, combat rules, deck rules, modifier pipelines, event dispatch, status/relic runtime logic and validation logic.

Prefer Blueprint / UMG / DataAssets for visual assembly, widgets, artwork, simple presentation animation, content configuration and editor-created test assets.

All project-owned Unreal assets should live under:

```text
Content/SlayTheSpireDemo/
```

Recommended long-term tree:

```text
Content/SlayTheSpireDemo/
├── Maps/
├── Blueprints/
├── Data/
│   ├── Cards/
│   ├── Enemies/
│   ├── Status/
│   └── Relics/
├── UI/
├── Art/
├── Materials/
├── VFX/
├── Audio/
└── Dev/
```

Do not create all folders in advance.

Asset naming examples:

| Asset type | Prefix | Example |
|---|---|---|
| Blueprint Class | `BP_` | `BP_BattleManager` |
| Widget Blueprint | `WBP_` | `WBP_Card` |
| Data Asset | `DA_` | `DA_Card_Strike` |
| Level / Map | `L_` | `L_BattleTest` |
| Texture | `T_` | `T_Card_Strike` |
| Material | `M_` | `M_Card` |
| Material Instance | `MI_` | `MI_Card_Attack` |
| Niagara System | `NS_` | `NS_Hit` |
| Sound Wave | `S_` | `S_Card_Play` |
| Sound Cue | `SC_` | `SC_Attack` |

Prefer domain-qualified names such as `DA_Card_Strike`, `DA_Relic_Sundial`, `DA_Status_Strength`, `DA_Enemy_JawWorm`.

Keep gameplay rules, configuration and presentation distinct.

Text/source agents must not pretend to create or wire `.uasset` / `.umap` files without UE Editor access. Never hand-edit them.

Do not intentionally commit generated/local files such as:

```text
Binaries/
DerivedDataCache/
Intermediate/
Saved/
.vs/
*.sln
*.slnx
```

unless repository policy explicitly changes.

---

## 8. Change-Scope Rules for Agents

For every task:

1. Inspect relevant existing files first.
2. Make the smallest coherent change that completes the requested task.
3. Do not refactor unrelated code.
4. Do not rename public APIs/assets without need.
5. Do not silently change plugins, engine association or build target settings.
6. Do not add third-party dependencies without explicit approval.
7. Do not add multiplayer/network architecture unless requested.
8. Do not add map/shop/event/meta-progression before core combat architecture is validated unless requested.
9. Preserve the ability to explain each system from a learning perspective.
10. Do not skip development phases unless the user explicitly approves it.
11. Do not prematurely implement planned MBF/domain/phase enums before Phase 5 merely because they are documented.
12. Do not introduce `FInstancedStruct`, StructUtils or another card-effect representation dependency without a concrete requirement.

Prefer clear architecture and explainable code over clever abstractions.

---

## 9. Build and Verification Rules

After changing C++:

- verify include paths and module dependencies;
- build `SlayTheSpireDemoEditor` when a build environment is available;
- report build errors rather than masking them;
- never claim a successful UE build unless it was actually run;
- if source/text tooling cannot run UE, require user-side compilation/PIE verification before marking a phase complete.

When UE Editor validation is necessary, label instructions as `USER ACTION REQUIRED` and provide exact steps.

---

## 10. User-Action Boundary

The user performs UE Editor operations that cannot be safely represented by text-only repository edits, including:

- opening/rebuilding the project;
- creating Blueprint subclasses;
- placing actors in levels;
- assigning actor references in Details;
- creating/configuring DataAssets;
- creating/editing UMG widgets;
- visual Blueprint wiring;
- saving `.uasset` / `.umap` assets;
- PIE validation and returning logs/screenshots.

Whenever user action is required, instructions must include exact menu/path, class/asset name, property values, what to click/run, expected result and what to return if it differs.

---

## 11. Implemented Core Classes

### Phase 1

```text
Source/SlayTheSpireDemo/
├── Battle/BattleManager.h/.cpp
└── Combat/Combatant.h/.cpp
```

### Phase 2

```text
Source/SlayTheSpireDemo/Actions/
├── BattleAction.h/.cpp
├── BattleActionQueue.h/.cpp
├── DamageAction.h/.cpp
└── GainBlockAction.h/.cpp
```

### Phase 3

```text
Source/SlayTheSpireDemo/
├── Deck/DeckRuntime.h/.cpp
└── Actions/
    ├── DrawCardAction.h/.cpp
    ├── DiscardCardAction.h/.cpp
    └── ShuffleDeckAction.h/.cpp
```

### Phase 4

```text
Source/SlayTheSpireDemo/
├── Cards/
│   ├── CardTypes.h
│   ├── CardData.h/.cpp
│   ├── CardInstance.h/.cpp
│   ├── CardPlayContext.h
│   └── Effects/
│       ├── CardEffect.h
│       ├── DamageCardEffect.h/.cpp
│       ├── GainBlockCardEffect.h/.cpp
│       └── DrawCardEffect.h/.cpp
├── Actions/
│   ├── PlayCardAction.h/.cpp
│   └── FinishCardPlayAction.h/.cpp
└── Deck/DeckRuntime.h/.cpp
```

`ABattleManager` owns the battle-scoped `UBattleActionQueue` and `UDeckRuntime` in the current debug battle flow.

---

## 12. Acceptance Summary

### Phase 1 — PASSED

Minimal battle loop, HP, block, Energy, turn transition and terminal states validated in UE5.8 PIE.

### Phase 2 — PASSED

Action queue ordering, front/back insertion, explicit finish lifecycle, queued damage/block and queue-empty turn progression validated in UE5.8 PIE.

### Phase 3 — PASSED

Deck state, one-card draw actions, stable-identity discard, empty-deck safety, queued shuffle/retry behavior, one queue-empty completion and deterministic seeded shuffle behavior validated in UE5.8 PIE, including regression after Phase 4 migration.

### Phase 4 — PASSED

DataAsset-defined cards, independent runtime instances, reusable stateless effects, PlayArea lifecycle, Energy validation/spending, multi-effect action ordering, destination cleanup, stable debug identity and full card-play queue chaining validated in UE5.8 PIE.

---

## 13. Architecture Validation Principle

The project should eventually express complex interactions without special-case combo code.

Target example:

```text
Pommel Strike+ A
    ↓ draw 2
empty draw pile
    ↓
shuffle
    ↓ DeckShuffled event
Sundial counter
    ↓
draw
    ↓
second draw attempts against empty pile
    ↓
shuffle event again

Pommel Strike+ B
    ↓
repeat
```

Pommel Strike knows only its configured damage/draw effects.

The deck system knows only card zones, draw and shuffle rules.

Sundial knows only about shuffle events.

Damage-related statuses modify typed damage operations through the Modifier Pipeline rather than being hard-coded into cards.

If adding a card/relic/status requires editing many unrelated card classes or scattering concrete status checks through battle code, stop and reconsider the architecture.

---

## 14. Documentation and Progress Updates

When completing a meaningful phase:

- update `Current Repository State`;
- record durable architecture invariants introduced;
- list UE Editor assets created manually;
- keep documentation synchronized with actual code and validation status;
- do not fill this document with daily implementation trivia.
