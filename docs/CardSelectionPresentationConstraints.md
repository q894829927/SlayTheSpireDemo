# Card Selection Presentation Constraints

Date: **2026-09-08**

Status:

```text
PARTIALLY IMPLEMENTED / PARALLEL GROUP REDESIGN AUTHORIZED / GROUP PRODUCTION CODE NOT IMPLEMENTED / NOT SEALED
```

Scope: define the shared Native HUD / Presentation contract for player card-selection interactions before implementation continues.

This document is authoritative for the Presentation behavior of current and future player card-selection flows. It complements `docs/CardSelectionRefactorConstraints.md`, which defines Gameplay candidate capture, pending-selection, resolver, continuation, and interactive-boundary rules. The grouped multi-selection rules below are further specified by `docs/SelectionPresentationGroupDesign.md`.

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

## 7. Multi-selection ordering and explicit group exception

Gameplay and reducer ordering remain authoritative.

For a frozen candidate set and canonical validated result:

```text
A, B, C
```

Gameplay mutation, event dispatch and committed `PresentationSequence` remain in canonical authored order. Reducer application MUST also remain in that exact committed order.

UI click order MUST NOT become a hidden ordering control.

Visible playback normally follows `PresentationSequence`. The authorized exception is an explicitly committed, complete and Controller-validated Selection Presentation Group. Such a group may co-present/look ahead to its own frozen selected-card destination members from the already sealed Envelope so that A/B/C can animate together, including when ungrouped trigger Records are interleaved between those members.

The exception is narrow:

```text
Reducer order                    = always PresentationSequence order
Normal visible playback order    = PresentationSequence order
Validated explicit group members = may co-present together
```

Group lookahead MUST NOT:

- reduce B/C before their chronological cursor;
- consume, skip or mark interleaved ungrouped Records as played;
- include untagged Records;
- infer membership from CardId, Effect, destination, adjacency, timing or click order;
- expose mutable future Gameplay.

For the first grouped implementation, one-member Selection metadata may still be recorded, but `ExpectedMemberCount <= 1` stays on normal SingleRecord playback. Parallel Group playback is enabled only for a complete validated group with more than one member.

### 7.1 Visually-consumed-but-not-reduced suppression lifetime

A group child finishing its animation does **not** mean its visual ownership may be forgotten if that member has not yet reached the reducer cursor.

Example:

```text
A/B/C animate and disappear together
→ group visual completes
→ reduce A
→ ApplyPresentationSnapshot
→ B/C are still historically in Hand until their own Records are reduced
```

B/C MUST NOT reappear during that intermediate snapshot.

Required lifecycle:

```text
group member visual starts
→ exact RuntimeId becomes Presentation-suppressed from formal Hand display
→ child animation visually consumes the card
→ group completion cleans transient child visual
→ suppression remains active
→ zero or more interleaved Records / HUD rebuilds may occur
→ reducer reaches that exact member Record
→ apply committed member mutation
→ release suppression for that exact member
→ publish/reconcile snapshot
```

Suppression is transient Presentation state only. It does not remove the card from Gameplay or rewrite the historical snapshot. Formal Hand rendering MUST honor the suppression set so `RefreshHand()` cannot visibly recreate an already-consumed future group member.

Suppression identity MUST be exact and battle/resolution scoped. For card Selection it is keyed by the exact selected RuntimeId plus the active group/member ownership; CardId or Hand index is insufficient.

All retained group suppression MUST be cleared on global reconciliation boundaries, including:

- exact member reduction for that member;
- sequential fallback before any group visual ownership is accepted;
- Skip / collapse / timeout catch-up as applicable;
- Widget loss/replacement;
- Envelope completion/replacement;
- battle replacement;
- PresentationUnavailable/direct-baseline fallback.

