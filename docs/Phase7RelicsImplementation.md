# Phase 7 — Relics

Date: **2026-09-02**

Status: **DESIGN AUTHORIZED / IMPLEMENTATION NOT STARTED**

Phase 6UI-A is complete, validated and sealed by `docs/Phase6UIA3Seal.md`. Phase 7 is now the next project phase.

This document is the active Phase 7 design authority. Runtime code must not be started from this document until the user explicitly authorizes the implementation slice.

---

## 1. Goal

Add Relics as a first-class, deterministic Gameplay system without modeling a Relic as a Status and without introducing card/relic combination special cases.

The first vertical slice is **Sundial**:

```text
committed DeckShuffled event
→ Sundial Relic trigger becomes eligible
→ reaction Action advances Sundial runtime counter
→ every third committed gameplay shuffle resets the counter
→ dependent GainEnergyAction grants +2 Energy
→ existing committed Presentation remains authoritative
```

Initial battle setup shuffle is already excluded from `FDeckShuffledEvent` and therefore does not advance Sundial.

Phase 7 must leave the project ready for Phase 8:

```text
upgraded Pommel Strike(s)
→ Draw
→ real Shuffle
→ FDeckShuffledEvent
→ Sundial trigger
→ reaction Action
→ Energy gain
```

No code may ask whether the card is Pommel Strike or whether a particular card/relic pair exists.

---

## 2. Predecessor contracts that remain sealed

Phase 7 reuses, rather than redefines:

```text
BattleAction / BattleActionQueue mutation authority
BattleEvent = immutable-by-contract committed fact
Trigger = read-only eligibility + Action builder
reaction order = Priority → RuntimeSequence → LocalTriggerIndex
reaction batches = atomic insertion / nested depth-first semantics
DeckShuffled event timing = after successful shuffle commit, before RetryDraw
initial setup shuffle = no gameplay DeckShuffled event
Gameplay and Presentation timelines remain separate
Native HUD is the sole active production battle UI
A3 does not predict Trigger/Relic reactions
```

Do not reopen sealed A2/A3 architecture to implement Relics.

---

## 3. Scope and explicit non-scope

### In Phase 7 first required scope

```text
RelicData immutable definition
RelicInstance mutable runtime state
RelicContainer battle ownership
shared deterministic RuntimeSequence allocation
Relic Trigger participation in BattleEventDispatcher
Sundial counter + DeckShuffled reaction
reusable GainEnergyAction
minimal read/frozen/HUD Relic view
focused Automation
one final basic PIE acceptance when visible UI arrives
```

### Explicitly deferred

```text
run/map reward screens
relic acquisition choices
save/load or cross-battle run persistence
shop/reward generation
relic rarity pools
relic removal
relic stacking/duplicates
relic-specific VFX/SFX
RelicTriggered dedicated Presentation Record
advanced tooltip/inspect UX
A3 prediction of Relic reactions
universal modifier-source framework
persistent Trigger Registry
GAS migration
```

The current demo has no run-level ownership layer. `URelicInstance` is therefore battle-scoped in Phase 7. A future run system may move ownership upward without changing the definition/runtime split. Do not build speculative run persistence now.

---

## 4. Definition and runtime model

Relics follow the same durable definition/runtime principle as Cards and Statuses, but they are not subclasses of either.

```text
URelicData      = immutable shared definition
URelicInstance  = mutable battle runtime state
URelicContainer = authoritative collection for the battle
```

### 4.1 URelicData

Recommended first-version fields:

```cpp
FName RelicId;
FText DisplayName;
FText Description;
TObjectPtr<UTexture2D> Icon;              // optional for first asset
TArray<TObjectPtr<UBattleTrigger>> Triggers;
```

The Trigger subobjects are immutable definition objects, exactly like Status trigger definitions.

Do not put mutable Sundial progress on `URelicData`.

### 4.2 URelicInstance

Recommended first-version runtime state:

