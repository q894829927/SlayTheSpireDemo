# Presentation Runtime Rules

Applies to `Source/SlayTheSpireDemo/Presentation/**`.

For historical Record/Envelope changes, consult `docs/Phase6UIA2Implementation.md` and `docs/Phase6UIA2EImplementation.md`. For Selection/group playback, consult `docs/SelectionPresentationGroupDesign.md` and its implementation plan. Read the portions relevant to the changed contract; A2E is sealed history.

Group rules below define the required design, not proof that Group playback is implemented or authorization to start its planned stages.

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

`BattlePresentationController` owns playback sequencing and its bounded post-delivery backlog. Blueprint/Widgets do not scan future Envelope Records, infer groups, or reorder Records on their own.

Reducer application order MUST always remain the committed `PresentationSequence` order.

Visible playback normally follows the same order. The only authorized exception is an explicitly committed, complete and Controller-validated `PresentationGroup`: the Controller may co-present/look ahead to that group's own frozen members from the already sealed Envelope without reducing those future members early. This exception MUST NOT consume, skip, reorder or mark interleaved ungrouped Records as played, and MUST NOT be inferred from CardId, Effect type, destination, adjacency, timing or Widget state.

Before a non-contiguous group is offered for co-presentation, Controller preflight MUST prove not only chronological reducer validity but also future-member visual independence. If any interleaved ungrouped Record directly moves, plays, replaces, or otherwise modifies an exact group member whose own reducer cursor has not yet been reached, the entire group is ineligible for lookahead and degrades to sequential playback. A dry-run reducer that merely succeeds is not sufficient evidence that early visible consumption is safe.

If a future group member is visually consumed before its reducer cursor is reached, its formal historical visual MUST remain Presentation-suppressed by exact committed identity until that member is reduced. Intermediate `ApplyPresentationSnapshot` / HUD rebuilds must not make the already-consumed visual reappear. Suppression is Presentation-only state and must be cleared on exact member reduction or global reconciliation boundaries such as Skip, active-envelope failure reconciliation, envelope replacement/completion, battle replacement or Presentation-unavailable fallback.

Each Envelope applies its own FinalSnapshot. Do not rebuild display state from latest Gameplay after playback catches up. Refresh only latest-revision live input bindings after the Controller reaches the newest matching `(BattleId, StateRevision)`.

## Playback Unit and Token

Controller-facing playback may be a single Record or an explicitly validated group, but `UBattleHUDWidgetBase` remains the hardening boundary for both. Concrete HUDs MUST NOT bypass the base wrapper or notify `BattlePresentationController` directly.

Record and group playback MUST share one tracked Presentation-playback owner, not independent record/group owners. The tracked unit includes exact token identity and unit kind so stale callbacks from a previous group cannot clear or complete a newer single Record, and vice versa.

Async Blueprint/native playback returns `true` only when valid playback actually started. Successful completion calls `NotifyPresentationFinished` with the exact active token through the base Widget surface.

The existing hardening applies identically to every playback unit:

- exact-token ownership before entering concrete playback;
- deferred completion forwarding so synchronous Widget callbacks cannot re-enter Controller sequencing;
- stale, duplicate, old-Battle, post-Skip and post-replacement callback rejection;
- timeout bound to the exact unit token/generation;
- cancellation that targets only the currently tracked unit;
- Widget replacement/destruction that cannot cancel playback owned by a newer Widget.

Normal group completion and group timeout are different terminal paths. A group timeout MUST NOT call the ordinary single-record/group-success completion path and MUST NOT reduce only the leader. After exact-token validation, timeout handling cancels the exact tracked group unit, cleans every child visual, discards `VisuallyPresented` ownership for that failed unit, and atomically reconciles the current `ActiveEnvelope` to its own `FinalSnapshot` while clearing all group suppression/handoff state owned by that envelope.

Active-group timeout reconciliation MUST preserve already queued later Envelopes. Do not reuse a helper whose semantics reset the whole playback queue (for example a global collapse/skip helper) if that would discard valid backlog. After the current envelope is reconciled and marked complete, normal playback may continue with the next queued envelope.

No intermediate ViewModel/HUD broadcast may occur between clearing failed-group suppression and applying the active envelope final snapshot if that gap could make a previously suppressed historical card reappear for a frame.

Cancel/reconcile restores the historical ViewModel/sealed-snapshot contract. It must not commit Gameplay, fake normal completion or complete a stale token.

## Failure Separation

`PresentationUnavailable` is a visible UI-only state. `ResolutionFault` is a Gameplay/framework resolution failure. They are not interchangeable.

Presentation backlog, malformed/incomplete/interfered group metadata, group rejection, timeout, missing callback, Widget loss, skip or disablement causes Presentation fallback/catch-up only. These failures do not request a Gameplay `ResolutionFault` by themselves.
