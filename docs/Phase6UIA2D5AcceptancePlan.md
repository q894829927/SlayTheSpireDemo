# Phase 6UI-A2D5 — Combined C++ Acceptance Plan

Date: **2026-08-22**

Status: **PLAN LOCKED / READY TO IMPLEMENT**.

Validated incoming baseline:

```text
UE5.8 Editor Development build   PASS
A2D1                            PASS 3/3
A2D2                            PASS 4/4
A2D3                            PASS 4/4
A2D4                            PASS 6/6
Phase6R aggregate               PASS 94/94
Shipping exclusion              PASS
```

A2D-5 starts from the sealed A2D-1 through A2D-4 implementation. It is a combined acceptance slice, not a new Presentation feature slice.

---

## 1. Goal

A2D-5 proves that the already-implemented committed Presentation pipeline works correctly when A2B, A2C and A2D are exercised together across real Gameplay actions, multiple Records, multiple Envelopes and the real Controller playback path.

A2D-5 answers this question:

```text
Do the independently validated pieces still preserve one coherent historical truth
when they are composed into real battle flows?
```

The primary integration chain under test is:

```text
Gameplay commit
→ committed Presentation Record
→ immutable Resolution Envelope
→ BattlePresentationController
→ record-by-record playback
→ WorkingPresentationSnapshot reducer
→ BattleHUDViewModel
→ exact FinalSnapshot reconciliation
```

A2D-5 must detect cross-slice contract defects that focused A2B/A2C/A2D1-A2D4 tests may not expose independently.

---

## 2. Scope boundary

A2D-5 owns only combined C++ / Automation acceptance.

It may add:

```text
focused integration tests
shared test fixtures/helpers
per-Envelope reducer-owned consistency assertions
A2D5 focused workflow
updated Phase6R expected discovered count
validation / source-review documentation
```

It does **not** add new runtime Presentation capability merely to satisfy a test.

Out of scope:

```text
new Presentation Record types
new Gameplay Status mechanics
TurnEnded Presentation Record
new Recorder architecture
new Controller architecture
new PlaybackToken protocol
new mutable prediction / preview system
Blueprint visual implementation
UMG animation/art polish
PIE end-to-end visual smoke
reward/map/progression flow
unrelated API cleanup
```

A2D1-A2D4 are not proactively refactored. They may be changed only when A2D-5 proves a real cross-slice contract defect.

---

## 3. Existing contracts that A2D-5 must preserve

### 3.1 Card cost is represented only by `CardPlayed`

For a card play, energy spend history is already part of:

```text
CardPlayed.EnergyBefore
CardPlayed.EnergyAfter
CardPlayed.CostPaid
```

A2D-5 must **not** expect or introduce a duplicate `EnergyChanged` for the same card cost.

Therefore a lethal attack-card flow is expected to look like:

```text
CardPlayed
→ Damage
→ other real follow-up Records, if any
→ CardZoneChanged(PlayArea → Destination)
→ Victory
```

not:

```text
CardPlayed
→ EnergyChanged(card cost)   [invalid duplicate history]
→ ...
```

Independent `EnergyChanged` remains reserved for non-card-play energy mutations such as turn-end clear and turn-start restore, and only when the numeric value actually changes.

### 3.2 `TurnEnded` is Gameplay flow, not a Presentation Record

There is no `TurnEnded` Presentation Record in the current taxonomy.

A2D-5 validates the visible committed facts caused by the turn boundary, not an invented Record for the Gameplay Event.

### 3.3 No-op means no Record

Examples:

```text
0 Block → ClearBlock
Energy already equals requested value
no Discard pile available to shuffle
stale exact-instance Status mutation
MAX_int32 Status increase that changes nothing
```

Expected:

```text
no committed mutation
→ no Presentation Record
```

Tests must never require a Record merely because a code path executed.

### 3.4 Presentation failure is not Gameplay ResolutionFault

```text
real Queue / Gameplay framework failure
→ ResolutionFaulted
→ ResolutionFault terminal Record

Presentation freeze / append / reducer / preflight failure
→ PresentationUnavailable or FinalSnapshot collapse
→ Gameplay remains unchanged
→ no fake ResolutionFaulted
```

