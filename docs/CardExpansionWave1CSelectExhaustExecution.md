# Card Expansion — Wave 1C-B Select-Exhaust Effect Execution Record

Date: **2026-09-06**

Status:

```text
COMPLETE / VALIDATED / READY FOR SEAL
```

Branch:

```text
main
```

Authority:

```text
docs/CardExpansionWave1CSelectionPrimitive.md ("Wave 1C-B Design Details (locked)")
```

This record covers only the composable `USelectExhaustHandCardEffect` (manual selection mode). The deferred `Random` selection mode and production `DA_Card_BurningPact` asset authoring are outside this slice.

---

## Implemented source surface

New composable Effect (`Source/SlayTheSpireDemo/Cards/Effects/SelectExhaustHandCardEffect.*`):

```text
USelectExhaustHandCardEffect
  BuildActions:
    candidates = current Hand cards, excluding the played card (Context.Card)
    non-empty -> enqueue USelectionRequestAction(min=1,max=1)
                 + UExhaustSelectedContinuation (builds UExhaustCardAction)
    empty     -> skip burning entirely (no selection, no exhaust, no fault)
  preview hooks (GetPreviewArgumentNames / BuildPreviewArguments /
                 ValidatePreviewConfiguration) are no-ops: no authored numeric value
```

New concrete Continuation (`Source/SlayTheSpireDemo/Selection/ExhaustSelectedContinuation.*`):

```text
UExhaustSelectedContinuation (stateless/immutable definition object)
  holds stable battle-scoped wiring (Deck, PresentationCardSource,
  EventDispatcher, EventCombatants) captured at BuildActions time
  BuildNextActions(FSelectionResult, Queue, OutActions):
    cast selected object -> UCardInstance -> build one UExhaustCardAction
```

Resolver submit surface (extended `USelectionResolver`):

```text
BeginSelection(Request, Continuation, PendingAction)  // now retains the awaiting Action
SubmitResult(FSelectionResult)   // resumes the awaiting Action (deterministic submit)
SubmitCancel()                   // resumes the awaiting Action as cancel
ClearPendingSelectionInternal()
```

BattleManager ownership (`Source/SlayTheSpireDemo/Battle/BattleManager.*`):

```text
ABattleManager now owns a battle-scoped USelectionResolver (created in StartBattle,
bound to the authoritative ActionQueue) and exposes GetSelectionResolver().
```

Ownership / determinism points frozen by this slice:

```text
- Selection state remains Gameplay-owned (resolver owned by ABattleManager).
- The Effect is the authored composition point that bridges selection -> exhaust,
  while USelectionRequestAction / UExhaustCardAction stay mutually neutral.
- Ordering after the Effect (e.g. draw) is authored by the card's Effects[] array.
- The played card is excluded from candidates by identity (Context.Card), not by
  relying on physical Hand movement timing.
- Empty candidates skip burning: no selection is begun, no exhaust, no fault.
```

---

## Focused Automation validation

Prefix:

```text
SlayTheSpireDemo.CardExpansion.Wave1C
```

New 1C-B cases (2):

```text
SelectExhaust.ExhaustsChosenCard    -> chosen Hand card exhausts; other card untouched
SelectExhaust.SkipsWhenNoCandidate  -> single-card Hand skips burning; nothing exhausts
```

Regression: the 5 existing `Selection.*` cases still pass after the resolver
`BeginSelection`/submit-surface change.

Result:

```text
7/7 PASS (EXIT CODE 0)
```

Coverage:

```text
playing a select-exhaust card begins a pending selection
candidates = exactly the other Hand cards (played card excluded)
resolving the selection exhausts the chosen card (exact identity)
the unchosen card remains in Hand
no resolution fault after a successful select-exhaust
single-card Hand -> no selection pending -> nothing exhausts -> no fault
resolver submit/cancel surface resumes the awaiting action deterministically
```

---

## Seal gate

```text
[X] Editor Development Build PASS
[X] focused Wave 1C-B SelectExhaust Automation PASS (7/7 incl. 1C-A regression)
[X] source review of Effect / Continuation / resolver ownership PASS
[ ] Final user seal confirmation
```

Not included in this slice:

```text
- no Random selection mode (deferred)
- no production DA_Card_BurningPact asset (user-authored later)
- no draw follow-up wiring in the Effect (draw is a separate UDrawCardEffect)
```
