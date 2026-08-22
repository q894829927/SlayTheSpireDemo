# Phase 6UI-A2D5 Source Review

Date: **2026-08-22**

Status: **A2D5-1 VALIDATED / A2D5-2 VALIDATED / A2D5-3 VALIDATED / READY FOR A2D5-4**.

Validated baseline after A2D5-3:

```text
UE5.8 Editor Development build   PASS
A2D1                            PASS 3/3
A2D2                            PASS 4/4
A2D3                            PASS 4/4
A2D4                            PASS 6/6
A2D5 focused                   PASS 2/2
Phase6R aggregate               PASS 96/96
Shipping exclusion              PASS
```

A2D5 remains a combined acceptance slice. No new runtime Presentation capability has been introduced by A2D5-1 through A2D5-3.

---

## 1. A2D5 acceptance infrastructure

The shared acceptance path uses:

```text
real BattleManager / Gameplay producer
→ committed Presentation Resolution Envelope
→ BattlePresentationController
→ visible record-by-record playback
→ PlaybackToken completion
→ WorkingSnapshot progression
→ exact FinalSnapshot reconciliation
```

Supporting files:

```text
Source/SlayTheSpireDemoTests/Private/Phase6UIA2D5TestTypes.h
Source/SlayTheSpireDemoTests/Private/Phase6UIA2D5TestTypes.cpp
Source/SlayTheSpireDemoTests/Private/Phase6UIA2D5TestSupport.h
Source/SlayTheSpireDemoTests/Private/Phase6UIA2D5TestSupport.cpp
Source/SlayTheSpireDemoTests/Private/Phase6UIA2D5PlaybackAssertions.cpp
```

The fixture owns a real Game `UWorld`, Player, Enemy, `ABattleManager`, `UBattleHUDViewModel`, playback widget and `UBattlePresentationController`.

Each captured Envelope stores its own historical baseline:

```text
FCapturedEnvelope
├── Baseline
└── Envelope
```

Independent Resolutions are never flattened into one synthetic replay history.

`AssertReducerOwnedStateMatchesFinalSnapshot()` replays one Envelope through the production Controller reducers and compares only reducer-owned fields against that Envelope's `FinalSnapshot`.

`AssertCapturedEnvelopeOrder()` verifies monotonic `(BattleId, ResolutionId)` publication order without sorting.

`AssertControllerPlaybackMatchesCapturedHistory()` flattens captured Records strictly in producer order and verifies the real widget history and playback tokens by:

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

Empty Envelopes contribute zero expected playback calls and remain legal.

---

## 2. A2D5-1 — acceptance fixture + consistency helper

A2D5-1 established:

```text
shared real-battle fixture
Envelope capture helpers
per-Envelope production reducer consistency helper
real Controller playback capture
```

Validation issues found during UE5.8 build and fixed:

```text
c4ed21daeaabaf6eab02ecf829242e3697269c64
fix(tests): isolate automation cpp translation units

6baf3b62dc5ae8a752cafeaa8fc769334dca509e
fix(ui-a2d5): include deck runtime in acceptance support
```

Validated result:

```text
A2D5-1 VALIDATED
Phase6R 94/94 PASS
Shipping exclusion PASS
```

---

## 3. A2D5-2 — StatusLifecycle

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

The hardened test validates:

```text
real pending stale Action identity
exact old-instance isolation
stale mutation → Gameplay NoOp
no StatusChanged Record for no-op
empty stale Envelope publication remains optional
Weak#B remains unchanged
recreated instance receives newer RuntimeSequence
frozen descriptions/icon metadata remain historical
RuntimeSequence array ordering is exercised with AnchorStatus + Weak
real Controller record/token order matches producer history
per-Envelope reducer consistency
```

Validated result:

```text
A2D5 focused    1/1 PASS
Phase6R         95/95 PASS
Shipping        PASS
```

A2D5-2 is sealed.

---

## 4. A2D5-3 — CardStatusIntegration

Top-level Automation test:

```text
Source/SlayTheSpireDemoTests/Private/Phase6UIA2D5CardStatusIntegrationTest.cpp
SlayTheSpireDemo.Phase6UIA2D5.CardStatusIntegration
```

The scenario authors one real one-cost Enemy-target Attack card using only existing runtime effects:

```text
Effects[0] = DamageCardEffect(7)
Effects[1] = ApplyStatusCardEffect(Weak, +2)
Effects[2] = ApplyStatusCardEffect(Vulnerable, +1)
DefaultDestination = Discard
```

Two runtime copies of the same immutable card definition are drawn into the opening Hand.

### 4.1 First card Resolution

Required producer order:

```text
CardPlayed
→ Damage
→ StatusChanged(Weak Applied)
→ StatusChanged(Vulnerable Applied)
→ CardZoneChanged(PlayArea → DiscardPile)
```

