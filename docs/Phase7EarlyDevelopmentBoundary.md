# Phase 7 Development Boundary and Full Flow — Relic System

Date: **2026-08-22**

Status: **EARLY GAMEPLAY DEVELOPMENT ALLOWED / FULL PHASE FLOW LOCKED / PHASE 7A CONTRACT LOCKED / PRESENTATION DEFERRED**

Branch: `phase7-relic-gameplay`

This document defines both:

1. the temporary boundary for starting Phase 7 Relics before Phase 6UI-A is fully closed; and
2. the intended full Phase 7 development flow that later work must follow unless the project explicitly revises this contract.

The intent is to use the already-validated Phase 5 modifier framework and Phase 6 event/trigger architecture to develop the Relic Gameplay architecture in parallel with the remaining Phase 6UI-A2D work, without creating dependencies on unstable Presentation code.

This document does **not** change the project-wide formal phase order. Phase 7 remains formally planned after Phase 6UI-A. Work performed early on this branch is an intentionally isolated Gameplay slice. Full Phase 7 completion still requires the later Presentation and full acceptance stages defined below.

---

## 1. Phase 7 architectural goal

Phase 7 is **not** merely "implement Sundial".

Its architectural purpose is to prove that the battle framework can support a second real Gameplay source family in addition to Status.

Before Phase 7, the effective source model is approximately:

```text
Status
↓
Modifier / Trigger contribution
↓
Battle Gameplay
```

Phase 7 must evolve this into:

```text
Gameplay Sources
├── Status
└── Relic
```

without replacing the existing architecture with an unnecessary universal gameplay-effect framework.

The successful Phase 7 result should demonstrate that:

```text
Status and Relic are different runtime source families
↓
both can contribute to the existing deterministic battle architecture
↓
shared behavior is extracted only through the smallest required boundary
↓
no content-specific combo code is required
```

The system must remain consistent with the project's existing long-term flow:

```text
Gameplay Commit
↓
BattleEvent
↓
collect current Trigger contributions
↓
filter + deterministic sort
↓
Build Reaction Actions
↓
BattleActionQueue
↓
Gameplay Commit
```

---

## 2. Current dependency boundary

The relevant dependency graph is:

```text
Phase 5 Modifier / Status Framework
                COMPLETE
                   |
                   v
Phase 6 Event / Trigger Architecture
                COMPLETE
                   |
          +--------+--------+
          |                 |
          v                 v
Phase 6UI-A2D          Phase 7 Gameplay
IN PROGRESS            EARLY WORK ALLOWED
          |                 |
          +--------+--------+
                   |
                   v
       Full Relic Presentation Integration
                   |
                   v
           Phase 7 Full Acceptance
                   |
                   v
              Phase 8
```

Phase 7 Gameplay may depend on stable Gameplay contracts from Phases 1–6.

Phase 7 early development must **not** depend on unfinished UI-A2D Status/Terminal Presentation internals.

---

## 3. Full Phase 7 development slices

The intended full Phase 7 flow is:

```text
Phase 7A — Relic Runtime
        ↓
Phase 7B — Trigger Source Integration
        ↓
Phase 7C — Sundial Gameplay Vertical Slice
        ↓
Phase 7D — Multi-Relic / Architecture Validation
        ↓
Phase 7E — Early Gameplay Regression Gate
        ↓
Phase 7P — Relic Presentation
        ↓
Phase 7R — Full Acceptance / Regression
        ↓
Phase 7 COMPLETE
        ↓
Phase 8
```

Current early-development branch may implement **7A through 7E**.

`7P` and final `7R` remain deferred until the Phase 6UI-A committed-Presentation architecture is stable enough to consume rather than redesign.

---

# 4. Phase 7A — Relic Runtime Design Contract

Phase 7A is intentionally narrow.

Its purpose is to prove that a Relic can exist as an independent, deterministic, battle-scoped Runtime source **before** Relics are allowed to participate in Trigger behavior.

The Phase 7A flow is:

```text
7A-1  Relic Definition + Runtime Identity
        ↓
7A-2  RelicContainer Membership / Duplicate / Lookup
        ↓
7A-3  Battle Ownership + Initialization + Restart Lifecycle
        ↓
7A-4  Focused Automation + Affected Regression
```

Phase 7A stops there.

Phase 7B is the first phase allowed to answer:

```text
How does a RelicInstance contribute Trigger behavior?
```

## 4.1 Phase 7A hard scope boundary

Phase 7A owns only:

```text
what a Relic definition is
how a runtime Relic instance is created
who owns runtime Relics during a battle
how Relics are identified
how membership is added/queried/reset
how duplicate/invalid input behaves
how deterministic enumeration is preserved
how battle restart clears old runtime state
focused Runtime Automation
```