---

## 4. Planned top-level A2D-5 scenarios

The current plan defines **six** top-level integration scenarios.

This is not a fixed quota. If implementation reveals a seventh independent acceptance gap, add the seventh test and raise expected discovery counts accordingly.

Planned prefixes:

```text
SlayTheSpireDemo.Phase6UIA2D5.StatusLifecycle
SlayTheSpireDemo.Phase6UIA2D5.CardStatusIntegration
SlayTheSpireDemo.Phase6UIA2D5.TurnCycleOrdering
SlayTheSpireDemo.Phase6UIA2D5.Terminal.Victory
SlayTheSpireDemo.Phase6UIA2D5.Terminal.Defeat
SlayTheSpireDemo.Phase6UIA2D5.Terminal.ResolutionFault
```

Do not collapse these into one giant monolithic test.

---

## 5. Scenario 1 — StatusLifecycle

Exercise one concrete runtime status through a complete lifecycle:

```text
Create Weak#A      0 → 2   Applied
Merge Weak#A       2 → 3   Increased
Reduce Weak#A      3 → 2   Reduced
TurnEndDecay #A    2 → 1   TurnEndDecay
Remove Weak#A      1 → 0   Removed
Recreate Weak#B    0 → 2   Applied
```

Required validation:

```text
StatusId remains definition identity
RuntimeSequence remains concrete runtime identity
Weak#B RuntimeSequence > Weak#A RuntimeSequence
AmountBefore / AmountAfter are exact
bCreated / bRemoved reflect actual structural mutation
Reason is exact for every transition
DescriptionBefore / DescriptionAfter match the committed transition
DisplayName and icon/atlas values are frozen from the effective runtime definition
WorkingSnapshot status order remains RuntimeSequence ascending
FinalSnapshot exact identity/order agrees with committed state
```

### 5.1 Stale exact-instance isolation

The stale-action check must come from real Action identity, not from reusing an expired Presentation writer across Resolutions.

Required shape:

```text
capture exact Weak#A runtime instance
→ queue/retain an Action that targets exact Weak#A
→ Weak#A is removed
→ same StatusId is recreated as Weak#B
→ old Action executes
→ exact-instance validation fails membership match
→ Gameplay mutation = NoOp
→ no StatusChanged Record
→ Weak#B remains untouched
```

The test proves:

```text
same StatusId != same runtime status instance
```

Concrete identity remains:

```text
TargetPresentationId
+ StatusId
+ RuntimeSequence
```

### 5.2 Reaction order

If multiple reacting statuses participate, the test must compare the Records in the actual Gameplay-produced order:

```text
Priority
→ RuntimeSequence
→ LocalTriggerIndex
```

The test must **not** sort the actual Records before comparing them, because doing so would hide an ordering defect.

---

## 6. Scenario 2 — CardStatusIntegration

Use a real card/effect composition that produces card, damage and status presentation in one accepted card-play Resolution.

Representative shape:

```text
CardPlayed
→ Damage
→ StatusChanged(Weak Applied)
→ StatusChanged(Vulnerable Applied)
→ CardZoneChanged(PlayArea → Destination)
```

A later real application of the same status should produce:

```text
StatusChanged(Increased)
```

against the same runtime instance rather than a duplicate UI entry.

Required validation:

```text
no duplicate card-cost EnergyChanged
CardPlayed energy fields equal the committed card spend
Damage history follows the card play
Status records preserve real effect/action order
repeated status application updates exact existing RuntimeSequence
no duplicate status row for one runtime identity
Finish-card CardZoneChanged occurs after the card effects that were queued before FinishCardPlayAction
Controller consumes the full Record stream in order
FinalSnapshot reconciliation is exact
```

This scenario is the primary cross-check of A2C + A2B + A2D composition.

---

## 7. Scenario 3 — TurnCycleOrdering

This test must use a deliberately non-trivial fixture so the intended mutation paths are actually exercised.

Required setup should include, where supported by the fixture:

```text
Player Energy > 0
Player Hand non-empty
at least one decaying Status
clearable Block on the relevant turn-start combatant
DrawPile insufficient for requested draw
DiscardPile non-empty so a real shuffle/retry draw occurs
```

