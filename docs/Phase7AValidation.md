# Phase 7A — Relic Runtime Validation

Date: **2026-08-22**

Status: **VALIDATED / COMPLETE / READY FOR PHASE 7B**

Branch: `phase7-relic-gameplay`

This document records closure of Phase 7A against the locked contract in `docs/Phase7EarlyDevelopmentBoundary.md`.

## Validation result

The Phase 7A GitHub Actions workflow completed successfully.

Validated gate:

```text
UE5.8 Editor build                     PASS
Phase7A focused Automation             4/4 PASS
Affected Phase5/6/UI-A2 regression    84/84 PASS
Static source review                   PASS
```

Focused prefix:

```text
SlayTheSpireDemo.Phase7A
```

Focused tests:

```text
Runtime.MembershipAndIdentity
Runtime.InvalidAndReset
Runtime.DefinitionIsolation
Runtime.BattleRestartLifecycle
```

## Closed Phase 7A scope

Validated runtime foundation:

```text
URelicData
URelicInstance
URelicContainer
BattleManager-owned PlayerRelicContainer
logical RelicId identity
exact runtime instance identity
battle-scoped RuntimeSequence
explicit Battle context
typed Invalid / Duplicate / Added semantics
deterministic ordered membership
DebugStartingRelics setup injection
BattleId-based restart isolation
```

The implementation remains inside the Phase 7A boundary and does not introduce:

```text
Relic Trigger definitions
Sundial behavior
DeckShuffled Relic handling
Relic counters
GainEnergyAction for Sundial
Relic Modifier contribution
Relic Presentation
Relic HUD / UMG
Phase 8 combo code
```

## Durable handoff rules for Phase 7B

Phase 7B must consume the validated 7A runtime rather than redesign it.

Required rules:

```text
1. Relic remains separate from Status.
2. Relic Trigger behavior must use the existing BattleEvent / Trigger / Reaction Action architecture.
3. Status + Relic trigger contributions must enter one deterministic ordering domain.
4. Ordering remains Priority -> RuntimeSequence -> LocalTriggerIndex.
5. Do not order by source family.
6. Do not mutate Relic runtime state inside Trigger evaluation.
7. Trigger decides/builds Actions; Actions perform authoritative mutation.
8. Do not cache Relic membership across BattleId changes.
9. Exact queued work must validate current exact runtime membership.
10. Phase 7B must not begin Sundial reward behavior or Presentation work beyond what is strictly required for trigger-source integration.
```

## Phase status

```text
Phase 7A Relic Runtime              COMPLETE
Phase 7B Trigger Source Integration READY TO DESIGN / IMPLEMENT
Phase 7C Sundial                    NOT STARTED
Phase 7D Multi-Relic Validation     NOT STARTED
Phase 7E Gameplay Regression Gate   NOT STARTED
Phase 7P Presentation               DEFERRED
Phase 7R Full Acceptance            DEFERRED
```

Phase 7 overall remains **IN PROGRESS**.
