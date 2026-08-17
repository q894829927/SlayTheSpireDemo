# SlayTheSpireDemo Agent Instructions

## 1. Project Goal

This repository is an Unreal Engine 5.8 C++ learning/demo project inspired by the combat architecture of Slay the Spire.

The immediate goal is not to recreate the full game. The goal is to build a small, extensible card-battle framework that can naturally support combinations such as:

- card effects composed from reusable actions;
- draw / discard / exhaust / shuffle piles;
- buffs, debuffs and relic triggers;
- deterministic battle timing through an action queue;
- event-driven interactions between cards, statuses and relics;
- emergent combos such as two upgraded Pommel Strikes interacting with Sundial without hard-coding that combo.

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
BattleEvent
        ↓
Status / Relic / Enemy listeners
        ↓
New BattleAction
```

`BattleStateMachine` controls the large-scale turn flow, while `BattleActionQueue` controls the fine-grained resolution order inside a turn.

---

## 2. Current Repository State

Current status at the time this file was created:

- [x] Unreal Engine C++ project created.
- [x] Runtime module `SlayTheSpireDemo` exists.
- [x] Repository contains the basic UE-generated `Config`, `Source` and `.uproject` files.
- [x] Git repository is connected to GitHub.
- [x] Project-level agent constraints are defined in this `AGENTS.md`.
- [ ] Phase 1 minimal combat loop implemented.
- [ ] `BattleActionQueue` implemented.
- [ ] Deck / hand / discard / exhaust system implemented.
- [ ] Data-driven card system implemented.
- [ ] Status / modifier system implemented.
- [ ] Battle event system implemented.
- [ ] Relic system implemented.
- [ ] Sundial + Pommel Strike integration test implemented.

Agents must update this section when a phase is completed. Do not mark work complete unless the relevant code is implemented and the user has been told about any required editor-side validation.

---

## 3. Development Order

Do not skip ahead unless the user explicitly requests it.

### Phase 1 — Minimal Combat Loop

Goal: prove the basic battle state loop before introducing cards.

Implement only:

- player combatant;
- enemy combatant;
- HP;
- block;
- player energy;
- battle state;
- test attack;
- end turn;
- enemy test attack;
- victory / defeat checks.

Expected flow:

```text
BattleStart
    ↓
PlayerTurn
    ↓
TestAttack / EndTurn
    ↓
EnemyTurn
    ↓
EnemyAttack
    ↓
PlayerTurn
```

Do NOT implement cards during Phase 1.

### Phase 2 — BattleActionQueue

Replace direct combat mutations from battle commands with queued actions.

First actions:

- `UBattleAction`;
- `UBattleActionQueue`;
- `UDamageAction`;
- `UGainBlockAction`.

Required rule:

```text
request action
    ↓
queue action
    ↓
execute one action
    ↓
wait until finished
    ↓
execute next action
```

Do not resolve multiple gameplay actions simultaneously.

### Phase 3 — Deck System

Implement:

- draw pile;
- hand;
- discard pile;
- exhaust pile;
- draw action;
- discard action;
- shuffle action.

The UI must never be the authority for pile state.

### Phase 4 — Data-Driven Cards

Introduce:

- `UCardData` / `UPrimaryDataAsset`;
- `UCardInstance`;
- card type;
- target type;
- cost;
- reusable card effects.

Normal cards should be configured from data and reusable effects. Avoid one C++ class or one Blueprint class per ordinary card.

### Phase 5 — Status and Damage Modifier Pipeline

Implement statuses such as:

- Strength;
- Weak;
- Vulnerable.

Damage must be calculated through a defined modifier pipeline instead of scattered `if` statements in cards.

### Phase 6 — Battle Events

Introduce explicit battle events such as:

- battle start;
- turn start / end;
- card played;
- card drawn;
- card exhausted;
- deck shuffled;
- before / after damage;
- enemy killed.

Gameplay listeners may react to an event by producing new actions. Avoid deep synchronous chains of gameplay mutation.

### Phase 7 — Relics

Implement relic listeners using the same event architecture.

First validation relics should include:

- Sundial;
- optionally Abacus.

### Phase 8 — Combo Architecture Validation

Create an integration/debug scenario for:

```text
Pommel Strike+
Pommel Strike+
Sundial
```

The combo must work through generic draw/shuffle/event/action rules.

Forbidden implementation:

```text
if player has two Pommel Strikes and Sundial:
    enable infinite combo
```

The engine must not know that this combo exists.

---

## 4. Core Architecture Rules

### 4.1 Gameplay code must be deterministic

For the same initial combat state, input sequence and RNG seed, gameplay results should be reproducible.

Do not make gameplay correctness depend on frame rate, animation frame timing or unordered listener execution.

### 4.2 Cards describe effects; they do not own the battle rules

A card should request reusable effects/actions.

Preferred:

```text
Pommel Strike+
  - DamageEffect(10)
  - DrawEffect(2)