Expected visible facts are conditional on real commits.

Representative ordering across the real turn flow:

```text
EndTurn Resolution:
    EnergyChanged(current → 0)            [only when changed]
    Hand → Discard CardZoneChanged        [for real cards]
    StatusChanged(TurnEndDecay)           [when reaction Action commits]

Enemy-turn progression:
    BlockChanged(TurnStartClear)           [only when Block was non-zero]
    Damage                                [when committed]

Player-turn-start progression:
    EnergyChanged(current → MaxEnergy)     [only when changed]
    BlockChanged(TurnStartClear)           [only when committed]
    DeckShuffled                           [only when real shuffle occurs]
    DrawPile → Hand CardZoneChanged        [only successful draws]
```

`TurnEnded` itself remains a Gameplay Event / macro-flow boundary and is not expected as a Presentation Record.

Required validation:

```text
Gameplay-defined order is preserved exactly
Presentation never re-sorts committed Records for visual convenience
no-op paths produce no Records
multi-Envelope Controller consumption preserves BattleId/ResolutionId order
FinalSnapshot of each Envelope becomes the next historical baseline
```

---

## 8. Scenario 4 — Terminal.Victory

Use a real player card/action flow that causes lethal enemy damage.

Representative history:

```text
CardPlayed
→ lethal Damage
→ any remaining real card follow-up Records
→ CardZoneChanged(PlayArea → Destination)
→ Victory
```

Required validation:

```text
Victory is exactly one terminal Record
Victory is the final Record of its Envelope
Winner = Player PresentationId
Defeated = Enemy PresentationId
Damage reducer has already made Enemy.bDead == true before terminal preflight
real WorkingSnapshot remains Outcome=None while Victory playback is active
ViewModel remains Resolving / input locked during terminal playback
Blueprint completion token advances the terminal Record exactly once
only after completion does WorkingSnapshot become Victory
ViewModel then enters Terminal
Envelope completion reconciles exactly to FinalSnapshot
stale/duplicate terminal completion cannot mutate newer playback
```

No duplicate `EnergyChanged` is expected for the card cost.

---

## 9. Scenario 5 — Terminal.Defeat

Use a real enemy action that causes lethal player damage.

Required validation mirrors Victory:

```text
lethal Damage precedes Defeat
Defeat is final and unique
Winner = Enemy PresentationId
Defeated = Player PresentationId
WorkingSnapshot.Player.bDead is already true before terminal preflight
WorkingSnapshot remains non-terminal during Defeat playback
real Controller token completion commits Defeat once
ViewModel becomes Terminal only after completion
FinalSnapshot reconciliation is exact
```

The test should use the same generic Controller playback mechanism as Victory, not a special direct reducer-only path.

---

## 10. Scenario 6 — Terminal.ResolutionFault

This scenario must create a **real Gameplay / ActionQueue framework fault**.

It must not manufacture ResolutionFault by corrupting Presentation history.

Required shape:

```text
one or more valid committed ordinary Records
→ genuine queue/framework structural failure
→ authoritative Gameplay BattleState = ResolutionFaulted
→ ResolutionFault terminal Record
```

Required validation:

```text
ordinary committed history remains ordered and visible before the terminal fault
ResolutionFault is final and unique
fault payload freezes the authoritative framework diagnostics
WorkingSnapshot remains non-terminal while fault presentation is active
real Controller completion commits ResolutionFaulted once
ViewModel enters Terminal after completion
FinalSnapshot reconciliation is exact
```

Also retain a negative assertion elsewhere in A2D-5 coverage:

```text
Presentation append / preflight / reducer failure
!= Gameplay ResolutionFault
```

---

## 11. Per-Envelope reducer-owned consistency assertion

A2D-5 adds a reusable test helper concept, for example:

```text
AssertReducerOwnedStateMatchesFinalSnapshot(...)
```

The helper must operate **per Envelope**.

Correct model:

```text
PreEnvelopeBaseline(N)
→ apply Envelope(N).Records sequentially
→ reducer-owned reconstructed state
≈ Envelope(N).FinalSnapshot reducer-owned fields
```

Then:

```text
Envelope(N).FinalSnapshot
→ next historical baseline for Envelope(N+1)
```

