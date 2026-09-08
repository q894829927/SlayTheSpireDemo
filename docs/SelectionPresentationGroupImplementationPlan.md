# Selection Presentation Group Implementation Plan

Date: **2026-09-09**

Status:

```text
PLANNED / OWNERSHIP-LIFECYCLE REVIEW INCORPORATED /
UI-FOUNDATION-FIRST / BEHAVIOR-SAFE STAGING /
NO PRODUCTION IMPLEMENTATION YET / NOT VALIDATED / NOT SEALED
```

Related contracts:

- `docs/SelectionPresentationGroupDesign.md`
- `docs/CardSelectionPresentationConstraints.md`
- `docs/CardSelectionRefactorConstraints.md`
- `Source/SlayTheSpireDemo/Presentation/AGENTS.md`
- `Source/SlayTheSpireDemo/UI/AGENTS.md`

## 1. Objective

Deliver a reusable card-selection Presentation system in which:

```text
Hand
→ SelectionArea
→ Transition
→ ConsumedPendingReducer
→ Done
```

is the stable visual-ownership lifecycle, and a validated explicit Selection Presentation Group can run N selected-card destination animations concurrently without changing Gameplay or reducer chronology.

The implementation must also correct the UI architecture issues that caused the current multi-select flashback:

- coarse whole-HUD refresh;
- recreate-all Hand reconciliation;
- visual identity coupled to array index;
- Selection Widget owning destination animation;
- globally single-instance card animation state;
- implicit recovery scope;
- historical state and transient interaction/ownership lifecycle coupled too tightly.

## 2. Staging rule

Every landed stage must preserve production behavior, not merely compile.

A stage is not acceptable if it leaves production in an intermediate state where:

```text
selected visible owner = SelectionArea
but current SingleRecord transition can only consume Hand
```

or any equivalent behavior gap.

Infrastructure may land dormant before activation. Production ownership switches only after all required downstream consumers exist.

## 3. Existing architecture to preserve

Do not weaken these existing contracts while refactoring:

- Gameplay remains deterministic and authoritative;
- Selection candidate capture occurs at Execute time;
- Player Selection still requires explicit Presentation boundary and explicit Confirm;
- trigger reaction ordering remains authored;
- Presentation consumes immutable committed facts;
- Controller owns chronological reducer sequencing;
- Base Widget exact-token/deferred-callback/cancellation hardening remains the Controller-facing visual boundary;
- no Effect/CardId-specific Presentation branches;
- production `.uasset` / `.umap` changes are not required by this redesign.

## 4. G0-A — Incremental ViewModel/HUD dirty propagation

### Goal

Stop treating every ViewModel change as permission to rebuild every formal HUD surface.

Current artifact:

```text
ApplyPresentationSnapshot
→ BroadcastChanged
→ RefreshHUDFromViewModel
→ RefreshHand + RefreshStatuses + ...
```

Target:

```text
old displayed state + new displayed state
→ exact dirty/change-set
→ refresh only affected surfaces
```

### Design

Introduce behavior equivalent to dirty flags/change descriptors for at least:

```text
Hand
Combatants
Statuses
Energy
PileCounts
Input
Feedback
Intent
Terminal
PresentationAvailability
```

The complete historical `FPresentationStateSnapshot` remains intact. This is a UI reconciliation optimization/ownership fix, not partial historical state.

### Requirements

- Damage-only change does not recreate Hand;
- Energy-only change does not recreate Hand;
- Status-only change does not recreate Hand;
- transient Preview remains on its dedicated channel;
- battle/revision replacement may still request broad reconciliation when genuinely necessary.

### Tests

- Hand Widget identity unchanged across Damage/Energy/Status-only publication;
- correct affected surfaces still refresh;
- PresentationUnavailable/terminal transitions remain correct.

## 5. G0-B — RuntimeId-keyed Hand reconcile

### Goal

Replace unconditional:

```text
HB_Hand.ClearChildren
→ recreate every card
```

with stable RuntimeId-keyed reconciliation.

### Target algorithm

```text
Old formal RuntimeIds
vs
New HandCards RuntimeIds

survivor → reuse exact Widget
missing  → remove exact Widget
added    → create exact Widget
order    → reconcile to frozen Hand order
visibility/input → derived from current Presentation ownership
```

### Historical index rule

Keep:

```text
Hand index = historical record/snapshot validation
```

Stop using index as primary live visual identity:

```text
RuntimeId = current visual identity
```

### Tests