```text
Definition
RelicId
RuntimeSequence
Counter
```

`Counter` is runtime state used by Sundial. It is not stored on the Trigger definition.

The first version does not need a universal arbitrary key/value state bag. Add another explicit runtime field only when a concrete second Relic requires it.

### 4.3 URelicContainer

`ABattleManager` owns one battle-scoped `URelicContainer`.

Responsibilities:

```text
reset for a new battle
instantiate configured starting Relics in deterministic definition order
reject invalid or duplicate RelicId definitions
own RelicInstance lifetime
expose read-only ordered instances to Gameplay queries / dispatcher
```

Mid-battle relic acquisition is out of scope; first-version Relics are configured before the battle starts.

### 4.4 RuntimeSequence

Relics use the existing battle-wide:

```cpp
ABattleManager::AllocateRuntimeSequence()
```

Do not create a separate relic sequence counter.

Status and Relic trigger candidates therefore share one deterministic ordering key:

```text
Priority
→ RuntimeSequence
→ LocalTriggerIndex
```

No `SourceKind` tiebreaker is required if RuntimeSequence allocation remains unique battle-wide.

Starting Relics are instantiated in their configured array order at one fixed battle-setup point before gameplay shuffle events can occur.

---

## 5. Minimal Trigger-source generalization

The current Phase 6 implementation is intentionally Status-shaped in two places:

```text
FTriggerContext runtime source = UStatusInstance*
BattleEventDispatcher candidate discovery = Combatant StatusContainers only
```

Phase 7 introduces the first concrete need to generalize this boundary.

Do **not** add a persistent Trigger Registry. Candidate discovery remains snapshot-based at dispatch time.

### 5.1 Source-neutral runtime descriptor

Introduce one small C++-only source descriptor, conceptually:

```cpp
enum class ETriggerRuntimeSourceKind : uint8
{
    Status,
    Relic
};

struct FTriggerRuntimeSource
{
    ETriggerRuntimeSourceKind Kind;
    UObject* RuntimeObject;
    FName SourceId;
    uint64 RuntimeSequence;
    ACombatant* CombatantOwner; // null for battle-owned Relic
};
```

This is not a new Gameplay entity hierarchy. It is a lightweight dispatcher/context view over an already-existing runtime source.

### 5.2 FTriggerContext compatibility

Internally, `FTriggerContext` should store the source-neutral descriptor.

Preserve the existing Status-oriented accessor needed by sealed Status triggers, and add neutral/Relic accessors rather than forcing a broad Phase 6 rename.

Conceptual surface:

```cpp
UObject* GetRuntimeSourceObject() const;
UStatusInstance* GetRuntimeSource() const; // compatibility: Status source or null
URelicInstance* GetRelicSource() const;
ETriggerRuntimeSourceKind GetSourceKind() const;
FName GetSourceId() const;
uint64 GetRuntimeSequence() const;
ACombatant* GetOwner() const;               // null for battle-owned Relic
UObject* GetActionOuter() const;
ABattleManager* GetBattle() const;
const FPresentationRecordWriter& GetPresentationRecordWriter() const;
```

`TurnEndStatusDecayTrigger` must remain a Status trigger and should not gain Relic branches.

### 5.3 Dispatcher candidate collection

At each dispatch:

```text
collect current Status candidates from supplied Combatants
collect current Relic candidates from BattleContext.Relics
↓
run CanReact read-only
↓
snapshot all eligible candidates
↓
sort one combined array
Priority → RuntimeSequence → LocalTriggerIndex
↓
BuildReactions read-only
↓
validate local batches
↓
insert one final reaction batch atomically
```

Collection order is not reaction order; explicit sorting remains authoritative.

Candidate source should be neutral:

```text
runtime source object
source kind
source id
runtime sequence
trigger definition
local trigger index
```

Use a source-neutral `TSet<UObject*>` only to reject accidental duplicate runtime-source enumeration; never order by pointer/address.

