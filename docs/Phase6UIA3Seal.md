# Phase 6UI-A3 — Final Seal

Date: **2026-09-02**

Status: **COMPLETE / VALIDATED / SEALED**

This document is the final acceptance/status authority for Phase 6UI-A3. Earlier `Phase6UIA3Implementation.md` status lines describe the implementation plan as it existed before A3-2 through A3-5 were completed; where those historical status lines disagree with this seal, this document controls. The detailed architecture boundaries in the implementation document remain valid unless explicitly amended by `Phase6UIA3CardFacePreviewAmendment.md`.

## Final phase state

```text
Phase 6UI-A: COMPLETE / VALIDATED / SEALED

A3-1 Dynamic Text: COMPLETE / VALIDATED / SEALED
A3-2 Target-Specific Current-State Preview: COMPLETE / VALIDATED / SEALED
A3-3 Energy + Target-Aware Legality: COMPLETE / VALIDATED / SEALED
A3-4 ViewModel Transient Preview Lifecycle: COMPLETE / REVALIDATED / SEALED
A3-5 Native card-face Preview + A2/A3 PIE: COMPLETE / VALIDATED / SEALED
A3-5 RichText per-value comparison styling: COMPLETE / VALIDATED / SEALED
```

Phase 7 Relics is now the next authorized design phase. This seal does not itself authorize runtime Phase 7 implementation beyond the scope separately recorded in the Phase 7 design document.

## Locked A3 boundary

```text
A3
= pre-commit
= read-only current-state values for supported Operations
= not a second simulator

A2
= post-commit
= playback of immutable facts that actually committed
```

A3 does not predict Trigger/Relic reactions, final HP, terminal outcome, draw/shuffle results or arbitrary future-state chains.

## Final visible behavior

```text
select card
→ nominate a legal PreviewTarget
→ Gameplay builds FImmediateCardPreview through existing read-only pipelines
→ selected Native Hand card temporarily displays target-specific supported values
→ submit / leave / cancel / revision change clears Preview
→ committed Gameplay resolves
→ sealed A2 Presentation owns committed playback
```

A3 never owns `OV_PlayArea`; that surface remains A2-only.

The Native card description is a `URichTextBlock` using `DT_BattleCardTextStyles`:

```text
Default
PreviewIncrease
PreviewDecrease
```

Only the affected semantic numeric argument is styled. UI does not search formatted text for numbers and does not recalculate Damage/Block formulas.

Normal current-state Hand card text also compares supported Damage/Block values against the authored effect base, so source/self modifiers can color the card face without target hover.

## Regression closure

The missing `CardPlayed` animation was traced to A3-4 revision invalidation publishing structural `OnChanged` after deferred committed Presentation delivery had already started an A2 visual.

Final locked split:

```text
structural/frozen HUD changes
→ OnChanged

PreviewTarget / ImmediatePreview-only changes
→ OnPreviewChanged
```

Presentation-owned `ReadStateReady` revision invalidation may clear stale A3 transient state and lock input, but must not structurally refresh/cancel the already-started A2 visual before historical Presentation catches up.

The first A3-5 standalone Preview child in `OV_PlayArea` and Preview-hover structural Hand rebuild were also real ownership problems and remain removed.

## Final acceptance evidence

User-reported UE 5.8 evidence on 2026-09-02:

```text
SlayTheSpireDemo.UIA3.ViewModelPreviewLifecycle: 3/3 PASS
SlayTheSpireDemo.UIA3.NativePreviewIntegration: 3/3 PASS
SlayTheSpireDemo.UIA3.RichCardTextBaseline: 2/2 PASS

Production L_BattleTest PIE:
- CardPlayed visible animation restored: PASS
- target-specific card-face Preview remains coherent with committed A2 playback: PASS
- Strength-modified current card-face Damage changes comparison color: PASS
```

The project currently has no playable Dexterity-granting card, so a Dexterity-specific manual PIE spot-check was not run. This is accepted and is not a seal blocker because `SlayTheSpireDemo.UIA3.RichCardTextBaseline.BlockTracksDexterityAndFrailty` directly covers the authored-base Block RichText rule and passed.

## Sealed invariants for forward work

Do not reopen A3 for Phase 7 unless a concrete new defect directly invalidates one of these contracts:

```text
Gameplay owns preview values and legality.
A3 is read-only and revision-stamped.
A3 PreviewTarget lifecycle is separate from inspection.
A3 transient preview does not use structural OnChanged.
A3 never adds Preview children to OV_PlayArea.
A2 committed Presentation remains authoritative after submission.
RichText styling is presentation-only and semantic-value based.
Relic reactions are not added to A3 first-version prediction.
```

Do not rerun broad Phase6R, A2D5, Shipping, Legacy parity or other sealed suites merely to reconfirm this phase. Follow `docs/ValidationExecutionPolicy.md` for any future defect-driven validation.

## Next phase

```text
Phase 6UI-A COMPLETE / SEALED
↓
Phase 7 Relics — DESIGN AUTHORIZED
```

The Phase 7 design must preserve the existing event/trigger ordering contract and must not model Relics as Statuses.