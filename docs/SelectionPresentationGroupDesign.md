# Selection Presentation Group + Parallel Animation Design

Date: **2026-09-09**

Status:

```text
DRAFT / REVIEW-ADJUSTED / UPSTREAM CONTRACTS UPDATED /
NO PRODUCTION GROUP CODE IMPLEMENTED / NOT VALIDATED / NOT SEALED
```

Scope: replace the current one-Record-at-a-time visual treatment of a confirmed multi-card Selection with an explicit Selection Presentation Group. A validated multi-member group may co-present its committed selected-card destination facts while Gameplay, event dispatch, `PresentationSequence`, and reducer chronology remain unchanged.

Authoritative related contracts:

- `docs/CardSelectionPresentationConstraints.md`
- `docs/CardSelectionRefactorConstraints.md`
- `Source/SlayTheSpireDemo/Presentation/AGENTS.md`
- `Source/SlayTheSpireDemo/UI/AGENTS.md`

This revision incorporates the architecture review that identified three blockers in the first draft:

1. visually consumed future group members require suppression until their own reducer cursor is reached;
2. non-contiguous group lookahead is an explicit visible-playback-order exception and must be authorized by upstream Presentation contracts;
3. Group playback must inherit the exact-token/deferred-callback/cancellation hardening of `UBattleHUDWidgetBase` through one tracked playback-unit owner.

Those upstream contracts have now been amended. This document still does **not** claim any production implementation or validation.

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

For a confirmed multi-card Selection that exhausts `A, B, C`, this creates two problems:

1. total selected-card visual delay scales as approximately `N × animation duration`;
2. after A reduces, `RefreshHand()` may rebuild B/C at normal Hand positions before their own records play, producing a flashback to the Hand row.

A per-Record deferred/reposition patch can hide part of the second symptom but preserves the wrong playback unit and can stall later Presentation.

The intended visual cohort is the **Selection decision**, while authoritative mutation and reducer chronology remain per committed Record.

---

## 2. Non-negotiable invariants

### 2.1 Gameplay and committed chronology are unchanged

This design MUST NOT change:

```text
SelectionResult canonical order
→ direct continuation Action order
→ Gameplay mutation order
→ BattleEvent dispatch order
→ Trigger reaction order
→ PresentationSequence assignment order
```

Presentation never waits inside Gameplay and never changes Gameplay to make visuals simultaneous.

### 2.2 Reducer order is always `PresentationSequence` order

Even when A/B/C are visually co-presented:

```text
reduce A
→ reduce chronological interleaved records
→ reduce B
→ ...
→ reduce C
```

The Controller MUST NOT apply B/C early merely because their visuals have already completed.

### 2.3 Visible playback normally follows committed order, with one explicit exception

Normal visible playback follows `PresentationSequence`.

A complete, explicit, Controller-validated `PresentationGroup` may authorize co-presentation/lookahead of **its own frozen members only** from the already sealed Envelope.

This is an intentional visible-order exception, now authorized by the upstream Presentation contracts. It is not reducer reordering.

The exception MUST NOT:

- consume or skip interleaved ungrouped Records;
- reduce future group members early;
- infer membership from CardId, Effect, destination, adjacency, click order or timing;
- permit Widget code to scan future Envelope Records;
- read future mutable Gameplay.

### 2.4 Grouping is explicit committed Presentation metadata

A Record is a group member only if it carries an explicit group tag.

The Controller MUST NOT guess a group from consecutive `Hand→Exhaust` records or similar visual similarity.

### 2.5 Trigger reactions do not inherit Selection group context

A selected-card Action may append its direct `CardZoneChanged` record and then dispatch an event such as `CardExhausted`. Trigger reactions may be inserted before the next selected-card continuation Action.

The ordinary `FPresentationRecordWriter` is propagated into trigger reactions today and must remain only the Resolution-recording capability.

Therefore:

```text
writer inheritance           = yes
Selection group inheritance  = no
```

Group correlation is Action-local Presentation metadata assigned only to the Selection's direct continuation batch.

### 2.6 Visually consumed does not mean reducer-consumed

If B/C disappear during group playback before their chronological reducer records are reached, they remain **Presentation-suppressed** from formal Hand rendering.

Intermediate ViewModel snapshot applications or Hand rebuilds MUST NOT make them visible again.

Suppression is released only when the exact member is chronologically reduced, or when a global Presentation reconciliation path invalidates the group lifecycle.

