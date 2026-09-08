# Selection Presentation Group Implementation Plan

Date: **2026-09-09**

Status:

```text
PLANNED / UI-FOUNDATION-FIRST / NO PRODUCTION IMPLEMENTATION YET /
NOT BUILT / NOT AUTOMATED / NOT PIE-VALIDATED / NOT SEALED
```

Related architecture contracts:

- `docs/SelectionPresentationGroupDesign.md`
- `docs/CardSelectionPresentationConstraints.md`
- `docs/CardSelectionRefactorConstraints.md`
- `Source/SlayTheSpireDemo/Presentation/AGENTS.md`
- `Source/SlayTheSpireDemo/UI/AGENTS.md`

This document is the implementation plan for the SelectionArea visual-ownership and grouped parallel-presentation redesign. It also records the UI/Presentation architecture issues that should be corrected before grouped playback is enabled, so the new feature does not become another compatibility layer over full HUD rebuilds and index-owned Widget identity.

No Gameplay `SelectionZone` is introduced. Gameplay remains authoritative for Hand/DrawPile/Discard/Exhaust membership.

---

## 1. Why the Hand currently refreshes

The current behavior is an implementation artifact, not a gameplay requirement.

Current path:

```text
Presentation Record finishes
→ BattlePresentationController reduces Record into WorkingPresentationSnapshot
→ ViewModel.ApplyPresentationSnapshot(...)
→ ViewModel.BroadcastChanged()
→ BattleHUDWidget receives OnChanged
→ RefreshHUDFromViewModel()
→ RefreshHand()
→ HB_Hand.ClearChildren()
→ recreate every Hand card Widget
```

The important distinction is:

```text
GOOD:
Controller keeps a complete chronological WorkingPresentationSnapshot.

BAD:
Every complete snapshot publication causes the UI to rebuild every surface.
```

The redesign MUST preserve complete historical snapshots while making HUD reconciliation incremental.

---

## 2. Architecture findings

### 2.1 Coarse `OnChanged` causes whole-HUD refresh

`UBattleHUDViewModel` currently exposes a broad structural `OnChanged` channel. `UBattleHUDWidget::NativeOnBattleHUDViewModelChanged()` responds by calling `RefreshHUDFromViewModel()`, which refreshes Hand, combatants, statuses, Energy, piles, input, feedback, intent and terminal state regardless of which values actually changed.

This means a Damage, Energy or Status Record can indirectly recreate Hand Widgets even when Hand did not change.

Target:

```text
old ViewModel state + new historical snapshot
→ calculate exact dirty surfaces
→ update only dirty surfaces
```

### 2.2 Hand Widget identity is destroyed on every Hand refresh

Current `RefreshHand()` removes every child and recreates every card.

That destroys visual continuity for cards that did not leave Hand:

- Widget object identity;
- cached geometry;
- render transform;
- hover/focus state;
- local presentation state;
- animation source ownership;
- delegate bindings.

Target: RuntimeId-keyed reconciliation that preserves existing Widgets for surviving Hand cards.

### 2.3 Historical index and visual identity are conflated

Committed `FromIndex` / `HandIndexBefore` values are valid historical facts and should continue to validate the frozen Hand sequence.

They should NOT be the primary way to locate a live visible Widget.

Required distinction:

```text
Historical index
→ validates committed snapshot/order.

RuntimeId
→ identifies the exact card presentation object.
```

A selected card may legitimately satisfy:

```text
WorkingSnapshot.HandCards[2].RuntimeId == A
PresentationOwner(A) == SelectionArea
```

while `HB_Hand` keeps only a Hidden structural placeholder for A.

### 2.4 Selection Widget owns too many responsibilities

`UBattleHUDSelectionWidget` currently combines:

- pending-selection input;
- selection layout/backdrop;
- Confirm/Cancel behavior;
- selected-card positioning;
- confirmed-center capture;
- played-card visibility;
- Hand→DrawPile animation;
- `CardZoneChanged` interception;
- transfer cleanup.

