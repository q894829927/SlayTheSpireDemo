# Selection Presentation Group + Parallel Animation Design

Date: **2026-09-09**

Status:

```text
DRAFT / REVIEW-ADJUSTED / VISUAL-OWNERSHIP MODEL ADOPTED /
NO PRODUCTION GROUP CODE IMPLEMENTED / NOT VALIDATED / NOT SEALED
```

Scope: replace one-Record-at-a-time selected-card Presentation with an explicit Selection Presentation Group and a stable SelectionArea visual-ownership model. A validated multi-member group may co-present its committed selected-card destination facts while Gameplay, event dispatch, `PresentationSequence`, and reducer chronology remain unchanged.

Authoritative related contracts:

- `docs/CardSelectionPresentationConstraints.md`
- `docs/CardSelectionRefactorConstraints.md`
- `Source/SlayTheSpireDemo/Presentation/AGENTS.md`
- `Source/SlayTheSpireDemo/UI/AGENTS.md`

This revision preserves the previously accepted group/reducer/token/interference/timeout rules, but replaces the earlier UI continuity model based primarily on `confirmed-position handoff lease + suppression + RefreshHand transform restoration` with an explicit Presentation ownership state machine:

```text
Hand
→ SelectionArea
→ Transition
→ ConsumedPendingReducer
→ Done
```

The older terms remain useful only as compatibility descriptions:

- “suppressed from Hand” means the exact RuntimeId currently has a Presentation owner other than `Hand`;
- “confirmed handoff” is represented primarily by the stable visible SelectionArea card visual itself rather than a position-only lease that must later reconstruct the card after Hand rebuilds.

No Gameplay `SelectionZone` is introduced. Gameplay continues to treat the card as belonging to `Hand` until the authored continuation actually commits a zone change.

---

## 1. Target behavior

For a confirmed multi-card Selection such as A/B/C:

```text
player selects A/B/C
→ A/B/C visibly leave normal Hand presentation and live in SelectionArea
→ Confirm
→ if safe, A/B/C destination transitions begin together
→ total wait ≈ one card-transition duration
→ Gameplay/trigger/reducer order remains unchanged
→ already consumed future members never flash back to Hand
```

If co-presentation is unsafe or unavailable:

```text
A/B/C remain owned by SelectionArea
→ A plays sequentially
→ Hand snapshot may rebuild
→ B/C still remain visibly in SelectionArea
→ B plays
→ C plays
```

The sequential fallback is slower but must still be visually correct.

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

Presentation never mutates Gameplay to make animations simultaneous.

### 2.2 Reducer chronology is always committed order

Even when selected-card visuals are co-presented:

```text
reduce A
→ play/reduce chronological interleaved Records
→ reduce B
→ ...
→ reduce C
```

No future member is reduced early.

### 2.3 Visible playback normally follows committed order

Normal visible playback follows `PresentationSequence`.

The only exception is an explicit, complete, Controller-validated Presentation Group. It may look ahead into the already sealed immutable Envelope and co-present only its own members.

The exception MUST NOT:

- reduce future members early;
- consume or skip interleaved ungrouped Records;
- mark interleaved Records visually presented;
- infer group membership from CardId, Effect type, destination, adjacency, timing or click order;
- let Widget code scan future Envelope Records;
- read mutable future Gameplay.

### 2.4 One exact RuntimeId has one visible Presentation owner

For the selected-card visual lifecycle, exactly one surface owns the visible card at a time.

Conceptually:

```cpp
enum class ECardPresentationOwner : uint8
{
    Hand,
    SelectionArea,
    Transition,
    ConsumedPendingReducer
};
```

This is Presentation-only state. It is not `ECardZone` and has no Gameplay meaning.

### 2.5 No Effect/Card-specific animation ownership

Warcry, Burning Pact and future cards do not own SelectionArea or group playback.

Responsibilities remain:

```text
Gameplay
    authoritative card zone / mutation / trigger order

Selection Presentation
    player selection + SelectionArea ownership

Controller
    group validation / lookahead / reducer chronology

Generic card transition engine
    destination animation
```

---

