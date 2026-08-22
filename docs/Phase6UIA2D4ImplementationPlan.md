# Phase 6UI-A2D4 — Formal Terminal Presentation Implementation Plan

Date: **2026-08-22**

Status: **PLAN LOCKED / READY TO IMPLEMENT**.

Baseline before A2D-4:

```text
A2D-1  PASS 3/3
A2D-2  PASS 4/4
A2D-3  PASS 4/4
Phase6R PASS 88/88
UE5.8 Editor build PASS through Phase6R prerequisite
```

A2D-4 starts from the already validated A2D-1 through A2D-3 status path. It must not reopen or redesign those slices unless a new terminal-specific defect proves that a shared invariant is wrong.

---

## 1. Goal

A2D-4 formalizes how the Presentation layer finishes a battle.

It owns exactly three terminal record semantics:

```text
Victory
Defeat
ResolutionFault
```

The intended player-visible ordering is:

```text
last ordinary combat presentation
→ terminal record playback
→ terminal playback completes
→ WorkingPresentationSnapshot becomes terminal
→ ViewModel enters terminal state
→ Envelope completes
→ immutable FinalSnapshot performs exact reconciliation
```

A2D-4 must prevent the UI from entering Victory / Defeat / ResolutionFaulted before the terminal animation has completed.

---

## 2. Locked decisions

These decisions are fixed before implementation.

### 2.1 Terminal state changes only after terminal playback completion

Receiving a terminal Record is not itself permission to switch the displayed state to terminal.

Required timing:

```text
terminal Record accepted
→ preflight validates history
→ Blueprint/native terminal playback starts
→ WorkingSnapshot remains non-terminal
→ ViewModel remains Resolving + InputLocked
→ callback / false fallback / timeout
→ reducer commits terminal state to WorkingSnapshot
→ ViewModel becomes Terminal
```

The terminal overlay/state must not appear before the terminal visual has completed.

### 2.2 ResolutionFault uses the same formal playback path

`ResolutionFault` is a real terminal Presentation fact, not a special Controller bypass.

It uses the same:

```text
Record
PlaybackToken
Blueprint acceptance
native false fallback
timeout
completion callback
WorkingSnapshot transition
FinalSnapshot reconciliation
```

as Victory and Defeat.

`PresentationUnavailable` remains separate. A Presentation failure must never fabricate a Gameplay `ResolutionFault`.

### 2.3 Terminal Record remains the final Record

The existing Recorder invariant remains authoritative:

```text
Victory / Defeat / ResolutionFault
= at most one terminal Record
= final Record in the Resolution
```

After a terminal Record, any further append invalidates the unpublished Presentation Resolution.

Illegal examples:

```text
Victory → Damage
Defeat → StatusChanged
Victory → Defeat
Victory → ResolutionFault
ResolutionFault → any Record
```

### 2.4 Terminal follows the A2D-3 preflight pattern

Before a terminal Record reaches Blueprint, the Controller validates it against:

```text
current WorkingPresentationSnapshot
+
Envelope.FinalSnapshot
```

Required flow:

```text
Terminal Record arrives
→ copy WorkingSnapshot
→ run terminal reducer/validator on the copy

invalid
→ do not call Blueprint PlayPresentationRecord
→ collapse directly to Envelope.FinalSnapshot

valid
→ discard temporary result
→ play terminal Record
→ real WorkingSnapshot remains non-terminal during animation
→ completion
→ apply terminal reducer to real WorkingSnapshot
```

This prevents a corrupt Victory / Defeat / ResolutionFault animation from being shown before recovery.

---

## 3. Data model changes

### 3.1 Shared Victory / Defeat payload

Add:

```cpp
USTRUCT(BlueprintType)
struct SLAYTHESPIREDEMO_API FTerminalPresentationPayload
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|Terminal")
    FName WinnerPresentationId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|Terminal")
    FName DefeatedPresentationId = NAME_None;
};
```

