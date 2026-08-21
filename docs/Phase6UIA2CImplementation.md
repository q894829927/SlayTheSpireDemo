# Phase 6UI-A2C — Card / Energy / Zone Committed Presentation

Status: **SOURCE IMPLEMENTED / UE5.8 VALIDATION PENDING**.

UI-A2C extends the established UI-A2A transport/lifecycle and UI-A2B committed-record model. It does not redesign Resolution, Envelope, RecordWriter, Controller backlog, PlaybackToken, PresentationUnavailable, latest-only input binding, or fail-soft no-partial-history behavior.

Current source state was statically reviewed after the A2C implementation and focused Automation source were added. The repository is configured to run a 77-test Phase5–Phase6UIA2C aggregate gate. This status does **not** claim an Unreal Engine build or A2C Automation pass yet.

A2B Blueprint Damage/Block playback and PIE smoke remain a separate visual-integration item; A2C C++ correctness does not depend on them.

## 1. A2C scope

A2C owns exactly these committed presentation facts:

```text
CardPlayed
EnergyChanged
CardZoneChanged
DeckShuffled
Controller WorkingPresentationSnapshot reduction
```

A2C does **not** add separate `CardDrawn`, `CardDiscarded`, `CardExhausted`, or `CardRemoved` records. Those are zone transitions.

A2C also does not add Status/Relic presentation, terminal/fault polish, or Blueprint-owned state mutation.

## 2. Record taxonomy

The four A2C business record types are:

```text
CardPlayed
EnergyChanged
CardZoneChanged
DeckShuffled
```

Zone semantics:

```text
DrawPile → Hand        = Draw
Hand → DiscardPile     = Discard
PlayArea → DiscardPile = Finish to Discard
PlayArea → ExhaustPile = Exhaust
PlayArea → RemovedPile = Removed
```

`DeckShuffled` is one batch semantic fact. Shuffle never emits one zone record per moved card.

## 3. Card identity and frozen card snapshot

Concrete historical card identity is:

```text
(BattleId, CardRuntimeId)
```

`CardId` remains definition identity. Do not add a duplicate `CardDefinitionId`.

Every concrete-card A2C record carries a frozen presentation value:

```cpp
FPresentationCardSnapshot
{
    int32 RuntimeId;
    FName CardId;
    FText DisplayName;
    int32 Cost;
    ECardType CardType;
    ECardTargetType TargetType;
    FText Description;
    TObjectPtr<UTexture2D> CardArt;
};
```

It must not contain live `UCardInstance*`, `UCardData*`, `bGameplayPlayable`, or `UnplayableReason`.

The frozen card snapshot is required on both `CardPlayed` and `CardZoneChanged`. Historical playback must remain possible even when the concrete card is absent from the Envelope final Hand snapshot.

## 4. Energy CommitResult

Gameplay mutation truth is:

```cpp
FEnergyCommitResult
{
    bool bSucceeded;
    bool bCommitted;
    int32 EnergyBefore;
    int32 EnergyAfter;
    int32 Delta;
};
```

Locked invariants:

```text
Delta = EnergyAfter - EnergyBefore
bCommitted = EnergyBefore != EnergyAfter
```

`bSucceeded` means the requested energy operation was legal and completed. `bCommitted` means the numeric Energy value actually changed.

Therefore zero-cost spend is valid:

```text
bSucceeded = true
bCommitted = false
EnergyBefore == EnergyAfter
Delta = 0
```

A zero-cost card still emits `CardPlayed` with `CostPaid = 0`.

### 4.1 Card-play spend

Card-play cost is represented only inside `CardPlayed`:

```text
CardPlayed.EnergyBefore
CardPlayed.EnergyAfter
CardPlayed.CostPaid
```

Do not emit a duplicate `EnergyChanged` for the same card cost.

### 4.2 Independent energy changes

`EnergyChanged` is for non-card-play energy mutations. Current producers are:

```text
RequestEndPlayerTurn → Energy current → 0, only when changed
StartPlayerTurn      → Energy current → MaxEnergy, only when changed
```

BattleStart initialization and terminal/fault normalization do not emit separate A2C energy records; stable/final snapshots express them.

## 5. Deck mutation results

