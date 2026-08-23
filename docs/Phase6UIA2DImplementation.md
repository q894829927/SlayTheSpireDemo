# Phase 6UI-A2D — Status / Terminal Committed Presentation

Status: **C++ / AUTOMATION COMPLETE / VALIDATED / SEALED**.

UI-A2D extends the established UI-A2A transport/lifecycle, UI-A2B committed Damage/Block presentation, and UI-A2C card/energy/zone committed presentation. It does not redesign Resolution, Envelope, RecordWriter, Controller backlog, PlaybackToken, PresentationUnavailable, immutable FinalSnapshot ownership, or fail-soft no-partial-history behavior.

The A2D C++ committed-presentation contract is now validated through A2D1–A2D5. Owner-confirmed final evidence is:

```text
A2D5 focused      6/6 PASS
Phase6R aggregate 100/100 PASS
Shipping          PASS
```

Blueprint visual integration and PIE smoke are intentionally owned by the next stage, `UI-A2E — Unified Blueprint Playback & PIE Acceptance`.

---

## 1. A2D scope

A2D owns exactly these additions:

```text
StatusChanged + Reason
Status mutation CommitResult / exact-instance remove path
Status historical RuntimeSequence identity
Status frozen Before/After dynamic presentation values
Status WorkingPresentationSnapshot reducer
formal Victory / Defeat visual playback contract
formal ResolutionFault visual playback contract
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

Use one Record Type:

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

`StatusChanged` is appended to the existing `EBattlePresentationRecordType` enum without reordering existing entries merely for grouping/readability.

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

Every Gameplay → Presentation conversion validates:

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

The committed payload is self-sufficient for historical playback and carries:

```text
SourcePresentationId
TargetPresentationId
StatusId
RuntimeSequence
AmountBefore
AmountAfter
bCreated
bRemoved
Reason
DisplayName
DescriptionBefore
DescriptionAfter
icon/atlas metadata
```

Blueprint must not query `UStatusInstance`, `UStatusData`, `ACombatant`, BattleManager status queries, or mutable live status state during historical playback.

### 4.1 Dynamic description Before/After

Both descriptions are required because dynamic Status text can change when Amount changes.

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

The implementation must not temporarily mutate the Status amount backward to reconstruct historical text. The pre-mutation description is captured before the Container changes the instance.

Apply capture order:

```text
read-only lookup of PreExistingInstance by incoming StatusId
↓
if PreExistingInstance exists
    capture RuntimeSequence / EffectiveDefinition / DescriptionBefore
else
    DescriptionBefore = Empty
↓
Container Apply mutation decides create / merge / no-op / invalid
↓
validate committed identity against the captured instance when merging
↓
resolve DescriptionAfter from the effective committed instance
```

Reduce/remove capture order:

```text
validate and capture DescriptionBefore from exact ExpectedInstance
↓
Container exact-instance mutation
↓
validate Result identity against captured instance
↓
resolve DescriptionAfter from Result.EffectiveInstance, or Empty when removed
```

If Gameplay commits but returned identity contradicts the pre-captured identity while a writer is active, the current record batch is invalidated without rolling Gameplay back.

### 4.3 EffectiveDefinition rule

If an incoming request uses a different `UStatusData*` but the same StatusId as an existing runtime status, Gameplay merges into the **existing instance**. Frozen presentation metadata comes from the true `EffectiveDefinition` owning that runtime instance, not blindly from the new request definition.

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

A null Source carrying a fake/non-empty ID is invalid Presentation history. A valid Source carrying `NAME_None` is invalid Presentation history. Target must represent an active battle participant and resolve to a trustworthy non-empty ID.

Writer absent from the start remains legal no-history mode.

---

## 6. Gameplay Status MutationResult

Container returns Gameplay mutation truth without depending on Recorder/Presentation.

Implemented mutation truth preserves:

```text
Outcome
StatusId
RuntimeSequence
AmountBefore
AmountAfter
bCreated
bRemoved
EffectiveInstance
EffectiveDefinition
```

`EffectiveInstance` and `EffectiveDefinition` are synchronous Action/Battle-layer consumption references only. They are never stored in Presentation Records, Envelopes, Controller backlog or other asynchronous caches.

Committed result invariants:

```text
AmountBefore >= 0
AmountAfter  >= 0

