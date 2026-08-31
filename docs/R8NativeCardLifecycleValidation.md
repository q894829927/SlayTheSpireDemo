# Phase 6UI-A2N — R8 Card Lifecycle

Status:

```text
R7 COMPLETE / VALIDATED
R8 COMPLETE / VALIDATED
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
and `HandIndexBefore`, source/optional target PresentationIds, frozen Energy Before,
paid cost, and the current producer's PlayArea index. Begin hides the exact formal
Hand Widget and creates one frozen presentation-only `UBattleCardWidget` in
`OV_PlayArea`.

The presentation card is `HitTestInvisible`, has `bGameplayPlayable=false`, and has
no HUD request delegate. It transitions from the exact historical Hand position into
centered PlayArea. CardPlayed does not synthesize an `EnergyChanged` visual. Exact
Finish keeps the PlayedCard transient for the later PlayArea destination Record.
Exact Cancel removes it and restores the exact historical Hand Widget visibility.

### CardZoneChanged

Only these current producer pairs are accepted:

```text
Hand -> DiscardPile
DrawPile -> Hand
PlayArea -> DiscardPile
PlayArea -> ExhaustPile
PlayArea -> RemovedPile
```

Hand-to-Discard hides the exact historical card while preserving its layout slot,
creates one frozen noninteractive transient, and moves/fades it toward
`Txt_DiscardCount`. Finish retires the transient without proactive `RefreshHand`;
Cancel retires it and restores the exact prior visibility.

PlayArea-to-destination requires the exact frozen PlayedCard identity. Discard moves
and fades the PlayedCard toward `Txt_DiscardCount`; Exhaust/Removed scale and fade it
out at PlayArea. Finish retires the transient without changing unrelated pile counts
early.

### Strict per-Record DrawPile-to-Hand presentation

Each Draw Record owns exactly one frozen, noninteractive card. The handler validates
that `FromIndex` is the frozen DrawPile top, `ToIndex` is the current Hand append
position produced by Gameplay, and RuntimeId does not already exist in formal or
transient Hand.

Begin inserts only this Record's card at `HB_Hand[ToIndex]`, updates the frozen Draw
count for this transition, and moves/scales/fades it from the DrawPile visual anchor
to the final Hand slot. Exact Finish releases only the current Token; Controller then
applies only that completed Record's working snapshot before the next Draw Record may
begin. Therefore consecutive draws remain strictly one Record/card at a time.

Exact Cancel removes only the active draw transient and restores the frozen Draw
count Before.

## Exact-token and cleanup semantics

```text
stale / duplicate Finish -> no-op
wrong-token Cancel -> no-op
exact Finish -> committed visual cleanup, exact Notify once
exact Cancel -> historical/local restore, never Notify
NativeDestruct -> timer/transient/local-reference cleanup only
```

`NativePlayedCardWidget` is the only cross-Record local reference. It intentionally
survives exact CardPlayed Finish until the matching PlayArea destination Record.

## Post-review P1 cleanup fix

Review found one cross-Record cleanup gap: if `CardPlayed` had finished and retained
`NativePlayedCardWidget`, then a later Record was abandoned by `SkipPresentation` /
fail-safe exact Cancel, the current Record cleanup could leave the earlier PlayedCard
inside `OV_PlayArea`.

The fix centralizes cleanup at the exact native Cancel boundary:

```text
exact current Token required
-> clear current timer
-> run current Record type-specific Cancel
-> remove/reset any retained NativePlayedCardWidget
-> clear local ownership
-> never Notify normal completion
```

Wrong/stale Token cancellation returns before this cleanup, so it cannot remove a
valid current PlayedCard.

Fix commits:

```text
ec361b0ea67a96b423e0c710399e18080779e1e7
  fix(ui-a2n): clear retained played card on cancel

d1a48d486ea80cf759e6556396df4124805cd06f
  test(ui-a2n): cover R8 skip transient cleanup
```

New focused case:

```text
SlayTheSpireDemo.Phase6UIA2N.R8.Zone.SkipClearsRetainedPlayedCard
```

It establishes:

```text
CardPlayed Finish
-> retained PlayedCard
-> Draw Begin
-> SkipPresentation
-> active Draw transient removed
-> retained PlayedCard removed
-> local timer/ownership cleared
-> OV_PlayArea empty
```

## Automated Gates — PASS

The P1 runtime edit invalidated only the Editor Build and focused R8 Automation
results. The user reran both against the corrected branch head on **2026-09-01** and
confirmed both passed.

```text
SlayTheSpireDemoEditor Win64 Development: PASS
Result: Succeeded

SlayTheSpireDemo.Phase6UIA2N.R8: 6/6 PASS
0 failed
0 notRun
```

Focused coverage now includes:

```text
CardPlayed.ExactIdentityFinishAndCancel
CardPlayed.InvalidIdentityZeroSideEffects
Zone.DrawToHandSequentialPresentation
Zone.HandToDiscardFinishCancelAndInvalid
Zone.PlayAreaDestinationsAndDestruct
Zone.SkipClearsRetainedPlayedCard
```

No R3-R7, A2D5, Phase6R, Shipping, aggregate regression, reviewer, or R9+ suite was
rerun.

## Manual PIE Gate — PASS / STICKY

The first user pass on **2026-09-01** found that only DrawPile-to-Hand animated;
CardPlayed, Hand discard, PlayedCard discard and Exhaust were still immediate state
changes. Those missing visual transitions were corrected and the user then completed
the corrected minimal PIE pass in:

```text
/Game/SlayTheSpireDemo/Maps/L_BattleTest_Native
```

Accepted observations:

1. Played cards visibly transition from Hand into PlayArea and then toward
   DiscardPile, without duplicate cards or flashback.
2. Exhaust cards disappear correctly at PlayArea.
3. End-turn/manual discard cards visibly transition toward DiscardPile.
4. DrawPile-to-Hand remains strictly one Record/card at a time.
5. Final Hand/HUD state remains correct with no permanent Input Lock, transient
   leak, duplicate card, or abnormal HUD.

This manual Gate remained valid after the P1 cleanup fix because that fix changes
only abandoned/Skip cleanup and does not change normal visual paths.

## Current acceptance state

```text
R8 COMPLETE / VALIDATED
R9 NOT STARTED
```

The corrected Editor Build, focused R8 6/6 Automation, and the sticky corrected
manual PIE evidence close R8. Do not start R9 automatically.