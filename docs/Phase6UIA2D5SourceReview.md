# Phase 6UI-A2D5 Source Review

Date: **2026-08-22**

Status: **A2D5-1 VALIDATED / A2D5-2 VALIDATED / READY FOR A2D5-3**.

Validated baseline after A2D5-2:

```text
UE5.8 Editor Development build   PASS
A2D1                            PASS 3/3
A2D2                            PASS 4/4
A2D3                            PASS 4/4
A2D4                            PASS 6/6
A2D5 focused                   PASS 1/1
Phase6R aggregate               PASS 95/95
Shipping exclusion              PASS
```

A2D5 remains a combined acceptance slice. A2D5-2 adds one top-level integration scenario and does not add a new runtime Presentation capability.

---

## 1. Acceptance playback capture

Files:

```text
Source/SlayTheSpireDemoTests/Private/Phase6UIA2D5TestTypes.h
Source/SlayTheSpireDemoTests/Private/Phase6UIA2D5TestTypes.cpp
```

`UPhase6UIA2D5PlaybackWidget` records every visible `FPresentationRecord` and its corresponding `FPresentationPlaybackToken` while preserving the existing async accept/decline contract.

This lets A2D5 scenarios inspect the real Controller playback order instead of relying only on offline Record arrays.

---

## 2. Real-battle acceptance fixture

Files:

```text
Source/SlayTheSpireDemoTests/Private/Phase6UIA2D5TestSupport.h
Source/SlayTheSpireDemoTests/Private/Phase6UIA2D5TestSupport.cpp
```

`FAcceptanceFixture` owns:

```text
UWorld
Player / Enemy
ABattleManager
UBattleHUDViewModel
UPhase6UIA2D5PlaybackWidget
UBattlePresentationController
captured Resolution Envelopes
```

Each Envelope is stored with its own historical playback baseline:

```text
FCapturedEnvelope
├── Baseline
└── Envelope
```

After capture, that Envelope's immutable `FinalSnapshot` becomes the baseline for the next captured Envelope. Independent Resolutions are never flattened into one synthetic history.

`ResetAcceptanceCapture()` succeeds only when the Controller is caught up:

```text
not waiting for completion
AND backlog == 0
```

---

## 3. Real Controller playback helpers

The fixture exposes:

```text
CompleteCurrentPlayback()
DrainPlayback()
LastCapturedEnvelope()
FindCapturedEnvelope(ResolutionId)
```

Acceptance scenarios therefore exercise:

```text
real Envelope delivery
→ Controller visible playback
→ PlaybackToken
→ NotifyPresentationFinished
→ WorkingSnapshot advance
→ next Record / next Envelope
```

The offline consistency helper remains supplemental and does not replace this integration path.

---

## 4. Production reducer replay test seam

A test-only method exists on `UBattlePresentationController`:

```cpp
#if WITH_DEV_AUTOMATION_TESTS
bool ReduceEnvelopeForTesting(
    const FPresentationStateSnapshot& Baseline,
    const FPresentationResolutionEnvelope& Envelope,
    FPresentationStateSnapshot& OutReducedSnapshot
);
#endif
```

Implementation:

```text
Source/SlayTheSpireDemo/Presentation/BattlePresentationControllerTesting.cpp
```

It invokes the existing production `ApplyRecordToWorkingSnapshot()` reducer for every Record in order. The test module does not maintain a duplicate reducer implementation.

This remains restricted to `WITH_DEV_AUTOMATION_TESTS` and does not alter Shipping runtime behavior.

---

## 5. Per-Envelope consistency helper

`AssertReducerOwnedStateMatchesFinalSnapshot()` performs:

```text
captured Baseline
→ production reducer replay over this Envelope only
→ ReducedSnapshot
→ compare reducer-owned state with Envelope.FinalSnapshot
```

Compared state includes:

```text
Player / Enemy:
    HP
    Block
    bDead
    Status array count/order
    StatusId
    RuntimeSequence
    Amount
    DisplayName
    Description
    frozen atlas/icon metadata

Hand:
    ordered concrete RuntimeId sequence

Piles:
    DrawCount
    DiscardCount
    ExhaustCount

Energy:
    compared for non-terminal Envelopes

Terminal:
    BattleState
    Outcome
    bCanEndTurn
```