Phase 7A must **not** implement or design:

```text
Relic Trigger types
Relic Trigger arrays on DataAssets
Trigger contributor interfaces
Sundial
DeckShuffled handling
Sundial counters
GainEnergyAction for Sundial
Relic Modifier contribution
Relic Presentation
Relic HUD / UMG
Relic animation
Phase 8 combo behavior
```

If a 7A implementation needs any of these to make the Runtime layer work, the design has crossed the phase boundary and should be reconsidered.

## 4.2 Required Runtime model

Implement the three-layer split:

```text
URelicData
URelicInstance
URelicContainer
```

with the conceptual ownership model:

```text
URelicData
= immutable shared definition/configuration

URelicInstance
= one concrete battle-scoped runtime identity

URelicContainer
= authoritative membership/runtime owner for the player's battle Relics
```

The separation is mandatory:

```text
RelicData != RelicInstance != RelicContainer
```

and remains separate from Status:

```text
StatusData      != RelicData
StatusInstance  != RelicInstance
StatusContainer != RelicContainer
```

## 4.3 `URelicData` contract

`URelicData` is immutable definition data.

The initial 7A definition should remain minimal. Conceptually required fields are:

```text
RelicId
DisplayName
Description
```

Additional fields may be added only if an existing project convention requires them for compilation or stable authored data. Do not add speculative Presentation or Trigger fields merely because they may be useful later.

In particular, Phase 7A does **not** add:

```text
Triggers[]
Modifiers[]
SundialCounter
CurrentCounter
bTriggeredThisTurn
runtime object references
battle ownership
```

Mutable state must never live on the shared definition.

Forbidden:

```text
DA_Relic_Sundial.CurrentCounter = 2
```

The same DataAsset may be referenced by different battles without sharing runtime mutation.

## 4.4 `URelicInstance` contract

`URelicInstance` represents one concrete Relic in one battle Runtime.

The 7A base instance should contain only state that every real Relic instance necessarily requires.

Expected common data is conceptually:

```text
Definition
RuntimeSequence
explicit battle/runtime ownership context as required by the existing architecture
```

Useful narrow helpers may include:

```text
GetDefinition()
GetRelicId()
GetRuntimeSequence()
GetDebugLabel()
```

A debug identity may look like:

```text
Sundial#17
```

where:

```text
Sundial = logical RelicId
17      = exact runtime RuntimeSequence
```

### 4.4.1 No universal Counter/state bag

Do **not** add a generic field such as:

```text
Counter
Counter2
StateValue
GenericInt
GenericBool
```

to the base `URelicInstance` merely to prepare for Sundial.

Different Relics may eventually require different Runtime state shapes, while many may require no mutable state at all.

Phase 7C will introduce the first real Relic-specific state requirement and must choose the smallest concrete mechanism at that time.

Phase 7A must not guess that mechanism in advance.

## 4.5 Logical identity vs exact runtime identity

Phase 7A locks two different concepts:

```text
RelicId
= logical/content identity
= answers "does the player own Sundial?"

RuntimeSequence
= exact battle-runtime identity
= answers "which concrete runtime Sundial instance is this?"
```

They are not interchangeable.

Future stale queued work must be able to distinguish:

```text
Sundial#10 removed
↓
Sundial#30 created later
```

An operation intended for `Sundial#10` must never retarget `Sundial#30` merely because both share the same `RelicId`.

Exact-instance enforcement is primarily consumed by later phases, but 7A must establish the identity model correctly.

## 4.6 RuntimeSequence allocation

Relic instances should use the existing battle-scoped RuntimeSequence allocator rather than inventing a separate Relic-only sequence space.

Conceptually:

```text
Battle-scoped allocator
├── Status#10
├── Relic#11
├── Status#12
└── Relic#15
```

This preserves one comparable deterministic identity domain for Phase 7B mixed Status + Relic Trigger ordering.

Required assumptions:

```text
RuntimeSequence > 0
new created runtime identities are unique
new created runtime identities are monotonic according to the battle allocator
```

Tests must **not** assume:

```text
next RuntimeSequence == previous RuntimeSequence + 1
```

Gaps are legal.

## 4.7 Relic uniqueness / duplicate rule

For the current Slay-the-Spire-style project scope, one `URelicContainer` must contain at most one active instance for a given non-empty `RelicId`.

Example:

```text
Add Sundial
→ Sundial#10 created

Add Sundial again
→ no second runtime instance
→ duplicate/no-op result
```

Do not silently create:

```text
Sundial#10
Sundial#15
```

in the same container.

This is a current project rule, not a universal statement that every future game must forbid duplicate relics.

