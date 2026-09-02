# Battle UI Rules

Applies to `Source/SlayTheSpireDemo/UI/**`.

Read `docs/Phase6UIA3Implementation.md`, `docs/Phase6UIA3CardFacePreviewAmendment.md`, `docs/Phase6UIA2EImplementation.md`, and `docs/LegacyUIPreservationPolicy.md` before active A3 battle-UI work.

## Legacy UI Preservation

The retained Legacy battle UI assets are deprecated reference/recovery artifacts only:

```text
/Game/SlayTheSpireDemo/UI/Out/Legacy/WBP_BattleHUD
/Game/SlayTheSpireDemo/UI/Out/Legacy/WBP_BattleCard
/Game/SlayTheSpireDemo/UI/Out/Legacy/WBP_BattleStatus
```

Normal forward development must use only the Native battle UI stack. Do not add new runtime references to the Legacy assets, restore a Legacy Presenter/default, dual-write behavior into Legacy, use Legacy as a new test execution target, or copy new Native behavior back into Legacy for parity.

Production runtime Legacy HUD/Card/Status dependency count must remain `0`. Opening the Legacy assets for inspection or referring to them in historical documentation is allowed and is not runtime fallback.

R14-B destructive removal is not required under the current project decision and remains not authorized. Do not delete, rename, move, or fix redirectors for the retained Legacy assets without a new explicit user authorization. An emergency Legacy recovery is also a new explicit user decision; asset existence alone is never authorization to restore the Legacy path.

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

UI-A2E is complete and sealed. Active A3 work is authorized under `docs/Phase6UIA3Implementation.md` plus the later explicit UX amendment in `docs/Phase6UIA3CardFacePreviewAmendment.md`. The amendment controls A3-5 visible presentation where the two documents differ.

Use the name **Target-Specific Current-State Preview**. Preview construction belongs to a Gameplay/read Query boundary. ViewModel/UMG own selection, preview-target nomination, hover/focus, clearing and display only; they must not iterate CardEffects or reimplement Damage/Block/Energy legality rules.

The first Preview model uses a flat Blueprint-friendly Operations array. It reports supported values for the current `(BattleId, StateRevision, Card, Target)` and does not promise the final outcome of the whole card Resolution. Damage preview is resolved incoming damage per hit before Block absorption, not predicted HP loss.

The visible A3-5 Preview belongs to the currently selected Native Hand card. Gameplay-resolved supported operation values may temporarily replace matching semantic values in that card face. Do not render a separate Damage/Block/Energy Preview label.

`OV_PlayArea` is committed A2-only. A3 Preview must never add a child to it or reuse A2 damage-number presentation as a pre-commit surface.

Energy/cost remain valid ImmediatePreview DTO fields for Gameplay-owned legality but are not a standalone visible A3-5 loss preview.

Inspection and Preview are separate lifecycles. Do not reuse combatant/status inspection events as PreviewTarget ownership merely because hover may drive both.

On BattleId/StateRevision change, clear selection, legal targets, PreviewTarget and Preview rather than retaining/recomputing selection across revisions.

On accepted authoritative submission, restore/clear the pre-commit card-face Preview before A2 committed playback takes visual ownership.
