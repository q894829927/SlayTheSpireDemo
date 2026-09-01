# Phase 6UI-A3 — Deterministic Immediate Preview

Date: **2026-09-02**

Status:

```text
UI-A3: IN PROGRESS / AUTHORIZED
A3-1 Dynamic Text: COMPLETE / VALIDATED / SEALED
A3-2 Target-Specific Current-State Preview: NEXT IMPLEMENTATION SLICE
A3-3 Energy + Target-Aware Legality: NOT STARTED
A3-4 ViewModel Transient Preview Lifecycle: NOT STARTED
A3-5 Minimal Native UMG + A2/A3 Combined PIE: NOT STARTED
```

This is the dedicated implementation and acceptance document for the remaining Phase 6UI-A3 work. It consolidates the A3 roadmap previously recorded in `docs/Phase6UIA2EImplementation.md`, the sealed A3-1 source slice in `docs/Phase6UIA3DynamicTextImplementation.md`, and the durable UI ownership rules in `Source/SlayTheSpireDemo/UI/AGENTS.md`.

For active A3 implementation, this document is the phase-specific authority. The A2E document remains sealed A2 history and design background; the A3-1 document remains the validation/source record for Dynamic Text.

---

## 1. Goal

Phase 6UI-A3 adds a deterministic **pre-commit** player-facing preview without creating a second gameplay simulator.

The locked distinction is:

```text
A3
= before submission
= read-only current-state values for supported Operations

A2
= after submission
= playback of immutable facts that actually committed
```

The first target-specific preview answers only:

> For this current `BattleId + StateRevision`, this Card, and this concrete current Target, what do the supported Operations resolve to if evaluated through the current read-only Gameplay pipelines now?

It does **not** answer:

> What is the guaranteed final result of the whole card Resolution?

The formal name is therefore:

```text
Target-Specific Current-State Preview
```

Do not call it an exact final-result preview, outcome simulation, predicted final damage, or future-state simulation.

---

## 2. Locked implementation order

```text
A3-1 Dynamic Text                         COMPLETE / SEALED
↓
A3-2 Target-Specific Current-State Preview
↓
A3-3 Energy + Target-Aware Legality
↓
A3-4 ViewModel Transient Preview Lifecycle
↓
A3-5 Minimal Native UMG + A2/A3 Combined PIE
↓
Phase 6UI-A COMPLETE
↓
Phase 7 Relics
```

Do not start Phase 7 before A3 is completed unless the user explicitly changes the phase order.

Each A3 slice is closed independently. Finish its smallest required Build/Automation/manual Gate, record evidence, commit, and stop before beginning the next slice unless the user explicitly asks to continue.

---

## 3. Non-negotiable architecture boundaries

### 3.1 Gameplay owns Preview construction

Preview construction belongs to the Gameplay/read Query boundary.

Required ownership:

```text
Card + current Target
↓
BattleManager read-only Query
↓
CardEffect read-only preview contribution
↓
DamageSpec / BlockSpec
↓
existing Modifier Pipeline
↓
Immediate Preview DTO
↓
ViewModel transient state
↓
Native HUD display
```

ViewModel and UMG may own:

```text
selection
preview-target nomination
hover/focus lifecycle
clearing
formatting/display
```

ViewModel and UMG must **not**:

```text
iterate CardEffects to calculate rules
implement Damage formulas
implement Block formulas
apply Strength / Weak / Vulnerable / Dexterity / Frailty rules
calculate Energy legality independently
predict Trigger or Relic reactions
mutate Gameplay state
```

### 3.2 Preview is read-only

Preview evaluation must never:

```text
Commit Gameplay state
construct/enqueue BattleAction objects as part of evaluation
pump BattleActionQueue
emit BattleEvents
execute Triggers
consume battle RNG
move cards between Deck zones
spend Energy
change HP / Block / Status state
change BattleState
create Presentation Records
```

The existing typed Damage and Block Modifier Pipelines may be reused because they resolve specs read-only.

### 3.3 A2 remains sealed

A3 must not change the meaning, ordering, token ownership, reducers or FinalSnapshot semantics of committed A2 Presentation.