## 3. Gameplay Zone and Presentation Owner are separate concepts

A selected card may simultaneously be:

```text
Gameplay Zone       = Hand
Presentation Owner  = SelectionArea
```

This is intentional.

Selecting a card MUST NOT perform:

```text
Hand → GameplaySelectionZone
```

because that would change authoritative Hand semantics before confirmation and would require Gameplay rollback on deselect/cancel.

The initial implementation therefore has no new Gameplay zone or authoritative mutation for SelectionArea.

---

## 4. SelectionArea visual ownership lifecycle

### 4.1 Normal Hand state

Before selection:

```text
Gameplay Hand contains A
Presentation Owner(A) = Hand
```

The normal formal Hand Widget is visible and interactive according to current input rules.

### 4.2 Player selects A

On a legal local Selection click:

```text
Presentation Owner(A): Hand → SelectionArea
```

Gameplay remains unchanged.

The visual result is:

```text
HB_Hand:
[A hidden structural slot] [other Hand cards]

SelectionArea:
[A visible selection visual]
```

### 4.3 Player deselects A

While Selection is still pending and policy allows deselection:

```text
Presentation Owner(A): SelectionArea → Hand
```

The SelectionArea visual is removed/released and the exact formal Hand Widget becomes visible again.

No Gameplay mutation occurs.

### 4.4 Player confirms

Confirm freezes the selected RuntimeId set but does not immediately destroy SelectionArea visuals.

```text
A/B/C stay visibly owned by SelectionArea
→ authoritative SelectionResult submits
→ committed destination Records later determine where each card goes
```

This is a key rule: closing the interactive overlay does not mean throwing away the selected-card visual objects.

### 4.5 Destination transition begins

When a committed eligible `CardZoneChanged` Record is actually accepted for visible playback:

```text
Presentation Owner(RuntimeId): SelectionArea → Transition
```

The generic transition engine takes ownership of the existing SelectionArea visual or an equivalent stable transfer object without reconstructing the source from the Hand row.

### 4.6 Transition finishes before reducer reaches the Record

For a grouped future member B/C:

```text
Transition → ConsumedPendingReducer
```

No visible card remains, but Hand still has no right to display that RuntimeId.

### 4.7 Reducer consumes exact member

When the chronological reducer reaches that member's committed zone change:

```text
ApplyRecordToWorkingSnapshot(member)
→ remove exact Presentation ownership entry
→ Done
```

At that point the authoritative historical Hand no longer contains the card, so no separate hiding state is required.

---

## 5. Formal Hand slot stability

Current historical Hand lookup depends on exact child count and index correspondence. Therefore SelectionArea ownership does not remove the formal structural Hand slot.

While a selected RuntimeId is still present in `WorkingPresentationSnapshot.HandCards`:

```text
one frozen Hand entry
↔ one formal HB_Hand card Widget at same index
```

If `PresentationOwner(RuntimeId) != Hand`, the formal Widget:

```text
Visibility = Hidden
Input = disabled
```

Initial implementation explicitly forbids:

```text
skip creating the formal child
remove the child from HB_Hand
Visibility = Collapsed
```

This preserves current child-count/index contracts.

The important distinction is:

```text
formal Hand Widget = structural historical slot
SelectionArea visual = visible Presentation owner
```

A RuntimeId may therefore temporarily have two Widget objects but only one visible card presentation:

```text
formal Hand Widget      = Hidden structural placeholder
SelectionArea Widget    = visible owner
```

No duplicate visible representation is allowed.

---

## 6. SelectionArea visual identity

SelectionArea ownership is keyed by exact RuntimeId.

Conceptually:

```cpp
struct FSelectionAreaCardVisual
{
    int32 RuntimeId = INDEX_NONE;
    TObjectPtr<UBattleCardWidget> VisibleCardWidget = nullptr;
};
```

The design does not require this exact type, but requires these semantics:

- RuntimeId is exact and unique;
- CardId/Hand index/click order are not identity;
- the visible SelectionArea visual survives normal Hand rebuilds;
- its current screen-space position is naturally available because the visual itself still exists;
- Confirm freezes ownership, not merely a pair of coordinates.

