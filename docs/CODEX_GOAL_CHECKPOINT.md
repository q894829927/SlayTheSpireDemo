# Codex Goal Checkpoint — Phase 6UI-A3

Last updated: **2026-09-02**

## Goal

Complete Phase 6UI-A3 without changing Gameplay authority, sealed A2 committed-presentation semantics, Native/Legacy ownership or phase ordering.

```text
A3 = pre-commit read-only current-state supported Operation values
A2 = post-commit playback of immutable facts that actually committed
```

## Current status

```text
UI-A3: IN PROGRESS / AUTHORIZED
A3-1 Dynamic Text: COMPLETE / VALIDATED / SEALED
A3-2 Target-Specific Current-State Preview: COMPLETE / VALIDATED / SEALED
A3-3 Energy + Target-Aware Legality: COMPLETE / VALIDATED / SEALED
A3-4 ViewModel Transient Preview Lifecycle: COMPLETE / VALIDATED / SEALED
A3-5 Native card-face Preview + A2/A3 PIE: IMPLEMENTED / REVALIDATION PENDING
```

The previous A3-5 standalone Preview implementation passed its focused Automation but failed production PIE because the played card no longer appeared in the A2 play area. That run is historical evidence only and does not validate the current head.

## Active authority

```text
AGENTS.md
Source/SlayTheSpireDemo/UI/AGENTS.md
docs/Phase6UIA3Implementation.md
docs/Phase6UIA3CardFacePreviewAmendment.md
docs/Phase6UIA3DynamicTextImplementation.md
docs/ValidationExecutionPolicy.md
```

`docs/Phase6UIA3CardFacePreviewAmendment.md` is the later explicit A3-5 UX amendment and controls visible Preview presentation where it differs from the original A3 implementation document.

UI-A2 remains complete/sealed. Native HUD remains the sole active production implementation. Legacy HUD/Card/Status remain retained/deprecated with zero intended production runtime dependency.

## Sealed predecessor evidence

```text
A3-2A Editor Build: PASS
A3-2A Automation SlayTheSpireDemo.UIA3.ImmediatePreview: 3/3 Success, exit 0
A3-2B Editor Build: PASS
A3-2B Automation SlayTheSpireDemo.UIA3.ImmediatePreviewQuery: 2/2 Success, exit 0

A3-3 Editor Build: PASS
A3-3 Automation SlayTheSpireDemo.UIA3.ImmediatePreviewLegality: 2/2 Success, exit 0
A3-3 compatibility rerun ImmediatePreviewQuery: 2/2 Success (user-reported)

A3-4 Editor Build: PASS (user-reported)
A3-4 Automation SlayTheSpireDemo.UIA3.ViewModelPreviewLifecycle: 3/3 PASS (user-reported)
```

Historical shared-contract note: the old `SlayTheSpireDemo.Phase6UIA1.ViewModel` suite contains two stale assertions that mutate CardData/Status after the opening frozen Presentation baseline and expect a later subscriber to see those mutable changes. Do not weaken sealed frozen Presentation semantics to satisfy them.

## Why A3-5 was redesigned

The first A3-5 visible Preview used a dynamically created `UBattleImmediatePreviewTextBlock` and attached it to `OV_PlayArea`.

That was a bad ownership fit because sealed A2 `CardPlayed` treats `OV_PlayArea` as its committed-animation container and rejects starting its Native visual when its strict historical/presentation preconditions are not met. Production PIE showed the symptom:

```text
Gameplay request/resolution proceeds
but played card does not visibly enter the Native play area
```

The Controller correctly immediate-falls-back when a Native Record visual returns false, so Gameplay can remain correct while the CardPlayed animation is silently skipped.

The current redesign removes A3 from `OV_PlayArea` completely.

## Current A3-5 visible design

```text
select card
→ hover/focus a legal PreviewTarget
→ BattleManager builds FImmediateCardPreview through current read-only Gameplay pipelines
→ ViewModel stores exact BattleId/StateRevision/Card/Target stamped Preview
→ selected UBattleCardWidget temporarily displays target-specific card-face values
→ leave/unfocus/submit/revision change
→ card face restores frozen historical description
```

Visible rules:

```text
No standalone Damage preview label
No standalone Block preview label
No standalone Energy-loss preview label
A3 never adds a child to OV_PlayArea
```

Energy/cost remain in the DTO for Gameplay-owned legality only.

### Card-face semantic formatting

A3-1 remains the normal card-face baseline. `FBattleTextResolver` first builds the validated current source-side/self semantic arguments, then the target-specific A3 path overrides only semantic names supplied by already-resolved `ImmediatePreview.Operations`.