## 4.8 `URelicContainer` ownership

The current Phase 7 contract treats Relics as player-owned battle Runtime state.

Preferred ownership is conceptually:

```text
ABattleManager
└── PlayerRelicContainer
```

Do not attach a RelicContainer to every `ACombatant` merely to make the design look generic. Enemy Relics are not a current requirement.

If a future concrete requirement introduces enemy-owned Relics, extend the ownership model then.

The RelicContainer must not recover authoritative Battle identity through:

```text
GetOuter() chain assumptions
world searches
GetAllActorsOfClass
singleton/global Relic manager
```

Dependencies must remain explicit.

## 4.9 `URelicContainer` responsibility

`URelicContainer` is authoritative for Relic membership and Runtime ownership.

It may own responsibility for:

```text
validating definition input
checking duplicate RelicId
creating/storing runtime instances
querying by RelicId
checking membership
enumerating current Relics
reset/clear at battle lifecycle boundaries
```

It does **not** own:

```text
Trigger execution
Reaction Action creation
Energy/Block/HP mutation
Presentation
UMG
Sundial semantics
```

## 4.10 Initial Container API should remain small

The 7A public surface should stay narrow.

Conceptually useful operations are:

```text
Initialize / Reset
AddRelic
FindRelicById
ContainsRelic
GetRelics
Clear
```

Do not add a broad dynamic Relic-management API without a current gameplay requirement.

In particular, a formal runtime `RemoveRelic` API may be deferred if no current Phase 7A Gameplay caller needs battle-time removal. Container reset/cleanup must still work.

## 4.11 `AddRelic` should expose typed outcome semantics

Avoid reducing all Add failures to one boolean.

Preferred conceptual result shape:

```text
ERelicAddOutcome
├── Invalid
├── Duplicate
└── Added

FRelicAddResult
├── Outcome
└── Instance
```

Exact naming may follow repository conventions, but callers/tests must be able to distinguish at least:

```text
invalid definition/input
duplicate/no-op
successful creation
```

A duplicate is an expected no-op condition, not a Gameplay ResolutionFault.

## 4.12 Invalid input semantics

At minimum, the following must not create a runtime Relic:

```text
null Definition
RelicId == None / empty
invalid/uninitialized ownership context where required
invalid RuntimeSequence allocation
failed UObject runtime creation
```

Invalid input must leave membership unchanged.

Ordinary invalid setup/test input should fail safely and must not partially add a Relic.

## 4.13 Deterministic storage and enumeration

Relic gameplay ordering must never depend on unordered container iteration.

Do not allow future Trigger ordering to inherit nondeterminism from direct iteration over an unordered `TMap`/`TSet`.

For the small expected Relic counts, a stable ordered storage model such as an owned array is sufficient unless a stronger concrete need appears.

A lookup acceleration structure may exist later, but the authoritative iteration path used by deterministic Gameplay must have an explicit stable ordering contract.

Phase 7A should preserve insertion/runtime identity determinism rather than optimize prematurely.

## 4.14 Battle initialization flow

The intended eventual Runtime setup is conceptually:

```text
BattleStart / battle runtime setup
↓
reset battle-scoped Relic Runtime
↓
obtain player Relic definitions from the current battle setup input
↓
for each valid Relic definition in deterministic setup order
    validate duplicate / RelicId
    allocate battle RuntimeSequence
    create RelicInstance
    add to PlayerRelicContainer
↓
Relic Runtime ready
↓
continue normal battle setup
```

Phase 7A must not introduce Run inventory, SaveGame, reward selection, map progression or persistence architecture merely to supply Relic definitions.

The demo may use the smallest explicit battle setup/injection path needed to validate Runtime ownership.

## 4.15 Battle restart / reset lifecycle

Battle restart behavior is a required Phase 7A acceptance condition.

Example:

```text
Battle 1
├── Sundial#10
└── Abacus#11

restart / start new battle runtime
↓
Battle 2
```

Required guarantees:

```text
old Relic membership is cleared
old Runtime instances are not treated as members of the new battle
old Relic-specific mutable state cannot leak into the new battle
new runtime identities follow the battle allocator contract
lookup/enumeration sees only current-battle members
```

7A must not defer this lifecycle correctness to Sundial or Presentation work.

## 4.16 Phase 7A does not define Trigger storage

Do not add `Triggers[]` to `URelicData` during 7A simply because 7B will need Relic Trigger definitions.

7B must first inspect the real existing Trigger classes/context and then add the smallest concrete Relic Trigger definition boundary required by both the existing architecture and the first real Relic.

This avoids coupling the Runtime foundation to a guessed Trigger representation.

## 4.17 Phase 7A Automation matrix