The handoff remains:

```text
A3 preview visible
↓
player submits authoritative Request
↓
Preview clears immediately
↓
Gameplay resolves normally
↓
A2 committed Presentation takes ownership
↓
Controller catches up
↓
new live binding/revision becomes available
```

Do not render A3 prediction through an A2 Record and do not use A2 historical payloads to build a pre-commit preview.

### 3.4 Native-only UI

All new A3 HUD work targets only:

```text
UBattleHUDWidget / WBP_BattleHUD_Native
UBattleCardWidget / WBP_BattleCard_Native
UBattleStatusWidget / WBP_BattleStatus_Native
```

The retained Legacy assets under `/Game/SlayTheSpireDemo/UI/Out/Legacy/` remain deprecated and must not receive A3 behavior, runtime references, fallback logic or parity backports.

---

## 4. A3-1 — sealed Dynamic Text predecessor

A3-1 is already complete and is not reopened by A3-2.

Its current semantic boundary is:

```text
Card-face Dynamic Text
= current source-side/self presentation value

A3-2 Target-Specific Current-State Preview
= supported Operation value for one concrete current target
  at one BattleId/StateRevision
```

For enemy-target Damage, the card face deliberately omits one concrete Enemy target. Source-side modifiers may be reflected; target-specific modifiers are reserved for A3-2.

For Self-target Block, the card-face value may resolve Player as both Source and Target.

The A3-1 implementation already established reusable read-only primitives:

```text
FCardEffectPreviewContext
FPreviewTextArgumentBuilder
UCardEffect::BuildPreviewArguments(...)
DamageSpec / BlockSpec + existing Modifier Pipelines
```

A3-2 may reuse these semantics, but must not turn formatted card text into the preview authority. Numeric preview Operations are first-class DTO data, not values parsed back out of `FText`.

Validation evidence and authored DataAsset details remain in `docs/Phase6UIA3DynamicTextImplementation.md`.

---

## 5. Final first-version Preview model

The first implementation uses a flat operation array. Do not assume one Damage effect and one Block effect per card.

Conceptual final model:

```text
FImmediateCardPreview
├── BattleId
├── StateRevision
├── CardRuntimeId
├── SourcePresentationId
├── TargetPresentationId
├── Validation / FailureReason
├── EnergyBefore
├── EffectiveCost
├── optional EnergyAfter
└── Operations[]

FImmediatePreviewOperation
├── EffectIndex
├── SemanticArgumentName
├── Type = Damage / Block
├── ResolvedAmount
└── HitCount
```

The exact USTRUCT/enum exposure may be adjusted only as required by the current Native UI/C++ boundary, but the semantic fields above are locked.

### 5.1 Operation identity

`EffectIndex` is the stable per-card-definition position of the contributing effect for this query. It is not Gameplay runtime identity and must not be used to mutate an effect.

`SemanticArgumentName` reuses the authoring meaning already established by A3-1, for example:

```text
Damage
Block
```

Do not use array position alone as a semantic label.

### 5.2 Damage meaning

For a Damage operation:

```text
ResolvedAmount
= resolved incoming damage per hit
= after current Damage Modifier Pipeline
= before Block absorption
```

It is **not** guaranteed HP loss.

For fixed multi-hit Damage:

```text
Base card effect: 7 x 2
Current pipeline: each hit resolves to 9

Preview Operation:
ResolvedAmount = 9
HitCount = 2

UI may display:
9 x 2
```

Do not collapse this into guaranteed `18 HP` loss. Each committed hit is an independent DamageAction and later hits may observe state changes caused by earlier commits/reactions.

### 5.3 Block meaning

The first version supports current Self Block only.

Block preview resolves through the existing Block Modifier Pipeline using the current Player as the required self target where the card/effect semantics require Self targeting.

`ResolvedAmount` is the current Block amount that this supported Block operation resolves to now. It is not a promise about later trigger/relic changes.

### 5.4 Unsupported effects

The first Preview surface intentionally does not model every CardEffect.

Effects outside the supported first-version operation set do not create fabricated preview values. They are omitted from `Operations[]` while the rest of the card may still produce supported operations.

