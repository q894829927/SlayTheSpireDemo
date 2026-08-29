# Battle UI Rules

Applies to `Source/SlayTheSpireDemo/UI/**`.

Read `docs/Phase6UIA2EImplementation.md` and `docs/UIA2ERemainingSteps.zh-CN.md` before current UI-A2E work.

## Authority Boundary

UI never owns Gameplay truth. Normal UI uses formal Query/Request APIs and must not directly construct or enqueue authoritative `BattleAction` objects.

Query results are advisory. Request APIs revalidate current authoritative state. `AcceptedForResolution` means accepted into Gameplay resolution, not that effects or playback are complete.

## ViewModel

The ViewModel stores frozen player-facing display state, transient presentation/input state and latest-only weak runtime bindings used to submit current Requests. It is not a second Gameplay model.

Historical display comes only from `FPresentationStateSnapshot` plus the active Record payload. It must not query mutable Gameplay objects or advance ahead of the currently playing Record.

Runtime input bindings are not historical state. Refresh them only after Presentation catches up to the newest matching `(BattleId, StateRevision)`.

When Presentation is enabled, `OnReadStateReady` must not bypass Presenter/Controller and directly apply live state to the HUD ViewModel.

## Committed Presentation

The committed-history flow is:

```text
Record payload
→ transient presentation
→ exact-token completion callback
→ reducer advances working ViewModel state
→ Envelope FinalSnapshot reconciliation
```

PresentationId is visual mapping identity, not Gameplay identity. Historical Status identity uses `TargetPresentationId + StatusId + RuntimeSequence`; never match only by StatusId or array index.

Resolved combatant PresentationIds are non-empty, battle-scoped unique and immutable for the battle after the first exact frozen baseline.

`PresentationUnavailable` still initializes the ViewModel enough for the normal HUD/error surface, disables input and shows a clear development-facing error.

## Input and Interaction

Phase 6UI-A uses explicit card selection followed by legal-target selection. Enemy-target and Self-target cards use Gameplay-provided public LegalTargets. Widget mapping uses PresentationId; Requests submit current runtime Gameplay identity and revalidate.

Presentation may lock the View while Gameplay is request-eligible. Unlock only after the Controller catches up to the newest matching revision and authoritative Gameplay remains request-eligible.

## Preview Phase Boundary

Do not resume unfinished A3 Preview work before UI-A2E is complete.

When A3 resumes, use the name **Target-Specific Current-State Preview**. Preview construction belongs to a Gameplay/read Query boundary. ViewModel/UMG own selection, hover/focus, clearing and display only; they must not iterate CardEffects or reimplement Damage/Block/Energy legality rules.

The first Preview model uses a flat Blueprint-friendly Operations array. It reports supported values for the current `(BattleId, StateRevision, Card, Target)` and does not promise the final outcome of the whole card Resolution. Damage preview is resolved incoming damage before Block absorption, not predicted HP loss.

On BattleId/StateRevision change, clear selection, legal targets and Preview rather than retaining/recomputing selection across revisions.
