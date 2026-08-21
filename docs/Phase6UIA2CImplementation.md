# Phase 6UI-A2C — Card / Energy / Zone Committed Presentation

Status: **DESIGN LOCKED / IMPLEMENTATION NOT STARTED**.

UI-A2C extends the already-established UI-A2A transport/lifecycle and UI-A2B committed-record model. It must not redesign Resolution, Envelope, RecordWriter, Controller backlog, PlaybackToken, PresentationUnavailable, latest-only input binding, or the fail-soft no-partial-history rules.

A2B visual Blueprint Damage/Block playback and PIE smoke may remain pending while A2C C++ work proceeds. A2C must not use card/energy records as a workaround for missing A2B visual polish.

## 1. Scope

UI-A2C adds committed presentation for:

```text
CardPlayed
EnergyChanged
CardZoneChanged
DeckShuffled
Controller intermediate WorkingPresentationSnapshot reduction
```

It does not add separate semantic records for:

```text
CardDrawn
CardDiscarded
CardExhausted
CardRemoved
```

Those are expressed by `CardZoneChanged`.

UI-A2C also does not add Status presentation, Relic presentation, terminal/fault polish, or a second display-state owner.

## 2. Final Record taxonomy

A2C adds exactly four business record types:

```text
CardPlayed
EnergyChanged
CardZoneChanged
DeckShuffled
```

Do not add `CardDrawn`, `CardDiscarded`, `CardExhausted`, or `CardRemoved`.

Zone semantics are:

```text
DrawPile → Hand        = Draw
Hand → DiscardPile     = Discard
PlayArea → DiscardPile = Finish to Discard
PlayArea → ExhaustPile = Exhaust
PlayArea → RemovedPile = Removed
```

`DeckShuffled` is one batch semantic fact. Do not emit one `CardZoneChanged` per card moved by a shuffle.

## 3. Card identity and frozen card value

Historical concrete-card identity is:

```text
(BattleId, CardRuntimeId)
```

`CardId` remains the immutable card-definition identity already used by the project. Do not introduce a redundant `CardDefinitionId`.

A2C Records that identify a concrete card must carry a frozen, presentation-only value snapshot:

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

Do not store:

```text
UCardInstance*
UCardData*
bGameplayPlayable
UnplayableReason
```

`bGameplayPlayable` and the unplayable reason belong to a complete stable/current snapshot query, not to a historical movement record.

The frozen card value is required on both:

```text
CardPlayed
CardZoneChanged
```

Reason: a card may enter and leave Hand in one Resolution, and the Envelope final snapshot may no longer contain that concrete card. Historical playback must not query live CardInstance/DataAsset state to reconstruct it.

An immutable `UTexture2D` asset reference is allowed as presentation value data; runtime card objects are not.

## 4. Energy CommitResult

A2C introduces an exact Gameplay energy mutation result:

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

Invariant:

```text
Delta = EnergyAfter - EnergyBefore
bCommitted = EnergyBefore != EnergyAfter
```

The two booleans have different meanings:

```text
bSucceeded
= the requested energy operation was legal and completed

bCommitted
= the Energy value actually changed
```

Therefore a legal zero-cost spend is:

```text
bSucceeded = true
bCommitted = false
EnergyBefore == EnergyAfter
Delta = 0
```

A zero-cost card still produces `CardPlayed` with `CostPaid = 0`.

### 4.1 Card-play energy

Card-play cost must not generate both `CardPlayed` and `EnergyChanged`.

Lock:

```text
card-play spend
→ represented only inside CardPlayed
→ CardPlayed stores exact EnergyBefore / EnergyAfter / CostPaid
```

### 4.2 Independent energy changes

`EnergyChanged` is reserved for non-card-play energy mutations.

Current A2C producer scope:

```text
RequestEndPlayerTurn
→ player-turn-end Energy -> 0 when value changes

StartPlayerTurn
→ Energy -> MaxEnergy when value changes
```

BattleStart initialization and terminal/fault normalization do not produce separate `EnergyChanged` in A2C; final/stable snapshots own those states.

Future energy gains such as Sundial use `EnergyChanged`.

## 5. Generic card-zone mutation result

DeckRuntime returns Gameplay-only mutation truth:

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

Identity rule:

```text
RuntimeId = authoritative card identity
Index     = ordering and consistency data
```

Controller must never use only an index to guess which card moved.

Index semantics are zero-based authoritative array positions before/after mutation.

For DrawPile:

```text
DrawPile top = DrawPile.Num() - 1
```

All failed/no-op zone operations must obey:

```text
bCommitted = false
⇒ every authoritative zone remains unchanged
```

## 6. Fix existing draw fail-after-mutation behavior

Current `TryDrawTopCard()` pops first and validates the popped card afterward. A2C must remove this failure-after-mutation path.