For example, an Uppercut-like card may preview its current Damage operation while ApplyStatus effects remain outside first-version target-specific outcome preview.

The UI must not present `Operations[]` as a complete simulation of all future card consequences.

---

## 6. A3-2 — Target-Specific Current-State Preview

### 6.1 Scope

A3-2 establishes the Gameplay-owned target-specific numeric operation query. It does not yet add hover/focus lifecycle or final UMG presentation.

The public query is conceptually:

```cpp
bool ABattleManager::TryBuildImmediateCardPreview(
    const UCardInstance* Card,
    const ACombatant* Target,
    FImmediateCardPreview& OutPreview
) const;
```

The `bool` means that a coherent Preview DTO could be constructed. Normal gameplay rejection belongs in the Preview validation fields; a rejected card is not automatically a transport/build failure.

The returned DTO is stamped with the current:

```text
BattleId
StateRevision
CardRuntimeId
SourcePresentationId
TargetPresentationId
```

No caller may treat a Preview from an older revision as current.

### 6.2 Effect contribution boundary

Prefer one narrow read-only `UCardEffect` contribution hook over central `BattleManager` branches on concrete effect classes.

Conceptually:

```cpp
virtual void BuildImmediatePreviewOperations(
    const FCardEffectPreviewContext& Context,
    int32 EffectIndex,
    TArray<FImmediatePreviewOperation>& OutOperations
) const;
```

The base implementation may be a default no-op so unsupported effects remain intentionally absent rather than forcing every effect to fabricate an operation.

First supported overrides:

```text
UDamageCardEffect
UGainBlockCardEffect
```

This hook is read-only and must not call `BuildActions`.

### 6.3 Damage implementation

`UDamageCardEffect` builds an `FDamageSpec` from its immutable definition and current query context:

```text
Source = current Player/source
Target = concrete current Target
DamageKind = effect DamageKind
BaseAmount = effect BaseAmount
↓
FDamageModifierPipeline::Resolve
↓
ResolvedAmount
```

The operation carries the effect's authored `HitCount` unchanged.

If the concrete target is invalid for the requested preview, do not invent a target-specific amount.

### 6.4 Block implementation

`UGainBlockCardEffect` builds an `FBlockSpec` from the current self-target context:

```text
Source = Player
Target = Player
BaseAmount = effect BaseAmount
↓
FBlockModifierPipeline::Resolve
↓
ResolvedAmount
```

Do not resolve Self Block against the enemy merely because the player is currently hovering an Enemy for another operation.

### 6.5 A3-2 stale/read-only contract

A3-2 Automation must prove at minimum:

```text
same BattleId/StateRevision + same Card/Target -> deterministic same Operations
Strength changes source-side Damage through the real pipeline
Weak changes source-side Attack Damage through the real pipeline
Vulnerable changes target-specific incoming Damage through the real pipeline
Dexterity/Frailty change Self Block through the real pipeline
fixed multi-hit preserves HitCount and per-hit resolved amount
multiple supported effects preserve EffectIndex order
unsupported effects do not fabricate operations
query does not mutate Energy/HP/Block/Deck/Status/BattleState
query does not enqueue Actions or emit Events
query does not consume RNG
returned identity matches the current battle/revision/card/target
```

### 6.6 Recommended implementation boundary

Prefer a small coherent source slice such as:

```text
Source/SlayTheSpireDemo/Battle/BattleImmediatePreview.h          new DTOs
Source/SlayTheSpireDemo/Battle/BattleManager.h                   public Query declaration
Source/SlayTheSpireDemo/Battle/BattleManagerUIA3Preview.cpp      Query implementation
Source/SlayTheSpireDemo/Cards/Effects/CardEffect.h               narrow read-only operation hook
Source/SlayTheSpireDemo/Cards/Effects/DamageCardEffect.*         Damage contribution
Source/SlayTheSpireDemo/Cards/Effects/GainBlockCardEffect.*      Block contribution
Source/SlayTheSpireDemoTests/Private/Phase6UIA3ImmediatePreviewTests.cpp
```

