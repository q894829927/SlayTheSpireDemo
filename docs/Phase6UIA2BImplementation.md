# Phase 6UI-A2B — Damage + Block Committed Presentation

Status: **DESIGN LOCKED / IMPLEMENTATION NOT STARTED**.

UI-A2B is the first real business-record slice built on the completed UI-A2A transport/lifecycle architecture. It must not redesign Resolution, Envelope, frozen snapshot, deferred delivery, Controller backlog, PlaybackToken, latest-only input binding, or PresentationUnavailable ownership.

Current design basis: `main` includes fixed multi-hit `UDamageCardEffect::HitCount`; therefore multi-hit behavior is a current A2B requirement, not a future extension.

## 1. Scope

UI-A2B adds only:

```text
FDamageCommitResult
FBlockCommitResult
Damage Presentation Record
BlockChanged Presentation Record
TurnStartClear Block history
Victory / Defeat terminal record ordering
Damage / Block playback through the existing PlayPresentationRecord contract
focused UI-A2B Automation
```

UI-A2B does not add CardPlayed/Energy/card-zone/shuffle/status presentation, formal terminal/fault visual polish, VFX/SFX, animation batching, or a second presentation queue.

## 2. Gameplay mutation results

Gameplay Runtime owns mutation truth and remains presentation-agnostic.

### 2.1 Damage

`ACombatant::TakeCombatDamage(int32 Amount)` returns an `FDamageCommitResult` containing:

```text
bCommitted
IncomingDamage
HPBefore
HPAfter
BlockBefore
BlockAfter
BlockedDamage
HPDamage
```

Semantics:

```text
Amount <= 0 or Target already dead
→ bCommitted = false
→ no mutation

Amount > 0 and Target alive
→ bCommitted = true
→ IncomingDamage is the fully modifier-resolved amount passed to the commit
```

Exact invariants for every committed result:

```text
BlockedDamage = BlockBefore - BlockAfter
HPDamage      = HPBefore - HPAfter

BlockedDamage >= 0
HPDamage >= 0

BlockedDamage + HPDamage <= IncomingDamage
```

The final inequality may be strict for overkill. `HPDamage` means actual HP delta, not unblocked attempted damage.

Examples:

```text
HP 100 Block 5 Incoming 12
→ HP 93 Block 0 BlockedDamage 5 HPDamage 7

HP 100 Block 20 Incoming 12
→ HP 100 Block 8 BlockedDamage 12 HPDamage 0

HP 3 Block 0 Incoming 10
→ HP 0 Block 0 BlockedDamage 0 HPDamage 3
```

Fully blocked damage is a committed Damage fact and must produce a Damage Record when history recording is active.

### 2.2 Block

`GainBlock()` and `ClearBlock()` return `FBlockCommitResult`:

```text
bCommitted
BlockBefore
BlockAfter
BlockDelta
```

Invariant:

```text
BlockDelta = BlockAfter - BlockBefore
```

Rules:

```text
GainBlock Amount <= 0 or dead Target
→ bCommitted = false

valid positive GainBlock
→ bCommitted = true
→ BlockDelta > 0

ClearBlock when Block == 0
→ bCommitted = false

ClearBlock when Block > 0
→ bCommitted = true
→ BlockAfter = 0
→ BlockDelta < 0
```

`ClearBlock()` remains a direct BattleManager lifecycle mutation. It must not be migrated into a BattleAction merely for Presentation.

## 3. Presentation record types and typed payloads

Append new enum values after the existing A2A values so existing numeric values remain stable:

```text
Damage
BlockChanged
Victory
Defeat
```

`Damage` uses a typed payload containing:

```text
SourcePresentationId
TargetPresentationId
DamageKind
IncomingDamage
HPBefore
HPAfter
BlockBefore
BlockAfter
BlockedDamage
HPDamage
```

Do not record Damage `BaseAmount`; A2B explains committed facts, not modifier-debug history.

`BlockChanged` uses:

```text
SourcePresentationId
TargetPresentationId
Reason = Gain | TurnStartClear
BlockBefore
BlockAfter
BlockDelta
```

Damage absorption is represented only by the Damage Record. Damage consuming Block must never emit a second `BlockChanged` for that same commit.

