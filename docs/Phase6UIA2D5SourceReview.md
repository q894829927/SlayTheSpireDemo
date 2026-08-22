# Phase 6UI-A2D5 Source Review

Date: **2026-08-22**

Status: **A2D5-1 VALIDATED / READY FOR A2D5-2**.

Validated baseline after A2D5-1:

```text
UE5.8 Editor Development build   PASS
A2D1                            PASS 3/3
A2D2                            PASS 4/4
A2D3                            PASS 4/4
A2D4                            PASS 6/6
Phase6R aggregate               PASS 94/94
Shipping exclusion              PASS
```

A2D5-1 adds acceptance-test infrastructure only. It does not add a new Presentation Record, Gameplay mechanic, Controller lifecycle rule, or production reducer behavior.

---

## 1. Acceptance playback capture

Files:

```text
Source/SlayTheSpireDemoTests/Private/Phase6UIA2D5TestTypes.h
Source/SlayTheSpireDemoTests/Private/Phase6UIA2D5TestTypes.cpp
```

`UPhase6UIA2D5PlaybackWidget` records every visible `FPresentationRecord` and its corresponding `FPresentationPlaybackToken` while preserving the existing async accept/decline contract.

This lets later A2D5 scenarios inspect the real Controller playback order instead of relying only on offline Record arrays.

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

The fixture starts a real battle with committed Presentation enabled, initializes the real Controller/ViewModel path, and captures published Envelopes only after the BattleStart baseline has stabilized.

Each Envelope is stored with its own historical playback baseline:

```text
FCapturedEnvelope
├── Baseline
└── Envelope
```

After capture, that Envelope's immutable `FinalSnapshot` becomes the baseline for the next captured Envelope. Independent Resolutions are therefore never flattened into one synthetic history.

`ResetAcceptanceCapture()` succeeds only when the Controller is caught up:

```text
not waiting for completion
AND backlog == 0
```

This prevents tests from rebasing against a Gameplay snapshot that is newer than the displayed Controller state.

---

## 3. Real Controller playback helpers

The fixture exposes:

```text
CompleteCurrentPlayback()
DrainPlayback()
LastCapturedEnvelope()
FindCapturedEnvelope(ResolutionId)
```

Later acceptance scenarios can therefore exercise:

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

It temporarily installs the supplied Baseline/Envelope into an isolated Controller harness and invokes the existing production `ApplyRecordToWorkingSnapshot()` reducer for every Record in order.

The test module does not maintain a duplicate Damage/Block/Card/Energy/Zone/Shuffle/Status/Terminal reducer implementation.

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

Gameplay terminal paths may set:

```text
Victory          → Energy = 0
Defeat           → Energy = 0
ResolutionFault  → Energy = 0
```

while terminal reducers own only:

```text
BattleState
Outcome
bCanEndTurn
```

Therefore terminal Envelopes may legitimately require FinalSnapshot reconciliation for exact Energy. The A2D5 consistency helper does not incorrectly require reduced terminal Energy to equal `FinalSnapshot.Energy`.

Non-terminal Envelopes still require exact Energy equality.

---

## 7. Multi-Envelope structure

`AssertCapturedEnvelopeOrder()` checks:

```text
BattleId continuity
ResolutionId > 0
strictly increasing ResolutionId
next Baseline revision == previous FinalSnapshot revision
```

Captured Envelopes are never sorted before comparison. Later TurnCycle and other multi-Resolution scenarios must additionally verify real Controller playback order from the capture widget.

---

## 8. Validation issues found and fixed

The first UE5.8 regression build exposed a Unity-build-only test translation-unit collision: `Phase5RegressionTests.cpp` had a file-level `using namespace Phase5Regression;`, and Unity compilation made its `FFixture` collide with `Phase6UIA2AHardeningTest::FFixture`.

The test module was therefore made non-Unity only:

```text
c4ed21daeaabaf6eab02ecf829242e3697269c64
fix(tests): isolate automation cpp translation units
```

This affects only the Editor Automation test module and does not alter runtime behavior.

The next UE5.8 build exposed one incomplete-type compile error in A2D5 support: `IsValid(UDeckRuntime*)` required the full `UDeckRuntime` definition. The exact include was added:

```text
6baf3b62dc5ae8a752cafeaa8fc769334dca509e
fix(ui-a2d5): include deck runtime in acceptance support
```

No production Presentation contract change was required.

---

## 9. Final A2D5-1 validation

After the two compile fixes above, the full Phase6R workflow was reported successful.

Validated result:

```text
UE5.8 Editor Development build   PASS
Phase6R aggregate               PASS 94/94
Shipping exclusion              PASS
```

A2D5-1 adds no top-level Automation tests, so the aggregate discovery count remains **94** at this stage. The planned A2D5 scenario tests have not yet raised the expected total.

No high-confidence A2D1-A2D4 production defect was found during A2D5-1.

Final status:

```text
A2D5-1 VALIDATED
ACCEPTANCE FIXTURE READY
PER-ENVELOPE CONSISTENCY HELPER READY
REAL CONTROLLER PLAYBACK CAPTURE READY
READY FOR A2D5-2 STATUS LIFECYCLE
```