### 2.7 Base Widget remains the hardened Controller-facing boundary

Group playback MUST use `UBattleHUDWidgetBase` just like Record playback.

Concrete Native/Blueprint HUD code MUST NOT bypass the base wrapper, maintain an independent group token owner, or notify the Controller directly.

Record and Group playback share one tracked playback-unit owner and the same exact-token, deferred-completion, cancellation, timeout, stale-callback and Widget-replacement semantics.

### 2.8 No Effect/CardId visual branching

Warcry, Burning Pact and future cards do not own grouping or destination animation.

Selection owns interaction/handoff. Controller owns group sequencing. Generic zone Presentation owns destination animation.

---

## 3. High-level model

```text
Player confirms Selection
        ↓
Gameplay validates SelectionResult
        ↓
SelectionRequestAction builds direct continuation batch
        ↓
writer-scoped allocation of one Presentation GroupId
        ↓
create Action-local Selection group context
        ↓
stamp context onto direct continuation Actions only
        ↓
Gameplay executes normally
        ↓
direct selected-card CardZoneChanged Records may carry GroupTag
trigger/reaction Records remain ungrouped
        ↓
sealed immutable Presentation Envelope
        ↓
Controller reaches first eligible multi-member group Record
        ↓
Controller discovers + validates + preflights complete group
        ↓
Base Widget accepts one Group playback unit
        ↓
generic card-transition engine starts N children together
        ↓
formal future-member visuals become suppressed
        ↓
one group visual completion
        ↓
transient children are cleaned, suppression remains
        ↓
Controller resumes chronological reducer cursor
        ↓
release each member suppression only when that exact member is reduced
```

A Selection Presentation Group is a **visual cohort**, not a Gameplay transaction.

---

## 4. Record metadata

Add Presentation-only grouping metadata to `FPresentationRecord`.

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

Group identity is:

```text
(BattleId, ResolutionId, Kind, GroupId)
```

Rules:

```text
GroupId == 0
↔ no group

ExpectedMemberCount > 0
for any non-zero SelectionDestination group tag
```

`ExpectedMemberCount` is repeated on each member so the sealed Envelope is self-validating.

No separate member order is introduced. `PresentationSequence` remains canonical committed order.

### 4.1 Initial eligible member shape

The first implementation is intentionally narrow:

```text
Record.Type = CardZoneChanged
Record.CardZoneChanged.FromZone = Hand
Group.Kind = SelectionDestination
```

Initial destination support:

- `ExhaustPile`;
- `DrawPile`;
- `DiscardPile` only if the generic Native destination path is ready when that stage is reached.

Mixed destinations are architecturally legal once each child transition is supported; group membership does not imply equal destinations.

---

## 5. GroupId allocation: writer-scoped only

`SelectionSource` is semantic metadata, not identity. Multiple selections may use the same source in one Resolution.

GroupId allocation MUST be exposed only through the frozen writer capability:

```cpp
bool FPresentationRecordWriter::TryAllocatePresentationGroupId(
    int64& OutGroupId) const;
```

Conceptual implementation:

```text
Writer.TryAllocatePresentationGroupId
→ weak Recorder
→ Recorder.AllocatePresentationGroupId(
      WriterBattleId,
      WriterResolutionId,
      OutGroupId)
→ Recorder validates IsWriterCurrentAndValid(...)
→ allocate only from that active Resolution builder
```

The Recorder MUST NOT expose a general caller-facing `AllocateGroupId()` that can allocate against whichever Resolution happens to be active.

The counter belongs in the active Resolution builder:

```cpp
struct FActiveResolutionBuilder
{
    ...
    int64 NextPresentationGroupId = 1;
};
```

Consequences:

- GroupId has no Gameplay meaning;
- IDs need only be unique inside `(BattleId, ResolutionId)`;
- a stale writer cannot allocate from a newer active Resolution;
- if Presentation recording is unavailable or the writer is stale, allocation fails and Gameplay continues normally without group metadata.

The writer allocates identity only. It does **not** carry an active Selection group context that automatically propagates to trigger Actions.

---

## 6. Selection correlation and direct Action context

### 6.1 SelectionRequestAction remains object-type neutral

`USelectionRequestAction` MUST NOT cast selected objects to `UCardInstance`.

After successful resolution, it maps `Result.SelectedObjects` back to the frozen `Request.Candidates` and obtains the selected canonical `RuntimeSequence` values.

For `UCurrentHandSelectionSource`, `RuntimeSequence == CardRuntimeId`. Other Selection domains may use another deterministic sequence without becoming card-specific.