Do not implement one monolithic test. Use focused tests/scenarios covering at least the following.

### 4.17.1 Runtime creation

```text
valid RelicData
↓
AddRelic
↓
Outcome = Added
Instance exists
Definition exact match
RelicId exact match
RuntimeSequence > 0
Container contains exact instance
```

### 4.17.2 Duplicate handling

```text
Add Sundial
Add Sundial again
↓
second result = Duplicate/no-op
Container count unchanged
original instance remains authoritative
no second RuntimeSequence-backed Relic member is created
```

### 4.17.3 Invalid definition/input

Test at least:

```text
null Definition
empty RelicId
```

Expected:

```text
Outcome = Invalid
membership unchanged
no partial runtime instance published to the Container
```

### 4.17.4 Runtime identity

Create multiple real Relics and prove:

```text
RuntimeSequence values are non-zero
RuntimeSequence values are unique
later created runtime identity > earlier created runtime identity
```

Do not require contiguous `+1` values.

### 4.17.5 Lookup / membership

Prove:

```text
ContainsRelic(RelId)
FindRelicById(RelId)
GetRelics()
```

return only current authoritative membership and the expected exact instance.

### 4.17.6 Deterministic enumeration

Given the same battle Relic setup/order, repeated equivalent setup must expose Relics in the same deterministic order.

Do not validate behavior through unordered-map incidental order.

### 4.17.7 Battle restart/reset

Prove:

```text
Battle 1 Relics created
↓
reset/restart
↓
old membership absent
new Runtime membership clean
no old runtime state leaks
```

### 4.17.8 No Trigger / Presentation dependency

The 7A test suite itself should demonstrate that Relic Runtime creation, ownership, lookup and reset work without:

```text
Relic Trigger classes
FDeckShuffledEvent usage
Sundial behavior
Presentation Records
UMG
```

## 4.18 Phase 7A source-layout guidance

Prefer dedicated Runtime files under:

```text
Source/SlayTheSpireDemo/Relics/
├── RelicData.h/.cpp
├── RelicInstance.h/.cpp
├── RelicContainer.h/.cpp
└── RelicRuntimeTypes.h        // only if typed results justify a separate file
```

Focused tests should live in the Editor-only test module, for example:

```text
Source/SlayTheSpireDemoTests/Private/Phase7ARelicRuntimeTests.cpp
```

Do not add Sundial/Trigger/Presentation files as part of the 7A implementation commit.

## 4.19 Phase 7A exit condition

Phase 7A is complete only when all of the following are true:

```text
RelicData is immutable definition data
RelicInstance is a concrete battle-scoped runtime identity
RelicContainer is authoritative for player Relic membership
RelicId uniqueness rule is enforced
logical RelicId and exact RuntimeSequence identity are separate
Relics use the battle-scoped RuntimeSequence allocator
Container API has explicit invalid/duplicate/success semantics
iteration is deterministic
battle reset/restart clears old runtime membership/state
focused 7A Automation passes
required affected regression remains green
no Trigger implementation was required
no Presentation dependency was introduced
```

Allowed completion claim:

```text
Phase 7A Relic Runtime COMPLETE / VALIDATED
```

Only then should Phase 7B begin implementing Relic Trigger contribution.

---

# 5. Phase 7B — Trigger Source Integration

Phase 7 introduces the first real non-Status Trigger source family.

The implementation may extract the **smallest contributor/collector boundary** required to combine:

```text
Status Trigger sources
+
Relic Trigger sources
```

The resulting flow must remain:

```text
BattleEvent
↓
collect all current Trigger contributions
↓
filter eligibility
↓
deterministic sort
↓
Build Reaction Actions
↓
atomic reaction batch insertion
↓
BattleActionQueue
```

## 5.1 Mixed source collection is one ordering domain

Do **not** dispatch Status and Relic families separately.

Forbidden shape:

```text
Collect Status Triggers
↓
Sort + Dispatch Status
↓
Collect Relic Triggers
↓
Sort + Dispatch Relic
```

Required conceptual shape:

```text
Collect Status Trigger contributions
        +
Collect Relic Trigger contributions
        ↓
      one list
        ↓
      one sort
        ↓
  one reaction batch
```

Existing deterministic ordering remains:

```text
Priority
→ RuntimeSequence
→ LocalTriggerIndex
```

Source family must **not** become an implicit higher-priority ordering key.

For equal Trigger priority, a Relic and a Status participate in the same RuntimeSequence ordering domain rather than "all Status first" or "all Relics first".

## 5.2 No speculative universal framework

Do not introduce speculative abstractions such as:

```text
Universal Trigger Registry
Universal Effect Source
Universal Modifier Context
Potions/Powers/Artifacts source hierarchy
```

