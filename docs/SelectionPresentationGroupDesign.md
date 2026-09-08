# Selection Presentation Group + Parallel Animation Design

Date: **2026-09-09**

Status:

```text
DRAFT / REVIEW-ADJUSTED / SECOND-REVIEW BLOCKERS CLOSED IN DESIGN /
UPSTREAM CONTRACTS UPDATED / NO PRODUCTION GROUP CODE IMPLEMENTED /
NOT VALIDATED / NOT SEALED
```

Scope: replace the current one-Record-at-a-time visual treatment of a confirmed multi-card Selection with an explicit Selection Presentation Group. A validated multi-member group may co-present its committed selected-card destination facts while Gameplay, event dispatch, `PresentationSequence`, and reducer chronology remain unchanged.

Authoritative related contracts:

- `docs/CardSelectionPresentationConstraints.md`
- `docs/CardSelectionRefactorConstraints.md`
- `Source/SlayTheSpireDemo/Presentation/AGENTS.md`
- `Source/SlayTheSpireDemo/UI/AGENTS.md`

This revision incorporates both architecture reviews. The first review established:

1. visually consumed future members require suppression until their own reducer cursor;
2. non-contiguous group lookahead is an explicit visible-order exception;
3. Group playback inherits `UBattleHUDWidgetBase` exact-token/deferred-callback/cancellation hardening;
4. GroupId allocation is writer-scoped;
5. initial eligibility requires exactly one eligible member Record per selected runtime sequence;
6. one-member selections remain SingleRecord Controller playback initially.

The second review closes four further implementation blockers:

1. suppressed historical Hand cards retain their exact formal Widget/slot and use `Hidden`, never omitted/`Collapsed` in the first implementation;
2. group preflight rejects interleaving that directly touches a future group member even when chronological reducer dry-run succeeds;
3. Group timeout has one dedicated active-envelope reconciliation path and never masquerades as normal group/leader completion;
4. sequential degradation retains confirmed source handoff leases across Hand rebuilds until each exact SingleRecord transition actually accepts ownership.

No production implementation or validation is claimed by this document.

---

## 1. Problem

The current Controller kernel is one Record at a time:

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

For a confirmed multi-card Selection that exhausts `A, B, C`, this creates two visible problems:

1. total selected-card visual delay is approximately `N × animation duration`;
2. after A reduces, `RefreshHand()` may rebuild B/C at normal Hand positions before their own Records play, causing later cards to flash back to the Hand row.

A per-Record deferred/timing patch can hide one symptom but keeps the wrong playback unit, can stall later Presentation, and does not define robust fallback behavior.

The intended model is:

```text
Selection decision
→ one visual cohort when safe

committed Records
→ still reduced individually in original order
```

---

## 2. Non-negotiable invariants

### 2.1 Gameplay chronology is unchanged

The design MUST NOT change:

```text
SelectionResult canonical order
→ direct continuation Action order
→ Gameplay mutation order
→ BattleEvent dispatch order
→ Trigger reaction order
→ PresentationSequence assignment order
```

Presentation never changes Gameplay to make visuals simultaneous.

### 2.2 Reducer chronology is always committed order

Even when selected-card visuals are co-presented:

```text
reduce A
→ reduce/play chronological interleaved Records
→ reduce B
→ ...
→ reduce C
```

No future member is reduced early.

### 2.3 Visible playback normally follows committed order

Normal visible playback follows `PresentationSequence`.

The only exception is an explicit, complete, Controller-validated Presentation Group. It may look ahead into the already sealed immutable Envelope and co-present **only its own members**.

This exception MUST NOT:

- reduce future members early;
- consume or skip interleaved ungrouped Records;
- mark interleaved Records visually presented;
- infer membership from CardId, Effect type, destination, adjacency, click order or timing;
- allow Widget code to scan future Envelope Records;
- read mutable future Gameplay.

### 2.4 Group metadata is explicit committed Presentation metadata

A Record is grouped only when it carries a valid explicit `FPresentationGroupTag`.

Controller does not infer groups from consecutive `Hand→Exhaust` or any similar pattern.

### 2.5 Trigger reactions do not inherit Selection grouping

The ordinary `FPresentationRecordWriter` remains only the Resolution-recording capability.

```text
trigger Action inherits writer             = yes
trigger Action inherits Selection group    = no
```

Selection group context is Action-local and assigned only to direct continuation Actions created from that resolved Selection.

### 2.6 Visually consumed is not reducer-consumed

A future member may already be visually gone while still existing in the chronological `WorkingPresentationSnapshot`.

Such a member remains Presentation-suppressed until its own Record is reduced.

### 2.7 Formal Hand structure remains historical

Suppression is visual only. It MUST NOT alter the frozen Hand child/index structure in the first implementation.

### 2.8 Base Widget remains the hardened Controller-facing boundary

SingleRecord and SelectionGroup playback share one exact tracked playback-unit owner in `UBattleHUDWidgetBase`.

Concrete HUD code cannot bypass that wrapper or notify Controller directly.

### 2.9 Sequential degradation is a supported correctness path

A Group failure/rejection is not permission to return to the known broken “second card jumps back to normal Hand” path.