### 6.2 Action-local group context

If Presentation group allocation succeeds, create conceptual context:

```cpp
struct FSelectionPresentationGroupContext
{
    EPresentationGroupKind Kind = EPresentationGroupKind::SelectionDestination;
    int64 GroupId = 0;
    int32 ExpectedMemberCount = 0;
    TArray<int32> SelectedRuntimeSequences;
};
```

For the initial card Selection path:

```text
ExpectedMemberCount = resolved SelectedObjects.Num()
```

The exact same context is stamped onto the Selection's **direct continuation Actions only**.

### 6.3 EventDispatcher does not copy group context

`UBattleAction` stores ordinary writer and optional Selection group context separately.

Existing trigger propagation remains:

```text
trigger Action gets PresentationRecordWriter
trigger Action does NOT get SelectionPresentationGroupContext
```

This is mandatory even when a trigger is caused by a grouped selected-card Action.

### 6.4 Concrete Action stamps only its matching direct committed fact

A concrete card-zone Action may stamp its direct `CardZoneChanged` only if:

```text
Action carries valid SelectionDestination context
AND
CommitResult.CardRuntimeId ∈ SelectedRuntimeSequences
AND
Record shape is eligible for SelectionDestination grouping
```

An unrelated Record emitted by the same Action must remain ungrouped.

### 6.5 Exact-one-eligible-record-per-selected-member invariant

`ExpectedMemberCount = selected count` is valid only when each selected runtime sequence produces **exactly one** eligible direct committed group member.

This is a group eligibility invariant, not a universal Selection guarantee.

Examples:

```text
Selected A/B
A produces one eligible grouped zone Record
B produces zero
→ actual members 1 != expected 2
→ group disabled, sequential fallback
```

```text
Selected A
A produces two matching eligible grouped Records
→ duplicate RuntimeId / member-count mismatch
→ group disabled, sequential fallback
```

The group feature therefore does not claim arbitrary continuation shapes. Unsupported shapes degrade Presentation only.

---

## 7. Complete-group discovery and validation

The Controller receives the entire sealed Envelope before playback and may inspect only that immutable data.

When the reducer cursor reaches a tagged Record, discover all Records in the active Envelope with the same exact group identity.

Co-presentation is eligible only when all conditions hold:

1. `ExpectedMemberCount > 1`;
2. discovered member count equals `ExpectedMemberCount`;
3. every member repeats identical `(Kind, GroupId, ExpectedMemberCount)`;
4. every member is an initially supported `CardZoneChanged` from Hand;
5. member `PresentationSequence` values are unique and strictly follow Envelope chronology;
6. exact card RuntimeIds are valid and unique across members;
7. each member represents exactly one selected runtime sequence;
8. no terminal Record belongs to the group;
9. each payload passes existing frozen-record validation;
10. chronological dry-run reducer preflight through the last member succeeds, including interleaved ungrouped Records.

If any condition fails:

```text
mark group identity disabled for this Envelope
→ do not establish suppression
→ do not mark members VisuallyPresented
→ use existing sequential SingleRecord playback
```

Malformed/incomplete grouping metadata by itself is Presentation degradation and does not request Gameplay `ResolutionFault`.

### 7.1 One-member metadata

A resolved one-card Selection may still produce a valid group tag for correlation consistency.

However:

```text
ExpectedMemberCount <= 1
→ Controller does NOT enter SelectionGroup playback
→ normal SingleRecord path
```

The generic child transition engine is still shared, so single and multi-card visual style remains unified without expanding initial group-kernel risk.

---

## 8. Non-contiguous group and visible-order semantics

Group members need not be contiguous because selected-card Actions may dispatch events that insert trigger reactions before the next selected-card Action.

Example committed Envelope order:

```text
A Hand→Exhaust            [Group G]
A-trigger Damage          [ungrouped]
A-trigger StatusChanged   [ungrouped]
B Hand→Exhaust            [Group G]
B-trigger Draw            [ungrouped]
C Hand→Exhaust            [Group G]
```

Changing authoritative Action/trigger order to make members contiguous is forbidden.

### 8.1 Controlled lookahead

When the Controller reaches A and validates complete Group G, it may offer frozen A/B/C together as one playback unit.

Visible selected-card behavior:

```text
A/B/C destination animations start together
```

This intentionally makes B/C visible before some interleaved ungrouped Presentation that chronologically precedes their reducer records.

