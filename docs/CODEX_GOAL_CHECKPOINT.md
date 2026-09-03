# Codex Goal Checkpoint — Phase 7 Relics

Last updated: **2026-09-03**

## Goal

Implement Phase 7 Relics as a first-class deterministic Gameplay system, beginning with Sundial, without reopening sealed Phase 6UI-A Gameplay architecture or modeling Relics as Statuses.

## Current status

```text
Phase 6UI-A: COMPLETE / VALIDATED / SEALED
Phase 6UI-A3: COMPLETE / VALIDATED / SEALED

Phase 7 Relics: IN PROGRESS
Phase 7 design: SEALED
7A Relic Runtime: COMPLETE / VALIDATED / SEALED
7B Status + Relic Trigger Sources: COMPLETE / VALIDATED / SEALED
7C Sundial + GainEnergyAction: IMPLEMENTED / BULK-DRAW TESTS PASS / FINAL ACCEPTANCE PENDING
7D Relic Read/Frozen/Native UI: NOT STARTED

Post-seal card-face continuity correction:
SHARED PRESENTATION-ONLY MAPPER IMPLEMENTED / VALIDATION PENDING
```

Active Phase 7 authority:

```text
docs/Phase7RelicsImplementation.md
```

Validated predecessor evidence:

```text
7A Development Editor Build                     PASS
7A SlayTheSpireDemo.Phase7.RelicRuntime        5/5 PASS
7B SlayTheSpireDemo.Phase7.TriggerSources      3/3 PASS
7B SlayTheSpireDemo.Phase6A.Trigger            PASS
```

## 7C Gameplay implementation

Reusable Energy primitive:

```text
BattleEnergyMutation::TryGain
UGainEnergyAction
```

Sundial runtime/content:

```text
URelicInstance::Counter
USundialTrigger
USundialAdvanceAction
```

Sundial reacts only to the authoritative battle `FDeckShuffledEvent`; no card identity, DrawAction identity, RetryDraw identity or Pommel Strike special case exists.

## Bulk Draw contract

```text
UDrawCardEffect DrawCount=N
→ UDrawCardsAction(N)
   - owns RemainingDraws
   - evaluates Hand capacity + DrawPile + DiscardPile at Execute time
   - plans deterministic continuation batches
   ↓
   UDrawCardAction x available-now
   → UShuffleDeckAction when the bulk request still owes draws
   → UDrawCardsAction(Remaining)

UDrawCardAction
= one atomic DrawPile -> Hand commit only
= no shuffle planning
= no retry recursion
```

`ABattleManager::BuildDrawActionBatch()` also creates one `UDrawCardsAction(DrawCount)` so opening-hand and turn-start draw use the same semantics as card effects.

A fresh bulk request against `Draw=0 / Discard=0` ends without a shuffle. A zero-card shuffle may still commit when that `UShuffleDeckAction` was already planned by an earlier bulk step before the final available DrawPile card was consumed. This produces the generic two-Pommel-Strike+/Sundial infinite without content special cases.

User-reported focused results:

```text
SlayTheSpireDemo.Phase6C                         PASS
SlayTheSpireDemo.Phase7.Sundial                 PASS
```

Those gates are sticky for the current Presentation-only correction; Draw/Shuffle/Sundial code is unchanged.

## Post-seal card-face continuity correction

Two related visual continuity defects were exposed during PIE:

```text
A3 -> A2 CardPlayed:
resolved/target-specific card face could revert when A2 created its historical presentation card.

CardZoneChanged Draw -> Hand:
a Strength-modified card could display resolved RichText during draw animation,
then temporarily fall back to plain/white text after the Record reducer rebuilt the Working Hand,
then return to RichText when Envelope.FinalSnapshot reconciled.
```

The authority boundary remains unchanged:

```text
A3 = transient pre-commit current-state preview
A2 = immutable committed historical playback
FinalSnapshot = stable frozen current-state reconciliation for that Resolution
```

### Root cause now fixed

`FPresentationCardSnapshot -> FBattleHUDCardView` had two independent presentation-only copiers. `UBattleHUDWidgetBase::MakePresentationCardView()` copied `RichDescription`; the Controller's private Draw reducer copier did not.

The durable boundary is now:

```text
FPresentationCardSnapshot
→ PresentationCardView::MakePresentationOnlyCardView
→ FBattleHUDCardView
```

Implementation:

```text
Source/SlayTheSpireDemo/Presentation/PresentationCardView.h/.cpp
```

The mapper is explicitly presentation-only:

```text
- complete frozen display projection, including RichDescription
- bGameplayPlayable = false
- UnplayableReason = empty
- MUST NOT be used for FCardReadView / FinalSnapshot formal Hand freezing
```

`UBattleHUDWidgetBase::MakePresentationCardView()` keeps its existing BlueprintPure API and delegates to the shared mapper.

`UBattlePresentationController` no longer owns a separate `MakeHUDCardView()` field copier. `CardZoneChanged DrawPile -> Hand` inserts the shared presentation-only projection into the WorkingSnapshot.

The independent formal current-state path remains:

```text
FCardReadView
→ ABattleManager::TryFreezePresentationStateSnapshot
→ FBattleHUDCardView with current Gameplay legality
```

Do not route that path through the presentation-only mapper.

Identity comparison remains intentionally narrower than projection. In particular, do not add `RichDescription` mechanically to CardPlayed Hand identity predicates: the Hand may contain source-side RichText while the committed CardPlayed snapshot legitimately contains target-specific Vulnerable-resolved RichText.

## Focused regressions added

Mapper contract:

```text
SlayTheSpireDemo.Phase6UIA2D4.PresentationCardViewMapper
```

It verifies the shared mapper and the preserved BlueprintPure wrapper both project all current presentation-only card fields, including `RichDescription`, while remaining non-gameplay-playable.

Controller Stage-B continuity:

```text
SlayTheSpireDemo.Phase6UIA2C.Record.CardZoneChanged.WorkingSnapshotRichContinuity
```

It uses a real asynchronous Controller playback window with two consecutive Draw Records:

```text
Draw A active
→ complete A
→ A is reduced into WorkingSnapshot
→ Draw B becomes active
→ FinalSnapshot has NOT yet replaced WorkingSnapshot
→ assert Working A.RichDescription == Record A.RichDescription
```

The payloads are non-empty and distinct. Assertions depend on non-empty/distinct frozen text and exact Record equality, not on one localized sentence.

The test calls `Controller::NotifyPresentationFinished()` directly because its target is reducer state; Widget CoreTicker completion deferral already has separate coverage and is not part of this invariant.

## Durable architecture rule

`docs/Architecture.md` now records:

```text
FPresentationCardSnapshot -> FBattleHUDCardView
must use the shared presentation-only projection.

FCardReadView -> FBattleHUDCardView
remains the independent stable current-state freeze path.
```

`CODEX_GOAL_CHECKPOINT.md` records execution status only. Do not write PASS evidence to `docs/Validation.md` until the local validation below actually runs.

## Production Sundial asset

Expected local UE asset:

```text
DA_Relic_Sundial : URelicData
RelicId = Sundial
DisplayName = 日晷
Description = 每洗牌3次，获得2点能量。
Triggers[0] = USundialTrigger
    ShufflesRequired = 3
    EnergyGain = 2
```

Icon/HUD display remains 7D.

## Required current-head validation gate

This correction changes shared Presentation mapping/Controller code but not Gameplay. Run only:

```text
1. Regenerate project files once because new .h/.cpp and focused test .cpp files were added.
2. Development Editor Build once.
3. SlayTheSpireDemo.Phase6UIA2D4.PresentationCardViewMapper once; expected 1/1.
4. SlayTheSpireDemo.Phase6UIA2C.Record.CardZoneChanged once; this prefix now covers both:
   - the existing CardZoneChanged producer/regression test;
   - WorkingSnapshotRichContinuity.
   Expected 2/2.
5. One focused PIE visual check:
   - Player already has Strength;
   - draw an Attack whose numeric card face is modified by Strength;
   - draw animation -> Working Hand -> later Record playback -> FinalSnapshot stays on the same resolved RichText/color;
   - no visible red -> white -> red flashback.
6. After those gates pass, record the exact evidence in docs/Validation.md and STOP.
```

Do not run `WorkingSnapshotRichContinuity` separately before the CardZoneChanged prefix; that would only duplicate the same focused evidence. Do not rerun Phase6C, Phase7.Sundial, EnergyGain, TriggerSources, Phase6R, A2D5, Shipping, Legacy parity or unrelated UI suites unless a concrete failure invalidates them.

## Scope protection

This correction touches only source/tests/docs. Do not save, move, rename, regenerate or otherwise modify unrelated `Content/` assets while validating it.

## Next exact action

USER ACTION REQUIRED:

Regenerate project files, build current `main`, run the mapper gate and the CardZoneChanged prefix once each, then perform the single Strength draw PIE visual check.

Do not begin 7D until this correction and 7C final acceptance are closed.
