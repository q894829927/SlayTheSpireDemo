# Phase 6UI-A2N — R8 Card Lifecycle

Status:

```text
R7 COMPLETE / VALIDATED
R8 SOURCE IMPLEMENTED
AUTOMATED VALIDATION PASS
MANUAL PIE PENDING
R9 NOT STARTED
```

Branch: `a2n/r8-native-card-lifecycle`
Starting main HEAD: `22f0955787551b0c5a3201f9ca45cf35e5167cbf`
Validation date: **2026-09-01**

R8 migrates only the committed `CardPlayed` and `CardZoneChanged` presentation
Records into the Native HUD. It does not modify Gameplay, Controller, reducer,
Record/Envelope contracts, the Legacy WBP, production WidgetClass, Status/terminal
presentation, UI-A3, or any R9+ behavior.

## Implemented boundary

Both Record handlers reuse the sealed R5 exact-token playback kernel:

```text
validate Record metadata, Token and frozen payload
-> validate exact historical Widget/ViewModel identity
-> prepare one presentation-only Card when required
-> CommitNativePresentationOwnership
-> apply frozen local visual state
-> StartNativePresentationFinishTimer
-> exact-token Finish or exact-token Cancel
```

Validation failure occurs before local ownership or visible mutation and returns
`false`, preserving Controller immediate fallback with zero local visual side
effects.

### CardPlayed

The Native HUD validates the frozen card snapshot, unique RuntimeId, exact CardId
and `HandIndexBefore`, source/optional target PresentationIds, the frozen Energy
Before state, paid cost, and the current producer's PlayArea index. Begin hides the
exact formal Hand Widget and creates one frozen presentation-only `UBattleCardWidget`
in `OV_PlayArea`.

The presentation card is `HitTestInvisible`, has `bGameplayPlayable=false`, and has
no HUD request delegate. CardPlayed does not synthesize an `EnergyChanged` visual.
Exact Finish keeps the PlayedCard transient for its later PlayArea destination
Record. Exact Cancel removes that transient and restores the exact historical Hand
Widget visibility.

### CardZoneChanged

Only these current producer pairs are accepted:

```text
Hand -> DiscardPile
DrawPile -> Hand
PlayArea -> DiscardPile
PlayArea -> ExhaustPile
PlayArea -> RemovedPile
```

Hand-to-Discard hides the exact historical card on Begin, leaves it hidden on
Finish without proactive `RefreshHand`, and restores its exact prior visibility on
Cancel.

PlayArea-to-destination requires the exact frozen PlayedCard identity. Exact Finish
retires it without changing unrelated pile counts early. Cancellation/destruction
performs local transient cleanup without normal completion Notify.

### Strict per-Record DrawPile-to-Hand presentation

Each Draw Record owns exactly one frozen, noninteractive card. The handler validates
that `FromIndex` is the frozen DrawPile top, that `ToIndex` is the current Hand
append position produced by Gameplay, and that RuntimeId does not already exist in
the formal or transient Hand.

Begin inserts only this Record's card at `HB_Hand[ToIndex]`, updates the frozen Draw
count for this transition, and drives a Native ease-out movement/scale/fade from the
`Txt_DrawCount` cached geometry anchor to the card's final Hand slot. The draw card
remains presentation-only for its complete lifetime.

Exact Finish normalizes the card at the target slot and notifies only the current
Token. Controller then applies only this completed Record's working snapshot and
the formal Hand refresh replaces that one transient before the next Draw Record may
begin. Therefore an N-card draw is displayed strictly one Record/card at a time;
later cards cannot appear through an early all-at-once Hand refresh. Exact Cancel
removes only the active draw transient and restores the frozen Draw count Before.

## Exact-token and cleanup semantics

```text
stale / duplicate Finish -> no-op
wrong-token Cancel -> no-op
exact Finish -> committed visual cleanup, exact Notify once
exact Cancel -> historical/local restore as defined above, never Notify
NativeDestruct -> timer/transient/local-reference cleanup only
```

`NativePlayedCardWidget` is the only cross-Record local reference, surviving an
exact CardPlayed Finish until the matching PlayArea destination Record. It is never
Gameplay authority and is removed on destination Finish/Cancel or NativeDestruct.

## Automated Gates — PASS

### Editor Build

```text
SlayTheSpireDemoEditor Win64 Development: PASS
Result: Succeeded
```

The first invocations were rejected before compilation because the open Editor held
an active Live Coding session. After a normal Editor close, only the affected Build
Gate was rerun and passed. No source correction or unrelated Gate rerun was needed.

No production reflected binding/API contract changed. The new reflected probes are
Editor-only test types, so targeted compile of `WBP_BattleHUD_Native` or
`WBP_BattleCard_Native` was not required.

### Focused Automation

Prefix:

```text
SlayTheSpireDemo.Phase6UIA2N.R8
```

Result:

```text
CardPlayed.ExactIdentityFinishAndCancel:        PASS
CardPlayed.InvalidIdentityZeroSideEffects:      PASS
Zone.DrawToHandSequentialPresentation:          PASS
Zone.HandToDiscardFinishCancelAndInvalid:       PASS
Zone.PlayAreaDestinationsAndDestruct:           PASS

5 succeeded
0 failed
0 notRun
```

Evidence:

```text
Saved/AutomationReports/R8FocusedPhase6UIA2N/index.json
```

Coverage includes exact RuntimeId/CardId/HandIndex identity, duplicate RuntimeId and
wrong CardId rejection, no duplicate Energy visual, Hand-to-Discard Finish/Cancel,
one-card-at-a-time Draw sequencing, noninteractive draw transients, rejection of a
second Begin before exact Finish, per-Record formalization before the next draw,
Draw Cancel, all three PlayArea destinations, invalid zone/index zero-side-effect
false, stale/duplicate Finish, wrong/exact Cancel, next-Record isolation, transient
cleanup and NativeDestruct local cleanup.

No R3-R7, A2D5, Phase6R, Shipping, aggregate regression, reviewer, or R9+ suite was
run.

## Manual PIE Gate — USER ACTION REQUIRED

Run one minimal PIE pass in:

```text
/Game/SlayTheSpireDemo/Maps/L_BattleTest_Native
```

1. Play one card and observe `Hand -> PlayArea -> DiscardPile`; confirm no duplicate
   card or flashback.
2. If an Exhaust card is conveniently available, observe one
   `PlayArea -> ExhaustPile` transition.
3. End Turn / draw enough cards to observe consecutive draws. Confirm every card
   visibly travels from the DrawPile start to its Hand slot strictly one at a time,
   and no later card appears before the preceding card finishes.
4. Confirm draw transients cannot be clicked, the final formal Hand is correct, and
   there is no permanent Input Lock, transient leak, duplicate card, or abnormal HUD.

Do not replace this visual acceptance with screenshots or additional Automation.

## Current acceptance state

```text
R8 SOURCE IMPLEMENTED
AUTOMATED VALIDATION PASS
MANUAL PIE PENDING
R9 NOT STARTED
```

R8 is not `COMPLETE / VALIDATED` until the user explicitly confirms this Manual PIE
Gate. Do not start R9 automatically.
