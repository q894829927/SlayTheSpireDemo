# Codex Goal Checkpoint — Phase 7 Relics

Last updated: **2026-09-03**

## Goal

Implement Phase 7 Relics as a first-class deterministic Gameplay system, beginning with Sundial, without reopening sealed Phase 6UI-A architecture or modeling Relics as Statuses.

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

Post-seal A3→A2 card-face continuity defect:
FIX IMPLEMENTED / VALIDATION PENDING
```

Active authority:

```text
docs/Phase7RelicsImplementation.md
```

Validation authorities already sealed:

```text
docs/Phase7AValidation.md
docs/Phase7BValidation.md
```

## Locked Phase 7 boundaries

```text
Relic != Status
RelicData != RelicInstance
Relics use the battle-wide ABattleManager RuntimeSequence allocator
Status + Relic trigger order = Priority → RuntimeSequence → LocalTriggerIndex
BattleEventDispatcher remains snapshot-based; no persistent Trigger Registry
Trigger remains read-only eligibility + Action construction
Sundial counter mutation occurs through USundialAdvanceAction
Sundial reward occurs through UGainEnergyAction
A3 does not predict Relic reactions
```

## Accepted predecessor evidence

7A user-reported UE 5.8 gate:

```text
Development Editor Build                         PASS
SlayTheSpireDemo.Phase7.RelicRuntime            5/5 PASS
```

7B user-reported UE 5.8 gate:

```text
SlayTheSpireDemo.Phase7.TriggerSources          3/3 PASS
SlayTheSpireDemo.Phase6A.Trigger                PASS
```

## 7C implementation

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

Current production structure:

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

User-reported current bulk focused results:

```text
SlayTheSpireDemo.Phase6C                         PASS
SlayTheSpireDemo.Phase7.Sundial                 PASS
```

Those gates are sticky for the current card-face-only correction below; do not rerun them unless later Draw/Shuffle/Sundial code changes.

## Post-seal A3→A2 card-face continuity correction

A gameplay PIE pass exposed a presentation defect:

```text
Strength and/or target Vulnerable
→ selected Hand card correctly shows resolved RichText value/color in A3
→ submit
→ A3 correctly clears before A2 ownership
→ A2 CardPlayed creates a new presentation card from FPresentationCardSnapshot
→ snapshot previously contained only the stable plain/source-side Description
→ outgoing card visually reverted to the lower/base value and neutral/source-only styling
```

The architecture remains unchanged: A3 is still pre-commit/transient and clears before A2; A2 still renders only immutable committed history and never reads live Gameplay.

Implemented narrow fix:

```text
FPresentationCardSnapshot
- Description      = stable plain/source-side semantic description
- RichDescription  = optional frozen Native RichText presentation

PresentationCardSnapshot::TryBuild
→ freezes normal current source-side RichDescription

PlayCardAction before follow-up mutation
→ reuses CardEffect::BuildImmediatePreviewOperations read-only pipelines
→ resolves exact target-specific committed card-face RichDescription
→ CardPlayed snapshot overrides only RichDescription
→ stable Description remains unchanged for historical Hand identity matching

UBattleHUDWidgetBase::MakePresentationCardView
→ copies frozen RichDescription into FBattleHUDCardView

A2 CardPlayed
→ still creates its own presentation card in OV_PlayArea
→ that card now displays the frozen submitted value/color
→ no dependency on the transient A3 Widget or mutable Gameplay during playback
```

The sealed `TargetSubmissionClearsPreviewBeforeAuthoritativeRequest` behavior is intentionally preserved.

New focused regression:

```text
SlayTheSpireDemo.UIA3.CardPlayedRichHandoff.StrengthAndVulnerableStayFrozen
```

It proves a base-6 Strike with +2 source Strength and target Vulnerable freezes:

```text
stable Description     = Deal 8 damage.
CardPlayed RichText     = Deal <PreviewIncrease>12</> damage.
Presentation card view  = same frozen RichText
visible RichText widget = same frozen RichText
```

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

The bulk Draw/Shuffle tests already passed and are not invalidated by the card-face correction. Run only:

```text
1. Regenerate project files once because the new focused test .cpp was added.
2. Development Editor Build once.
3. SlayTheSpireDemo.UIA3.CardPlayedRichHandoff once; expected 1/1.
4. SlayTheSpireDemo.Phase6UIA2C.Record.CardPlayed once; existing CardPlayed contract must remain green.
5. SlayTheSpireDemo.UIA3.NativePreviewIntegration once; expected existing 3/3, proving submit still clears A3 through the sealed handoff.
6. One focused production PIE check:
   - Strength-modified attack keeps its resolved value/color while flying to PlayArea;
   - target Vulnerable-modified attack keeps the target-specific resolved value/color while flying to PlayArea;
   - no duplicate card, CardPlayed rejection, flashback or input lock.
7. Record evidence and STOP.
```

Do not rerun Phase6C, Phase7.Sundial, Phase6R, A2D5, Shipping, Legacy parity or unrelated UI suites unless a concrete new failure invalidates them.

## Next exact action

USER ACTION REQUIRED:

Regenerate project files, build current `main`, run the three focused card-handoff gates above, then repeat the exact PIE case that exposed the text/value/color reversion.

Do not begin 7D until this correction and 7C final acceptance are closed.
