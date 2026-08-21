# Phase 6UI-A2D — Status / Terminal Committed Presentation

Status: **DESIGN LOCKED / IMPLEMENTATION NOT STARTED**.

UI-A2D extends the established UI-A2A transport/lifecycle, UI-A2B committed Damage/Block presentation, and UI-A2C card/energy/zone committed presentation. It does not redesign Resolution, Envelope, RecordWriter, Controller backlog, PlaybackToken, PresentationUnavailable, immutable FinalSnapshot ownership, or fail-soft no-partial-history behavior.

Blueprint visual integration and PIE smoke remain intentionally deferred until the complete A2B + A2C + A2D C++ presentation pipeline is validated.

---

## 1. A2D scope

A2D owns exactly these additions:

```text
StatusChanged + Reason
Status mutation CommitResult / exact-instance remove path
Status historical RuntimeSequence identity
Status frozen Before/After dynamic presentation values
Status WorkingPresentationSnapshot reducer
formal Victory / Defeat visual playback
formal ResolutionFault visual playback
terminal WorkingSnapshot transition after playback completion
combined C++ / Automation acceptance
```

A2D does **not**:

```text
create a second status system
query mutable Gameplay from Blueprint during playback
reorder existing trigger/reaction execution
invent new Gameplay StatusApplied events for Presentation
turn PresentationUnavailable into ResolutionFault
make Blueprint an authoritative state owner
```

---

## 2. Status Record taxonomy

Use one new Record Type:

```text
StatusChanged
```

Do not split into:

```text
StatusApplied
StatusIncreased
StatusReduced
StatusRemoved
```

The semantic cause is carried by:

```cpp
UENUM(BlueprintType)
enum class EStatusChangeReason : uint8
{
    Applied,
    Increased,
    Reduced,
    TurnEndDecay,
    Removed
};
```

`StatusChanged` must be appended to the end of the existing `EBattlePresentationRecordType` enum. Existing enum entries must not be reordered merely for grouping/readability.

`Reason` is the player-facing semantic classification of the committed change; payload fields describe **what actually happened**.

The enum intentionally contains both operation classifications and one currently required causal specialization:

```text
Applied / Increased / Reduced / Removed
= classification derived from the committed mutation

TurnEndDecay
= concrete cause preserved for the ordinary reduction performed by turn-end lifecycle
```

Therefore callers do not use `Reason` as arbitrary display prose. Amount/membership facts remain authoritative even when a causal specialization is present.

Examples:

```text
Applied
Amount 0 → 2
bCreated = true
bRemoved = false

Increased
Amount 2 → 4
bCreated = false
bRemoved = false

Reduced
Amount 4 → 3
bCreated = false
bRemoved = false

Reduced
Amount 3 → 0
bCreated = false
bRemoved = true

TurnEndDecay
Amount 1 → 0
bCreated = false
bRemoved = true

Removed
Amount 3 → 0
bCreated = false
bRemoved = true
```

A TurnEnd decay that reaches zero remains `Reason=TurnEndDecay`; it must not be rewritten to `Removed` merely because `bRemoved=true`.

`Removed` is reserved for explicit remove semantics, primarily `URemoveStatusAction`.

---

## 3. Status historical identity

Concrete historical identity is:

```text
BattleId
+ TargetPresentationId
+ StatusId
+ RuntimeSequence
```

`TargetPresentationId + StatusId` alone is insufficient.

Example:

```text
Weak#10 removed
↓
Weak#15 recreated later
↓
late/stale Weak#10 record arrives
```

Without RuntimeSequence, old history could mutate/delete the new runtime instance.

### 3.1 RuntimeSequence type boundary

Gameplay keeps its existing unsigned identity:

```cpp
uint64 RuntimeSequence;
```

Presentation / Blueprint-facing structs use:

```cpp
int64 RuntimeSequence;
```

Every Gameplay → Presentation conversion must validate:

```text
RuntimeSequence > 0
RuntimeSequence <= MAX_int64
```

If Gameplay committed and a current valid Writer exists but RuntimeSequence cannot be safely represented, the current unpublished Presentation history becomes invalid. Gameplay is not rolled back.

### 3.2 RuntimeSequence is monotonic, not contiguous

RuntimeSequence guarantees:

```text
unique for created runtime instances
monotonically increasing across allocations
```