Confirmed source continuity must survive fallback.

### 2.10 No Effect/Card-specific presentation branching

Warcry, Burning Pact and future cards do not own group sequencing or destination animation.

---

## 3. High-level model

```text
Player explicitly confirms Selection
        ↓
Gameplay validates canonical SelectionResult
        ↓
Selection Presentation freezes source handoff leases
        ↓
SelectionRequestAction builds direct continuation batch
        ↓
writer-scoped allocation of GroupId
        ↓
Action-local Selection group context assigned to direct batch only
        ↓
Gameplay executes normally
        ↓
direct eligible CardZoneChanged Records may carry GroupTag
trigger/reaction Records stay ungrouped
        ↓
sealed immutable Envelope
        ↓
Controller reaches first tagged member
        ↓
validate membership + chronological reducer preflight
        ↓
validate future-member visual independence/interference
        ↓
if eligible multi-member group:
    Base Widget transactionally prepares N children
    → accepts exact Group playback unit
    → establish formal-member suppression
    → transfer source handoff ownership
    → start N children together
else:
    preserve handoff leases
    → sequential SingleRecord fallback
        ↓
Group visual success:
    child transients end
    VisuallyPresented marks retained
    future-member suppression retained
    reducer resumes chronologically
        ↓
exact member reduction:
    apply Record
    → release that member suppression
    → publish snapshot
```

A Selection Presentation Group is a visual cohort, not a Gameplay transaction.

---

## 4. Presentation group metadata

Conceptual types:

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

`FPresentationRecord` gains one group tag.

Group identity:

```text
(BattleId, ResolutionId, Kind, GroupId)
```

Rules:

```text
GroupId == 0
↔ ungrouped

ExpectedMemberCount > 0
for non-zero SelectionDestination metadata
```

`PresentationSequence` remains canonical Record/member chronology. No independent member-order field is introduced.

### 4.1 Initial eligible member shape

Initial grouped member:

```text
Record.Type = CardZoneChanged
FromZone = Hand
Group.Kind = SelectionDestination
```

Initial destination support:

- `ExhaustPile`;
- `DrawPile`;
- `DiscardPile` only when its generic Native Selection-origin transition is proven ready.

Mixed destinations are architecturally valid once each child style is supported.

---

## 5. GroupId allocation is writer-scoped only

`SelectionSource` is semantic metadata and is not unique identity.

Only the frozen writer capability exposes allocation:

```cpp
bool FPresentationRecordWriter::TryAllocatePresentationGroupId(
    int64& OutGroupId) const;
```

Conceptual call:

```text
Writer.TryAllocatePresentationGroupId
→ Recorder weak pointer
→ Recorder.AllocatePresentationGroupId(
      WriterBattleId,
      WriterResolutionId,
      OutGroupId)
→ IsWriterCurrentAndValid(...)
→ allocate from that exact active Resolution builder
```

The counter belongs inside the active builder:

```cpp
struct FActiveResolutionBuilder
{
    ...
    int64 NextPresentationGroupId = 1;
};
```

Consequences:

- GroupId has no Gameplay meaning;
- uniqueness is required only inside `(BattleId, ResolutionId)`;
- stale writers cannot allocate from a later Resolution;
- no-history/Presentation-unavailable allocation failure does not affect Gameplay;
- writer allocates identity but never implicitly carries active Selection membership into trigger Actions.

---

## 6. Selection correlation and direct Action context

### 6.1 Generic Selection boundary stays object-type neutral

`USelectionRequestAction` MUST NOT cast selected objects to `UCardInstance`.

After resolution it maps `Result.SelectedObjects` back to the frozen `Request.Candidates` and obtains canonical `RuntimeSequence` values.

For `UCurrentHandSelectionSource`:

```text
RuntimeSequence == CardRuntimeId
```

Other Selection domains may define other deterministic sequences.

### 6.2 Action-local context

Conceptually:

```cpp
struct FSelectionPresentationGroupContext
{
    EPresentationGroupKind Kind = EPresentationGroupKind::SelectionDestination;
    int64 GroupId = 0;
    int32 ExpectedMemberCount = 0;
    TArray<int32> SelectedRuntimeSequences;
};
```

For initial card Selection:

```text
ExpectedMemberCount = resolved SelectedObjects.Num()
```

The same context is assigned only to direct continuation Actions.

### 6.3 Trigger propagation remains writer-only

`BattleEventDispatcher` does not copy Selection group context.

```text
Selected Exhaust A
→ A direct Hand→Exhaust Record may be Group G
→ CardExhausted dispatch
→ trigger Action gets Writer
→ trigger Action does not get Group G
```

### 6.4 Concrete Action stamps only matching committed identity

A concrete card-zone Action may stamp a group tag only when:

```text
Action has valid SelectionDestination context
AND CommitResult.CardRuntimeId is in SelectedRuntimeSequences
AND direct Record shape is eligible
```

Unrelated Records emitted from the same Action remain ungrouped.

### 6.5 Exact-one-eligible-record-per-selected-member invariant

`ExpectedMemberCount = selected count` is valid only when each selected runtime sequence produces exactly one eligible direct group Record.