This couples player interaction to destination animation and makes every new destination/lifecycle change affect the Selection subclass.

Target split:

```text
Selection interaction
→ selected/deselected/Confirm/Cancel only

SelectionArea presentation
→ selected RuntimeId visual ownership + layout

Generic card transition presenter
→ current visual owner → committed destination
```

### 2.5 Native card transition state is globally single-instance

`UBattleHUDWidget` currently stores one active moving card, one historical Hand card, one timer and one set of start/end animation values.

That data model cannot represent parallel A/B/C transitions safely.

Target:

```text
one generic FCardTransitionInstance per RuntimeId

SingleRecord
→ 1 instance

SelectionGroup
→ N instances
```

### 2.6 Historical display state and interaction state are too tightly coupled

`ApplyPresentationSnapshot()` currently copies historical display values and also clears/reinitializes interaction state, live bindings and selection state.

The Controller currently publishes a working snapshot after each reduced Record, so an ordinary historical update can indirectly reset unrelated interaction/presentation continuity.

Target separation:

```text
Historical display state
    Hand / HP / Block / Status / Energy / pile counts / outcome

Interaction state
    input lock / ordinary card selection / target choice / preview

SelectionArea visual ownership
    independent Presentation lifecycle
```

A historical snapshot application must not implicitly destroy SelectionArea ownership that is intentionally retained across chronological reducer steps.

### 2.7 Status rows use the same full-rebuild pattern

`RefreshStatusRows()` currently clears and recreates status Widgets.

This is not the direct cause of the current multi-select bug, but it is the same architectural pattern and will cause similar continuity problems for status hover/tooltip/animation work.

Target follow-up: exact identity reconciliation using:

```text
TargetPresentationId + StatusId + RuntimeSequence
```

Status reconciliation is adjacent cleanup; it is not required to enable the first Selection Group if scope needs to stay small.

### 2.8 Presentation recovery scope is implicit

Current Controller helpers mix several meanings:

- fail one active visual;
- abandon the active Envelope;
- skip/collapse the whole backlog.

Grouped timeout requires a precise active-envelope-only recovery path that preserves later queued Envelopes.

Target conceptual scope:

```cpp
enum class EPresentationRecoveryScope : uint8
{
    ActivePlaybackUnit,
    ActiveEnvelope,
    EntireBacklog
};
```

The exact type is optional; the behavioral separation is mandatory.

---

## 3. Target architecture

The redesign is organized around three foundations.

### 3.1 Incremental HUD reconciliation

```text
complete historical snapshots remain authoritative
        ↓
ViewModel determines actual changed surfaces
        ↓
HUD reconciles only those surfaces
```

Do not replace historical snapshots with mutable live reads.

### 3.2 Card visual ownership

```text
Gameplay Zone
    Hand / DrawPile / Discard / Exhaust / PlayArea

Presentation Owner
    Hand / SelectionArea / Transition / ConsumedPendingReducer
```

These are separate concepts.

Exact RuntimeId is the visual identity key.

### 3.3 Presentation playback unit

```text
SingleRecord
OR
explicit validated SelectionGroup
        ↓
generic N-child card-transition engine
```

Reducer chronology remains exact committed `PresentationSequence` order.

---

## 4. Required pre-Group foundation: G0

G0 is intentionally before Group metadata/playback. Its purpose is to remove the UI assumptions that created the flashback bug.

### G0-A — ViewModel change-set / dirty surfaces

Introduce an incremental historical-display notification contract.

Conceptual flags:

```cpp
enum class EBattleHUDDirtyFlags : uint32
{
    None       = 0,
    Hand       = 1 << 0,
    Combatants = 1 << 1,
    Statuses   = 1 << 2,
    Energy     = 1 << 3,
    Piles      = 1 << 4,
    Input      = 1 << 5,
    Feedback   = 1 << 6,
    Intent     = 1 << 7,
    Terminal   = 1 << 8
};
```

Exact implementation may use a struct instead of bit flags.

Requirements:

- compare previous displayed state with the incoming historical snapshot;
- mark `Hand` dirty only when Hand content/order/card display data actually changes;
- ordinary Damage/Energy/Status changes do not rebuild Hand;
- Preview remains on its dedicated transient channel;
- initial/full-baseline application may request all surfaces.

G0-A must not weaken the frozen historical snapshot contract.

### G0-B — RuntimeId-keyed Hand reconciliation

Replace default `ClearChildren() + recreate all` behavior with exact reconciliation.

Conceptual registry:

```cpp
TMap<int32, TObjectPtr<UBattleCardWidget>> HandWidgetsByRuntimeId;
```

For:

```text
Old Hand = [A, B, C, D]
New Hand = [B, C, D, E]
```

reconcile as:

```text
A → remove
B → reuse same Widget
C → reuse same Widget
D → reuse same Widget
E → create
```

Then reconcile container order to match the frozen historical Hand.

Requirements:

- duplicate RuntimeId remains invalid;
- exact historical order remains authoritative;
- surviving RuntimeIds preserve Widget identity;
- Bind/Unbind card request delegate only when actual Widget lifetime changes;
- full rebuild remains available only as explicit recovery/destruction behavior, not normal Record progression.

### G0-C — Separate historical index validation from visual lookup

Keep index validation for committed facts:

```text
FromIndex / HandIndexBefore
→ WorkingSnapshot.HandCards[index]
→ exact RuntimeId/card snapshot validation
```

Replace visual lookup with RuntimeId ownership lookup:

```text
RuntimeId
→ CardVisualRegistry
→ current owner
→ exact current visible visual
```

Do not require:

```text
visible widget == HB_Hand.GetChildAt(FromIndex)
```

when Presentation ownership intentionally moved the card to SelectionArea or Transition.

### G0-D — Card visual ownership registry + SelectionArea

Introduce Presentation-only exact RuntimeId ownership.

Conceptual states:

```cpp
enum class ECardPresentationOwner : uint8
{
    Hand,
    SelectionArea,
    Transition,
    ConsumedPendingReducer
};
```

Conceptual registry responsibility:

```text
RuntimeId
→ owner
→ visible visual, when owner has one
```

Requirements:

- one RuntimeId has at most one visible card presentation;
- Hand formal structural slot may coexist as Hidden placeholder when owner != Hand;
- selecting a card transfers visible ownership `Hand → SelectionArea` without Gameplay mutation;
- deselect transfers `SelectionArea → Hand`;
- Confirm freezes SelectionArea ownership; it does not reduce ownership to coordinates;
- SelectionArea visual survives ordinary historical Hand reconciliation;
- `ConsumedPendingReducer` has no visible card but continues to deny Hand visible ownership until exact reducer consumption.

### G0-E — Split Selection interaction from destination presentation

Refactor responsibilities so `UBattleHUDSelectionWidget` no longer owns destination-specific transfer animation.

Target responsibilities:

```text
UBattleHUDSelectionWidget
    pending Selection interaction
    candidate/selected styling
    Confirm/Cancel

SelectionArea presentation component/layer
    selected-card visual ownership
    selected-card layout

Generic CardTransition presenter/engine
    source owner → committed destination
```

Remove `Hand→DrawPile` as a Selection-subclass-specific animation after generic equivalence is proven.

Do not remove the old path before focused tests establish equivalent single-card Warcry behavior.

### G0-F — Historical display vs interaction lifecycle separation

Refine ViewModel update APIs so a chronological historical snapshot step does not automatically erase unrelated Presentation ownership.

Conceptually separate:

```text
ApplyHistoricalSnapshot(...)
SetPresentationBusy(...)
ResetInteractionForRevision(...)
ApplyPresentationOwnership(...)
```

The exact public API names may differ.

Requirements:

- BattleId/StateRevision boundaries still invalidate stale input;
- normal Record reduction may keep input locked without destroying SelectionArea/ConsumedPendingReducer ownership;
- live binding refresh remains latest-revision-only;
- SelectionArea ownership is transient Presentation state, not frozen Gameplay truth.