`Victory` and `Defeat` share this payload.

The Record Type determines the semantic meaning and UI text. Gameplay does not freeze localized strings such as "Victory" or "Defeat".

### 3.2 Typed ResolutionFault payload

Add:

```cpp
USTRUCT(BlueprintType)
struct SLAYTHESPIREDEMO_API FResolutionFaultPresentationPayload
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|Fault")
    FString Reason;

    UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|Fault")
    int32 ExecutedActionCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|Fault")
    FName LastActionName = NAME_None;
};
```

### 3.3 Remove duplicate fault truth

Replace the current `FPresentationRecord` root fault fields:

```text
FaultReason
FaultExecutedActionCount
FaultLastActionName
```

with:

```text
FTerminalPresentationPayload Terminal
FResolutionFaultPresentationPayload ResolutionFault
```

Do not keep both representations after migration.

The migration must update every producer, test, helper and assertion in the same implementation slice so the repository never intentionally settles on two fault payload authorities.

---

## 4. Producer responsibilities

### 4.1 Victory producer

Authoritative location:

```text
ABattleManager::CheckBattleResult
```

When Gameplay commits Victory:

```text
BattleState becomes Victory
→ build Victory terminal Record
→ WinnerPresentationId = Player
→ DefeatedPresentationId = Enemy
→ append terminal Record
→ no later Record may be appended
```

Producer failure while Gameplay is already committed follows existing Presentation fail-soft behavior:

```text
invalid/frozen participant identity or append failure
→ invalidate unpublished Presentation history
→ do not roll Gameplay back
→ do not create a second Gameplay fault
```

### 4.2 Defeat producer

Also owned by `ABattleManager::CheckBattleResult`.

Required identities:

```text
WinnerPresentationId = Enemy
DefeatedPresentationId = Player
```

The same fail-soft rules apply.

### 4.3 ResolutionFault producer

The queue/framework fault path freezes one typed `FResolutionFaultPresentationPayload` describing the already-authoritative framework failure.

The payload is diagnostic history only. It does not decide whether Gameplay is faulted.

The implementation must preserve the distinction:

```text
Gameplay/framework failure
→ ResolutionFault terminal Record

Presentation freeze/reducer/append failure
→ PresentationUnavailable or FinalSnapshot collapse
→ never synthesize ResolutionFault
```

---

## 5. Terminal semantic validator / reducer

A single Controller terminal path should own validation and transition rather than distributing special-case logic across playback branches.

Recommended shape:

```text
ApplyTerminalRecordToWorkingSnapshot(
    WorkingSnapshot,
    FinalSnapshot,
    Record
)
```

or equivalent helpers with one shared semantic entry point.

The function must be deterministic and must use only immutable Record + Snapshot values. It must not query mutable live Gameplay.

### 5.1 Victory validation

Require at minimum:

```text
Record.Type == Victory
WinnerPresentationId == WorkingSnapshot.Player.PresentationId
WinnerPresentationId == FinalSnapshot.Player.PresentationId
DefeatedPresentationId == WorkingSnapshot.Enemy.PresentationId
DefeatedPresentationId == FinalSnapshot.Enemy.PresentationId
WorkingSnapshot.Enemy.bDead == true
FinalSnapshot.Enemy.bDead == true
WorkingSnapshot is not already terminal
FinalSnapshot.BattleState == Victory
FinalSnapshot.Outcome == Victory
```

After validation, the reducer changes only terminal-owned state.

### 5.2 Defeat validation

Require at minimum:

```text
Record.Type == Defeat
WinnerPresentationId == WorkingSnapshot.Enemy.PresentationId
WinnerPresentationId == FinalSnapshot.Enemy.PresentationId
DefeatedPresentationId == WorkingSnapshot.Player.PresentationId
DefeatedPresentationId == FinalSnapshot.Player.PresentationId
WorkingSnapshot.Player.bDead == true
FinalSnapshot.Player.bDead == true
WorkingSnapshot is not already terminal
FinalSnapshot.BattleState == Defeat
FinalSnapshot.Outcome == Defeat
```