No intermediate ViewModel/HUD refresh may restore a suppressed member merely because it is still present in the current chronological `WorkingPresentationSnapshot`.

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
→ exact selected RuntimeId card visibly flies Hand → DrawPile
→ selection UI ends
→ Warcry PlayArea visual is shown/available for cleanup
→ normal generic PlayArea → Exhaust Presentation runs because Warcry exhausts
→ resolution completes
```

Warcry is a one-member Selection destination case. In the first grouped implementation it remains normal SingleRecord Controller playback while reusing the same generic card-transition child engine as multi-member groups.

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

## 14. Next implementation scope: grouped Selection destination Presentation

The explicit Confirm and initial single-card handoff work already exist. The next development phase is the grouped parallel redesign described in `docs/SelectionPresentationGroupDesign.md`.

The implementation SHOULD proceed through separable compile/test stages and must include:

1. explicit group metadata and writer-scoped correlation for direct Selection continuation Actions only;
2. complete-group validation with exact-one-eligible-record-per-selected-member eligibility;
3. Controller-owned non-contiguous group lookahead without reducer reordering;
4. visually-consumed-but-not-reduced suppression across intermediate historical snapshots;
5. one `UBattleHUDWidgetBase` tracked playback-unit hardening path shared by Record and Group playback;
6. one generic N-child card-zone transition engine used by both SingleRecord and grouped playback;
7. Selection layer supplying exact RuntimeId source handoff only;
8. focused Automation plus manual PIE only for the genuinely visual concurrency/no-flash behavior.

The phase MUST NOT:

- batch or reorder Gameplay mutations/events for visual convenience;
- make trigger reaction Actions inherit Selection group metadata;
- add Effect/Card-specific animation branches;
- let Widget code scan future Envelope Records;
- preserve the old per-Record deferred/reposition patch as the multi-select solution.

## 15. Acceptance gates

Implementation is not complete until focused coverage and PIE demonstrate at minimum:

- selecting the required count does not auto-submit;
- Confirm is unavailable before the required count and available at the required count;
- Confirm submits the exact selected RuntimeIds once;
- the selected card visual flies from Hand to the DrawPile anchor after confirmation;
- the selected card is not presented as DrawPile→Hand, Discard, or Exhaust;
- Hand/DrawCount reconcile to committed facts;
- Gameplay, event dispatch, `PresentationSequence` and reducer order remain canonical;
- a valid multi-member Selection group starts its member destination animations together rather than `N × duration` serial playback;
- non-contiguous group lookahead co-presents only exact tagged members and later still presents interleaved ungrouped Records;
- already visually consumed future members remain suppressed through intermediate `ApplyPresentationSnapshot` / Hand rebuilds until their own reducer record is consumed;
- incomplete, duplicate or otherwise malformed group membership degrades to normal sequential playback without Gameplay fault;
- one selected object producing zero eligible members or more than one matching eligible member disables grouped playback for that group;
- `ExpectedMemberCount <= 1` remains normal SingleRecord Controller playback;
- Record and Group callbacks share the same base-Widget exact-token/deferred-completion/cancellation hardening and stale callbacks cannot cross unit ownership;
- the Selection animation path is reusable and contains no Warcry/CardId/Effect-specific branch;
- Warcry's one-member Hand→DrawPile uses the same generic transition child engine without requiring Group playback;
- Warcry cleanup reuses the existing generic Exhaust animation;
- no dedicated Warcry Exhaust/fade animation exists;
- Draw → Selection ordering remains correct;
- input used to cross into Selection or enable Confirm is not reused as the next interaction;
- Gameplay remains authoritative if Presentation is skipped/degraded according to existing policy.

No Build, Automation, or PIE gate may be marked PASS without actual execution evidence.

## 16. Production integration and confirmation repair — 2026-09-08

This section records the pre-group implementation history and validation. It does **not** validate the grouped parallel redesign authorized above. The known serial multi-select visual limitation is the reason the next redesign exists.

The production `WBP_BattleHUD_Native` still inherited `UBattleHUDWidget`, bypassing the shared Selection subclass. Candidate clicks could select transient RuntimeIds, but the production Confirm delegate called ordinary card-play confirmation and displayed `Choose a legal target.` Reparented the existing Native asset to `UBattleHUDSelectionWidget` in UE, compiled and saved it. No Legacy or card/map assets changed.

The shared HUD now dims the background, raises the selectable Hand and confirmation controls, and positions selected formal cards centrally in canonical Hand order. Deselect restores the card's Hand transform. Temporary Canvas layout/Z changes restore on submit, cancellation and destruction. Selection never submits on the final candidate click.

Confirmation captures visual centers by RuntimeId before clearing transient input. Committed Hand→DrawPile records fly from those centers to the DrawPile anchor in record order. Anchor calculations use the stationary PlayArea coordinate space; they do not measure offsets relative to the moving/scaled card. Played-card visuals are hidden during selection/transfer, restored before ordinary cleanup, and retain the existing generic PlayArea→Exhaust animation. No separate played-card fade was added.

The confirmation handoff also preserves the selected formal Hand widget's render translation and visibility. A committed Hand→Exhaust record therefore reuses the existing generic Exhaust opacity fade on that same widget at its confirmed selection position; it is not reset to the Hand layout and no second consume animation is introduced. This works for the first serially presented member but does not solve multi-member concurrency or suppression across later reducer snapshots.

AUTOMATED GATES:

- Standard bundled UE project generation and Development Editor build.
- `SlayTheSpireDemo.CardSelection.Presentation` (including production asset ancestry and the actual selection Confirm delegate).
- `SlayTheSpireDemo.CardExpansion.Wave1CC0.Selection.NativeHUDExactNClick` for multi-selection confirmation.
- `SlayTheSpireDemo.CardExpansion.Wave1CC1.DrawPileTop` for authoritative move/order and interactive boundary.
- `SlayTheSpireDemo.Phase6UIA2N.R8.Zone.PlayAreaDestinationsAndDestruct` for reused generic cleanup.

MANUAL PIE GATES — USER ACTION REQUIRED:

In the existing Native `L_BattleTest`, play the authored Warcry. Let Draw finish. Verify the selection background dims and Warcry is hidden; select a Hand card (including an attack or the newly drawn card), verify it is centered and Confirm enables without submitting; click the selected card again to deselect, then reselect and explicitly Confirm. Observe the selected card flying to the lower-left draw pile, followed by Warcry reappearing and disappearing through normal Exhaust. Expect Draw +1, Exhaust +1, no legal-target feedback, no duplicate/flashback and input restored. One short recording or explicit observation of this sequence is sufficient. This manual gate is not implied by Automation.

Validation evidence: bundled project generation and Editor build passed (`Saved/Logs/SelectionPresentationBuild.log`). After adding retained-played-card skip cleanup, the runtime rebuild passed (`SelectionPresentationFinalBuild.log`); a test-only sequence-token correction also built successfully (`SelectionPresentationTestBuild.log`). The closed-scope Automation run reported **11 PASS / 1 FAIL**, no warnings (`Saved/AutomationReports/SelectionPresentationRepair/index.json`). The failure was the older Draw-before-selection test assuming candidate click immediately submitted. Updated it to assert no continuation before explicit Confirm, rebuilt the changed test (`SelectionBoundaryConfirmTestBuild.log`), and reran only that gate: **1/1 PASS**, exit 0 (`Saved/AutomationReports/SelectionBoundaryConfirm/index.json`). The follow-up in-place Exhaust repair rebuilt successfully (`Saved/Logs/InPlaceExhaustFadeFinalBuild.log`); the final focused `SlayTheSpireDemo.CardSelection.Presentation` prefix passed **4/4**, including `HandToExhaust.FadesInPlace`, with no warnings (`Saved/AutomationReports/CardSelectionPresentationInPlaceFinal/index.json`). No manual PIE acceptance is claimed for the grouped parallel redesign.
