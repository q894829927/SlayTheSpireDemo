# Selection Presentation Group Implementation Plan

Date: **2026-09-09**

Status:

```text
PLANNED / OWNERSHIP-LIFECYCLE REVIEW INCORPORATED /
COMPLETION-WATERMARK + OWNERSHIP-DIRTY CONTRACT DEFINED /
G8 EARLY-INPUT / PRESENTATION-PIPELINING PHASE PLANNED /
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

A later G8 phase builds on that foundation to permit a new legal player request while older explicitly NonBlocking visual work is still running.

## 2. Staging rule

Every landed stage must preserve production behavior, not merely compile.

A stage is not acceptable if it leaves production in an intermediate state where:

```text
selected visible owner = SelectionArea
but current SingleRecord transition can only consume Hand
```

or any equivalent behavior gap.

Infrastructure may land dormant before activation. Production ownership switches only after all required downstream consumers exist.

G8 follows the same rule: early input may not be enabled until blocking interaction barriers and exact revision readiness are explicitly represented and tested.

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

G8 additionally preserves strict serial authoritative Gameplay: a newer card request may be accepted only after the older Gameplay resolution has completed and the newer exact revision is genuinely request-eligible.

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
→ exact historical dirty/change-set
→ refresh only affected historical HUD surfaces
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

The complete historical `FPresentationStateSnapshot` remains intact. This is a UI reconciliation/ownership fix, not partial historical state.

Historical dirty is **not** the future card ownership notification path. G0-C introduces a separate transient Presentation dirty channel.

### Requirements

- Damage-only change does not recreate Hand;
- Energy-only change does not recreate Hand;
- Status-only change does not recreate Hand;
- transient Preview remains on its dedicated channel;
- battle/revision replacement may still request broad historical reconciliation when genuinely necessary.

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
visibility/input → eventually derived from current Presentation ownership
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
- Hidden structural slot remains layout-present when explicitly requested by ownership infrastructure tests.

## 6. G0-C — Dormant Presentation ownership infrastructure

### Goal

Introduce a complete, independently reconcilable ownership lifecycle without yet switching production Selection visuals away from the old path.

G0-C is not complete until both of these are runtime-defined and tested:

```text
1. exact confirmed-lifecycle completion watermark
2. independent ownership mutation dirty/event channel
```

### 6.1 Logical state separation

Use a dedicated transient card Presentation ownership state logically separate from `FPresentationStateSnapshot` copy semantics.

It may be implemented as:

```text
UBattleHUDViewModel-owned struct/component
or
separate transient Presentation state object
```

but behavior must guarantee:

- normal `ApplyPresentationSnapshot()` does not overwrite/reset ownership;
- ownership has its own lifecycle identity and notification channel;
- snapshot application may reconcile ownership only after historical state copy;
- ownership state is not a second Gameplay model.

### 6.2 Ownership entry

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
    CompletionWatermark
}
```

Owner states:

```text
Hand
SelectionArea
Transition
ConsumedPendingReducer
```

### 6.3 Completion watermark

The implementation MUST NOT use `SelectionResolver no longer pending` or `pending request cleared` as lifecycle completion.

Conceptual tagged watermark:

```cpp
ESelectionPresentationCompletionMode
{
    Unresolved,
    RecordedResolution,
    DirectStateRevision
};
```

Recorded mode stores/scopes the owning/continuation `ResolutionId`.

It is reached only when that exact Resolution is formally Presentation-complete, including:

```text
normal envelope completion
FinalSnapshot reconciliation
formal collapse/recovery that marks that Resolution complete
```

Direct/no-history mode stores/scopes the authoritative post-confirm `StateRevision`/baseline edge.

It is reached only when that post-confirm state is actually displayed.

Immediately after Confirm the watermark may temporarily be unresolved. Unresolved means **not complete** and must never restore ownership to Hand.

### 6.4 Runtime reconciliation predicate

Implement behavior equivalent to:

```text
if RuntimeId absent from displayed Hand:
    clear ownership immediately

else if entry is Confirmed
     AND exact BattleId + SelectionGeneration + boundary still match
     AND CompletionWatermark is resolved
     AND CompletionWatermark is reached
     AND no accepted Transition/pending destination ownership remains:
    owner = Hand
```

Explicitly reject:

```text
request cleared → owner = Hand
resolver idle → owner = Hand
overlay closed → owner = Hand
```