Only extract the smallest concrete boundary required by the two real source families that now exist:

```text
Status
Relic
```

## 5.3 7B exit condition

```text
Status and Relic Trigger sources participate in one deterministic
collection/filter/sort/reaction path without either source masquerading
as the other and without changing existing Status semantics.
```

Mixed Status + Relic ordering tests are mandatory.

---

# 6. Fundamental Phase 7 mutation rule — Triggers decide, Actions mutate

The Phase 6 architecture rule remains authoritative:

```text
Events notify
Triggers decide whether/how to react
Actions perform authoritative mutation
```

A Relic Trigger must not directly mutate:

```text
Relic runtime counters
Energy
Block
HP
Status
Deck state
or any other authoritative Gameplay state
```

## 6.1 Sundial counter mutation must be Action-driven

A superficially simple implementation such as this is forbidden:

```text
DeckShuffledEvent
↓
Sundial Trigger
↓
ShuffleCounter++
↓
if third shuffle: Energy += 2
```

The Trigger may inspect enough state to determine eligibility and build an Action, but the committed Relic runtime mutation belongs to an Action.

Preferred conceptual flow:

```text
FDeckShuffledEvent
↓
Sundial Trigger
↓
Build USundialReactionAction (or equivalent narrow Action)
↓
BattleActionQueue
↓
Action validates the exact Relic instance
↓
Action commits ShuffleCounter transition
↓
if threshold reached
    queue reusable Energy gain Action
↓
Finish current Action
↓
Energy Action executes
↓
authoritative Energy commit
```

The exact C++ class name may differ, but the responsibility split may not.

## 6.2 Exact-instance safety

Queued Relic mutation Actions must retain/validate enough identity to avoid mutating a replacement runtime Relic instance accidentally.

A stale queued Action must fail soft/no-op rather than retarget a newer Relic instance solely because it shares the same `RelicId`.

---

# 7. Phase 7C — Sundial Gameplay Vertical Slice

Sundial is the first required Phase 7 vertical slice.

## 7.1 Event source

Sundial must reuse the existing Phase 6C:

```text
FDeckShuffledEvent
```

Do not invent:

```text
SundialShuffleEvent
OnDeckWasShuffledForRelics
or any second shuffle notification path
```

The qualifying flow begins only after a successful discard-to-draw shuffle commit that already produces `FDeckShuffledEvent`.

Initial battle setup shuffle remains governed by the existing Phase 6C semantics and must **not** become a qualifying event merely for Sundial convenience.

## 7.2 Required gameplay behavior

The vertical slice should prove:

```text
Shuffle 1 → counter progresses, no Energy reward
Shuffle 2 → counter progresses, no Energy reward
Shuffle 3 → threshold reached, Energy reward commits
Shuffle 4 → next cycle begins
Shuffle 5 → progresses
Shuffle 6 → threshold reached again, Energy reward commits
```

Repeated cycles must work deterministically.

## 7.3 Energy mutation path

If the existing formal Gameplay API lacks a reusable queued Energy-gain mutation, Phase 7 may add the smallest reusable Action required by Sundial.

Preferred shape:

```text
UGainEnergyAction
```

or an equally narrow generic Energy mutation Action if existing code clearly supports that naming/contract.

Do not add a Sundial-only Energy shortcut.

Wrong:

```text
SundialAction → Player.Energy += 2
```

Preferred:

```text
SundialReactionAction
↓
queue GainEnergyAction
↓
GainEnergyAction
↓
formal Gameplay Energy commit
```

Existing Energy Presentation contracts must not be redesigned from this branch.

## 7.4 Sundial focused acceptance

Focused Automation should prove at minimum:

```text
Relic runtime ownership
Sundial exact runtime identity
first and second shuffle do not reward Energy
third qualifying shuffle rewards Energy
repeated 3-shuffle cycles
initial setup shuffle exclusion
stale exact-instance reaction fails soft
queue drains normally
no ordinary no-op creates Gameplay ResolutionFault
Energy mutation uses the formal queued Gameplay path
```

## 7.5 7C exit condition

```text
Sundial gameplay behavior is deterministic and fully testable without UMG,
without Presentation dependencies and without Trigger-side mutation.
```

---

# 8. Phase 7D — Multi-Relic / Architecture Validation

Sundial alone is not sufficient proof that a general Relic architecture exists; it could still hide Sundial-specific assumptions.

Phase 7D therefore validates at least one additional real Relic behavior or an equivalent scenario that proves multiple Relic Trigger sources coexist correctly.

## 8.1 Default second validation relic: Abacus

The preferred second Relic is Abacus because it can reuse the same real `FDeckShuffledEvent` while producing a different reaction:

```text
FDeckShuffledEvent
↓
Abacus Trigger
↓
GainBlockAction
↓
Block Commit
```

This provides a compact proof that the event/trigger architecture supports multiple Relics without Sundial-specific branching.

If Abacus is not implemented, replacing it with another Relic for Phase 7D requires an explicit project decision and the replacement must provide equivalent architectural coverage.

## 8.2 Multi-Relic behavior

A battle owning both relics should conceptually allow:

```text
DeckShuffledEvent
↓
collect Relic contributions
├── Sundial
└── Abacus
↓
merge with any Status contributions for the same event
↓
Priority → RuntimeSequence → LocalTriggerIndex
↓
deterministic reaction batch
```

The implementation must not hard-code:

```text
Sundial first
Abacus second
Status first
Relic second
```

unless that order naturally follows the shared deterministic ordering keys.

## 8.3 Required mixed-source validation

Phase 7D tests must include a scenario where:

```text
Status Trigger contribution
+
Relic Trigger contribution(s)
```

participate in the same dispatch cycle and are ordered by the common deterministic rule.

The purpose is to prove:

```text
one Event
↓
one mixed contribution list
↓
one deterministic ordering rule
↓
one atomic reaction batch
```

## 8.4 7D exit condition

```text
The Relic framework supports more than one concrete Relic and
mixed Status + Relic Trigger ordering without content-specific dispatch code.
```

---

# 9. Modifier-source boundary

Relics may eventually need to participate in typed Damage/Block/etc. modifier pipelines.

Do **not** generalize the Modifier collector merely because Relics now exist.

Current rule:

```text
Trigger abstraction
→ extract now because Sundial/Abacus create a real Relic Trigger need

Modifier abstraction
→ defer until the first real Relic requires Modifier participation
```

When that real need appears, extract only the smallest boundary required to combine:

```text
Status Modifier contributions
+
Relic Modifier contributions
```

Do not pre-build a universal modifier-source system for hypothetical future content.

---

# 10. Phase 7E — Early Gameplay Regression Gate

After 7A–7D, run the smallest meaningful affected regression set covering:

```text
Phase 5 modifier/status behavior
Phase 6 event/trigger behavior
Phase 6 deterministic reaction ordering
Phase 6C DeckShuffled semantics
Phase 7 Relic runtime tests
Phase 7 mixed Status + Relic Trigger tests
Phase 7 Sundial tests
Phase 7 second-Relic / multi-Relic tests
```

Required early-gameplay claims after this gate:

```text
Phase 7 Relic Gameplay implemented and validated
```

or, if all 7A–7D slices are complete:

```text
Phase 7 Gameplay COMPLETE / VALIDATED
Phase 7 overall IN PROGRESS
Phase 7 Presentation DEFERRED
```

Do **not** claim:

```text
Phase 7 COMPLETE
```

from the early Gameplay gate alone.

---

# 11. Phase 7P — Relic Presentation (deferred)

Phase 7P begins only after the Phase 6UI-A committed-Presentation architecture is stable enough to consume.

The objective is for players to understand both:

```text
which Relics currently exist
and
when/why a Relic activated
```

Potential concerns include:

```text
frozen Relic HUD state
Relic icon/display metadata
runtime counter display
Relic activation historical facts
WorkingPresentationSnapshot support
Presentation Controller playback
Blueprint/UMG integration
PIE visual acceptance
```

However, this document deliberately does **not** lock the final Presentation Record taxonomy yet.

Do not prematurely commit to structures such as:

```text
RelicTriggered
RelicCounterChanged
```

until A2D has finished validating the Record → reducer → playback → terminal architecture.

Phase 7P must reuse the finished committed-Presentation architecture rather than create a parallel Relic-specific playback system.

A desirable eventual visible flow for Sundial is conceptually:

```text
Shuffle presentation
↓
Sundial visibly activates
↓
Energy gain is presented
```

rather than allowing the Energy total to appear to change without understandable cause.

---

# 12. Phase 7R — Full Acceptance / Regression

Phase 7R is the final Phase 7 gate after Gameplay and Presentation are both integrated.

The full acceptance matrix should prove:

```text
Relic Runtime                     PASS
Relic ownership/lifetime         PASS
Relic deterministic identity     PASS

Status + Relic trigger collection PASS
mixed deterministic ordering      PASS
atomic reaction insertion          PASS

Sundial first cycle               PASS
Sundial repeated cycles           PASS
initial setup shuffle exclusion   PASS
stale exact-instance isolation    PASS

second Relic / multi-Relic        PASS
no Sundial-specific dispatcher    PASS

Gameplay fail-soft behavior       PASS
Phase 5/6 affected regression     PASS

Relic HUD/frozen state            PASS
Relic activation presentation     PASS
Presentation fail-soft behavior   PASS
Blueprint/PIE acceptance          PASS
```