```

Avoid putting unrelated battle-system logic directly inside a card implementation.

### 4.3 Gameplay mutations should flow through actions once ActionQueue exists

After Phase 2, direct mutation such as the following should be treated as suspicious outside the owning low-level component/action:

```cpp
Target->HP -= Damage;
Player->Energy += 2;
Deck->Shuffle();
```

Preferred flow:

```text
request
→ BattleAction
→ BattleActionQueue
→ owning gameplay component
```

### 4.4 Events notify; actions mutate

Use events to announce meaningful gameplay facts.

Examples:

```text
DeckShuffled
CardPlayed
TurnStarted
DamageTaken
```

Listeners should normally enqueue actions rather than recursively performing large chains of gameplay mutations inside callbacks.

### 4.5 Avoid recursive gameplay chains

Do not build gameplay flow around recursion such as:

```text
DrawCard()
→ Shuffle()
→ OnShuffle()
→ GainEnergy()
→ OnGainEnergy()
→ DrawCard()
→ ...
```

Prefer queued state transitions/actions so every step can finish cleanly and be inspected.

### 4.6 Separate definitions from runtime instances

Static configuration and runtime state are different concepts.

Examples:

```text
CardData      != CardInstance
StatusData    != StatusInstance
RelicData     != RelicInstance
```

Runtime state such as temporary cost, upgrade state, counters or stacks should not mutate shared asset definitions.

### 4.7 Composition over card-specific inheritance

Prefer reusable effects/components/data composition.

Do not create large inheritance trees such as:

```text
Card
→ AttackCard
→ DrawAttackCard
→ StrengthDrawAttackCard
→ ...
```

### 4.8 UI is presentation, not gameplay authority

UMG widgets may request gameplay actions and display state.

UMG widgets must not own authoritative values for:

- HP;
- block;
- energy;
- deck contents;
- status stacks;
- relic counters;
- turn state.

### 4.9 Keep dependencies explicit

Avoid repeatedly searching the world for core combat objects with patterns such as:

```text
GetAllActorsOfClass
GetActorOfClass
```

inside normal gameplay execution.

Core systems should receive/store explicit references or use a clear battle context/manager relationship.

### 4.10 No premature GAS migration

Do not introduce Gameplay Ability System only because the project is in UE.

This project should first implement and understand the card-battle-specific architecture directly. GAS may be evaluated later for specific concerns, but the action queue and deterministic card-resolution model remain first-class project systems.

---

## 5. UE5 C++ Conventions

### Naming

Follow Unreal naming conventions:

- `A` — Actor;
- `U` — UObject / ActorComponent;
- `F` — struct;
- `E` — enum;
- `I` — interface;
- `b` prefix for booleans.

Examples:

```text
ABattleManager
ACombatant
UBattleAction
UBattleActionQueue
FCardEffectSpec
EBattleState
```

### Source organization

Target structure as systems are introduced:

```text
Source/SlayTheSpireDemo/
├── Battle/
├── Combat/
├── Actions/
├── Cards/
├── Deck/
├── Status/
├── Relics/
├── Events/
├── Enemy/
└── UI/
```

Do not create empty folders merely to match this design. Add directories when the relevant system is actually implemented.

### Headers

Prefer forward declarations where practical. Keep public headers small. Avoid unnecessary engine-wide includes.

### UObject ownership

Any UObject that must survive garbage collection must have a valid owner/reference and should normally be held by an appropriate `UPROPERTY`/`TObjectPtr`.

Do not create gameplay UObjects without considering their lifetime and GC ownership.

### Tick

Do not enable Tick by default.

Card battle logic should primarily be event/action driven. Enable Tick only when a specific visual or temporal behavior genuinely requires it.

---

## 6. Blueprint and Asset Rules

### C++ vs Blueprint

Prefer C++ for:

- battle state;
- action queue;
- combat rules;
- deck rules;
- event dispatch rules;
- status/relic runtime logic;
- validation logic.

Prefer Blueprint / UMG / DataAssets for:

- visual assembly;
- widget layout;
- card artwork;
- simple presentation animation;
- content configuration;
- editor-created test actors/assets.

### Binary assets

Agents working only through source control/text tools must not pretend to have created or correctly wired `.uasset` / `.umap` assets when they cannot actually operate the UE editor.

When a step requires Unreal Editor interaction, stop at a compilable/text-complete state and provide the user with exact editor steps.

Never hand-edit binary `.uasset` or `.umap` files.

### Generated UE files

Do not intentionally commit generated or machine-local files such as:

```text
Binaries/
DerivedDataCache/
Intermediate/
Saved/
.vs/
*.sln
*.slnx
```

unless the repository policy is explicitly changed later for a specific reason.

Before adding new generated-file patterns, inspect `.gitignore` first.

---

## 7. Change-Scope Rules for Agents

For every task:

1. Inspect the relevant existing files before editing.
2. State the intended scope internally before implementation.
3. Make the smallest coherent change that completes the requested phase/task.
4. Do not refactor unrelated code.
5. Do not rename public APIs/assets without need.
6. Do not silently change project plugins, engine association or build target settings.
7. Do not add third-party dependencies without explicit user approval.
8. Do not add network/multiplayer architecture unless explicitly requested.
9. Do not add map/shop/event/meta-progression systems before the core combat architecture is validated unless explicitly requested.
10. Preserve the ability to explain each system from a learning perspective.

This is a learning project. Prefer clear architecture and explainable code over clever abstractions.

---

## 8. Build and Verification Rules

After changing C++:

- verify includes and module dependencies;
- build the `SlayTheSpireDemoEditor` target when a build environment is available;
- report build errors rather than masking them;
- do not claim a successful UE build if it was not actually run.

When UE Editor validation is necessary, explicitly label it as `USER ACTION REQUIRED` and provide exact steps.

A feature is not fully validated merely because the source code looks syntactically correct.

---

## 9. User-Action Boundary

The agent should perform source/text work whenever possible.

The user is expected to perform UE Editor operations that cannot be safely represented as text-only repository edits, including examples such as:

- opening the project after C++ types are added;
- allowing Unreal to rebuild modules when required;
- creating Blueprint subclasses from C++ classes;
- placing actors in a level;
- assigning instance references in Details;
- creating DataAsset instances;
- creating/editing UMG widgets;
- wiring visual-only Blueprint nodes when an asset cannot be produced safely by the current toolchain;
- saving `.uasset` / `.umap` assets;
- running PIE and reporting observed behavior/screenshots/logs when validation is editor-only.

Whenever user action is required, instructions must include:

1. exact menu/content-browser path;
2. exact class/asset name to create or select;
3. exact property values to set;
4. what to click/run;
5. expected visible/log result;
6. what information to return if the result differs.

Do not simply say “set it up in Blueprint” or “test it in UE.”

---

## 10. Phase 1 Planned Classes

The first implementation phase should introduce only the minimum necessary classes.

Suggested initial layout:

```text
Source/SlayTheSpireDemo/
├── Battle/
│   ├── BattleManager.h
│   └── BattleManager.cpp
└── Combat/
    ├── Combatant.h
    └── Combatant.cpp