Examples:

```text
Selected A/B
A → one eligible Record
B → zero
→ incomplete group
→ sequential degradation
```

```text
Selected A
A → two matching eligible Records
→ duplicate/multiple member shape
→ sequential degradation
```

This feature does not claim arbitrary continuation shapes.

---

## 7. Complete-group discovery and eligibility

Controller operates only on the sealed Envelope.

At the first tagged Record, discover all Records with the exact group identity.

Co-presentation eligibility requires:

1. `ExpectedMemberCount > 1`;
2. discovered member count equals expected count;
3. every member repeats identical group identity and expected count;
4. every member has an initially supported `CardZoneChanged`-from-Hand shape;
5. member `PresentationSequence` values are unique and chronological;
6. exact member RuntimeIds are valid and unique;
7. every selected runtime sequence corresponds to exactly one eligible member;
8. no terminal Record belongs to the group;
9. all payloads pass existing frozen-record validity;
10. chronological reducer dry-run through the final member succeeds including all interleaved Records;
11. future-member interference preflight succeeds.

Any failure disables group playback for this group identity in the current Envelope:

```text
no suppression established
no VisuallyPresented mark
no handoff consumed
→ sequential SingleRecord path
```

Grouping degradation never requests Gameplay `ResolutionFault` by itself.

### 7.1 One-member metadata

A one-card Selection may still carry group metadata, but:

```text
ExpectedMemberCount <= 1
→ Controller does not enter SelectionGroup playback
→ SingleRecord path
```

Single and group cases later share the same generic card-transition child engine.

---

## 8. Non-contiguous groups and future-member interference

Group members may be non-contiguous because selected-card Actions can dispatch events that insert trigger reactions before the next selected-card Action.

Example:

```text
A Hand→Exhaust [G]
A-trigger Damage [ungrouped]
A-trigger Status [ungrouped]
B Hand→Exhaust [G]
B-trigger Draw [ungrouped]
C Hand→Exhaust [G]
```

Changing Gameplay/trigger order to make members contiguous is forbidden.

### 8.1 Controlled visual lookahead

When Group G passes all preflight, Controller may offer frozen A/B/C together.

```text
A/B/C destination visuals start together
```

Interleaved records are still played/reduced later when the chronological cursor reaches them.

### 8.2 Reducer dry-run alone is insufficient

A valid state chronology does not prove that future-member early visual consumption is safe.

Unsafe example:

```text
A Hand→Exhaust [G]
B Hand→Discard [ungrouped]
B Discard→Hand [ungrouped]
B Hand→Exhaust [G]
```

The chronological state can be perfectly valid, yet pre-hiding B before its interleaved movements would create contradictory visible behavior.

Therefore the entire group degrades to sequential playback.

### 8.3 Initial interference predicate

For each interleaved ungrouped Record between group leader and final member, determine the set of group RuntimeIds whose own member Record is still in the future at that chronological position.

If an interleaved Record directly acts on one of those future RuntimeIds, co-presentation is rejected.

At minimum initial exact-card interference includes:

```text
CardZoneChanged.Card.RuntimeId == future member
→ interference

CardPlayed.Card.RuntimeId == future member
→ interference
```

Future exact-card record types that can move, replace, transform, or otherwise affect a card's visible identity must either implement the same `TouchesFutureGroupMember` classification or conservatively disable group lookahead until classified.

Records that do not touch a future member identity do not disable grouping merely because they are interleaved:

```text
unrelated Damage
unrelated StatusChanged
EnergyChanged
another card's CardZoneChanged
```

### 8.4 Preflight result

Interference failure is Presentation degradation only:

```text
mark group disabled
→ preserve all source handoff leases
→ no suppression
→ sequential fallback
```

---

## 9. Four distinct visual lifetimes

These concepts MUST NOT be conflated.

### 9.1 Formal Hand slot lifetime

The formal Hand surface mirrors the chronological frozen Hand array.

While a suppressed group member still exists in `WorkingPresentationSnapshot.HandCards`:

```text
one frozen Hand entry
↔ one HB_Hand formal Widget at same index
```

The Widget remains in the layout:

```text
Visibility = Hidden
Input = disabled
```

First implementation explicitly forbids:

```text
skip creating suppressed child
remove child from HB_Hand
Visibility = Collapsed
```

because existing historical Hand lookup contracts require child count/index to match the frozen Hand.

If future architecture removes that index dependency, `Collapsed` may be reconsidered only with an explicit contract/test rewrite.

### 9.2 Confirmed source handoff lease

At explicit Confirm, Selection freezes per-runtime source information:

```cpp
struct FConfirmedCardVisualHandoff
{
    int32 RuntimeId = INDEX_NONE;
    FVector2D ConfirmedAbsoluteCenter;
    // optional frozen local transform/geometry data as needed
};
```

Conceptually owned as:

```text
RuntimeId → outstanding handoff lease
```

The lease persists across Hand rebuilds until an actual destination transition accepts ownership.

It is not group suppression.

### 9.3 Group suppression lifetime

Suppression exists only after an accepted multi-member Group playback unit transactionally owns continuity.