This is the explicit PresentationGroup visible-order exception authorized by `Presentation/AGENTS.md` and `CardSelectionPresentationConstraints.md`.

### 8.2 Interleaved Records remain chronological

After the group visual completes:

```text
reduce A
→ play/reduce A-trigger Damage
→ play/reduce A-trigger StatusChanged
→ reach B: B visual already presented, reduce B without replay
→ play/reduce B-trigger Draw
→ reach C: reduce C without replay
```

No interleaved Record is marked visually presented merely because it lies inside the group member span.

---

## 9. Visually-consumed member suppression lifetime

This is a required part of the group kernel, not a later visual polish patch.

### 9.1 Why suppression is required

After A/B/C animate together, reducer chronology still has B/C in Hand until their own records are consumed.

Without suppression:

```text
group animation finishes
→ transient A/B/C removed
→ reduce A
→ ApplyPresentationSnapshot
→ historical Hand still contains B/C
→ RefreshHand recreates B/C in Hand row
→ visible flashback
```

Therefore group completion must not release formal-visual ownership for future members.

### 9.2 Ownership split

Recommended responsibility:

```text
BattlePresentationController
→ owns which exact group members are visually consumed but not reduced

UBattleHUDViewModel
→ stores transient Presentation suppression identities used during historical HUD refresh

UBattleHUDWidget / RefreshHand
→ obeys suppression and does not visibly create/show suppressed formal cards
```

Conceptual ViewModel state:

```cpp
TSet<int32> PresentationSuppressedHandRuntimeIds;
```

Production implementation may use a richer battle/resolution/group/member identity structure, but RuntimeId lookup must remain exact and group ownership must prevent stale suppression from crossing Resolution/Battle boundaries.

This set is transient Presentation state, not authoritative Gameplay and not part of frozen `FPresentationStateSnapshot` truth.

### 9.3 Establish suppression

Suppression is established only after a complete group is accepted transactionally for visible playback.

Before every child is successfully prepared, no formal member may be permanently hidden and no suppression may be committed.

Once the group start is accepted:

```text
for each exact member RuntimeId
→ establish suppression
→ child transient owns visible transition
```

### 9.4 Group animation completion

At shared animation completion:

```text
clean transient child visuals
mark exact member indices VisuallyPresented
notify Controller once
```

Do **not** clear suppression for members whose reducer cursor has not yet reached their record.

Thus:

```text
child lifetime         ends at group visual completion
suppression lifetime   ends at exact member reducer consumption
```

These are deliberately different lifetimes.

### 9.5 Exact member reduction

When the chronological cursor reaches an already-VisuallyPresented member:

```text
ApplyRecordToWorkingSnapshot(member)
→ release suppression for that exact member
→ publish/apply resulting Presentation snapshot
→ advance cursor
```

Release must occur after the member mutation has been applied to the WorkingSnapshot and before/with publishing that post-member snapshot, so no frame can reconstruct the card as visible Hand content.

Interleaved ungrouped Record snapshot refreshes do not release future-member suppression.

### 9.6 Global cleanup

All active group suppression must be cleared during global reconciliation paths:

- group rejected before ownership: nothing established;
- Skip/collapse to final snapshot;
- Presentation timeout catch-up path;
- Widget loss/replacement;
- Envelope completion/replacement;
- battle replacement/reset;
- PresentationUnavailable;
- direct-baseline/no-history transition.

Cleanup must not leave hidden cards after final-snapshot reconciliation.

---

## 10. Controller playback unit

Refactor Controller semantics from:

```text
active playback = one Record
```

to:

```text
active playback unit =
    SingleRecord
    OR SelectionGroup
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

`ActiveRecordIndex` remains the chronological reducer cursor at all times.

### 10.1 Group preflight

Before offering a group to the Widget:

1. clone `WorkingPresentationSnapshot`;
2. dry-run reducer application from current cursor through the last discovered group member in exact Envelope order;
3. include all interleaved ungrouped Records in the dry run;
4. validate every group member against the correct historical state;
5. publish none of the dry-run snapshots.

This requires reducer logic that can apply a Record to an arbitrary snapshot copy rather than only mutating Controller member state.

### 10.2 Group accepted

If the hardened Widget group wrapper returns true:

```text
active unit = SelectionGroup
bWaitingForCompletion = true
one exact group token
one group timeout
```

No reducer cursor advancement occurs at Begin.

### 10.3 Group visual completion

On exact group completion:

```text
cancel exact timeout
mark only group member indices VisuallyPresented
retain suppression for not-yet-reduced members
clear active group playback-unit ownership
resume chronological reducer cursor at original first member
```

### 10.4 Already-VisuallyPresented member

When cursor reaches a member already co-presented:

```text
skip visible Begin
→ apply Record chronologically
→ release exact member suppression
→ publish snapshot according to Controller batching rules
→ advance cursor
```

Consecutive already-presented members may be reduced synchronously and publish once at the end of that consecutive run, but an interleaved ungrouped Record breaks the run and receives normal visible playback.

### 10.5 Group disabled or Widget declines

If validation fails or the Widget group wrapper returns false:

```text
mark exact group identity group-playback-disabled for this Envelope
→ no suppression retained
→ no VisuallyPresented members
→ immediately execute existing SingleRecord path at current cursor
```

Future tagged members from that disabled identity also remain ordinary sequential Records.

---

## 11. Base Widget unified playback-unit hardening

`PlayPresentationGroup()` must be a Controller-facing `UBattleHUDWidgetBase` wrapper symmetric with `PlayPresentationRecord()`.

Conceptual public wrappers:

```cpp
bool PlayPresentationRecord(
    const FPresentationRecord& Record,
    const FPresentationPlaybackToken& Token);

bool PlayPresentationGroup(
    const FPresentationPlaybackGroup& Group,
    const FPresentationPlaybackToken& Token);
```

Both wrappers feed one internal tracked playback owner.

### 11.1 One tracked owner

Do not create:

```text
TrackedRecordPlayback
TrackedGroupPlayback
```

Use conceptually:

```cpp
struct FTrackedPresentationPlayback
{
    EPresentationPlaybackUnitKind Kind = EPresentationPlaybackUnitKind::None;
    FPresentationPlaybackToken Token;
};
```

At most one Controller-owned visual playback unit is active in the Widget.

### 11.2 Token identity

Extend token identity conceptually with:

```cpp
EPresentationPlaybackUnitKind PlaybackUnitKind;
int64 PresentationGroupId = 0;
```

SingleRecord token:

```text
PlaybackUnitKind = SingleRecord
PresentationGroupId = 0
PresentationSequence = record sequence
```

Group token:

```text
PlaybackUnitKind = SelectionGroup
PresentationGroupId = group id
PresentationSequence = first member sequence (leader/diagnostic identity)
```

Token equality includes unit kind and group id in addition to existing battle/resolution/sequence/generation identity.

### 11.3 Existing hardening is mandatory for groups

Group wrapper must preserve the existing base semantics:

- track exact token before concrete playback entry;
- return true only if concrete playback actually starts asynchronously;
- completion forwards through the base deferred/CoreTicker path, preventing synchronous Controller re-entry;
- stale/duplicate/old-battle/post-Skip callbacks are ignored;
- Cancel targets only the exact currently tracked playback unit;
- timeout uses the same unit token/generation;
- Widget destruction/replacement cannot cancel or complete a newer owner;
- concrete HUD never calls Controller directly.

### 11.4 Base fallback

The base group implementation may return false.

That means unsupported HUD surfaces safely trigger Controller sequential fallback without changing committed facts.

---

## 12. Generic parallel card-transition engine

Current Native card presentation fields are single-instance (`ActiveNativeMovingCardWidget`, one historical card, one timer/elapsed state, one start/end pair). They cannot safely represent N concurrent cards.

Extract a generic per-child transition state.

Conceptual child:

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

One generic builder chooses visual style from committed source/destination facts plus optional Selection source handoff.

### 12.1 Hand→Exhaust

```text
source = confirmed Selection position when available
end position = source
opacity → 0
```

Grouped playback uses independently owned transient children while formal members are suppressed.

This is the same generic Exhaust style, not a Burning Pact or multi-select animation.

### 12.2 Hand→DrawPile

```text
source = confirmed Selection position
end = DrawPile visual anchor
visible movement toward DrawPile
```

The existing Selection-subclass-only shared transfer timer/state should ultimately migrate into this generic engine.

### 12.3 Hand→DiscardPile

Use the generic discard destination transition when available, with the confirmed source handoff.

### 12.4 Single and group share one engine

```text
SingleRecord CardZoneChanged
→ generic engine with 1 child