Exact file placement may follow existing build organization, but do not expand the slice into ViewModel/UMG yet.

### 6.7 A3-2 acceptance

AUTOMATED GATES

```text
1. Editor Build once.
2. Run the smallest A3-2 focused Automation prefix once.
3. Confirm all read-only, target-specific and identity assertions pass.
```

MANUAL PIE GATES

```text
none required for A3-2
```

Do not run Phase6R, Shipping, broad A2 scenarios or manual Preview UI checks in A3-2.

---

## 7. A3-3 — Energy + Target-Aware Legality

A3-3 completes the query-level decision surface. It reuses existing Gameplay validation; it does not duplicate request rules.

### 7.1 Before a concrete target is bound

Use:

```cpp
QueryCardPlayability(Card)
```

This may expose current failures such as:

```text
InvalidBattle
BattleEnded
ResolutionFaulted
WrongTurn
ResolutionBusy
InvalidCard
CardNoLongerInHand
NotEnoughEnergy
```

The absence of a concrete target must not by itself turn the pre-target preview into `InvalidTarget`.

### 7.2 After a concrete target is bound

Use:

```cpp
QueryPlayCard(Card, Target)
```

Target legality remains owned by Gameplay.

The Preview query must not reproduce TargetType checks separately in UI code.

### 7.3 Energy fields

Use the current authoritative Energy and the card's current effective cost (`UCardInstance::GetCurrentCost()` under the current implementation).

Example:

```text
EnergyBefore = 3
EffectiveCost = 2
Validation = Allowed
EnergyAfter = 1
```

If Energy is insufficient:

```text
EnergyBefore = 1
EffectiveCost = 2
Validation = NotEnoughEnergy
EnergyAfter = unavailable
```

Do not fabricate `EnergyAfter = -1`.

The model therefore needs an explicit optional/validity bit for `EnergyAfter` rather than treating every integer as meaningful.

### 7.4 Failure presentation

Gameplay remains the owner of `EGameplayRequestFailureReason` semantics.

The ViewModel may map the existing reason to player-facing `FText` using the same current failure-text semantics already used by the HUD. Do not create a second independent legality enum/rule table merely for Preview unless a concrete Blueprint exposure requirement later proves necessary.

### 7.5 A3-3 acceptance

AUTOMATED GATES

```text
1. Editor Build once after the A3-3 code change.
2. Run the focused A3-3 legality/Energy suite once.
3. Prove pre-target QueryCardPlayability vs target-bound QueryPlayCard behavior.
4. Prove no fabricated negative EnergyAfter.
5. Prove Request semantics remain unchanged.
```

MANUAL PIE GATES

```text
none required for A3-3
```

---

## 8. A3-4 — ViewModel transient Preview lifecycle

A3-4 moves Preview data into explicit transient UI state while preserving Gameplay authority.

### 8.1 Separate PreviewTarget semantics

Add an explicit Preview-target lifecycle, conceptually:

```cpp
SetPreviewTargetById(TargetId)
ClearPreviewTarget()
```

Do not reuse combatant/status inspection lifecycle such as:

```text
OnInspectRequested
OnInspectCleared
```

Inspection and Preview are separate UI concerns even when mouse hover happens to drive both.

### 8.2 ViewModel state

The ViewModel may own transient fields conceptually equivalent to:

```text
SelectedCard identity
LegalTargets
PreviewTarget identity
ImmediatePreview
```

It does not own a copied Gameplay model.

The Preview is valid only while it matches the current live runtime binding and the exact current `BattleId + StateRevision`.

### 8.3 Conservative revision policy

First-version policy is deliberately strict:

```text
BattleId changes
OR
StateRevision changes
↓
clear selected card
clear legal targets
clear PreviewTarget
clear ImmediatePreview
↓
wait for Controller catch-up / current live bindings
```

Do not retain selection across revisions and silently recompute it in the first implementation.

This avoids stale UI acting as if the user were still selecting the same runtime state after Gameplay changed.

### 8.4 Other required clear points

Clear Preview on:

```text
CancelSelection
accepted authoritative card request
target unhover/unfocus
terminal transition
ResolutionFault / PresentationUnavailable transition
loss of the selected card's live binding
loss of the target's live binding
new BattleId/StateRevision
```

A failed normal preview query must fail soft: clear or expose an unavailable Preview state without mutating Gameplay.

### 8.5 A3-4 acceptance

AUTOMATED GATES

```text
1. Editor Build once.
2. Run focused ViewModel Preview-lifecycle Automation once.
3. Assert exact clear/rebuild behavior for selection, target, cancel, accepted request and revision change.
4. Assert stale Preview is never retained across StateRevision.
5. Assert inspection state and PreviewTarget state are independent.
```

MANUAL PIE GATES

```text
none required for A3-4
```

---

## 9. A3-5 — Minimal Native UMG + A2/A3 handoff

A3-5 is the first player-visible target-specific Preview integration.

### 9.1 Native HUD only

Add the smallest dedicated Preview surface to `WBP_BattleHUD_Native` / `UBattleHUDWidget`.

Do not overload:

```text
Enemy Intent text
normal feedback/error text
Status tooltip/inspection surface
A2 committed damage-number surface
```

A Preview surface must be visibly transient and pre-commit.

### 9.2 Preview target nomination

Mouse hover and keyboard/controller focus may both nominate the same logical PreviewTarget candidate.

The combatant presentation layer may expose dedicated Preview request/clear events, but these must remain separate from inspection and authoritative target submission.

Conceptual interaction:

```text
select Strike
↓
legal Enemy target highlighted
↓
hover/focus Enemy
↓
SetPreviewTargetById
↓
ViewModel obtains current Preview Query
↓
HUD shows current Damage value
↓
leave/unfocus Enemy
↓
ClearPreviewTarget
↓
Preview disappears
```

### 9.3 Submission handoff

Required order:

```text
select card
↓
Preview visible for current target
↓
submit/click target
↓
Preview clears immediately
↓
RequestPlayCard authoritative revalidation
↓
if accepted: Interaction enters resolving path
↓
A2 committed Presentation owns visible resolution
↓
A2 catches up to FinalSnapshot
↓
new revision/live bindings become current
```

Do not keep a pre-commit Preview visible over the committed A2 Damage playback.

### 9.4 Minimal display rules

First version may display compact diagnostic text such as:

```text
9
9 x 2
Block 8
Energy 3 -> 2
Not enough Energy
```

Exact visual polish is not an A3 requirement. The key requirement is semantic correctness and a clear pre-commit/post-commit ownership handoff.

If the chosen UMG design uses only the numeric Damage value near a target, the operation type may be conveyed by the dedicated preview context/layout rather than by adding redundant words.

### 9.5 A3-5 combined PIE acceptance

Use production:

```text
/Game/SlayTheSpireDemo/Maps/L_BattleTest
```

One focused PIE session may cover the required visual checks.

Minimum scenarios:

```text
Strike / Enemy target
- select Strike
- hover/focus Enemy
- target-specific Damage Preview appears
- submit Enemy
- Preview disappears before committed A2 playback
- CardPlayed / Damage / CardZoneChanged playback remains coherent
- input unlocks only after A2 catch-up

Defend / Player self target
- select Defend
- preview Player as the legal self target
- current Block value is visible
- submit Player
- Preview clears and committed Block playback takes over

revision invalidation
- cause a relevant state change such as modifier/status change
- old selection/Preview does not survive the new StateRevision
- after a new selection, the new Preview reflects current modifier state
```

### 9.6 A3-5 acceptance

AUTOMATED GATES

```text
1. Editor Build once.
2. Compile/save the affected Native WBP assets once and require BS_UP_TO_DATE / 0 errors.
3. Run the smallest focused A3-5 UI/ViewModel integration Automation once.
4. Preserve production Legacy dependency count = 0 when UI asset dependencies change.
```

MANUAL PIE GATES

```text
1. One production L_BattleTest focused PIE session.
2. Verify Preview appears/disappears at the correct interaction boundaries.
3. Verify no Preview remains visible over A2 committed playback.
4. Verify the displayed Damage/Block value is readable and corresponds to current state.
5. Verify no input lock, duplicate target highlight, stale Preview or visible A3/A2 flashback.
```

