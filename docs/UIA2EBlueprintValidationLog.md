# UI-A2E Blueprint Playback Validation Log

Last updated: 2026-08-29

This file records owner-confirmed Blueprint/PIE validation evidence for UI-A2E so later work does not regress or accidentally re-open already validated slices.

`docs/WBPSavedBlueprintSnapshot.md` remains the source-readable snapshot of the currently saved WBP structure. This file records validation status and acceptance evidence.

## Current validated playback slices

### CardPlayed

Status: **VALIDATED**

Validated behavior:

```text
CardPlayed
→ original Hand card is hidden
→ transient presentation card appears in OV_PlayArea
→ Blueprint owns the async playback token
→ completion goes through NotifyPresentationFinished(Token)
```

The transient played card is not removed by CardPlayed itself; retirement remains owned by the later `CardZoneChanged(PlayArea -> Destination)` record.

### Damage

Status: **VALIDATED**

Current visible playback:

```text
Damage
→ validate TargetPresentationId against Player / Enemy
→ target RenderOpacity = 0.45
→ Txt_DamagePresentation displays frozen Record.IncomingDamage
→ target HUD temporarily displays Record.HPAfter
→ target HP ProgressBar uses HPAfter / Max(MaxHP, 1)
→ target HUD temporarily displays Record.BlockAfter
→ StartPresentationFinishTimer (0.5 s)
→ FinishPresentationRecord
→ hide damage text
→ restore target RenderOpacity = 1.0
→ NotifyPresentationRecordFinished
```

Contract checks already validated:

- `IncomingDamage` is displayed directly from the frozen Record.
- HP uses `HPAfter`; Block uses `BlockAfter`.
- Blueprint does not recompute `HPDamage` or `BlockedDamage`.
- Blueprint does not write authoritative HP/Block values back into the ViewModel.
- Controller reducer advances the historical ViewModel only after the exact playback token completes.

Owner-confirmed PIE evidence:

```text
Normal Strike:
Enemy HP 100/100
→ Strike
→ Damage presentation displays 94/100
→ final authoritative HUD remains 94/100
→ playback returns to Idle

Blocked Damage:
Player HP = 80/80
Player BlockBefore = 5
IncomingDamage = 5
Gameplay result: blocked = 5, hpDamage = 0
Record HPAfter = 80
Record BlockAfter = 0
→ Damage presentation keeps HP at 80/80 and shows Block 0
→ IncomingDamage is not incorrectly treated as HPDamage
```

### BlockChanged

Status: **VALIDATED**

Current visible playback:

```text
BlockChanged
→ Router validates TargetPresentationId against Player / Enemy
→ unknown target returns false to C++ immediate fallback
→ PlayBlockChangedPresentation
→ ActivePresentationToken = Token
→ ActivePresentationType = BlockChanged
→ target block text = frozen Record.BlockAfter
→ StartPresentationFinishTimer (0.5 s)
→ FinishPresentationRecord
→ NotifyPresentationRecordFinished
```

Contract checks already validated:

- Uses frozen `BlockAfter` directly.
- Does not recompute `BlockBefore + BlockDelta`.
- Does not mutate ViewModel authoritative state.
- Player and Enemy target paths both use the shared completion timer.
- Finish does not restore `BlockBefore`.
- Cancel does not maintain a second authoritative Block state.

Owner-confirmed PIE acceptance:

```text
Gain:
Player Block 0 → N
→ BlockChanged presentation is visible for the async interval
→ final Block remains N
→ playback continues and returns to Idle

TurnStartClear:
Player Block N → 0
→ BlockChanged presentation shows 0
→ no N → 0 → N → 0 flashback
→ playback continues into the player turn
→ final Block remains 0
```

Therefore:

```text
BlockChanged Blueprint Playback = VALIDATED
```

### CardZoneChanged

Status: **VALIDATED** for the currently implemented `FromZone = PlayArea` playback slice.

Validated behavior:

```text
CardZoneChanged(PlayArea -> Destination)
→ validates PlayedCardWidget and RuntimeId
→ async completion path runs
→ transient PlayedCardWidget is removed in FinishPresentationRecord
→ controller advances the historical zone state
```

### StatusChanged — creation (`bCreated = true`)

Status: **VALIDATED**

Saved implementation:

```text
StatusChanged Router
→ validate TargetPresentationId against Player / Enemy
→ require bCreated = true
→ require bRemoved = false
→ invalid/non-creation case returns false to C++ immediate fallback
→ PlayStatusChangedPresentation
→ ActivePresentationToken = Token
→ ActivePresentationType = StatusChanged
→ MakePresentationStatusView(StatusChanged)
→ create WBP_BattleStatus
→ SetStatusView using frozen Record fields
→ add transient status widget to Player or Enemy WrapBox
→ StartPresentationFinishTimer (0.5 s)
→ FinishPresentationRecord
→ NotifyPresentationRecordFinished
→ clear ActiveStatusPresentationWidget reference
```

