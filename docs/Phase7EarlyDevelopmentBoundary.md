# Phase 7 Development Boundary and Full Flow — Relic System

Date: **2026-08-22**

Status: **EARLY GAMEPLAY DEVELOPMENT ALLOWED / FULL PHASE FLOW LOCKED / PRESENTATION DEFERRED**

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

## 4. Phase 7A — Relic Runtime

### 4.1 Required runtime model

Implement the Relic definition/runtime/container split:

```text
URelicData
URelicInstance
URelicContainer
```

Required ownership model:

```text
URelicData
= immutable shared definition/configuration

URelicInstance
= battle/runtime identity and mutable runtime state

URelicContainer
= authoritative relic membership/runtime owner
```

Allowed fields/responsibilities include:

```text
RelicId
DisplayName
Description
Definition-owned trigger configuration
RuntimeSequence
battle owner/context
runtime counters/state
stable debug identity
```

### 4.2 Hard ownership rules

Do not store mutable battle state in `URelicData`.

Wrong:

```text
URelicData.SundialCounter = 2
```

Correct:

```text
URelicInstance(Sundial#17).ShuffleCounter = 2
```

Do not model Relics as fake Status instances merely to reuse Status infrastructure.

Required conceptual separation:

```text
StatusData      != RelicData
StatusInstance  != RelicInstance
StatusContainer != RelicContainer
```

### 4.3 Runtime identity

Relic runtime identity must be deterministic and suitable for mixed Trigger ordering.

Tests must not assume RuntimeSequence values are contiguous unless the allocator contract explicitly guarantees contiguity.

The safe assumption is:

```text
new runtime identities are unique
and monotonic according to the battle allocator contract
```

### 4.4 7A exit condition

```text
Relics can exist as deterministic battle-runtime instances
without Trigger behavior or Presentation dependencies.
```

Focused runtime tests must pass before 7B is treated as stable.

---

## 5. Phase 7B — Trigger Source Integration

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

### 5.1 Mixed source collection is one ordering domain

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

### 5.2 No speculative universal framework

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

### 5.3 7B exit condition

```text
Status and Relic Trigger sources participate in one deterministic
collection/filter/sort/reaction path without either source masquerading
as the other and without changing existing Status semantics.
```

Mixed Status + Relic ordering tests are mandatory.

---

## 6. Fundamental Phase 7 mutation rule — Triggers decide, Actions mutate

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

### 6.1 Sundial counter mutation must be Action-driven

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

### 6.2 Exact-instance safety

Queued Relic mutation Actions must retain/validate enough identity to avoid mutating a replacement runtime Relic instance accidentally.

A stale queued Action must fail soft/no-op rather than retarget a newer Relic instance solely because it shares the same `RelicId`.

---

## 7. Phase 7C — Sundial Gameplay Vertical Slice

Sundial is the first required Phase 7 vertical slice.

### 7.1 Event source

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

### 7.2 Required gameplay behavior

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

### 7.3 Energy mutation path

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

### 7.4 Sundial focused acceptance

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

### 7.5 7C exit condition

```text
Sundial gameplay behavior is deterministic and fully testable without UMG,
without Presentation dependencies and without Trigger-side mutation.
```

---

## 8. Phase 7D — Multi-Relic / Architecture Validation

Sundial alone is not sufficient proof that a general Relic architecture exists; it could still hide Sundial-specific assumptions.

Phase 7D therefore validates at least one additional real Relic behavior or an equivalent scenario that proves multiple Relic Trigger sources coexist correctly.

### 8.1 Default second validation relic: Abacus

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

### 8.2 Multi-Relic behavior

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

### 8.3 Required mixed-source validation

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

### 8.4 7D exit condition

```text
The Relic framework supports more than one concrete Relic and
mixed Status + Relic Trigger ordering without content-specific dispatch code.
```

---

## 9. Modifier-source boundary

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

## 10. Phase 7E — Early Gameplay Regression Gate

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

## 11. Phase 7P — Relic Presentation (deferred)

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

## 12. Phase 7R — Full Acceptance / Regression

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

