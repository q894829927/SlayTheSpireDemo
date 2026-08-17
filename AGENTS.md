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
- [ ] Phase 3 deck / hand / discard / exhaust system implemented.
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

Editor assets created manually for Phase 1:

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

### Phase 2 validation record

Validated manually in UE5.8 PIE:

- player test attack creates and executes `UDamageAction` through `UBattleActionQueue`;
- `AddToBack(7)`, `AddToBack(8)`, then `AddToFront(6)` resolves in deterministic `6 → 7 → 8` order;
- queue-empty notification occurs only after the complete queued batch resolves;
- `UGainBlockAction` executes through the same queue and grants 4 block;
- enemy attack is queued as `UDamageAction` and resolves before the next player turn starts;
- the player-turn transition waits for `OnQueueEmpty` rather than occurring before enemy damage;
- block still absorbs damage correctly when damage is delivered through an action;
- victory is detected after queue resolution when enemy HP reaches 0;
- defeat is detected after queue resolution when player HP reaches 0;
- player commands remain rejected after victory/defeat.

`L_BattleTest` Level Blueprint currently contains temporary debug wiring:

```text
BeginPlay → StartBattle
Keyboard 1 → TestAttack
Space → EndPlayerTurn
Keyboard B → TestGainBlock
Keyboard Q → TestActionQueueOrder
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

The Phase 1 direct combat-mutation path has been migrated to queued damage/block actions in Phase 2 where appropriate.

### Phase 2 — BattleActionQueue — COMPLETE

Implemented and PIE-validated:

- `UBattleAction`;
- `UBattleActionQueue`;
- `UDamageAction`;
- `UGainBlockAction`.

Required flow is now established:

```text
request action
    ↓
queue action
    ↓
execute current action
    ↓
action finishes
    ↓
execute next action
```

Durable Phase 2 invariants:

- one authoritative action executes at a time;
- queue ordering is explicit and deterministic;
- action completion is explicit through `Finish()` rather than assuming `Execute()` returning means resolution is finished;
- normal back insertion and explicit front insertion are supported;
- `BattleManager` observes queue-empty completion before advancing enemy-turn flow;
- queue processing does not require Tick;
- synchronous actions may finish immediately while the API remains compatible with future asynchronous presentation/animation;
- related action batches are assembled before `StartProcessing()` so a premature `QueueEmpty` cannot occur between batch members;
- player/enemy damage and debug block behavior resolve through actions.

`UDamageAction` carries future modifier-resolution context:

```text
Source
Target
BaseAmount
```

`UGainBlockAction` uses the same terminology:

```text
Source
Target
BaseAmount
```

Phase 2 intentionally does not implement the Modifier-Based Framework.

### Phase 3 — Deck System

Implement:

- draw pile;
- hand;
- discard pile;
- exhaust pile;
- draw action;
- discard action;
- shuffle action.

UI must never own authoritative pile state.

### Phase 4 — Data-Driven Cards

Introduce:

- `UCardData` / `UPrimaryDataAsset`;
- `UCardInstance`;
- card type;
- target type;
- cost;
- reusable card effects.

Normal cards should be configured from data and reusable effects. Avoid one C++ or Blueprint class per ordinary card.

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

Do not implement every gameplay mechanism as a Modifier. Status effects that react to completed events and create additional gameplay belong to the BattleEvent / Trigger architecture.

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

Events announce facts:

```text
DeckShuffled
CardPlayed
TurnStarted
DamageTaken
```

Listeners should normally enqueue actions rather than perform deep synchronous mutation chains.

### 4.5 Avoid recursive gameplay chains

Avoid designs like:

```text
DrawCard()
→ Shuffle()
→ OnShuffle()
→ GainEnergy()
→ OnGainEnergy()
→ DrawCard()
→ ...
```

Prefer queued resolution.

### 4.6 Separate definitions from runtime instances

```text
CardData   != CardInstance
StatusData != StatusInstance
RelicData  != RelicInstance
```

Do not mutate shared definition assets with runtime state such as temporary cost, stacks or counters.

### 4.7 Composition over card-specific inheritance

Prefer reusable effects/components/data composition.

Avoid inheritance trees such as:

```text
Card
→ AttackCard
→ DrawAttackCard
→ StrengthDrawAttackCard
```

### 4.8 UI is presentation, not authority

UMG may request gameplay actions and display state, but must not own authoritative:

- HP;
- block;
- energy;
- deck contents;
- status stacks;
- relic counters;
- turn state.

### 4.9 Keep dependencies explicit

Avoid repeated `GetAllActorsOfClass` / `GetActorOfClass` during normal gameplay execution.

Core systems should receive/store explicit references or use a clear battle context/manager relationship.

### 4.10 No premature GAS migration

Do not introduce GAS merely because the project uses Unreal Engine.

Understand and implement the card-battle-specific action queue, modifier pipeline and deterministic resolution model first. GAS may be evaluated later for specific concerns.

---

## 5. Modifier-Based Framework Architecture

The project adopts selected ideas from a Modifier-Based Framework (MBF) to standardize how stackable, interceptable and order-sensitive gameplay effects are resolved.

MBF does not replace the battle action queue.

The three core responsibilities are:

```text
BattleActionQueue
→ controls when gameplay operations execute and in what order