Conceptual exact ownership should be richer than bare RuntimeId, for example:

```cpp
struct FPresentationSuppressionKey
{
    int64 BattleId;
    int64 ResolutionId;
    int64 GroupId;
    int32 MemberRecordIndex;
    int32 RuntimeId;
};
```

Exact implementation shape may differ, but stale suppression cannot cross Battle/Resolution/Group boundaries.

### 9.4 Playback-unit lifetime

The active Controller/Widget visual owner is one exact unit:

```text
SingleRecord OR SelectionGroup
```

Playback-unit completion/timeout is separate from handoff and suppression lifetimes.

---

## 10. Suppression lifecycle

### 10.1 Why suppression exists

After A/B/C disappear together:

```text
group visual completes
→ reduce A
→ ApplyPresentationSnapshot
→ B/C still historically in Hand
```

Without suppression B/C would be rebuilt visibly.

### 10.2 Responsibility split

Recommended ownership:

```text
BattlePresentationController
→ owns exact visually-consumed/not-yet-reduced members

UBattleHUDViewModel
→ carries transient Presentation suppression identity through refreshes

UBattleHUDWidget / RefreshHand
→ preserves every formal Hand slot and applies Hidden/disabled state
```

Suppression is not stored in frozen `FPresentationStateSnapshot` and does not mutate Gameplay.

### 10.3 Transactional establishment

Before group acceptance, every child must prepare successfully.

No durable suppression and no handoff consumption occur during partial preparation.

After all children are valid and Base Widget accepts the exact Group unit:

```text
establish suppression for every member
→ transfer/consume every relevant handoff lease into durable group ownership
→ start all children
```

No partial group start is allowed.

### 10.4 Group visual success

At shared child completion:

```text
clean child transients
→ mark exact member indices VisuallyPresented
→ retain suppression for future members
→ notify Controller once with exact Group token
```

Child lifetime ends here; future-member suppression does not.

### 10.5 Exact member reduction

When chronological cursor later reaches a visually presented member:

```text
ApplyRecordToWorkingSnapshot(member)
→ release exact member suppression
→ publish/apply post-member snapshot
→ advance cursor
```

Release occurs after mutation and before/with the post-member publish so no refresh can recreate that member visibly.

Interleaved Records never release future-member suppression.

### 10.6 Global suppression cleanup

Clear all applicable suppression on:

- exact member reduction for that member;
- active-envelope timeout reconciliation;
- Skip/global collapse;
- Widget loss/replacement reconciliation;
- Envelope completion/replacement;
- battle reset/replacement;
- PresentationUnavailable;
- direct-baseline/no-history transition.

Group rejection/fallback before accepted ownership establishes no group suppression to clear.

---

## 11. Sequential fallback and handoff lease semantics

Sequential fallback must be visually correct even though it is slower.

### 11.1 Group validation/rejection cannot consume handoff

These operations are read/prepare-only:

```text
group discovery
membership validation
reducer dry-run
interference preflight
child 1 prepare
child 2 prepare
...
```

If any step fails before accepted group ownership:

```text
rollback all prepared transient children
restore formal visuals
consume zero handoff leases
establish zero group suppression
→ sequential fallback
```

### 11.2 Formal rebuild with outstanding handoff

If a sequential Record reduces A and rebuilds the historical Hand while B/C are still awaiting their own transitions:

```text
RefreshHand recreates formal slots in frozen order
→ B/C formal Widgets remain present at exact indexes
→ outstanding B/C handoff leases restore their confirmed Selection source transforms/positions
→ B/C stay visible in the Selection-origin position rather than snapping to normal Hand layout
```

This is generic source-handoff restoration, not an Effect special case.

### 11.3 Exact SingleRecord handoff consumption

For B:

```text
lookup exact B handoff
→ configure generic B destination child from confirmed source
→ generic transition successfully accepts exact SingleRecord playback ownership
→ consume only B handoff
```

If the SingleRecord transition itself declines/fails to start, B's handoff remains until normal fallback/reconciliation decides the visual lifecycle.

### 11.4 Required fallback example

```text
Confirm A/B/C
→ Group prepare child A succeeds
→ Group prepare child B fails
→ rollback A temporary preparation
→ no handoff consumed
→ no suppression

A SingleRecord Exhaust begins from confirmed A position
→ consume A handoff
→ reduce A / RefreshHand
→ B/C rebuild at retained confirmed positions

B SingleRecord Exhaust begins from confirmed B position
→ consume B handoff
...
```

No card may flash back to the normal Hand row during this path.

### 11.5 Relationship to rejected deferred patch

Forbidden as the primary concurrency solution:

```text
serial playback + per-card timer/deferred retry to fake batch behavior
```

Required for degradation correctness:

```text
stable confirmed handoff lease
→ generic handoff-aware Hand rebuild/source restoration
→ exact SingleRecord transition consumes lease only on successful ownership
```

These are different mechanisms. The latter is part of the shared Selection source-handoff contract, not a workaround for parallel timing.

---

## 12. Controller playback unit

Controller evolves from:

```text
active playback = one Record
```

to:

```text
active playback unit = SingleRecord OR SelectionGroup
```