- surviving B/C retain object identity when A leaves Hand;
- added/removed RuntimeIds reconcile correctly;
- frozen order/index remains exact;
- child count remains equal to historical Hand count;
- Hidden structural slot remains layout-present.

## 6. G0-C — Dormant Presentation ownership infrastructure

### Goal

Introduce the ownership model without yet switching production Selection visuals away from the old path.

### Infrastructure

Behavior equivalent to:

```cpp
FCardPresentationOwnershipEntry
{
    BattleId
    SelectionGeneration
    SelectionBoundaryRevision
    RuntimeId
    Owner
    Phase // Pending / Confirmed where applicable
}
```

Owner states:

```text
Hand
SelectionArea
Transition
ConsumedPendingReducer
```

### Persistent SelectionAreaHost

Create a runtime persistent visual layer:

```text
SelectionAreaHost
- NOT HB_Hand
- NOT OV_PlayArea
- survives Hand reconciliation
- survives interactive overlay close
- stable HUD coordinate space
```

The Host may exist but production selected cards remain on the current path until G4/G5.

### Ownership reconciliation

Implement/test reconciliation independently of animation:

```text
owned RuntimeId absent from displayed Hand
→ clear ownership / stale visual
```

```text
owned RuntimeId still in displayed Hand
AND confirmed lifecycle definitively ended
AND no destination pending
→ owner back to Hand
```

### Lifecycle scope

Reject stale ownership mutation by prior:

```text
BattleId
SelectionGeneration
boundary/revision
```

### Critical staging rule

G0-C MUST NOT yet hide the production selected formal Hand card or make SelectionArea the production source unless the generic transition source resolver can consume SelectionArea in the same coherent landed change.

## 7. G1 — PresentationGroup metadata + writer-scoped correlation

### Goal

Add explicit committed correlation with no visible behavior change.

### Work

- add `EPresentationGroupKind` / tag fields or equivalent;
- add writer-scoped `TryAllocatePresentationGroupId`;
- group counter belongs to active Resolution builder;
- stale writer validation uses writer BattleId/ResolutionId;
- `USelectionRequestAction` creates object-type-neutral direct-continuation context;
- current-Hand selected RuntimeSequences map to exact card RuntimeIds;
- eligible direct continuation Actions stamp matching group metadata;
- trigger reactions inherit writer only, never group context.

### Eligibility invariant

Initial group requires:

```text
one selected RuntimeSequence
→ exactly one eligible direct CardZoneChanged member
```

0/2-member shapes disable grouping later.

### Tests

- stale writer cannot allocate in newer Resolution;
- two Selection decisions get distinct IDs in same Resolution;
- trigger records ungrouped;
- canonical selection order, not click order, defines selected identities;
- no-history Gameplay unchanged.

## 8. G2 — Controller semantic Group discovery, dry-run and interference

### Goal

Teach Controller which groups are semantically safe while leaving visible Group playback disabled.

### Controller may inspect

Only sealed Envelope facts:

- group metadata;
- expected member count;
- member record type/zone shape;
- RuntimeId uniqueness;
- `PresentationSequence` validity;
- exact-one-member invariant;
- chronological reducer dry-run;
- future-member interference.

### Controller must not inspect

- SelectionArea ownership;
- concrete Widget existence;
- geometry;
- destination anchors;
- Selection Presenter state.

### Interference

At minimum:

```text
interleaved CardZoneChanged touching future member → reject group
interleaved CardPlayed touching future member      → reject group
```

Unrelated Damage/Status/Energy/other-card records do not reject merely by interleaving.

### Tests

- contiguous 3-member group validates;
- unrelated non-contiguous group validates;
- future member leave/return Hand causes semantic visual rejection even if reducer dry-run succeeds;
- malformed/incomplete groups sequentially disabled;
- `ExpectedMemberCount <= 1` not offered as Group playback.

## 9. G3 — Base Widget playback-unit hardening + recovery scopes

### Goal

Upgrade Controller↔Widget protocol from one Record to one playback unit:

```text
SingleRecord OR Group
```

without weakening exact-token hardening.

### Base Widget

One tracked owner, not independent Record/Group owners.

Token behavior includes unit kind and GroupId where applicable.

Preserve:

- track before concrete Begin;
- deferred synchronous completion forwarding;
- exact stale/duplicate rejection;
- exact cancel;
- timeout bound to current generation/token;
- Widget replacement safety.

### Recovery scopes

Implement behaviorally distinct scopes equivalent to:

```text
ActivePlaybackUnit
ActiveEnvelope
EntireBacklog
```

