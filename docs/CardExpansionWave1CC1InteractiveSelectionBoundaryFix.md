# Card Expansion Wave 1C-C1 — Interactive Selection Presentation Boundary Fix

Date: **2026-09-08**

Status:

```text
IMPLEMENTATION AUTHORED
BUILD REQUIRED
FOCUSED AUTOMATION REQUIRED
PIE RECHECK REQUIRED
C1 NOT SEALED
```

## 1. Player-visible failure

Warcry-style authored order:

```text
Draw
→ Select current Hand card
→ move selected exact CardInstance to DrawPile top
→ normal card cleanup
```

Gameplay committed Draw before Selection, but the player saw the Selection UI before the Draw presentation played.

Observed symptom:

```text
player is asked to choose a card
→ only after the choice does the Draw animation play
```

The CardData Effect order was correct and must not be reversed.

## 2. Root cause

`UDrawCardsAction` correctly schedules concrete `UDrawCardAction` work ahead of the deferred selection, and `UDeferredHandSelectionAction` correctly reads CURRENT Hand at Execute time.

The failure was between Gameplay and Presentation timelines:

```text
Draw Gameplay commit
→ Draw Presentation Record buffered in active Resolution
→ SelectionRequestAction becomes async CurrentAction and waits for player
→ ActionQueue remains busy
→ ordinary stable-boundary Presentation seal cannot occur
→ pending Selection was exposed directly through the ViewModel anyway
```

Therefore Gameplay truth was already correct, but visible historical state had not caught up.

## 3. Required invariant

Gameplay must never wait for Presentation playback.

Correct interactive ordering:

```text
prior Gameplay commits
→ interactive boundary freezes/seals committed prefix
→ immediately open continuation Presentation segment
→ Gameplay may enter authoritative pending Selection
→ UI hides that Selection while its displayed revision is behind the sealed boundary
→ Presentation plays/catches up
→ Selection RuntimeIds become player-visible
→ player resolves Selection
→ Gameplay continuation executes
```

Only input exposure waits for display catch-up. BattleAction execution does not wait for animation completion or tokens.

## 4. Implementation

### Queue read-only interactive scope

`UBattleActionQueue` now supports a narrowly validated synchronous read scope for the current boundary Action.

During that read only, normal command-busy reporting ignores the current boundary/tail so the existing player-facing snapshot builder can be reused. Queue mutation remains rejected; the Queue never pumps or advances inside the read scope.

The queue also supports atomically rebinding already-authored pending Actions to the next Presentation writer.

### BattleManager boundary

`ABattleManager::AdvancePresentationAtInteractiveSelectionBoundary(...)`:

```text
validate exact current boundary Action
→ AdvanceStateRevision
→ build player-facing read snapshot through the controlled read-only scope
→ leave scope
→ freeze snapshot while real Gameplay busy state is visible again
→ seal current Presentation Resolution
→ enqueue deferred public delivery
→ schedule normal delivery
→ begin next Presentation Resolution with the same origin
→ return continuation writer
```

Presentation failure remains Presentation-only and returns an unavailable/empty writer; it does not request Gameplay ResolutionFault.

### Deferred current-Hand selection

Only when an actual non-zero selection with candidates exists:

```text
build CURRENT Hand candidate set
→ establish interactive Presentation boundary on a real BattleManager queue
→ rebind pending tail Actions to continuation writer
→ create ordinary SelectionRequestAction with continuation writer
→ enqueue selection at front
```

Standalone/unit queues without a BattleManager outer keep the existing pure Gameplay behavior.

### ViewModel exposure gate

When committed Presentation owns display, pending card selection is exposed only if:

```text
Displayed BattleId/StateRevision
==
latest frozen Presentation boundary BattleId/StateRevision
```

C0-style selection without a preceding newly sealed boundary remains immediately visible because its displayed baseline already matches. Warcry Draw→Selection remains hidden until the Draw boundary is displayed.

## 5. Focused regression test

New test:

```text
SlayTheSpireDemo.CardExpansion.Wave1CC1.DrawPileTop.Presentation.DrawBeforeSelectionBoundary
```

It validates:

```text
Gameplay selection is already pending after Draw
newly drawn RuntimeId is already a Gameplay candidate
Presentation-owned ViewModel hides the pending selection before catch-up
first Envelope contains CardPlayed then DrawPile→Hand
first Envelope contains no Hand→DrawPileTop
applying first FinalSnapshot makes pending selection visible
newly drawn card is selectable
selection resolution produces second Envelope
second Envelope contains exact Hand→DrawPileTop then played-card cleanup
```

The C1 focused prefix therefore increases from **6 tests to 7 tests**.

## 6. Validation required after this fix

The previously reported C1 Build PASS and 6/6 focused Automation PASS are invalidated by this C++/test change.

Run only the invalidated Gates:

```text
[ ] Development Editor Win64 Build PASS
[ ] SlayTheSpireDemo.CardExpansion.Wave1CC1.DrawPileTop 7/7 PASS
[ ] Warcry PIE: Draw visibly completes before Selection becomes interactive
[ ] newly drawn card is selectable
[ ] chosen exact card reaches DrawPile top
[ ] Warcry later cleanup remains correct
```

No unrelated C0/Phase6/Phase7 aggregate rerun is required unless a concrete failure implicates those contracts.