For destination playback, prefer transferring/reparenting the stable visible SelectionArea visual into an animation layer when practical. If implementation must create a transition copy, the copy must be transactionally created from the exact SelectionArea owner before the visible owner is released.

---

## 7. Group metadata

Grouping remains explicit committed Presentation metadata.

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

Group identity:

```text
(BattleId, ResolutionId, Kind, GroupId)
```

No separate member-order field is required. `PresentationSequence` remains canonical committed order.

Initial eligible member shape:

```text
Record.Type = CardZoneChanged
FromZone = Hand
Group.Kind = SelectionDestination
```

Initial destination support:

- ExhaustPile;
- DrawPile;
- DiscardPile only when the generic Native destination path is ready.

---

## 8. Writer-scoped GroupId allocation

GroupId allocation is exposed only through the frozen writer capability:

```cpp
bool FPresentationRecordWriter::TryAllocatePresentationGroupId(
    int64& OutGroupId) const;
```

Conceptual flow:

```text
Writer
→ Recorder with WriterBattleId + WriterResolutionId
→ IsWriterCurrentAndValid
→ active Resolution builder allocates GroupId
```

The group counter belongs to the active Resolution builder, not battle-global mutable state.

A stale writer cannot allocate against a newer Resolution.

The writer allocates identity only. It does not carry active group membership into trigger Actions.

---

## 9. Selection correlation and direct continuation Actions

`USelectionRequestAction` remains object-type neutral.

After resolution it maps selected objects back to frozen candidates and obtains canonical selected `RuntimeSequence` values.

For current-Hand candidates:

```text
RuntimeSequence == exact CardRuntimeId
```

If GroupId allocation succeeds, direct continuation Actions receive an Action-local Selection group context conceptually containing:

```cpp
struct FSelectionPresentationGroupContext
{
    EPresentationGroupKind Kind = EPresentationGroupKind::SelectionDestination;
    int64 GroupId = 0;
    int32 ExpectedMemberCount = 0;
    TArray<int32> SelectedRuntimeSequences;
};
```

Trigger reactions inherit the ordinary `FPresentationRecordWriter` but MUST NOT inherit this Selection group context.

A concrete card-zone Action may stamp only a direct matching committed Record whose `CardRuntimeId` is one of the selected runtime sequences.

---

## 10. Exact-one-member eligibility

For the initial implementation:

```text
ExpectedMemberCount = resolved selected count
```

is valid only if each selected RuntimeSequence produces exactly one eligible direct committed group member.

Examples:

```text
Selected A/B
A → one eligible Record
B → zero
→ group invalid, sequential
```

```text
Selected A
A → two matching eligible Records
→ group invalid, sequential
```

`ExpectedMemberCount <= 1` may retain metadata for correlation but does not enter the Group Controller playback path initially.

---

## 11. Group discovery and preflight

Controller operates only on the sealed immutable Envelope.

At the first tagged member, complete-group co-presentation requires:

1. `ExpectedMemberCount > 1`;
2. discovered member count equals expected count;
3. identical group identity/expected count on all members;
4. eligible `CardZoneChanged`-from-Hand member shapes;
5. unique chronological `PresentationSequence` values;
6. exact unique RuntimeIds;
7. exactly one eligible member per selected RuntimeSequence;
8. no terminal Record in the group;
9. normal frozen-record validity;
10. chronological reducer dry-run through the final member succeeds;
11. future-member interference preflight succeeds;
12. every member that requires Selection continuity still has an exact visible/consumed Presentation ownership state consistent with the proposed playback.

Any failure disables co-presentation for that group identity and falls back sequentially without Gameplay fault.

---

## 12. Future-member interference

Reducer validity alone does not prove early visual consumption is safe.

Unsafe example:

```text
A Hand→Exhaust [G]
B Hand→Discard [ungrouped]
B Discard→Hand [ungrouped]
B Hand→Exhaust [G]
```

Although chronological state may be valid, B cannot be co-presented with A because interleaved Records visibly operate on B before B's own group Record.

Initial interference rule:

