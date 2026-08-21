# Phase 7 Early Development Boundary — Relic Gameplay

Date: **2026-08-22**

Status: **EARLY GAMEPLAY DEVELOPMENT ALLOWED / PRESENTATION DEFERRED**

Branch: `phase7-relic-gameplay`

This document defines the temporary development boundary for starting Phase 7 Relics before Phase 6UI-A is fully closed.

The intent is to use the already-validated Phase 5 modifier framework and Phase 6 event/trigger architecture to develop the Relic Gameplay vertical slice in parallel with the remaining Phase 6UI-A2D work, without creating dependencies on unstable Presentation code.

This document does **not** change the project-wide phase order. Phase 7 remains formally planned after Phase 6UI-A. Work performed under this branch is an intentionally isolated early Gameplay slice that may be merged only after the acceptance conditions in this document are satisfied.

---

## 1. Current dependency boundary

The current relevant dependency graph is:

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
Phase 6UI-A2D          Phase 7 Early Gameplay
IN PROGRESS            ALLOWED ON THIS BRANCH
          |                 |
          +--------+--------+
                   |
                   v
       Full Presentation Integration
                   |
                   v
              Phase 8
```

Phase 7 Gameplay may depend on stable Gameplay contracts from Phases 1–6.

Phase 7 early development must **not** depend on unfinished UI-A2D Status/Terminal Presentation internals.

---

## 2. Allowed scope

The following work is explicitly allowed on this branch.

### 2.1 Relic definition/runtime model

Allowed:

```text
URelicData
URelicInstance
URelicContainer
Relic runtime identity
Relic battle ownership/lifetime
Relic deterministic ordering metadata
Relic debug labels/helpers required by tests
```

Required ownership model:

```text
URelicData
= immutable shared definition data

URelicInstance
= battle/runtime state

