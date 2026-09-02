# Codex Goal Checkpoint — Phase 7 Relics

Last updated: **2026-09-02**

## Goal

Implement Phase 7 Relics as a first-class deterministic Gameplay system, beginning with Sundial, without reopening sealed Phase 6UI-A contracts or modeling Relics as Statuses.

## Current status

```text
Phase 6UI-A: COMPLETE / VALIDATED / SEALED
Phase 6UI-A3: COMPLETE / VALIDATED / SEALED

Phase 7 Relics: IN PROGRESS
Phase 7 design: SEALED
7A Relic Runtime: IMPLEMENTED / VALIDATION PENDING
7B Status + Relic Trigger Sources: NOT STARTED
7C Sundial + GainEnergyAction: NOT STARTED
7D Relic Read/Frozen/Native UI: NOT STARTED
```

Active authority:

```text
docs/Phase7RelicsImplementation.md
```

Final Phase 6UI-A authority remains:

```text
docs/Phase6UIA3Seal.md
```

## Phase 7 design review closure

The 2026-09-02 pre-7A review was incorporated into the authority and the design is now sealed for the first vertical slice.

Locked clarifications:

```text
1. No first-version RelicCounterChanged / RelicTriggered Presentation Record.
   During active A2 playback the Relic counter remains at the last completed historical snapshot.
   Envelope completion reconciles Relic state to FinalSnapshot.

2. Starting Relics are instantiated explicitly during StartBattle after the battle RuntimeSequence allocator reset.
   GetPlayerRelicContainer() is not a lazy initialization boundary.
   Configured Starting Relics therefore receive earlier RuntimeSequences than later runtime Status creation.

3. Future HUD Relic DTO uses bShowCounter / Counter / CounterMax.
   Native UI must not recognize Sundial by RelicId merely to decide counter visibility.

4. USundialTrigger will freeze ShufflesRequired / EnergyGain into USundialAdvanceAction when building the reaction.
   The Action does not rediscover/reinterpret Trigger configuration during Execute.

5. Frozen/HUD Relic DTOs exclude mutable Gameplay runtime pointers but may retain immutable presentation asset references.

6. GainEnergyAction receives its own focused primitive tests in 7C rather than being proved only through Sundial.
```

Design seal/update commit:

```text
d00ae6c7793291ca4dee4143c260043ec8544871
docs(phase7): seal relic design boundaries before 7a
```

## phase7-relic-gameplay branch integration decision

The historical `phase7-relic-gameplay` branch was not merged wholesale. At review time it was 28 commits ahead but 335 commits behind current `main`.

Its Phase 7A runtime foundation was selectively ported because the core definition/runtime/container model remained compatible, while its old lazy `GetPlayerRelicContainer()` initialization was deliberately rejected.

Not imported:

```text
old Phase 7 authority documents
lazy BattleId/getter initialization
7B/7C/Presentation work
```

## 7A implementation now on main

Runtime foundation:

```text
URelicData
- RelicId
- DisplayName
- Description
- editor validation for non-empty RelicId

URelicInstance
- Definition
- explicit Battle context
- RuntimeSequence
- logical RelicId / exact runtime identity accessors

URelicContainer
- Initialize / Reset
- Invalid / Duplicate / Added typed add result
- one active member per RelicId
- exact-instance membership query
- deterministic ordered TArray membership
- ABattleManager::AllocateRuntimeSequence()
```

Battle ownership/setup:

```text
ABattleManager owns transient PlayerRelicContainer
DebugStartingRelics is the current demo setup input

StartBattle
→ reset NextRuntimeSequence = 1
→ initialize Player / Enemy StatusContainers
→ establish new BattleId/state
→ InitializeRelicsForBattle()
→ instantiate DebugStartingRelics in authored order
→ later runtime Status creation receives later RuntimeSequence
→ continue Presentation/opening-hand flow
```

`GetPlayerRelicContainer()` now only returns the already-owned pointer and has no initialization side effects.

Key implementation commits:

```text
348a1417ec366d2f6a83135547a624ebb47c00e3
feat(phase7a): wire explicit relic runtime ownership

e009cde85b9a339bb0001a02c29ff6d18aacab44
feat(phase7a): initialize starting relics during battle setup

51319c0a1d7387cfb3c8fa65fdbf2cdda2494a42
test(phase7a): cover relic runtime and setup ordering
```

Roadmap status update:

```text
7581d113a8c103320fbcd4136ee43ce48ead220e
docs(phase7): mark relic runtime implementation pending validation
```

## 7A focused Automation

Prefix:

```text
SlayTheSpireDemo.Phase7.RelicRuntime
```

Current five tests:

```text
MembershipAndIdentity
InvalidAndReset
DefinitionIsolation
BattleRestartLifecycle
StartingRelicsPrecedeLaterStatus
```

The final test explicitly proves that the Relic getter does not lazily initialize before StartBattle, configured Relics preserve authored order, and a Status created after setup receives a later battle-wide RuntimeSequence.

## Validation state

No current-main UE build or Phase 7A Automation run has been performed or claimed yet.

Historical `phase7-relic-gameplay` build/4-test results are background evidence only; they do not validate the selectively ported current-main implementation.

The required 7A gate is intentionally small:

```text
1. Development Editor Build once.
2. Run SlayTheSpireDemo.Phase7.RelicRuntime once; expected 5 focused tests.
3. No manual PIE gate for 7A.
4. Record the result and STOP.
```

Do not run Phase6R, A2D5, Shipping, Legacy parity or the historical branch's old 84-test regression batch unless a concrete failure directly invalidates one of those sealed contracts.

## Next exact action

USER ACTION REQUIRED:

Run the current `main` Development Editor build. If it passes, run exactly:

```text
SlayTheSpireDemo.Phase7.RelicRuntime
```

If Build and all five focused tests pass, record 7A as **COMPLETE / VALIDATED / SEALED** and move the active phase boundary to **7B — Status + Relic Trigger Sources**.

Do not begin 7B code before the 7A focused gate is accepted.