Modifier Pipeline
→ modifies, intercepts, overrides or constrains an operation before it is committed

Battle Event / Trigger
→ reacts to completed gameplay events and may enqueue new BattleActions
```

These concepts must remain separate.

### 5.1 Action vs Modifier vs Trigger

Use a `BattleAction` when the effect represents an authoritative gameplay operation.

Examples:

```text
DamageAction
GainBlockAction
DrawAction
ShuffleAction
GainEnergyAction
ApplyStatusAction
```

Use a `Modifier` when an existing operation must be changed before the final gameplay mutation is committed.

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

Use a `Trigger` when an effect reacts to something that has already happened and produces additional gameplay.

Examples:

```text
Sundial reacting to DeckShuffled
Thorns reacting to damage received
gain energy after killing an enemy
draw a card after playing a Power
```

Triggers should normally create/enqueue new `BattleAction` objects instead of directly mutating unrelated gameplay state.

Preferred:

```text
DeckShuffled
    ↓
Sundial Trigger
    ↓
GainEnergyAction
    ↓
BattleActionQueue
```

Avoid:

```cpp
if (HasSundial())
{
    Energy += 2;
}
```

### 5.2 Modifier ordering rules

Do not resolve all modifiers with one global integer priority.

Modifier resolution must use this ordering hierarchy:

```text
Domain
  ↓
Phase
  ↓
Priority
  ↓
StableOrder
```

#### Domain

`Domain` identifies which gameplay mechanism is being modified.

Possible domains include:

```text
Damage
Block
CardCost
StatusApplication
Healing
Energy
Draw
```

Effects from unrelated domains must not compete in the same priority list.

Examples:

```text
Strength       → Damage
Weak           → Damage
Vulnerable     → Damage
Dexterity      → Block
Frailty        → Block
Artifact       → StatusApplication
```

Do not ask whether `Strength` should execute before `Artifact`; they belong to different mechanisms.

#### Phase

Each domain should define a fixed, explicit resolution pipeline.

A future damage pipeline may resemble:

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

Example classification:

```text
Strength       → Damage / FlatAdd
Weak           → Damage / SourceMultiplier
Vulnerable     → Damage / TargetMultiplier
Intangible     → Damage / Clamp
```

Phase ordering is more important than raw priority. A lower-priority modifier in a later phase still executes after a higher-priority modifier in an earlier phase.

#### Priority

`Priority` is only used to order modifiers that belong to the same:

```text
Domain + Phase
```

Do not use arbitrary global values such as:

```text
Strength = 100
Weak = 200
Vulnerable = 300
Artifact = 400
Barricade = 500
```

unless those values are scoped to a well-defined domain and phase.

#### StableOrder

Modifier execution must remain deterministic when both `Phase` and `Priority` are equal.

Do not rely on unstable ordering sources such as:

```text
TSet iteration order
UObject memory address
Actor discovery order
unordered registration order
GetAllActorsOfClass return order
```

Use a deterministic tie-break rule.

Recommended conceptual ordering:

```text
Phase
↓
Priority
↓
ModifierDefinitionId
↓
RuntimeInstanceSequence
```

For the same initial battle state, input sequence and RNG seed, modifier order must be reproducible.

### 5.3 Typed modifier pipelines

Do not create one universal modifier context containing every possible gameplay field.

Avoid designs such as:

```text
FModifierContext
├── Damage
├── Block
├── Cost
├── Status
├── Draw
├── Healing
├── Energy
├── ...
```

Prefer shared modifier infrastructure with typed operation specs.

Examples:

```text
FDamageSpec
FBlockSpec
FCardCostSpec
FStatusApplySpec
```

A future damage operation should conceptually follow:

```text
UDamageAction
    ↓
FDamageSpec
    ↓
Damage Modifier Pipeline
    ↓
Final Damage Spec
    ↓
Commit damage
```

A future block operation should follow:

```text
UGainBlockAction
    ↓
FBlockSpec
    ↓
Block Modifier Pipeline
    ↓