`Victory` / `Defeat` require no extra business payload in A2B; the type plus the Envelope FinalSnapshot is sufficient. Formal visible terminal treatment remains UI-A2D.

## 4. Terminal state and terminal-record invariants

Terminal Gameplay state is irreversible.

`ABattleManager::CheckBattleResult()` must immediately return when state is already:

```text
Victory
Defeat
ResolutionFaulted
```

Only these transitions are legal:

```text
NonTerminal → Victory
NonTerminal → Defeat
```

A later call must never change Victory into Defeat, Defeat into Victory, or either terminal state into another terminal state.

Only a real nonterminal-to-terminal transition may append a `Victory` or `Defeat` Record.

Recorder terminal records are exactly:

```text
ResolutionFault
Victory
Defeat
```

After any terminal record is appended, the current unpublished Resolution is closed to further records. Any later Append, repeated terminal append, or attempt to place both Victory and Defeat in the same Resolution invalidates the whole unpublished Presentation batch. Gameplay remains unchanged.

Lethal ordering must remain:

```text
Damage commit
→ Damage Record
→ DamageAction Finish
→ Queue/macro stable result check
→ Victory or Defeat Gameplay transition
→ Victory or Defeat Record
→ FinalSnapshot
→ Seal
```

Never publish terminal-before-Damage history.

## 5. PresentationId producer matrix

Presentation identity must cover every current runtime Action producer, not only card construction.

Current producers to wire explicitly:

| Producer | Damage | Block | ID source |
|---|---:|---:|---|
| `UDamageCardEffect::BuildActions` | yes, including every `HitCount` Action | no | resolved values carried by `FCardPlayContext` |
| `UGainBlockCardEffect::BuildActions` | no | yes | resolved values carried by `FCardPlayContext` |
| `ABattleManager::QueueDamageAction` | yes | no | BattleManager resolver |
| `ABattleManager::QueueGainBlockAction` | no | yes | BattleManager resolver |
| `ABattleManager::StartEnemyTurn` direct `UDamageAction` | yes | no | BattleManager resolver |
| Debug/System runtime Damage/Block helpers with an active writer | as applicable | as applicable | BattleManager resolver |
| isolated unit-test Actions with no writer | allowed | allowed | no-history mode; IDs not required |

Every runtime Action that may commit while holding an active writer must receive `SetPresentationParticipantIds(...)` from its producer.

Identity rules:

```text
Target committed mutation
→ TargetPresentationId must resolve

Source is a valid battle Combatant
→ SourcePresentationId must resolve

Source == nullptr
→ valid System/no-source operation
→ SourcePresentationId = None
→ this is not Presentation corruption
```

`TurnStartClear` is a lifecycle/System mutation:

```text
SourcePresentationId = None
TargetPresentationId = combatant whose Block was cleared
```

Do not represent it as self-caused Block removal.

## 6. Payload validation happens only after a real commit

Presentation identity may be resolved/cached before execution, but missing/invalid Presentation payload context must not invalidate a Resolution before Gameplay has actually committed a fact that requires a Record.

Required Action ordering:

```text
Action Execute
↓
Gameplay validation
↓
Modifier Resolve
↓
Gameplay commit / CommitResult
↓
if no committed mutation
    no Record
    no payload validation
    no Presentation invalidation
↓
if committed mutation and Writer available
    validate required Presentation IDs/payload
    Append or invalidate current Presentation Resolution
↓
Finish
```

An Action that exits because its Target is already dead is a legitimate no-commit Action. It must not invalidate earlier valid records in the same Resolution merely because it produces no presentation fact.

## 7. Writer absence versus damaged history

Do not expose a vague `Invalidate()` API.

Use a narrow operation such as:

```text
FPresentationRecordWriter::InvalidateCurrentResolution()
```

It may succeed only when all are true:

```text
Recorder exists
Writer BattleId matches current active builder
Writer ResolutionId matches current active builder
active builder exists
active builder is still valid
```

Semantics are deliberately different:

```text
Writer unavailable from the start
→ legal no-history mode
→ do not invalidate
→ do not mark Presentation unavailable

Writer was current/available
+ Gameplay committed
+ a trustworthy required Record cannot be built/appended
→ invalidate the current unpublished Presentation Resolution
→ discard its complete buffered history
→ Gameplay continues
→ stable boundary freezes latest exact baseline
→ Presentation enters its existing unavailable/fail-safe path
```