It does **not** guarantee contiguous `+1` identities.

A candidate sequence can be consumed/abandoned by a merge or no-op path. Tests must never require:

```text
next created RuntimeSequence == previous + 1
```

They may require only:

```text
next created RuntimeSequence > previous created RuntimeSequence
```

---

## 4. Frozen Status presentation payload

Add:

```cpp
USTRUCT(BlueprintType)
struct FStatusChangedPresentationPayload
{
    GENERATED_BODY()

    FName SourcePresentationId;
    FName TargetPresentationId;

    FName StatusId;
    int64 RuntimeSequence;

    int32 AmountBefore;
    int32 AmountAfter;

    bool bCreated;
    bool bRemoved;
    EStatusChangeReason Reason;

    FText DisplayName;
    FText DescriptionBefore;
    FText DescriptionAfter;

    bool bUseAtlasIcon;
    FVector2D UVOffset;
    FVector2D UVScale;
    FVector2D TrimOffset;
    FVector2D TrimScale;
};
```

The Record must be self-sufficient for historical playback. Blueprint must not query `UStatusInstance`, `UStatusData`, `ACombatant`, BattleManager status queries, or mutable live status state.

### 4.1 Dynamic description Before/After

Both descriptions are required because dynamic Status text can change when Amount changes.

Example:

```text
Weak 2 → 1

DescriptionBefore:
攻击伤害降低25%，持续2回合。

DescriptionAfter:
攻击伤害降低25%，持续1回合。
```

Creation:

```text
DescriptionBefore = Empty
DescriptionAfter  = resolved current description
```

Removal:

```text
DescriptionBefore = resolved pre-removal description
DescriptionAfter  = Empty
```

### 4.2 Description capture timing

The implementation must not temporarily mutate the Status amount backward to reconstruct historical text. Because the current text resolver reads a live `UStatusInstance`, the pre-mutation description must be captured **before** the Container changes that instance.

Apply capture order is:

```text
read-only lookup of PreExistingInstance by incoming StatusId
↓
if PreExistingInstance exists
    capture its RuntimeSequence / EffectiveDefinition
    ResolveStatusDescription(PreExistingInstance)
    → DescriptionBefore
else
    DescriptionBefore = Empty
↓
Container Apply mutation decides create / merge / no-op / invalid
↓
if committed merge
    Result.EffectiveInstance and RuntimeSequence must match the pre-capture
if committed create
    no pre-existing instance may have been captured
↓
if committed instance still exists
    ResolveStatusDescription(Result.EffectiveInstance)
    → DescriptionAfter
else
    DescriptionAfter = Empty
```

The read-only pre-lookup is only a historical text capture. It must not decide create versus merge, allocate identity, or compete with `StatusContainer` mutation authority. The returned `FStatusMutationResult` remains authoritative.

Reduce/remove capture order is:

```text
validate and capture DescriptionBefore from the exact ExpectedInstance
↓
Container exact-instance mutation
↓
validate Result identity against the captured instance
↓
resolve DescriptionAfter from Result.EffectiveInstance, or Empty when removed
```

If Gameplay commits but the returned identity contradicts the pre-captured identity while a writer is active, the Action must treat that as invalid Presentation history, invalidate the current record batch and preserve the committed Gameplay result. It must not publish a record assembled from mismatched before/after instances.

For creation there is no pre-existing effective runtime instance, so `DescriptionBefore` is Empty.

### 4.3 EffectiveDefinition rule

If the incoming request uses a different `UStatusData*` but the same `StatusId` as an already-existing runtime Status, current Gameplay merges into the **existing instance**.

Therefore frozen Presentation data must use:

```text
EffectiveInstance->GetDefinition()
```

not blindly use the Definition supplied by the new apply request.

`DisplayName`, Description, atlas values and every frozen status visual field must come from the true `EffectiveDefinition` that owns the runtime instance.

---

## 5. Source / Target PresentationId rules

Status payload follows the same authoritative participant-ID rules as A2B.

```text
Source == nullptr
→ SourcePresentationId == NAME_None

Source valid
→ SourcePresentationId must be non-empty
→ must come from BattleManager resolver

Target valid
→ TargetPresentationId must be non-empty
→ must come from BattleManager resolver
```

A null Source carrying a fake/non-empty ID is invalid Presentation history.