Only after the required 7R acceptance is satisfied may the project claim:

```text
Phase 7 COMPLETE
```

---

# 13. Full runtime flow

The intended architectural flow is:

```text
                 RelicData
                    │
                    ▼
               RelicInstance
                    │
                    ▼
               RelicContainer
                    │
                    │
Gameplay Commit ────┼──────────────┐
                    │              │
                    ▼              │
               BattleEvent         │
                    │              │
                    ▼              │
          Collect Contributions    │
             /             \
         Status             Relic
             \             /
                    ▼
          deterministic sort
                    │
                    ▼
               Trigger rule
                    │
                    ▼
         Build Reaction Action
                    │
                    ▼
            BattleActionQueue
                    │
                    ▼
              Action Execute
                    │
         ┌──────────┴──────────┐
         ▼                     ▼
Relic runtime mutation   Gameplay mutation
(counter etc.)           Energy/Block/etc.
         │                     │
         └──────────┬──────────┘
                    ▼
                 Commit
                    │
          ┌─────────┴─────────┐
          ▼                   ▼
     further Events     Presentation facts
          │                   │
          ▼                   ▼
     more Triggers       Controller playback
```

Presentation is downstream of committed Gameplay facts. It is never authoritative over Relic state.

---

# 14. Explicitly forbidden early-development scope

The following work must **not** be finalized on the early Gameplay branch unless a later explicit project decision changes this boundary.

## 14.1 Relic Presentation implementation

Do not finalize during 7A–7E:

```text
RelicTriggered Presentation Record
RelicCounterChanged Presentation Record
Relic Presentation payload taxonomy
Relic WorkingPresentationSnapshot reducer
Relic historical playback
Relic Presentation Controller behavior
Relic PlaybackToken semantics
Relic animation callbacks
```

## 14.2 Relic HUD / UMG

Do not implement during 7A–7E:

```text
Relic HUD bar
Relic icon widgets
hover tooltip behavior
relic activation animation
relic VFX/SFX
relic counter animation
Blueprint playback wiring
PIE visual acceptance for Relics
```

Temporary debug-only output is allowed only when it does not become a second Gameplay/UI contract.

## 14.3 Formal Phase 8 combo validation

Do not start the formal Phase 8 acceptance scenario on this branch:

```text
Pommel Strike+
+
Pommel Strike+
+
Sundial
```

Phase 8 must validate architecture **and presentation** together after Phase 7 Gameplay and the required Phase 6UI-A presentation path are stable.

## 14.4 Broad framework generalization

Do not introduce speculative abstractions for future systems such as:

```text
Potions
Powers
Artifacts
Charms
Equipment
Universal Effect Sources
Universal Modifier Context
Universal Trigger Registry
```

## 14.5 UI-A2D redesign

This branch must not redesign:

```text
FPresentationResolutionEnvelope
FPresentationRecordWriter
PresentationSequence lifecycle
WorkingPresentationSnapshot ownership
StatusChanged payload
Status reducer behavior
Victory / Defeat terminal presentation
ResolutionFault presentation
Presentation backlog/token semantics
```

If Phase 7 Gameplay exposes an apparent Presentation limitation, record it as a follow-up instead of solving it by modifying UI-A2D from this branch.

---

# 15. Durable architecture rules

All Phase 7 work must preserve these rules.

## 15.1 Events notify; Actions mutate

Relic Triggers may observe committed Events and build reaction Actions.

Relic Triggers must not directly mutate authoritative state.

## 15.2 Relic is not Status

Relic and Status remain separate runtime domains.

Shared behavior may be extracted only through narrow contributor boundaries justified by both source families.

## 15.3 No world search

Do not use Gameplay-time actor discovery to recover battle dependencies.

Forbidden examples:

```text
GetAllActorsOfClass
world search for BattleManager
Outer-chain inference as authoritative Battle identity
singleton/global Relic manager
```

Pass explicit battle/context dependencies through the existing Action/Event/Trigger paths.

## 15.4 Determinism

Same initial battle state + same user inputs + same RNG seed must produce the same Relic outcomes and reaction order.

Correctness must not depend on:

```text
frame timing
UObject address
widget creation order
delegate registration order
unordered iteration order
animation timing
```

## 15.5 Definition/runtime separation

Relic definitions are immutable configuration.

Runtime counters and other mutable battle values belong to Runtime instances or another explicitly owned battle-runtime object.

## 15.6 Fail soft

Invalid or stale Relic runtime inputs should normally degrade to no-op/fail-soft behavior unless a true framework invariant has been violated.

