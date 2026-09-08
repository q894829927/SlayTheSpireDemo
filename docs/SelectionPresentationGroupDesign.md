# Selection Presentation Group + Parallel Animation Design

Date: **2026-09-09**

Status:

```text
DRAFT / DESIGN ONLY / NO PRODUCTION CODE CHANGED / NOT VALIDATED / NOT SEALED
```

Scope: replace the current one-Record-at-a-time visual treatment of a confirmed multi-card Selection with an explicit Selection Presentation Group. The group may present multiple committed selected-card destination facts concurrently while preserving authoritative Gameplay order and reducer order.

This design is subordinate to the current user request and complements:

- `docs/CardSelectionPresentationConstraints.md`
- `docs/CardSelectionRefactorConstraints.md`
- `Source/SlayTheSpireDemo/Presentation/AGENTS.md`
- `Source/SlayTheSpireDemo/UI/AGENTS.md`

It does **not** authorize implementation yet beyond the design described here.

---

## 1. Problem

The current Controller playback kernel is one Record at a time:

```text
Record A
→ play A
→ wait
→ reduce A
→ ApplyPresentationSnapshot
→ Record B
→ play B
→ wait
→ reduce B
→ ApplyPresentationSnapshot
→ ...
```

For a confirmed multi-card Selection that exhausts `A, B, C`, this produces two visible defects:

1. the total visual delay scales with selected-card count (`N × animation duration`);
2. reducing/rebuilding Hand after A may destroy the retained Selection transform for B/C, causing later cards to flash back to the Hand row before disappearing.

A per-Record handoff/deferred-layout patch can hide the second symptom but does not solve the first and keeps the wrong playback unit.

The desired visual unit is the **Selection decision**, not each selected card independently.

---

## 2. Non-negotiable invariants

### 2.1 Gameplay order is unchanged

This design MUST NOT change:

```text
SelectionResult canonical order
→ continuation Action order
→ Gameplay mutation order
→ BattleEvent dispatch order
→ Trigger reaction order
→ PresentationSequence assignment order
```

Gameplay still executes all Actions immediately and deterministically. Presentation never blocks Gameplay.

### 2.2 Reducer order is unchanged

Even if selected-card visuals are co-presented, committed Records are still reduced strictly by original `PresentationSequence`.

The group MUST NOT apply B before A merely because A/B/C animate together.

### 2.3 Grouping is explicit committed Presentation metadata

The Controller MUST NOT infer a group from:

- CardId;
- Effect type;
- consecutive `Hand→Exhaust` records;
- click order;
- equal destinations;
- timing/frame proximity.

A Record is a group member only when it carries an explicit Selection Presentation group tag created from the Selection decision that caused the direct continuation Action.

### 2.4 Trigger reactions are not Selection group members by inheritance

`UExhaustCardAction` may append its direct `CardZoneChanged` Record and then dispatch `CardExhausted`. Trigger reactions are inserted at the front of the Queue and may execute before the next selected-card continuation Action.

Therefore group metadata MUST NOT live inside the ordinary `FPresentationRecordWriter` in a form that automatically propagates through `BattleEventDispatcher`.

The writer remains the resolution-recording capability only.

Selection group correlation is separate Action-local Presentation metadata.

### 2.5 No Effect/CardId visual branching

Warcry, Burning Pact and future cards do not own the group or its animation.

The group is shared Selection Presentation infrastructure.

---

## 3. High-level model

```text
Player confirms Selection
        ↓
Gameplay validates SelectionResult
        ↓
SelectionRequestAction builds direct continuation batch
        ↓
create one Selection Presentation correlation
        ↓
stamp correlation onto direct continuation Actions only
        ↓
Gameplay executes normally
        ↓
direct selected-card CardZoneChanged Records carry same group tag
trigger/reaction Records do not
        ↓
sealed Presentation Envelope
        ↓
Controller reaches first group member
        ↓
Controller gathers and preflights complete committed group
        ↓
Widget starts all selected-card destination child transitions together
        ↓
one group completion boundary
        ↓
Controller continues reducer cursor in original PresentationSequence order
```