### 6.5 Ownership dirty/event channel

Introduce a transient notification/change descriptor equivalent to:

```text
OnCardPresentationOwnershipChanged
or
TransientPresentationDirty(CardOwnership, ChangedRuntimeIds...)
```

It must fire independently of historical snapshot dirty state for:

```text
Hand → SelectionArea(Pending)
SelectionArea → Hand
Pending → Confirmed
SelectionArea → Transition
Transition → ConsumedPendingReducer
ownership reconciliation clear/restore
```

Formal Hand and SelectionArea presenters must be able to react immediately even if `HandCards`, `StateRevision`, HP, Energy, etc. did not change.

### 6.6 Snapshot ordering

When historical snapshot application also changes ownership through reconciliation, apply/publish in this order:

```text
copy historical displayed state
→ calculate historical dirty
→ reconcile ownership against displayed state + completion watermark
→ calculate ownership dirty
→ publish coherent/batched notifications
```

No intermediate notification may expose a duplicate visible RuntimeId or a ghost/no-owner frame.

### 6.7 Persistent SelectionAreaHost

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

### 6.8 Lifecycle scope

Reject stale ownership mutation by prior:

```text
BattleId
SelectionGeneration
boundary/revision
completion watermark scope
```

### 6.9 Critical staging rule

G0-C MUST NOT yet hide the production selected formal Hand card or make SelectionArea the production source unless the generic transition source resolver can consume SelectionArea in the same coherent landed change.

### 6.10 G0-C tests — blocking before G1

These tests are required before starting G1 production changes:

- select-style ownership mutation can emit ownership dirty without any historical snapshot change;
- deselect-style ownership mutation can emit ownership dirty without historical change;
- Pending→Confirmed ownership phase change emits ownership dirty;
- request/resolver clearing immediately after Confirm does **not** mark lifecycle complete;
- unresolved watermark does not restore owner;
- recorded watermark is reached only after exact owning Resolution completion/reconciliation;
- direct watermark is reached only after exact post-confirm StateRevision baseline is displayed;
- zero-member recorded lifecycle restores to Hand only after watermark reached;
- absent RuntimeId clears ownership immediately without waiting for watermark;
- stale BattleId/SelectionGeneration/boundary cannot mutate current entry;
- `ApplyPresentationSnapshot()` preserves ownership storage before reconciliation;
- historical dirty and ownership dirty can be published coherently with no duplicate-visible transient state.

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

### Completion watermark integration

Recorded completion watermark publication/observation must have one formal source of truth associated with Controller Resolution completion/reconciliation.

At minimum:

```text
normal CompleteActiveEnvelope
→ mark exact Resolution completion watermark reached

active-envelope timeout/failure FinalSnapshot reconciliation
→ mark exact Resolution completion watermark reached
```

Global/battle replacement paths terminate ownership by their explicit reconciliation semantics; they must not leave a stale recorded watermark half-owned by a newer battle.

### Timeout path

```text
exact group timeout
→ cancel exact group unit
→ clean visual unit
→ mark no failed member VisuallyPresented
→ ActiveEnvelope.FinalSnapshot
→ mark exact Resolution completion watermark reached
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
- recovered active envelope advances only its exact recorded completion watermark;
- global Skip still intentionally clears backlog/reconciles ownership according to its own policy.

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
- selected visual supports deselect;
- ownership dirty updates surfaces immediately without historical snapshot mutation.

### Confirm

```text
SelectionArea(Pending)
→ SelectionArea(Confirmed)
```

- Confirmed visual remains alive after overlay/backdrop closes;
- input disabled;
- authoritative SelectionResult submits;
- completion watermark stays Unresolved until exact recorded/direct outcome identity is known;
- request clearing does not restore Hand;
- do not reduce ownership to coordinates.

### SingleRecord consumption

```text
SelectionArea(Confirmed)
→ generic SingleRecord transition accepts
→ Transition
→ displayed-state reconciliation terminates/restores owner
```

### Widget decline / no-record paths

Ownership reconciliation closes the lifecycle without relying on animation:

```text
card absent from displayed Hand
→ clear immediately