Incorrect model:

```text
Battle-start snapshot
→ concatenate Records from several independent Envelopes
→ compare only with the last FinalSnapshot
```

A Resolution Envelope is the atomic committed-history boundary and must remain the unit of consistency checking.

### 11.1 Fields compared

Compare reducer-owned state:

```text
Player / Enemy:
    HP
    Block
    bDead

Energy

Hand:
    ordered concrete RuntimeId sequence

Piles:
    DrawCount
    DiscardCount
    ExhaustCount

Statuses:
    stable array order
    StatusId
    RuntimeSequence
    Amount
    DisplayName
    Description
    frozen icon / atlas metadata

Terminal:
    BattleState
    Outcome
    bCanEndTurn
```

The comparison may include additional reducer-owned fields already formally projected by the implemented reducers when needed for exact acceptance.

### 11.2 Fields deliberately excluded

Do not require reducer equality for non-owned or transient state such as:

```text
hover
selection
legal-target bindings
live UObject bindings
Widget state
PlaybackToken as UI state
derived preview values not explicitly represented by committed Records
```

Exact immutable FinalSnapshot remains authoritative at Envelope completion.

---

## 12. Helper does not replace the real Controller path

The consistency helper is an assertion tool, not an alternate acceptance architecture.

A2D-5 scenarios must still exercise the real integration path where relevant:

```text
BattleManager
→ OnPresentationResolutionReady
→ BattlePresentationController
→ PlayPresentationRecord
→ PlaybackToken
→ NotifyPresentationFinished / fallback / timeout as applicable
→ WorkingPresentationSnapshot
→ ViewModel
→ FinalSnapshot reconciliation
```

This is required to expose defects such as:

```text
stale token accepted
newer Envelope advanced too early
terminal state visible before terminal completion
duplicate callback advances twice
Resolution order skipped or reordered
FinalSnapshot reconciled at the wrong time
```

Offline reducer replay alone cannot prove those properties.

---

## 13. Multi-Envelope ordering

For tests spanning multiple formal Requests / Resolutions, capture and validate each Envelope separately.

The Controller must consume valid pending Envelopes in monotonic resolution order for the current battle:

```text
(BattleId, ResolutionId)
```

Required assertions should cover:

```text
same BattleId for one continuous battle
strictly increasing accepted ResolutionId
no later Envelope visibly starts before the active earlier Envelope completes unless an explicit skip/catch-up path owns that behavior
stale callbacks from an earlier generation cannot advance a later Envelope
```

A2D-5 must not flatten several Envelopes into one synthetic history stream.

---

## 14. No-op and conditional Record rules

A2D-5 test expectations must be mutation-driven rather than path-driven.

Examples:

```text
EnergyChanged
→ expect only if EnergyBefore != EnergyAfter

BlockChanged(TurnStartClear)
→ expect only if BlockBefore != 0 and clear committed

DeckShuffled
→ expect only if a real discard-to-draw shuffle committed

StatusChanged
→ expect only if exact status mutation committed

CardZoneChanged draw
→ expect only if the concrete draw committed
```

A test that forces a no-op then expects a Record is invalid.

---

## 15. Rules for modifying sealed A2D1-A2D4 code

A2D1-A2D4 are validated baseline slices.

If A2D-5 fails:

```text
first determine whether the fixture/assertion made an invalid assumption
↓
if the test is wrong, fix the test
↓
if a real cross-slice contract defect is proven, fix the narrow owning runtime boundary
↓
rerun focused affected tests + A2D5 + full Phase6R
```

Allowed reasons to modify sealed code:

```text
true cross-slice ordering bug
real reducer ownership inconsistency
real identity mismatch across valid producers/reducers
Controller multi-Envelope lifecycle defect
terminal timing defect exposed only by combined history
no-op incorrectly publishes a Record
```

Not sufficient reasons:

```text
style cleanup
API aesthetic preference
large opportunistic refactor
renaming for consistency only
unrelated compatibility cleanup
```

The compatibility overload on `UReduceStatusAction` is not an A2D-5 cleanup target by itself. If production/authoritative queued paths still rely on it in a way that violates A2D context requirements, that concrete usage may be fixed; otherwise cleanup remains outside acceptance scope.

