# UI-A2E Blueprint Playback Validation Log

Last updated: 2026-08-30

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
- Update/reduction and removal are wired in the saved Blueprint (`Branch(bCreated)` update path plus Router identity lookup for both non-removed and removed rows), but neither slice is **PIE-validated** yet.
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

### StatusChanged — update/reduction and removal

Status: **VALIDATED**

The current saved HUD routes both non-creation lifecycles by the exact identity
`TargetPresentationId + StatusId + RuntimeSequence`:

```text
TargetKnown
→ bRemoved?
   ├ true  → FindStatusWidgetByIdentity → Found → PlayStatusChangedPresentation(FoundWidget)
   └ false → bCreated?
              ├ true  → PlayStatusChangedPresentation(None)
              └ false → FindStatusWidgetByIdentity → Found → PlayStatusChangedPresentation(FoundWidget)
```

The removal playback reuses the found widget and sets it to `Collapsed`; it does not
create a second status row or call `RemoveFromParent`. The update path uses the frozen
`StatusView` and the single `ActiveStatusPresentationWidget` output for `SetStatusView.self`.
The exact-token completion and StatusChanged Cancel rebuild remain shared with the
creation path.

Visible floating PIE acceptance on saved HUD SHA-256 `574FF058...` used the real
`TestApplyPhase5AStatuses` Gameplay path and two real EndTurn requests:

```text
Strength#1 Amount=2 Created=true
Strength#1 Amount=3 Created=false
Weak#3 Amount=2 Created=true
Weak#3 Amount 2 -> 1 Reason=3
Weak#3 Amount 1 -> 0 Reason=3
```

The HUD showed one Player Strength widget at amount `3`, one Enemy Weak widget at
amount `2` and then `1`, and no duplicate or A→B→A flashback. After the removal
Record the exact Weak widget disappeared and stayed absent. Later Records completed
and the controller returned to the ready/Idle state. Therefore update/reduction and
removal are both validated on the unchanged saved asset.

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
CardZoneChanged         VALIDATED (current producer set)
StatusChanged creation  VALIDATED

StatusChanged update    VALIDATED
StatusChanged removal   VALIDATED
EnergyChanged           VALIDATED
DeckShuffled            VALIDATED
Victory                 VALIDATED
Defeat                  VALIDATED
ResolutionFault         VALIDATED
PresentationUnavailable separation VALIDATED
Global Cancel/Reconcile VALIDATED
Scenario A-E PIE        VALIDATED
Active Skip/Input Unlock VALIDATED
```

A2E implementation is **VALIDATED**, but remains **UNSEALED** until the final-head
A2D5, Phase6R, and Shipping-exclusion gates pass.

## Locked next step

Immediate next work: **final saved Blueprint snapshot, local implementation commit,
then the final-head seal gates**.

## Batch 2 — Energy / CardZone / Shuffle acceptance (2026-08-31)

Status: **VALIDATED** on saved HUD SHA-256
`7BF7488DC97F5A1E22CDB12BF8A29E9D9EBC4C166476BFEFCC24C326EACDCB55`.

Real PIE used the committed Gameplay producers rather than handmade payloads:

```text
CardPlayed: Energy 5/5 -> 4/5, CostPaid=1, no duplicate EnergyChanged
EndTurn: 4 Hand -> Discard records in order
Draw: 5 DrawPile -> Hand operations in order, no duplicate card
DeckShuffled: Moved=5, Draw 0->5, Discard 5->0, exactly once
Post-shuffle Draw continued; final Draw=4
ActionQueue empty -> ReadStateReady / State=2; input usable
```

The Batch 2 architecture review found and blocked on five P1 wiring defects. The
saved graph was corrected for the formal Energy format, Energy Cancel restoration,
CardZone identity/ToZone gates, both shuffled-count comparisons, and
`PlayedCardWidget` cleanup. The corrected HUD compiled with zero errors, was saved,
and passed the minimal real PIE regression above.

Focused validation ran `SlayTheSpireDemo.Phase6UIA2C`: 8 total, 5 succeeded,
3 succeededWithWarnings, 0 failed, 0 notRun. The warning-bearing cases are the
expected rollback/fail-soft paths. Final-head A2D5, Phase6R, and Shipping exclusion
were intentionally not run at this batch boundary.

## Batch 3 — Terminal acceptance (2026-08-31)

Status: **VALIDATED** on saved HUD SHA-256
`24BA3F8B9F24DF9713BB29A6DB8F64EAAD38607630AEE968DC47C33F746983D5`.

Real terminal evidence:

```text
Victory: enemy 29/100 -> 0/100; formal Overlay showed 胜利
Defeat: player 2/80 -> 0/80; formal Overlay showed 战斗失败
ResolutionFault: real EndTurn queue fault; 7 Records; exactly one final fault;
                 formal Overlay Visible with 战斗结算异常
PresentationUnavailable: real freeze failure; no fault Envelope; Gameplay stayed
                         PlayerTurn / Outcome=None; terminal Overlay Collapsed