A Selection Presentation Group is a **visual cohort**, not a Gameplay transaction and not a second mutation batch.

---

## 4. Record metadata

Add explicit Presentation-only grouping metadata to `FPresentationRecord`.

Conceptual shape:

```cpp
enum class EPresentationGroupKind : uint8
{
    None,
    SelectionDestination
};

struct FPresentationGroupTag
{
    EPresentationGroupKind Kind = EPresentationGroupKind::None;
    int64 GroupId = 0;
    int32 ExpectedMemberCount = 0;
};
```

`FPresentationRecord` gains one `FPresentationGroupTag`.

Rules:

```text
GroupId == 0
↔ ungrouped Record

Group identity
= (BattleId, ResolutionId, Kind, GroupId)
```

`ExpectedMemberCount` is repeated on each member so a sealed Envelope can validate whether the group is complete.

No separate member order field is required. `PresentationSequence` remains the canonical deterministic member order.

### 4.1 Eligible initial group members

The first implementation is intentionally narrow:

```text
Record.Type = CardZoneChanged
FromZone = Hand
Group.Kind = SelectionDestination
```

Supported destinations initially:

- `ExhaustPile`
- `DrawPile`
- `DiscardPile` when/if the existing generic Native destination path is ready for Selection use

The architecture permits mixed destinations in one group; destination animation is chosen independently from each committed Record.

---

## 5. Correlation creation

### 5.1 Do not use SelectionSource as identity

`FSelectionRequest::SelectionSource` is semantic data, not a unique request identity. Multiple decisions may use the same source name in one Resolution.

### 5.2 Allocate a Presentation-scoped GroupId

`FPresentationRecordWriter`/`UBattlePresentationRecorder` should expose a narrow Presentation-only allocation surface such as:

```cpp
bool TryAllocatePresentationGroupId(int64& OutGroupId) const;
```

The Recorder owns the counter because it already owns:

```text
BattleId
ResolutionId
PresentationSequence allocation
```

The group counter is scoped to the active Resolution and has no Gameplay meaning.

If Presentation recording is unavailable, allocation simply fails and Gameplay continues with no group metadata.

### 5.3 SelectionRequestAction remains object-type neutral

`USelectionRequestAction` MUST NOT cast selected objects to `UCardInstance`.

After a resolved result, it can map `Result.SelectedObjects` back to the frozen `Request.Candidates` and obtain the canonical selected `RuntimeSequence` values.

For CurrentHand candidates, `RuntimeSequence` is the exact card RuntimeId. For other future Selection domains it remains only the generic candidate runtime sequence.

Create an Action-local correlation context conceptually containing:

```cpp
struct FSelectionPresentationGroupContext
{
    int64 GroupId = 0;
    int32 ExpectedMemberCount = 0;
    TArray<int32> SelectedRuntimeSequences;
};
```

The same context is assigned to the direct continuation Actions returned by the resolved Selection.

### 5.4 Group context does not propagate through EventDispatcher

`UBattleAction` continues to store the ordinary `FPresentationRecordWriter` separately from Selection group context.

`BattleEventDispatcher` propagates only the writer, as it does today.

Therefore:

```text
Selected Exhaust Action A
→ direct Hand→Exhaust Record: group member
→ CardExhausted event
→ Trigger reaction Actions: NOT group members
```

This is mandatory.

### 5.5 Direct Action stamps only matching committed identity

A direct continuation Action may stamp the group tag only when the committed presentation fact corresponds to one of the selected candidate runtime sequences.

For card-zone Actions:

```text
CommitResult.CardRuntimeId ∈ SelectedRuntimeSequences
→ stamp SelectionDestination GroupTag
```

This keeps the generic Selection boundary object-type neutral while allowing a concrete card-zone Action to bind its own committed RuntimeId to the generic Selection correlation.

A direct Action that emits unrelated Records must not stamp them merely because the Action carries a group context.