Gameplay-only zone mutation truth is:

```cpp
FCardZoneMutationResult
{
    bool bCommitted;
    int32 CardRuntimeId;
    FName CardId;
    ECardZone FromZone;
    ECardZone ToZone;
    int32 FromIndex;
    int32 ToIndex;
};
```

Required zones:

```text
DrawPile
Hand
PlayArea
DiscardPile
ExhaustPile
RemovedPile
```

Identity/index rules:

```text
RuntimeId = authoritative concrete-card identity
Index     = ordering and consistency data
DrawPile top = DrawPile.Num() - 1
```

Every failed/no-op operation obeys:

```text
bCommitted = false
⇒ every authoritative zone remains unchanged
```

## 6. Draw failure is mutation-free

`TryDrawTopCardCommit()` must validate before removal:

```text
DrawPile non-empty
Hand has capacity
Top entry is valid
↓
remove from DrawPile
add to Hand
↓
return committed result
```

An invalid top entry cannot be popped as part of a failed draw. This is Gameplay correctness, not a Presentation workaround.

## 7. CardPlayed is one composite fact

Accepted play is represented by one composite record:

```text
Hand → PlayArea commit
+
card-play Energy spend result
=
CardPlayed
```

Do not additionally emit `CardZoneChanged(Hand→PlayArea)` or card-cost `EnergyChanged`.

Payload:

```cpp
FCardPlayedPresentationPayload
{
    FPresentationCardSnapshot Card;
    FName SourcePresentationId;
    FName TargetPresentationId;
    int32 HandIndexBefore;
    int32 PlayAreaIndexAfter;
    int32 EnergyBefore;
    int32 EnergyAfter;
    int32 CostPaid;
};
```

`TargetPresentationId = None` is valid for targetless cards. A player/source combatant must resolve a trustworthy SourcePresentationId when an active writer requires history.

Formal order remains authoritative:

```text
Hand → PlayArea CommitResult
↓
Energy spend CommitResult
↓
CardPlayed
↓
effect Records
↓
CardZoneChanged PlayArea → resolved destination
```

Examples:

```text
Strike:
CardPlayed → Damage → PlayArea→Discard

Pommel Strike:
CardPlayed → Damage → DrawPile→Hand → PlayArea→Discard
```

## 8. Exact rollback contract

If Hand→PlayArea commits but the Execute-time energy spend fails:

```text
restore the same card to exact original HandIndexBefore
Energy unchanged
PlayArea restored
no CardPlayed
no rollback CardZoneChanged
```

Appending to the end of Hand is not an exact rollback.

If exact rollback itself fails after the first Gameplay mutation committed, the queue must enter Gameplay `ResolutionFault`; the engine must not continue as though no play occurred.

## 9. CardZoneChanged producers

All non-CardPlayed single-card zone mutations use:

```cpp
FCardZoneChangedPresentationPayload
{
    FPresentationCardSnapshot Card;
    ECardZone FromZone;
    ECardZone ToZone;
    int32 FromIndex;
    int32 ToIndex;
};
```

Current mappings:

```text
UDrawCardAction       → DrawPile → Hand
UDiscardCardAction    → Hand → DiscardPile
UFinishCardPlayAction → PlayArea → Discard/Exhaust/Removed
```

No committed mutation means no record.

Writer absent from the start is legal no-history mode. If a current valid writer exists and Gameplay committed but the frozen payload cannot be trusted, the current unpublished Presentation Resolution is invalidated; Gameplay is not rolled back for Presentation failure.

## 10. DeckShuffled ordering

Shuffle payload contains:

```cpp
FDeckShuffledPresentationPayload
{
    int32 MovedCardCount;
    int32 DrawCountBefore;
    int32 DrawCountAfter;
    int32 DiscardCountBefore;
    int32 DiscardCountAfter;
};
```

Ordering is locked:

```text
ShuffleDiscardIntoDrawPile commit
↓
DeckShuffled Record
↓
FDeckShuffledEvent
↓
reaction Actions
↓
RetryDraw
↓
CardZoneChanged DrawPile → Hand
```

Because reactions are inserted to the front, actual record history may be:

```text
DeckShuffled
Reaction Records
CardZoneChanged DrawPile → Hand
```