```

Victory and Defeat followed their preceding committed Records. The fault and
unavailable cases ran in separate real `UEDPIE` worlds through existing authoritative
testing producers; the temporary Editor-only harness constructed no Record or Payload
and was deleted afterward. A standard Editor build then succeeded with no C++ diff.

The one Batch 3 architecture review reported no P0/P1/P2 finding. The one focused
`SlayTheSpireDemo.Phase6UIA2C` run completed 8 total: 5 succeeded,
3 succeededWithWarnings, 0 failed, 0 notRun. This was not a final-head gate.

## Batch 4 — Global Cancel / Reconcile and full PIE acceptance (2026-08-31)

Status: **VALIDATED** on saved HUD SHA-256
`990125C951D52D5F23194D9EB7C079C2F3C514C78A285DF0DDE273B6B1C0F94A`.

The final Cancel graph is single-direction:

```text
clear active timer
-> switch ActivePresentationType
-> type-specific historical ViewModel restore
-> clear Played/Hidden/Drawn/Status transient references
-> clear Damage/Block target flags
-> ActivePresentationType=None
-> ActivePresentationToken=default
-> end without Notify
```

The independent Batch 4 review initially blocked four P1 issues: a cleanup loop,
disconnected cleanup fields, reversed Damage Cancel visibility/opacity values, and
missing Hand-discard restoration. The saved graph was corrected, compiled, saved,
reloaded, and the final directed review passed with no remaining P0/P1.

Real PIE acceptance used real Gameplay/UI request paths rather than constructed
presentation data:

```text
Scenario A: Strike -> Energy 5/5 to 4/5; Enemy 100/100 to 94/100; input returned
Scenario B: Uppercut -> Enemy 100 to 87; Weak/Vulnerable 2 -> 1 -> 0; no duplicate
Scenario C: discard x5 -> TurnEnded -> enemy Damage -> draw x5 -> shuffle -> draw;
            final PlayerTurn, Energy 5/5, queue caught up
Scenario D: Victory and Defeat terminal surfaces PASS
Scenario E: ResolutionFault and PresentationUnavailable isolation PASS
```

A temporary Editor-only PIE Automation harness then used the formal
`ViewModel->RequestEndTurn()` path, waited for a real active playback token, and
called public `WidgetInstance->SkipPresentation()`. It confirmed resolving/input
locked before Skip, no waiting/backlog after catch-up, all Blueprint transient/type/
token fields cleared, stale-token rejection after the timer window, a subsequent
real request completing normally, and final Idle/input unlocked. The harness created
no Record/Payload, was removed, and the standard Editor build succeeded with no
Source diff. This run supersedes an earlier discarded harness assertion that used
the debug `TestAttack()` producer and incorrectly required it to set ViewModel
`Resolving`.

The saved Blueprint now preserves the exact status identity:

```text
TargetPresentationId
+ StatusId
+ RuntimeSequence
```

Validated behavior (saved as HUD SHA-256 `574FF058...`):

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

The required StatusChanged Cancel restoration is also saved and compiled:

```text
ActivePresentationType == StatusChanged
→ IsValid(ViewModel)
→ RebuildStatusIcons(ViewModel.Player.Statuses, WB_PlayerStatuses)
→ RebuildStatusIcons(ViewModel.Enemy.Statuses, WB_EnemyStatuses)
→ common cleanup clears ActiveStatusPresentationWidget / Type / Token
→ no RemoveFromParent on the Status path; no Notify; no second Token comparison
```

Compile/save evidence on 2026-08-30:

```text
WBP_BattleStatus compile invoked; no compiler error logged
WBP_BattleHUD compile: UE Blueprint tool returned successfully after the duplicate-link cleanup
WBP_BattleHUD save: AssetTools returned true; is_dirty=false
WBP_BattleHUD saved 21:50:03 Asia/Shanghai
SHA-256 574FF05882D4876831B373D60D23CA3DFF564AE19E4AD143B77CE4724E48EAA3
```

The earlier independent architecture review reported P0=0 and behavior-architecture P1=0. This turn changed only the redundant Blueprint data link described above; after the edit the saved HUD was re-read, `WBP_BattleHUD is_dirty=false`, and the disk hash is `574FF058...`.

Remaining predecessor evidence before `update/reduction` can be marked VALIDATED:

```text
Real PIE: creation regression; reapply/increase (same identity, one widget,
no flashback); non-removing reduction/TurnEndDecay (AmountAfter > 0, same widget).
```

The 2026-08-30 MCP PIE run reached `ReadStateReady` only. The Editor had no capturable Slate window and produced no current-run Status commit, so this is explicitly **not PIE acceptance evidence**. Earlier Gameplay logs are not evidence for the current saved Blueprint.

Cancellation of an update/reduction presentation must restore the formal status list from the current historical ViewModel state rather than leaving the temporary `AmountAfter` visible.

After update/reduction is validated, perform PIE acceptance for the already-saved removal path using the same exact identity. Only after full `StatusChanged` validation should mainline proceed to:

```text
EnergyChanged + EndTurn
→ shuffle / remaining zone transitions
→ terminal records
→ full A2E PIE acceptance
```
