# Phase 6UI-A2D4 — Formal Terminal Presentation Implementation Plan

Date: **2026-08-22**

Status: **IMPLEMENTED / STATIC REVIEW COMPLETE / UE5.8 VALIDATION PENDING**.

Baseline before A2D-4:

```text
A2D-1  PASS 3/3
A2D-2  PASS 4/4
A2D-3  PASS 4/4
Phase6R PASS 88/88
UE5.8 Editor build PASS through Phase6R prerequisite
```

A2D-4 starts from the validated A2D-1 through A2D-3 status path. It does not reopen those slices.

---

## 1. Goal

A2D-4 formalizes the Presentation-layer battle ending for:

```text
Victory
Defeat
ResolutionFault
```

Required visible ordering:

```text
last ordinary combat presentation
→ terminal Record preflight
→ terminal visible playback
→ terminal playback completes
→ WorkingPresentationSnapshot becomes terminal
→ ViewModel enters Terminal
→ Envelope completes
→ immutable FinalSnapshot exact reconciliation
```

The UI must not enter Victory / Defeat / ResolutionFaulted merely because the immutable FinalSnapshot is already terminal while earlier or terminal presentation is still playing.

---

## 2. Locked decisions

### 2.1 Terminal state changes after playback completion

```text
terminal Record received
→ validate on a temporary WorkingSnapshot copy
→ if valid, offer to Blueprint/native playback
→ real WorkingSnapshot remains non-terminal during animation
→ callback / false fallback / timeout
→ apply terminal reducer to real WorkingSnapshot
→ ViewModel enters Terminal
```

### 2.2 ResolutionFault uses the same formal playback path

`ResolutionFault` is a real terminal Presentation fact. It uses the same Record, PlaybackToken, Blueprint acceptance, fallback, timeout, completion and FinalSnapshot reconciliation path as Victory and Defeat.

`PresentationUnavailable` remains separate and must never manufacture Gameplay `ResolutionFaulted`.

### 2.3 Terminal Record remains final

The Recorder invariant remains:

```text
Victory / Defeat / ResolutionFault
= at most one terminal Record
= final Record of the Resolution
```

Illegal examples:

```text
Victory → Damage
Defeat → StatusChanged
Victory → Defeat
Victory → ResolutionFault
ResolutionFault → any Record
```

### 2.4 Invalid terminal history never reaches Blueprint

A terminal Record is preflighted before a PlaybackToken is handed to Blueprint.

```text
preflight succeeds
→ discard temporary result
→ visible playback
→ completion
→ real WorkingSnapshot terminal commit

preflight fails
→ no Blueprint playback
→ collapse directly to Envelope.FinalSnapshot
→ Gameplay unchanged
```

---

## 3. Typed payloads

A2D-4 adds:

```cpp
USTRUCT(BlueprintType)
struct FTerminalPresentationPayload
{
    GENERATED_BODY()

    FName WinnerPresentationId;
    FName DefeatedPresentationId;
};

USTRUCT(BlueprintType)
struct FResolutionFaultPresentationPayload
{
    GENERATED_BODY()

    FString Reason;
    int32 ExecutedActionCount;
    FName LastActionName;
};
```

`FPresentationRecord` now owns:

```text
Terminal
ResolutionFault
```

The legacy root fields were removed:

```text
FaultReason
FaultExecutedActionCount
FaultLastActionName
```

There is only one frozen fault truth.

---

## 4. Producers

### Victory

`ABattleManager::CheckBattleResult()`:

```text
Enemy dead
→ Gameplay commits BattleState=Victory
→ resolve Player PresentationId as Winner
→ resolve Enemy PresentationId as Defeated
→ require IDs non-empty and distinct
→ append Victory terminal Record
```

If Gameplay has committed but terminal identity cannot be frozen, the active unpublished Presentation Resolution is invalidated. Gameplay is not rolled back.

### Defeat

```text
Player dead
→ Gameplay commits BattleState=Defeat
→ resolve Enemy PresentationId as Winner
→ resolve Player PresentationId as Defeated
→ append Defeat terminal Record
```

### ResolutionFault

`HandleActionQueueResolutionFaulted()` remains Gameplay/framework authority. `AppendPresentationResolutionFault()` freezes the normalized framework diagnostic into the typed `ResolutionFault` payload and appends the terminal Record.

---

## 5. Terminal envelope validation

Before playback, Controller validates global terminal shape:

```text
zero or one terminal Record
terminal Record, when present, must be final
non-terminal FinalSnapshot must not contain terminal Record
terminal FinalSnapshot must contain matching terminal Record
Victory FinalSnapshot    ↔ Victory Record
Defeat FinalSnapshot     ↔ Defeat Record
ResolutionFaulted Final  ↔ ResolutionFault Record
```

A mismatch collapses directly to FinalSnapshot.

---

## 6. Terminal reducer

The reducer may change only:

```text
BattleState
Outcome
bCanEndTurn
```

It must not repair HP, Block, Status, Energy, card zones or other missing history.

### Victory

Require:

```text
Winner == Working.Player == Final.Player
Defeated == Working.Enemy == Final.Enemy
Working.Enemy.bDead == true
Final.Enemy.bDead == true
Final.BattleState == Victory
Final.Outcome == Victory
```

### Defeat

Require:

```text
Winner == Working.Enemy == Final.Enemy
Defeated == Working.Player == Final.Player
Working.Player.bDead == true
Final.Player.bDead == true
Final.BattleState == Defeat
Final.Outcome == Defeat
```

### ResolutionFault

Require:

```text
Reason non-empty
ExecutedActionCount >= 0
Final.BattleState == ResolutionFaulted
Final.Outcome == ResolutionFaulted
```

No participant death is required for ResolutionFault.

---

## 7. Playback timing

Normal terminal playback:

```text
ordinary Records finish
→ terminal Record begins
→ ViewModel remains Resolving / InputLocked
→ callback or fallback or timeout
→ terminal reducer commits
→ ViewModel becomes Terminal
→ FinalSnapshot reconciliation
```

Skip is intentionally different:

```text
Skip
→ invalidate playback generation
→ apply newest retained FinalSnapshot immediately
→ enter Terminal immediately when FinalSnapshot is terminal
```

Stale/duplicate callbacks remain blocked by the existing PlaybackToken + LocalPlaybackGeneration checks.

---

## 8. Existing A2A test compatibility

Pre-A2D4 A2A Controller tests used synthetic `ResolutionFault` records merely as generic async playback probes. That is no longer semantically valid because ResolutionFault is now a formal terminal record.

Those generic probes were migrated to no-state-change `BlockChanged` records. Tests that genuinely exercise framework faults continue to use real typed `ResolutionFault` records.

---

## 9. Focused Automation

A2D-4 currently defines six top-level tests:

```text
SlayTheSpireDemo.Phase6UIA2D4.Producer.VictoryPayload
SlayTheSpireDemo.Phase6UIA2D4.Producer.DefeatPayload
SlayTheSpireDemo.Phase6UIA2D4.Producer.ResolutionFaultPayload
SlayTheSpireDemo.Phase6UIA2D4.Playback.TerminalCompletionTiming
SlayTheSpireDemo.Phase6UIA2D4.Playback.TerminalTimeout
SlayTheSpireDemo.Phase6UIA2D4.Safety.PreflightFallbackSkip
```

Coverage proves or is intended to prove:

```text
Victory typed participant identity
Defeat typed participant identity
ResolutionFault typed diagnostics
terminal remains non-terminal in display during animation
terminal commits after callback
invalid terminal is rejected before Blueprint
Blueprint false fallback
terminal timeout
skip to terminal FinalSnapshot
stale callback after skip ignored
Gameplay remains unchanged by Presentation recovery
```

---

## 10. CI gates

Dedicated workflow:

```text
.github/workflows/ue-phase6uia2d4-tests.yml
Prefix: SlayTheSpireDemo.Phase6UIA2D4
ExpectedCount: 6
```

Phase6R expected counts are now:

```text
Phase5          13
Phase6A         23
Phase6B         12
Phase6C          5
Phase6UIA2A      8
Phase6UIA2B      8
Phase6UIA2C      8
Phase6UIA2D1     3
Phase6UIA2D2     4
Phase6UIA2D3     4
Phase6UIA2D4     6
------------------
Total           94
```

The Phase6R workflow still includes the Shipping test-module exclusion gate.

---

## 11. Implementation commits

Core implementation:

```text
a6dfe349  feat(ui-a2d4): add typed terminal presentation payloads
f5a73145  feat(ui-a2d4): freeze resolution fault into typed payload
019bbbf3  feat(ui-a2d4): add terminal preflight and reducer timing
be6a3f4b  feat(ui-a2d4): freeze victory and defeat participant identities
7807889f  refactor(ui-a2d4): restore BattleManager source shape
```

Tests / compatibility / CI:

```text
fd380325  test(ui-a2d4): cover terminal producers and playback timing
6cf03f77  test(ui-a2d4): migrate A2A playback probes off terminal fault records
b4a30f97  test(ui-a2d4): use nonterminal probes for A2A controller lifecycle
1e098ae4  ci(ui-a2d4): add focused terminal presentation gate
5e9950d6  ci(ui-a2d4): add terminal gate to Phase6R
60bc3b64  test(ui-a2d4): prove terminal timeout completion
e4892ef1  ci(ui-a2d4): include terminal timeout coverage
524bf2be  ci(ui-a2d4): raise regression gate for timeout test
```

---

## 12. Validation requirement

Source implementation and static review are complete, but this document does **not** claim runtime validation yet.

Required authoritative gates:

```text
UE5.8 Editor Development build
SlayTheSpireDemo.Phase6UIA2D4  6/6
SlayTheSpireDemo.Phase6UIA2D1  3/3
SlayTheSpireDemo.Phase6UIA2D2  4/4
SlayTheSpireDemo.Phase6UIA2D3  4/4
Phase6R aggregate             94/94
Shipping exclusion             PASS
```

Only after these pass may A2D-4 be marked:

```text
VALIDATED / READY FOR A2D-5
```