Conceptual state:

```cpp
enum class EPresentationPlaybackUnitKind : uint8
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

`ActiveRecordIndex` remains chronological reducer cursor at all times.

### 12.1 Group preflight

Before offer:

1. discover exact member indices;
2. validate member shape/count/identity;
3. clone `WorkingPresentationSnapshot`;
4. dry-run every Record from cursor through final member in exact chronology;
5. include interleaved ungrouped Records;
6. run future-member interference preflight;
7. publish none of the dry-run state.

### 12.2 Group accepted

If hardened Base Widget wrapper returns true:

```text
active unit = SelectionGroup
bWaitingForCompletion = true
one exact group token
one timeout boundary
no reducer cursor movement yet
```

### 12.3 Normal group completion

On exact completion:

```text
cancel group timeout
→ mark only exact member indices VisuallyPresented
→ retain future-member suppression
→ clear active Group playback owner
→ resume reducer at original leader cursor
```

### 12.4 Already-VisuallyPresented member

When cursor reaches a member:

```text
skip visible Begin
→ apply Record chronologically
→ release exact suppression
→ publish snapshot
→ advance
```

Consecutive already-presented members may be drained synchronously if no interleaved Record needs visible playback.

### 12.5 Group disabled or Widget declines

```text
mark exact identity group-disabled for active Envelope
→ consume no handoff
→ establish no suppression
→ mark no VisuallyPresented
→ sequential SingleRecord path
```

---

## 13. Group timeout has one dedicated failure path

Current single-record timeout semantics cannot be copied blindly to Groups.

Normal group completion and timeout are different terminal states.

### 13.1 Forbidden timeout behavior

Do NOT:

```text
Group timeout
→ CompleteActiveRecord()
→ reduce only leader
→ clear all group suppression
→ continue normally
```

That would recreate future members and misrepresent the failed Group as successful playback.

### 13.2 Required exact timeout sequence

After verifying timeout token equals the exact active SelectionGroup unit:

```text
1. cancel timeout ownership
2. cancel exact tracked Base-Widget Group unit
3. concrete cancellation cleans every child transient
4. do not mark any new member VisuallyPresented from the timed-out unit
5. clear active Group playback ownership
6. atomically clear envelope-owned group suppression + confirmed handoff/transient ownership
7. reconcile current ActiveEnvelope directly to ActiveEnvelope.FinalSnapshot
8. mark current envelope Presentation-complete
9. preserve later PlaybackQueue entries
10. start next queued Envelope normally, or refresh bindings if caught up
```

### 13.3 Dedicated reconciliation helper

Implementation should use semantics equivalent to:

```text
ReconcileActiveEnvelopeAfterPlaybackUnitFailure()
```

It MUST NOT reuse a global `CollapseToEnvelope`/`ResetPlaybackState` path if that helper clears later queued Envelopes.

The operation is scoped to the current active Envelope.

### 13.4 Atomic HUD reconciliation

Do not broadcast an intermediate UI state between:

```text
clear failed-group suppression/handoff
and
apply ActiveEnvelope.FinalSnapshot
```

if that intermediate refresh could reveal a historical selected card for one frame.

ViewModel may require a batched/atomic transient-state + snapshot reconciliation API or equivalent notification suppression.

### 13.5 Timeout remains Presentation-only

Group timeout never requests Gameplay `ResolutionFault` by itself.

Gameplay already committed independently.

---

## 14. Base Widget unified playback-unit hardening

`PlayPresentationGroup()` is a Controller-facing `UBattleHUDWidgetBase` wrapper symmetric with `PlayPresentationRecord()`.

Conceptual wrappers:

```cpp
bool PlayPresentationRecord(
    const FPresentationRecord& Record,
    const FPresentationPlaybackToken& Token);

bool PlayPresentationGroup(
    const FPresentationPlaybackGroup& Group,
    const FPresentationPlaybackToken& Token);
```

### 14.1 One tracked owner

Do not maintain independent tracked Record and Group tokens.

Conceptually:

```cpp
struct FTrackedPresentationPlayback
{
    EPresentationPlaybackUnitKind Kind = EPresentationPlaybackUnitKind::None;
    FPresentationPlaybackToken Token;
};
```

### 14.2 Token identity

Extend token conceptually:

```cpp
EPresentationPlaybackUnitKind PlaybackUnitKind;
int64 PresentationGroupId = 0;
```

SingleRecord:

```text
Kind = SingleRecord
GroupId = 0
PresentationSequence = record sequence
```

SelectionGroup:

```text
Kind = SelectionGroup
GroupId = exact group id
PresentationSequence = first member sequence
```

Token equality includes kind/group identity as well as battle/resolution/sequence/generation.

### 14.3 Existing hardening applies unchanged

Both units preserve:

- track exact owner before concrete playback;
- `true` only when async playback actually starts;
- deferred completion forwarding to avoid Controller re-entry;
- stale/duplicate/old-Battle/post-Skip callback rejection;
- exact cancellation;
- exact timeout token/generation;
- replacement/destruction cannot cancel newer owner;
- concrete HUD never calls Controller directly.

Base group implementation may return false, causing safe sequential degradation.

---

## 15. Generic card-transition child engine

Current Native state is single-instance and cannot represent N parallel cards safely.

Extract per-child state:

```cpp
struct FNativeCardTransitionInstance
{
    int32 RuntimeId = INDEX_NONE;
    ECardZone FromZone;
    ECardZone ToZone;

