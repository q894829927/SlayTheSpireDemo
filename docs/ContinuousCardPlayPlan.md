# Continuous Card Play — Gameplay-ahead / Presentation-lag Plan

Date: **2026-09-08**

Status:

```text
DESIGN PROPOSED
NOT IMPLEMENTED
C1 INTERACTIVE-SELECTION FENCE IS A PREREQUISITE
```

## 1. User-facing goal

Allow ordinary cards to be played while the previous card's committed visual playback is still running, without making animation authoritative and without executing two Gameplay resolutions concurrently.

Target feel:

```text
Card A Gameplay commits and becomes stable
→ Card A Presentation is still playing
→ player clicks Card B
→ Card B Gameplay request may already resolve
→ Card B Presentation is appended behind Card A
```

Presentation remains ordered historical playback; Gameplay remains authoritative and deterministic.

## 2. Explicit amendment to sealed UI-A2E input behavior

`docs/Phase6UIA2EImplementation.md` currently locks normal input until Presentation catches up and explicitly says repeated card input during playback must not start another player request.

This new initiative intentionally supersedes that one input-lock rule only after implementation and validation. It does **not** supersede A2E historical-record ordering, frozen snapshot ownership, playback-token correctness, reducer semantics, PresentationUnavailable separation, or Gameplay authority.

Until this plan is implemented and sealed, the old UI-A2E normal-input rule remains production behavior.

## 3. Current implementation and why it is not true continuous play

`UBattleHUDWidget::SelectCard(..., bAllowFastPresentationCatchUp=true)` currently supports a fast-click path by:

```text
active Native Presentation
→ SkipPresentation()
→ collapse to newest frozen snapshot
→ retry the card click next ticker
```

This is **fast catch-up**, not continuous playback. It cancels/skips the previous visual so input can resume.

The target behavior is different:

```text
previous Presentation keeps playing
+
next Gameplay request may proceed independently
```

No automatic Skip is required for a normal rapid play.

## 4. Three timelines

The target architecture has three distinct timelines:

```text
Gameplay Resolution Queue
Presentation Envelope/Record Queue
Player Interaction State
```

### Gameplay

- `BattleActionQueue` remains the only authoritative execution queue.
- Only one Gameplay resolution executes at a time.
- A new card request is accepted only after the previous Gameplay resolution is stable/request-eligible.
- Presentation backlog must not make Gameplay `ResolutionBusy` by itself.

### Presentation

- Every accepted Gameplay resolution still emits immutable Records and a frozen FinalSnapshot.
- `BattlePresentationController` continues consuming Envelopes strictly by ResolutionId/order.
- Card B's Envelope may be queued while Card A's visual is still playing.
- Backlog timeout/skip/collapse remains Presentation-only and never changes Gameplay outcome.

### Interaction

There are two categories of input:

```text
ordinary card-play input
interactive Gameplay input (selection/grid/modal choice)
```

Ordinary card-play input may run ahead of Presentation when the current Gameplay state can revalidate it.

Interactive Gameplay input uses a Presentation fence: the UI must first display the state on which the player is expected to make that choice.

## 5. RuntimeId request facade

The current normal ViewModel path needs newest-revision live UObject bindings before calling:

```cpp
RequestPlayCard(UCardInstance*, ACombatant*)
```

That requirement prevents ordinary input while Presentation intentionally lags.

Add a Gameplay-owned RuntimeId request facade, proposed shape:

```cpp
struct FCardPlayRuntimeRequest
{
    int32 CardRuntimeId = INDEX_NONE;
    FName TargetPresentationId = NAME_None;
};

FGameplayRequestResult RequestPlayCardByRuntimeIdentity(
    const FCardPlayRuntimeRequest& Request);
```

Gameplay resolves the RuntimeId against the **current authoritative Hand**, resolves target PresentationId against current battle participants, then runs the unchanged `ValidatePlayCard` / `RequestPlayCard` rules.

UI never receives or stores an authoritative `UCardInstance*` for this fast path.

Required rejection remains authoritative:

```text
card already left Hand      → CardNoLongerInHand / InvalidCard
not enough Energy           → NotEnoughEnergy
wrong target                → InvalidTarget
Gameplay ActionQueue busy   → ResolutionBusy
pending interactive choice  → ResolutionBusy / interaction-specific rejection
```

No speculative success is allowed from stale historical display state.

## 6. No new Gameplay command queue in the first slice

Do **not** initially add a second card-command queue.

When Presentation is playing an already-sealed Envelope, the Gameplay resolution that produced it is normally already stable. Therefore the first implementation can simply permit a fresh RuntimeId request while Presentation remains active.

```text
Card A Gameplay stable
Presentation A active
→ Card B direct RuntimeId request
→ if Gameplay is request-eligible: execute B now
→ Presentation B joins existing presentation backlog
```

If Gameplay itself is still busy, reject normally rather than buffering a speculative command.

A true buffered input queue is a separate future need and must not be introduced without a concrete UX requirement.

## 7. Interactive Presentation fence

Continuous ordinary play must stop at player-choice effects.

Examples:

```text
Draw → choose Hand card
select card to Exhaust
Grid selection
future modal target/choice effects
```

Contract:

```text
Gameplay may commit facts up to the interactive boundary
→ seal/freeze a player-facing boundary revision
→ Gameplay may enter authoritative pending-selection state
→ Presentation plays toward that boundary independently
→ pending selection is hidden from UI until displayed revision catches up
→ then selection RuntimeIds become interactive
```

Gameplay never waits for animation callbacks. Only input exposure waits for historical display catch-up.