This preserves the A2A no-partial-history invariant without turning recording-disabled operation into an error.

## 8. Multi-hit is a current contract

`UDamageCardEffect::HitCount` builds independent `UDamageAction`s. Each hit resolves the current modifier pipeline independently and may independently commit or no-op.

Required surviving-target case:

```text
Twin Strike 5 × 2
Enemy HP 20

Damage #1: HP 20 → 15
Damage #2: HP 15 → 10

→ two Damage Records
→ distinct increasing PresentationSequence values
→ second HPBefore equals first HPAfter
```

Required first-hit-lethal case:

```text
Enemy HP 4
Damage 5 × 2

Hit #1 commits: HP 4 → 0
Hit #2 sees dead target: no commit, no Record, no Presentation invalidation
Queue result check transitions to Victory

Records:
Damage
Victory
```

Forbidden:

```text
Damage
Damage(0)
Victory
```

## 9. Damage and Block Action conversion rules

### DamageAction

```text
validate Gameplay prerequisites
→ resolve FDamageSpec
→ if resolved amount cannot commit, Finish with no Record
→ TakeCombatDamage(resolved amount)
→ if result not committed, Finish with no Record
→ if Writer unavailable, legal no-history path
→ if Writer available, validate participant IDs after commit
→ build Damage payload only from Action semantic context + FDamageCommitResult
→ append; append/payload failure invalidates current Presentation history only
→ Finish
```

Do not reconstruct Before values by reading the mutated Combatant after commit.

### GainBlockAction

```text
validate Gameplay prerequisites
→ resolve FBlockSpec
→ GainBlock(resolved amount)
→ if not committed, no Record
→ if Writer unavailable, legal no-history path
→ if Writer available, validate participant IDs after commit
→ append BlockChanged(Reason=Gain) from FBlockCommitResult
→ Finish
```

### Turn-start ClearBlock

BattleStart normalization:

```text
Player ClearBlock
→ ignore result for Presentation
→ no BlockChanged Record
```

Normal turn lifecycle:

```text
StartEnemyTurn ClearBlock
→ if committed: BlockChanged(None → Enemy, TurnStartClear)
→ then Enemy Action batch

StartPlayerTurn ClearBlock
→ if committed: BlockChanged(None → Player, TurnStartClear)
→ then turn-start draw work
```

If Block is already zero, emit nothing.

## 10. Blueprint playback responsibility

A2B is the first slice where Blueprint may formally take ownership of a business Record playback completion.

Existing API contract is locked as:

```text
PlayPresentationRecord(Record, Token) returns true
→ Blueprint accepted playback ownership
→ Blueprint must eventually call NotifyPresentationFinished(Token) exactly once for that accepted playback

returns false
→ Controller performs immediate fallback/catch-up for that Record
```

If Blueprint cannot start a trustworthy animation because the TargetPresentationId cannot be mapped, the combatant widget is missing, the record type is not implemented, or animation startup fails, it must return `false` rather than return `true` and wait for timeout.

Timeout remains a fail-safe, not the normal completion mechanism. Duplicate/stale/old-battle/pre-skip completion tokens continue to be ignored by Controller.

## 11. Temporary A2B visual boundary

A2B deliberately has no A2C CardPlayed/Energy/card-zone history.

During Damage/Block playback, the currently displayed historical snapshot may temporarily still show values such as:

```text
played Strike still visible in Hand
old Energy value still visible
```

The Envelope FinalSnapshot corrects all display state when that Resolution finishes.

This is an accepted temporary A2B limitation. A2B must not hide it by mutating Hand/Energy directly in Blueprint or by adding ViewModel side-channel state changes. UI-A2C owns the formal `CardPlayed`, Energy commit, and card-zone records that will make those transitions historical and animated.

## 12. Focused Automation gate

Keep exactly **8 top-level UI-A2B tests**. Add the required assertions inside those tests rather than increasing discovery count.