This means:

```text
Strike:   Deal {Damage} damage. -> target-specific Damage replaces {Damage}
Defend:   Gain {Block} Block.   -> current resolved Block replaces {Block}
Uppercut: supported Damage may change while unsupported Weak/Vulnerable values keep normal A3-1 text
```

UI does not parse formatted text and does not rerun Damage/Block formulas.

Each supported operation now carries:

```text
BaseAmount     = authored immutable effect amount
ResolvedAmount = current Gameplay-pipeline result
```

Native comparison styling:

```text
ResolvedAmount > BaseAmount  -> red emphasis
ResolvedAmount < BaseAmount  -> blue emphasis
ResolvedAmount == BaseAmount -> original description style
```

The current Native Designer uses a plain `UTextBlock` for the description, so this C++ slice colors that description surface as a whole. Exact per-number run coloring requires a later RichText Designer migration; it must not reintroduce a separate Preview overlay or Gameplay calculations in UMG.

## CardPlayed rejection diagnostics

A precise read-only diagnostic now runs only when a Native `CardPlayed` Record is rejected before Controller immediate fallback.

Search prefix:

```text
[BattleHUD][CardPlayedReject]
```

It logs:

```text
Playback token and Record identity
Card RuntimeId / CardId / HandIndexBefore / PlayAreaIndexAfter
ViewModel / HB_Hand / OV_PlayArea validity
card snapshot validity
source/target PresentationId validity
EnergyBefore/EnergyAfter/CostPaid/CardCost consistency
ViewModel historical Energy match
actual OV_PlayArea child count vs expected
ViewModel Hand index/card snapshot match
formal Hand child count vs ViewModel Hand count
required historical Hand widget/card snapshot match
ViewModel and Widget RuntimeId match counts
Hand and PlayArea child class/name summaries
```

Diagnostics do not mutate Gameplay, ViewModel, Widget ownership or Presentation state and must not be used as justification to weaken sealed A2 predicates.

## Focused Automation at current head

Prefix remains:

```text
SlayTheSpireDemo.UIA3.NativePreviewIntegration
```

Expected exactly 3 tests:

```text
DedicatedPreviewEventsStayIndependentFromInspection
SelectedCardFaceRendersAndRestoresPreviewValues
TargetSubmissionClearsPreviewBeforeAuthoritativeRequest
```

The card-face test proves target-specific visible replacement, red-above-base, blue-below-base and restoration. The handoff test proves A3 card-face Preview does not alter formal Hand structure and clears before authoritative submission.

## Validation history

Historical pre-redesign evidence:

```text
Editor Build: PASS (user-reported)
old NativePreviewIntegration: 3/3 Success, exit 0 (user-reported)
production L_BattleTest PIE: FAIL — played card did not appear in play area
```

Current card-face/diagnostic head:

```text
Editor Build: NOT YET RUN
SlayTheSpireDemo.UIA3.NativePreviewIntegration: NOT YET RUN
Production L_BattleTest PIE: NOT YET RUN
Native WBP compile/save after current parent C++ changes: NOT YET CONFIRMED
```

## Next exact validation

Run only:

```text
1. Development Editor Build once.
2. Compile/Save WBP_BattleHUD_Native and WBP_BattleCard_Native once; require 0 Blueprint errors.
3. Run SlayTheSpireDemo.UIA3.NativePreviewIntegration once; expect 3/3 Success, exit 0.
4. Run one production /Game/SlayTheSpireDemo/Maps/L_BattleTest PIE session.
```

PIE acceptance:

```text
Strike / Enemy:
select Strike -> hover/focus Enemy -> selected Strike card-face Damage changes
no separate Damage/Energy preview appears
submit Enemy -> card-face Preview clears -> A2 played card visibly enters OV_PlayArea -> committed Damage playback remains coherent

Defend / Player:
select Defend -> hover/focus Player -> selected Defend card-face Block changes
no separate Block/Energy preview appears
submit Player -> card-face Preview clears -> A2 playback remains coherent

styling:
above authored base -> red
below authored base -> blue
equal -> normal style

revision invalidation:
old selection/Preview does not survive new StateRevision
```

If CardPlayed still does not visibly enter the play area, capture the `[BattleHUD][CardPlayedReject]` lines from that PIE run and diagnose the exact failed predicate before making any further behavioral change.

Do not run Phase6R, A2D5, Shipping, broad Scenario suites or Legacy parity unless a concrete failure invalidates a sealed shared contract. Do not start Phase 7 until A3-5 is validated and sealed.
