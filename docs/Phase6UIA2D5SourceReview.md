# Phase 6UI-A2D5 Source Review

Date: **2026-08-22**

Status: **A2D5-1 IMPLEMENTED / STATIC REVIEW COMPLETE / UE5.8 VALIDATION PENDING**.

Incoming validated baseline:

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

## 1. Added acceptance playback capture type

Files:

```text
Source/SlayTheSpireDemoTests/Private/Phase6UIA2D5TestTypes.h
Source/SlayTheSpireDemoTests/Private/Phase6UIA2D5TestTypes.cpp
```

`UPhase6UIA2D5PlaybackWidget` records every visible `FPresentationRecord` and its corresponding `FPresentationPlaybackToken` while preserving the existing async accept/decline contract.

This allows later A2D5 scenarios to inspect the real Controller playback order rather than relying only on offline Record arrays.

---

## 2. Added real-battle acceptance fixture

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

The fixture starts a real battle with committed Presentation enabled, initializes the real Controller/ViewModel path, and captures published Envelopes after the BattleStart baseline has stabilized.

Each captured Envelope is stored together with its own playback baseline:

```text
FCapturedEnvelope
├── Baseline
└── Envelope
```

After an Envelope is captured, its immutable `FinalSnapshot` becomes the capture baseline for the next published Envelope. A2D5 therefore does not concatenate independent Resolutions into one synthetic history.

`ResetAcceptanceCapture()` is allowed only when the Controller is caught up:

```text
not waiting for completion
AND backlog == 0
```

This prevents a test from resetting to a latest frozen Gameplay baseline while the displayed Controller state is still behind.

---

## 3. Real Controller playback helpers

The fixture exposes:

```text
CompleteCurrentPlayback()
DrainPlayback()
LastCapturedEnvelope()
FindCapturedEnvelope(ResolutionId)
```

Later scenarios can therefore drive:

```text
real Envelope delivery
→ Controller visible playback
→ real PlaybackToken
→ NotifyPresentationFinished
→ next Record / next Envelope
```

The offline consistency helper does not replace this path.

---

## 4. Production reducer replay test seam

A test-only method was added to `UBattlePresentationController`:

```cpp
#if WITH_DEV_AUTOMATION_TESTS
bool ReduceEnvelopeForTesting(
    const FPresentationStateSnapshot& Baseline,
    const FPresentationResolutionEnvelope& Envelope,
    FPresentationStateSnapshot& OutReducedSnapshot
);
#endif
```

Implementation lives in:

```text
Source/SlayTheSpireDemo/Presentation/BattlePresentationControllerTesting.cpp
```

The method temporarily installs the supplied Baseline/Envelope into an isolated Controller harness and invokes the existing production `ApplyRecordToWorkingSnapshot()` reducer for every Record in sequence.

It does not duplicate Damage/Block/Card/Energy/Zone/Shuffle/Status/Terminal reducer logic in the test module.

The temporary Controller state is restored before the method returns.

This is a test seam only; no Shipping runtime behavior changes.

---

## 5. Per-Envelope consistency helper

`AssertReducerOwnedStateMatchesFinalSnapshot()` performs:

```text
captured Baseline
→ production reducer replay over this Envelope only
→ ReducedSnapshot
→ compare reducer-owned fields with this Envelope.FinalSnapshot
```

Current compared fields are:

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
    bUseAtlasIcon
    UVOffset / UVScale
    TrimOffset / TrimScale

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
    compared when a terminal Record exists
```

---

## 6. Terminal Energy ownership clarification discovered during A2D5-1

The A2C contract intentionally does not emit `EnergyChanged` for terminal/fault normalization.

Current Gameplay terminal paths set:

```text
Victory          → Energy = 0
Defeat           → Energy = 0
ResolutionFault  → Energy = 0
```

but terminal reducers are explicitly allowed to mutate only:

```text
BattleState
Outcome
bCanEndTurn
```

Therefore a terminal Envelope may legally produce:

```text
Baseline / ordinary Records reduce Energy to X
terminal Gameplay normalization makes FinalSnapshot.Energy = 0
terminal reducer does not synthesize Energy = 0
FinalSnapshot reconciliation applies the exact terminal Energy
```

For that reason the A2D5 consistency helper does **not** require reduced Energy to equal FinalSnapshot Energy when the Envelope contains Victory, Defeat, or ResolutionFault.

This is not a production defect. It is the existing ownership boundary between committed Records and exact FinalSnapshot reconciliation.

Non-terminal Envelopes still require reduced Energy to equal FinalSnapshot Energy.

---

## 7. Multi-Envelope structural helper

`AssertCapturedEnvelopeOrder()` checks:

```text
BattleId continuity
ResolutionId > 0
strictly increasing ResolutionId
next captured Baseline revision == previous Envelope.FinalSnapshot revision
```

It deliberately does not sort captured Envelopes before comparison.

Later TurnCycle and multi-Resolution scenarios must combine this structural assertion with the real Controller playback-order assertions from the capture widget.

---

## 8. Top-level test count

A2D5-1 adds no new top-level Automation test yet.

Therefore the currently validated aggregate baseline remains:

```text
Phase6R aggregate = 94/94 PASS
```

The planned A2D5 six-scenario discovery count is not activated until those scenario tests are implemented.

Do not claim an expected total of 100 from A2D5-1 alone.

---

## 9. Static review result

No high-confidence production contract defect was found during A2D5-1 source review.

No A2D1-A2D4 runtime behavior was redesigned.

The new Runtime-facing change is restricted to `WITH_DEV_AUTOMATION_TESTS` and delegates to the already validated production reducer implementation.

Authoritative validation still requires a real UE5.8 Editor build after these source additions.

Current status remains:

```text
A2D5-1 IMPLEMENTED
STATIC REVIEW COMPLETE
UE5.8 BUILD / REGRESSION VALIDATION PENDING
```

Recommended next validation before building A2D5 scenarios:

```text
UE5.8 Editor Development build PASS
existing Phase6R aggregate 94/94 PASS
Shipping exclusion PASS
```