### G0-G — Focused recovery-scope API

Before Group timeout is enabled, Controller must distinguish:

```text
ActivePlaybackUnit failure
ActiveEnvelope reconciliation
EntireBacklog Skip/collapse
```

Requirements:

- active-envelope reconciliation applies only that Envelope's FinalSnapshot;
- later queued Envelopes are preserved;
- user/global Skip may still collapse the entire backlog according to existing policy;
- no recovery helper has hidden broader side effects than its name/contract.

### G0-H — Status keyed reconciliation (adjacent, not Group-blocking)

Prefer after Hand reconciliation or in a follow-up patch:

```text
(TargetPresentationId, StatusId, RuntimeSequence)
→ stable Status Widget
```

Do not delay Selection Group solely for G0-H if G0-A through G0-G are complete and focused regression coverage protects status behavior.

---

## 5. G0 acceptance gates

### 5.1 Dirty-surface behavior

- Damage snapshot update does not call/reconcile Hand.
- Energy-only update does not call/reconcile Hand.
- Status-only update does not call/reconcile Hand.
- actual Hand change marks Hand dirty exactly once.
- initial baseline can request full HUD initialization.

### 5.2 Stable Hand identity

- removing A from `[A,B,C]` preserves the exact B/C Widget objects;
- adding D creates only D;
- reordering uses the frozen Hand order without recreating unchanged RuntimeIds;
- duplicate/invalid RuntimeId fails closed;
- card-request delegate is not duplicated after repeated reconcile.

### 5.3 Visual identity vs index

- historical `FromIndex` validation still rejects a wrong committed index;
- a valid RuntimeId owned by SelectionArea can satisfy historical Hand validation without requiring its visible Widget to be the Hand child at that index;
- visual lookup always resolves by exact RuntimeId/owner.

### 5.4 SelectionArea ownership

- select A: Hand formal slot becomes Hidden; SelectionArea owns the only visible A;
- deselect A: SelectionArea visual retires and formal Hand A becomes visible again;
- ordinary Hand reconcile while A is SelectionArea-owned does not steal A back;
- Confirm preserves the stable SelectionArea visual;
- no position-only reconstruction is required after a Hand snapshot change.

### 5.5 Interaction separation

- chronological historical snapshot application does not erase retained SelectionArea ownership;
- revision change still invalidates stale interactive input;
- Presentation catch-up still keeps Gameplay input fail-closed.

### 5.6 Recovery scope

- active-envelope recovery preserves a later queued Envelope;
- entire-backlog Skip still clears/collapses backlog according to existing policy;
- no intermediate refresh exposes a visually consumed card during reconciliation.

No G0 gate may be marked PASS without actual automated/build evidence.

---

## 6. Group implementation after G0

After required G0 foundations pass, continue the group design in small stages.

### G1 — Group metadata + writer-scoped correlation

- add `EPresentationGroupKind` / group tag to Presentation records;
- add writer-scoped GroupId allocation;
- counter belongs to active Resolution builder;
- stale writer allocation fails;
- SelectionRequestAction creates Action-local context for direct continuations only;
- concrete eligible card-zone Actions stamp only matching selected RuntimeIds;
- trigger reactions inherit writer but not Selection group context;
- no visible behavior change.

### G2 — Controller group discovery + interference preflight

- discover exact group members from sealed Envelope;
- require `ExpectedMemberCount > 1` for Group playback;
- validate exact-one-eligible-record-per-selected-member;
- chronological arbitrary-snapshot dry-run;
- reject future-member interference;
- group rejection falls back sequentially with SelectionArea ownership unchanged.

### G3 — Base Widget unified playback unit + timeout reconciliation

- one tracked Record-or-Group playback owner;
- token includes playback unit kind and GroupId where applicable;
- shared exact-token/deferred completion/stale callback/cancel hardening;
- dedicated active-envelope failure reconciliation;
- preserve later PlaybackQueue entries.

### G4 — Generic N-child card-transition engine