bCreated == (AmountBefore == 0 AND AmountAfter > 0)
bRemoved == (AmountBefore > 0 AND AmountAfter == 0)
bCreated and bRemoved are never both true

otherwise Committed requires AmountBefore != AmountAfter
```

Any result violating these invariants is unusable Presentation history. If Gameplay already committed and a writer is active, invalidate the current record batch without rolling Gameplay back or requesting Gameplay ResolutionFault.

`StatusContainer` owns only Gameplay mutation semantics. Action/Battle layer owns Source, Target, Reason, PresentationId resolution, before/after text freezing, atlas freezing and Record append.

---

## 7. Mutation outcome semantics

### 7.1 Invalid

Examples:

```text
invalid Owner
invalid Definition
StatusId == None
Amount <= 0
CandidateRuntimeSequence == 0 for an Apply call reaching the Container
failed UObject creation
structurally invalid expected-instance arguments
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
Reduce exact instance no longer exists
Remove exact instance no longer exists
Apply tries to increase MAX_int32 but Amount remains MAX_int32
```

Result:

```text
Outcome = NoOp
no Record
not a Gameplay ResolutionFault
```

A stale exact-instance mutation never retargets a replacement instance with the same StatusId.

Every Apply invocation that reaches `StatusContainer` supplies a non-zero CandidateRuntimeSequence. Merge/no-op results report the existing instance's actual RuntimeSequence; unused candidates may create valid sequence gaps.

### 7.3 Committed

Committed requires a real authoritative change:

```text
AmountBefore != AmountAfter
OR
membership actually changed by create/remove
```

Saturated unchanged amount is `NoOp`, not `Committed`.

---

## 8. Reason ownership

### 8.1 Apply path

Apply reason is derived from MutationResult:

```text
bCreated == true
→ Applied

bCreated == false
AND Outcome == Committed
AND AmountAfter > AmountBefore
→ Increased
```

### 8.2 Reduce path

Reduce action carries only accepted semantic causes:

```text
ordinary reduction → Reduced
turn-end reaction   → TurnEndDecay
```

If reduction reaches zero, `bRemoved=true` while Reason remains the original reduction cause.

### 8.3 Explicit remove path

Only explicit remove semantics use:

```text
Reason = Removed
```

---

## 9. Exact-instance Remove entry point

Formal queued path:

```text
URemoveStatusAction
```

It targets an exact `UStatusInstance*`. Container exact-instance removal verifies the pointer/runtime identity against current membership before removal. A queued remove action must not store only StatusId because that could delete a newer same-name runtime instance.

---

## 10. ReduceStatusAction formal context

ReduceStatusAction carries the Battle, Source, Target, exact ExpectedInstance, AmountToRemove and accepted reason. It continues to receive the optional presentation writer through the established action mechanism and never world-searches the Recorder.

Turn-end decay uses:

```text
Source = Owner
Target = Owner
Reason = TurnEndDecay
```

---

## 11. Status/Event/Reaction ordering

General mutation ordering:

```text
Status Gameplay Commit
↓
StatusChanged Record
↓
if the Gameplay mechanism defines a BattleEvent
    Dispatch Event
