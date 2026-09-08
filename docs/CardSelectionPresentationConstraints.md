# Card Selection Presentation Constraints

Date: **2026-09-08**

Status:

```text
IMPLEMENTED / AUTOMATED GATES PASS / MANUAL PIE PENDING / NOT SEALED
```

Scope: define the shared Native HUD / Presentation contract for player card-selection interactions before implementation continues.

This document is authoritative for the Presentation behavior of current and future player card-selection flows. It complements `docs/CardSelectionRefactorConstraints.md`, which defines Gameplay candidate capture, pending-selection, resolver, continuation, and interactive-boundary rules.

## 1. Core principle

Selection Presentation belongs to the shared Selection interaction framework.

It MUST NOT belong to:

- Warcry;
- `USelectHandCardToDrawPileTopEffect`;
- Burning Pact;
- `USelectExhaustHandCardEffect`;
- any other individual card or Effect.

The reusable interaction model is:

```text
Player Selection becomes display-eligible
→ show shared Selection UI
→ player changes transient selected RuntimeIds
→ required count is satisfied
→ Confirm becomes available
→ player explicitly confirms
→ authoritative SelectionResult is submitted
→ shared Selection Presentation owns the selected-card visual handoff
→ committed Gameplay facts determine the destination transition
→ later authored Effects / played-card cleanup continue through their normal Presentation paths
```

No `CardId`-specific Presentation sequence is permitted.

## 2. Selection and confirmation are separate player intents

Choosing enough cards MUST NOT automatically submit the Selection.

For exact-N Player Selection:

```text
selected count < required count
→ Confirm disabled

selected count == required count
→ Confirm enabled
→ Selection remains pending
→ no authoritative result submitted yet

player presses Confirm
→ submit exactly the currently selected RuntimeIds
```

The last candidate click MUST NOT double as confirmation.

The selected RuntimeId set before confirmation is transient UI state only. Gameplay remains authoritative for the pending request and legal frozen candidate set.

Selection UI MAY allow deselect/reselect while the request remains pending, subject to the current Selection policy.

## 3. Shared Selection Presentation lifecycle

All Player card selections in scope MUST reuse one shared Presentation lifecycle.

The shared layer owns:

- entering the card-selection visual mode;
- displaying the prompt and candidate affordances;
- transient selected/unselected visuals;
- Confirm enabled/disabled state;
- consuming the explicit Confirm action;
- freezing the confirmed selected RuntimeIds for visual handoff;
- leaving/closing the selection overlay;
- transferring selected card visuals into the committed post-selection transition;
- preventing the just-finished selection UI from fabricating Gameplay state.

An Effect MUST NOT implement any of those responsibilities itself.

## 4. Confirmed-card visual handoff

After confirmation, the selected cards' Presentation visuals are handed from the shared Selection interaction into the normal committed transition path.

The Selection layer identifies the exact selected card visuals by RuntimeId. It MUST NOT infer identity from CardId, widget position, click order, or current Hand array re-enumeration.

The destination and authoritative mutation are determined only by committed Gameplay / Presentation facts, such as `CardZoneChanged`.

Conceptually:

```text
confirmed selected RuntimeIds
        +
committed CardZoneChanged facts
        ↓
shared selected-card transition
```

The Effect does not choose or trigger an animation directly.

## 5. Destination transitions are generic, not Effect-specific

The Selection Presentation framework MUST dispatch the confirmed selected-card visual according to committed zone facts.

Examples:

```text
Hand → DrawPile
→ selected Hand card flies to the DrawPile visual anchor

Hand → DiscardPile
→ selected Hand card uses the generic Hand→Discard destination transition

Hand → ExhaustPile
→ selected Hand card uses the generic Hand→Exhaust destination transition
```

Future candidate zones may reuse the same Selection lifecycle, but each source/destination domain still requires a valid visual surface and transition implementation.

Adding a new Effect MUST NOT require a second copy of the Selection interaction animation when the same source/destination transition already exists.

## 6. Hand → DrawPileTop Presentation contract

For a confirmed Selection whose committed fact is:

```text
CardZoneChanged
FromZone = Hand
ToZone   = DrawPile
RuntimeId = X
```

Native Presentation MUST:

1. locate the exact selected Hand card visual for RuntimeId `X`;
2. detach that visual from normal Hand layout ownership for the transition;
3. move the card visibly from its Hand position toward the DrawPile visual anchor;
4. preserve card identity during the flight;
5. complete the visual transfer at the DrawPile area;
6. reconcile the Hand layout and DrawCount with the committed snapshot/facts.

It MUST NOT:

- use an in-place fade as the primary Hand→DrawPileTop transition;
- replay a DrawPile→Hand draw animation;
- reuse Discard/Exhaust semantics when the committed destination is DrawPile;
- mutate Gameplay to make the animation work;
- contain Warcry-specific or Effect-specific branches.