    TObjectPtr<UBattleCardWidget> MovingVisual = nullptr;
    TWeakObjectPtr<UBattleCardWidget> HistoricalFormalCard;

    FVector2D StartTranslation;
    FVector2D EndTranslation;
    float StartScale = 1.0f;
    float EndScale = 1.0f;
    float StartOpacity = 1.0f;
    float EndOpacity = 1.0f;
    float ElapsedSeconds = 0.0f;
};
```

One builder chooses style from committed zone facts plus optional confirmed source handoff.

### 15.1 Hand→Exhaust

```text
source = confirmed Selection source when available
end position = same source
opacity → 0
```

Grouped case uses transient child visual while formal historical slot remains `Hidden` under suppression.

Same generic Exhaust style; no Burning Pact-specific animation.

### 15.2 Hand→DrawPile

```text
source = confirmed Selection source
end = DrawPile visual anchor
visible movement toward DrawPile
```

Selection-subclass-only transfer state should migrate into this engine.

### 15.3 Hand→DiscardPile

Use generic discard destination behavior with confirmed source when supported.

### 15.4 Single and Group share engine

```text
SingleRecord CardZoneChanged
→ 1 child

SelectionGroup
→ N children
```

No duplicate destination implementation.

---

## 16. Group transaction and Selection handoff transfer

Selection layer owns interaction and outstanding confirmed source leases; it does not own destination style.

For proposed multi-member start:

1. every member must have a trustworthy source handoff when Selection-origin continuity requires one;
2. prepare all generic transition children transactionally without consuming leases;
3. if any child fails, destroy/rollback all prepared transients and restore formal visual state;
4. return false to Controller;
5. only after every child is prepared and hardened Group ownership is accepted establish suppression;
6. only then transfer/consume all relevant leases into group child/suppression ownership;
7. begin all children in the same Native tick.

No partial Group visual ownership is permitted.

---

## 17. Group child completion

Initial children use the existing uniform card transition duration.

```text
Begin accepted Group
→ all elapsed = 0 same Native tick
→ tick together
→ one shared duration
→ clean all child transients
→ keep future-member suppression
→ notify exact Group token once
```

Total selected-card visual wait is approximately one transition duration rather than `N × duration`.

If future durations differ, group completes after the maximum child duration; not required initially.

---

## 18. Warcry one-member behavior

Warcry may receive correlation metadata with:

```text
ExpectedMemberCount = 1
```

But first implementation remains:

```text
Warcry Selection Confirm
→ tagged Hand→DrawPile Record
→ SingleRecord Controller path
→ generic child engine with 1 child
→ consume exact handoff only when transition begins
→ reducer continues
→ Warcry PlayArea→Exhaust remains ungrouped
→ existing generic played-card Exhaust cleanup
```

No Warcry-specific animation.

---

## 19. Multi-exhaust behavior

For A/B/C:

```text
Confirm A/B/C
→ three outstanding source handoff leases
→ one SelectionDestination group correlation G
```

When G is complete, non-interfered and accepted:

```text
A fade ┐
B fade ├─ start together at confirmed Selection positions
C fade ┘
```

At visual completion:

```text
transients gone
A/B/C marked visually presented
future members retain Hidden formal slots under suppression
→ chronological reducer/interleaved presentation continues
```

If G is incomplete/interfered/rejected/preparation-failed:

```text
all leases remain
→ sequential A/B/C
→ each formal rebuild restores remaining confirmed source positions
→ each exact SingleRecord consumes only its own lease on successful Begin
```

---

## 20. Failure and degradation matrix

These are Presentation-only failures and do not fault Gameplay by themselves:

- group allocation unavailable/stale writer;
- incomplete group;
- 0/2 eligible Records for selected member;
- unsupported destination;
- chronological preflight failure for grouping eligibility;
- future-member interference;
- Base Widget group rejection;
- child N preparation failure;
- Group timeout;
- Skip;
- Widget replacement/loss;
- PresentationUnavailable/no-history.

Behavior:

```text
before accepted Group ownership
→ sequential fallback with handoff leases retained

after accepted Group ownership + normal completion
→ VisuallyPresented + suppression lifecycle