Group timeout requires ActiveEnvelope reconciliation that preserves later queued envelopes.

### Timeout path

```text
exact group timeout
→ cancel exact group unit
→ clean visual unit
→ mark no failed member VisuallyPresented
→ ActiveEnvelope.FinalSnapshot
→ ownership reconciliation
→ active envelope complete
→ later backlog preserved
```

No leader-only `CompleteActiveRecord()` behavior.

### Tests

- stale Group callback cannot finish newer SingleRecord;
- stale SingleRecord callback cannot finish newer Group;
- exact cancellation;
- active-envelope recovery preserves backlog;
- global Skip still intentionally clears backlog.

## 10. G4 — Generic card transition engine + SelectionArea-capable source resolver

### Goal

Replace destination animation special cases/single global moving-card state with one generic per-child engine before switching production Selection ownership.

### Child state

Behavior equivalent to:

```cpp
FCardTransitionInstance
{
    RuntimeId
    FromZone
    ToZone
    MovingVisual
    start/end geometry
    opacity/scale
    elapsed
}
```

### Source resolver

Must support:

```text
owner == Hand          → exact Hand visual
owner == SelectionArea → exact SelectionArea visual
```

`ConsumedPendingReducer` must not replay.

### Destination resolver

Driven only by committed facts:

- Exhaust;
- DrawPile;
- Discard when current generic support is ready;
- existing played-card destination behavior as appropriate.

### Migration order

Before production SelectionArea switch, migrate existing SingleRecord paths to the generic engine and prove behavior parity.

Warcry remains SingleRecord and is a key migration target once SelectionArea source support exists.

### Tests

- existing Hand→Exhaust parity;
- existing Hand→Discard parity;
- Hand→DrawPile visible movement parity;
- source resolver can consume a test SelectionArea visual;
- SingleRecord still uses one child;
- no CardId/Effect branch.

## 11. G5 — Production SelectionArea ownership switch

### Goal

Make the new ownership model production-visible only after G4 can consume it safely.

### Select

```text
Hand → SelectionArea(Pending)
```

- formal Hand slot stays Hidden;
- visible exact card lives in SelectionAreaHost;
- selected visual supports deselect.

### Confirm

```text
SelectionArea(Pending)
→ SelectionArea(Confirmed)
```

- Confirmed visual remains alive after overlay/backdrop closes;
- input disabled;
- authoritative SelectionResult submits;
- do not reduce ownership to coordinates.

### SingleRecord consumption

```text
SelectionArea(Confirmed)
→ generic SingleRecord transition accepts
→ Transition
→ displayed-state reconciliation terminates/restores owner
```

### Widget decline / no-record paths

Ownership reconciliation must close the lifecycle without relying on animation.

### Remove old primary behavior

`ConfirmedCardCenters`, restore-confirmed-transform and Selection-subclass destination animation stop being authoritative/primary.

Temporary compatibility data may remain only if another not-yet-migrated SingleRecord path still requires it and must have explicit deletion target in G7.

### Tests

- select/deselect ownership;
- Pending/Confirmed input behavior;
- Host survives Hand reconcile;
- Host survives overlay close after Confirm;
- Widget decline leaves no ghost card;
- no-history/direct baseline leaves no ghost card;
- selected member with zero eligible destination restores/clears correctly;
- Warcry SingleRecord SelectionArea→DrawPile works before Group parallel is enabled.

## 12. G6 — N-child Group playback + ConsumedPendingReducer

### Goal

Enable true parallel selected-card destination playback.

### Visual transactional preflight

Controller has already semantically validated the group. Concrete HUD now validates visual state:

- exact owner exists;
- visible object valid;
- geometry valid;
- destination valid;
- every child constructable.

No durable ownership transfer occurs during partial preparation.

Child N failure:

```text
rollback every prepared child
→ zero ownership transferred
→ return false
→ sequential SingleRecord fallback
```

### Accepted Group

```text
A/B/C SelectionArea→Transition atomically
→ N children begin in same Native tick
```

### Normal completion

```text
future visually consumed members
→ ConsumedPendingReducer
→ Base Widget exact Group completion
```

Controller marks exact record indices VisuallyPresented and resumes chronological reducer from leader.

When B/C reducer records arrive later, no visible Begin occurs; displayed-state reconciliation clears exact ownership after historical consumption.

### Sequential fallback

If Group is unavailable/unsafe/declined before ownership transfer:

```text
A/B/C stay SelectionArea(Confirmed)
→ A SingleRecord
→ B/C remain SelectionArea across Hand reconcile
→ B SingleRecord
→ C SingleRecord
```

No confirmed-position reconstruction.

### Tests

- N children begin same tick;
- second/Nth child prepare failure transfers zero owner;
- sequential fallback does not flash B/C to Hand;
- interleaved unrelated records still play later in chronological order;
- ConsumedPendingReducer survives intermediate snapshots;
- exact reducer consumption clears only exact owner.

## 13. G7 — Cleanup, optional Status reconcile, validation and seal preparation

### Mandatory cleanup

Delete or retire superseded production paths after equivalence is proven:

- confirmed-position handoff as primary continuity;
- Hand rebuild transform restoration;
- Selection-subclass Hand→DrawPile destination ownership;
- duplicate independent suppression truth;
- obsolete single-global-card animation fields/helpers replaced by transition instances.

### Optional adjacent Status cleanup

Status rows currently have a similar recreate-all pattern. If scope/risk allows, add exact identity reconciliation keyed by:

```text
TargetPresentationId + StatusId + RuntimeSequence
```

This is not required to enable Selection Group and must not expand the critical path unnecessarily.

### Validation

Fresh evidence is required after production code changes:

- Development Editor build;
- focused Selection Presentation Automation;
- C0 multi-select regressions affected by shared Selection changes;
- C1 DrawPileTop/Warcry regressions;
- existing generic zone/played-card cleanup regressions;
- manual PIE for true parallel timing/no-flash and Warcry sequence.

No prior validation may be reused after relevant shared UI/Presentation code changes.

## 14. Acceptance matrix

### Foundation

- non-Hand changes do not rebuild Hand;
- surviving Hand RuntimeId preserves Widget object identity;
- historical child count/index remains exact;
- visible lookup is RuntimeId/owner-based.

### Ownership

- exact BattleId/SelectionGeneration lifecycle;
- Hand↔SelectionArea Pending select/deselect;
- Confirmed owner survives overlay close;
- stale generation cannot mutate current owner;
- Widget decline/no-history/zero-member/global recovery reconciles ownership;
- no ghost SelectionArea card.

### SingleRecord

- generic engine handles Hand source;
- generic engine handles SelectionArea source;
- Warcry selected card visibly moves SelectionArea→DrawPile;
- Warcry played card later uses ordinary generic Exhaust.

### Group

- semantic validation is Controller-only immutable data;
- visual validation is Widget/Presenter-only transactional state;
- safe multi-member transitions start together;
- unsafe future-member interference degrades;
- sequential degradation stays in SelectionArea;
- future consumed members never reappear in Hand.

### Recovery

- Group timeout does not leader-complete;
- active-envelope recovery preserves later backlog;
- ownership reconciliation is atomic with final snapshot where required;
- stale callbacks cannot cross playback-unit ownership.

## 15. Files likely affected during implementation

Expected production areas include, subject to actual compile dependencies:

```text
Source/SlayTheSpireDemo/Presentation/PresentationTypes.*
Source/SlayTheSpireDemo/Presentation/BattlePresentationRecorder.*
Source/SlayTheSpireDemo/Presentation/BattlePresentationController.*
Source/SlayTheSpireDemo/Actions/SelectionRequestAction.*
selected card-zone Actions that emit eligible CardZoneChanged records
Source/SlayTheSpireDemo/UI/BattleHUDViewModel.*
Source/SlayTheSpireDemo/UI/BattleHUDWidgetBase.*
Source/SlayTheSpireDemo/UI/BattleHUDWidget.*
Source/SlayTheSpireDemo/UI/BattleHUDSelectionWidget.*
Source/SlayTheSpireDemoTests/Private/CardSelectionPresentationTests.cpp
related focused Controller/Presentation tests as needed
```

No card/map production asset change is implied.

## 16. Explicit non-goals

This plan does not authorize:

- a Gameplay SelectionZone;
- Gameplay batching/reordering for visual convenience;
- trigger inheritance of Selection group context;
- CardId/Effect-specific destination animation;
- Widget scanning future Envelope records;
- removal/Collapse of still-historical Hand structural slots before index contracts are deliberately replaced;
- keeping both position-handoff and ownership as competing authoritative systems;
- marking Build/Automation/PIE PASS without execution evidence.

## 17. Start condition

Implementation should begin at **G0-A**, then **G0-B**.

Do not start visible Group playback first. The UI identity/reconciliation foundation must be stable before parallel playback is enabled.