## 13. Full runtime flow

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
             /             \       │
         Status             Relic   │
             \             /       │
                    ▼              │
          deterministic sort       │
                    │              │
                    ▼              │
               Trigger rule        │
                    │              │
                    ▼              │
         Build Reaction Action     │
                    │              │
                    ▼              │
            BattleActionQueue      │
                    │              │
                    ▼              │
              Action Execute       │
                    │              │
         ┌──────────┴──────────┐   │
         ▼                     ▼   │
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

## 14. Explicitly forbidden early-development scope

The following work must **not** be finalized on the early Gameplay branch unless a later explicit project decision changes this boundary.

### 14.1 Relic Presentation implementation

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

### 14.2 Relic HUD / UMG

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

### 14.3 Formal Phase 8 combo validation

Do not start the formal Phase 8 acceptance scenario on this branch:

```text
Pommel Strike+
+
Pommel Strike+
+
Sundial
```

Phase 8 must validate architecture **and presentation** together after Phase 7 Gameplay and the required Phase 6UI-A presentation path are stable.

### 14.4 Broad framework generalization

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

### 14.5 UI-A2D redesign

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

## 15. Durable architecture rules

All Phase 7 work must preserve these rules.

### 15.1 Events notify; Actions mutate

Relic Triggers may observe committed Events and build reaction Actions.

Relic Triggers must not directly mutate authoritative state.

### 15.2 Relic is not Status

Relic and Status remain separate runtime domains.

Shared behavior may be extracted only through narrow contributor boundaries justified by both source families.

### 15.3 No world search

Do not use Gameplay-time actor discovery to recover battle dependencies.

Forbidden examples:

```text
GetAllActorsOfClass
world search for BattleManager
Outer-chain inference as authoritative Battle identity
singleton/global Relic manager
```

Pass explicit battle/context dependencies through the existing Action/Event/Trigger paths.

### 15.4 Determinism

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

### 15.5 Definition/runtime separation

Relic definitions are immutable configuration.

Runtime counters and other mutable battle values belong to Runtime instances or another explicitly owned battle-runtime object.

### 15.6 Fail soft

Invalid or stale Relic runtime inputs should normally degrade to no-op/fail-soft behavior unless a true framework invariant has been violated.

Ordinary Relic no-op behavior must not cause a Gameplay ResolutionFault.

### 15.7 No Action queue driving from content

Relic Triggers/definitions may build Actions, and Actions may enqueue required dependent work according to existing queue rules, but Relic content must not directly pump/advance the queue.

---

## 16. Parallel-development file ownership guidance

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

## 17. Early branch merge boundary

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

## 18. Agent instruction summary

Any Agent working on Phase 7 should follow this compact rule set:

```text
1. Treat Phase 7 as a Relic architecture phase, not merely a Sundial feature.
2. Keep RelicData / RelicInstance / RelicContainer separate from Status.
3. Use stable Phase 5/6 Gameplay contracts.
4. Collect Status + Relic Trigger contributions into one ordering domain.
5. Preserve Priority → RuntimeSequence → LocalTriggerIndex ordering.
6. Do not order by source family.
7. Relic Triggers decide/build Actions; they never mutate Gameplay or Relic counters directly.
8. Queued Relic mutation must validate exact runtime identity and fail soft when stale.
9. First concrete Relic is Sundial.
10. Sundial must reuse FDeckShuffledEvent; do not invent another shuffle event.
11. Initial setup shuffle does not qualify for Sundial.
12. Use a formal queued Energy Action for Sundial reward; no direct Energy shortcut.
13. Validate repeated Sundial cycles.
14. Phase 7D should validate a second Relic; default choice is Abacus.
15. Test mixed Status + Relic + multi-Relic deterministic ordering.
16. Extract Trigger contributor abstraction only as narrowly as Status + Relic require.
17. Do not generalize Modifier sources until a real Relic Modifier requires it.
18. Do not design/finalize Relic Presentation during 7A–7E.
19. Do not modify UI-A2D contracts to make Phase 7 easier.
20. Do not start formal Phase 8 combo acceptance.
21. Keep focused Automation and affected Phase 5/6 regressions green.
22. After 7A–7E, claim Phase 7 Gameplay COMPLETE / VALIDATED at most.
23. Claim Phase 7 COMPLETE only after 7P + final 7R acceptance.
```
