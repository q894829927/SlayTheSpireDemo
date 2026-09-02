# Phase 7A — Relic Runtime Validation

Date: **2026-09-02**

Status: **COMPLETE / VALIDATED / SEALED**

Validated implementation HEAD before this evidence-only documentation update:

```text
86de988bef8e85c17d6197394f72cf756627e693
```

## Scope

Phase 7A established the battle-scoped Relic runtime foundation only:

```text
URelicData
URelicInstance
URelicContainer
ABattleManager-owned PlayerRelicContainer
explicit StartBattle setup
configured Starting Relic authored order
battle-wide RuntimeSequence allocation
logical RelicId identity
exact runtime-instance membership
restart isolation
```

Phase 7A does not contain Dispatcher Relic-source integration, Sundial behavior, positive Energy gain, Relic Presentation or Relic HUD work.

## Accepted validation evidence

User-reported UE 5.8 validation on 2026-09-02:

```text
Development Editor Build                         PASS
SlayTheSpireDemo.Phase7.RelicRuntime            5/5 PASS
Manual PIE                                      NOT REQUIRED FOR 7A
```

Focused tests:

```text
MembershipAndIdentity
InvalidAndReset
DefinitionIsolation
BattleRestartLifecycle
StartingRelicsPrecedeLaterStatus
```

The final test locks the cross-source setup-order contract introduced by the Phase 7 design review:

```text
StartBattle
→ reset battle RuntimeSequence allocator
→ initialize combatant StatusContainers
→ explicitly initialize configured Starting Relics in authored order
→ later runtime Status creation receives a later RuntimeSequence
```

`GetPlayerRelicContainer()` is a read accessor only and is not an initialization boundary.

## Acceptance conclusion

The required 7A validation budget is satisfied. No Phase6R, A2D5, Shipping, Legacy parity or manual PIE rerun is required by the 7A contract.

Therefore:

```text
Phase 7A Relic Runtime: COMPLETE / VALIDATED / SEALED
Phase 7B Status + Relic Trigger Sources: NEXT / NOT STARTED
```

Phase 7 overall remains **IN PROGRESS**.