- replace global single-card transition state with per-child instances;
- SingleRecord uses one child;
- Group uses N children;
- source lookup uses exact RuntimeId visual owner;
- destination style remains committed-zone-driven;
- migrate Selection-specific Hand→DrawPile path into generic engine.

### G5 — SelectionArea + Group integration

- transactionally prepare every group child before ownership transfer;
- if child N fails, SelectionArea keeps all visible members and sequential fallback begins;
- accepted Group transfers all exact members `SelectionArea → Transition` in one playback unit;
- all children begin in the same Native tick;
- normal single-card Warcry remains SingleRecord Controller playback but uses the same generic child engine.

### G6 — `ConsumedPendingReducer`, cleanup and validation

- grouped child completion moves future members to `ConsumedPendingReducer`;
- formal historical Hand placeholders remain Hidden;
- exact reducer member clears its ownership only after applying the committed Record;
- remove superseded confirmed-center/selection-specific transfer code after equivalence is proven;
- run focused Automation and manual PIE.

---

## 7. Explicit scope boundaries

This plan does NOT authorize:

```text
new Gameplay SelectionZone
Gameplay batching/reordering for animation
trigger ordering changes
Effect/CardId-specific Presentation branches
production .uasset or .umap edits
Legacy HUD restoration/modification
```

It also does not require all adjacent UI cleanup to land before Group work. The required Group foundation is G0-A through G0-G. G0-H Status reconciliation may follow separately.

---

## 8. Recommended commit sequence

Keep patches small enough that each architectural boundary can be reviewed independently.

```text
Commit 1  G0-A change-set contract + tests
Commit 2  G0-B Hand RuntimeId reconciler + tests
Commit 3  G0-C/G0-D card visual registry + SelectionArea ownership
Commit 4  G0-E Selection interaction/destination responsibility split
Commit 5  G0-F historical/interaction lifecycle split
Commit 6  G0-G recovery-scope helper + tests

Commit 7  G1 group metadata/writer correlation
Commit 8  G2 Controller group preflight/interference
Commit 9  G3 Base playback-unit hardening
Commit 10 G4 generic N-child engine
Commit 11 G5 SelectionArea group integration
Commit 12 G6 cleanup + docs + focused regression tests
```

Combine commits only when a real compile dependency makes separation impractical. Do not combine all UI foundation and Group implementation into one refactor commit.

---

## 9. Validation order

For every stage that changes production C++:

```text
1. Development Editor build
2. smallest focused Automation prefix for changed contract
3. existing relevant Selection/Presentation regression prefix
4. manual PIE only after automated contracts pass when visual continuity/concurrency must be observed
```

Grouped parallel playback is not considered complete until manual PIE verifies:

```text
select A/B/C
→ cards visibly live in SelectionArea
→ Confirm
→ safe group starts together
→ no selected card returns to normal Hand
→ interleaved Presentation remains chronologically correct
→ input restores
```

Sequential degradation must also be visibly checked at least once:

```text
SelectionArea owns A/B/C
→ Group disabled
→ A transitions
→ B/C remain in SelectionArea
→ B transitions
→ C transitions
```

Warcry regression remains:

```text
Draw
→ SelectionArea
→ explicit Confirm
→ selected card moves to DrawPile through generic transition engine
→ Warcry ordinary PlayArea→Exhaust cleanup
```

---

## 10. Final implementation objective

The final architecture should no longer depend on full Hand rebuilds to synchronize card visuals.

Target flow:

```text
Committed historical snapshot
→ incremental HUD reconcile

RuntimeId
→ stable card visual identity
→ explicit Presentation owner

Selection
→ Hand → SelectionArea

Committed destination
→ SelectionArea/Hand → Transition

Explicit multi-member group
→ N generic transition children in parallel

Visual completion before reducer
→ ConsumedPendingReducer

Exact chronological reducer consumption
→ ownership cleared
```

The central rule is:

> Historical snapshots determine what is true; RuntimeId-keyed reconciliation preserves visual identity; Presentation ownership determines which surface may display the card; Groups only decide which committed transitions may be co-presented.