The original empty-draw attempt emits no record. RetryDraw emits one draw zone record only if it actually commits.

Initial battle shuffle and opening-hand draws remain initialization normalization and emit no A2C records.

## 11. EndTurn energy ordering

One successful EndTurn macro Resolution may contain two legitimate energy records:

```text
EnergyChanged current → 0
↓
Hand cleanup CardZoneChanged records
↓
Player TurnEnded reactions
↓
Enemy turn
↓
Enemy TurnEnded reactions
↓
EnergyChanged 0 → MaxEnergy
↓
Player TurnStartClear
↓
player-turn Draw records
↓
FinalSnapshot(PlayerTurn)
```

If the player dies before the next PlayerTurn, no restore record is emitted.

## 12. Producer matrix

| Producer | Presentation fact |
|---|---|
| `UPlayCardAction` | `CardPlayed` composite Hand→PlayArea + card-play spend |
| `UFinishCardPlayAction` | `CardZoneChanged` PlayArea→Discard/Exhaust/Removed |
| `UDrawCardAction` | `CardZoneChanged` DrawPile→Hand |
| `UDiscardCardAction` | `CardZoneChanged` Hand→DiscardPile |
| `UShuffleDeckAction` | `DeckShuffled` after commit, before Event Dispatch |
| `ABattleManager::RequestEndPlayerTurn` | `EnergyChanged` if value changes |
| `ABattleManager::StartPlayerTurn` | `EnergyChanged` if value changes |

All producer paths retain the A2A/A2B fail-soft split:

```text
writer absent from start
→ legal no-history

current writer + committed fact + invalid payload/append
→ invalidate unpublished Presentation history
→ publish no partial Envelope
→ Gameplay remains authoritative unless the Gameplay operation itself structurally failed
```

## 13. WorkingPresentationSnapshot ownership

Controller is the single C++ owner of historical intermediate display state.

```text
DisplayedPresentationSnapshot
↓ Envelope begins
WorkingPresentationSnapshot = DisplayedPresentationSnapshot
↓
play/fallback Record
↓
ApplyRecordToWorkingSnapshot
↓
ViewModel.ApplyPresentationSnapshot(WorkingPresentationSnapshot)
↓
next Record
↓ Envelope ends
apply exact Envelope.FinalSnapshot
DisplayedPresentationSnapshot = FinalSnapshot
```

The reducer may use only immutable Record data. It must not query mutable Gameplay to reconstruct intermediate state.

## 14. Reducer rules

### Damage

```text
Target.HP    = HPAfter
Target.Block = BlockAfter
Target.bDead follows represented HP
```

### BlockChanged

```text
Target.Block = BlockAfter
```

### CardPlayed

```text
find exact RuntimeId in Hand
validate HandIndexBefore/CardId consistency
remove it from Hand
Energy = EnergyAfter
```

### EnergyChanged

```text
validate current Energy == EnergyBefore
Energy = EnergyAfter
```

### CardZoneChanged DrawPile→Hand

```text
insert frozen Card at ToIndex
DrawCount--
```

### CardZoneChanged Hand→DiscardPile

```text
find exact RuntimeId/index/CardId
remove from Hand
DiscardCount++
```

### CardZoneChanged PlayArea→Discard/Exhaust/Removed

Update represented destination counts where the presentation snapshot exposes them. The final Envelope snapshot remains exact authority for fields not represented in intermediate state.

### DeckShuffled

```text
validate Draw/Discard before counts
DrawCount = DrawCountAfter
DiscardCount = DiscardCountAfter
```

A2B `Damage` and `BlockChanged` use the same reducer so there is one intermediate-state ownership model.

`Victory`, `Defeat`, and `ResolutionFault` may still rely on final-envelope reconciliation in A2C.

## 15. Playback completion/fallback

Existing token contract is unchanged:

```text
PlayPresentationRecord returns true
→ Blueprint accepted completion responsibility
→ eventually NotifyPresentationFinished(Token) exactly once

returns false
→ Controller immediately falls back
```

Normal completion, immediate fallback, and timeout completion apply the current record's reducer before advancing when history remains usable.

Skip, widget loss, invalid reducer history, or backlog collapse may deterministically collapse to the relevant immutable FinalSnapshot. Stale/duplicate tokens cannot reapply a record.