A valid Source carrying `NAME_None` is invalid Presentation history.

Target must always represent an active battle participant and resolve to a trustworthy non-empty ID.

Writer absent from the start remains legal no-history mode.

---

## 6. Gameplay Status MutationResult

Container returns Gameplay mutation truth without depending on Recorder/Presentation.

Recommended types:

```cpp
enum class EStatusMutationOutcome : uint8
{
    Invalid,
    NoOp,
    Committed
};

struct FStatusMutationResult
{
    EStatusMutationOutcome Outcome;

    FName StatusId;
    uint64 RuntimeSequence;

    int32 AmountBefore;
    int32 AmountAfter;

    bool bCreated;
    bool bRemoved;

    UStatusInstance* EffectiveInstance;
    UStatusData* EffectiveDefinition;
};
```

`EffectiveInstance` and `EffectiveDefinition` are synchronous Action/Battle-layer consumption references only. They must never be stored in a Presentation Record, Envelope, Controller backlog or other asynchronous cache. Records freeze value data before the Action finishes.

For exact removal, the caller's exact `ExpectedInstance` reference must remain valid long enough to freeze pre-mutation values. After the mutation, result references are consumed synchronously only; later playback never depends on the removed UObject remaining alive.

Committed result invariants are:

```text
AmountBefore >= 0
AmountAfter  >= 0

bCreated == (AmountBefore == 0 AND AmountAfter > 0)
bRemoved == (AmountBefore > 0 AND AmountAfter == 0)
bCreated and bRemoved are never both true

otherwise Committed requires AmountBefore != AmountAfter
```

Any result that violates these invariants is unusable Presentation history. If Gameplay has already committed and a writer is active, invalidate the current record batch without rolling Gameplay back or requesting Gameplay ResolutionFault.

`StatusContainer` owns only Gameplay mutation semantics.

Action / Battle layer owns:

```text
Source
Target
Reason
PresentationId resolution
Before/After text freezing
atlas freezing
Record append
```

Container must not include or depend on Recorder / RecordWriter / Presentation types.

---

## 7. Mutation outcome semantics

### 7.1 Invalid

Examples:

```text
invalid Owner
invalid Definition
StatusId == None
Amount <= 0
CandidateRuntimeSequence == 0 for any Apply call that reaches the Container
failed UObject creation
structurally invalid expected instance arguments
```

Result:

```text
Outcome = Invalid
no committed Gameplay mutation
no StatusChanged Record
Action finishes fail-soft unless a separate framework invariant is violated
```

### 7.2 NoOp

Examples:

```text
Reduce exact instance no longer exists in Container
Remove exact instance no longer exists in Container
Apply tries to increase MAX_int32 but Amount remains MAX_int32
```

Result:

```text
Outcome = NoOp
no Record
not a Gameplay ResolutionFault
```

A stale exact-instance mutation must never silently retarget a replacement instance with the same StatusId.

Every Apply invocation that reaches `StatusContainer` supplies a non-zero `CandidateRuntimeSequence`, including merge and no-op attempts. The candidate is consumed as deterministic allocation input but becomes the runtime identity only when a new instance is created. Merge/no-op results report the existing instance's actual `RuntimeSequence`; they never publish the unused candidate as effective identity. Gaps in the battle-scoped allocator are therefore valid and expected.

Example:

```text
Weak#10 ReduceAction queued
↓
Weak#10 removed
↓
Weak#15 recreated
↓
old ReduceAction executes
↓
Weak#15 remains untouched
↓
Outcome = NoOp
no Record
```

### 7.3 Committed

Committed requires a real authoritative change:

```text
AmountBefore != AmountAfter
OR
membership actually changed by create/remove
```

`UStatusInstance::AddAmount()` returning true is not sufficient by itself. If amount was already `MAX_int32` and remains unchanged, the mutation is `NoOp`, not `Committed` and not `Increased`.

---

## 8. Reason ownership

Reason cannot be treated as an arbitrary display label supplied by every caller.

### 8.1 Apply path

Apply semantics are derived from real MutationResult:

```text
bCreated == true
→ Reason = Applied

bCreated == false
AND Outcome == Committed
AND AmountAfter > AmountBefore
→ Reason = Increased
```

Callers do not choose between Applied and Increased.

### 8.2 Reduce path