↓
trigger reactions
↓
reaction Actions execute
↓
subsequent Records
```

A2D creates no new Gameplay event solely for Presentation.

### 11.1 ApplyStatus

```text
Apply commit
↓
StatusChanged
↓
Finish
```

### 11.2 Turn-end decay

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

### 11.3 Multiple reacting statuses

Record ordering follows existing Gameplay reaction order:

```text
Trigger Priority
→ RuntimeSequence
→ LocalTriggerIndex
```

Presentation never resorts committed Records for visual convenience.

---

## 12. `FBattleHUDStatusView` identity extension

Frozen status views carry:

```text
RuntimeSequence
```

The status view remains presentation-only immutable value data and contains no live `UStatusInstance*` / `UStatusData*` reference for historical playback.

---

## 13. Frozen Status ordering

Final frozen Status arrays are explicitly sorted by:

```text
RuntimeSequence ascending
```

Do not rely on StatusContainer insertion order. Do not sort by StatusId, DisplayName, localized Description, Amount or Widget creation order.

WorkingSnapshot reducer preserves the same sequence ordering.

---

## 14. Controller Status reducer

Status reducer finds a concrete entry using full identity:

```text
TargetPresentationId
+ StatusId
+ RuntimeSequence
```

It uses only immutable Record fields and WorkingPresentationSnapshot, never mutable Gameplay.

Create validates no existing exact identity and inserts in RuntimeSequence order. Update/reduce validates `Current.Amount == AmountBefore`, then updates Amount/Description. Remove validates exact identity/AmountBefore and removes only that runtime row.

Any historical mismatch causes incremental playback collapse to immutable Envelope.FinalSnapshot. Gameplay remains unchanged and no Gameplay ResolutionFault is manufactured.

---

## 15. Status reducer projection boundary

Status mutation can indirectly affect other derived values such as card descriptions or enemy intent previews. The A2D Status reducer does **not** recalculate those fields from live Gameplay.

It owns only:

```text
Combatant.Statuses[] identity/order
Amount
Description
DisplayName/icon metadata
```

Other derived fields may remain at their prior historical value until FinalSnapshot reconciliation.

---

## 16. Terminal payloads

### 16.1 Victory / Defeat

Victory and Defeat use a typed terminal payload containing WinnerPresentationId and DefeatedPresentationId. UMG chooses localized title/text from Record Type; Gameplay does not generate display strings such as “Victory” or “Defeat”.

### 16.2 ResolutionFault

ResolutionFault uses one typed payload containing Reason, ExecutedActionCount and LastActionName. Human-readable diagnostic wording is not treated as a stable ABI; ownership equality with authoritative Queue diagnostics is the tested contract.

---

## 17. Terminal Recorder invariant

```text
Victory
Defeat
ResolutionFault
= terminal Record
= final Record of the unpublished Resolution
```

Any attempt to append after a terminal Record invalidates the entire active unpublished Presentation Resolution. Duplicate/mixed terminal combinations are invalid.

### 17.1 ResolutionFault is not PresentationUnavailable

```text
ResolutionFault
= authoritative Gameplay/framework resolution failure

PresentationUnavailable
= Presentation-history / freezing / append / bootstrap degradation
```

Presentation failure never creates a fake ResolutionFault Record or changes Gameplay state to ResolutionFaulted.

---

## 18. Terminal playback ownership

All formal terminal Records participate in the generic PlaybackToken path:

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

The ViewModel does **not** enter Terminal merely because the Envelope FinalSnapshot is terminal while earlier Records or the terminal Record itself are still being played.

Formal sequence:

```text
previous Records complete
↓
start Victory / Defeat / ResolutionFault playback
↓
WorkingPresentationSnapshot remains non-terminal
↓
ViewModel remains Resolving + InputLocked
↓
Blueprint callback / false fallback / timeout
↓
terminal reducer validates Record against FinalSnapshot
↓
WorkingPresentationSnapshot switches terminal BattleState / Outcome
↓
ViewModel applies WorkingSnapshot
↓
Envelope completes
↓
exact FinalSnapshot reconciliation
```

Skip directly catches up to the newest retained FinalSnapshot. Old/duplicate terminal callbacks are ignored by token/generation checks.

---

## 20. Terminal reducer validation

Victory requires the player winner / enemy defeated identities and enemy-dead state to agree across WorkingSnapshot and FinalSnapshot. Defeat requires the inverse with player dead. ResolutionFault requires a ResolutionFaulted FinalSnapshot.

After validation, a terminal reducer changes only terminal-owned fields:

```text
BattleState
Outcome
bCanEndTurn
```

It does not repair missing HP, Block, Status, Card, pile or Energy history. Any mismatch collapses directly to FinalSnapshot without requesting a Gameplay fault.

---

## 21. Producer matrix

| Producer | Committed Presentation fact |
|---|---|
| `UApplyStatusAction` | `StatusChanged(Applied/Increased derived from result)` |
| `UReduceStatusAction` | `StatusChanged(Reduced or TurnEndDecay)` |
| `URemoveStatusAction` | `StatusChanged(Removed)` |
| `ABattleManager::CheckBattleResult` victory path | `Victory` terminal payload |
| `ABattleManager::CheckBattleResult` defeat path | `Defeat` terminal payload |
| queue/framework fault handling | `ResolutionFault` typed payload |

Every committed Status producer resolves Source/Target PresentationIds through BattleManager and freezes values from the true effective runtime status.

---

## 22. Combined acceptance matrix

A2D5 uses focused top-level tests rather than one monolithic test.

Validated scenarios:

```text
StatusLifecycle
CardStatusIntegration
TurnCycleOrdering
Terminal.Victory
Terminal.Defeat
Terminal.ResolutionFault
```

They prove exact status identity, stale-instance isolation, integrated card/status order, full turn-cycle order, terminal last-record rules, genuine framework fault ownership, duplicate terminal-token NoOp and PresentationUnavailable separation.

---

## 23. Reducer consistency assertion

Development/test assertion:

```text
Displayed baseline
+ sequential reducer over one Envelope's committed Records
≈ that Envelope.FinalSnapshot
```

Comparison is limited to reducer-owned fields. Selection/hover/legal-target runtime bindings/token and non-recorded derived preview fields are excluded. FinalSnapshot remains authoritative at Envelope completion.

---

## 24. Presentation failure semantics

A2D retains the fail-soft split:

```text
Writer absent from start
→ legal no-history