after accepted Group ownership + timeout/fatal playback-unit failure
→ exact cancel + active-envelope final-snapshot reconciliation
```

A malformed committed Record that violates pre-existing envelope validity may still use existing Presentation collapse behavior, but Presentation never rewrites Gameplay.

---

## 21. Explicitly rejected designs

### 21.1 Remove or Collapse suppressed historical Hand child

Not allowed in initial implementation.

### 21.2 Dry-run-only lookahead validation

Reducer success without future-member interference analysis is insufficient.

### 21.3 Treat Group timeout as ordinary completion

No leader-only reduce after timed-out parallel playback.

### 21.4 Consume handoff during partial Group preparation

No handoff is consumed until durable accepted ownership exists.

### 21.5 Broken sequential fallback

Sequential fallback may be slower but must preserve confirmed source continuity.

### 21.6 Ad-hoc serial deferred timing as parallel solution

Do not simulate a group by waiting/repositioning one Record at a time.

Generic handoff restoration for sequential degradation is allowed and required; timer/deferred retry as the concurrency architecture is not.

### 21.7 Effect/Card special cases

No CardId/Effect branching for group or transition behavior.

### 21.8 Gameplay batching for visuals

Do not replace per-card Gameplay Actions/events with bulk mutation for animation convenience.

### 21.9 Writer-carried implicit active group

Writer allocates GroupId but does not automatically propagate membership.

### 21.10 Widget-owned Envelope scanning

Group discovery belongs to Controller.

### 21.11 Independent concrete-HUD Group token owner

Base Widget owns one Record-or-Group tracked unit.

### 21.12 Force one-member case through Group kernel

Not required initially.

---

## 22. Proposed implementation order

Production implementation should remain staged and compilable/testable.

### G1 — metadata and writer-scoped correlation

- add group types to Presentation records;
- add writer-scoped GroupId allocation;
- counter inside active Resolution builder;
- stale writer rejection;
- add optional Action-local Selection context;
- generic mapping from resolved candidate objects to runtime sequences;
- direct selected-card Actions stamp matching group metadata only;
- trigger reactions remain ungrouped;
- no visible behavior change.

G1 tests include 0/2 eligible-member degradation.

### G2 — Controller discovery, chronological/interference preflight, suppression model

- exact group discovery/validation;
- `ExpectedMemberCount > 1` requirement;
- arbitrary-snapshot chronological dry-run;
- `TouchesFutureGroupMember` interference predicate;
- group-disabled state for ineligible identities;
- transient suppression ownership model;
- ViewModel suppression identity;
- `RefreshHand` preserves one formal child per historical Hand entry;
- suppressed child uses `Hidden` and disabled input;
- exact release/global cleanup contracts.

Group playback may remain disabled until G3/G4, but validation/suppression structures must compile and test independently.

### G3 — Base Widget playback unit + timeout reconciliation

- extend token with unit kind/group id;
- one tracked playback owner;
- `PlayPresentationGroup` Base wrapper;
- shared deferred completion/cancel/stale hardening;
- Controller dedicated active-envelope playback-unit-failure reconciliation;
- Group timeout exact cancel;
- atomic transient cleanup + ActiveEnvelope FinalSnapshot application;
- preserve later PlaybackQueue entries;
- Base default group rejection remains sequential.

### G4 — generic N-child transition engine

- extract one-card zone transition into per-child state;
- SingleRecord uses one child;
- migrate Hand→DrawPile into generic engine;
- N-child parallel playback support;
- destination styles stay zone-driven.

### G5 — Selection handoff leases, fallback correctness, parallel enable

- formal `RuntimeId → confirmed source handoff lease` lifecycle;
- Hand rebuild restores outstanding lease transform without changing slot/index;
- SingleRecord consumes lease only after exact transition Begin succeeds;
- Group preparation is transactional and read-only with respect to lease ownership;
- accepted Group transfers all leases after durable suppression established;
- child N failure rolls back without consuming any lease;
- enable multi-exhaust parallel playback.

### G6 — cleanup/docs/validation

- delete superseded Selection-specific transfer state only after generic equivalence;
- update execution/status docs;
- run focused automation;
- user PIE validates concurrency/no-flicker/Warcry.

Do not combine stages into one large patch unless a real compile dependency requires it.

---

## 23. Automated acceptance plan

### 23.1 G1 correlation/allocation

- stale writer cannot allocate GroupId from a newer Resolution;
- two Selections in one Resolution get distinct IDs;
- IDs may restart in another Resolution because ResolutionId scopes identity;
- canonical candidate sequence, not click order, defines membership;
- direct eligible selected-card Records share group identity;
- trigger Records remain ungrouped;
- no-history leaves Gameplay unchanged;
- selected count 2 / only 1 eligible member → group invalid;
- selected count 1 / 2 matching members → group invalid.

### 23.2 G2 discovery/interference/formal suppression structure

- complete contiguous 3-member group validates;
- non-contiguous unrelated interleaving validates;
- chronological dry-run includes interleaved Records;
- interleaved `CardZoneChanged` touching future member disables group;
- interleaved `CardPlayed` touching future member disables group;
- example future member leaves and returns Hand → dry-run valid but interference disables group;
- unrelated Damage/Status/Energy does not disable group;
- `ExpectedMemberCount <= 1` not offered as group playback;
- suppressed still-historical member remains one formal child at exact index;
- suppressed visibility is `Hidden`, never `Collapsed`;
- suppressed member cannot submit input;
- exact historical Hand child count remains equal to frozen Hand count;
- suppression release occurs only on exact member reducer consumption/global reconciliation.

### 23.3 G3 playback-unit/timeout

- Record and Group wrappers share one tracked owner;
- exact Group completion succeeds once;
- stale/duplicate Group callback ignored;
- stale Group callback cannot complete newer SingleRecord;
- stale Single callback cannot complete newer Group;
- synchronous concrete completion is deferred;
- exact cancel only targets current unit;
- Widget replacement cannot affect newer owner;
- Group Base rejection enters sequential path;
- Group timeout never invokes leader-only normal completion;
- Group timeout cancels every child;
- Group timeout marks no new member VisuallyPresented;
- timeout atomically clears group transient/suppression state and applies ActiveEnvelope.FinalSnapshot;
- queued later Envelope remains queued and begins afterward;
- no one-frame suppressed-card reappearance during timeout reconciliation.

### 23.4 G4/G5 generic engine + handoff/fallback

- N Exhaust children begin same Native tick;
- exact confirmed RuntimeId positions are used;
- all use generic Exhaust style;
- future formal members remain `Hidden` after child cleanup until reducer consumption;
- no flash to Hand through interleaved presentation;
- SingleRecord Warcry uses same generic child builder;
- Warcry cleanup remains ordinary PlayArea→Exhaust;
- outstanding B/C handoff survives A reducer/HUD rebuild in sequential mode;
- rebuilt B/C formal Widgets remain at confirmed Selection positions with preserved formal slots;
- SingleRecord B consumes only B lease after successful Begin;
- Group child 1 prepares, child 2 fails → all temporary children rollback;
- same preparation failure consumes zero leases and establishes zero suppression;
- fallback then presents A/B/C sequentially from exact confirmed positions;
- if a SingleRecord transition fails to accept, its lease is not prematurely consumed;
- Skip/reconciliation leaves no stale child, suppression or handoff.

---

## 24. Manual PIE acceptance

Manual PIE is required after automated contracts pass.

### 24.1 Multi-exhaust parallel success

```text
select A/B/(C)
→ explicit Confirm
→ all selected cards begin disappearing together at Selection positions
→ formal Hand layout does not visibly pull any card back
→ total selected-card wait ≈ one Exhaust duration
→ interleaved unrelated trigger presentation continues chronologically afterward
→ input restores
```

### 24.2 Sequential degradation visual correctness

Force/use a controlled test path where Group cannot be accepted but Selection handoff is valid.

Expected:

```text
A disappears from confirmed Selection position
→ B/C remain visually at confirmed Selection positions across Hand rebuild
→ B disappears from confirmed Selection position
→ C likewise
→ no card snaps to normal Hand row
```

This path may take `N × duration`; correctness is required even when concurrency is unavailable.

### 24.3 Warcry regression

```text
Draw presentation
→ Selection
→ explicit Confirm
→ selected card generic Hand→DrawPile movement
→ Warcry visible/available in PlayArea
→ ordinary generic PlayArea→Exhaust cleanup
```

Warcry does not require Group playback.

No new Blueprint animation asset is expected. Any newly discovered Designer-backed requirement is a separate explicit user action.

---

## 25. Initial implementation impact boundaries

Expected source areas:

```text
PresentationTypes / Recorder / Writer
BattleAction optional Presentation correlation metadata
SelectionRequestAction correlation creation
eligible selected-card zone Actions stamping tags
BattlePresentationController group discovery/preflight/interference/suppression/timeout reconcile
BattleHUDViewModel transient suppression + handoff-aware transient presentation state as appropriate
BattleHUDWidgetBase playback-unit wrapper
BattleHUDWidget formal Hand Hidden-slot rendering + generic child engine
BattleHUDSelectionWidget confirmed source handoff lease provider
focused Automation tests
```

Not authorized:

```text
Card/Effect-specific animation logic
Gameplay bulk mutation changes
Trigger ordering changes
production .uasset/.umap changes
Legacy HUD changes
```

---

## 26. Responsibility summary

```text
Gameplay
    authoritative mutation/event order

