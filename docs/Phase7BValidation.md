# Phase 7B — Status + Relic Trigger Sources Validation

Date: **2026-09-03**

Status: **COMPLETE / VALIDATED / SEALED**

Validated implementation state is the 7B source-neutral Trigger integration on `main` immediately before this evidence-only documentation update.

## Scope

Phase 7B changed only the Trigger runtime-source boundary:

```text
FTriggerRuntimeSource
FTriggerContext source-neutral accessors
URelicData.Triggers[]
BattleEventDispatcher Status + Relic candidate collection
combined deterministic ordering
source-neutral eligibility trace
```

It did not implement Sundial behavior, Relic counters, positive Energy gain, Relic Presentation, Relic HUD, Modifier-source generalization or a persistent Trigger Registry.

## Accepted validation evidence

User-reported UE 5.8 validation on 2026-09-03:

```text
SlayTheSpireDemo.Phase7.TriggerSources     3/3 PASS
SlayTheSpireDemo.Phase6A.Trigger          PASS
Manual PIE                                NOT REQUIRED FOR 7B
```

The new 7B suite proves:

```text
ContextCompatibility
RelicReactionParticipation
CombinedOrderingAndTrace
```

The Phase 6A Trigger prefix is the smallest existing regression surface directly affected by the Dispatcher refactor and passed without requiring the broader Phase6R aggregate.

The prescribed Development Editor build step produced a runnable current-main test binary; the user separately reported both required Automation groups passing. No build or runtime failure was reported.

## Locked acceptance conclusions

```text
Status and Relic remain distinct runtime source kinds.
Relic does not masquerade as Status.
Historical Status GetRuntimeSource()/Owner behavior remains compatible.
Relic Trigger definitions participate in the real Dispatcher.
Status + Relic candidates share one ordering domain:
    Priority → RuntimeSequence → LocalTriggerIndex
SourceKind is not an ordering key.
Eligibility trace uses SourceKind + SourceId while preserving StatusId compatibility.
Dispatcher remains snapshot-based; no persistent Trigger Registry exists.
```

Therefore:

```text
Phase 7B Status + Relic Trigger Sources: COMPLETE / VALIDATED / SEALED
Phase 7C Sundial + GainEnergyAction: NEXT / NOT STARTED
```

Phase 7 overall remains **IN PROGRESS**.