---

## 6. Complete-group validation

The Controller receives the entire sealed Envelope before playback, so it can validate a group without consulting mutable Gameplay.

When the cursor reaches the first member of a group, gather all Records in the active Envelope with the same exact group identity.

The group is eligible for co-presentation only when all are true:

1. member count equals `ExpectedMemberCount`;
2. every member repeats the same non-zero identity and expected count;
3. every member is an initially supported `CardZoneChanged` from Hand;
4. member `PresentationSequence` values are unique and increasing in Envelope order;
5. selected card RuntimeIds are unique;
6. no terminal Record belongs to the group;
7. all member payloads pass normal frozen-record validation;
8. the relevant historical state can be preflight-reduced successfully through the member span.

If the group is incomplete or malformed, this is a **Presentation grouping degradation**, not a Gameplay fault.

Fallback:

```text
ignore group co-presentation
→ process those Records through the existing sequential path
```

Do not invalidate committed Gameplay or request a ResolutionFault.

---

## 7. Trigger/interleaving semantics

Group members are not guaranteed to be contiguous.

Example authoritative execution:

```text
A Exhaust Record          [Group 7]
A CardExhausted trigger Record(s) [ungrouped]
B Exhaust Record          [Group 7]
B CardExhausted trigger Record(s) [ungrouped]
C Exhaust Record          [Group 7]
```

Changing Action/trigger ordering to make the group contiguous is forbidden.

### 7.1 Controlled visual lookahead

When the Controller reaches the first complete group member, it is allowed to look ahead in the **already sealed immutable Envelope** and co-present only the other committed members of the same explicit group.

Thus visually:

```text
A/B/C selected-card destination animations start together
```

This does not reduce B/C early.

It does not mark interleaved trigger Records as played.

It does not expose future mutable Gameplay.

The lookahead is authorized only by explicit group metadata and frozen committed facts.

### 7.2 Reducer remains chronological

After the group visual completes, the Controller continues from the original cursor:

```text
reduce A
→ encounter/interactively present A-trigger Record(s)
→ encounter B group member: visual already presented, reduce B without replay
→ present B-trigger Record(s)
→ encounter C group member: reduce C without replay
→ ...
```

Therefore:

- visible selected-card movement/fade is co-presented;
- trigger/status/draw/damage presentation is not silently skipped;
- WorkingSnapshot remains authored-order deterministic.

This is the deliberate distinction between **visual presentation order** and **reducer order** for an explicit Selection group.

---

## 8. Controller playback unit

Refactor the Controller concept from:

```text
active playback = one Record
```

to:

```text
active playback unit =
    SingleRecord
    OR SelectionPresentationGroup
```

Conceptual Controller state:

```cpp
enum class EPresentationPlaybackUnitKind
{
    None,
    SingleRecord,
    SelectionGroup
};

struct FActivePresentationGroup
{
    FPresentationGroupTag Tag;
    TArray<int32> MemberRecordIndices;
};
```

`ActiveRecordIndex` remains the reducer cursor.

The group stores member indices but does not move the cursor ahead at Begin time.

### 8.1 Group preflight

Before offering the group to the Widget, clone `WorkingPresentationSnapshot` and dry-run the reducer from the current cursor through the last member index in original Record order.

The preflight includes interleaved non-group Records.

Purpose:

- validate each later group member against the correct historical state;
- avoid asking the Widget to validate B/C against the current pre-A ViewModel;
- keep the Widget independent of mutable or future Gameplay state.

This requires extracting/refactoring the reducer so it can apply a Record to an arbitrary snapshot copy rather than only the Controller member field.

Preflight does **not** publish the copied snapshot to the HUD.

### 8.2 Group accepted

If Widget accepts group playback:

```text
bWaitingForCompletion = true
→ one exact group token
→ one timeout boundary
```

On completion, mark its member indices as `VisuallyPresented` for the current Envelope.

Then continue reducer processing at the original cursor.

### 8.3 VisuallyPresented member

