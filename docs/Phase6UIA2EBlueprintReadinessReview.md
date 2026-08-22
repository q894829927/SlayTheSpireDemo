# Phase 6UI-A2E Blueprint Readiness Review

Date: **2026-08-22**

Status: **C++ HARDENING IMPLEMENTED / UE5.8 REVALIDATION PENDING / WBP WIRING NOT STARTED**.

This review is based on the current committed C++ source and the saved-asset graph/layout snapshot in `docs/WBPSavedBlueprintSnapshot.md`. The snapshot remains the source-readable description of the real saved `.uasset` state; this document does not claim any Blueprint asset was modified.

## 1. Readiness conclusion

```text
FPresentationRecord / typed payloads      READY / Blueprint-readable
FPresentationPlaybackToken                READY / Blueprint-readable
Controller ordering + reducers            SEALED by A2D5
Presenter -> Widget assembly              READY
Existing WBP_BattleHUD display surface    READY to extend
Blueprint async completion boundary       HARDENED in C++
Fail-safe stale-visual cancellation        HARDENED in C++
Concrete A2E WBP playback router           NOT WIRED YET
PIE end-to-end acceptance                 NOT RUN YET
```

No Gameplay rule, Record taxonomy, Envelope contract, reducer behavior or test discovery count was added by this hardening.

## 2. C++ hardening added before WBP wiring

### 2.1 Controller-facing wrapper + Blueprint playback event

`UBattlePresentationController` still calls the C++ method:

```cpp
Widget->PlayPresentationRecord(Record, Token);
```

`UBattleHUDWidgetBase` now owns that method as a controller-facing wrapper. It establishes the exact active visual token before entering Blueprint, then dispatches the Blueprint-native event shown in the editor as:

```text
Play Presentation Record
```

C++ name:

```cpp
BeginPresentationRecordPlayback(Record, Token)
```

Blueprint contract:

```text
return false
= no asynchronous visual was started
= Controller uses immediate native fallback

return true
= asynchronous visual really started
= Blueprint must later call Notify Presentation Finished(Token)
```

The current saved `WBP_BattleHUD` does not yet implement this event; adding it belongs to the next user-side A2E asset step.

### 2.2 Exact stale-visual cancellation

The Widget base tracks exactly one active presentation visual token because the Controller offers only one active Record at a time.

New Blueprint event:

```text
Cancel Presentation Record Playback(Token)
```

C++ name:

```cpp
CancelPresentationRecordPlayback(Token)
```

The base clears ownership before entering Blueprint. Therefore a stale cancellation/completion callback cannot clear a newer tracked visual.

Blueprint cancellation is presentation-only:

```text
stop matching animation/timer
clear transient played-card / floating-number visual state
clear locally stored matching Token
DO NOT call Notify Presentation Finished from cancel
DO NOT mutate Gameplay/ViewModel authoritative display data
```

The cancellation event is driven when a still-tracked visual becomes obsolete because the ViewModel advances through timeout/fail-safe/collapse/unavailable handling, and directly before the Widget's explicit `Skip Presentation` wrapper collapses playback.

Normal successful completion clears the matching tracked visual ownership first and suppresses cancellation while Controller synchronously advances the completed historical snapshot.

### 2.3 Blueprint completion is forced non-reentrant

`UBattleHUDWidgetBase::NotifyPresentationFinished(Token)` no longer forwards to the Controller in the same Blueprint call stack. It schedules a one-shot CoreTicker callback and then forwards the token.

Therefore even a miswired Blueprint graph such as:

```text
Event Play Presentation Record
→ Notify Presentation Finished(Token)
→ Return true
```

cannot synchronously re-enter `UBattlePresentationController::StartNextRecord()` before the event returns.

This does not make synchronous completion the recommended Blueprint pattern. Correct A2E wiring remains:

```text
Play animation/timer
→ Return true
...
Animation Finished
→ Notify Presentation Finished(Token)
```

For records with no asynchronous visual, return `false` instead of calling Notify immediately.

### 2.4 Blueprint cannot bypass the Widget hardening