card still in displayed Hand
→ restore only after exact lifecycle completion watermark reached
```

### Remove old primary behavior

`ConfirmedCardCenters`, restore-confirmed-transform and Selection-subclass destination animation stop being authoritative/primary.

Temporary compatibility data may remain only if another not-yet-migrated SingleRecord path still requires it and must have explicit deletion target in G7.

### Tests

- select/deselect ownership;
- Pending/Confirmed input behavior;
- ownership dirty updates without historical snapshot change;
- Host survives Hand reconcile;
- Host survives overlay close after Confirm;
- request clearing after Confirm does not flash card back to Hand;
- Widget decline leaves no ghost card;
- no-history/direct baseline leaves no ghost card;
- selected member with zero eligible destination restores/clears only at correct watermark;
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
→ ownership dirty emitted
→ N children begin in same Native tick
```

### Normal completion

```text
future visually consumed members
→ ConsumedPendingReducer
→ ownership dirty emitted
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
- exact reducer consumption clears only exact owner;
- Transition→ConsumedPendingReducer updates visibility through ownership dirty without requiring Hand snapshot change.

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

### Ownership lifecycle

- exact BattleId/SelectionGeneration/boundary lifecycle;
- ownership dirty independent from historical dirty;
- normal snapshot copy cannot overwrite ownership;
- request/resolver clearing does not equal lifecycle completion;
- recorded watermark reaches only at exact Resolution completion/reconciliation;
- direct watermark reaches only at exact post-confirm displayed baseline;
- unresolved watermark never restores Hand;
- historical + ownership notifications are coherent/batched.

### Ownership visuals

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
- recovered Resolution advances exact completion watermark;
- ownership reconciliation is atomic/coherent with final snapshot where required;
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

G8 may additionally affect Controller scheduling/input-readiness code and focused tests around overlapping visual jobs. Exact production files should be selected only after G0-G7 has stabilized the ownership/transition boundaries.

## 16. Explicit non-goals

This plan does not authorize:

- a Gameplay SelectionZone;
- Gameplay batching/reordering for visual convenience;
- trigger inheritance of Selection group context;
- CardId/Effect-specific destination animation;
- Widget scanning future Envelope records;
- removal/Collapse of still-historical Hand structural slots before index contracts are deliberately replaced;
- keeping both position-handoff and ownership as competing authoritative systems;
- treating request/resolver disappearance as lifecycle completion;
- making ownership updates dependent on historical snapshot publication;
- treating `Gameplay request-eligible` alone as enough to unlock input;
- marking Build/Automation/PIE PASS without execution evidence.

## 17. Start condition

Implementation should begin at **G0-A**, then **G0-B**, then **G0-C**.

G1 must not begin until G0-C has passing focused tests for:

```text
completion watermark semantics
ownership dirty/event semantics
snapshot-copy separation
stale lifecycle rejection
zero-member/no-history reconciliation
```

Do not start visible Group playback first. The UI identity/reconciliation foundation must be stable before parallel playback is enabled.

G8 does not block G0-G7. It begins only after the G0-G7 production path has fresh build/Automation/PIE evidence and the generic multi-instance transition/ownership system is stable enough to support overlapping jobs.

## 18. G8 — Presentation Pipelining / Early Input

### Goal

Support Slay-the-Spire-style responsiveness without concurrent authoritative Gameplay resolutions:

```text
play A
→ A Gameplay resolution completes
→ exact next revision becomes legally interactive
→ A still has NonBlocking visual work running
→ player plays B
→ A/B visuals overlap
```

Authoritative order remains:

```text
A Gameplay commit / triggers / resolution complete
→ B request accepted
→ B Gameplay commit / triggers / resolution
```

G8 changes Presentation scheduling and interaction readiness only.

### 18.1 G8-A — Persistent PresentationPlaybackJob scheduler

Replace the assumption that all visual lifetime is owned by one blocking active playback slot.

Introduce behavior equivalent to:

```cpp
struct FPresentationPlaybackJob
{
    FPresentationPlaybackToken Token;
    int64 BattleId;
    int64 SourceResolutionId;
    int64 SourceStateRevision;
    EPresentationInteractionPolicy InteractionPolicy;
    // visual children / ownership handles / completion state
};
```

and:

```cpp
enum class EPresentationInteractionPolicy : uint8
{
    Blocking,
    NonBlocking
};
```

Requirements:

- multiple NonBlocking jobs may coexist;
- one job cannot overwrite another job's transition instance/state;
- existing G4/G6 per-child transition engine is reused;
- stale token callbacks affect only the exact job;
- record/reducer completion and visual-job completion become distinct lifetimes.

Initial G8-A may keep all existing jobs classified Blocking. The scheduler can land dormant before early input is enabled.

### 18.2 G8-B — Explicit InteractionBarrier classification

Define which Presentation must complete before the player has a trustworthy interaction surface.

A barrier is semantic Presentation policy, not CardId-specific logic.

Likely blocking examples:

```text
Draw/Hand catch-up needed before a Selection candidate set is usable
pending Selection presentation
pending target-choice presentation
current-revision visual transfer that owns the exact card/target needed for the next interaction
terminal/recovery boundaries
```

Candidate NonBlocking examples:

```text
damage numbers
hit flashes
late played-card pile cleanup
a card already detached from Hand flying to its destination
pure cosmetic status VFX that does not define the next decision surface
```

Classification must default conservatively. Unknown/new Presentation types remain Blocking until explicitly proven safe.

### 18.3 G8-C — InteractionReadyWatermark

Introduce a runtime-comparable input-readiness watermark separate from Selection ownership completion watermark.

Behavior equivalent to:

```text
InteractionReady(BattleId, StateRevision) =
    authoritative Gameplay is request-eligible for exact revision
    AND read/display interaction state corresponds to exact revision
    AND every InteractionBarrier required for that revision is cleared