Required order:

```text
validate DrawPile non-empty
validate Hand capacity
inspect and validate top card without removing it
↓
only then mutate DrawPile and Hand
↓
return committed mutation result
```

An invalid top entry must return no commit while leaving DrawPile and Hand exactly unchanged.

This is a Gameplay correctness fix required by the A2C CommitResult contract, not a Presentation-only workaround.

## 7. CardPlayed is one composite committed fact

`CardPlayed` absorbs the two commits that together define accepted play:

```text
Hand → PlayArea
+ card-play Energy spend
```

Do not emit separate:

```text
CardZoneChanged(Hand → PlayArea)
EnergyChanged(card cost)
```

for the same play.

Suggested payload:

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

`TargetPresentationId = None` is valid for targetless cards.

`SourcePresentationId` must resolve for the player/source combatant when an active writer requires a historical Record.

## 8. Formal PlayCard commit order

Current authoritative order is preserved:

```text
TryMoveHandCardToPlayArea()
↓
TrySpendEnergy()
↓
enqueue effect Actions + FinishCardPlayAction
```

A2C turns the first two successful internal commits into one historical `CardPlayed` fact.

Required sequence:

```text
Hand → PlayArea CommitResult
↓
Energy spend CommitResult
↓
if both operations succeed
    append CardPlayed
↓
card effect Records
↓
CardZoneChanged(PlayArea → resolved destination)
```

Example Strike:

```text
CardPlayed
  Card: Strike#N
  HandIndexBefore = X
  PlayAreaIndexAfter = Y
  Energy 3 → 2
  CostPaid = 1
↓
Damage
↓
CardZoneChanged PlayArea → DiscardPile
```

Example Pommel Strike:

```text
CardPlayed
↓
Damage
↓
CardZoneChanged DrawPile → Hand
↓
CardZoneChanged PlayArea → DiscardPile
```

This is not Presentation reordering. `CardPlayed` is a composite fact emitted only after both authoritative internal commits have succeeded.

## 9. PlayCard rollback invariant

If:

```text
Hand → PlayArea succeeds
Energy spend fails
```

Gameplay must rollback PlayArea → Hand before the Action finishes.

Rollback must restore the card to the exact original `HandIndexBefore`.

The existing behavior of merely `Hand.Add(Card)` is insufficient because it may change visible Hand order.

Required rollback result:

```text
card object restored
same RuntimeId
same original Hand index
PlayArea restored
Energy unchanged
no CardPlayed Record
no CardZoneChanged rollback Record
```

Rollback is an internal transactional repair, not historical gameplay that should animate.

If exact rollback fails after the first mutation committed, this is Gameplay state corruption and must request a Gameplay `ResolutionFault`; do not continue as if the play never happened.

## 10. CardZoneChanged payload

All other concrete-card zone mutations use:

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

Required A2C mappings:

```text
UDrawCardAction
→ DrawPile → Hand

UDiscardCardAction
→ Hand → DiscardPile

UFinishCardPlayAction
→ PlayArea → DiscardPile / ExhaustPile / RemovedPile
```

No-op/failed mutation means no Record.

A writer absent from the start remains legal no-history mode.

A current valid writer plus a committed zone mutation that cannot produce a trustworthy frozen card/identity/payload invalidates the unpublished Presentation Resolution only; Gameplay is not rolled back because of Presentation failure.

## 11. DeckShuffled payload and ordering

`DeckShuffled` is appended after the successful authoritative shuffle commit and before `FDeckShuffledEvent` dispatch.

Required order:

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

Because trigger reactions are inserted at the front, actual historical Records may be:

```text
DeckShuffled
↓
Reaction Records
↓
CardZoneChanged DrawPile → Hand
```

The initial DrawAction that merely discovers an empty DrawPile has no commit and emits no Record.

`RetryDraw` emits exactly one DrawPile → Hand Record only if the draw actually commits.

Initial battle setup remains:

```text
initial deterministic shuffle → no DeckShuffled Record
opening-hand draws             → no CardZoneChanged Records
```

Suggested payload:

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

Do not freeze the entire post-shuffle DrawPile order for A2C because the current HUD does not render that order. Each later actual draw freezes the concrete card it reveals.

## 12. EndTurn EnergyChanged ordering

The entire automatic EndTurn progression remains one Presentation Resolution.

One normal EndTurn Envelope may legitimately contain two independent Energy transitions:

```text
EnergyChanged current → 0
↓
Hand cleanup CardZoneChanged records
↓
Player TurnEnded reactions
↓
Enemy turn work
↓
Enemy TurnEnded reactions
↓
EnergyChanged 0 → MaxEnergy
↓
Player TurnStartClear
↓
normal player-turn Draw records
↓
stable PlayerTurn FinalSnapshot
```