When the reducer cursor later reaches a member already co-presented by its group:

```text
skip BeginPresentationRecordPlayback
→ ApplyRecordToWorkingSnapshot in normal order
→ advance cursor
```

Consecutive already-presented members may be drained synchronously and publish one HUD snapshot at the end of that consecutive run.

An interleaved ungrouped Record breaks the drain and is played normally.

### 8.4 Widget declines group

If the Widget cannot start a complete group:

```text
mark this group identity as group-playback-disabled for the active Envelope
→ immediately fall back to existing single-Record path at current cursor
```

No Record is lost and no group member is marked visually presented.

---

## 9. Playback token and cancellation

The completion/cancellation boundary remains exact and generation-scoped.

Extend the existing token semantics so a token can identify a playback unit, not merely one Record.

Conceptually add:

```text
PlaybackUnitKind
PresentationGroupId (0 for single Record)
```

For a group token:

```text
BattleId
ResolutionId
PresentationSequence = first member sequence (diagnostic/leader identity)
PresentationGroupId = group id
LocalPlaybackGeneration
```

Stale/duplicate/old-battle/post-skip callbacks remain ignored.

### 9.1 Timeout

One group timeout covers the whole parallel visual cohort.

Timeout cancels all child visuals owned by the group token, then continues through Presentation fallback/catch-up policy. It never faults Gameplay.

### 9.2 Skip / Widget loss

Skip cancels the entire active group as one visual owner and keeps the existing Controller behavior of collapsing to the newest frozen final snapshot.

No half-owned child animation may survive Skip or Widget replacement.

---

## 10. Widget API

The Controller, not the Widget, discovers group membership.

Add a Controller-facing group wrapper parallel to the current Record wrapper, conceptually:

```cpp
bool PlayPresentationGroup(
    const FPresentationPlaybackGroup& Group,
    const FPresentationPlaybackToken& Token);
```

The group contains only exact frozen member Records in canonical `PresentationSequence` order.

The Widget MUST NOT scan future Envelope Records itself.

The Native stack is the production target. The base implementation may return false so unsupported surfaces fall back to sequential playback without Blueprint asset work.

---

## 11. Generic parallel card-transition engine

Current Native card presentation state is fundamentally single-instance (`ActiveNativeMovingCardWidget`, one start/end anchor, one elapsed timer, one card presentation kind). That state cannot safely animate N cards in parallel.

Extract the card-zone animation into per-child state.

Conceptual child:

```cpp
struct FNativeCardTransitionInstance
{
    int32 RuntimeId;
    ECardZone FromZone;
    ECardZone ToZone;

    TObjectPtr<UBattleCardWidget> MovingVisual;
    TWeakObjectPtr<UBattleCardWidget> HistoricalFormalCard;

    FVector2D StartTranslation;
    FVector2D EndTranslation;
    float StartScale;
    float EndScale;
    float StartOpacity;
    float EndOpacity;
    float ElapsedSeconds;
};
```

One generic transition builder decides style only from committed zone facts.

### 11.1 Hand→Exhaust

```text
source = confirmed Selection visual position when supplied
end = same position
opacity 1 → 0
```

For grouped Selection playback, use an independently owned transient visual for each member and hide the corresponding formal Hand widget.

This is still the same generic Exhaust transition style; it is not a second Burning Pact/multi-select animation.

### 11.2 Hand→DrawPile

```text
source = confirmed Selection visual position
end = DrawPile visual anchor
move + existing generic scale/fade policy
```

The current Selection-subclass-only Hand→DrawPile timer/state should ultimately migrate into this generic card-transition engine.

### 11.3 Hand→DiscardPile

Use the existing generic discard destination behavior with the confirmed source position.

### 11.4 Single Record and group share the engine

Do not keep two independent implementations.

```text
single CardZoneChanged
→ one transition instance

grouped Selection destination
→ N transition instances
```

This is the required reuse boundary.

---

## 12. Selection visual handoff