### 5.4 Eligibility trace compatibility

`FTriggerEligibilityRecord` currently exposes `StatusId` for Phase 6 tests.

Do not break that historical test surface unnecessarily. Add neutral fields such as:

```text
SourceKind
SourceId
```

and keep `StatusId` populated for Status candidates during the migration. New Phase 7 tests should assert the neutral fields.

---

## 6. Sundial vertical slice

### 6.1 Definition

Create one `URelicData` asset/definition:

```text
RelicId      = Sundial
DisplayName  = 日晷 / Sundial according to project localization convention
Trigger      = USundialTrigger
Counter      = runtime only, starts at 0
```

`USundialTrigger` may expose immutable authored configuration:

```cpp
int32 ShufflesRequired = 3;
int32 EnergyGain = 2;
```

The production DataAsset uses `3` and `2`.

### 6.2 CanReact

Sundial reacts only when:

```text
Event.TryGet<FDeckShuffledEvent>() succeeds
Relic runtime source is valid
Battle is valid
Event Deck is the current authoritative battle DeckRuntime
```

Do not react to setup shuffle because setup emits no `FDeckShuffledEvent`.

Do not inspect card identity, DrawAction identity or RetryDraw identity.

### 6.3 Trigger remains read-only

`USundialTrigger::BuildReactions` must not increment the counter and must not grant Energy directly.

It creates one reaction Action:

```text
USundialAdvanceAction
```

The trigger definition carries configuration; mutable progress belongs to `URelicInstance` and is committed by the Action.

### 6.4 SundialAdvanceAction

This is intentionally a narrow content vertical-slice Action rather than a speculative universal counter/reward framework.

Execute-time behavior:

```text
validate RelicInstance / RelicData / Battle
validate ShufflesRequired > 0 and EnergyGain > 0
↓
advance the live Sundial counter by 1
↓
if counter < ShufflesRequired:
    commit counter
    Finish

if counter reaches ShufflesRequired:
    reset counter to 0
    create dependent UGainEnergyAction(+EnergyGain)
    propagate PresentationRecordWriter
    insert dependent action before finishing
    Finish
```

The dependent Action is queued before `USundialAdvanceAction::Finish()`. The Action never pumps the queue.

If a later second Relic proves the same counter/reward state machine is reusable, extract a generic action then. Do not create that framework in advance.

### 6.5 Relic counter mutation

Counter mutation occurs only from authoritative Action execution.

A small mutation helper analogous to `BattleEnergyMutation` is acceptable if it improves invariants, for example:

```text
RelicRuntimeMutation::AdvanceCyclic(...)
```

It should return before/after/threshold information and fail soft on invalid input. Trigger/UI code must not call it.

No BattleEvent is emitted merely for changing the Sundial counter in the first version.

---

## 7. Reusable GainEnergyAction

Sundial creates the first concrete need for a positive Energy mutation Action.

Add:

```text
BattleEnergyMutation::TryGain
UGainEnergyAction
```

### 7.1 TryGain semantics

```text
Amount must be > 0
invalid Battle / invalid amount / integer overflow -> fail soft
EnergyAfter = EnergyBefore + Amount
no MaxEnergy clamp in the first version
```

`MaxEnergy` remains the normal turn-refill baseline; temporary in-turn energy may exceed it unless a later explicit Gameplay rule changes that contract.

### 7.2 GainEnergyAction

`UGainEnergyAction`:

```text
owns intended positive amount
commits through BattleEnergyMutation::TryGain
records the committed Energy change using the existing A2 EnergyChanged semantics
Finish on success or fail-soft rejection
```

Do not let Sundial directly assign `Battle->Energy`.

If the existing EnergyChanged record construction is currently private to `BattleManager.cpp`, extract only one narrowly scoped shared helper so `BattleManager` and `UGainEnergyAction` produce identical committed record semantics. Do not create a general Presentation framework refactor.

---

