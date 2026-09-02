# Phase 7 — Relics

Date: **2026-09-02**

Status: **DESIGN SEALED / 7A IMPLEMENTATION AUTHORIZED**

Phase 6UI-A is complete, validated and sealed by `docs/Phase6UIA3Seal.md`. Phase 7 is now the active project phase.

This document is the formal Phase 7 implementation authority. The user has explicitly authorized Phase 7A after the design review recorded on 2026-09-02. Later slices remain separately gated by this document.

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
RelicCounterChanged dedicated Presentation Record
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
TObjectPtr<UTexture2D> Icon;              // optional until the UI slice
TArray<TObjectPtr<UBattleTrigger>> Triggers; // introduced when 7B needs it
```

The Trigger subobjects are immutable definition objects, exactly like Status trigger definitions.

Do not put mutable Sundial progress on `URelicData`.

### 4.2 URelicInstance

Core 7A runtime identity is:

```text
Definition
RelicId
RuntimeSequence
Battle context
```

Sundial adds its concrete mutable `Counter` only when 7C requires it. Do not add a universal arbitrary key/value state bag in 7A. Add another explicit runtime field only when a concrete Relic requires it.

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

### 4.4 RuntimeSequence and exact battle-setup order

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

No `SourceKind` tiebreaker is required because RuntimeSequence allocation is unique inside one battle.

Because RuntimeSequence is a cross-source ordering contract, starting Relic creation must not be lazy and must not depend on the first caller of a getter. `GetPlayerRelicContainer()` is a read/access boundary, not a hidden initialization boundary.

The Phase 7A integration point is locked to the current `ABattleManager::StartBattle()` lifecycle:

```text
create/reset ActionQueue + EventDispatcher + DeckRuntime
→ initialize DeckRuntime from configured deck
   - setup shuffle may happen here
   - setup shuffle emits no gameplay DeckShuffled event
   - deck setup consumes no battle RuntimeSequence
→ NextRuntimeSequence = 1
→ Player.InitializeCombatant / Enemy.InitializeCombatant
   - resets Status containers
   - does not create runtime Status instances
→ advance BattleId / establish the new battle state
→ initialize PlayerRelicContainer for this battle
→ instantiate configured StartingRelics in authored array order
   - each Relic consumes ABattleManager::AllocateRuntimeSequence()
→ all subsequent runtime Status creation and other sequence-owning sources
→ normal opening-hand / turn flow
```

This gives configured Starting Relics the earliest runtime sequences in the new battle after the allocator reset. Any Status created later in the battle receives a later RuntimeSequence. If a future feature introduces formal configured Starting Statuses, their relative setup position must be explicitly designed rather than silently changing this contract.

A battle restart rebuilds Relic membership at the same explicit setup point. RuntimeSequence may restart from `1` in a new BattleId; cross-battle identity must therefore never be interpreted as RuntimeSequence alone.

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

### 6.3 Trigger remains read-only and freezes intended configuration

`USundialTrigger::BuildReactions` must not increment the counter and must not grant Energy directly.

It creates one reaction Action and freezes the immutable intended values into that Action at construction time:

```text
USundialTrigger::BuildReactions
↓
new USundialAdvanceAction
↓
Initialize(
    RelicInstance,
    ShufflesRequired,
    EnergyGain
)
```

The Action therefore owns its intended operation:

```text
RequiredShuffles = authored ShufflesRequired at BuildReactions time
EnergyGain       = authored EnergyGain at BuildReactions time
```

`USundialAdvanceAction::Execute()` must not rediscover the Trigger by walking `RelicData.Triggers[]`, casting back to `USundialTrigger`, and reinterpreting definition configuration.

Mutable progress still belongs to `URelicInstance` and is committed only by the Action.

### 6.4 SundialAdvanceAction

This is intentionally a narrow content vertical-slice Action rather than a speculative universal counter/reward framework.

Execute-time behavior:

```text
validate exact current RelicInstance membership / Battle
validate frozen RequiredShuffles > 0 and frozen EnergyGain > 0
↓
advance the live Sundial counter by 1
↓
if counter < RequiredShuffles:
    commit counter
    Finish

if counter reaches RequiredShuffles:
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

### 7.3 Independent primitive acceptance

Sundial tests validate the Relic mechanic. `TryGain` / `UGainEnergyAction` receive a separate small focused primitive test group so the reusable Energy contract is not proved only indirectly through Sundial.

It must cover:

```text
Gain +2 succeeds
Energy may exceed MaxEnergy
zero amount rejected
negative amount rejected
integer overflow rejected
invalid Battle fails soft
committed EnergyChanged payload has exact Before / After / Delta
```

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

Freeze to a **mutable-gameplay-runtime-pointer-free player-facing DTO**. It must not retain `URelicInstance*` or other mutable Gameplay runtime truth. Immutable presentation asset references such as `UTexture2D` are allowed, matching existing frozen Card/HUD presentation practice.

Conceptually:

```text
FBattleHUDRelicView
- RelicId
- RuntimeSequence
- DisplayName
- Description
- bShowCounter
- Counter
- CounterMax
- Icon
```

Counter presentation is data-driven rather than RelicId-driven:

```text
Sundial:
    bShowCounter = true
    Counter      = current runtime progress
    CounterMax   = 3

Relic with no counter UI:
    bShowCounter = false
    Counter / CounterMax are ignored by the Widget
```

Native UI must never write `if (RelicId == "Sundial")` merely to decide whether a counter is visible.

The Presentation FinalSnapshot owns historical display state while A2 playback is active, exactly as for existing HUD surfaces.