URelicContainer
= authoritative relic membership/runtime owner
```

Do not store mutable runtime state in `URelicData`.

### 2.2 Relic trigger contribution

Phase 7 introduces the first real non-Status trigger source.

It is allowed to extract the **smallest contributor/collector boundary** required to combine:

```text
Status trigger sources
+
Relic trigger sources
```

The resulting flow must remain conceptually:

```text
BattleEvent
↓
collect current trigger contributions
↓
filter
↓
deterministic sort
↓
Build Reaction Actions
↓
atomic reaction batch insertion
↓
BattleActionQueue
```

Existing deterministic trigger ordering must remain:

```text
Priority
→ RuntimeSequence
→ LocalTriggerIndex
```

Do not create a universal gameplay-effect registry or a generic context system merely because Relics are the second source family.

### 2.3 Sundial vertical slice

Sundial is the first required Phase 7 validation target.

Allowed gameplay flow:

```text
successful discard-to-draw Shuffle commit
↓
existing FDeckShuffledEvent
↓
Sundial trigger observes event
↓
Sundial runtime counter advances
↓
every third qualifying shuffle
↓
queue Energy gain through the formal Gameplay Action path
↓
authoritative Energy commit
```

The implementation must reuse the existing Phase 6C `FDeckShuffledEvent` contract.

Do not invent a second shuffle notification path for Sundial.

Initial battle setup shuffle remains governed by the existing Phase 6C semantics and must not become a qualifying `DeckShuffled` event merely for Sundial convenience.

### 2.4 Minimal energy action support

If Sundial exposes a concrete gap in the existing formal Gameplay API, the smallest reusable Energy mutation Action may be added.

Requirements:

```text
Gameplay mutation remains Action/Queue driven
Energy authority remains in Gameplay
existing Energy presentation contracts are not redesigned
no Sundial-specific energy mutation shortcut
```

Prefer a generic reusable Energy Action rather than putting Energy mutation directly inside a Relic trigger.

### 2.5 Focused Automation

Allowed test coverage includes:

```text
Relic runtime create/ownership
stable RuntimeSequence identity
Status + Relic trigger collection
combined deterministic trigger ordering
Sundial shuffle counter
Sundial third-shuffle activation
no activation before threshold
repeated threshold cycles
initial setup shuffle exclusion
reaction Action ordering
Energy gain correctness
stale/invalid runtime fail-soft behavior
queue drains normally
no Gameplay ResolutionFault for ordinary no-op cases
```

Tests must not assume runtime sequences are contiguous unless the allocator contract explicitly guarantees that property.

---

## 3. Explicitly forbidden scope

The following work must **not** be implemented on this early branch unless a later explicit project decision changes this document.

### 3.1 Relic Presentation

Do not add or finalize:

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

Reason:

Phase 6UI-A2D is still closing the complete committed-Presentation pipeline, reducer behavior and terminal playback rules. Relic Presentation must consume that finished architecture rather than create a parallel or premature design.

### 3.2 Relic HUD / UMG

Do not implement:

```text
Relic HUD bar
Relic icon widgets
hover tooltip behavior
relic activation animation
relic VFX/SFX
relic counter animation
Blueprint playback wiring
PIE visual acceptance for relics
```

Temporary debug-only output is allowed only when it does not become a second gameplay/UI contract.

### 3.3 Phase 8 combo validation

Do not start the formal Phase 8 acceptance scenario on this branch:

```text
Pommel Strike+
+
Pommel Strike+
+
Sundial
```

Phase 8 must validate architecture **and presentation** together after Phase 7 Gameplay and the required Phase 6UI-A presentation path are stable.

### 3.4 Broad framework generalization

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

Only extract abstractions that are concretely required to support both existing Status sources and the new Relic source.

### 3.5 UI-A2D redesign

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

If Phase 7 Gameplay exposes an apparent Presentation limitation, record it as a follow-up instead of solving it by changing UI-A2D from this branch.

---

## 4. Durable architecture rules

All Phase 7 early work must preserve the following rules.

### 4.1 Events notify; Actions mutate

Relic triggers may observe committed events and build reaction Actions.

Relic triggers must not directly mutate authoritative gameplay state.

Correct:

```text
DeckShuffledEvent
↓
Sundial Trigger
↓
Build GainEnergyAction
↓
BattleActionQueue
↓
Energy Commit
```

Forbidden:

```text
DeckShuffledEvent
↓
Sundial Trigger
↓
Player.Energy += 2
```

### 4.2 Relic is not Status

Do not model Relics as fake Status instances merely to reuse Status infrastructure.

Required conceptual separation:

```text
StatusData != RelicData
StatusInstance != RelicInstance
StatusContainer != RelicContainer
```

Shared behavior should be extracted through narrow contributor interfaces only when both real source families need it.

### 4.3 No world search

Do not use Gameplay-time actor discovery to recover battle dependencies.

Forbidden examples:

```text
GetAllActorsOfClass
world search for BattleManager
Outer-chain inference as authoritative Battle identity
singleton/global Relic manager
```

Pass explicit battle/context dependencies through the existing action/event/trigger paths.

### 4.4 Determinism

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

### 4.5 Definition/runtime separation

Relic definitions are immutable configuration.

Runtime counters such as Sundial shuffle progress belong to `URelicInstance` or another explicitly owned battle-runtime object, never to shared DataAssets.

### 4.6 Fail soft

Invalid or stale Relic runtime inputs should normally degrade to no-op/fail-soft behavior unless a true framework invariant has been violated.

Ordinary Relic no-op behavior must not cause a Gameplay ResolutionFault.

---

## 5. Recommended implementation slices

Implement early Phase 7 in this order.

### Phase 7A — Relic Runtime

```text
URelicData
URelicInstance
URelicContainer
battle initialization/ownership
runtime identity
focused runtime tests
```

Exit condition:

```text
Relics can exist as deterministic battle-runtime instances
without trigger behavior or Presentation dependencies.
```

### Phase 7B — Trigger Contributor Boundary

```text
extract minimum shared trigger contributor/collector boundary
adapt Status contribution without semantic changes
add Relic contribution
preserve Priority → RuntimeSequence → LocalTriggerIndex ordering
focused combined-order tests
```

Exit condition:

```text
Status and Relic trigger sources can participate in one deterministic
collection/sort path without either source masquerading as the other.
```

### Phase 7C — Sundial Gameplay Vertical Slice

```text
Sundial Data/Instance
shuffle counter runtime state
listen to existing FDeckShuffledEvent
third qualifying shuffle activation
generic queued Energy gain
repeat-cycle behavior
focused Automation
```

Exit condition:

```text
Sundial gameplay behavior is deterministic and fully testable without UMG.
```

### Phase 7R — Early Regression Gate

Run the smallest meaningful affected regression set covering:

```text
Phase 5 modifier/status behavior
Phase 6 event/trigger ordering
Phase 6C DeckShuffled semantics
Phase 7 Relic runtime/trigger/Sundial tests
```

Do not claim full Phase 7 completion from this gate.

---

## 6. Parallel-development file ownership guidance

Phase 6UI-A2D and Phase 7 may proceed in parallel, but Phase 7 should minimize edits to files under active Presentation development.

Prefer Phase 7-owned additions under a dedicated runtime area such as:

```text
Source/SlayTheSpireDemo/Relics/
```

Likely legitimate shared touch points include:

```text
BattleManager initialization
BattleEventDispatcher / trigger collection
Battle runtime identity allocation
Build.cs when required
Automation test module
```

Avoid touching Presentation files unless required for compilation only.

If both branches modify the same shared Gameplay file, Phase 7 must preserve the existing public contract and keep the patch minimal to simplify later rebase/integration.

---

## 7. Merge boundary

Development on `phase7-relic-gameplay` may proceed before Phase 6UI-A closes.

However, merging this branch into the main development line should require all of the following:

```text
Phase 7A runtime tests PASS
Phase 7B combined trigger-order tests PASS
Phase 7C Sundial gameplay tests PASS
affected Phase 5/6 regressions PASS
no required Relic Presentation dependency
no UI-A2D contract redesign
no Phase 8 special-case combo code
```

If UI-A2D modifies a shared Gameplay contract before merge, rebase Phase 7 onto the updated main line and rerun the affected regression gate.

---

## 8. Completion claims

Allowed claim after this early branch is validated:

```text
Phase 7 Relic Gameplay vertical slice implemented and validated.
```

Do **not** claim:

```text
Phase 7 COMPLETE
```

until the project formally finishes the remaining required Relic integration/presentation acceptance defined after Phase 6UI-A stabilization.

---

## 9. Agent instruction summary

Any Agent working on this branch should follow this compact rule set:

```text
1. Work only on Relic Gameplay/runtime/trigger infrastructure.
2. Use existing Phase 5/6 Gameplay contracts.
3. First concrete relic is Sundial.
4. Reuse FDeckShuffledEvent; do not invent another shuffle event.
5. Relic triggers build Actions; they never mutate gameplay directly.
6. Relic is a separate source family; do not disguise it as Status.
7. Extract only the minimum Status + Relic contributor boundary.
8. Preserve deterministic ordering.
9. Do not design Relic Presentation yet.
10. Do not modify UI-A2D contracts to make Phase 7 easier.
11. Do not start formal Phase 8 combo acceptance.
12. Add focused Automation and keep affected Phase 5/6 regressions green.
```