## 8. Read state, frozen Presentation and minimal UI

Relic Gameplay truth must flow through the same read/frozen boundary as other HUD state.

### 8.1 Read view

Conceptually add:

```text
FRelicReadView
- RelicInstance weak reference
- RelicData weak reference
- RelicId
- RuntimeSequence
- Counter
```

`FBattleReadSnapshot` carries an ordered `Relics[]` array.

### 8.2 Frozen/HUD view

Freeze to a pointer-free player-facing DTO:

```text
FBattleHUDRelicView
- RelicId
- RuntimeSequence
- DisplayName
- Description
- Counter
- Icon
```

The Presentation FinalSnapshot owns historical display state while A2 playback is active, exactly as for existing HUD surfaces.

### 8.3 No dedicated RelicTriggered record in first version

Do not add a new Presentation Record merely to flash Sundial in the first slice.

First-version visible behavior is sufficient if:

```text
DeckShuffled committed Presentation plays normally
EnergyChanged committed Presentation shows +2 Energy on the third shuffle
FinalSnapshot/catch-up updates Sundial counter coherently
```

A dedicated RelicTriggered/RelicCounterChanged visual record is deferred until a concrete UX need proves it necessary.

### 8.4 Native UI

Add the smallest Native relic surface required to inspect current state, conceptually:

```text
UBattleRelicWidget / WBP_BattleRelic_Native
HUD relic container (for example HB_Relics)
```

First version only needs:

```text
name/icon
counter when relevant
basic tooltip/description if cheap within existing Native patterns
```

Do not add Legacy relic UI.

Do not make the Widget query Gameplay or increment the counter.

---

## 9. A3 interaction remains unchanged

Phase 7 does not expand deterministic Immediate Preview into future reaction simulation.

Example:

```text
Pommel Strike current A3 Preview
= supported current Damage + legality/energy fields

NOT:
= predicted draw
= predicted shuffle
= predicted Sundial counter advance
= predicted +2 Energy from a future shuffle
```

A2 remains responsible for showing what actually committed after submission.

---

## 10. Recommended implementation slices

### 7A — Relic Runtime

Scope:

```text
URelicData
URelicInstance
URelicContainer
BattleManager ownership/setup
battle-wide RuntimeSequence
focused runtime tests
```

No dispatcher changes, Sundial trigger or UI yet.

Acceptance:

```text
AUTOMATED
- Editor Build once
- SlayTheSpireDemo.Phase7.RelicRuntime focused suite once

MANUAL PIE
- none
```

### 7B — Status + Relic Trigger Sources

Scope:

```text
source-neutral FTriggerContext internals
combined dispatcher candidates
neutral eligibility trace fields
preserve Status trigger behavior/order
Relic test trigger proves participation
```

No Sundial counter behavior yet.

Acceptance:

```text
AUTOMATED
- Editor Build once
- Phase7 TriggerSources focused suite once
- rerun only the smallest existing Phase 6 dispatcher/ordering prefix directly invalidated by this refactor

MANUAL PIE
- none
```

Do not run the full Phase6R aggregate.

### 7C — Sundial + GainEnergyAction

Scope:

```text
BattleEnergyMutation::TryGain
UGainEnergyAction
USundialTrigger
USundialAdvanceAction
DA_Relic_Sundial or equivalent production definition
focused Sundial tests
```

Acceptance must prove:

```text
setup shuffle does not advance Sundial
first gameplay shuffle: 0 -> 1, no Energy gain
second gameplay shuffle: 1 -> 2, no Energy gain
third gameplay shuffle: 2 -> 0, exactly +2 Energy
fourth gameplay shuffle: 0 -> 1
only real committed DeckShuffled events count
trigger/build stage is read-only
counter mutation occurs through Action execution
Energy gain occurs through dependent UGainEnergyAction
reaction order remains deterministic
no card identity special case
```

Manual PIE is not yet required if UI has not arrived.

