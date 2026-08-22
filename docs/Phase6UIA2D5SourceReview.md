# Phase 6UI-A2D5 Source Review

Date: **2026-08-22**

Status: **A2D5-1 VALIDATED / A2D5-2 VALIDATED / A2D5-3 IMPLEMENTED / UE5.8 VALIDATION PENDING**.

Validated baseline entering A2D5-3:

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

A2D5 remains a combined acceptance slice. A2D5-3 adds one top-level integration scenario and does not add a new runtime Presentation capability.

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

A2D5 also uses:

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

## 10. A2D5-2 — StatusLifecycle

Top-level Automation test:

```text
Source/SlayTheSpireDemoTests/Private/Phase6UIA2D5StatusLifecycleTest.cpp
SlayTheSpireDemo.Phase6UIA2D5.StatusLifecycle
```

Validated lifecycle:

```text
Weak#A  0 → 2   Applied
Weak#A  2 → 3   Increased
Weak#A  3 → 2   Reduced
Weak#A  2 → 1   TurnEndDecay
Weak#A  1 → 0   Removed
Weak#B  0 → 2   Applied
```

The hardened scenario also validates:

```text
real pending stale Action identity
no-op → no StatusChanged Record
optional empty Envelope semantics
RuntimeSequence ordering with AnchorStatus + Weak
real Controller record/token order
per-Envelope production reducer consistency
```

Validated result:

```text
UE5.8 Editor Development build   PASS
A2D5 focused                    PASS 1/1
Phase6R aggregate               PASS 95/95
Shipping exclusion              PASS
```

---

## 11. A2D5-2 final status

```text
A2D5-1 VALIDATED
A2D5-2 STATUS LIFECYCLE VALIDATED
A2D5 FOCUSED 1/1 PASS
PHASE6R 95/95 PASS
SHIPPING EXCLUSION PASS
```

---

## 12. A2D5-3 — CardStatusIntegration implementation

Added top-level Automation test:

```text
Source/SlayTheSpireDemoTests/Private/Phase6UIA2D5CardStatusIntegrationTest.cpp

SlayTheSpireDemo.Phase6UIA2D5.CardStatusIntegration
```

The fixture authors one real one-cost Enemy-target Attack card with the existing effect system:

```text
Effects[0] = DamageCardEffect(7)
Effects[1] = ApplyStatusCardEffect(Weak, +2)
Effects[2] = ApplyStatusCardEffect(Vulnerable, +1)
DefaultDestination = Discard
```

Two runtime copies of the same immutable definition are drawn into the opening Hand. No test-only Gameplay action path is used.

### 12.1 First card Resolution

The first accepted card play must produce exactly:

```text
CardPlayed
→ Damage
→ StatusChanged(Weak Applied)
→ StatusChanged(Vulnerable Applied)
→ CardZoneChanged(PlayArea → DiscardPile)
```

The test requires:

```text
CardPlayed RuntimeId == exact first runtime card
Energy 3 → 2
CostPaid = 1
no EnergyChanged Record for the card cost
Damage HP 100 → 93
Weak 0 → 2 Applied
Vulnerable 0 → 1 Applied
exact effect-authored status order is preserved
only one CardZoneChanged exists
finish-card zone change is last
```

This explicitly rechecks the A2C ownership boundary in a combined A2B + A2C + A2D Resolution: the card fee is represented only by `CardPlayed.EnergyBefore/EnergyAfter/CostPaid`.

### 12.2 Record-by-record Controller timing

After Gameplay has fully committed the first card, the real Controller is still waiting on `CardPlayed`.

Before the first token completes, the test requires the displayed ViewModel to remain at the historical baseline:

```text
Energy = 3
Hand = 2
Enemy HP = 100
Discard = 0
Enemy Statuses = 0
```

while authoritative Gameplay is already:

```text
Energy = 2
Hand = 1
Enemy HP = 93
Discard = 1
Weak = 2
Vulnerable = 1
```