Frozen DTO conversion:

```text
StatusId        ← Record.StatusId
RuntimeSequence ← Record.RuntimeSequence
DisplayName     ← Record.DisplayName
Description     ← Record.DescriptionAfter
Amount          ← Record.AmountAfter
bUseAtlasIcon   ← Record.bUseAtlasIcon
UVOffset        ← Record.UVOffset
UVScale         ← Record.UVScale
TrimOffset      ← Record.TrimOffset
TrimScale       ← Record.TrimScale
```

Validated contract:

- Creation accepts only a known Player/Enemy target.
- Creation accepts only `bCreated = true && bRemoved = false`.
- Update/reduction/removal still return `false` and use C++ immediate fallback.
- The visual is built from frozen Record payload data; Blueprint does not query `UStatusInstance` or `UStatusData`.
- `StatusId` and `RuntimeSequence` are preserved in the presentation DTO.
- Amount uses `AmountAfter`; description uses `DescriptionAfter`.
- Cancel removes the transient presentation status and does not call normal completion Notify.
- Normal completion does not remove the transient widget before Notify; the subsequent ViewModel refresh rebuilds the formal status list.

Owner-confirmed PIE acceptance:

```text
Target starts without the status
→ play a card/effect that creates Weak/Vulnerable or equivalent
→ StatusChanged creation appears on the correct combatant
→ displayed amount is correct
→ the creation presentation remains visible for the async interval
→ exact-token completion succeeds
→ reducer advances the ViewModel status list
→ the formal HUD status remains visible after rebuild
→ no visible status -> disappear -> status flashback is observed
→ later Records continue and playback returns to Idle
```

Interpretation of the flashback check above: the invalid `status visible → disappear → visible again` sequence was **not** observed during acceptance.

Therefore:

```text
StatusChanged creation Blueprint Playback = VALIDATED
```

## Shared async / cancellation contract

Status: **VALIDATED for the currently wired and accepted slices**

Current shared rules:

```text
ActivePresentationToken
ActivePresentationType
ActivePresentationTimer
StartPresentationFinishTimer
FinishPresentationRecord
NotifyPresentationRecordFinished
```

Rules to preserve:

- `Return true` only when Blueprint actually starts valid async playback.
- Unknown/invalid targets must return `false` so C++ can use immediate fallback.
- `NotifyPresentationFinished(ActivePresentationToken)` must occur before clearing the active token.
- Cancel clears the timer and presentation-only transient visuals, but does not call normal completion Notify.
- Blueprint playback reads frozen Record data and must not query mutable historical Gameplay state.
- ViewModel represents historical facts whose playback has already completed.

## Current A2E state

```text
CardPlayed              VALIDATED
Damage                  VALIDATED
BlockChanged            VALIDATED
CardZoneChanged         VALIDATED (PlayArea -> Destination slice)
StatusChanged creation  VALIDATED

StatusChanged update    NOT WIRED
StatusChanged removal   NOT WIRED
EnergyChanged           NOT WIRED
DeckShuffled            NOT WIRED
Victory                 NOT WIRED
Defeat                  NOT WIRED
ResolutionFault         NOT WIRED
```

A2E remains **PARTIAL** and must not be marked COMPLETE/SEALED yet.

## Locked next step

Immediate next implementation target: **StatusChanged amount update / increase / reduction**.

The next slice must preserve the exact status identity:

```text
TargetPresentationId
+ StatusId
+ RuntimeSequence
```

Required behavior:

```text
existing formal status row is located by exact identity
→ consume frozen Record.AmountAfter / DescriptionAfter / icon metadata
→ temporarily update that exact row during the active Record
→ short async timer
→ exact-token Notify
→ reducer advances the ViewModel status state
→ normal HUD rebuild produces the same final status value
```

The implementation must not identify a status only by array index or by `StatusId` alone, and must not mutate the ViewModel status array from Blueprint.

Cancellation of an update/reduction presentation must restore the formal status list from the current historical ViewModel state rather than leaving the temporary `AmountAfter` visible.

After update/reduction is validated, implement removal using the same exact identity. Only after full `StatusChanged` validation should mainline proceed to:

```text
EnergyChanged + EndTurn
→ shuffle / remaining zone transitions
→ terminal records
→ full A2E PIE acceptance
```