For each interleaved ungrouped Record before a future member's own reducer position, if that Record directly acts on the same RuntimeId, the group degrades to sequential.

At minimum:

```text
CardZoneChanged touching future RuntimeId → interference
CardPlayed touching future RuntimeId      → interference
```

Future exact-card Record types must opt into the same classification or conservatively disable lookahead until classified.

Unrelated Damage/Status/Energy/other-card records do not disable grouping merely because they are interleaved.

---

## 13. Controller playback unit

Controller evolves from:

```text
active playback = one Record
```

to:

```text
active playback unit = SingleRecord OR SelectionGroup
```

`ActiveRecordIndex` remains the chronological reducer cursor.

For a valid Group:

```text
Controller discovers members
→ preflights chronology + interference
→ offers frozen group to hardened Base Widget wrapper
→ no reducer cursor movement at Begin
```

On normal group visual completion:

```text
mark exact group member indices VisuallyPresented
→ retain future-member owner = ConsumedPendingReducer
→ resume reducer at original leader cursor
```

When reducer later reaches an already visually presented member:

```text
skip visible Begin
→ ApplyRecordToWorkingSnapshot
→ clear exact Presentation ownership entry
→ publish snapshot
→ advance
```

Interleaved Records still receive their normal visible playback.

---

## 14. Base Widget playback-unit hardening

`PlayPresentationGroup()` must be a Controller-facing `UBattleHUDWidgetBase` wrapper symmetric with `PlayPresentationRecord()`.

Record and Group share one tracked playback owner.

Conceptually:

```cpp
struct FTrackedPresentationPlayback
{
    EPresentationPlaybackUnitKind Kind;
    FPresentationPlaybackToken Token;
};
```

Group playback inherits the existing hardening:

- exact token tracked before concrete Begin;
- synchronous concrete completion is deferred before Controller sequencing resumes;
- stale/duplicate/old-battle callbacks ignored;
- timeout bound to exact token/generation;
- exact cancellation only;
- Widget replacement/destruction cannot cancel newer ownership;
- concrete HUD never notifies Controller directly.

Token identity should distinguish SingleRecord vs Group and include exact GroupId for Group playback.

---

## 15. Generic card-transition engine

SingleRecord and Group playback share one generic per-child card transition implementation.

Conceptual child:

```cpp
struct FNativeCardTransitionInstance
{
    int32 RuntimeId = INDEX_NONE;
    ECardZone FromZone;
    ECardZone ToZone;
    TObjectPtr<UBattleCardWidget> MovingVisual = nullptr;
    float ElapsedSeconds = 0.0f;
    ...
};
```

Source visual resolution:

```text
if owner == SelectionArea
→ take exact SelectionArea visual as source

if owner == Hand
→ use exact historical Hand visual

if owner == ConsumedPendingReducer
→ visual playback must not begin again
```

Destination style comes only from committed zone facts.

### 15.1 Hand/SelectionArea → Exhaust

```text
source = current visible owner position
opacity → 0
```

### 15.2 Hand/SelectionArea → DrawPile

```text
source = current visible owner position
→ visible movement to DrawPile anchor
```

### 15.3 Hand/SelectionArea → DiscardPile

Use the generic discard destination style when supported.

No Burning Pact/Warcry-specific animation branch is allowed.

---

## 16. Group transactional visual transfer

A Group start is all-or-nothing.

Before accepting visible ownership transfer:

1. require exact member Records;
2. require valid Presentation ownership for every member;
3. prepare every child transition;
4. validate every transition source/target;
5. do not release any SelectionArea visual during partial preparation.

If child N fails preparation:

```text
rollback all prepared child transients
→ SelectionArea keeps A/B/C visible ownership
→ formal Hand slots remain Hidden
→ no VisuallyPresented member
→ sequential fallback
```

Only after every child is valid and the Base Widget accepts the Group unit:

```text
SelectionArea → Transition for every member
→ all children begin in same Native tick
```

This replaces the old requirement to preserve a position-only handoff lease across partial group preparation.

---

## 17. Sequential fallback under SelectionArea ownership

Sequential fallback requires no source-position reconstruction.

Example:

```text
SelectionArea owns [A][B][C]
→ Group rejected
```

Then:

```text
A SingleRecord accepts
→ owner A: SelectionArea → Transition
→ A animation completes
→ reduce A

RefreshHand may rebuild historical B/C slots
→ B/C formal slots remain Hidden
→ B/C visible SelectionArea widgets are unchanged

B SingleRecord accepts
→ owner B: SelectionArea → Transition
...
```

No `RefreshHand restore confirmed transform` step is required because B/C never returned to Hand presentation ownership.

This is the main simplification introduced by the visual-ownership model.

---

## 18. ConsumedPendingReducer replaces ad-hoc suppression as primary model

After A/B/C grouped destination animations finish together:

```text
A owner = ConsumedPendingReducer
B owner = ConsumedPendingReducer
C owner = ConsumedPendingReducer
```

There is no visible SelectionArea or Transition card anymore.

However formal Hand widgets for historically still-present B/C remain:

```text
Hidden + input disabled
```

because `owner != Hand`.

Thus the prior “suppression” requirement is satisfied naturally by ownership state rather than by an independent `SuppressedRuntimeIds` mechanism.

Implementation may still cache a derived suppression set internally for efficiency, but it MUST be derived from or behaviorally equivalent to exact ownership state and MUST NOT become a second competing source of truth.

---

## 19. Group timeout and playback-unit failure

Normal group completion and timeout are different terminal paths.

Forbidden:

```text
Group timeout
→ CompleteActiveRecord()
→ reduce leader only
```

Required timeout flow after exact-token validation:

```text
cancel exact tracked Group unit
→ clean all child transients
→ mark no new member VisuallyPresented from failed unit
→ clear/terminate active group playback owner
→ atomically clear SelectionArea/Transition/ConsumedPendingReducer state owned by the failed active Envelope
→ reconcile current ActiveEnvelope directly to ActiveEnvelope.FinalSnapshot
→ mark current Envelope Presentation-complete
→ preserve later queued Envelopes
→ continue backlog normally
```

Do not reuse a global reset/collapse helper if it discards valid later queued Envelopes.

No intermediate HUD broadcast may expose selected historical cards between clearing transient ownership and applying the final snapshot.

Group timeout remains Presentation-only and never requests Gameplay `ResolutionFault` by itself.

---

## 20. Warcry / one-member behavior

Warcry selects one exact Hand card and commits `Hand→DrawPile`.

Initial Controller behavior remains:

```text
ExpectedMemberCount = 1
→ normal SingleRecord path
```

Visual ownership flow:

```text
Hand
→ SelectionArea
→ Confirm
→ SingleRecord Hand→DrawPile accepts
→ Transition
→ DrawPile animation completes
→ reducer consumes member
→ Done
```

Warcry's own PlayArea→Exhaust Record remains ungrouped and continues through the existing generic played-card cleanup path.

No Warcry-specific animation is introduced.

---

## 21. Multi-exhaust behavior

For Burning-Pact-style A/B/C Selection:

```text
select A/B/C
→ owner A/B/C = SelectionArea
→ Confirm
→ one SelectionDestination group G
```

If G passes validation/interference and starts successfully:

```text
A fade ┐
B fade ├─ begin together from SelectionArea visuals
C fade ┘
```

At visual completion:

```text
owner A/B/C = ConsumedPendingReducer
```

Then reducer/interleaved playback continues chronologically. Future members never flash back to Hand because their formal slots remain Hidden until their exact committed records remove them from historical Hand state.

If G is incomplete/interfered/rejected/preparation-failed:

```text
owner A/B/C stays SelectionArea
→ sequential A/B/C
```

No source-position restoration is necessary.

---

## 22. Failure and degradation matrix

Presentation-only degradation conditions include:

- group allocation unavailable/stale writer;
- incomplete/malformed group;
- 0/2 eligible Records for a selected member;
- unsupported destination;
- reducer preflight failure for grouping eligibility;
- future-member interference;
- missing/inconsistent SelectionArea ownership;
- Base Widget group rejection;
- child N preparation failure;
- Group timeout;
- Widget loss/replacement;
- PresentationUnavailable/no-history.