Reduce action may carry semantic cause:

```text
ordinary reduction
→ Reason = Reduced

turn-end reaction reduction
→ Reason = TurnEndDecay
```

If either reduction reaches zero:

```text
bRemoved = true
Reason remains the original cause
```

`UReduceStatusAction` must validate its requested reason **before** mutation. The first implementation accepts only:

```text
Reduced
TurnEndDecay
```

Any other reason is an invalid Action request: no Gameplay mutation and no Record. Do not commit first and then discover that Presentation reason was invalid.

### 8.3 Explicit remove path

Only explicit remove semantics use:

```text
Reason = Removed
```

---

## 9. Exact-instance Remove entry point

Add a formal queued Gameplay path:

```text
URemoveStatusAction
```

Recommended initialization:

```cpp
Initialize(
    ABattleManager* Battle,
    ACombatant* Source,
    ACombatant* Target,
    UStatusInstance* ExpectedInstance
);
```

`URemoveStatusAction` always derives:

```text
Reason = Removed
```

The caller cannot supply a different reason to the explicit-remove Action.

Container adds:

```text
RemoveStatus(ExpectedInstance)
→ FStatusMutationResult
```

The exact pointer/runtime identity must be verified against the current Container before removal.

`RemoveStatusById()` may remain as synchronous setup/test/convenience API, but a queued Action must **not** store only `StatusId`, because that could delete a newer same-name runtime instance created after the Action was queued.

---

## 10. ReduceStatusAction formal context

Current simplified Reduce Action must evolve to carry the data required for committed Presentation.

Recommended initialization:

```cpp
Initialize(
    ABattleManager* Battle,
    ACombatant* Source,
    ACombatant* Target,
    UStatusInstance* ExpectedInstance,
    int32 AmountToRemove,
    EStatusChangeReason Reason
);
```

The Action continues to receive the explicit optional `FPresentationRecordWriter` through the established `UBattleAction` mechanism.

Do not world-search the Recorder.

### 10.1 TurnEndDecay source/target

Turn-end status decay uses:

```text
Source = Owner
Target = Owner
Reason = TurnEndDecay
```

This provides deterministic battle participant identity while preserving the true semantic reason.

A future explicit system mutation may use:

```text
Source = nullptr
SourcePresentationId = NAME_None
```

provided all existing null-source invariants are preserved.

---

## 11. Status/Event/Reaction ordering

General mutation ordering is:

```text
Status Gameplay Commit
↓
StatusChanged Record
↓
if this Gameplay mechanism defines a BattleEvent
    Dispatch Event
↓
trigger reactions
↓
reaction Actions execute
↓
subsequent Records
```

A2D must not create new Gameplay events solely so Presentation has an event to observe.

### 11.1 ApplyStatus

If current ApplyStatus has no dedicated StatusApplied BattleEvent, A2D keeps it that way.

```text
Apply commit
↓
StatusChanged
↓
Finish
```

### 11.2 Turn-end decay

Existing turn-end semantics remain authoritative:

```text
TurnEndedAction
↓
PlayerTurnEnded / EnemyTurnEnded Event Dispatch
↓
trigger eligibility snapshot + deterministic sort
↓
ReduceStatusAction inserted
↓
Reduce commit
↓
StatusChanged(TurnEndDecay)
```

The TurnEnded Event happens before the reaction Status mutation. StatusChanged is emitted when the queued reaction Action actually commits.

### 11.3 Multiple reacting statuses

Record ordering continues to follow existing Gameplay reaction order:

```text
Trigger Priority
→ RuntimeSequence
→ LocalTriggerIndex
```

Presentation must never resort those committed Records for visual convenience.

---

## 12. `FBattleHUDStatusView` identity extension

Add:

```cpp
int64 RuntimeSequence;
```

to `FBattleHUDStatusView`.

The frozen combatant snapshot must carry exact runtime status identity rather than only `StatusId`.

The status view remains presentation-only immutable value data. It must not contain live `UStatusInstance*` / `UStatusData*` references for historical playback.

---

## 13. Frozen Status ordering

Final frozen Status arrays must be explicitly sorted by:

```text
RuntimeSequence ascending
```

Do not rely on current `StatusContainer` array insertion order merely because it happens to match today.

Do not sort by:

```text
StatusId
DisplayName
localized Description
Amount
Widget creation order
```