### 5.3 ResolutionFault validation

Require at minimum:

```text
Record.Type == ResolutionFault
WorkingSnapshot is not already terminal
FinalSnapshot.BattleState == ResolutionFaulted
FinalSnapshot.Outcome == ResolutionFaulted
ExecutedActionCount >= 0
```

`Reason` may be empty only if the existing authoritative queue/framework contract already permits an empty diagnostic reason. If current producer semantics guarantee a non-empty reason, A2D-4 should lock and test that stronger invariant instead of weakening it for Presentation convenience.

No dead participant is required for ResolutionFault.

### 5.4 Reducer ownership boundary

A successful terminal reducer may change only terminal-owned fields:

```text
BattleState
Outcome
bCanEndTurn
```

It must not repair:

```text
HP
Block
Status
Energy
Hand
Deck piles
Enemy intent
```

Those facts must already be represented by earlier Records or will be reconciled only by `Envelope.FinalSnapshot`.

Example:

```text
Victory Record
but WorkingSnapshot still shows Enemy alive
```

is invalid incremental history.

Behavior:

```text
no Victory playback
→ collapse to FinalSnapshot
```

Do not invent the missing Damage transition inside the terminal reducer.

---

## 6. Controller playback changes

### 6.1 Visible Record participation

Ensure all three terminal types participate in the generic visible path:

```text
Victory
Defeat
ResolutionFault
```

They use the existing `FPresentationPlaybackToken` ownership rules.

### 6.2 StartNextRecord preflight

Extend the A2D-3 preflight concept:

```text
if Record is terminal
    require valid WorkingPresentationSnapshot
    make temporary copy
    validate/apply terminal reducer on copy
    if invalid
        collapse before Blueprint
        return
```

Only a valid terminal Record receives a PlaybackToken / Blueprint call.

### 6.3 CompleteActiveRecord commit

For a terminal Record:

```text
callback / native false fallback / timeout
→ re-run terminal reducer on real WorkingSnapshot
→ ApplyPresentationSnapshot(WorkingSnapshot)
→ ViewModel enters Terminal
→ continue normal Envelope completion
→ exact FinalSnapshot reconciliation
```

The preflight copy must never become authoritative.

### 6.4 Skip

Skip remains direct catch-up:

```text
Skip
→ invalidate current token generation
→ apply newest retained Envelope.FinalSnapshot
→ terminal state may appear immediately
```

Skip intentionally does not wait for terminal animation completion because it explicitly abandons incremental playback.

### 6.5 Stale and duplicate callbacks

Existing token/generation rules remain authoritative.

A stale or duplicate terminal completion must not:

```text
reapply terminal state
advance a newer Record
finish a newer Resolution
modify a newer Battle
change input lock incorrectly
```

---

## 7. Expected source files

Primary files expected to change:

```text
Source/SlayTheSpireDemo/Presentation/PresentationTypes.h
Source/SlayTheSpireDemo/Presentation/BattlePresentationController.h
Source/SlayTheSpireDemo/Presentation/BattlePresentationController.cpp
Source/SlayTheSpireDemo/Presentation/BattlePresentationRecorder.cpp
Source/SlayTheSpireDemo/Battle/BattleManager.cpp
```

Additional queue/framework or test helper files may change if required by the existing ResolutionFault producer boundary.

Tests should be implemented in a dedicated A2D-4 test source rather than overloading A2D-3 tests unless a shared generic Controller test is the clearly correct ownership location.

---

## 8. Implementation slices

Implement A2D-4 in six small slices.

### A2D4-1 — Typed payload migration

```text
add FTerminalPresentationPayload
add FResolutionFaultPresentationPayload
add payload fields to FPresentationRecord
migrate fault producer/readers
remove legacy root fault fields
compile
```