---

## 16. Focused workflow and discovery counts

Create a dedicated focused workflow for:

```text
Prefix: SlayTheSpireDemo.Phase6UIA2D5
```

Current planned top-level count:

```text
Expected discovered A2D5 tests = 6
```

Current validated Phase6R baseline is:

```text
94/94 PASS
```

If exactly six A2D-5 tests are implemented, update the Phase6R workflow to:

```text
Expected discovered total = 100
```

This is an **expected discovery count**, not a pass claim.

Only after the full workflow actually executes successfully may validation documentation state:

```text
Phase6R aggregate = 100/100 PASS
```

If a seventh independent A2D-5 acceptance test is justified:

```text
Expected A2D5 = 7
Expected Phase6R discovered total = 101
```

Do not constrain coverage merely to preserve a round-number total.

---

## 17. Recommended implementation sequence

### A2D5-1 — Acceptance fixture + consistency helper

```text
shared real-battle fixture
Envelope capture helpers
per-Envelope reducer-owned comparator
Controller playback completion helpers
ordered Record assertion helpers
```

Gate:

```text
existing 94-test baseline still compiles
helper does not require new runtime Presentation feature
```

### A2D5-2 — StatusLifecycle

```text
full status lifecycle
stale exact-instance isolation
reaction ordering where practical
per-Envelope consistency
```

### A2D5-3 — CardStatusIntegration

```text
CardPlayed + Damage + StatusChanged + CardZoneChanged
no duplicate card-cost EnergyChanged
repeated status merge identity
real Controller playback
```

### A2D5-4 — TurnCycleOrdering

```text
non-trivial turn fixture
conditional no-op-aware expectations
multiple Envelopes
BattleId/ResolutionId ordering
per-Envelope consistency
```

### A2D5-5 — Terminal combined scenarios

```text
Victory
Defeat
real framework ResolutionFault
real Controller terminal timing
stale/duplicate token safety
```

### A2D5-6 — CI + regression + documentation

```text
A2D5 focused workflow
Phase6R expected-discovery update
UE5.8 Editor build
A2D5 focused run
full Phase6R aggregate
Shipping exclusion
validation docs
```

---

## 18. Regression requirements

Before implementation:

```text
A2D1 3/3 PASS
A2D2 4/4 PASS
A2D3 4/4 PASS
A2D4 6/6 PASS
Phase6R 94/94 PASS
Shipping exclusion PASS
```

After implementation, required validation is:

```text
UE5.8 Editor Development build PASS
A2D5 focused gate PASS with exact implemented discovery count
A2D1 3/3 PASS
A2D2 4/4 PASS
A2D3 4/4 PASS
A2D4 6/6 PASS
updated Phase6R aggregate PASS
Failed = 0
NotRun = 0
Shipping exclusion PASS
```

If the implementation adds exactly six top-level tests, the expected aggregate discovery count is 100.

---

## 19. Definition of Done

A2D-5 is complete only when all of the following are true:

```text
combined Status lifecycle is proven
stale exact-instance mutation cannot retarget recreated same-StatusId instance
CardPlayed/Damage/Status/CardZone composition is proven
card cost is not duplicated as EnergyChanged
turn-cycle visible facts preserve real Gameplay order
TurnEnded remains Gameplay-only and no new Record type is introduced
no-op mutations emit no Record
consistency is checked per Envelope
multi-Envelope Controller order is proven
real Controller playback path is exercised
Victory combined path is proven
Defeat combined path is proven
real framework ResolutionFault combined path is proven
Presentation failures do not manufacture Gameplay faults
reducer-owned state agrees with each FinalSnapshot at Envelope boundary
A2D5 focused Automation passes
updated full Phase6R aggregate passes
Shipping exclusion passes
```

After those gates pass, the C++ A2D phase may be marked:

```text
A2D C++ VALIDATED / READY FOR UNIFIED BLUEPRINT + PIE INTEGRATION
```

The next phase may then integrate the already-validated committed Presentation records across:

```text
A2B Damage / Block
+
A2C Card / Energy / Zone
+
A2D Status / Terminal
→ unified Blueprint rendering/playback
→ PIE end-to-end smoke
```