Gameplay mutation no-op / invalid
→ no Record

Gameplay committed + active Writer + invalid frozen payload/append
→ invalidate current unpublished Presentation Resolution
→ publish no partial Envelope
→ Gameplay commit remains authoritative

Presentation reducer mismatch during playback
→ collapse to immutable FinalSnapshot
→ Gameplay unchanged
```

Only genuine Gameplay framework structural failures become ResolutionFaulted.

The A2D5 review additionally fixed the public read-edge identity so a Presentation-availability transition is observable even when Gameplay BattleId/StateRevision do not change.

---

## 25. Blueprint ownership boundary

During A2E integration:

```text
Blueprint may animate Status / terminal visuals
Blueprint may inspect frozen Record payload
Blueprint may choose to accept or decline playback
Blueprint may call NotifyPresentationFinished(Token)
```

Blueprint must **not** authoritatively mutate HP/Block/Energy/Hand/piles/statuses, recompute dynamic historical values from live Gameplay, switch Terminal before Controller reducer ownership, or query runtime Status/Card objects to reconstruct the past.

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

Completed in this order:

```text
A2D-1 Gameplay Status mutation truth       COMPLETE / VALIDATED
A2D-2 Status committed Presentation        COMPLETE / VALIDATED
A2D-3 Status historical reducer            COMPLETE / VALIDATED
A2D-4 Formal terminal Presentation         COMPLETE / VALIDATED
A2D-5 Combined Automation acceptance       COMPLETE / VALIDATED / SEALED
```

Final evidence:

```text
A2D5 focused      6/6 PASS
Phase6R aggregate 100/100 PASS
Shipping          PASS
```

Only after these C++ gates passed does the roadmap proceed to:

```text
unified A2B Damage/Block Blueprint integration
+
unified A2C Card/Energy/Zone/Shuffle Blueprint integration
+
unified A2D Status/Terminal Blueprint integration
↓
UI-A2E PIE end-to-end acceptance
```

---

## 27. Locked design summary

UI-A2D is sealed around:

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
+ formal terminal visible playback contract
+ WorkingSnapshot stays non-terminal during terminal animation
+ terminal callback/fallback/timeout completes transition
+ FinalSnapshot exact reconciliation
```

Central ownership rule:

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
A2D design contract                  SEALED
A2D-1 Status MutationResult          COMPLETE / VALIDATED
A2D-2 StatusChanged producer         COMPLETE / VALIDATED
A2D-3 Status reducer                 COMPLETE / VALIDATED
A2D-4 terminal formal presentation   COMPLETE / VALIDATED
A2D-5 combined Automation            COMPLETE / VALIDATED
A2D5 focused                         PASS 6/6
Phase6R aggregate                    PASS 100/100
Shipping exclusion                   PASS
Blueprint integration                NEXT IN UI-A2E
PIE smoke                            NEXT IN UI-A2E
```

A2D C++ implementation is closed. Do not extend A2D with Preview functionality. The next stage is `docs/Phase6UIA2EImplementation.md`.