Final Block Spec
    ↓
Commit block
```

### 5.4 Modifier lifecycle

Preferred resolution lifecycle:

```text
Create operation spec
↓
Collect relevant modifiers
↓
Filter modifiers by applicability
↓
Sort by Domain / Phase / Priority / StableOrder
↓
Apply modifiers
↓
Check cancellation / overrides
↓
Commit authoritative gameplay mutation
↓
Emit BattleEvent
↓
Triggers react
↓
Triggers enqueue new BattleActions
```

Modifiers act before `Commit`.

Triggers normally react after `Commit`.

Do not recursively perform large gameplay chains inside modifier callbacks.

### 5.5 Cancellation and interception

Modifier pipelines may cancel an operation before it commits.

This must be represented explicitly in the operation spec or resolution result.

Conceptually:

```text
ApplyStatusAction
    ↓
FStatusApplySpec
    ↓
StatusApplication Pipeline
    ↓
Artifact Modifier
    ↓
bCancelled = true
    ↓
No status is committed
```

Preferred:

```text
intercept pending operation
→ cancel before commit
```

Avoid:

```text
apply Weak
→ detect Artifact
→ remove Weak again
```

The same principle may later support rules such as:

```text
cannot gain Block
cannot draw cards
cost is fixed to 0
damage cannot exceed 1
```

through appropriate intercept, override or clamp phases.

### 5.6 Modifier scope and collection

Modifiers should only be evaluated when their scope is relevant to the current operation.

Possible sources include:

```text
Source combatant modifiers
Target combatant modifiers
Source relic modifiers
Target relic modifiers
Battle/global modifiers
Card/runtime modifiers
```

A damage pipeline may conceptually collect:

```text
Source Status
Source Relic
Target Status
Target Relic
Battle Rules
```

then filter them against the current `FDamageSpec`.

Do not run every modifier in the battle for every operation.

### 5.7 Phase 2 compatibility with future MBF

Phase 2 implements only the action queue and has been completed without introducing the full Modifier-Based Framework.

The completed Phase 2 action APIs retain enough context to avoid unnecessary redesign when typed modifier pipelines are added later.

`UDamageAction` carries:

```text
Source
Target
BaseAmount
```

`UGainBlockAction` carries:

```text
Source
Target
BaseAmount
```

Future Phase 5 resolution evolves toward:

```text
BaseAmount
↓
Typed Spec
↓
Modifier Pipeline
↓
FinalAmount
↓
Commit
```

Do not implement Strength, Weak, Vulnerable or other modifiers before Phase 5 merely because these fields exist.

### 5.8 MBF design guardrails

When adding a new buff, debuff, relic or battle rule, do not begin by asking:

```text
What Priority number should this use?
```

First determine:

```text
1. Is this an Action, Modifier or Trigger?
2. If Modifier, what Domain does it modify?
3. Which Phase of that Domain applies?
4. Does same-phase ordering matter?
5. Only then assign Priority.
6. Define a deterministic StableOrder tie-break.
```

Do not solve modifier interactions with scattered special-case conditionals in cards, `ABattleManager` or unrelated gameplay classes.

Avoid card-local logic such as:

```cpp
if (HasStrength)
{
    Damage += Strength;
}

if (HasWeak)
{
    Damage *= 0.75f;
}

if (TargetHasVulnerable)
{
    Damage *= 1.5f;
}
```

Prefer:

```text
Card
↓
DamageAction(BaseAmount)
↓
DamageSpec
↓
Damage Modifier Pipeline
↓
Final Damage
```

The battle core should not need to know the concrete identity of every buff, debuff or relic.

---

## 6. UE5 C++ Conventions

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
├── Modifiers/
├── Relics/
├── Events/
├── Enemy/
└── UI/
```

Do not create empty folders merely to reserve future architecture.

### Headers

- prefer forward declarations where practical;
- keep public headers small;
- avoid unnecessary engine-wide includes;
- use include paths that are valid for the module's actual directory layout;
- do not assume nested source folders are automatically include roots.

### UObject ownership

Any UObject that must survive GC must have clear ownership and an appropriate `UPROPERTY` / `TObjectPtr` reference.

### Tick

Do not enable Tick by default. Card-battle gameplay should primarily be event/action driven.

---

## 7. Blueprint and Asset Rules

### C++ vs Blueprint

Prefer C++ for:

- battle state;
- action queue;
- combat rules;
- modifier pipelines;
- deck rules;
- event dispatch rules;
- status/relic runtime logic;
- validation logic.

Prefer Blueprint / UMG / DataAssets for:

- visual assembly;
- widget layout;
- artwork;
- simple presentation animation;
- content configuration;
- editor-created test actors/assets.