Blueprint must not mutate Hand, Energy, HP, Block, Draw/Discard counts, or any other historical state directly.

## 16. Focused Automation gate

A2C locks exactly **8 top-level tests**:

```text
SlayTheSpireDemo.Phase6UIA2C.Commit.EnergyResult
SlayTheSpireDemo.Phase6UIA2C.Commit.DeckMutation
SlayTheSpireDemo.Phase6UIA2C.Record.CardPlayed
SlayTheSpireDemo.Phase6UIA2C.Record.CardZoneChanged
SlayTheSpireDemo.Phase6UIA2C.Record.ShuffleOrdering
SlayTheSpireDemo.Phase6UIA2C.Record.EndTurnEnergy
SlayTheSpireDemo.Phase6UIA2C.Playback.WorkingSnapshot
SlayTheSpireDemo.Phase6UIA2C.Failure.PresentationDoesNotAffectGameplay
```

Required semantic coverage includes:

- positive, zero-cost, insufficient, clear/restore Energy results and Delta identity;
- exact zone identities/indices, mutation-free no-ops, invalid DrawPile top not popped;
- one composite CardPlayed with no duplicate Hand→PlayArea/Energy records;
- exact rollback to original Hand order on Execute-time spend failure;
- Draw/Discard/Discard-finish/Exhaust/Removed records with frozen card payloads;
- DeckShuffled committed before event dispatch and RetryDraw after reactions;
- opening setup emits no A2C history;
- EndTurn clear + restore ordering and lethal no-restore path;
- CardPlayed updates Hand/Energy before subsequent Damage display;
- A2B Damage reducer remains functional under the unified WorkingSnapshot model;
- writer-absent no-history versus current-writer-invalid-history/append-failure isolation.

## 17. CI gate

The aggregate self-hosted regression workflow is configured for:

```text
Phase5        13
Phase6A       23
Phase6B       12
Phase6C        5
Phase6UIA2A    8
Phase6UIA2B    8
Phase6UIA2C    8
----------------
Total         77
```

The gate still requires exact discovery counts, zero failed/not-run tests, and zero Editor exit code.

This configuration is **validation pending** until it is run successfully on UE5.8.

## 18. Static source review result

Static review covered the new reflected types/includes, Deck/Energy CommitResult APIs, Action initializer overload compatibility, PlayCard transaction/rollback path, Presentation payload types, frozen-card builder signature, producer wiring, Controller reducer declarations/definitions, test-only hooks, the 8 Automation definitions, and the workflow prefix/count configuration.

No remaining high-confidence C++/UHT compile blocker was identified by source inspection. Static review is not a substitute for UHT/MSVC/Unreal Automation execution.

## 19. Validation status

```text
Design contract                   LOCKED
A2C C++ source                    IMPLEMENTED
Static compile review             COMPLETE
8 top-level Automation tests      AUTHORED
CI aggregate gate                 CONFIGURED: 77
UE5.8 Editor build                PENDING
Phase6UIA2C Automation 8/8        PENDING
Affected 77-test regression       PENDING
Blueprint card/energy/zone visual PENDING
PIE smoke                         PENDING
```

Do not mark UI-A2C fully COMPLETE until UE5.8 build + focused A2C 8/8 + affected regression pass on the same source revision.

## 20. Completion definition

UI-A2C C++ validation closes when all are true:

```text
exact Energy CommitResult implemented
exact Deck mutation CommitResults implemented
failed/no-op Deck mutations are mutation-free
exact PlayCard rollback implemented
CardPlayed composite fact implemented
CardZoneChanged Draw/Discard/Finish implemented
DeckShuffled ordering implemented
independent turn-lifecycle EnergyChanged implemented
concrete-card records are self-sufficient frozen values
Controller reducer owns A2B+A2C intermediate state
Phase6UIA2C focused Automation = 8/8
Phase5–Phase6UIA2C aggregate gate = 77/77
```

Visible Blueprint animation/PIE validation may remain a later presentation-integration step, but C++ must preserve the single-owner rule:

```text
immutable Record → Controller reducer → ViewModel
```

Blueprint never becomes a second state authority.