The Wave 1C-C1 Draw→Selection bug fix is the first concrete implementation of this fence.

While an authoritative interactive selection is pending, normal continuous card play is forbidden.

## 8. Historical Hand behavior while Gameplay is ahead

The Native Hand remains a historical Presentation surface. Do not snap it directly to current Gameplay merely because fast play is allowed.

A rapid click uses the RuntimeId shown on that historical card widget. Gameplay revalidates that RuntimeId against the current Hand.

If still valid:

```text
accept request
→ locally mark that historical card as submitted/non-clickable
→ do not remove/reorder it from historical Hand early
→ committed CardPlayed Record later owns the visible removal
```

If no longer valid:

```text
reject request
→ keep Gameplay unchanged
→ allow Presentation to catch up
→ show concise rejection feedback if needed
```

This prevents duplicate rapid clicks from becoming duplicate card plays.

## 9. Targeted cards

Enemy/Self target cards must use current Gameplay revalidation, not stale ViewModel UObject bindings.

First implementation may preserve the current visible target-selection interaction but submit:

```text
CardRuntimeId
+
TargetPresentationId
```

Gameplay resolves both identities at request time.

Target preview remains historical/current-query UX and must not become authoritative.

## 10. Presentation backlog

No change to authoritative result ordering:

```text
Gameplay Card A Resolution
Gameplay Card B Resolution
Gameplay Card C Resolution
```

produces ordered Envelopes:

```text
A
B
C
```

Controller playback stays:

```text
A records completely
→ B records completely
→ C records completely
```

Animations may visually overlap internally only if a future Presentation-specific design explicitly supports that. Continuous **input** does not require concurrent Record reducers or out-of-order Envelope playback.

The existing bounded backlog/collapse policy remains Presentation-only.

## 11. Implementation slices

### CP-1 — RuntimeId Gameplay request facade

- resolve exact current Hand `CardRuntimeId` inside Gameplay;
- resolve optional target PresentationId inside Gameplay;
- reuse current validation/request path;
- no UI UObject authority;
- focused Automation for valid/stale/energy/target/busy cases.

### CP-2 — Native ordinary fast-play path

Replace the current automatic `SkipPresentation()` retry behavior for eligible ordinary card input:

```text
active Presentation + Gameplay idle
→ submit RuntimeId request directly
→ keep active Presentation running
```

Keep explicit Skip as a user/presentation control, not as the mechanism that makes rapid play possible.

### CP-3 — local submitted-card input staging

- immediately prevent duplicate clicks on a successfully submitted historical RuntimeId;
- do not remove the historical card before its CardPlayed Record;
- clear staging on matching CardPlayed playback, rejection/catch-up, battle reset and teardown.

### CP-4 — target-selection compatibility

- RuntimeId + PresentationId request route;
- rapid Enemy/Self card interaction;
- stale target/card rejection remains Gameplay-owned.

### CP-5 — interactive fence integration

Generalize the C1 fence contract where a future interactive effect requires it.

Do not force every Effect through a Presentation barrier. Only player-choice boundaries require one.

### CP-6 — validation and seal

Focused Automation plus one rapid-play Native PIE pass.

## 12. Automated Gates

Proposed focused prefix:

```text
SlayTheSpireDemo.UI.ContinuousCardPlay
```

Minimum deterministic coverage:

```text
[ ] RuntimeId resolves exact current Hand card
[ ] stale historical RuntimeId is rejected
[ ] normal request can be accepted while Presentation backlog exists and Gameplay is idle
[ ] Gameplay-busy request is still rejected
[ ] pending interactive selection blocks ordinary rapid play
[ ] two accepted rapid plays produce monotonically ordered Presentation ResolutionIds
[ ] Presentation backlog does not reorder card resolutions
[ ] duplicate submitted RuntimeId cannot play twice
[ ] target identity is revalidated by Gameplay
[ ] Presentation skip/failure does not affect committed Gameplay
```

## 13. Manual PIE Gate

Use Native HUD with simple cards, for example:

```text
Strike
Defend
Strike
```

Rapidly play them before prior visuals finish.

Expected:

```text
[ ] next card input is accepted without automatically skipping previous playback
[ ] Energy and Hand legality follow authoritative Gameplay
[ ] cards resolve in click/request order
[ ] Presentation later plays the same committed order
[ ] no duplicate card widget / ghost / tail / snap-back
[ ] invalid rapid click is rejected rather than speculatively queued
```

Then use a Warcry-style interactive card:

```text
Draw
→ choose Hand card
```

Expected:

```text
[ ] ordinary rapid play stops at the pending choice
[ ] Draw becomes visible before choice is interactive
[ ] newly drawn card is selectable
[ ] after resolving choice, ordinary rapid play becomes eligible again when Gameplay is stable
```

## 14. Non-goals

This plan does not authorize:

```text
- two concurrent BattleActionQueue resolutions
- Gameplay waiting for animation completion
- animation timers controlling legality
- speculative queued card commands while Gameplay is busy
- out-of-order Presentation reducers
- UI-owned CardInstance pointers
- Legacy HUD changes
- GAS migration
```

## 15. Recommended order

```text
finish and validate C1 interactive-selection fence
↓
seal C1
↓
start Continuous Card Play as a separate UI/Gameplay amendment
↓
CP-1 RuntimeId request facade
↓
CP-2 Native non-skip rapid input
↓
CP-3/4 staging + target compatibility
↓
CP-5 interactive fence integration
↓
focused Automation + one PIE
↓
seal amendment
```