Gate:

```text
no legacy FaultReason/FaultExecutedActionCount/FaultLastActionName references remain
existing non-terminal tests still compile
```

### A2D4-2 — Victory / Defeat producer

```text
freeze participant IDs from BattleManager authority
append correct Victory / Defeat terminal payload
preserve terminal-last Recorder invariant
producer failure remains Presentation-only degradation
```

Gate:

```text
lethal player path produces exactly one final Victory Record
lethal enemy path produces exactly one final Defeat Record
```

### A2D4-3 — ResolutionFault producer

```text
freeze typed diagnostic payload
append exactly one terminal fault Record
preserve already-committed ordinary history before the fault
no Presentation failure can manufacture this Record
```

Gate:

```text
framework fault produces typed terminal Record
fault Record is final
FinalSnapshot is ResolutionFaulted
```

### A2D4-4 — Terminal preflight + reducer

```text
single semantic terminal validator/reducer
Victory checks
Defeat checks
ResolutionFault checks
preflight on WorkingSnapshot copy before Blueprint
mismatch collapses directly to FinalSnapshot
```

Gate:

```text
malformed terminal Record never reaches Blueprint
valid terminal Record does
real WorkingSnapshot remains non-terminal before playback completion
```

### A2D4-5 — Playback completion timing

```text
async callback
Blueprint false fallback
timeout
skip
stale callback
duplicate callback
```

Gate:

```text
callback/fallback/timeout commit terminal state exactly once
skip catches up immediately
stale/duplicate token cannot mutate current playback
```

### A2D4-6 — Automation + regression integration

```text
dedicated A2D4 prefix
focused expected-count workflow
Phase6R aggregate count update
Editor build
focused A2D4 tests
full Phase6R
```

A2D-4 is not complete until the aggregate regression is green.

---

## 9. Automation plan

Create focused top-level scenarios rather than one monolithic test.

Recommended minimum top-level coverage:

```text
1. Victory producer + terminal timing
2. Defeat producer + terminal timing
3. ResolutionFault typed producer + playback
4. malformed terminal preflight collapse
5. terminal fallback / timeout safety
6. terminal skip / stale-token / duplicate-callback safety
```

The exact number may change if existing generic Controller tests can safely carry part of this coverage. If top-level tests are added, both the dedicated A2D4 workflow and Phase6R expected total must be updated in the same change.

### 9.1 Victory scenario

Example:

```text
Damage leaves Enemy dead
→ Victory Record is last
→ Controller reaches Victory Record
→ preflight succeeds
→ Blueprint accepts async playback
→ ViewModel still Resolving / non-terminal
→ callback
→ WorkingSnapshot becomes Victory
→ ViewModel becomes Terminal
→ FinalSnapshot reconciliation exact
```

### 9.2 Defeat scenario

Mirror Victory with Player as defeated participant.

### 9.3 ResolutionFault scenario

Example:

```text
one or more ordinary committed Records
→ framework fault
→ typed ResolutionFault is final Record
→ previous Records play normally
→ fault Record preflight succeeds
→ fault animation plays
→ WorkingSnapshot becomes ResolutionFaulted only after completion
```

### 9.4 Invalid history scenarios

At minimum cover:

```text
Victory with wrong WinnerPresentationId
Victory while WorkingSnapshot Enemy is alive
Defeat with wrong defeated participant
terminal Type inconsistent with FinalSnapshot
ResolutionFault with non-fault FinalSnapshot
record after terminal / duplicate terminal rejected by Recorder
```

Expected Controller behavior for semantic mismatches:

```text
Blueprint terminal PlayCallCount does not increase
Controller collapses to immutable FinalSnapshot
Gameplay remains untouched
```

### 9.5 Generic completion safety

Prove terminal participation in:

```text
async callback
false fallback
timeout
skip
stale callback
duplicate callback
```