Behavior:

```text
before accepted Group ownership transfer
→ keep SelectionArea ownership
→ sequential fallback

after accepted Group normal completion
→ ConsumedPendingReducer until exact reducer consumption

after accepted Group timeout/fatal playback-unit failure
→ exact cancel + active-envelope final-snapshot reconciliation
```

Gameplay remains authoritative in all cases.

---

## 23. Explicitly rejected designs

### 23.1 Gameplay SelectionZone

Do not mutate authoritative Hand membership merely because the player selected a card in UI.

### 23.2 Selected flag while Hand remains visible owner

Do not keep the selected card as a normal visible Hand-owned card and rely only on transform offsets.

### 23.3 Position-only handoff as primary continuity model

Do not make Hand rebuild + confirmed-center restoration the normal post-confirm architecture.

### 23.4 Independent suppression truth

Do not maintain a second authoritative `SuppressedRuntimeIds` state that can disagree with Presentation ownership.

### 23.5 Remove/Collapse formal historical Hand slot

Not allowed while index-based Hand contracts remain.

### 23.6 Dry-run-only group preflight

Reducer validity without future-member interference analysis is insufficient.

### 23.7 Treat Group timeout as ordinary completion

No leader-only reduce after timed-out group playback.

### 23.8 Effect/Card special cases

No CardId/Effect branching for SelectionArea, group or destination animation.

### 23.9 Gameplay batching for visuals

Do not replace authoritative per-card Actions/events with one bulk mutation.

### 23.10 Widget-owned Envelope scanning

Group discovery belongs to Controller.

---

## 24. Proposed implementation order

### G1 — metadata and writer-scoped correlation

- group types on Presentation records;
- writer-scoped GroupId allocation;
- active-Resolution counter;
- stale writer rejection;
- Action-local Selection group context;
- direct selected-card Records stamp matching metadata only;
- trigger reactions remain ungrouped;
- no visible behavior change.

### G2 — explicit SelectionArea ownership

- add Presentation-only exact RuntimeId ownership state;
- implement Hand ↔ SelectionArea select/deselect transitions;
- keep formal Hand slots Hidden while owner != Hand;
- create/retain stable visible SelectionArea card visuals;
- Confirm freezes SelectionArea ownership instead of reducing it to coordinates;
- normal `RefreshHand()` never steals visible ownership back from SelectionArea.

### G3 — Controller group discovery/interference preflight

- exact group discovery/validation;
- arbitrary-snapshot chronological dry-run;
- future-member interference predicate;
- group-disabled state for ineligible identities;
- `ExpectedMemberCount > 1` requirement for Group path.

### G4 — Base Widget playback-unit + timeout reconciliation

- Record vs Group token kind;
- one tracked Base Widget playback owner;
- Group wrapper/deferred completion/cancel/stale hardening;
- dedicated active-envelope timeout/failure reconciliation;
- preserve later PlaybackQueue entries.

### G5 — generic N-child transition engine

- generic child transition state;
- SingleRecord uses one child;
- source may be Hand or SelectionArea;
- migrate Hand→DrawPile into generic engine;
- N-child same-tick Group playback;
- Group transfer SelectionArea → Transition transactionally.

### G6 — ConsumedPendingReducer + cleanup/validation

- after grouped child completion set exact future members to `ConsumedPendingReducer`;
- reducer clears exact ownership entry after applying member Record;
- derive formal Hand hiding from ownership;
- remove superseded position-restoration/suppression-only code paths after equivalence is proven;
- run focused automation and manual PIE.

Do not combine stages into one large patch unless a genuine compile dependency requires it.

---

## 25. Automated acceptance plan

### 25.1 Ownership

- selecting A changes visible owner `Hand → SelectionArea` without Gameplay mutation;
- deselect returns `SelectionArea → Hand`;
- formal Hand child remains at exact historical index while SelectionArea owns the visible card;
- formal child is `Hidden`, not `Collapsed`;
- only one visible representation exists for a RuntimeId;
- `RefreshHand()` does not move a SelectionArea-owned card back to Hand;
- Confirm keeps the exact SelectionArea visual alive until destination playback accepts it.

