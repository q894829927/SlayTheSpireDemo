# Presentation Runtime Rules

Applies to `Source/SlayTheSpireDemo/Presentation/**`.

Read before changing Presentation code:

- `docs/Phase6UIA2Implementation.md`
- `docs/Phase6UIA2EImplementation.md`
- `docs/UIA2ERemainingSteps.zh-CN.md` when implementing current A2E work

## Core Principle

Presentation represents committed historical facts. It is not authoritative Gameplay and Gameplay never waits for Presentation playback.

## Records and Historical State

A Presentation Record is immutable-by-contract. Historical rendering must use the frozen Record payload and the matching frozen `FPresentationStateSnapshot`.

Never reconstruct historical state from mutable `ACombatant`, `UCardInstance`, `UStatusInstance`, `UDeckRuntime`, current `ABattleManager` state or current Gameplay Query results.

One sealed Resolution owns:

```text
BattleId
ResolutionId
Origin
FinalStateRevision
Records[]
FinalSnapshot
```

Records and FinalSnapshot belong to the same historical Resolution. Never pair old Records with a newer Gameplay snapshot.

Envelope identity is `(BattleId, ResolutionId)`. Read-state public-edge identity separately includes `(BattleId, StateRevision, Presentation availability)`.

## Resolution Lifecycle

There is at most one active builder. Once Gameplay/macro work is stable, freeze/seal synchronously releases it before another `BeginResolution`; deferred public delivery is a separate lifecycle.

Ordinary validation rejection creates no Presentation Resolution. A post-validation framework fault is represented by a fault/system Resolution whose `ResolutionFault` Record is last.

If the writer is absent from Resolution start, no-history mode is valid. If an active Append fails, invalidate the whole current record batch, discard buffered unpublished Records and never seal/publish a partial Envelope. Freeze/Seal/Append failure degrades Presentation only and never becomes Gameplay `ResolutionFault`.

Sealed Envelopes awaiting deferred delivery use a battle-scoped bounded FIFO. Preserve Resolution order, clear it on battle restart and reject old-Battle entries. Overflow may collapse/skip toward the newest frozen FinalSnapshot or disable Presentation, but must not affect Gameplay.

## Record Semantics

- `CardPlayed` preserves exact Energy Before/After/CostPaid.
- Damage carries HP and consumed-Block Before/After; do not emit duplicate BlockChanged for damage-consumed Block.
- Status history preserves exact `TargetPresentationId + StatusId + RuntimeSequence` and frozen Before/After metadata.
- Initial setup shuffle/opening-hand draws emit no Presentation Records; an empty-record BattleStart Envelope applies its FinalSnapshot directly.
- Victory, Defeat and ResolutionFault are unique terminal Records and final in the Envelope.

## Controller Ownership

`BattlePresentationController` owns playback sequencing and its bounded post-delivery backlog. Blueprint/Widgets do not reorder Records.

Each Envelope applies its own FinalSnapshot. Do not rebuild display state from latest Gameplay after playback catches up. Refresh only latest-revision live input bindings after the Controller reaches the newest matching `(BattleId, StateRevision)`.

## Playback Token

Async Blueprint playback returns `true` only when valid playback actually started. Successful completion calls `NotifyPresentationFinished` with the exact active token.

Ignore stale, duplicate, old-Battle, post-Skip and post-replacement callbacks. Timeout completion is bound to the same token/generation. A stale Widget destruction callback must not skip playback owned by a replacement Widget.

Cancel/reconcile restores the historical ViewModel/sealed-snapshot contract. It must not commit Gameplay, fake normal completion or complete a stale token.

## Failure Separation

`PresentationUnavailable` is a visible UI-only state. `ResolutionFault` is a Gameplay/framework resolution failure. They are not interchangeable.

Presentation backlog, timeout, missing callback, Widget loss, skip or disablement causes Presentation catch-up/fallback only.