Ordinary Relic no-op behavior must not cause a Gameplay ResolutionFault.

## 15.7 No Action queue driving from content

Relic Triggers/definitions may build Actions, and Actions may enqueue required dependent work according to existing queue rules, but Relic content must not directly pump/advance the queue.

---

# 16. Parallel-development file ownership guidance

Phase 6UI-A2D and Phase 7 may proceed in parallel, but Phase 7 should minimize edits to files under active Presentation development.

Prefer Phase 7-owned additions under a dedicated runtime area such as:

```text
Source/SlayTheSpireDemo/Relics/
```

Likely legitimate shared touch points include:

```text
BattleManager initialization
BattleEventDispatcher / Trigger collection
battle runtime identity allocation
Build.cs when required
Automation test module
```

Avoid touching Presentation files during 7A–7E unless required only for compilation compatibility.

If both branches modify the same shared Gameplay file, Phase 7 must preserve the existing public contract and keep the patch minimal to simplify later rebase/integration.

If UI-A2D changes a shared Gameplay contract before merge, rebase Phase 7 onto the updated main line and rerun the affected regression gate.

---

# 17. Early branch merge boundary

Development on `phase7-relic-gameplay` may proceed before Phase 6UI-A closes.

Merging the **Gameplay slice** into the main development line should require all of the following:

```text
Phase 7A runtime tests PASS
Phase 7B mixed trigger-order tests PASS
Phase 7C Sundial gameplay tests PASS
Phase 7D multi-Relic architecture tests PASS
Phase 7E affected Phase 5/6 regressions PASS
no required Relic Presentation dependency
no UI-A2D contract redesign
no Phase 8 special-case combo code
```

The merge may establish:

```text
Phase 7 Gameplay COMPLETE / VALIDATED
```

but must not establish:

```text
Phase 7 COMPLETE
```

until 7P and final 7R are complete.

---

# 18. Agent instruction summary

Any Agent working on Phase 7 should follow this compact rule set:

```text
1. Treat Phase 7 as a Relic architecture phase, not merely a Sundial feature.
2. Phase 7A implements Relic Runtime only; do not design Trigger behavior in 7A.
3. Keep RelicData / RelicInstance / RelicContainer separate from Status.
4. Keep RelicData immutable; never store mutable battle state in a DataAsset.
5. Keep the base RelicInstance minimal; do not add a universal Counter/state bag.
6. RelicId is logical identity; RuntimeSequence is exact runtime identity.
7. Use the battle-scoped RuntimeSequence allocator; do not create a Relic-only sequence space.
8. Do not assume RuntimeSequence values are contiguous.
9. Enforce one active Relic instance per non-empty RelicId in the current PlayerRelicContainer.
10. Prefer BattleManager-owned player battle RelicContainer; do not attach one to every Combatant without a real requirement.
11. RelicContainer owns membership/lifetime, not Trigger or Gameplay mutation.
12. Keep Container APIs narrow and expose typed Invalid/Duplicate/Added semantics.
13. Preserve deterministic Relic enumeration; never depend on unordered iteration.
14. Battle restart/reset must clear old Relic membership/runtime state.
15. Do not add Relic Triggers[] in 7A; 7B will add the smallest real Trigger boundary.
16. Use stable Phase 5/6 Gameplay contracts.
17. Collect Status + Relic Trigger contributions into one ordering domain in 7B.
18. Preserve Priority → RuntimeSequence → LocalTriggerIndex ordering.
19. Do not order by source family.
20. Relic Triggers decide/build Actions; they never mutate Gameplay or Relic counters directly.
21. Queued Relic mutation must validate exact runtime identity and fail soft when stale.
22. First concrete Relic is Sundial.
23. Sundial must reuse FDeckShuffledEvent; do not invent another shuffle event.
24. Initial setup shuffle does not qualify for Sundial.
25. Use a formal queued Energy Action for Sundial reward; no direct Energy shortcut.
26. Validate repeated Sundial cycles.
27. Phase 7D should validate a second Relic; default choice is Abacus.
28. Test mixed Status + Relic + multi-Relic deterministic ordering.
29. Extract Trigger contributor abstraction only as narrowly as Status + Relic require.
30. Do not generalize Modifier sources until a real Relic Modifier requires it.
31. Do not design/finalize Relic Presentation during 7A–7E.
32. Do not modify UI-A2D contracts to make Phase 7 easier.
33. Do not start formal Phase 8 combo acceptance.
34. Keep focused Automation and affected Phase 5/6 regressions green.
35. After 7A–7E, claim Phase 7 Gameplay COMPLETE / VALIDATED at most.
36. Claim Phase 7 COMPLETE only after 7P + final 7R acceptance.
```