This transition is a reusable Selection/zone Presentation capability for every future selected Hand card moved to DrawPile.

## 7. Multi-selection ordering

Gameplay ordering remains authoritative.

For a frozen candidate set and canonical validated result:

```text
A, B, C
```

if committed Gameplay emits sequential Hand→DrawPile records for `A`, then `B`, then `C`, Presentation MUST consume those committed records in that order.

UI click order MUST NOT become a hidden ordering control.

The Selection layer may visually mark cards in the order the user clicked them, but the post-confirm transition sequence and final DrawPile semantics must follow committed canonical facts.

## 8. Played-card cleanup is independent from selected-card transfer

The card being played and the cards selected by its Effect are separate Presentation responsibilities.

For Warcry-style resolution:

```text
Warcry is in PlayArea
→ Draw resolves and is presented
→ shared Player Selection UI appears
→ player selects card(s)
→ player presses Confirm
→ selected card(s) use shared Hand→DrawPile transition
→ selection presentation ends
→ Warcry PlayArea visual is visible/available again
→ normal FinishCardPlay cleanup proceeds
→ Warcry uses its authored destination
```

If the played card's authored destination is Exhaust, its final disappearance MUST use the existing generic played-card Exhaust Presentation path.

The implementation MUST NOT add:

- `PlayWarcryFadeOut()`;
- a Warcry-specific Exhaust animation;
- an Exhaust animation inside `USelectHandCardToDrawPileTopEffect`;
- a combined "selected card transfer + Warcry cleanup" hard-coded sequence.

The selected-card move and the played-card cleanup are separate committed facts and must remain separate generic Presentation transitions.

## 9. Gameplay / Presentation separation

Gameplay mutation MUST remain independent of animation completion.

The intended relationship is:

```text
Gameplay commits validated Selection continuation
→ Presentation consumes immutable committed facts
→ selected-card visuals animate to their committed destination
```

Animation callbacks MUST NOT perform authoritative Hand/DrawPile/Discard/Exhaust mutation.

Presentation failure or skip/catch-up may reconcile visuals according to existing policy, but MUST NOT invent a different Gameplay result.

## 10. Interaction boundary and input-event consumption

The shared Selection Presentation starts only after the existing interactive Presentation boundary makes the pending Selection display-eligible.

A single pointer/input event MUST NOT be reused across interaction-state transitions.

Required rule:

```text
input event fast-forwards / completes prior Presentation
→ Selection becomes visible
→ that event is consumed
→ a later input event may select a card
```

Likewise:

```text
candidate click completes required selection count
→ Confirm becomes enabled
→ same click ends there
→ a later explicit Confirm input submits the Selection
```

This prevents accidental selection or confirmation caused by one event crossing multiple UI states.

## 11. Effect contract

A card Effect may configure Gameplay selection intent only, including:

- candidate source;
- Player/Random mode where supported;
- requested count/count policy;
- cancellation policy;
- semantic SelectionSource/prompt data;
- authored Continuation.

A card Effect MUST NOT configure or own:

- Confirm button behavior;
- selection overlay lifetime;
- selected-card flight animation;
- destination anchor animation logic;
- played-card cleanup animation;
- input-event consumption between Presentation states.

## 12. C1 / Warcry explicit acceptance flow

For Wave 1C-C1, the required observable flow is:

```text
Warcry played
→ Draw Gameplay commit
→ Draw Presentation is shown
→ post-Draw Hand becomes the displayed selectable Hand
→ shared Selection UI appears
→ player selects the required card(s)
→ selection does NOT auto-submit
→ Confirm becomes available
→ player presses Confirm
→ exact selected RuntimeId card(s) visibly fly Hand → DrawPile
→ selection UI ends
→ Warcry PlayArea visual is shown/available for cleanup
→ normal generic PlayArea → Exhaust Presentation runs because Warcry exhausts
→ resolution completes
```

## 13. Explicitly superseded C1 Presentation requirements

Any earlier Wave 1C-C1 plan or note that specifies:

```text
Hand → DrawPileTop
→ in-place opacity fade
→ no translation
```

is superseded by this document.

The accepted requirement is now:

```text
confirmed selected Hand card
→ shared Selection Presentation
→ visible movement to DrawPile anchor
```

Any earlier implication that Warcry needs a dedicated fade/disappear animation is also superseded. Warcry's own final disappearance is the existing generic Exhaust Presentation driven by its normal played-card cleanup fact.

## 14. Implementation scope for the next development step

The next implementation phase SHOULD be limited to the smallest reusable capabilities needed to satisfy these constraints:

1. explicit Confirm for Player card Selection;
2. shared Selection visual handoff by exact RuntimeId;
3. generic selected Hand→DrawPile transition to the DrawPile visual anchor;
4. correct transition-state input consumption;
5. preservation/reuse of the existing generic played-card Exhaust animation for Warcry cleanup;
6. focused Automation for reusable Selection behavior plus PIE acceptance for observable sequencing.

The phase MUST NOT add card-specific animation code or broaden Gameplay selection semantics beyond the already-authorized Selection refactor.

## 15. Acceptance gates

Implementation is not complete until focused coverage and PIE demonstrate at minimum:

- selecting the required count does not auto-submit;
- Confirm is unavailable before the required count and available at the required count;
- Confirm submits the exact selected RuntimeIds once;
- the selected card visual flies from Hand to the DrawPile anchor after confirmation;
- the selected card is not presented as DrawPile→Hand, Discard, or Exhaust;
- Hand/DrawCount reconcile to committed facts;
- multi-select transition order follows committed canonical record order;
- the Selection animation path is reusable and contains no Warcry/CardId/Effect-specific branch;
- Warcry cleanup reuses the existing generic Exhaust animation;
- no dedicated Warcry Exhaust/fade animation exists;
- Draw → Selection ordering remains correct;
- input used to cross into Selection or enable Confirm is not reused as the next interaction;
- Gameplay remains authoritative if Presentation is skipped/degraded according to existing policy.

No Build, Automation, or PIE gate may be marked PASS without actual execution evidence.

## 16. Production integration and confirmation repair — 2026-09-08

The production `WBP_BattleHUD_Native` still inherited `UBattleHUDWidget`, bypassing the shared Selection subclass. Candidate clicks could select transient RuntimeIds, but the production Confirm delegate called ordinary card-play confirmation and displayed `Choose a legal target.` Reparented the existing Native asset to `UBattleHUDSelectionWidget` in UE, compiled and saved it. No Legacy or card/map assets changed.

The shared HUD now dims the background, raises the selectable Hand and confirmation controls, and positions selected formal cards centrally in canonical Hand order. Deselect restores the card's Hand transform. Temporary Canvas layout/Z changes restore on submit, cancellation and destruction. Selection never submits on the final candidate click.

Confirmation captures visual centers by RuntimeId before clearing transient input. Committed Hand→DrawPile records fly from those centers to the DrawPile anchor in record order. Anchor calculations use the stationary PlayArea coordinate space; they do not measure offsets relative to the moving/scaled card. Played-card visuals are hidden during selection/transfer, restored before ordinary cleanup, and retain the existing generic PlayArea→Exhaust animation. No separate played-card fade was added.

AUTOMATED GATES:

- Standard bundled UE project generation and Development Editor build.
- `SlayTheSpireDemo.CardSelection.Presentation` (including production asset ancestry and the actual selection Confirm delegate).
- `SlayTheSpireDemo.CardExpansion.Wave1CC0.Selection.NativeHUDExactNClick` for multi-selection confirmation.
- `SlayTheSpireDemo.CardExpansion.Wave1CC1.DrawPileTop` for authoritative move/order and interactive boundary.
- `SlayTheSpireDemo.Phase6UIA2N.R8.Zone.PlayAreaDestinationsAndDestruct` for reused generic cleanup.

MANUAL PIE GATES — USER ACTION REQUIRED:

In the existing Native `L_BattleTest`, play the authored Warcry. Let Draw finish. Verify the selection background dims and Warcry is hidden; select a Hand card (including an attack or the newly drawn card), verify it is centered and Confirm enables without submitting; click the selected card again to deselect, then reselect and explicitly Confirm. Observe the selected card flying to the lower-left draw pile, followed by Warcry reappearing and disappearing through normal Exhaust. Expect Draw +1, Exhaust +1, no legal-target feedback, no duplicate/flashback and input restored. One short recording or explicit observation of this sequence is sufficient. This manual gate is not implied by Automation.

Validation evidence: bundled project generation and Editor build passed (`Saved/Logs/SelectionPresentationBuild.log`). After adding retained-played-card skip cleanup, the runtime rebuild passed (`SelectionPresentationFinalBuild.log`); a test-only sequence-token correction also built successfully (`SelectionPresentationTestBuild.log`). The closed-scope Automation run reported **11 PASS / 1 FAIL**, no warnings (`Saved/AutomationReports/SelectionPresentationRepair/index.json`). The failure was the older Draw-before-selection test assuming candidate click immediately submitted. Updated it to assert no continuation before explicit Confirm, rebuilt the changed test (`SelectionBoundaryConfirmTestBuild.log`), and reran only that gate: **1/1 PASS**, exit 0 (`Saved/AutomationReports/SelectionBoundaryConfirm/index.json`). Runtime code was unchanged after the first test run. All required automated gates now have passing evidence; no manual PIE acceptance is claimed.