Selection, hover, LegalTargets, Widget bindings and other non-reducer-owned state are deliberately excluded.

---

## 6. Terminal Energy ownership boundary

A2C intentionally does not emit `EnergyChanged` for terminal/fault normalization.

Gameplay terminal paths may set Energy to zero while terminal reducers own only:

```text
BattleState
Outcome
bCanEndTurn
```

Therefore terminal Envelopes may legitimately require FinalSnapshot reconciliation for exact Energy. Non-terminal Envelopes still require exact Energy equality.

---

## 7. Multi-Envelope structure

`AssertCapturedEnvelopeOrder()` checks:

```text
BattleId continuity
ResolutionId > 0
strictly increasing ResolutionId
next Baseline revision == previous FinalSnapshot revision
```

Captured Envelopes are never sorted before comparison.

A2D5-2 review additionally introduced:

```text
AssertControllerPlaybackMatchesCapturedHistory()
```

Implementation:

```text
Source/SlayTheSpireDemoTests/Private/Phase6UIA2D5PlaybackAssertions.cpp
```

It flattens captured Records strictly in producer/capture order without sorting and requires the real playback widget to match every record and token by:

```text
Type
BattleId
ResolutionId
PresentationSequence
PlaybackToken.BattleId
PlaybackToken.ResolutionId
PlaybackToken.PresentationSequence
positive LocalPlaybackGeneration
```

Empty Envelopes contribute zero expected playback calls and therefore remain legal.

---

## 8. A2D5-1 validation issues found and fixed

The first UE5.8 regression build exposed a Unity-build-only test translation-unit collision. The Editor Automation test module was made non-Unity only:

```text
c4ed21daeaabaf6eab02ecf829242e3697269c64
fix(tests): isolate automation cpp translation units
```

The next build exposed an incomplete `UDeckRuntime` type in A2D5 support. The exact include was added:

```text
6baf3b62dc5ae8a752cafeaa8fc769334dca509e
fix(ui-a2d5): include deck runtime in acceptance support
```

After those fixes, the full Phase6R workflow passed 94/94 and Shipping exclusion passed. A2D5-1 is sealed.

---

## 9. A2D5-1 final status

```text
A2D5-1 VALIDATED
ACCEPTANCE FIXTURE READY
PER-ENVELOPE CONSISTENCY HELPER READY
REAL CONTROLLER PLAYBACK CAPTURE READY
```

---

## 10. A2D5-2 — StatusLifecycle implementation

Top-level Automation test:

```text
Source/SlayTheSpireDemoTests/Private/Phase6UIA2D5StatusLifecycleTest.cpp

SlayTheSpireDemo.Phase6UIA2D5.StatusLifecycle
```

The test runs one concrete `Weak` status through the complete committed lifecycle:

```text
Weak#A  0 → 2   Applied
Weak#A  2 → 3   Increased
Weak#A  3 → 2   Reduced
Weak#A  2 → 1   TurnEndDecay
Weak#A  1 → 0   Removed
Weak#B  0 → 2   Applied
```

Required historical payload assertions include:

```text
StatusId
RuntimeSequence
AmountBefore / AmountAfter
bCreated / bRemoved
Reason
DisplayName
DescriptionBefore / DescriptionAfter
bUseAtlasIcon
UVOffset / UVScale
TrimOffset / TrimScale
```

The first five commits retain Weak#A's exact RuntimeSequence. Recreated Weak#B has a strictly newer RuntimeSequence.

### 10.1 Real Controller timing

Applied / Increased / Reduced / Removed paths run through the real async Controller widget.

The test explicitly checks that Gameplay commits first while the displayed ViewModel remains at its old historical state until the current PlaybackToken completes.

Examples:

```text
Gameplay Weak amount 2 → 3
while playback active:
    ViewModel Weak amount remains 2
completion token:
    ViewModel Weak amount becomes 3
```

and:

```text
Gameplay removes Weak#A
while Removed playback active:
    ViewModel still shows Weak#A amount 1
completion token:
    ViewModel removes the row
```