```

### `ACombatant`

Initial responsibilities:

- `MaxHP`;
- `HP`;
- `Block`;
- initialize/reset combat values;
- receive simple combat damage;
- gain/clear block;
- report death.

Do not put card/deck/status/relic systems into this class.

### `ABattleManager`

Initial responsibilities:

- player reference;
- enemy reference;
- `EBattleState`;
- player energy / max energy;
- start battle;
- start player turn;
- test attack;
- end player turn;
- execute one temporary enemy attack;
- check victory/defeat.

This direct-call implementation is intentionally temporary. Phase 2 will route combat actions through `UBattleActionQueue`.

---

## 11. Phase 1 Acceptance Criteria

Phase 1 is complete when all of the following are observable:

1. Battle starts in player turn.
2. Player starts a turn with 3 energy.
3. Test attack costs 1 energy.
4. Test attack deals 6 damage to the enemy.
5. Test attack is rejected at 0 energy.
6. Ending the player turn causes one enemy attack.
7. Enemy test attack deals 5 damage to the player.
8. A new player turn restores energy to 3.
9. Block absorbs damage before HP.
10. Enemy HP reaching 0 changes battle state to victory.
11. Player HP reaching 0 changes battle state to defeat.
12. Results are visible through logs and/or a minimal debug UI.

Do not move Phase 1 to completed until these behaviors are implemented and validated sufficiently to continue.

---

## 12. Architecture Validation Principle

The project should eventually be able to express complex interactions without special-case combo code.

The key validation example is:

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

Sundial should listen only to shuffle events. Pommel Strike should know only that it deals damage and draws cards. The deck system should know only how drawing and shuffling work.

If implementing a new card/relic requires editing many unrelated existing card classes, stop and reconsider the architecture.

---

## 13. Documentation and Progress Updates

When completing a meaningful phase:

- update `Current Repository State` in this file;
- briefly document any new architecture invariant introduced;
- list any UE Editor assets the user had to create manually;
- keep documentation synchronized with the actual code, not planned code.

Do not fill this document with daily implementation trivia. Record durable rules, architecture decisions and phase completion state.