Selection interaction
    confirm intent + exact selected RuntimeIds
    confirmed source handoff leases

Presentation Recorder/Record
    explicit immutable group correlation

BattlePresentationController
    group discovery
    chronological dry-run
    future-member interference preflight
    visible-order exception
    reducer chronology
    VisuallyPresented state
    suppression ownership
    active-envelope timeout/failure reconciliation

BattleHUDViewModel
    transient suppression/handoff-visible state across formal refresh as needed

UBattleHUDWidgetBase
    one hardened Record-or-Group playback-unit owner

UBattleHUDWidget
    exact formal Hand slot preservation
    Hidden suppression rendering
    generic card-zone child engine

UBattleHUDSelectionWidget
    Selection UI + confirmed source handoff provider
    no destination-specific animation ownership

Effect/CardId
    no group/transition sequencing ownership
```

The four visual lifetimes are deliberately distinct:

```text
Formal Hand slot
    lasts while chronological frozen Hand contains member

Confirmed handoff lease
    lasts from Confirm until actual transition accepts ownership

Group suppression
    lasts from accepted group ownership until exact reducer consumption/global reconcile

Playback unit
    lasts from Begin until normal exact completion or dedicated failure/timeout reconciliation
```

This design is now ready for implementation review at **Stage G1**. No Build, Automation, or PIE gate is passed by these documentation changes.