### 10.2 Real TurnEndDecay

Turn-end decay is produced through the real `RequestEndPlayerTurn()` macro flow, not by manually fabricating a `StatusChanged` Record.

The fixture intentionally uses zero enemy damage and no cards for this scenario so unrelated gameplay remains minimal while the real EndTurn/EnemyTurn/PlayerTurnStart progression still executes.

The test accepts additional non-status Records/Envelopes produced by that macro flow and validates every captured Envelope independently.

### 10.3 Review fix — stale exact-instance isolation uses a real pending batch

The hardened version uses a real pending Action batch:

```text
Weak#A exists
→ create independent holding UBattleActionQueue
→ create stale UReduceStatusAction with holding queue as Outer
→ AddToBack(stale Action) while Weak#A is still authoritative
→ verify holding queue PendingCount == 1
→ Weak#A removed through normal battle queue
→ Weak#B recreated through normal battle queue
→ verify stale Action is still pending
→ begin a new formal System Presentation Resolution
→ assign that current writer to the already-pending stale Action
→ start holding queue
→ exact-instance membership check returns Gameplay NoOp
```

This proves the stale mutation comes from a real queued Action identity while still avoiding reuse of an expired Presentation writer.

### 10.4 Review fix — empty stale Resolution publication is optional

The stale contract is:

```text
Gameplay mutation = NoOp
Weak#B remains amount 2
Weak#B concrete identity remains unchanged
no additional Weak StatusChanged Record
no visible Controller playback call for the stale mutation
```

The test does not require a new Envelope to exist.

If the current producer publishes one or more Envelopes during the stale finalization edge, every newly observed Envelope must contain zero Records. A producer that suppresses an empty Envelope remains valid.

### 10.5 Review fix — status ordering is exercised, not vacuous

Before the formal Weak lifecycle capture starts, the test creates a persistent non-decaying `AnchorStatus` and drains its real Controller playback.

It then calls `ResetAcceptanceCapture()` so the formal A2D5-2 baseline contains:

```text
AnchorStatus#A
```

Weak#A and later Weak#B are created with newer RuntimeSequence values, producing real two-row arrays:

```text
AnchorStatus#A
Weak#A
```

and later:

```text
AnchorStatus#A
Weak#B
```

This makes both the real ViewModel checks and the per-Envelope `CompareStatusArrays()` RuntimeSequence ordering assertions execute on arrays with more than one status.

### 10.6 Review fix — Controller consumption order is checked directly

At the end of the scenario, every captured Envelope is checked separately for reducer consistency and the real widget history is matched against the captured record stream without sorting.

This proves:

```text
no missing playback Record
no extra playback Record
no cross-Envelope reordering
no record/token identity mismatch
```

and supplements `AssertCapturedEnvelopeOrder()`, which only validates publication/capture order.

---

## 11. A2D5-2 validation

The UE5.8 focused workflow and updated Phase6R workflow were reported successful after the hardened review fixes.

Validated result:

```text
UE5.8 Editor Development build   PASS
A2D5 focused                    PASS 1/1
Phase6R aggregate               PASS 95/95
Shipping exclusion              PASS
```

The aggregate discovery count is now legitimately **95/95 PASS** because exactly one A2D5 top-level scenario has been implemented and validated.

---

## 12. A2D5-2 final status

Review findings addressed and validated:

```text
[validated] stale NoOp no longer requires an Envelope
[validated] stale Action is truly pending while Weak#A still exists
[validated] Controller record/token order matches captured history
[validated] RuntimeSequence ordering is exercised with AnchorStatus + Weak
```

No new Presentation Record type, Gameplay Status mechanic, Recorder rule, Controller lifecycle protocol or runtime reducer behavior was added.

No A2D1-A2D4 runtime code was modified for these fixes.

Final status:

```text
A2D5-1 VALIDATED
A2D5-2 STATUS LIFECYCLE VALIDATED
A2D5 FOCUSED 1/1 PASS
PHASE6R 95/95 PASS
SHIPPING EXCLUSION PASS
READY FOR A2D5-3 CARD STATUS INTEGRATION
```