### 8.3 Locked historical playback semantics without a Relic counter Record

Phase 7 first version deliberately does **not** provide record-by-record historical playback for Relic counter changes.

There is no `RelicCounterChanged` / `RelicTriggered` Record and therefore no reducer operation that can advance the historical Relic counter between individual Records inside one Envelope.

Locked behavior:

```text
while an A2 Envelope is actively playing:
    Relic counter display remains at the last completed historical snapshot

DeckShuffled Record may play:
    Relic counter display does not advance from that Record alone

third shuffle may then play EnergyChanged(+2):
    Relic counter may still visually show the previous historical value

when the Envelope completes / reconciles:
    Relic display is replaced/reconciled to Envelope.FinalSnapshot
    Counter then reflects the exact committed final value
```

This lag until FinalSnapshot reconciliation is intentional and acceptable for Phase 7. It preserves the existing A2 historical model without inventing a new Presentation Record solely for first-version counter animation.

A future `RelicCounterChanged` or `RelicTriggered` Record may provide per-trigger visual progression if a concrete UX requirement justifies it.

### 8.4 Native UI

Add the smallest Native relic surface required to inspect current state, conceptually:

```text
UBattleRelicWidget / WBP_BattleRelic_Native
HUD relic container (for example HB_Relics)
```

First version only needs:

```text
name/icon
counter when bShowCounter is true
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

## 10. Implementation slices

### 7A — Relic Runtime

Status: **AUTHORIZED / START NOW**

Scope:

```text
URelicData
URelicInstance
URelicContainer
BattleManager explicit ownership/setup
battle-wide RuntimeSequence
focused runtime tests
```

No dispatcher changes, Sundial trigger, Counter behavior or UI yet.

Acceptance:

```text
AUTOMATED
- Editor Build once
- SlayTheSpireDemo.Phase7.RelicRuntime focused suite once

The focused suite must include the explicit setup-order contract:
- configured Starting Relics are instantiated during StartBattle, not lazily from a getter
- configured Relics preserve authored order
- each receives a non-zero battle RuntimeSequence
- a Status/runtime source created after battle setup receives a later RuntimeSequence
- battle restart rebuilds exact Relic instances for the new BattleId

MANUAL PIE
- none
```

### 7B — Status + Relic Trigger Sources

Status: **NOT STARTED**

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

Status: **NOT STARTED**

Scope:

```text
BattleEnergyMutation::TryGain
UGainEnergyAction
USundialTrigger
USundialAdvanceAction
DA_Relic_Sundial or equivalent production definition
focused GainEnergy primitive tests
focused Sundial tests
```

Acceptance must prove:

```text
GainEnergy primitive:
+2 succeeds
may exceed MaxEnergy
0 / negative / overflow reject
invalid Battle fails soft
EnergyChanged Before / After / Delta exact

Sundial:
setup shuffle does not advance Sundial
first gameplay shuffle: 0 -> 1, no Energy gain
second gameplay shuffle: 1 -> 2, no Energy gain
third gameplay shuffle: 2 -> 0, exactly +2 Energy
fourth gameplay shuffle: 0 -> 1
only real committed DeckShuffled events count
trigger/build stage is read-only
USundialAdvanceAction consumes frozen RequiredShuffles / EnergyGain values
counter mutation occurs through Action execution
Energy gain occurs through dependent UGainEnergyAction
reaction order remains deterministic
no card identity special case
```

Manual PIE is not yet required if UI has not arrived.

### 7D — Relic Read/Frozen/Native UI

Status: **NOT STARTED**

Scope:

```text
Relic read DTO
frozen Presentation snapshot relic DTO
ViewModel/HUD relic view with bShowCounter / Counter / CounterMax
minimal Native relic Widget/container
Sundial counter visible through FinalSnapshot reconciliation
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
- completed Envelopes reconcile visible counter to exact FinalSnapshot state
- no claim of per-Record counter progression in Phase 7 first version
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

This is a phase forecast; each slice must touch only its own required subset.

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
Starting Relics are created explicitly during StartBattle after allocator reset; getters do not lazily create them.
Configured Starting Relics receive earlier RuntimeSequences than subsequent runtime Status creation.
BattleEventDispatcher remains snapshot-based; no persistent Trigger Registry.
Cross-source trigger order remains Priority → RuntimeSequence → LocalTriggerIndex.
UBattleTrigger remains read-only eligibility/reaction construction.
Sundial trigger freezes RequiredShuffles / EnergyGain into its reaction Action.
Sundial counter mutation happens in an Action, never in Trigger/UI.
Sundial reward uses a reusable GainEnergyAction.
GainEnergy has an independent reusable primitive contract and focused tests.
The first Phase 7 need generalizes Trigger sources only; Modifier pipelines are not generalized speculatively.
No dedicated RelicTriggered or RelicCounterChanged Presentation Record is required in the first vertical slice.
Without such a Record, Relic counters remain at the last completed historical snapshot during A2 playback and update on FinalSnapshot reconciliation.
HUD counter visibility is data-driven through bShowCounter / CounterMax, never by checking a concrete RelicId.
Frozen/HUD Relic DTOs exclude mutable Gameplay runtime pointers but may retain immutable presentation asset references.
A3 does not predict Relic reactions.
```

---

## 14. Next exact action

The design is sealed for the first Phase 7 vertical slice. Do not expand the architecture again without a concrete implementation need or defect.

The active implementation slice is now:

```text
Phase 7A — Relic Runtime
```

Implement only the definition/runtime/container foundation, explicit `StartBattle()` setup and focused runtime tests. Do not begin 7B/7C/7D in the same slice.