If the player dies before next PlayerTurn, the recovery record does not exist.

These two records are not duplicates; they represent different authoritative commits inside one macro Resolution.

## 13. Producer matrix

A2C current producer matrix is locked as:

| Producer | Record |
|---|---|
| `UPlayCardAction` | `CardPlayed` composite Hand→PlayArea + card-play Energy spend |
| `UFinishCardPlayAction` | `CardZoneChanged` PlayArea→Discard/Exhaust/Removed |
| `UDrawCardAction` | `CardZoneChanged` DrawPile→Hand |
| `UDiscardCardAction` | `CardZoneChanged` Hand→DiscardPile |
| `UShuffleDeckAction` | `DeckShuffled`, after commit and before Event Dispatch |
| `ABattleManager::RequestEndPlayerTurn` | `EnergyChanged` when Energy actually changes |
| `ABattleManager::StartPlayerTurn` | `EnergyChanged` when Energy actually changes |

BattleStart opening draw/shuffle normalization intentionally emits no A2C Records.

Every producer follows the same A2A/A2B history rules:

```text
no writer from start
→ legal no-history mode

current writer + committed fact + missing/invalid frozen payload or Append failure
→ invalidate whole unpublished Presentation Resolution
→ no partial Envelope
→ Gameplay continues unless the Gameplay mutation itself failed structurally
```

## 14. Controller WorkingPresentationSnapshot ownership

A2C must add intermediate display-state reduction. Records alone are insufficient if the ViewModel remains frozen until Envelope completion.

Controller becomes the only owner of historical intermediate display state.

Locked model:

```text
DisplayedPresentationSnapshot
= state currently reflected by the HUD

Envelope begins
↓
WorkingPresentationSnapshot = DisplayedPresentationSnapshot

for each Record
    Blueprint/C++ playback
    ↓
    accepted completion OR immediate fallback
    ↓
    ApplyRecordToWorkingSnapshot(Record)
    ↓
    ViewModel.ApplyPresentationSnapshot(WorkingPresentationSnapshot)
    ↓
    next Record

Envelope completes
↓
apply exact Envelope.FinalSnapshot
↓
DisplayedPresentationSnapshot = FinalSnapshot
```

Do not read mutable Gameplay to advance the WorkingSnapshot.

The Record `After` values and frozen payload are sufficient.

## 15. Reducer rules

The reducer must cover A2B and A2C records consistently rather than creating two state-ownership models.

### Damage

After the Damage Record completes/falls back:

```text
Target.HP    = HPAfter
Target.Block = BlockAfter
Target.bDead follows the represented HP state as appropriate
```

### BlockChanged

```text
Target.Block = BlockAfter
```

### CardPlayed

```text
remove the card matching RuntimeId from Hand
preserve identity checks with HandIndexBefore as consistency data
set Energy = EnergyAfter
update any represented Hand count implicitly through HandCards
```

Blueprint must not directly remove the card or subtract Energy.

### CardZoneChanged DrawPile → Hand

```text
add Record.Card to Hand at ToIndex
DrawCount decreases according to the committed transition
```

### CardZoneChanged Hand → DiscardPile

```text
remove exact RuntimeId from Hand
DiscardCount increases
```

### CardZoneChanged PlayArea → Discard/Exhaust/Removed

The current full presentation snapshot does not expose concrete PlayArea/Removed arrays; reducer updates the represented counts and any visible Hand state required by the record. FinalSnapshot remains the exact end-of-Envelope authority.

### DeckShuffled

```text
DrawCount    = DrawCountAfter
DiscardCount = DiscardCountAfter
```

### EnergyChanged

```text
Energy = EnergyAfter
```

`Victory`, `Defeat`, and `ResolutionFault` may continue to rely on final Envelope catch-up in A2C unless a later slice gives them visible intermediate state.

## 16. Playback completion and fallback

Existing playback-token semantics remain unchanged.

```text
PlayPresentationRecord returns true
→ Blueprint accepted responsibility
→ must eventually NotifyPresentationFinished(Token) exactly once

returns false
→ immediate fallback
```

After either normal completion or immediate fallback, Controller applies that Record to WorkingPresentationSnapshot before proceeding.

For timeout/widget loss/skip/backlog collapse:

```text
Controller must never wait on mutable Gameplay
Controller may deterministically collapse to the relevant Envelope.FinalSnapshot
old/stale completion tokens remain ignored
```

If a Record reducer cannot apply a supposedly valid historical Record consistently, treat that as Presentation history corruption/fail-safe behavior; never mutate Gameplay to repair presentation.

## 17. Snapshot and Record ownership boundary

A2C resolves the temporary A2B visual gap where Damage could play while old Hand/Energy remained displayed.