`UBattlePresentationController::NotifyPresentationFinished` and `SkipPresentation` remain public C++ methods but are no longer `BlueprintCallable`.

Concrete WBP code must use the inherited `UBattleHUDWidgetBase` nodes:

```text
Notify Presentation Finished
Skip Presentation
```

This prevents a Blueprint from bypassing deferred completion or exact visual cancellation by invoking the Controller directly.

## 3. Automation coverage without increasing discovered test count

The existing top-level test:

```text
SlayTheSpireDemo.Phase6UIA2D4.Playback.TerminalTimeout
```

now contains two sub-scenarios instead of adding a new top-level Automation test.

### Timeout cancellation

```text
Terminal Record offered asynchronously
→ Controller waiting
→ forced timeout
→ historical reducer/final snapshot advances
→ Widget receives exactly one Cancel Presentation Record Playback
→ canceled Token == exact abandoned playback Token
→ Gameplay ActionQueue remains healthy
```

### Accidental synchronous Blueprint Notify

```text
Blueprint probe calls Notify from inside Play event
→ Widget schedules deferred CoreTicker completion
→ Controller still waiting when Play event returns
→ no re-entrant terminal outcome/watermark advance
→ ticker callback forwards Token
→ Controller completes exactly once
→ no fail-safe visual cancellation
→ no stale timeout completion remains
```

The Phase6R workflow's expected aggregate remains `100`; this hardening adds assertions, not a 101st top-level test.

## 4. Saved WBP baseline used for A2E

`docs/WBPSavedBlueprintSnapshot.md` records the current real saved `WBP_BattleHUD` structure, including:

```text
Combatant_PlayerPresentation
Combatant_EnemyPresentation
PlayerPanel / EnemyPanel HP + Block
WB_PlayerStatuses / WB_EnemyStatuses
EnergyPanel
HB_Hand
DrawPilePanel / DiscardPilePanel / ExhaustPanel
Btn_EndTurn / Btn_Confirm / Btn_Cancel
Txt_Feedback
Overlay_Terminal
```

The current saved `Battle HUD View Model Changed` graph already rebuilds Hand, pile counts, Energy, Player/Enemy HP/Block, Intent, feedback, controls, terminal state and combatant presentations.

A2E must extend this saved graph; it must not replace it with a second HUD state owner.

The current saved asset has no committed-record playback router yet. That distinction remains explicit:

```text
CURRENT SAVED
= existing ViewModel-driven HUD and A1 interaction wiring

PLANNED / NOT WIRED
= A2E Play Presentation Record / Cancel Presentation Record Playback router
```

## 5. Next WBP implementation order after C++ revalidation

Do not wire all ten record types at once. Start with the smallest real committed-history vertical slice:

```text
CardPlayed
→ Damage
→ CardZoneChanged
```

using one normal Strike in PIE.

Recommended existing saved controls to reuse:

```text
CardPlayed
→ current HB_Hand / WBP_BattleCard data
→ new transient presentation-only PlayArea visual layer if needed
→ EnergyPanel uses CardPlayed.EnergyBefore/EnergyAfter

Damage
→ existing Enemy/Player HP + Block controls
→ choose combatant by TargetPresentationId
→ animate from frozen Damage payload Before values to After values

CardZoneChanged
→ retire transient played-card visual
→ final ViewModel refresh owns Discard/Exhaust count after completion
```

Only after this vertical slice works should A2E add:

```text
StatusChanged
EnergyChanged / EndTurn macro
DeckShuffled / Draw / Discard
Victory / Defeat / ResolutionFault
PresentationUnavailable separation
```

## 6. Revalidation required before asset wiring is considered source-ready

Because runtime public UFUNCTION exposure and Widget completion timing changed, run UE5.8 validation before treating this hardening as sealed:

```text
SlayTheSpireDemoEditor Development build
Phase6UIA2D4 focused suite
Phase6R aggregate expected 100/100
Shipping exclusion expected PASS
```

Until those runs are reported, this document remains **UE5.8 REVALIDATION PENDING**.
