# SlayTheSpireDemo Agent Instructions

## 1. Project Goal

This repository is an Unreal Engine 5.8 C++ learning/demo project inspired by the combat architecture of Slay the Spire.

The immediate goal is not to recreate the full game. Build a small, extensible, deterministic card-battle framework that can eventually support:

- reusable card effects and battle actions;
- draw / hand / discard / exhaust / shuffle piles;
- buffs, debuffs and relic triggers;
- deterministic resolution through an action queue;
- standardized modifier resolution for stackable and interceptable mechanics;
- event-driven interactions between cards, statuses, relics and enemies;
- emergent combos such as two upgraded Pommel Strikes + Sundial without hard-coding the combo.

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
- [ ] Phase 4 data-driven card system implemented.
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
- commands are rejected after victory/defeat because battle state is no longer `PlayerTurn`.

### Phase 2 validation record

Validated manually in UE5.8 PIE:

- player test attack creates and executes `UDamageAction` through `UBattleActionQueue`;
- `AddToBack(7)`, `AddToBack(8)`, then `AddToFront(6)` resolves in deterministic `6 → 7 → 8` order;
- queue-empty notification occurs only after the complete queued batch resolves;
- `UGainBlockAction` executes through the same queue and grants 4 block;
- enemy attack is queued as `UDamageAction` and resolves before the next player turn starts;
- block still absorbs damage correctly when damage is delivered through an action;
- victory/defeat are detected after queue resolution;
- commands remain rejected after a terminal battle state.

### Phase 3 validation record

Validated manually in UE5.8 PIE:

- `UDeckRuntime` initializes a deterministic three-card debug deck with `Card_A#1`, `Card_B#2`, `Card_C#3`;
- the end of `DrawPile` is the top of the pile;
- three draws resolve `Card_C#3 → Card_B#2 → Card_A#1` through `UDrawCardAction`;
- drawing when both draw and discard piles are empty finishes safely without retry loops;
- discard operations capture and resolve a stable `RuntimeId` instead of a transient hand index;
- three discards move the three runtime tokens from Hand to DiscardPile;
- an empty DrawPile with a non-empty DiscardPile inserts `ShuffleDeckAction` and a retry `DrawCardAction` through the existing queue;
- because `AddToFront` is stack-like, retry is inserted first and shuffle second so execution is `Shuffle → RetryDraw`;
- the original draw, shuffle and retry draw form one queue chain and produce only one final `QueueEmpty` notification;
- shuffle moves DiscardPile into DrawPile and uses a battle-scoped `FRandomStream` with deterministic Fisher–Yates ordering;
- two separate PIE runs using seed `1337` and the same input sequence produced identical shuffle/draw results;
- `ExhaustPile` exists as authoritative runtime state but no exhaust action is introduced before it is needed.

`L_BattleTest` Level Blueprint currently contains temporary debug wiring:

```text
BeginPlay → StartBattle
Keyboard 1 → TestAttack
Space → EndPlayerTurn
Keyboard B → TestGainBlock
Keyboard Q → TestActionQueueOrder
Keyboard D → TestDrawCard
Keyboard X → TestDiscardCard
```

Do not treat these temporary key bindings as the future card input architecture.

---

## 3. Development Order

Do not skip ahead unless the user explicitly requests it.

### Phase 1 — Minimal Combat Loop — COMPLETE

Implemented:

- `ACombatant`;
- `ABattleManager`;
- HP;
- block;
- player energy;
- battle state;
- test attack;
- end turn;
- enemy test attack;
- victory / defeat checks.

### Phase 2 — BattleActionQueue — COMPLETE

Implemented and PIE-validated:

- `UBattleAction`;
- `UBattleActionQueue`;
- `UDamageAction`;
- `UGainBlockAction`.

Durable Phase 2 invariants:

- one authoritative action executes at a time;
- queue ordering is explicit and deterministic;
- action completion is explicit through `Finish()`;
- normal back insertion and explicit front insertion are supported;
- `BattleManager` observes queue-empty completion before advancing enemy-turn flow;
- queue processing does not require Tick;
- synchronous actions may finish immediately while the API remains compatible with future asynchronous presentation/animation;
- related action batches are assembled before `StartProcessing()` so a premature `QueueEmpty` cannot occur between batch members.

`UDamageAction` and `UGainBlockAction` retain future modifier-resolution context:

```text
Source
Target
BaseAmount
```

### Phase 3 — Deck System — COMPLETE

Implemented and PIE-validated:

- `FDeckCardToken`;
- `UDeckRuntime`;
- DrawPile;
- Hand;
- DiscardPile;
- ExhaustPile;
- `UDrawCardAction`;
- `UDiscardCardAction`;
- `UShuffleDeckAction`.

Durable Phase 3 invariants:

- `UDeckRuntime` is the authoritative owner of pile state; UI must never own pile truth;
- all four runtime piles use explicit ordered storage;
- `DrawPile` array end is the top of the pile;
- Phase 3 uses lightweight runtime tokens only; `CardData` / `CardInstance` remain Phase 4 concerns;
- a runtime token has a stable runtime identity distinct from its debug/display name;
- discard actions target stable runtime identity, not hand-array position;
- one `UDrawCardAction` represents one draw attempt;
- if DrawPile is empty and DiscardPile is non-empty, draw does not directly shuffle; it inserts `ShuffleDeckAction` plus retry draw into `BattleActionQueue`;
- actions may enqueue dependent actions through their explicit queue execution context, but actions must never advance/pump the queue themselves;
- because `AddToFront` inserts at index 0, dependent front actions must be added in reverse of intended execution order when necessary;
- `UShuffleDeckAction` only shuffles; it does not draw automatically;
- shuffle uses one battle-scoped `FRandomStream` initialized once at battle setup and then consumed across future shuffles;
- do not reinitialize the random stream on every shuffle;
- deterministic Fisher–Yates shuffle is preferred for explicit reproducible ordering;
- drawing with both DrawPile and DiscardPile empty finishes safely rather than recursively retrying;
- `ExhaustPile` exists now, but do not add exhaust-specific behavior until a later phase requires it.

### Phase 4 — Data-Driven Cards

Introduce:

- `UCardData` / `UPrimaryDataAsset`;
- `UCardInstance`;
- card type;
- target type;
- cost;
- reusable card effects.

Normal cards should be configured from data and reusable effects. Avoid one C++ or Blueprint class per ordinary card.

Phase 4 should replace the temporary Phase 3 token representation with a real runtime card-instance model without moving authoritative deck state into UI.

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

### Phase 6 — Battle Events and Triggers

Introduce explicit events such as:

- battle start;
- turn start / end;
- card played;
- card drawn;
- card exhausted;
- deck shuffled;
- before / after damage where semantically needed;
- enemy killed.

Listeners should normally produce actions instead of recursively mutating gameplay state.

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

The interaction must emerge from generic draw, shuffle, event, modifier and action rules.

---

## 4. Core Architecture Rules

### 4.1 Deterministic gameplay

For the same initial state, input sequence and RNG seed, gameplay results should be reproducible.

Gameplay correctness must not depend on frame rate, animation timing, UObject addresses, unordered collection iteration or unordered listener execution.

### 4.2 Cards describe effects; they do not own battle rules

Preferred:

```text
Pommel Strike+
  - DamageEffect(10)
  - DrawEffect(2)
```

Avoid unrelated battle-system logic inside card implementations.

### 4.3 Gameplay mutations flow through actions after Phase 2

After `BattleActionQueue` exists, direct mutations such as these are suspicious outside owning low-level systems/actions:

```cpp
Target->HP -= Damage;
Player->Energy += 2;
Deck->Shuffle();
```

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

Prefer queued resolution over synchronous recursive chains such as draw → shuffle → trigger → draw → ... .

### 4.6 Separate definitions from runtime instances

```text
CardData   != CardInstance
StatusData != StatusInstance
RelicData  != RelicInstance
```

Do not mutate shared definition assets with runtime state such as temporary cost, stacks or counters.

### 4.7 Composition over card-specific inheritance

Prefer reusable effects/components/data composition. Avoid one class per ordinary card effect combination.

### 4.8 UI is presentation, not authority

UMG may request gameplay actions and display state, but must not own authoritative HP, block, energy, deck contents, status stacks, relic counters or turn state.

### 4.9 Keep dependencies explicit

Avoid repeated `GetAllActorsOfClass` / `GetActorOfClass` during normal gameplay execution.

### 4.10 No premature GAS migration

Implement and understand the card-battle-specific action queue, deck rules, modifier pipeline and deterministic resolution model first.

### 4.11 Actions may schedule dependencies but do not drive the queue

An executing action may receive the current `UBattleActionQueue` explicitly and enqueue dependent follow-up actions when the gameplay operation requires them.

Example:

```text
DrawCardAction
    ↓ empty DrawPile + non-empty DiscardPile
AddToFront(RetryDraw)
AddToFront(Shuffle)
    ↓
Finish current DrawCardAction
    ↓
Queue executes Shuffle
    ↓
Queue executes RetryDraw
```

The action must not call `PumpQueue`, `ProcessNext` or otherwise advance execution itself.

---

## 5. Modifier-Based Framework Architecture

The project adopts selected ideas from a Modifier-Based Framework (MBF) to standardize stackable, interceptable and order-sensitive gameplay effects.

MBF does not replace the battle action queue.

```text
BattleActionQueue
→ when operations execute and in what order

Modifier Pipeline
→ pre-commit modification / interception / override / clamp

Battle Event / Trigger
→ post-commit reaction that may enqueue new BattleActions
```

### 5.1 Action vs Modifier vs Trigger

Use a `BattleAction` for an authoritative gameplay operation.

Examples:

```text
DamageAction
GainBlockAction
DrawCardAction
ShuffleDeckAction
GainEnergyAction
ApplyStatusAction
```

Use a `Modifier` when an existing operation must be changed before commit.

Examples:

```text
Strength
Weak
Vulnerable
Dexterity
Frailty
Artifact
damage caps
cost overrides
operation cancellation
```