Do not automatically add Phase6R, A2D5, Shipping, broad Scenario A-E replay or Legacy parity. Run broader gates only if a concrete shared-contract change invalidates them or a later explicit Phase 6UI-A seal requires them.

---

## 10. First-version explicitly deferred scope

The following are **not** part of first A3 completion:

```text
multi-enemy Gameplay/read-model expansion
HP ghost bars
predicted final HP
predicted final HP loss
whole-card final-result simulation
Trigger/reaction simulation
Relic reaction simulation
Status merge/final-stack prediction
specific cards drawn
Shuffle result prediction
Victory/Defeat prediction
cross-revision retained selection
automatic selection rebuild across StateRevision
advanced target arrows
final animation/VFX/SFX polish
```

The current project has one Player and one formal current Enemy. Do not introduce multi-enemy architecture merely to prove A3 target variation. Use modifier/status changes across a new StateRevision to validate target-specific recalculation and stale-preview invalidation.

Advanced Preview belongs to Phase 6UI-B unless a concrete earlier playability requirement changes that boundary.

---

## 11. Validation policy for A3

Follow `docs/ValidationExecutionPolicy.md`.

Default per-slice budget:

```text
Build once
→ smallest focused Automation once
→ manual PIE only when the slice has a genuinely visual Gate
→ record evidence
→ STOP
```

Passing Gates are sticky. Do not rerun A3-1, A2E, A2N, Phase6R or unrelated historical suites merely because A3 advances.

If a shared contract such as `UBattleHUDViewModel` or `UBattleHUDWidgetBase` is materially changed, run only the directly affected historical regression required by that shared contract. Do not escalate automatically to full repository validation.

Every A3 slice must document acceptance under:

```text
AUTOMATED GATES
MANUAL PIE GATES
```

---

## 12. Documentation and checkpoint rules

During A3:

```text
docs/Phase6UIA3Implementation.md
= active design / ordering / acceptance authority

docs/Phase6UIA3DynamicTextImplementation.md
= sealed A3-1 implementation and evidence

docs/Phase6UIA2EImplementation.md
= sealed A2 history + original A3 follow-up design basis

docs/CODEX_GOAL_CHECKPOINT.md
= resumable current A3 execution state only

docs/DevelopmentPhases.md
= project-wide phase summary
```

Do not rewrite sealed A2/A2N validation history to make it look as though A3 existed earlier.

---

## 13. A3 completion criteria

Phase 6UI-A3 is complete only when all of the following are true:

```text
A3-1 Dynamic Text remains sealed
Gameplay owns target-specific Preview construction
Preview uses current BattleId + StateRevision identity
Damage uses the real current Damage Modifier Pipeline
Damage means incoming per-hit damage before Block absorption
fixed multi-hit retains independent HitCount semantics
Self Block uses the real current Block Modifier Pipeline
Energy and legality reuse existing Gameplay Query rules
ViewModel owns only transient selection/Preview lifecycle
Preview clears on revision change and accepted submission
UMG does not calculate rules or iterate CardEffects
Native HUD is the only active A3 implementation
Legacy runtime dependency count remains 0
A3 Preview hands off cleanly to A2 committed playback
focused Automation passes
required Native WBP compile/save passes
combined production-map A2/A3 PIE passes
```

Only after those criteria are satisfied should `DevelopmentPhases.md` move UI-A3 and Phase 6UI-A to complete and make Phase 7 Relics the normal next phase.

---

## 14. Next exact implementation action

The next implementation slice is:

```text
A3-2A — Immediate Preview DTO + read-only Effect contribution contract
```

Start with:

```text
FImmediateCardPreview
FImmediatePreviewOperation
Damage / Block operation type
UCardEffect narrow read-only contribution hook
UDamageCardEffect contribution
UGainBlockCardEffect contribution
focused read-only operation tests
```

Do **not** touch ViewModel/UMG in A3-2A.

After A3-2A reaches its focused automated Gate, stop and record evidence before moving to the BattleManager query completion/next A3-2 slice.