WorkingSnapshot reducer must preserve the same sequence ordering.

---

## 14. Controller Status reducer

Status reducer finds a concrete entry using the full identity:

```text
TargetPresentationId
+ StatusId
+ RuntimeSequence
```

The reducer uses only immutable Record fields and WorkingPresentationSnapshot. It must not query mutable Gameplay.

### 14.1 Create

```text
bCreated == true
↓
validate no existing exact identity
↓
create FBattleHUDStatusView from frozen payload
↓
insert so Statuses stays RuntimeSequence ascending
```

### 14.2 Update / increase / reduce

```text
find exact identity
↓
validate Current.Amount == AmountBefore
↓
Amount = AmountAfter
Description = DescriptionAfter
DisplayName / atlas values remain the frozen values from Record
```

### 14.3 Remove

```text
bRemoved == true
↓
find exact identity
↓
validate Current.Amount == AmountBefore
↓
remove that exact runtime status entry
```

A stale Record for an old RuntimeSequence must never modify a newer status instance with the same StatusId.

### 14.4 Reducer mismatch

Any historical mismatch such as:

```text
exact status identity missing when mutation expects it
AmountBefore does not match WorkingSnapshot
creation identity already exists
invalid RuntimeSequence
invalid TargetPresentationId
```

means the incremental history is no longer trusted.

Behavior:

```text
stop incremental reducer
↓
collapse to immutable Envelope.FinalSnapshot
↓
Gameplay remains unchanged
↓
no Gameplay ResolutionFault is manufactured
```

---

## 15. Status reducer projection boundary

Status mutation can indirectly affect other player-facing derived values, including:

```text
card dynamic descriptions
enemy intent resolved damage preview
playability / legal-target derived state
other future preview projections
```

The A2D Status reducer does **not** recalculate those fields from live Gameplay.

Status reducer owns only the status projection itself:

```text
Combatant.Statuses[] identity/order
Amount
Description
DisplayName/icon metadata
```

Other derived fields may remain at their previous historical value until the Envelope `FinalSnapshot` reconciliation.

This is intentional and avoids introducing a second mutable prediction system.

---

## 16. Terminal payloads

A2D formalizes typed terminal payloads.

### 16.1 Victory / Defeat

Add:

```cpp
USTRUCT(BlueprintType)
struct FTerminalPresentationPayload
{
    GENERATED_BODY()

    FName WinnerPresentationId;
    FName DefeatedPresentationId;
};
```

Use this payload for `Victory` and `Defeat` Records.

UMG chooses localized title/text from Record Type; Gameplay does not generate presentation strings such as “Victory” or “Defeat”.

### 16.2 ResolutionFault

Replace the existing scattered root fields:

```text
FaultReason
FaultExecutedActionCount
FaultLastActionName
```

with one typed payload:

```cpp
USTRUCT(BlueprintType)
struct FResolutionFaultPresentationPayload
{
    GENERATED_BODY()

    FString Reason;
    int32 ExecutedActionCount;
    FName LastActionName;
};
```

Optional future diagnostic data such as `StateBeforeFault` is not required in the first A2D implementation.

Do not keep both typed payload and legacy root fault fields as two competing truths.

---

## 17. Terminal Recorder invariant

Existing invariant remains unchanged:

```text
Victory
Defeat
ResolutionFault
= terminal Record
= final Record of the unpublished Resolution
```

Any attempt to append after a terminal Record invalidates the entire active unpublished Presentation Resolution.

This includes:

```text
ordinary Record after terminal
duplicate same terminal
Victory + Defeat mixed terminal
Terminal + ResolutionFault mixed terminal
```

The invalid batch must not publish partial history.

### 17.1 ResolutionFault is not PresentationUnavailable

```text
ResolutionFault
= authoritative Gameplay/framework resolution failure

PresentationUnavailable
= Presentation-history / freezing / append / bootstrap degradation
```

Presentation failure must never create a fake ResolutionFault Record or change Gameplay state to ResolutionFaulted.

---

## 18. Terminal playback ownership

All formal terminal Records participate in the generic playback path:

```text
Victory
Defeat
ResolutionFault
```

They use the same PlaybackToken protocol as other visible Records.