```text
SlayTheSpireDemo.Phase6UIA2B.Commit.DamageResult
SlayTheSpireDemo.Phase6UIA2B.Commit.BlockResult
SlayTheSpireDemo.Phase6UIA2B.Record.Damage
SlayTheSpireDemo.Phase6UIA2B.Record.BlockChanged
SlayTheSpireDemo.Phase6UIA2B.Record.TurnStartClear
SlayTheSpireDemo.Phase6UIA2B.Record.LethalOrdering
SlayTheSpireDemo.Phase6UIA2B.Playback.DamageBlockSequence
SlayTheSpireDemo.Phase6UIA2B.Failure.PresentationDoesNotAffectGameplay
```

### Commit.DamageResult

Must cover normal, partial Block, full Block, overkill lethal, and no-commit inputs. For every committed result assert:

```text
BlockedDamage == BlockBefore - BlockAfter
HPDamage == HPBefore - HPAfter
BlockedDamage >= 0
HPDamage >= 0
BlockedDamage + HPDamage <= IncomingDamage
```

### Commit.BlockResult

Must cover Gain, Clear, zero/no-op, and dead-target Gain; assert exact signed `BlockDelta`.

### Record.Damage

Must cover:

```text
modifier-resolved IncomingDamage rather than BaseAmount
resolved Source/Target PresentationIds
fully blocked Damage still records
Damage absorption emits no duplicate BlockChanged
Twin Strike 5×2 with living target → two continuous records
first hit lethal → second hit no record → Damage then Victory
current runtime producer paths provide required participant IDs
nullable Source/System damage remains valid with SourcePresentationId=None
```

### Record.BlockChanged

Must cover Gain payload, reason, resolved amount/delta, FinalSnapshot consistency, card/manager producers, and nullable Source where allowed.

### Record.TurnStartClear

Must prove BattleStart normalization emits no clear Record; Enemy and Player normal turn-start clears emit only on a real mutation, use `Source=None`, use resolved Target ID, and preserve actual macro ordering.

### Record.LethalOrdering

Must cover:

```text
Damage sequence < Victory/Defeat sequence
FinalSnapshot is terminal and HP is zero
terminal state cannot switch to the other terminal state
terminal transition Record is emitted once only
Recorder rejects/invalidate append after ResolutionFault/Victory/Defeat
Recorder rejects duplicate terminal and Victory+Defeat in one Resolution
```

### Playback.DamageBlockSequence

Must verify PresentationSequence order, existing PlaybackToken ownership, Blueprint-accepted completion, immediate fallback when playback returns false, skip/catch-up, and stale completion isolation.

### Failure.PresentationDoesNotAffectGameplay

Must explicitly distinguish:

```text
Writer absent from start
→ valid no-history mode
→ Gameplay commits normally
→ no Presentation failure

Writer current/valid but required committed payload cannot be trusted or append fails
→ current Presentation Resolution invalidates
→ no partial Envelope
→ Gameplay result/Action Finish/Queue result unchanged
→ no Gameplay ResolutionFault
→ latest frozen baseline still reflects committed Gameplay
```

## 13. CI expectations

Focused aggregate after A2B implementation:

```text
Phase5        13
Phase6A       23
Phase6B       12
Phase6C        5
Phase6UIA2A    8
Phase6UIA2B    8
----------------
Total         69
```

The broader UI owner gate must also rerun UI-A0/UI-A1/UI-A3 because A2B changes Combatant, Actions, BattleManager and Presentation types/controller behavior.

Do not hard-code the historical UI-A3 count from an older validation record: current source added a dedicated fixed multi-hit UI-A3 Automation test together with `HitCount`, so CI expected counts must be re-counted from current source before locking the broader aggregate total.

## 14. Completion definition

UI-A2B is complete only when:

```text
CommitResult APIs implemented
Damage / Block payloads and records implemented
all current runtime producers wire participant IDs
terminal Gameplay and Recorder invariants implemented
multi-hit behavior recorded correctly
TurnStartClear history implemented
Damage/Block Controller playback enabled through existing generic Widget API
8/8 focused UI-A2B Automation passed
A2A and affected prior regressions revalidated on the same source revision
no Gameplay result depends on Presentation availability/speed/callbacks
```

No A2C Hand/Energy/card-zone workaround may be introduced in A2B.