### 7D — Relic Read/Frozen/Native UI

Scope:

```text
Relic read DTO
frozen Presentation snapshot relic DTO
ViewModel/HUD relic view
minimal Native relic Widget/container
Sundial counter visible
```

Acceptance:

```text
AUTOMATED
- Editor Build once
- focused Relic read/frozen/UI tests once

ASSET
- compile/save affected Native WBP assets once

MANUAL PIE
- one production L_BattleTest pass
- Sundial visible
- real shuffles advance visible counter coherently
- third real shuffle gives +2 Energy through committed playback
- no duplicate relic widget / flashback / input lock
```

### 7S — Phase 7 seal

After 7A-7D pass, record final evidence and seal Phase 7. Abacus is not required for the first seal unless separately authorized.

---

## 11. Optional Abacus follow-up

Abacus remains optional after Sundial.

Conceptual behavior:

```text
FDeckShuffledEvent
→ Abacus Trigger
→ existing GainBlockAction(Player, 6)
```

Its value would be architectural confirmation that a second Relic can reuse the same source-neutral Dispatcher without another source-framework refactor.

Do not implement Abacus before Sundial is independently complete unless the user explicitly changes scope.

---

## 12. Files likely affected

This is a design forecast, not authorization to edit them yet.

```text
Source/SlayTheSpireDemo/Relics/RelicData.*
Source/SlayTheSpireDemo/Relics/RelicInstance.*
Source/SlayTheSpireDemo/Relics/RelicContainer.*
Source/SlayTheSpireDemo/Relics/SundialTrigger.*
Source/SlayTheSpireDemo/Actions/SundialAdvanceAction.*
Source/SlayTheSpireDemo/Actions/GainEnergyAction.*
Source/SlayTheSpireDemo/Battle/EnergyMutation.*
Source/SlayTheSpireDemo/Events/BattleTrigger.*
Source/SlayTheSpireDemo/Events/BattleEventDispatcher.*
Source/SlayTheSpireDemo/Battle/BattleManager.*
Source/SlayTheSpireDemo/Battle/BattleReadSnapshot.h
Source/SlayTheSpireDemo/Presentation/PresentationTypes.h
Source/SlayTheSpireDemo/UI/BattleHUDTypes.h
Source/SlayTheSpireDemo/UI/BattleHUDViewModel.*
Source/SlayTheSpireDemo/UI/BattleHUDWidget.*
Source/SlayTheSpireDemo/UI/BattleRelicWidget.*
Source/SlayTheSpireDemoTests/Private/Phase7*.cpp
Content/.../DA_Relic_Sundial.uasset
Content/.../WBP_BattleRelic_Native.uasset
Content/.../WBP_BattleHUD_Native.uasset
```

Do not touch all of these in one implementation commit. Follow 7A → 7B → 7C → 7D.

---

## 13. Architecture decisions locked by this design

```text
Relic != Status.
RelicData is immutable; RelicInstance owns mutable progress.
Relics are battle-scoped until a real run layer exists.
Relics share ABattleManager RuntimeSequence allocation with Statuses.
BattleEventDispatcher remains snapshot-based; no persistent Trigger Registry.
Cross-source trigger order remains Priority → RuntimeSequence → LocalTriggerIndex.
UBattleTrigger remains read-only eligibility/reaction construction.
Sundial counter mutation happens in an Action, never in Trigger/UI.
Sundial reward uses a reusable GainEnergyAction.
The first Phase 7 need generalizes Trigger sources only; Modifier pipelines are not generalized speculatively.
No dedicated RelicTriggered Presentation Record is required in the first vertical slice.
A3 does not predict Relic reactions.
```

---

## 14. Next exact action

The next implementation slice, once explicitly authorized, is:

```text
Phase 7A — Relic Runtime
```

Before writing code, inspect only the exact battle setup/runtime-sequence ownership needed to place `URelicContainer` deterministically. Do not begin 7B/7C in the same slice.