# Post-Seal Card-Face Continuity Validation

Date: **2026-09-03**

Status: **COMPLETE / VALIDATED**

## Scope

This validation closes the post-seal Presentation defect where a card with a modified RichText face could visibly regress during committed playback:

```text
Strength-modified Draw -> Hand:
resolved colored value
-> transient/plain white fallback
-> resolved colored value again
```

The fix did not change Gameplay, Strength/Vulnerable modifier semantics, Draw/Shuffle, Sundial, A3 ownership, or FinalSnapshot current-state freezing.

## Validated architecture

Committed historical card snapshots now use one presentation-only projection:

```text
FPresentationCardSnapshot
-> PresentationCardView::MakePresentationOnlyCardView
-> FBattleHUDCardView
```

The mapper preserves the complete frozen presentation payload, including `RichDescription`, and always produces a non-gameplay-playable historical card view.

The formal current-state path remains independent:

```text
FCardReadView
-> ABattleManager::TryFreezePresentationStateSnapshot
-> FBattleHUDCardView with current Gameplay legality
```

`RichDescription` remains a presentation field and is intentionally not added to generic CardPlayed Hand identity matching because committed target-specific text may legitimately differ from the source-side Hand baseline.

## User-reported validation evidence

The user completed the two requested focused Automation gates on the validated current implementation and reported both successful:

```text
SlayTheSpireDemo.Phase6UIA2D4.PresentationCardViewMapper        PASS
SlayTheSpireDemo.Phase6UIA2C.Record.CardZoneChanged             PASS
```

The second prefix includes the existing `CardZoneChanged` regression and the new:

```text
WorkingSnapshotRichContinuity
```

coverage. The continuity test explicitly observes the Controller Stage-B window after Draw A has been reduced into the WorkingSnapshot while Draw B is active and before FinalSnapshot reconciliation. Its frozen RichDescription payloads are non-empty and distinct, and the Working Hand is required to preserve the exact Record payload.

The user then repeated the real Strength-modified Draw PIE scenario and reported:

```text
no visible red -> white -> red transition
```

The card face remained visually continuous through draw presentation, Working Hand takeover, later Record playback, and FinalSnapshot reconciliation.

## Acceptance

The post-seal card-face continuity correction is therefore:

```text
IMPLEMENTED
AUTOMATION VALIDATED
PIE VISUALLY VALIDATED
COMPLETE / VALIDATED
```

No additional Phase6C, Sundial, EnergyGain, TriggerSources, Phase6R, A2D5, Shipping, Legacy-parity, or unrelated UI rerun is required by this correction.