### Content root policy

All project-owned Unreal assets should live under:

```text
Content/SlayTheSpireDemo/
```

Do not place project assets directly under `Content/` without a specific reason.

Do not create every directory below in advance. Add folders only when the related system or asset type is introduced.

### Recommended long-term Content directory tree

```text
Content/
└── SlayTheSpireDemo/
    ├── Maps/
    │   ├── L_BattleTest
    │   ├── L_MainMenu
    │   └── L_MapTest
    │
    ├── Blueprints/
    │   ├── Battle/
    │   │   └── BP_BattleManager
    │   ├── Characters/
    │   │   ├── Player/
    │   │   │   └── BP_PlayerCombatant
    │   │   └── Enemies/
    │   │       ├── BP_TestEnemy
    │   │       ├── BP_JawWorm
    │   │       └── BP_Cultist
    │   ├── Cards/
    │   │   └── (only special Blueprint-backed behavior when genuinely needed)
    │   └── Debug/
    │       └── BP_BattleDebugController
    │
    ├── Data/
    │   ├── Cards/
    │   │   ├── Ironclad/
    │   │   │   ├── Attacks/
    │   │   │   │   ├── DA_Card_Strike
    │   │   │   │   ├── DA_Card_Bash
    │   │   │   │   └── DA_Card_PommelStrike
    │   │   │   ├── Skills/
    │   │   │   │   ├── DA_Card_Defend
    │   │   │   │   └── DA_Card_ShrugItOff
    │   │   │   └── Powers/
    │   │   │       └── DA_Card_Inflame
    │   │   └── Shared/
    │   ├── Enemies/
    │   │   ├── DA_Enemy_JawWorm
    │   │   └── DA_Enemy_Cultist
    │   ├── Status/
    │   │   ├── DA_Status_Strength
    │   │   ├── DA_Status_Weak
    │   │   └── DA_Status_Vulnerable
    │   └── Relics/
    │       ├── DA_Relic_Sundial
    │       └── DA_Relic_Abacus
    │
    ├── UI/
    │   ├── Battle/
    │   │   ├── WBP_BattleHUD
    │   │   ├── WBP_PlayerPanel
    │   │   ├── WBP_EnemyPanel
    │   │   └── WBP_EnergyCounter
    │   ├── Cards/
    │   │   ├── WBP_Card
    │   │   ├── WBP_Hand
    │   │   └── WBP_CardTargetSelector
    │   ├── Status/
    │   │   └── WBP_StatusIcon
    │   ├── Relics/
    │   │   └── WBP_RelicIcon
    │   └── Common/
    │       ├── WBP_HealthBar
    │       └── WBP_Tooltip
    │
    ├── Art/
    │   ├── Characters/
    │   │   ├── Player/
    │   │   └── Enemies/
    │   ├── Cards/
    │   │   ├── Ironclad/
    │   │   └── Common/
    │   ├── Relics/
    │   ├── Status/
    │   ├── Backgrounds/
    │   └── Icons/
    │
    ├── Materials/
    │   ├── Cards/
    │   ├── UI/
    │   └── VFX/
    │
    ├── VFX/
    │   ├── Combat/
    │   ├── Cards/
    │   └── Status/
    │
    ├── Audio/
    │   ├── Music/
    │   ├── SFX/
    │   │   ├── Cards/
    │   │   ├── Combat/
    │   │   └── UI/
    │   └── Ambience/
    │
    └── Dev/
        ├── Debug/
        └── Tests/
```

Do not create all folders in advance. The tree is a target organization, not a requirement to create placeholder directories/assets.

### Phase 1 Content directory tree