Use a `Trigger` when an effect reacts to a completed gameplay fact and produces additional gameplay.

Examples:

```text
Sundial reacting to DeckShuffled
Thorns reacting to damage received
gain energy after killing an enemy
```

Triggers should normally enqueue new actions instead of mutating unrelated state directly.

### 5.2 Modifier ordering

Do not resolve all modifiers with one global integer priority.

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

`Domain` identifies the gameplay mechanism, such as Damage, Block, CardCost or StatusApplication.

Each domain defines explicit phases. A future damage pipeline may resemble:

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

`Priority` only orders modifiers inside the same Domain + Phase.

When Phase and Priority are equal, use a deterministic tie-break such as stable definition id plus runtime instance sequence.

Never rely on `TSet` iteration, UObject address, actor discovery order or unordered registration order.

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

Pending operations may be cancelled before commit.

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

Do not commit an invalid operation and then undo it afterward.

### 5.5 MBF guardrail

When adding a buff/debuff/relic/rule, determine in this order:

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

Follow Unreal naming conventions:

- `A` — Actor;
- `U` — UObject / ActorComponent;
- `F` — struct;
- `E` — enum;
- `I` — interface;
- `b` prefix for booleans.

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

Prefer forward declarations where practical, keep public headers small, and use include paths valid for the actual module layout.

Any UObject that must survive GC must have clear ownership and an appropriate `UPROPERTY` / `TObjectPtr` reference.

Do not enable Tick by default. Card-battle gameplay should primarily be event/action driven.

---

## 7. Blueprint and Asset Rules

Prefer C++ for battle state, action queue, combat rules, deck rules, modifier pipelines, event dispatch, status/relic runtime logic and validation logic.

Prefer Blueprint / UMG / DataAssets for visual assembly, widget layout, artwork, simple presentation animation, content configuration and editor-created test assets.

### Content root policy

All project-owned Unreal assets should live under:

```text
Content/SlayTheSpireDemo/
```

Recommended long-term tree:

```text
Content/SlayTheSpireDemo/
├── Maps/
├── Blueprints/
│   ├── Battle/
│   ├── Characters/
│   ├── Cards/
│   └── Debug/
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

Current Phase 1-3 editor assets remain:

```text
Content/SlayTheSpireDemo/
├── Maps/
│   └── L_BattleTest
└── Blueprints/
    ├── Battle/
    │   └── BP_BattleManager
    └── Characters/
        ├── Player/
        │   └── BP_PlayerCombatant
        └── Enemies/
            └── BP_TestEnemy
```

### Asset naming

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
8. Do not add map/shop/event/meta-progression systems before core combat architecture is validated unless requested.
9. Preserve the ability to explain each system from a learning perspective.
10. Do not skip development phases unless the user explicitly approves it.
11. Do not prematurely implement planned MBF/domain/phase enums before Phase 5 merely because they are documented.

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
- creating DataAssets;
- creating/editing UMG widgets;
- visual-only Blueprint wiring when appropriate;
- saving `.uasset` / `.umap` assets;
- PIE validation and returning logs/screenshots.

Whenever user action is required, instructions must include exact path/menu, asset/class name, property values, what to run, expected result and what to return if it differs.

---

## 11. Implemented Core Classes

### Phase 1

```text
Source/SlayTheSpireDemo/
├── Battle/
│   ├── BattleManager.h
│   └── BattleManager.cpp
└── Combat/
    ├── Combatant.h
    └── Combatant.cpp
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
├── Deck/
│   ├── DeckTypes.h
│   └── DeckRuntime.h/.cpp
└── Actions/
    ├── DrawCardAction.h/.cpp
    ├── DiscardCardAction.h/.cpp
    └── ShuffleDeckAction.h/.cpp
```

`ABattleManager` owns the battle-scoped `UBattleActionQueue` and `UDeckRuntime` during the current debug battle flow.

---

## 12. Acceptance Summary

### Phase 1 — PASSED

Minimal battle loop, HP, block, energy, turn transition and terminal states validated in UE5.8 PIE.

### Phase 2 — PASSED

Action queue ordering, front/back insertion, explicit finish lifecycle, queued damage/block and queue-empty turn progression validated in UE5.8 PIE.

### Phase 3 — PASSED

Deck state, one-card draw actions, stable-identity discard, empty-deck safety, queued shuffle/retry behavior, single queue-empty completion and deterministic seeded shuffle behavior validated in UE5.8 PIE.

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

Pommel Strike should know only that it deals damage and draws cards.

The deck system should know only how drawing, pile movement and shuffling work.

Sundial should know only about shuffle events.

Damage-related statuses should modify typed damage operations through the Modifier Pipeline rather than being hard-coded into cards.

If adding a card/relic/status requires editing many unrelated card classes or scattering concrete status checks through battle code, stop and reconsider the architecture.

---

## 14. Documentation and Progress Updates

When completing a meaningful phase:

- update `Current Repository State`;
- record durable architecture invariants introduced;
- list UE Editor assets the user had to create manually;
- keep documentation synchronized with actual code and validation status;
- do not fill this document with daily implementation trivia.