SelectionGroup
→ generic engine with N children
```

No duplicate Single vs Group destination-animation implementation is allowed.

---

## 13. Selection visual handoff

`UBattleHUDSelectionWidget` continues to own Selection interaction and confirmed exact RuntimeId source positions.

It provides conceptually:

```text
RuntimeId → confirmed absolute visual source position
```

It does not choose the destination style.

For a proposed multi-member group start:

1. require exact handoff for every member that needs Selection-origin continuity;
2. transactionally prepare every child transition;
3. if any child cannot prepare, roll back all transient work and return false;
4. only after all children are valid does the playback unit establish formal-member suppression and start all children;
5. source handoff may be consumed by the transition engine once durable group suppression owns continuity.

No partial group start is permitted.

The played card visual itself remains separate from selected-card destination children. Warcry/Burning Pact cleanup continues through ordinary committed played-card destination facts.

---

## 14. Group child completion

For the initial implementation, children in one group use the existing uniform card transition duration.

```text
Begin group
→ every child elapsed = 0 in same Native tick
→ tick all children together
→ after shared duration all children reach final visual state
→ remove/clean child transients
→ keep future-member suppression
→ notify exact Group token once
```

Total selected-card wait is approximately one transition duration rather than `N × duration`.

If future child durations differ, group visual completion is the maximum child duration, but this is not part of the first implementation.

---

## 15. Warcry / one-member behavior

Warcry selects one exact Hand card and commits `Hand→DrawPile`.

Correlation metadata may still be stamped:

```text
ExpectedMemberCount = 1
```

But the first implementation intentionally does **not** enter SelectionGroup Controller playback for this case:

```text
Warcry Selection Confirm
→ one tagged Hand→DrawPile Record
→ Controller normal SingleRecord path
→ generic transition engine creates 1 child
→ selected card flies to DrawPile
→ reducer continues
→ Warcry PlayArea→Exhaust remains ungrouped
→ existing generic played-card Exhaust cleanup
```

This proves single/group reuse at the generic child engine without making Warcry a regression test for group lookahead/suppression/token semantics.

No Warcry-specific animation is introduced.

---

## 16. Burning Pact / multi-exhaust behavior

For configured selection A/B/C:

```text
Confirm A/B/C
→ one SelectionDestination correlation G
```

Direct committed selected-card members conceptually:

```text
A Hand→Exhaust [G]
B Hand→Exhaust [G]
C Hand→Exhaust [G]
```

Trigger records may be interleaved in the final Envelope.

At first validated group member:

```text
A fade ┐
B fade ├─ same Native tick, same generic Exhaust style
C fade ┘
```

After one duration:

```text
A/B/C transients gone
B/C remain suppressed until their own reducer records
→ chronological trigger/record playback continues
→ no B/C flashback to Hand
```

---

## 17. Failure and degradation

The following are Presentation-only failure/degradation conditions and MUST NOT request Gameplay `ResolutionFault` by themselves:

- no writer / group ID allocation unavailable;
- stale writer rejects allocation;
- incomplete member count;
- zero eligible member for a selected object;
- duplicate/multiple matching eligible members for one selected object;
- unsupported group destination;
- reducer preflight failure attributable to grouping eligibility;
- base Widget group rejection;
- child transaction preparation failure;
- group timeout;
- Skip;
- Widget replacement/loss;
- PresentationUnavailable/no-history mode.

Fallback is either existing SingleRecord sequential presentation or current final-snapshot catch-up/collapse policy.

A malformed committed Record that violates pre-existing envelope/reducer validity may still use existing Presentation collapse behavior, but Presentation does not rewrite Gameplay.

---

## 18. Explicitly rejected designs

### 18.1 Per-card deferred Hand rebuild patch

Do not restore:

```text
A plays
→ rebuild Hand
→ manually reposition/defer B
→ B plays
→ rebuild Hand
→ manually reposition/defer C
```

It keeps `N × duration` and couples Selection continuity to repeated formal Hand reconstruction.

### 18.2 Clear suppression at group animation completion

Forbidden:

```text
group visually completes
→ clear all hidden/suppressed formal members
→ reduce A
```

This recreates the original flashback through intermediate snapshots.

### 18.3 Effect/Card special casing

No:

```cpp
if (CardId == BurningPact)
if (CardId == Warcry)
if (SelectedCount > 1 && Effect == Exhaust)
```

### 18.4 Gameplay batching for visuals

Do not replace per-card authoritative Actions/events with a bulk Gameplay mutation to force animation simultaneity.

### 18.5 Writer-carried implicit active group context

The writer may allocate GroupId, but it MUST NOT automatically stamp/carry current Selection group membership into all Actions that inherit the writer.

### 18.6 Widget-owned Envelope scanning

Widget code never discovers group membership by searching future Records.

### 18.7 Independent group token owner in concrete HUD

Do not add separate tracked Record and Group ownership that can cross-cancel or cross-complete. Base Widget owns one playback unit.

### 18.8 Force one-member cases through group kernel

Initial implementation does not require `ExpectedMemberCount == 1` to exercise Controller group logic. Reuse is proven at the generic child transition engine.

---

## 19. Proposed implementation order

Upstream contract amendments are complete in documentation. Production implementation should proceed in small compile/testable stages.

### Stage G1 — metadata and writer-scoped correlation only

- add `EPresentationGroupKind` / `FPresentationGroupTag` to Presentation types;
- add `FPresentationRecordWriter::TryAllocatePresentationGroupId`;
- keep `NextPresentationGroupId` inside active Resolution builder;
- validate writer BattleId/ResolutionId exactly before allocation;
- add optional Action-local Selection group context;
- `SelectionRequestAction` maps resolved objects to candidate runtime sequences without card cast;
- stamp context on direct continuation Actions only;
- concrete eligible card-zone Actions stamp only matching committed member Records;
- trigger reactions remain ungrouped;
- no visible behavior change.

G1 must prove malformed 0/2-member-per-selected shapes do not become valid groups.

### Stage G2 — Controller group discovery/preflight + suppression model

- discover exact group identities in sealed Envelope;
- require `ExpectedMemberCount > 1` for group playback;
- complete-group validation;
- arbitrary-snapshot chronological reducer preflight;
- track exact member indices intended for co-presentation;
- introduce transient Presentation suppression ownership and ViewModel/HUD formal-Hand filtering;
- define exact member release/global cleanup;
- group path may still be disabled/fallback until hardened Widget API exists.

Suppression and Controller group kernel belong to the same stage; suppression is not deferred to UI polish.

### Stage G3 — Base Widget unified playback-unit hardening

- extend token/unit identity for `SingleRecord` vs `SelectionGroup`;
- add `UBattleHUDWidgetBase::PlayPresentationGroup` wrapper;
- use one tracked playback-unit owner;
- share deferred completion forwarding, cancel, timeout and stale-callback semantics;
- Controller can safely offer validated groups; base default rejects/falls back sequentially.

### Stage G4 — generic N-child card-transition engine

- extract single-card Native zone transition into per-child state;
- route existing SingleRecord card-zone animations through one child;
- migrate Selection-specific Hand→DrawPile state into generic engine;
- support N simultaneous children for accepted groups;
- destination behavior remains committed-zone-driven.

### Stage G5 — Selection integration / enable parallel playback

- Selection HUD provides exact confirmed source positions only;
- group transaction captures every child before Begin;
- formal selected members become suppressed only on successful group acceptance;
- multi-exhaust begins all child fades together;
- grouped Hand→DrawPile works for future multi-card cases;
- one-member Warcry stays SingleRecord but uses same generic child engine.

### Stage G6 — cleanup, docs and validation

- delete superseded Selection-specific transfer state after generic equivalence is proven;
- update implementation/execution status documents;
- run only changed-contract automated gates;
- user runs focused manual PIE for concurrency/no-flash/Warcry visual sequence.

Do not combine all stages into one large patch unless a compile boundary makes separation impossible.

---

## 20. Automated acceptance plan

### 20.1 G1 correlation/allocation

- stale writer cannot allocate GroupId from a newer active Resolution;
- two Selection decisions in one Resolution receive distinct group IDs;
- group IDs may restart in another Resolution because identity includes ResolutionId;
- resolved Selection canonical runtime sequences, not click order, define context membership;
- all direct eligible selected-card records receive same group identity;
- trigger-created Actions/Records remain ungrouped despite inheriting writer;
- no-history/Presentation unavailable leaves Gameplay continuation unchanged;
- selected count 2 but only 1 eligible direct member → grouping invalid/sequential;
- selected count 1 but 2 matching eligible members → grouping invalid/sequential.

### 20.2 G2 Controller + suppression

- complete contiguous 3-member group validates;
- complete non-contiguous group with interleaved records validates;
- `ExpectedMemberCount <= 1` is not offered as group playback;
- duplicate RuntimeId, member-count mismatch or unsupported shape disables group;
- dry-run preflight includes interleaved Records chronologically;
- group lookahead selects only exact tagged members;
- interleaved records are never marked VisuallyPresented;
- after group visual completion, future members remain suppressed through intermediate snapshot applications;
- reducer consumes A/interleaved/B/interleaved/C in original order;
- release B suppression only when B reducer record is applied;
- Skip/collapse/battle reset clears all suppression.

### 20.3 G3 Base playback unit

- Record and Group wrappers share one tracked owner;
- group exact completion succeeds once;
- stale/duplicate group callback is ignored;
- stale Group callback cannot complete a newer SingleRecord token;
- stale SingleRecord callback cannot complete a newer Group token;
- synchronous concrete completion is deferred before Controller sequencing resumes;
- exact Cancel stops only current unit;
- Widget replacement cannot cancel newer ownership;
- group base rejection leads to Controller sequential fallback.

### 20.4 G4/G5 Native UI

- N Exhaust children begin in same Native tick;
- every member starts at exact confirmed RuntimeId source position;
- all use generic Exhaust opacity style;
- child completion cleans all group transients;
- formal future members remain suppressed after child cleanup until reducer consumption;
- no selected card flashes back to Hand during interleaved record playback;
- N DrawPile children can move concurrently toward DrawPile anchor when such a selection exists;
- SingleRecord Warcry Hand→DrawPile uses the same generic child builder with one child;
- Warcry later PlayArea→Exhaust remains ungrouped and uses existing generic cleanup;
- Skip leaves no group child or stale suppression.

---

## 21. Manual PIE acceptance

Manual PIE is required only after automated contracts pass because concurrency, flicker and spatial continuity are genuinely visual.

### 21.1 Multi-exhaust

Use/configure an existing Player Selection that exhausts at least two Hand cards.

Expected:

```text
select A/B/(C)
→ explicit Confirm
→ all selected cards begin disappearing together in Selection area
→ no card flashes back to normal Hand position
→ total selected-card visual wait ≈ one Exhaust duration
→ interleaved trigger/other presentation still appears afterward in its chronological place
→ input eventually restores
```

### 21.2 Warcry regression

Expected:

```text
Draw presentation
→ Selection
→ explicit Confirm
→ selected card Hand→DrawPile generic movement
→ Warcry reappears/continues in PlayArea
→ ordinary generic PlayArea→Exhaust cleanup
```

Warcry should not require Group playback to pass this regression.

No new Blueprint animation asset is expected by this design. If implementation discovers a genuinely required Designer-backed surface, it becomes a separate explicit `USER ACTION REQUIRED` step.

---

## 22. Initial implementation impact boundaries

Expected source areas:

```text
PresentationTypes / Recorder / Writer
BattleAction optional Presentation correlation metadata
SelectionRequestAction correlation creation
eligible selected-card zone Actions stamping tags
BattlePresentationController group discovery/preflight/suppression
BattleHUDViewModel transient suppression state
BattleHUDWidgetBase playback-unit wrapper
BattleHUDWidget generic card-transition child engine
BattleHUDSelectionWidget source handoff only
focused Automation tests
```

Not authorized by this design:

```text
Card/Effect-specific animation logic
Gameplay bulk mutation changes
Trigger ordering changes
production .uasset/.umap changes
Legacy HUD changes
```

---

## 23. Design decision summary

Chosen architecture:

```text
Selection decision
→ writer-scoped explicit group correlation
→ direct continuation Actions only
→ committed exact member Records
→ sealed Envelope
→ Controller-owned validation + controlled group lookahead
→ Base Widget one hardened playback-unit owner
→ generic N-child zone transition
→ visual group completion
→ future members remain suppressed
→ chronological reducer/interleaved playback continues
→ exact suppression released per member reduction
```

The key responsibility split is:

```text
Gameplay
    owns authoritative mutation/event order

Selection
    owns player interaction and exact source visual handoff

Presentation Recorder/Record
    owns explicit immutable group correlation

BattlePresentationController
    owns group discovery, visible-order exception, reducer chronology,
    VisuallyPresented member state and suppression lifetime

BattleHUDViewModel
    carries transient Presentation suppression through HUD refreshes

UBattleHUDWidgetBase
    owns one hardened Record-or-Group playback-unit boundary

Generic Native card-zone engine
    owns destination animation children

Effect/CardId
    owns none of the above visual sequencing
```

This design is ready for implementation review at **Stage G1**, but no implementation Gate is considered passed until production code is changed and the corresponding Build/Automation/PIE evidence is actually produced.
