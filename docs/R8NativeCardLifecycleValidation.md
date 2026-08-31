# Phase 6UI-A2N — R8 Card Lifecycle

Status:

```text
R7 COMPLETE / VALIDATED
R8 P1 FIX IMPLEMENTED / AUTOMATED REVALIDATION PENDING
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

The created card uses the exact historical Hand Widget as its visual start anchor
and eases into the centered PlayArea position. This preserves the distinct
CardPlayed fact before the later destination Record begins.

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

Hand-to-Discard hides the exact historical card while preserving its layout slot,
creates one frozen noninteractive transient, and moves/fades that transient from
the Hand anchor toward `Txt_DiscardCount`. Finish retires the moving transient and
leaves the historical card collapsed without proactive `RefreshHand`; Cancel
retires the transient and restores the exact prior visibility.

PlayArea-to-destination requires the exact frozen PlayedCard identity. Exact Finish
retires it without changing unrelated pile counts early. Discard moves/fades the
PlayedCard toward `Txt_DiscardCount`; Exhaust/Removed scale and fade it out at the
PlayArea. Cancellation/destruction performs local transient cleanup without normal
completion Notify.

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

## Previously accepted Automated Gates

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

Previously accepted result before the post-review P1 fix:

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
Saved/AutomationReports/R8FocusedPhase6UIA2NManualFix/index.json
```

Coverage includes exact RuntimeId/CardId/HandIndex identity, duplicate RuntimeId and
wrong CardId rejection, no duplicate Energy visual, Hand-to-Discard Finish/Cancel,
one-card-at-a-time Draw sequencing, noninteractive draw transients, rejection of a
second Begin before exact Finish, per-Record formalization before the next draw,
Draw Cancel, all three PlayArea destinations, invalid zone/index zero-side-effect
false, stale/duplicate Finish, wrong/exact Cancel, next-Record isolation, transient
cleanup and NativeDestruct local cleanup.

The post-PIE correction rerun additionally asserts real transform/opacity progress
for Hand-to-PlayArea, Hand-to-Discard, PlayArea-to-Discard, and PlayArea
Exhaust/Removed disappearance. The earlier automated result was invalidated by the
visual source changes; only the affected Editor Build and R8 focused suite were
rerun.

No R3-R7, A2D5, Phase6R, Shipping, aggregate regression, reviewer, or R9+ suite was
run.

## Manual PIE Gate — PASS / STICKY

The first user pass on **2026-09-01** found that only DrawPile-to-Hand animated;
CardPlayed, Hand discard, PlayedCard discard and Exhaust were still immediate state
changes. Those missing visual transitions were then implemented and the affected
automated Gates passed.

The user completed the corrected minimal PIE pass on **2026-09-01** in:

```text
/Game/SlayTheSpireDemo/Maps/L_BattleTest_Native
```

Accepted observations:

1. Played cards visibly transition from Hand into PlayArea and then toward
   DiscardPile, without duplicate cards or flashback.
2. Exhaust cards disappear correctly at PlayArea.
3. End-turn/manual discard cards visibly transition toward DiscardPile.
4. DrawPile-to-Hand remains strictly one Record/card at a time, with no later card
   appearing before the preceding card finishes.
5. Final Hand/HUD state remains correct with no permanent Input Lock, transient
   leak, duplicate card, or abnormal HUD.

This manual Gate remains valid after the P1 cleanup fix below because the fix changes
only abandoned/Skip cleanup and does not change the already accepted normal visual
paths. Do not rerun PIE unless a later edit changes those visuals.

## Post-review P1 fix — automated revalidation pending

A narrow review found one cross-Record cleanup gap: after an exact `CardPlayed`
Finish, `NativePlayedCardWidget` intentionally survives for a later
PlayArea-to-destination Record. If a later native Record (for example Damage or
DrawPile-to-Hand) was then abandoned through `SkipPresentation` / fail-safe exact
Cancel, the current Record cleanup did not necessarily retire that earlier retained
PlayedCard. Controller collapse could therefore leave a stale card in `OV_PlayArea`.

The fix is intentionally centralized at the exact native Cancel boundary:

```text
exact current Token required
-> clear current timer
-> run current Record type-specific Cancel
-> remove/reset any retained NativePlayedCardWidget
-> clear local ownership
-> never Notify normal completion
```

Wrong/stale Token cancellation still returns before this cleanup and therefore cannot
remove a valid current PlayedCard.

Fix commits:

```text
ec361b0ea67a96b423e0c710399e18080779e1e7
  fix(ui-a2n): clear retained played card on cancel

d1a48d486ea80cf759e6556396df4124805cd06f
  test(ui-a2n): cover R8 skip transient cleanup
```

A new focused case was added:

```text
SlayTheSpireDemo.Phase6UIA2N.R8.Zone.SkipClearsRetainedPlayedCard
```

It establishes `CardPlayed Finish -> retained PlayedCard -> Draw Begin ->
SkipPresentation` and requires both the active Draw transient and the prior retained
PlayedCard to be removed, with local ownership/timer cleared and no stale PlayArea
child.

Because this is a runtime source edit plus a new Editor-only Automation source file,
only these Gates are invalidated and must be rerun:

```text
1. SlayTheSpireDemoEditor Win64 Development build
2. SlayTheSpireDemo.Phase6UIA2N.R8 focused Automation
   expected discovery after the added test: 6 tests
```

Do not rerun R3-R7, A2D5, Phase6R, Shipping or the already accepted manual PIE.

## Current acceptance state

```text
R8 P1 FIX IMPLEMENTED
AUTOMATED REVALIDATION PENDING
MANUAL PIE PASS / STICKY
R9 NOT STARTED
```

Do not merge R8 to `main` or start R9 until the corrected Editor Build and the R8
focused suite pass on this branch head.