Validated ownership and values:

```text
CardPlayed RuntimeId == exact first runtime card
Energy 3 → 2
CostPaid = 1
EnergyChanged count = 0
Damage HP 100 → 93
Weak 0 → 2 Applied
Vulnerable 0 → 1 Applied
exact authored status order preserved
only one CardZoneChanged
no Hand → PlayArea zone Record
finish-card zone change is final Record
```

This confirms the locked A2C ownership boundary: card-play energy cost lives only in `CardPlayed.EnergyBefore/EnergyAfter/CostPaid` and does not emit a duplicate `EnergyChanged`.

### 4.2 Record-by-record historical Controller timing

After Gameplay has fully committed the first card, authoritative state is already:

```text
Energy = 2
Hand = 1
Enemy HP = 93
Discard = 1
Weak = 2
Vulnerable = 1
```

while the display remains at the pre-Envelope baseline before the first playback token completes:

```text
Energy = 3
Hand = 2
Enemy HP = 100
Discard = 0
Enemy Statuses = 0
```

Token completion then advances only the state owned by the completed Record:

```text
CardPlayed completion
    → Energy / Hand advance

Damage completion
    → Enemy HP advances

Weak completion
    → Weak row appears

Vulnerable completion
    → Vulnerable row appears

CardZoneChanged completion
    → Discard advances
    → exact FinalSnapshot reconciliation
```

The combined stream therefore remains historical and is not collapsed prematurely to `FinalSnapshot`.

### 4.3 Second card Resolution — merge existing concrete status identities

The second runtime card produces:

```text
CardPlayed
→ Damage
→ StatusChanged(Weak Increased)
→ StatusChanged(Vulnerable Increased)
→ CardZoneChanged(PlayArea → DiscardPile)
```

Validated values:

```text
Energy 2 → 1
EnergyChanged count = 0
Enemy HP 93 → 86
Weak 2 → 4 Increased
Vulnerable 1 → 2 Increased
```

Concrete identity must be preserved across reapplication:

```text
Weak object after second play == Weak object after first play
Weak RuntimeSequence unchanged

Vulnerable object after second play == Vulnerable object after first play
Vulnerable RuntimeSequence unchanged
```

The displayed enemy status row count remains exactly two, rejecting duplicate UI entries keyed only by application events.

### 4.4 Multi-Envelope and reducer consistency

Both card Resolutions are independently validated:

```text
Envelope[0].Baseline
→ production reducer replay of Envelope[0].Records
→ reducer-owned result == Envelope[0].FinalSnapshot

Envelope[1].Baseline
→ production reducer replay of Envelope[1].Records
→ reducer-owned result == Envelope[1].FinalSnapshot
```

The test also verifies:

```text
strict ResolutionId order
no sorting before comparison
no missing playback Record
no extra playback Record
no cross-Envelope Controller reordering
record/token identity match
```

Static review found no high-confidence cross-slice production defect and required no A2D1-A2D4 production runtime changes.

---

## 5. A2D5-3 validation

The updated UE5.8 focused workflow and full Phase6R workflow were reported successful after A2D5-3 implementation.

Validated result:

```text
UE5.8 Editor Development build   PASS
A2D5 focused                    PASS 2/2
Phase6R aggregate               PASS 96/96
Shipping exclusion              PASS
```

The aggregate discovery count is therefore legitimately **96/96 PASS**.

Final status after A2D5-3:

```text
A2D5-1 VALIDATED
A2D5-2 STATUS LIFECYCLE VALIDATED
A2D5-3 CARD STATUS INTEGRATION VALIDATED
A2D5 FOCUSED 2/2 PASS
PHASE6R 96/96 PASS
SHIPPING EXCLUSION PASS
READY FOR A2D5-4 TURN CYCLE ORDERING
```

---

## 6. Next slice — A2D5-4 TurnCycleOrdering

The next top-level acceptance scenario is:

```text
SlayTheSpireDemo.Phase6UIA2D5.TurnCycleOrdering
```

It must validate actual Gameplay output rather than introducing a `TurnEnded` Presentation Record.

Required visible facts remain:

```text
EnergyChanged(current → 0)                  [only if changed]
→ Hand → Discard                            [actual cards]
→ StatusChanged(TurnEndDecay)               [actual decay]
→ Enemy BlockChanged(clear)                 [if nonzero]
→ Enemy Damage
→ Player EnergyChanged(current → MaxEnergy) [only if changed]
→ Player BlockChanged(clear)                [if actual block clears]
→ DeckShuffled                              [if real shuffle]
→ DrawPile → Hand                           [actual draws]
```

The fixture should force nonzero Energy, nonempty Hand, decaying Status, clearable Block, insufficient DrawPile and nonempty Discard so these contracts are exercised rather than vacuously asserted.