```text
PlayPresentationRecord returns true
→ Blueprint accepted completion responsibility
→ must eventually NotifyPresentationFinished(Token)

returns false
→ immediate native fallback completion
```

Timeout remains fail-safe only.

---

## 19. Terminal ViewModel transition timing

The ViewModel must **not** enter Terminal merely because the Envelope's final state is already terminal while earlier Records or the terminal Record itself are still being visibly played.

Formal sequence:

```text
previous Records complete
↓
start Victory / Defeat / ResolutionFault Record playback
↓
WorkingPresentationSnapshot remains non-terminal
↓
ViewModel remains Resolving + InputLocked
↓
Blueprint callback / false fallback / timeout
↓
terminal reducer validates Record against FinalSnapshot
↓
WorkingPresentationSnapshot switches to terminal BattleState / Outcome
↓
ViewModel.ApplyPresentationSnapshot(WorkingSnapshot)
↓
ViewModel enters Terminal
↓
Envelope completes
↓
exact Envelope.FinalSnapshot reconciliation
```

This ensures terminal overlay/state does not appear before terminal animation has completed.

### 19.1 Skip

Skip remains a direct catch-up operation:

```text
Skip
→ invalidate active token generation
→ apply newest retained Envelope.FinalSnapshot
→ enter Terminal immediately if that snapshot is terminal
```

### 19.2 Stale / duplicate callbacks

Old or duplicate terminal callbacks must be ignored by the existing token/generation checks.

They must never:

```text
reapply terminal state
advance a newer Record
overwrite a newer battle
unlock/lock input incorrectly
```

---

## 20. Terminal reducer validation

Terminal reducer validates both Record semantic identity and the immutable FinalSnapshot.

### 20.1 Victory

Require:

```text
Record.Type == Victory
WinnerPresentationId    == WorkingSnapshot.Player.PresentationId
WinnerPresentationId    == FinalSnapshot.Player.PresentationId
DefeatedPresentationId  == WorkingSnapshot.Enemy.PresentationId
DefeatedPresentationId  == FinalSnapshot.Enemy.PresentationId
WorkingSnapshot.Enemy.bDead == true
FinalSnapshot.Enemy.bDead   == true
FinalSnapshot.BattleState == Victory
FinalSnapshot.Outcome     == Victory
```

Then set WorkingSnapshot terminal state from the validated terminal semantic.

### 20.2 Defeat

Require:

```text
Record.Type == Defeat
WinnerPresentationId    == WorkingSnapshot.Enemy.PresentationId
WinnerPresentationId    == FinalSnapshot.Enemy.PresentationId
DefeatedPresentationId  == WorkingSnapshot.Player.PresentationId
DefeatedPresentationId  == FinalSnapshot.Player.PresentationId
WorkingSnapshot.Player.bDead == true
FinalSnapshot.Player.bDead   == true
FinalSnapshot.BattleState == Defeat
FinalSnapshot.Outcome     == Defeat
```

### 20.3 ResolutionFault

Require:

```text
Record.Type == ResolutionFault
FinalSnapshot.BattleState == ResolutionFaulted
FinalSnapshot.Outcome     == ResolutionFaulted
```

The diagnostic fault payload does not become Gameplay authority; it is frozen history describing the already-authoritative framework failure.

No defeated participant is required for `ResolutionFault`. The WorkingSnapshot continues to reflect all valid earlier Records; the fault reducer changes only the terminal-owned state described below.

### 20.4 Terminal reducer ownership

After validation, a terminal reducer may change only terminal-owned fields:

```text
BattleState
Outcome
bCanEndTurn
```

It must not repair or synthesize missing HP, Block, Status, Card, pile or Energy history. Before Victory/Defeat is applied, the relevant WorkingSnapshot defeated/dead state and participant identities must already agree with the FinalSnapshot as a consequence of prior Records. If, for example, a Damage Record is missing and the WorkingSnapshot still shows a living enemy, terminal validation fails and the Controller collapses directly to `Envelope.FinalSnapshot` instead of presenting terminal semantics over an impossible intermediate state.

### 20.5 Terminal mismatch

Any mismatch means:

```text
bad/inconsistent Envelope history
↓
collapse to Envelope.FinalSnapshot
↓
no Gameplay fault request
```

FinalSnapshot remains the exact immutable reconciliation authority.

---

## 21. Producer matrix

Locked producer responsibilities:

| Producer | Committed Presentation fact |
|---|---|
| `UApplyStatusAction` | `StatusChanged(Applied/Increased derived from result)` |
| `UReduceStatusAction` | `StatusChanged(Reduced or TurnEndDecay)` |
| `URemoveStatusAction` | `StatusChanged(Removed)` |
| `ABattleManager::CheckBattleResult` victory path | `Victory` terminal payload |
| `ABattleManager::CheckBattleResult` defeat path | `Defeat` terminal payload |
| queue/framework fault handling | `ResolutionFault` typed payload |

Every committed Status producer must resolve Source/Target PresentationIds through BattleManager and freeze presentation values from the true effective runtime status.

---

## 22. Combined acceptance matrix

Do not implement one giant monolithic test. Use several focused top-level tests/scenarios that together prove the contract.

### 22.1 Status lifecycle

Scenario:

```text
Create Weak#10 0→2
Merge Weak#10 2→3
Reduce Weak#10 3→2
TurnEndDecay Weak#10 2→1
Remove Weak#10 1→0
Recreate Weak#15 0→2
```

Validate:

```text
RuntimeSequence identities remain exact
recreated instance uses a newer RuntimeSequence
old/stale exact-instance action cannot mutate new instance
DescriptionBefore / DescriptionAfter match each transition
Applied / Increased / Reduced / TurnEndDecay / Removed reasons are correct
bCreated / bRemoved represent actual structure
atlas/icon values are frozen
Working reducer keeps RuntimeSequence ascending order
```

### 22.2 Integrated card/status scenario

Example Uppercut-like flow:

```text
CardPlayed
↓
Damage
↓
StatusChanged Weak Applied
↓
StatusChanged Vulnerable Applied
↓
CardZoneChanged PlayArea→Discard
```

Repeated application should produce:

```text
StatusChanged Increased
```

not a second UI status entry with the same RuntimeSequence/identity.

### 22.3 Turn cycle

Exercise:

```text
EnergyChanged EndTurnClear
↓
Hand→Discard Records
↓
TurnEnded event/reactions
↓
StatusChanged TurnEndDecay
↓
Enemy Block clear / Damage
↓
EnergyChanged TurnStartRestore
↓
Draw / Shuffle / RetryDraw Records
```

Validate existing Gameplay order is preserved exactly.

### 22.4 Terminal scenarios

Cover separately:

```text
lethal player Damage → Victory
lethal enemy Damage  → Defeat
committed ordinary Records → ResolutionFault
```

Validate:

```text
terminal Record is always last
terminal payload identities match FinalSnapshot
WorkingSnapshot stays non-terminal until terminal Record completion
FinalSnapshot reconciliation ends exact
```

### 22.5 Controller safety

Across terminal paths cover the generic safety mechanisms:

```text
normal async completion
Blueprint false fallback
timeout
skip
stale token
duplicate callback
```

Do not require every terminal type to duplicate every low-level token test if equivalent generic Controller coverage already proves the mechanism; acceptance must still prove that all terminal Record types participate correctly in the same path.

---

## 23. Reducer consistency assertion

Add a general development/test assertion concept:

```text
Displayed baseline
+ sequential reducer over committed Records
≈ Envelope.FinalSnapshot
```

Comparison is limited to fields actually owned by committed reducers.

Recommended reducer-owned comparison set:

```text
Player/Enemy HP
Player/Enemy Block
Player/Enemy Status identity/order/Amount/Description
Energy
Hand concrete-card identity/order
Draw / Discard / Exhaust counts
represented terminal BattleState / Outcome
```

Do **not** require equality for transient or currently non-recorded derived fields such as:

```text
hover
selection
legal-target runtime bindings
PlaybackToken
derived card preview values not explicitly updated by Records
enemy-intent projections changed indirectly by a status
```

Exact Envelope.FinalSnapshot remains authoritative at Envelope completion.

---

## 24. Presentation failure semantics

A2D retains the A2A/A2B/A2C fail-soft split.

```text
Writer absent from start
→ legal no-history

Gameplay mutation no-op / invalid
→ no Record
→ no partial history problem

Gameplay committed + active Writer + invalid ID/frozen payload/append
→ invalidate current unpublished Presentation Resolution
→ publish no partial Envelope
→ Gameplay commit remains authoritative

Presentation reducer mismatch during playback
→ collapse to immutable FinalSnapshot
→ Gameplay unchanged
```