After `CardPlayed` completion:

```text
Hand and Energy move to their committed intermediate values before Damage/Block records play
```

After a Draw record completes:

```text
new card appears in Hand before later records continue
```

Blueprint is strictly a rendering layer. It must not own authoritative or historical state transitions such as:

```text
RemoveCardFromHand
Energy -= Cost
DrawCount--
DiscardCount++
```

Those transitions are applied by the Controller reducer from immutable Records.

## 18. Initial implementation order

Recommended implementation order:

```text
ECardZone + mutation result types
↓
DeckRuntime exact mutation-result APIs
↓
fix draw fail-after-mutation behavior
↓
exact rollback-to-original-Hand-index support
↓
FEnergyCommitResult and energy mutation APIs
↓
A2C typed Record payloads
↓
Card snapshot freezer helper
↓
UPlayCardAction CardPlayed
↓
Draw / Discard / Finish zone Records
↓
DeckShuffled before event dispatch
↓
EndTurn / StartPlayerTurn EnergyChanged
↓
Controller WorkingPresentationSnapshot reducer
↓
focused Automation
↓
regression gate
```

Do not modify Blueprint/UMG merely to prove C++ correctness. As in A2B, first close the C++ + Automation path, then wire visible presentation.

## 19. Required Automation coverage

Exact top-level discovery count may be locked when implementation begins, but the semantic gate must cover at least the following concepts.

### Energy commit semantics

```text
positive legal spend
zero-cost successful no-change
insufficient-energy failure
turn-end clear only when changed
turn-start restore only when changed
Delta identity
```

### Deck mutation semantics

```text
DrawPile→Hand exact indices and identity
Hand→Discard exact indices
Hand→PlayArea exact indices
PlayArea→Discard/Exhaust/Removed exact indices
all no-op/failure paths leave every zone unchanged
invalid top DrawPile entry does not Pop
```

### PlayCard composite fact

```text
one CardPlayed only
no duplicate Hand→PlayArea CardZoneChanged
no duplicate card-cost EnergyChanged
exact frozen card snapshot
exact HandIndexBefore / PlayAreaIndexAfter
exact EnergyBefore / After / CostPaid
zero-cost card still produces CardPlayed
```

### Rollback

```text
move to PlayArea succeeds
energy spend fails
exact original Hand index restored
no CardPlayed
no zone Record
Energy unchanged

rollback failure
→ Gameplay ResolutionFault
```

### Zone records

```text
Draw record freezes concrete card
Discard record
Finish-to-Discard
Finish-to-Exhaust
Finish-to-Removed
same-resolution draw then leave-Hand remains replayable without live CardInstance
```

### Shuffle

```text
successful shuffle emits DeckShuffled before FDeckShuffledEvent
reaction Records execute before RetryDraw record
original empty-draw attempt emits nothing
RetryDraw emits exactly one DrawPile→Hand on commit
initial setup shuffle/opening draws emit nothing
```

### EndTurn ordering

```text
Energy current→0 precedes hand cleanup
next-turn 0→MaxEnergy appears only if next player turn is reached
turn-start draw follows restore
lethal enemy flow has no restore record
```

### WorkingSnapshot reducer

```text
CardPlayed completion updates Hand + Energy before next Damage/Block record
Damage reducer updates HP/Block from After values
Block reducer updates Block
Draw reducer inserts frozen card
EnergyChanged reducer updates Energy
DeckShuffled reducer updates counts
final Envelope snapshot still wins exact reconciliation
false-return immediate fallback still applies Record After state
skip/collapse reaches FinalSnapshot
stale token cannot apply reducer twice
```

### Presentation failure isolation

```text
writer absent from start = valid no-history
committed card/energy/zone fact + invalid frozen payload/current-writer append failure
→ no partial Envelope
→ Gameplay not rolled back because of Presentation
→ latest frozen baseline remains exact
```

## 20. Completion definition

UI-A2C C++ source is complete only when:

```text
exact Energy CommitResult implemented
exact Deck mutation CommitResults implemented
failed/no-op Deck mutations are mutation-free
PlayCard exact rollback invariant implemented
CardPlayed composite fact implemented
CardZoneChanged implemented for Draw/Discard/Finish paths
DeckShuffled implemented before event dispatch
independent EnergyChanged implemented for current turn lifecycle
all concrete-card records freeze self-sufficient presentation card data
Controller WorkingPresentationSnapshot reducer implemented for A2B + A2C business records
focused A2C Automation passes
A2A/A2B and affected Phase5/6/UI regressions pass on the same source revision
```

Visible Blueprint card/energy/zone animation and PIE smoke may be validated as a later presentation integration step, but C++ must already preserve the single-owner rule: immutable Record → Controller reducer → ViewModel; Blueprint never becomes a second state authority.
