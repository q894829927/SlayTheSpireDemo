# Phase 6UI-A3 — Card-Face Preview Amendment

Date: **2026-09-02**

Status: **AUTHORIZED UX AMENDMENT / REVALIDATION PENDING**

This document records the explicit A3-5 UX/ownership change made after production PIE exposed conflicts between the first standalone Preview surface and sealed A2 `CardPlayed` playback.

Where this document conflicts with the A3-5 presentation details in `docs/Phase6UIA3Implementation.md`, this amendment controls. The underlying A3 Query semantics and all sealed A2 Presentation semantics remain unchanged.

## Locked visible behavior

A3 target-specific Preview is displayed on the **currently selected Native Hand card**.

```text
select card
→ nominate legal PreviewTarget by hover/focus
→ Gameplay builds current target-specific ImmediatePreview
→ selected UBattleCardWidget temporarily shows the resolved Damage/Block value in its card-face description
→ leave/unfocus/submit/revision change
→ card face restores its frozen historical description
```

Do not render a separate `Damage`, `Block` or `Energy a -> b` Preview label.

Energy legality/cost fields remain in `FImmediateCardPreview` because Gameplay owns legality and the DTO remains useful for validation, but A3-5 does not expose a separate Energy-loss preview surface.

## Card-face value authority

Normal A3-1 card-face text remains:

```text
current source-side/self semantic values
```

For one concrete PreviewTarget, A3 uses the already-resolved `ImmediatePreview.Operations` and overrides only the matching validated semantic argument names in the existing authored `FText::Format` description.

Example:

```text
DA_Card_Strike format = Deal {Damage} damage.
A3-1 current card face = Deal 6 damage.
Immediate target-specific Damage Operation = 9
A3 card face while target is nominated = Deal 9 damage.
```

Unsupported effects are not simulated or fabricated. Their normal A3-1 card-face arguments remain unchanged.

UI/UMG must not parse formatted text to recover numbers and must not rerun Damage/Block formulas. `BattleManager`/Effect pipelines remain the value authority.

## Comparison color

Each supported Immediate operation carries its immutable authored `BaseAmount` plus Gameplay-resolved `ResolvedAmount`.

First Native convention:

```text
ResolvedAmount > BaseAmount  -> increased emphasis (red)
ResolvedAmount < BaseAmount  -> decreased emphasis (blue)
ResolvedAmount == BaseAmount -> normal card-face style
```

The current Native card Designer exposes its description as a plain `UTextBlock`, so the first C++ implementation applies the comparison emphasis to that description text surface as a whole. Exact per-number run coloring requires a later `RichTextBlock`/rich-run Designer migration; that asset-level refinement must not reintroduce a standalone Preview overlay or Gameplay calculations in UMG.

## A2 ownership boundary

`OV_PlayArea` is A2-only.

```text
A3 card-face Preview -> HB_Hand / selected UBattleCardWidget only
A2 CardPlayed        -> OV_PlayArea only
```

A3 must never add a Preview child to `OV_PlayArea`.

On target submission:

```text
restore selected card face
→ clear transient Preview DTO/PreviewTarget
→ authoritative RequestPlayCard revalidation
→ Gameplay resolution
→ A2 committed Presentation
```

The A2 `CardPlayed` predicates, token ownership, reducers and FinalSnapshot semantics are not changed by this amendment.

## Preview notification ownership

Preview target nomination/clear is **not** structural HUD state and must not use the generic ViewModel `OnChanged` channel.

The Native HUD structural channel rebuilds formal surfaces, including:

```text
OnChanged
→ RefreshHUDFromViewModel
→ RefreshHand
→ HB_Hand.ClearChildren()
→ recreate Hand card Widgets
```

Destroying/recreating Hand Widgets during target hover or immediately before submission invalidates the stable historical Hand Widget/geometry that A2 `CardPlayed` uses as its animation start anchor.

Locked split:

```text
selection / frozen Presentation / interaction structure
→ OnChanged
→ normal Native HUD refresh may rebuild Hand

PreviewTarget / ImmediatePreview only
→ OnPreviewChanged
→ update/restore selected card face only
→ MUST NOT rebuild HB_Hand
```

`SetPreviewTargetById()` and `ClearPreviewTarget()` therefore publish only `OnPreviewChanged`. Selection cancel, accepted submission, revision invalidation and committed Presentation still use the normal structural `OnChanged` path as appropriate.

## CardPlayed rejection diagnostics

Until the production PIE handoff is confirmed, a rejected Native `CardPlayed` playback logs precise read-only diagnostics with prefix:

```text
[BattleHUD][CardPlayedReject]
```

The diagnostics report the CardPlayed token/record identity and all major acceptance predicates, including ViewModel/Hand/PlayArea validity, card snapshot and participant identity, Energy/cost consistency, historical Hand index/widget matching, runtime-id match counts, PlayArea child count, and child class/name summaries.

Diagnostics must never mutate Gameplay, ViewModel, Widget ownership or Presentation state.

## Focused acceptance

After this amendment:

```text
1. Editor Build once.
2. Run SlayTheSpireDemo.UIA3.NativePreviewIntegration once; expected 3/3 Success.
3. Run one production L_BattleTest PIE session.
```

Focused Automation must additionally prove Preview nomination/clear emits `OnPreviewChanged` without emitting structural `OnChanged`.

PIE must prove:

```text
Strike / Enemy:
selected card face changes to target-specific Damage
no standalone Damage/Energy preview appears
hover/leave does not recreate formal Hand Widgets
submit -> card-face Preview clears -> A2 played card visibly enters OV_PlayArea

Defend / Player:
selected card face changes to current Block
no standalone Block/Energy preview appears
submit -> card-face Preview clears -> committed A2 playback remains coherent

comparison styling:
value above authored base -> red emphasis
value below authored base -> blue emphasis
value equal to authored base -> normal style

revision invalidation:
old selection/Preview does not survive a new StateRevision
```

If the played card still fails to appear and `[BattleHUD][CardPlayedReject]` is present, capture those lines before changing A2 behavior. Do not weaken sealed A2 acceptance predicates merely to make the visual start.