Only genuine Gameplay framework structural failures may become `ResolutionFaulted`.

---

## 25. Blueprint ownership boundary

Even after later Blueprint integration:

```text
Blueprint may animate Status / terminal visuals
Blueprint may inspect frozen Record payload
Blueprint may choose to accept or decline playback
Blueprint may call NotifyPresentationFinished(Token)
```

Blueprint must **not**:

```text
Add/Remove status in ViewModel as authoritative state
modify Amount itself
recompute dynamic descriptions from live Gameplay
switch Terminal before Controller reducer owns that transition
change HP/Block/Energy/Hand/pile counts
query UStatusInstance/UStatusData to reconstruct historical state
```

Authoritative presentation-state transition remains:

```text
immutable Record
→ Controller reducer
→ WorkingPresentationSnapshot
→ ViewModel
→ Blueprint renders values
```

---

## 26. Implementation slices

Implement in this order:

### A2D-1 — Gameplay Status mutation truth

```text
FStatusMutationResult
EStatusMutationOutcome
Apply/Reduce real before-after semantics
MAX_int32 no-op correctness
exact RemoveStatus(ExpectedInstance)
URemoveStatusAction
ReduceStatusAction full Battle/Source/Target/Reason context
```

### A2D-2 — Status committed Presentation

```text
StatusChanged Record Type appended
EStatusChangeReason
FStatusChangedPresentationPayload
Source/Target resolver wiring
RuntimeSequence conversion validation
EffectiveDefinition freezing
DescriptionBefore / DescriptionAfter capture
Apply/Reduce/Remove producers
```

### A2D-3 — Status historical reducer

```text
FBattleHUDStatusView.RuntimeSequence
FinalSnapshot explicit RuntimeSequence sort
WorkingSnapshot Status reducer
exact identity matching
AmountBefore validation
create/update/remove semantics
reducer mismatch collapse
```

### A2D-4 — Formal terminal Presentation

```text
FTerminalPresentationPayload
FResolutionFaultPresentationPayload
remove legacy duplicate fault root fields
Victory/Defeat/Fault visible playback support
terminal semantic validation
terminal reducer
ViewModel stays Resolving until terminal completion
false/timeout/skip/stale-token behavior
```

### A2D-5 — Combined Automation acceptance

```text
Status lifecycle
stale instance isolation
integrated card + damage + statuses
turn-cycle ordering
Victory / Defeat / ResolutionFault
terminal token/fallback/skip behavior
reducer-owned consistency assertion
affected regression gate update
```

Only after all C++ gates pass:

```text
unified A2B Damage/Block Blueprint integration
+
unified A2C Card/Energy/Zone Blueprint integration
+
unified A2D Status/Terminal Blueprint integration
↓
PIE end-to-end smoke
```

---

## 27. Locked design summary

UI-A2D is locked around:

```text
one StatusChanged Record
+ semantic Reason
+ exact RuntimeSequence identity
+ frozen Before/After dynamic description
+ true Gameplay Status MutationResult
+ exact-instance Reduce/Remove
+ deterministic RuntimeSequence ordering
+ Status-only WorkingSnapshot reducer

Victory / Defeat / ResolutionFault typed payloads
+ terminal Record remains final
+ formal terminal visible playback
+ WorkingSnapshot stays non-terminal during terminal animation
+ terminal callback/fallback/timeout completes transition
+ FinalSnapshot exact reconciliation
```

The central ownership rule remains unchanged:

```text
Gameplay commits truth
↓
immutable committed Record freezes history
↓
Controller owns historical intermediate display state
↓
ViewModel exposes that state
↓
Blueprint only renders/animates it
```

---

## 28. Current implementation status

```text
A2D design contract                  LOCKED
A2D-1 Status MutationResult          NOT STARTED
A2D-2 StatusChanged producer         NOT STARTED
A2D-3 Status reducer                 NOT STARTED
A2D-4 terminal formal presentation   NOT STARTED
A2D-5 combined Automation            NOT STARTED
UE5.8 build                          NOT RUN for A2D
A2D Automation                       NOT RUN
Affected regression                  NOT RUN for A2D
Blueprint integration                DEFERRED
PIE smoke                            DEFERRED
```