```text
Content/
└── SlayTheSpireDemo/
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

### Asset naming conventions

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
| Niagara Emitter | `NE_` | `NE_HitBurst` |
| Sound Wave | `S_` | `S_Card_Play` |
| Sound Cue | `SC_` | `SC_Attack` |
| Animation asset | `A_` | `A_EnemyAttack` |

Prefer domain-qualified DataAsset names:

```text
DA_Card_Strike
DA_Relic_Sundial
DA_Status_Strength
DA_Enemy_JawWorm
```

### Asset responsibility separation

Keep rules, configuration and presentation distinct.

Example card split:

```text
Source/Cards/...                  -> gameplay schemas/rules
Content/.../Data/Cards/...        -> content configuration
Content/.../Art/Cards/...         -> artwork
Content/.../UI/Cards/...          -> presentation
```

Example enemy split:

```text
Source/Enemy/...                              -> runtime rules / intent logic
Content/.../Data/Enemies/...                  -> numeric/content configuration
Content/.../Blueprints/Characters/Enemies/... -> actor assembly/presentation
Content/.../Art/Characters/Enemies/...        -> visual assets
```

### Binary assets

Text/source agents must not pretend to have correctly created or wired `.uasset` / `.umap` files without UE Editor access.

Never hand-edit binary `.uasset` or `.umap` files.

### Generated UE files

Do not intentionally commit machine-local/generated files such as:

```text
Binaries/
DerivedDataCache/
Intermediate/
Saved/
.vs/
*.sln
*.slnx
```

unless repository policy is explicitly changed.

---

## 8. Change-Scope Rules for Agents

For every task:

1. Inspect relevant existing files first.
2. Make the smallest coherent change that completes the requested task.
3. Do not refactor unrelated code.
4. Do not rename public APIs/assets without need.
5. Do not silently change project plugins, engine association or build target settings.
6. Do not add third-party dependencies without explicit user approval.
7. Do not add multiplayer/network architecture unless requested.
8. Do not add map/shop/event/meta-progression systems before core combat architecture is validated unless requested.
9. Preserve the ability to explain each system from a learning perspective.
10. Do not skip development phases unless the user explicitly approves it.
11. Do not prematurely implement planned MBF/domain/phase enums during an earlier development phase merely because the architecture is documented.

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

Whenever user action is required, instructions must include:

1. exact menu/content-browser path;
2. exact class/asset name;
3. exact property values;
4. what to click/run;
5. expected result;
6. what to return if it differs.

Do not merely say “set it up in Blueprint” or “test it in UE.”

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
Source/SlayTheSpireDemo/
└── Actions/
    ├── BattleAction.h
    ├── BattleAction.cpp
    ├── BattleActionQueue.h
    ├── BattleActionQueue.cpp
    ├── DamageAction.h
    ├── DamageAction.cpp
    ├── GainBlockAction.h
    └── GainBlockAction.cpp
```

`ABattleManager` owns the battle-scoped `UBattleActionQueue` and creates queued actions for the current debug battle commands.

`ACombatant` remains the low-level owner of HP and block mutation.

---

## 12. Phase 1 Acceptance Criteria — PASSED

All criteria were validated in UE5.8 PIE:

1. Battle starts in player turn. ✅
2. Player starts with 3 energy. ✅
3. Test attack costs 1 energy. ✅
4. Test attack deals 6 damage. ✅
5. Test attack is rejected at 0 energy. ✅
6. Ending player turn causes enemy turn. ✅
7. Enemy test attack deals 5 damage. ✅
8. New player turn restores energy to 3. ✅
9. Block absorbs damage before HP. ✅
10. Enemy HP reaching 0 enters victory. ✅
11. Player HP reaching 0 enters defeat. ✅
12. Results were verified through PIE logs. ✅

---

## 13. Phase 2 Acceptance Criteria — PASSED

All criteria were validated in UE5.8 PIE:

1. Player damage is executed through `UDamageAction` and `UBattleActionQueue`. ✅
2. `AddToBack` preserves FIFO order. ✅
3. `AddToFront` inserts the next action ahead of pending back actions. ✅
4. The debug batch `Back(7), Back(8), Front(6)` resolves exactly `6 → 7 → 8`. ✅
5. `OnQueueEmpty` fires after the entire batch, not between actions. ✅
6. `UGainBlockAction` grants block through the action queue. ✅
7. Enemy damage executes through the action queue. ✅
8. Enemy turn waits for queue completion before entering the next player turn. ✅
9. Block absorption remains correct through queued damage. ✅
10. Victory is resolved after queue completion. ✅
11. Defeat is resolved after queue completion. ✅
12. Finished battle states reject further player commands. ✅

---

## 14. Architecture Validation Principle

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

Sundial should know only about shuffle events. Pommel Strike should know only that it deals damage and draws cards. The deck system should know only how drawing and shuffling work.

Damage-related statuses should modify a typed damage operation through the Modifier Pipeline rather than being hard-coded into Pommel Strike or another individual card.

The intended separation is:

```text
Action Queue
= execution timing/order

Modifier Pipeline
= pre-commit rule/value modification

BattleEvent / Trigger
= post-commit reactions and new actions
```

If adding a card/relic/status requires editing many unrelated existing card classes or scattering concrete status checks through battle code, stop and reconsider the architecture.

---

## 15. Documentation and Progress Updates

When completing a meaningful phase:

- update `Current Repository State`;
- record durable architecture invariants introduced;
- list UE Editor assets the user had to create manually;
- keep documentation synchronized with actual code and validation status;
- do not fill this document with daily implementation trivia.