Avoid unnecessary repetition if one generic token test already proves mechanics shared across all three types, but at least one test must prove each terminal Type participates in the formal path.

---

## 10. Regression gates

Before A2D-4 implementation:

```text
Phase6R baseline = 88/88
```

During implementation, run the smallest relevant focused prefix after each slice.

Final A2D-4 acceptance requires:

```text
UE5.8 Editor Development build PASS
A2D4 focused Automation PASS
A2D1 3/3 still PASS
A2D2 4/4 still PASS
A2D3 4/4 still PASS
updated Phase6R aggregate PASS with Failed=0 and NotRun=0
```

Do not mark A2D-4 validated from static review alone.

---

## 11. Explicitly out of scope

A2D-4 does not implement:

```text
new Victory/Defeat UMG art or animations
new ResolutionFault visual design
reward screen
map/progression transition
post-battle rewards
save/load battle results
PIE end-to-end visual smoke
A2D-5 combined acceptance
removal of unrelated compatibility APIs
```

Blueprint may later choose how the three terminal records look. A2D-4 only provides the correct immutable facts, timing, token ownership and state transition.

---

## 12. Risks and controls

### Risk 1 — UI switches terminal too early

Control:

```text
preflight on copy
real WorkingSnapshot commit only after terminal playback completion
```

### Risk 2 — corrupt terminal history visibly plays

Control:

```text
semantic preflight before Blueprint
invalid history collapses directly to FinalSnapshot
```

### Risk 3 — duplicate fault representation diverges

Control:

```text
typed fault payload migration is atomic
legacy root fields removed
```

### Risk 4 — terminal reducer hides missing ordinary history

Control:

```text
terminal reducer owns only BattleState / Outcome / bCanEndTurn
HP/death/status/card mismatches cause collapse, not repair
```

### Risk 5 — Presentation failure becomes fake Gameplay fault

Control:

```text
ResolutionFault producer stays only on authoritative framework fault path
Presentation failures remain fail-soft degradation
```

### Risk 6 — terminal callback mutates a newer playback

Control:

```text
existing BattleId / ResolutionId / PresentationSequence / LocalPlaybackGeneration token checks remain mandatory
```

---

## 13. Suggested commit sequence

Keep each semantic step reviewable:

```text
feat(ui-a2d4): add typed terminal presentation payloads
feat(ui-a2d4): record victory and defeat terminal facts
feat(ui-a2d4): migrate resolution fault presentation payload
feat(ui-a2d4): validate terminal history before playback
feat(ui-a2d4): commit terminal state after playback completion
test(ui-a2d4): cover terminal presentation lifecycle
docs(ui-a2d4): record validation results
```

Do not mix Blueprint visual integration into these commits.

---

## 14. Definition of Done

A2D-4 is complete only when all of the following are true:

```text
[ ] Victory and Defeat use typed FTerminalPresentationPayload
[ ] ResolutionFault uses typed FResolutionFaultPresentationPayload
[ ] legacy root fault fields are removed
[ ] exactly one terminal Record may finish a valid Resolution
[ ] no Record can follow a terminal Record
[ ] all terminal Records are preflighted before Blueprint playback
[ ] invalid terminal history never reaches Blueprint
[ ] valid terminal animation plays while WorkingSnapshot remains non-terminal
[ ] callback/fallback/timeout commits terminal state exactly once
[ ] ResolutionFault uses the same formal visible playback path
[ ] skip catches up directly to FinalSnapshot
[ ] stale/duplicate callbacks remain harmless
[ ] terminal reducer changes only terminal-owned state
[ ] FinalSnapshot remains exact reconciliation authority
[ ] UE5.8 Editor build passes
[ ] focused A2D4 Automation passes
[ ] updated Phase6R aggregate passes with zero failures/not-run tests
```

Once these gates pass, mark A2D-4:

```text
VALIDATED / READY FOR A2D-5
```