### 25.2 Group metadata/correlation

- stale writer cannot allocate against a newer Resolution;
- two Selection decisions in one Resolution get distinct IDs;
- canonical selected runtime sequence, not click order, defines membership;
- trigger Records remain ungrouped;
- 0/2 eligible-member shapes disable group playback.

### 25.3 Interference/controller

- contiguous 3-member group validates;
- non-contiguous unrelated interleaving validates;
- future member leave/return Hand causes interference rejection even when dry-run succeeds;
- interleaved CardPlayed touching a future member disables group;
- unrelated Damage/Status/Energy does not disable group;
- reducer order remains authored chronology.

### 25.4 Group/Single visual ownership

- valid A/B/C group transfers all three `SelectionArea → Transition` atomically;
- child N prepare failure leaves all members owned by SelectionArea;
- sequential fallback consumes A while B/C remain visibly in SelectionArea through Hand rebuild;
- no confirmed-position restoration step is required for B/C;
- grouped child completion moves exact future members to `ConsumedPendingReducer`;
- formal future-member slots remain Hidden through interleaved snapshots;
- exact reducer member clears its ownership entry only after applying the committed Record.

### 25.5 Timeout/hardening

- Record and Group share one Base Widget tracked owner;
- stale callbacks cannot cross-complete unit kinds;
- Group timeout never leader-completes;
- all group child transients are cleaned;
- active-envelope ownership is atomically cleared with FinalSnapshot reconciliation;
- later queued Envelope is preserved;
- no one-frame Hand flash appears during timeout reconciliation.

### 25.6 Warcry/multi-exhaust

- Warcry remains SingleRecord Controller playback but uses the same generic transition engine;
- Warcry selected visual goes `SelectionArea → Transition → Done` toward DrawPile;
- Warcry played-card Exhaust remains ordinary ungrouped cleanup;
- multi-exhaust A/B/C begins together when safe;
- sequential degradation leaves B/C in SelectionArea, not Hand;
- no card flashes back to Hand.

---

## 26. Manual PIE acceptance

### Multi-exhaust

Expected:

```text
select A/B/(C)
→ cards visibly leave normal Hand and collect in SelectionArea
→ explicit Confirm
→ all selected cards disappear together when Group is safe
→ no selected card flashes back to Hand
→ interleaved trigger Presentation still appears later in chronological place
→ input eventually restores
```

### Sequential degradation diagnostic

Force/use a case where Group cannot safely co-present.

Expected:

```text
SelectionArea holds A/B/C
→ A transitions first
→ B/C remain visibly in SelectionArea through Hand refresh
→ B transitions
→ C transitions
```

### Warcry regression

Expected:

```text
Draw presentation
→ SelectionArea ownership for chosen Hand card
→ explicit Confirm
→ selected card flies SelectionArea → DrawPile
→ Warcry remains/returns to PlayArea cleanup path
→ generic PlayArea→Exhaust
```

No new production `.uasset`/`.umap` change is authorized by this design.

---

## 27. Final responsibility split

```text
Gameplay
    owns authoritative Hand/DrawPile/Discard/Exhaust membership

Selection UI
    owns player selection intent and SelectionArea visible card ownership

Formal Hand surface
    preserves historical structural slots/indexes
    shows a card only when PresentationOwner == Hand

Presentation Record/Writer
    owns immutable group correlation

BattlePresentationController
    owns group discovery, visible-order exception,
    interference preflight, reducer chronology and playback-unit sequencing

UBattleHUDWidgetBase
    owns exact hardened Record-or-Group playback boundary

Generic Native card-transition engine
    owns Hand/SelectionArea → committed destination animation children

ConsumedPendingReducer ownership
    prevents visually consumed future members from returning to Hand
    until their exact committed Record is chronologically reduced
```

The core design rule is:

> Group decides **which committed selected-card transitions may play together**; SelectionArea ownership decides **which UI surface currently has the right to display each selected RuntimeId**. Hand rebuild is no longer responsible for preserving selected-card visual continuity.