The test then completes real playback tokens one record at a time and checks visible progression:

```text
CardPlayed completion
    → Energy 2 / Hand 1
    → HP still 100 / Discard still 0

Damage completion
    → Enemy HP 93
    → statuses still absent

Weak completion
    → Weak row appears at amount 2

Vulnerable completion
    → Vulnerable row appears at amount 1
    → Discard still 0

CardZoneChanged completion
    → Discard becomes 1
    → Envelope reconciles to exact FinalSnapshot
```

This proves the combined stream is not collapsed into FinalSnapshot before visible historical playback completes.

### 12.3 Second card Resolution — merge existing status identities

The remaining runtime card is played in a second real Resolution.

Expected history remains:

```text
CardPlayed
→ Damage
→ StatusChanged(Weak Increased)
→ StatusChanged(Vulnerable Increased)
→ CardZoneChanged(PlayArea → DiscardPile)
```

The test requires:

```text
Energy 2 → 1
no duplicate EnergyChanged
Enemy HP 93 → 86
Weak 2 → 4 Increased
Vulnerable 1 → 2 Increased
```

Most importantly, both reapplications must preserve concrete runtime identity:

```text
Weak object after second play == Weak object after first play
Weak RuntimeSequence unchanged

Vulnerable object after second play == Vulnerable object after first play
Vulnerable RuntimeSequence unchanged
```

The displayed enemy status row count must remain exactly two after the second playback drains. This rejects a duplicate UI-entry implementation that keys only on application events instead of concrete runtime status identity.

### 12.4 Producer order, Controller order and reducer consistency

Both Envelopes are validated independently with production reducers:

```text
Envelope[0].Baseline
→ replay Envelope[0].Records
→ reducer-owned result == Envelope[0].FinalSnapshot

Envelope[1].Baseline
→ replay Envelope[1].Records
→ reducer-owned result == Envelope[1].FinalSnapshot
```

The scenario also calls:

```text
AssertCapturedEnvelopeOrder()
AssertControllerPlaybackMatchesCapturedHistory()
```

No sorting occurs before comparison. This validates:

```text
real effect/action order
strict ResolutionId order
no missing visible Record
no extra visible Record
no cross-Envelope playback reordering
record/token identity match
```

---

## 13. A2D5-3 static review result

Static review found no high-confidence cross-slice production defect.

The implementation uses existing runtime contracts only:

```text
UCardData::Effects authored order
PlayCardAction follow-up order
DamageAction
ApplyStatusAction
FinishCardPlayAction
BattlePresentationController
A2D5 acceptance fixture/helpers
```

No new Presentation Record type, Gameplay status/card mechanic, Controller protocol, reducer ownership rule, or Recorder behavior was added.

No A2D1-A2D4 production runtime code was modified for A2D5-3.

---

## 14. Focused / Phase6R discovery count after A2D5-3

Focused gate now expects:

```text
SlayTheSpireDemo.Phase6UIA2D5
Expected discovered tests = 2
```

Phase6R now expects A2D5 count `2`.

Therefore before execution:

```text
validated aggregate entering A2D5-3 = 95/95 PASS
new expected discovered total          = 96
```

Do **not** call this `96/96 PASS` until UE5.8 actually runs the updated focused and aggregate workflows successfully.

Current status:

```text
A2D5-1 VALIDATED
A2D5-2 STATUS LIFECYCLE VALIDATED
A2D5-3 CARD STATUS INTEGRATION IMPLEMENTED
STATIC REVIEW COMPLETE
UE5.8 FOCUSED / UPDATED PHASE6R VALIDATION PENDING
EXPECTED DISCOVERED TOTAL = 96
```

Required validation before A2D5-3 can be sealed:

```text
UE5.8 Editor Development build PASS
A2D5 focused 2/2 PASS
updated Phase6R aggregate 96/96 PASS
Shipping exclusion PASS
```