`UBattleHUDSelectionWidget` continues to own Selection interaction state and confirmed source positions.

Its responsibility is conceptually:

```text
RuntimeId
→ confirmed absolute source position
```

It does not decide Exhaust/DrawPile/Discard animation.

When a group is offered:

1. require an exact handoff for every selected-card group member;
2. build all child transition instances transactionally;
3. only after every child is valid, hide formal selected cards and consume group handoff ownership;
4. if any child cannot start, roll back all temporary visuals and return false so Controller uses sequential fallback.

No partial group start is permitted.

The played card visual (for example Warcry/Burning Pact itself) remains separate. It stays hidden/retained as required during Selection destination playback and is restored for its own later normal committed cleanup.

---

## 13. Group completion

All child transitions begin in the same Native tick.

For the initial implementation, reuse the existing uniform card-transition duration.

```text
Begin group
→ initialize every child elapsed = 0
→ tick all children from same DeltaSeconds
→ group finishes after shared duration
→ clean every child
→ notify Controller once with exact group token
```

Total wait for N selected cards is approximately one transition duration, not `N × duration`.

If different child durations are introduced in the future, group completion would be the maximum child completion, but this is not required for the first implementation.

---

## 14. Warcry behavior

Warcry's Selection result is one selected Hand card moved to DrawPile.

The same architecture may represent this as a one-member `SelectionDestination` group:

```text
Warcry Selection Confirm
→ Group 12:
   selected RuntimeId A, Hand→DrawPile
→ one generic child transition
→ group completion
→ reducer continues
→ Warcry PlayArea→Exhaust Record remains ungrouped
→ existing generic played-card Exhaust cleanup
```

Warcry's own Exhaust Record MUST NOT inherit the Selection group because Warcry RuntimeId is not one of the selected candidate runtime sequences and it is not a direct selected-card destination fact.

No Warcry-specific animation is introduced.

---

## 15. Burning Pact / multi-exhaust behavior

For a three-card configured selection:

```text
Confirm A/B/C
→ one SelectionDestination group correlation

Committed direct Records:
A Hand→Exhaust [Group G]
B Hand→Exhaust [Group G]
C Hand→Exhaust [Group G]
```

At first group member playback:

```text
A fade ┐
B fade ├─ start together at their confirmed Selection positions
C fade ┘
```

After one duration, Controller resumes reducer order.

If Exhaust triggers produced interleaved ungrouped Records, those are still presented when the chronological reducer cursor reaches them.

---

## 16. Failure and degradation rules

Grouping is Presentation-only optimization/semantics.

The following MUST NOT cause Gameplay ResolutionFault by themselves:

- group ID allocation unavailable;
- incomplete group membership;
- unsupported group destination;
- Widget group rejection;
- child visual creation failure;
- timeout;
- Skip;
- Widget replacement/loss.

Fallback is existing sequential presentation or final-snapshot catch-up according to current Controller policy.

A malformed committed Record that already violates the existing Presentation envelope/reducer contract may still trigger existing Presentation collapse behavior, but not Gameplay mutation/fault.

---

## 17. Explicitly rejected designs

Do not implement:

### 17.1 Per-card deferred Hand rebuild patch

```text
A plays
→ rebuild Hand
→ manually restore B
→ B plays
→ rebuild Hand
→ manually restore C
```

This retains the incorrect `N × duration` playback model.

### 17.2 Effect/Card special casing

No:

```cpp
if (CardId == BurningPact)
if (SelectedCount > 1 && Effect == Exhaust)
```

### 17.3 Gameplay batching for visual convenience

Do not replace authoritative per-card Actions/events with one bulk Gameplay mutation merely to make animations simultaneous.

### 17.4 Writer-carried implicit group inheritance

Do not put active Selection group state into the ordinary writer such that trigger-created Actions inherit it automatically.

### 17.5 Widget-owned Envelope scanning

The Widget must not search future Records to construct a group. Controller owns sequencing/group discovery.

---

## 18. Proposed implementation order