```

Input unlock is driven by this watermark, not by generic Presentation-idle state and not by Gameplay eligibility alone.

Requirements:

- stale readiness from revision R cannot unlock R+1;
- a pending Selection/target boundary remains blocking even if Gameplay has already produced the request;
- clearing an unrelated old NonBlocking job is not required for readiness;
- reaching InteractionReady is monotonic only within the exact scoped BattleId/revision lifecycle and is invalidated by battle/revision replacement according to normal rules.

### 18.4 G8-D — Enable cross-resolution NonBlocking overlap

After scheduler + barriers + readiness watermark are validated, enable a narrow set of proven-safe jobs as NonBlocking.

Target flow:

```text
Resolution A reducer/read state reaches interactive revision R
→ required barriers for R clear
→ InteractionReady(R)
→ Hand/input unlock
→ A cosmetic job continues
→ player submits B
→ Resolution B begins
```

Visual ownership must remain exact:

```text
A owner = Transition    // older job still active
B/C/D owner = Hand      // current interactive Hand
```

Older visual jobs must not steal current Hand Widget identity or block unrelated current Hand cards.

Enable categories incrementally. Do not convert every animation to NonBlocking in one patch.

### 18.5 G8-E — Recovery, skip and replacement hardening

Define exact behavior for overlapping jobs under:

```text
job timeout/failure
active-envelope recovery
global Skip
Widget replacement
battle replacement
PresentationUnavailable
```

Requirements:

- battle replacement cancels all older-battle jobs/barriers;
- stale job callbacks cannot complete a newer job/barrier;
- failure of an old NonBlocking cosmetic job does not relock already-ready current input;
- failure of a current-revision Blocking job follows explicit barrier recovery and cannot silently unlock an incomplete decision surface;
- global Skip/collapse leaves one coherent displayed authoritative state and no ghost transition visuals;
- ownership reconciliation remains scoped by exact BattleId/RuntimeId/lifecycle identity.

### 18.6 G8 acceptance gates

Automated/focused coverage must prove:

- A Gameplay resolution is complete before B request is accepted;
- B may be submitted while an explicitly NonBlocking A visual job remains alive;
- A/B visual jobs can coexist without a single global animation owner;
- input remains locked while any exact current-revision InteractionBarrier remains;
- Draw→Selection and target-choice boundaries still block until their correct visual surface exists;
- stale InteractionReady watermark cannot unlock a newer revision;
- older Transition-owned card visuals do not re-enter or corrupt the current Hand;
- old job callback cannot finish/cancel a newer job;
- old NonBlocking job failure cannot relock an already-ready newer revision;
- skip/recovery/battle replacement leaves no ghost visuals or permanent input lock;
- Gameplay/event/trigger/reducer order is identical to authored serial behavior.

Manual PIE must additionally demonstrate the intended feel:

```text
play A
→ while A's proven-safe tail animation is visibly still running
→ click/play B successfully
→ both visuals remain coherent
```

The first G8 release should keep the NonBlocking allowlist narrow and conservative. Responsiveness must never come from simply setting `bInputLocked = false` whenever Gameplay reports request eligibility.