### Stage G1 — metadata/correlation only

- add group tag types to Presentation records;
- add Recorder-scoped group ID allocation;
- add Action-local Selection presentation group context;
- stamp direct selected-card zone Records;
- prove trigger reaction Actions do not inherit group context;
- no parallel UI yet.

### Stage G2 — Controller playback unit

- introduce SingleRecord vs SelectionGroup playback unit;
- complete-group discovery and validation;
- arbitrary-snapshot reducer preflight;
- `VisuallyPresented` member tracking;
- group token/timeout/skip semantics;
- Widget-declined group sequential fallback.

### Stage G3 — Native generic child transition engine

- extract current one-card transition state into reusable child instances;
- migrate single CardZoneChanged playback to one child;
- migrate Hand→DrawPile out of Selection-specific timer/state into generic zone transition;
- add N-child parallel group playback;
- Selection subclass supplies source handoff positions only.

### Stage G4 — integration/cleanup

- delete superseded per-Selection bespoke transfer state once generic child engine proves equivalent;
- update `CardSelectionPresentationConstraints.md` multi-selection section to make parallel co-presentation authoritative;
- update current checkpoint/validation docs.

Do not combine all stages into one unreviewable patch if smaller compilable steps are possible.

---

## 19. Automated acceptance plan

### Correlation

- one resolved multi-card Selection gives all direct selected-card destination Records the same non-zero group identity;
- two Selection decisions in one Resolution receive distinct group IDs;
- trigger-generated Records between selected-card Actions remain ungrouped;
- Presentation unavailable/no-history does not affect Gameplay continuation;
- group member identity follows canonical selected runtime sequence, not click order.

### Controller

- contiguous 3-member group is offered to Widget once;
- interleaved ungrouped Records are not treated as group members and still play later;
- group completion marks only exact member indices visually presented;
- reducer still applies A/interleaved/B/interleaved/C in original order;
- no grouped member is visually replayed when its reducer cursor is reached;
- incomplete/malformed group falls back to sequential playback;
- Widget group rejection falls back to sequential playback;
- stale/duplicate group completion ignored;
- group timeout and Skip cleanly terminate exact group ownership.

### Native UI

- N Exhaust members start same tick;
- all start at exact confirmed RuntimeId positions;
- no member flashes to Hand row;
- all use generic Exhaust opacity behavior;
- N DrawPile members move concurrently toward DrawPile anchor;
- one-member Warcry Selection uses same grouped/generic transition engine;
- Warcry's later PlayArea→Exhaust is not a group member and reuses existing generic cleanup;
- Skip removes every group child and leaves no retained transient.

---

## 20. Manual PIE acceptance

After automated gates pass, user validation should cover only genuinely visual behavior.

### Multi-exhaust

Configure/use an existing Player Selection that exhausts at least two Hand cards.

Expected:

```text
select A/B/(C)
→ explicit Confirm
→ all selected cards begin disappearing together in Selection area
→ no card flashes back to Hand
→ total selected-card visual wait ≈ one Exhaust animation duration
→ later trigger/other presentation continues normally
→ input eventually restores
```

### Warcry regression

Expected:

```text
Draw presentation
→ Selection
→ explicit Confirm
→ selected card Hand→DrawPile generic movement
→ Warcry reappears/continues in PlayArea
→ ordinary generic PlayArea→Exhaust cleanup
```

No new Blueprint animation asset is expected by this design. If implementation later discovers a required Designer-backed surface, that is a separate explicit USER ACTION REQUIRED step.

---

## 21. Design decision summary

Chosen architecture:

```text
Selection decision
→ explicit Presentation group correlation
→ direct continuation Actions only
→ committed member Records
→ Controller-owned group discovery/preflight
→ parallel generic card-zone child animations
→ one completion boundary
→ original reducer order preserved
```

The key separation is:

```text
Selection owns source visual handoff
Controller owns group sequencing
Generic zone Presentation owns destination animation
Gameplay/Triggers keep their existing authoritative order
```
