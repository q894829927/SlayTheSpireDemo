# SlayTheSpireDemo Agent Instructions

## 1. Project Goal

This is an Unreal Engine 5.8 C++ learning/demo project inspired by Slay the Spire combat architecture.

Build a small, extensible and deterministic card-battle framework that supports reusable card effects/actions, deck zones, statuses, relic triggers, modifier pipelines and event-driven interactions without hard-coded combos.

Long-term flow:

```text
CardData / CardInstance
        ↓
CardEffect
        ↓
BattleAction
        ↓
BattleActionQueue
        ↓
Operation Spec
        ↓
Modifier Pipeline
        ↓
Commit
        ↓
BattleEvent
        ↓
Trigger listeners
        ↓
New BattleAction
        ↓
BattleActionQueue
```

`BattleStateMachine` controls turn flow. `BattleActionQueue` controls execution order. Modifier pipelines handle deterministic pre-commit modification. Events/triggers handle post-commit reactions.

---

## 2. Current Repository State

- [x] UE5.8 C++ project and runtime module exist.
- [x] Phase 1 minimal combat loop implemented and PIE-validated.
- [x] Phase 2 `BattleActionQueue` implemented and PIE-validated.
- [x] Phase 3 deck system implemented and PIE-validated.
- [x] Phase 4 data-driven cards implemented and PIE-validated.
- [x] Phase 5 Modifier-Based Framework / status system implemented and validated.
  - [x] Phase 5A Status Runtime + ApplyStatusAction implemented and PIE-validated.
  - [x] Phase 5B1 Damage Spec + DamageFlatAdd + Strength implemented and PIE-validated.
  - [x] Phase 5B2 Damage Ratio + Weak + Vulnerable implemented and PIE-validated.
  - [x] Phase 5C Block Spec + Dexterity + Frailty implemented and PIE-validated.
  - [x] Phase 5R regression Automation gate implemented and UE5.8 self-hosted CI validated at 13/13.
- [x] Phase 6 battle events / triggers complete for the defined Phase 6 scope.
  - [x] Phase 6A TurnEnd Trigger Vertical Slice — COMPLETE; UE5.8 self-hosted CI validated at 23/23.
  - [x] Phase 6B Battle Turn Wiring — COMPLETE; expanded Queue contract suite passed at 12/12, total Phase5 + Phase6A + Phase6B gate passed 48/48, and post-hardening PIE turn-cycle validation passed.
  - [x] Phase 6C DeckShuffled Event — COMPLETE; UE5.8 Editor build + Phase6C 5/5 and total Phase5–Phase6C 53/53 Automation gate passed.
  - [x] Phase 6R Regression Gate + test-module extraction — COMPLETE; UE5.8 full 53/53 regression gate and Shipping exclusion validation passed.
- [ ] Phase 6UI-A playable Battle UI — IN PROGRESS.
  - [x] UI-A0 Playable Gameplay Boundary — COMPLETE; UE5.8 Editor build + Phase5/6 regressions + UI-A0 20/20 passed, current owner-only gate 73/73.
  - [x] UI-A1 Operable Battle HUD — COMPLETE; current Self-target Player-selection path revalidated with UE5.8 UI-A1 11/11 and manual PIE.
  - [ ] UI-A2 Basic Committed Presentation — NEXT / DESIGN LOCKED; UI-A2A infrastructure is the next implementation slice. See `docs/Phase6UIA2Implementation.md` and Section 16.
  - [ ] UI-A3 Deterministic Immediate Preview — dynamic card/status text slice implemented, DataAsset-authored and UE5.8/PIE/package revalidated with UI-A3 8/8; remaining target-specific/energy-result preview still planned.
- [ ] Phase 7 relic system — PLANNED AFTER Phase 6UI-A.
- [ ] Phase 8 Pommel Strike+ + Sundial architecture/presentation validation — PLANNED AFTER Phase 7.
- [ ] Phase 6UI-B advanced UX / preview / developer tooling — PLANNED AFTER Phase 8.
- [ ] Presentation Polish — PLANNED AFTER Phase 6UI-B.

### Validation summary

Phase 1 validated HP, Block, Energy, turn flow, enemy attacks, victory/defeat and command rejection after battle end.

Phase 2 validated queued Damage/Block, explicit `Finish()`, deterministic front/back ordering, one final `QueueEmpty`, and enemy-turn progression only after the queue drains.

Phase 3 validated Draw/Hand/Discard/Exhaust, DrawPile end as top, one-card draw actions, deterministic Fisher–Yates shuffle with battle-scoped `FRandomStream`, stable card identity, and queued `Shuffle → RetryDraw` with one final `QueueEmpty`.

Phase 4 validated data-driven CardData/CardInstance/effect composition, PlayArea lifecycle, Energy spending/rejection and the Pommel Strike `PlayCardAction → DamageAction → DrawCardAction → FinishCardPlayAction` chain.

Phase 5 validated the Status Runtime and typed Damage/Block Modifier pipelines. The owner-only regression gate remains Phase5 13/13.

Phase 6 validated deterministic post-commit Events/Triggers, exact-instance status decay, Queue fault safety, real turn lifecycle, DeckShuffled ordering and the Editor-only test module boundary. The trusted Phase5–Phase6C regression remains 53/53.

Phase 6UI-A0 validated authoritative turn/Hand lifecycle, formal Query/Request APIs, coherent `(BattleId, StateRevision)` snapshots, non-reentrant `OnReadStateReady`, committed Enemy Intent and UI-A0 20/20.

Phase 6UI-A1 validated the concrete Battle HUD through normal UI controls. Enemy-target and Self-target cards both use gameplay-provided public legal-target selection and formal Request revalidation. Defend resolves by selecting the highlighted Player presentation. The current owner gate passes Phase5 13/13 + Phase6A 23/23 + Phase6B 12/12 + Phase6C 5/5 + Phase6UIA0 20/20 + Phase6UIA1 11/11 + Phase6UIA3 8/8 = 92/92, with the normal PIE battle loop and packaged Defend `{Block}` text validated.

Phase 6UI-A3 dynamic-text slice validates `BattleTextResolver`, read-only Damage/Block preview pipelines and authored Card/Status format arguments. Target-specific exact preview and immediate Energy-result preview remain future UI-A3 work.

### Manual UE assets/configuration

Current project-owned content remains under `Content/SlayTheSpireDemo/`. Important validated cards/statuses include Strike, Pommel Strike, Defend and Strength/Weak/Vulnerable/Dexterity/Frailty DataAssets.

Current temporary `L_BattleTest` debug bindings remain debug-only and are not the formal card-input architecture.

---

## 3. Development Order

Do not skip ahead unless the user explicitly requests it.

### Phase 1 — Minimal Combat Loop — COMPLETE

Implemented `ACombatant`, `ABattleManager`, HP, Block, Energy, turn state and victory/defeat.

### Phase 2 — BattleActionQueue — COMPLETE

Implemented `UBattleAction`, `UBattleActionQueue`, `UDamageAction`, `UGainBlockAction`.

Durable rules:

- one authoritative action executes at a time;
- ordering and completion are explicit;
- actions may schedule dependencies but never pump/advance the queue;
- dependent batches needed for one logical chain must be queued before the current action finishes.

### Phase 3 — Deck System — COMPLETE

Implemented `UDeckRuntime`, DrawPile, Hand, DiscardPile, ExhaustPile, `UDrawCardAction`, `UDiscardCardAction`, `UShuffleDeckAction`.

Durable rules:

- DeckRuntime owns authoritative pile truth;
- DrawPile end is top;
- one DrawAction means one draw attempt;
- draw does not synchronously shuffle;
- `ShuffleDeckAction` only shuffles;
- battle RNG is initialized once and consumed across shuffles;
- runtime card identity is stable object identity after Phase 4 migration.

### Phase 4 — Data-Driven Cards — COMPLETE

Implemented `UCardData`, `UCardInstance`, card types/targets/destinations, reusable effects, `UPlayCardAction` and `UFinishCardPlayAction`.

Established flow:

```text
UCardData
↓
UCardInstance
↓
UPlayCardAction
↓
UCardEffect::BuildActions() const
↓
Damage / Block / Draw Actions
↓
UFinishCardPlayAction
↓
resolved destination
```

Durable rules:

- effect subobjects are immutable shared definitions;
- effects capture base intent, not future-state-dependent resolved values;
- effects must not control the queue;
- all dependent card-play actions are queued before `PlayCardAction::Finish()`;
- `UCardInstance*` is normal runtime identity; `RuntimeId` is a stable presentation/debug identifier;
- card destination resolves at cleanup Execute-time;
- actions fail soft and always `Finish()` on invalid execution preconditions.

### Phase 5 — Modifier-Based Framework and Status System — COMPLETE

Development slices:

```text
5A  Status Runtime + ApplyStatusAction                              COMPLETE
5B1 FDamageSpec + DamageFlatAdd + Strength                         COMPLETE
5B2 DamageRatio + Weak + Vulnerable                                COMPLETE
5C  FBlockSpec + BlockFlatAdd + BlockRatio + Dexterity + Frailty   COMPLETE
5R  Phase 5 Automation Regression Gate                             COMPLETE / CI PASSED 13/13
```

Durable Phase 5 rules:

- `UStatusData` is immutable definition data; `UStatusInstance` owns runtime Amount/RuntimeSequence/Owner;
- `UStatusContainer` owns authoritative status membership and merge/create decisions;
- status application is additive and uses positive Amount; lifecycle reduction uses exact-instance `ReduceStatusAction` rather than negative Apply;
- Damage and Block resolve through typed specs/pipelines at Execute-time;
- modifier ordering is deterministic by Domain/Phase/Priority/RuntimeSequence/LocalModifierIndex;
- ratio arithmetic uses explicit integer numerator/denominator and floors after each modifier;
- current modifier collection may read StatusContainer directly while Status is the only real modifier source.

### Phase 6 — Battle Events and Triggers — COMPLETE

Core responsibility split:

```text
BattleEvent
= immutable-by-contract fact describing something that already committed

Trigger
= read-only rule that decides whether it reacts and builds Reaction Actions

BattleAction
= the only object that performs authoritative gameplay mutation
```

Required flow:

```text
Action Execute
↓
Modifier Pipeline when applicable
↓
Commit
↓
BattleEvent
↓
collect current trigger sources
↓
filter + deterministic sort
↓
Build Reaction Actions
↓
atomic Reaction Batch insertion
↓
Current Action Finish
↓
BattleActionQueue executes reactions
```

#### Phase 6 development order

```text
6A  TurnEnd Trigger Vertical Slice                    COMPLETE / 23/23
6B  Battle Turn Wiring                                COMPLETE / 12/12
6C  DeckShuffled Event                                COMPLETE / 5/5
6R  Regression Gate + test-module extraction          COMPLETE / TOTAL 53/53 + SHIPPING EXCLUSION
```

Durable Phase 6 rules:

- events are short-lived typed value facts; Dispatch/Trigger never caches event references;
- Status triggers are collected on demand; no persistent Trigger Registry before a real second source family;
- Trigger ordering is `Priority → RuntimeSequence → LocalTriggerIndex`;
- trigger eligibility is snapshot semantics while resulting Actions still validate live state at Execute-time;
- triggers build Actions but never mutate gameplay or drive the queue directly;
- reaction batches are atomically inserted and nested reactions use queued depth-first semantics;
- Queue resolution faults enter only at safe points, broadcast once, suppress normal QueueEmpty and reject further queue mutation;
- `OnQueueEmpty` is an observable non-reentrant boundary; BattleManager defers macro turn continuation until all observers return;
- player and enemy TurnEnded timing, hand cleanup and turn-start work are authoritative gameplay semantics;
- `FDeckShuffledEvent` emits only after a successful discard-to-draw shuffle commit and before RetryDraw continuation;
- initial battle setup shuffle consumes the battle RNG but is not a DeckShuffled gameplay event;
- Automation-only code remains in the Editor-only `SlayTheSpireDemoTests` module and is excluded from Shipping.

### Phase 6UI-A — Playable Battle UI — IN PROGRESS

```text
UI-A0 Playable Gameplay Boundary   COMPLETE / 20/20
↓
UI-A1 Operable Battle HUD          COMPLETE / 11/11 + PIE/PACKAGE VALIDATED
↓
UI-A2 Basic Committed Presentation NEXT / DESIGN LOCKED / UI-A2A IMPLEMENTATION NEXT
↓
UI-A3 Deterministic Immediate Preview — DYNAMIC TEXT SLICE VALIDATED / 8/8; REMAINING TARGET/ENERGY PREVIEW PLANNED
```

Detailed UI-A2 implementation contract: `docs/Phase6UIA2Implementation.md` and Section 16.

### Phase 7 — Relics — PLANNED AFTER PHASE 6UI-A

Implement relic listeners through the event/trigger architecture. First validation: Sundial; optional Abacus.

When Phase 7 introduces the first non-Status modifier/trigger source, extract the smallest contributor/collector boundary required to combine Status and Relic sources. Do not disguise a Relic as a Status and do not introduce a universal modifier context.

### Phase 8 — Combo Architecture Validation — PLANNED AFTER PHASE 7

Validate two upgraded Pommel Strikes + Sundial without special-case combo code. The interaction must emerge from generic card, draw, shuffle, event, modifier and action rules and should be visually understandable through the playable UI.

### Phase 6UI-B — Advanced UX / Presentation Tooling — PLANNED AFTER PHASE 8

Advanced preview, Keyword/CardText presentation, Developer Overlay, presentation timeline tooling, controller/accessibility work and responsive-layout work belong here unless required earlier for basic playability or diagnosis.

### Presentation Polish — PLANNED AFTER PHASE 6UI-B

Drag/drop, fast-play shortcuts, final hand layout, target arrows, animation refinement, VFX/SFX and speed/skip polish belong here. Presentation remains non-authoritative.

---

## 4. Core Architecture Rules

### 4.1 Deterministic gameplay

Same initial state + input sequence + RNG seed must be reproducible. Correctness must not depend on frame rate, UObject address, unordered iteration, actor discovery order or animation timing.

### 4.2 Cards describe effects; they do not own battle rules

Normal cards are data + reusable effects. Avoid unrelated battle-system logic in individual card implementations.

### 4.3 Gameplay mutations flow through actions

Preferred:

```text
request → BattleAction → BattleActionQueue → typed resolution / owning gameplay object
```

### 4.4 Events notify; actions mutate

Events announce facts. Trigger listeners normally enqueue actions.

### 4.5 Avoid recursive gameplay chains

Prefer queued resolution over deep synchronous draw/shuffle/trigger chains.

### 4.6 Separate definitions from runtime instances

```text
CardData != CardInstance
StatusData != StatusInstance
RelicData != RelicInstance
```

Never store runtime state in shared definitions.

### 4.7 Composition over content-specific inheritance

Prefer reusable effects/modifiers and data composition.

### 4.8 UI is presentation, not authority

UMG may request gameplay and display state but never owns authoritative combat/deck/status state.

### 4.9 Keep dependencies explicit and minimal

Avoid gameplay-time actor searches. Pass/store explicit references or narrow contexts.

### 4.10 No premature GAS migration

Validate the project-specific queue/deck/modifier/trigger model first.

### 4.11 Actions may schedule dependencies but do not drive the queue

Actions may enqueue follow-ups when required but never call `PumpQueue`, `ProcessNext` or equivalent execution advancement.

### 4.12 Shared definition objects are immutable at runtime

Instanced definition subobjects such as CardEffects/Modifiers/Triggers are shared configuration and must be logically const/stateless.

### 4.13 Resolve future-state-dependent results at Execute-time

Enqueue-time captures stable intent/base inputs. Mutable-state-dependent values resolve when the action executes. Snapshot semantics must be explicit and mechanic-specific.

### 4.14 Action validation must fail soft and remain action-specific

Invalid execution dependencies must log when useful, call `Finish()`, and never wedge the queue. Framework invariant failures discovered while an Action is executing may request a Queue resolution fault, but the current Action must still honor the safe Finish/return contract.

### 4.15 Card destination is resolved, not hard-coded

Card cleanup resolves destination at Execute-time and delegates authoritative zone movement to DeckRuntime.

### 4.16 Turn-state transitions depending on queued work are transactional

Build and validate the required Action or full turn batch, atomically enqueue it, then commit the associated `BattleState`, then start processing.

### 4.17 Runtime trigger source is identity authority

For Trigger collection, derive Owner, RuntimeSequence and definition metadata from the exact runtime source.

### 4.18 QueueEmpty is a non-reentrant observable boundary

BattleManager must defer authoritative macro turn continuation until after all QueueEmpty multicast observers return.

### 4.19 DeckShuffled is a post-commit fact

`FDeckShuffledEvent` may be emitted only after a real discard-to-draw shuffle commits successfully. Expected/no-op shuffle attempts emit no event.

### 4.20 Public stable-read completion is battle-level and non-reentrant

`BattleActionQueue::OnResolutionIdle` is internal. Widgets must not use QueueEmpty/idle as their public completion protocol. BattleManager maps settled/fault outcomes into `OnReadStateReady(BattleId, StateRevision)`.

A public Ready publication must not fire before an accepted public Request returns. Ready is a non-replaying edge notification.

### 4.21 Player-facing Intent values must state their time semantics

`EnemyIntentPlayerFacing.CurrentResolvedDamageAmount` is a current-snapshot gameplay-derived value, not guaranteed future EnemyTurn damage.

### 4.22 Initial battle shuffle is setup, not DeckShuffled gameplay

Initial deterministic shuffle consumes the battle RNG but emits no DeckShuffled event or Presentation shuffle record.

---

## 5. Modifier-Based Framework Architecture

```text
ActionQueue       → execution timing/order
Modifier Pipeline → pre-commit modification/interception/override/clamp
BattleEvent       → post-commit fact
Trigger           → reaction that may enqueue new Actions
```

### 5.1 Action vs Modifier vs Trigger

Use Action for authoritative operations, Modifier for changing an operation before commit, Trigger for reacting to a completed fact.

### 5.2 Deterministic modifier ordering

Within a typed domain:

```text
Phase → Priority → RuntimeSequence → LocalModifierIndex
```

Do not use names, localized text, UObject addresses, unordered iteration, registration order or actor discovery order as tie-breaks.

### 5.3 Typed modifier pipelines

Prefer typed specs such as `FDamageSpec`, `FBlockSpec`, future concrete CardCost/StatusApply specs. Avoid one universal modifier context.

### 5.4 Cancellation and interception

Prefer intercept-before-commit to commit-then-undo. Add cancellation/override phases only when a concrete mechanic needs them.

### 5.5 Status runtime semantics

`UStatusData` is immutable definition data. `UStatusInstance` owns runtime Amount/RuntimeSequence/Owner. Reapplication preserves RuntimeSequence; removal then recreation gets a new sequence.

### 5.6 Integer modifier arithmetic

Ratio modifiers use explicit non-negative numerator and positive denominator; each ratio resolves/floors before the next modifier using safe integer intermediates.

### 5.7 Modifier collection boundary

Direct StatusContainer collection is an implementation detail while Status is the only source. Extract the smallest multi-source collector when Relics arrive.

---

## 6. UE5 C++ Conventions

Use normal Unreal prefixes. Prefer forward declarations and small public headers. UObject runtime ownership must be GC-safe through clear Outer/`UPROPERTY`/`TObjectPtr` references. Do not enable Tick by default.

Automation-only sources belong under `Source/SlayTheSpireDemoTests/Private/`.

`ABattleManager` may temporarily own battle orchestration, battle-scoped allocators and PIE debug entry points. Do not split it merely for aesthetic purity, and do not keep growing permanent debug/rule APIs when Automation can cover the same invariant.

---

## 7. Blueprint and Asset Rules

Prefer C++ for authoritative battle/deck/status/modifier/event logic. Prefer Blueprint/UMG/DataAssets for presentation, assembly and content configuration.

All project-owned assets live under `Content/SlayTheSpireDemo/`.

Keep Keyword presentation separate from Gameplay Status/Modifier/Action/DeckRule semantics. Card/Status descriptions use semantic named templates; `BattleTextResolver` resolves deterministic numeric placeholders through read-only gameplay pipelines. Do not implement KeywordLibrary/RichText tooling merely because status mechanics have player-facing names.

Text/source agents must never claim to create/edit `.uasset` or `.umap` assets without UE Editor access.

Do not intentionally commit generated/local files:

```text
Binaries/
DerivedDataCache/
Intermediate/
Saved/
.vs/
*.sln
*.slnx
```

---

## 8. Change-Scope Rules for Agents

1. Inspect relevant existing files first.
2. Make the smallest coherent change.
3. Do not refactor unrelated code.
4. Do not rename public APIs/assets without need.
5. Do not silently change plugins, engine association or build settings.
6. Do not add third-party dependencies without approval.
7. Do not add multiplayer architecture unless requested.
8. Do not add map/shop/meta-progression before core combat validation unless requested.
9. Preserve explainability for learning.
10. Do not skip development phases without explicit approval.
11. Do not implement future mechanisms merely because they are documented.
12. Do not introduce `FInstancedStruct`, StructUtils or another representation dependency without a concrete requirement.
13. Do not introduce a universal modifier context or GameplayTag-based damage taxonomy without a concrete implemented need.
14. Do not implement KeywordLibrary, rich-text parsing or keyword tooltips/styles merely because status mechanics have player-facing names.
15. Never model `Keyword = StatusData`.
16. Do not introduce a generic modifier-contributor framework while Status is the only real modifier source.
17. Never implement a Relic or unrelated modifier source as a fake Status.
18. Trigger/Event execution must not depend on delegate registration order, UObject address, actor discovery order, source enumeration order or unordered-container iteration.
19. Do not keep adding permanent rule-test entry points to `ABattleManager` when Automation can cover the same deterministic regression.
20. Do not introduce a persistent Trigger Registry while StatusContainer is the only real trigger-source membership authority.
21. Turn state changes occur only after required Action/batch insertion succeeds.
22. Resolution budget is checked before dequeuing/executing the next Action; fault broadcasts once and suppresses normal QueueEmpty.
23. RuntimeSource is authoritative for Trigger Owner, RuntimeSequence and identity.
24. Event/Trigger Context references are synchronous dispatch-lifetime values and must not be cached.
25. Trigger definitions are read-only rule builders; they must not directly mutate gameplay or drive the ActionQueue.
26. Per-trigger build failure is fail-soft, but final framework-level Reaction Batch insertion failure requests Queue ResolutionFault.
27. `ReduceStatusAction` targets an exact runtime StatusInstance.
28. Do not add TriggerPhase until a real timing mechanic requires it.
29. Current fixed Enemy batching does not imply all future enemy follow-ups must be precomputed.
30. ResolutionFault is framework safety, not gameplay balance.
31. QueueEmpty is non-reentrant and must not rely on multicast registration order.
32. Automation-only reflected classes remain in the Editor-only test module.
33. `FDeckShuffledEvent` emits only after successful shuffle commit and reactions precede RetryDraw.
34. Phase 7 follows playable Phase 6UI-A unless the user changes the order.
35. `PlayerTurn` is Gameplay request-eligible; Presentation may still lock the View.
36. Before UI-A2, UI-A0/UI-A1 presentation catch-up is immediate/no-op.
37. Formal UI consumers use battle-level Ready/coherent snapshots, not QueueEmpty.
38. `OnReadStateReady` must not fire re-entrantly before an accepted Request returns.
39. Intent current-resolved damage is current-state, not guaranteed-future semantics.
40. Initial setup shuffle emits no DeckShuffled gameplay event.
41. During QueueEmpty observer notification, healthy empty Action batches remain legal no-op success; non-empty direct insertion is rejected.
42. During UI-A2 playback, `FPresentationStateSnapshot` is the only historical display input. Historical Envelopes never query mutable Gameplay runtime to render.
43. Runtime input bindings are not historical state. Refresh them only after Presentation catches up to the newest matching `(BattleId, StateRevision)`.
44. When Presentation is enabled, `OnReadStateReady` must not bypass Presenter/Controller and directly apply live state to the HUD ViewModel.
45. Ordinary validation rejection creates no Presentation Resolution. Any post-validation framework fault must be represented by a fault/system Presentation Resolution.
46. `ACombatant`, `UDeckRuntime` and `UStatusContainer` return typed Commit/Mutation results and do not depend on Presentation Recorder.
47. Presentation backlog, timeout, missing callback, skip or disablement never requests Gameplay ResolutionFault.
48. Combatant Presentation IDs are resolved by one Battle-layer resolver and the resolved values are non-empty, battle-scoped unique and shared by Snapshot, LegalTargets and Presentation Records.
49. Presentation RecordWriter/Sink is an optional, explicit, battle-scoped dependency. Actions must not world-search, actor-search, infer it through UObject Outer or use a global Recorder; nested/reaction Actions in one active Resolution receive the same writer through explicit context propagation.
50. Record append failure is Presentation-only failure: Gameplay Commit remains committed, Action Finish/Queue ordering remains unchanged, and no Gameplay fault is requested.
51. One active Presentation Resolution seals at most once. Freeze/Seal failure clears the builder, disables/degrades Presentation for the battle, allows ordinary deferred `OnReadStateReady` to continue and never becomes Gameplay ResolutionFault.
52. `CardPlayed` history must preserve exact Energy Before/After/CostPaid rather than reading live/final Energy during playback.
53. `PresentationUnavailable` is a UI-only state. ViewModel initialization remains successful enough for Presenter to create the normal HUD/error surface; player input stays disabled and a clear development-facing error is shown.
54. Generic `ResolutionFault` Record lifecycle, PlaybackToken and Skip/timeout/stale-callback safety belong to UI-A2A infrastructure. A2D adds formal terminal/fault visual treatment rather than introducing those mechanisms for the first time.
55. Internal Resolution Seal and public notification are separate lifecycles. Once Gameplay/macro work becomes stable, Freeze/Seal must synchronously release the active builder before any next `BeginResolution`; a sealed Envelope may wait for deferred public delivery.
56. `OnPresentationResolutionReady` obeys the same accepted-Request non-reentrancy rule as `OnReadStateReady`: neither public callback may fire before the originating accepted Request returns.
57. Writer absence from the start is valid no-history mode. If an active writer Append fails, invalidate the whole current record batch, discard buffered unpublished Records and never Seal/Publish a partial Envelope; use the final frozen baseline/fail-safe path instead.
58. Envelope de-duplication identity is `(BattleId, ResolutionId)`. Read-state edge de-duplication identity is `(BattleId, StateRevision)`. Never use the last published read revision as the sole Envelope identity.

Prefer clear architecture over clever abstractions.

---

## 9. Build and Verification Rules

After C++ changes:

- verify includes/module dependencies;
- build `SlayTheSpireDemoEditor` when a build environment is available;
- report build errors instead of masking them;
- never claim successful UE build/PIE without actually running it;
- if source tooling cannot run UE, require user-side compile/PIE before marking new source changes validated.

Current trusted evidence:

```text
Phase 5       13/13 PASS
Phase 6A      23/23 PASS
Phase 6B      12/12 PASS
Phase 6C       5/5 PASS
Phase 6UI-A0  20/20 PASS
Phase 6UI-A1  11/11 PASS
Phase 6UI-A3   8/8 PASS
Current combined owner run 92/92 PASS
```

The exact totals are run evidence, not permanent acceptance constants.

Manual PIE normal UI player → enemy → player loop passed. Self-target Defend → highlighted Player passed. Packaged Defend dynamic `{Block}` description passed.

The next implementation slice is `Phase 6UI-A2A — committed-presentation infrastructure`; do not start Damage animation before the Section 16/detailed-doc A2A contracts pass focused Automation.

When UE Editor work is required, label it `USER ACTION REQUIRED` and give exact steps.

---

## 10. User-Action Boundary

User performs UE Editor work that text-only tools cannot safely represent, including creating/configuring DataAssets, Blueprints/UMG, level actors/references, visual Blueprint wiring, saving `.uasset`/`.umap`, and PIE validation.

Instructions must include exact path/menu, asset/class, property values, expected result and what logs/screenshots to return on failure.

---

## 11. Implemented Core Classes

Representative implemented source areas:

```text
Battle/BattleManager.h/.cpp
Battle/BattleReadSnapshot.h
Battle/BattleRequestTypes.h
Combat/Combatant.h/.cpp
Actions/BattleAction.h/.cpp
Actions/BattleActionQueue.h/.cpp
Actions/DamageAction.h/.cpp
Actions/GainBlockAction.h/.cpp
Actions/DrawCardAction.h/.cpp
Actions/DiscardCardAction.h/.cpp
Actions/ShuffleDeckAction.h/.cpp
Actions/PlayCardAction.h/.cpp
Actions/FinishCardPlayAction.h/.cpp
Actions/ApplyStatusAction.h/.cpp
Actions/ReduceStatusAction.h/.cpp
Actions/TurnEndedAction.h/.cpp
Cards/CardData.h/.cpp
Cards/CardInstance.h/.cpp
Cards/Effects/*
Deck/DeckRuntime.h/.cpp
Status/StatusData.h/.cpp
Status/StatusInstance.h/.cpp
Status/StatusContainer.h/.cpp
Modifiers/Damage/*
Modifiers/Block/*
Events/BattleEvent.h
Events/BattleTrigger.h/.cpp
Events/BattleEventDispatcher.h/.cpp
Enemy/EnemyIntent.h
UI/BattleHUDTypes.h
UI/BattleHUDViewModel.h/.cpp
UI/BattleHUDWidgetBase.h/.cpp
UI/BattleHUDCombatantPresentationWidgetBase.h/.cpp
UI/BattleHUDPresenter.h/.cpp
Source/SlayTheSpireDemoTests/Private/Phase5RegressionTests.cpp
Source/SlayTheSpireDemoTests/Private/Phase6*.cpp/.h
Source/SlayTheSpireDemoTests/Private/Phase6UIA*.cpp/.h
```

Relevant implementation documents:

```text
docs/Phase6CImplementation.md
docs/Phase6RImplementation.md
docs/Phase6DeferredEngineering.md
docs/Phase6UIA0Implementation.md
docs/Phase6UIA1Implementation.md
docs/Phase6UIA1CombatantInspectionSetup.md
docs/Phase6UIA2Implementation.md
docs/Phase6UIA3DynamicTextImplementation.md
```

---

## 12. Acceptance Summary

- Phase 1 — PASSED.
- Phase 2 — PASSED.
- Phase 3 — PASSED.
- Phase 4 — PASSED.
- Phase 5 — PASSED; focused Phase5 13/13.
- Phase 6A — PASSED; 23/23.
- Phase 6B — PASSED; 12/12 and PIE turn lifecycle.
- Phase 6C — PASSED; 5/5, Phase5–Phase6C 53/53.
- Phase 6R — PASSED; Editor test module + Shipping exclusion.
- Phase 6UI-A0 — PASSED; 20/20.
- Phase 6UI-A1 — PASSED; 11/11 + Self-target Player selection + normal PIE loop.
- Phase 6UI-A2 — NEXT / DESIGN LOCKED: frozen display state + explicit optional RecordWriter + Gameplay Begin/Abort/internal Seal + immutable Resolution Envelope + deferred public delivery + bounded fail-safe Controller queue; UI-A2A infrastructure precedes visible Damage animation.
- Phase 6UI-A3 — PARTIAL / CURRENT SLICE PASSED; dynamic text 8/8, remaining target-specific/Energy preview planned.
- Phase 6UI-A — IN PROGRESS.
- Phase 7 — PLANNED AFTER Phase 6UI-A.
- Phase 8 — PLANNED AFTER Phase 7.
- Phase 6UI-B — PLANNED AFTER Phase 8.
- Presentation Polish — PLANNED LAST.

---

## 13. Architecture Validation Principle

Complex interactions must emerge from generic rules.

Pommel Strike knows only configured damage/draw effects. Defend knows only Block. DeckRuntime knows card zones/draw/shuffle. Sundial should eventually know only shuffle events. Statuses/relics contribute through generic typed pipelines/events/triggers rather than special-case combo code.

If a new card/relic/status requires editing many unrelated classes or scattering concrete checks through battle code, reconsider the architecture.

---

## 14. Documentation and Progress Updates

When completing a meaningful phase:

- update Current Repository State;
- record durable architecture invariants;
- record required manual UE assets/configuration;
- keep Current Repository State, Development Order, Acceptance Summary and detailed phase sections synchronized;
- do not rely on a later section to silently override stale contradictory progress text elsewhere;
- keep documentation synchronized with actual source/PIE/Automation state;
- do not fill this file with daily implementation trivia.

---

## 15. Planned Playable UI, MVVM and Presentation Architecture

Phase 6UI-A0 and UI-A1 are complete. The current UI-A3 dynamic-text slice is validated. Phase 6UI-A2A is the next implementation slice. Section 16 and `docs/Phase6UIA2Implementation.md` are the synchronized UI-A2 contracts.

### 15.1 Post-Phase-6 development order

```text
Phase 6R                              COMPLETE
↓
Phase 6UI-A                           IN PROGRESS
    UI-A0 playable gameplay boundary COMPLETE
    UI-A1 operable Battle HUD        COMPLETE
    UI-A2 basic committed presentation NEXT / DESIGN LOCKED
    UI-A3 deterministic immediate preview — dynamic-text slice validated; remaining target/energy preview planned
↓
Phase 7 Relics
↓
Phase 8 Pommel Strike+ / Sundial validation
↓
Phase 6UI-B advanced UX/tooling
↓
Presentation Polish
```

### 15.2 Gameplay Request / Read boundary

Normal UI never constructs/enqueues authoritative BattleActions directly. It uses formal Request APIs. Query is advisory; Request revalidates current authoritative state.

`AcceptedForResolution` means accepted into the Gameplay resolution system, not completed effects.

Internal Gameplay may synchronously settle and Seal a Presentation Resolution during `StartProcessing()`. Public notification remains delayed: neither `OnPresentationResolutionReady` nor `OnReadStateReady` may fire re-entrantly before an accepted public Request returns.

### 15.3 MVVM-style responsibility split

```text
MODEL
BattleManager / Combatants / DeckRuntime / Status runtime / Enemy Intent / BattleActionQueue

VIEWMODEL
frozen player-facing display state
formal Request forwarding
presentation-only selection/focus
latest-only live input bindings

VIEW
UMG Widgets
```

The ViewModel is not a second Gameplay authority.

### 15.4 Coherent raw read vs frozen display state

`FBattleReadSnapshot` is the current coherent Gameplay/read structure and may contain weak runtime references.

`FPresentationStateSnapshot` is the one frozen player-facing display model. It contains all display values needed to apply one exact revision without mutable Gameplay lookups.

The HUD ViewModel applies/copies frozen presentation state. Historical rendering never re-reads `UCardInstance`, `UStatusInstance`, `ACombatant` or current Battle state.

### 15.5 Authoritative Enemy Intent

Displayed committed Enemy Intent remains the authoritative source used to build the corresponding EnemyTurn Actions. CurrentResolvedDamageAmount is current-snapshot gameplay-derived data, not guaranteed future damage.

### 15.6 Gameplay and Presentation are separate timelines

Forbidden:

```text
BattleAction
↓
wait for animation
↓
next Gameplay Action
```

Required:

```text
Gameplay Commit
↓
Presentation Record
↓
Gameplay continues independently
```

Presentation may lock input for readability but never becomes authoritative gameplay state.

### 15.7 Presentation Records, internal Seal and deferred delivery

A Record is an immutable player-facing historical fact with deterministic `BattleId / ResolutionId / PresentationSequence` identity.

At the internal Gameplay-stable boundary, all valid Records for one Resolution and the exact frozen final display state are sealed together:

```text
FPresentationResolutionEnvelope
├── BattleId
├── ResolutionId
├── Origin
├── FinalStateRevision
├── Records[]
└── FinalSnapshot : FPresentationStateSnapshot
```

Seal releases the active builder immediately. Public delivery of that immutable Envelope happens later at the deferred UI/read notification boundary.

```text
internal Queue/macro stable
↓
Freeze + Seal + release builder
↓
next Resolution may Begin

originating public Request returns
↓
deferred OnPresentationResolutionReady / OnReadStateReady
```

Historical playback never queries Recorder/current Gameplay to reconstruct the past.

Envelope identity is `(BattleId, ResolutionId)`. Stable read-edge identity is `(BattleId, StateRevision)`; these de-duplication domains must remain separate.

### 15.8 Presentation catch-up and input release

Once UI-A2 exists, the display flow is:

```text
Player submits Request
↓
final validation / BeginResolution
↓
Gameplay may resolve synchronously
↓
internal stable boundary freezes + seals Envelope and releases builder
↓
public Request returns AcceptedForResolution
↓
UI enters Resolving / input locked
↓
deferred sealed-Envelope delivery
↓
Controller plays that Envelope's Records
↓
Finished / Skip / fail-safe
↓
Apply that Envelope.FinalSnapshot
↓
consume next queued Envelope if any
↓
when Controller catches up to newest sealed (BattleId, StateRevision)
↓
refresh only live Runtime input bindings for that matching newest revision
↓
if authoritative Gameplay is request-eligible, release normal input
```

Do **not** capture/rebuild a newer display snapshot after playback catches up. The display state comes from the sealed `Envelope.FinalSnapshot` already paired with the Records.

Skip / fast-forward / backlog collapse:

```text
collapse or consume obsolete presentation work
↓
Apply newest sealed Envelope.FinalSnapshot
↓
discard obsolete transitional display state
↓
if that is the newest current BattleId/Revision, refresh live input bindings only
↓
release input when Gameplay is request-eligible
```

Do not “refresh from latest Gameplay snapshot” for display during catch-up; that would bypass the frozen Envelope pairing.

If Presentation was disabled from Resolution start, no historical Envelope is required; use the final frozen baseline. If an active writer unexpectedly fails, invalidate the whole record batch and do not publish a partial Envelope.

### 15.9 Interaction policy

Phase 6UI-A keeps the explicit two-stage card interaction. Enemy-target and Self-target cards expose gameplay-provided public LegalTargets. Widget matching uses PresentationId only as a visual mapping key; formal Requests submit the current gameplay TargetId/object and revalidate.

### 15.10 Operable HUD / scalability

UI-A1 minimum surface remains Player/Enemy HP/Block/Status, Energy, Hand/piles, committed Intent, End Turn, card selection/cancel/target selection, Resolving lock and terminal/fault feedback.

PresentationId is not Gameplay identity. Multi-enemy future work must not rely on UObject name/address/actor discovery order.

### 15.11 UI-A implementation slices

```text
UI-A0 — COMPLETE
Playable Gameplay Boundary

UI-A1 — COMPLETE
Operable Battle HUD

UI-A2 — NEXT / DESIGN LOCKED
Basic Committed Presentation

UI-A3 — PARTIAL
Deterministic Immediate Preview
```

UI-A2 slice ownership is fixed as:

```text
UI-A2A
- generic Record/Envelope transport
- BattleId / ResolutionId / PresentationSequence
- Begin / Abort / internal Seal lifecycle
- seal-before-next-Begin invariant
- deferred non-reentrant public Envelope/Ready notification
- ResolutionFault Record lifecycle and append-last invariant
- frozen FPresentationStateSnapshot
- explicit optional battle-scoped RecordWriter propagation
- writer-append failure invalidates the whole current record batch
- separate Envelope vs read-state de-duplication identities
- Freeze/Seal failure policy
- PresentationUnavailable bootstrap state
- Presenter/Controller as sole HUD display sequencer
- bounded Envelope backlog
- PlaybackToken
- Skip / timeout / stale callback / missing callback / Widget loss fail-safe
- newest-only live input-binding refresh
- no real Damage animation required

UI-A2B
- Damage / Block CommitResults + Records
- fully blocked damage
- TurnStartClear Block history
- lethal Damage → Victory/Defeat Record ordering
- simple Damage/Block playback

UI-A2C
- CardPlayed
- exact EnergyBefore / EnergyAfter / CostPaid
- CardZoneChanged
- Draw / Hand discard / Discard / Exhaust / Removed
- Shuffle → reactions → RetryDraw ordering

UI-A2D
- Status create / merge / reduce / remove + reason
- formal Victory/Defeat visual treatment
- formal ResolutionFault visual treatment
- combined end-to-end acceptance
```

`ResolutionFault` lifecycle and generic playback safety are therefore A2A infrastructure. A2D does not introduce them; it adds formal visible treatment and comprehensive acceptance.

### 15.12 UI/MVVM architecture summary

```text
                     GAMEPLAY / MODEL
                           │
             ┌─────────────┴─────────────┐
             │                           │
      Gameplay Validator          Authoritative State
             │                           │
 Query ──────┤                           │
 Request ────┘                           │
      │                                  │
      ▼                                  │
Begin Presentation Resolution            │
      │                                  │
      ▼                                  │
BattleActionQueue                        │
      │                                  │
      ▼                                  │
Modifier / Action / Event / Trigger      │
      │                                  │
      ├──────────── Commit ──────────────┤
      │                                  │
      ▼                                  ▼
Presentation Records              Coherent Raw Read Snapshot
      │                                  │
      │                                  ▼
      │                         Freeze Presentation State
      │                                  │
      └──────────────┬───────────────────┘
                     ▼
        INTERNAL STABLE SEAL BOUNDARY
          Immutable Resolution Envelope
          Records[] + FinalSnapshot
                     │
             release active builder
                     │
       originating public Request returns
                     │
                     ▼
          DEFERRED PUBLIC DELIVERY
                     │
                     ▼
          Presentation Controller
                     │
                     ▼
           VIEWMODEL / FROZEN STATE
                     │
                     ▼
                    VIEW
                     │
                    UMG

Latest authoritative read boundary
        │
        └── only after Controller catches up to newest matching Revision
                ↓
          Live Input Bindings
          RuntimeId → CardInstance
          TargetId  → Combatant
                ↓
          Formal Request forwarding
```

Durable interpretation:

```text
Gameplay
= what is true

Frozen display state
= what the player-facing state was at one exact stable revision

Presentation
= how already-committed facts are shown

Live input bindings
= temporary newest-revision object mapping used only to submit current formal Requests
```

---

## 16. Locked Phase 6UI-A2 Architecture Summary

This section is the compact agent-facing contract. The detailed synchronized design is `docs/Phase6UIA2Implementation.md`.

### 16.1 Required closed loop

```text
Request / System operation
↓
final validation
↓
BeginResolution(Origin)
↓
prepare / enqueue
↓
BattleActionQueue
↓
Gameplay Commit
↓
typed CommitResult / MutationResult
↓
Action / BattleManager adds Source / Reason / Resolution context
↓
optional explicit battle-scoped RecordWriter appends Record
↓
Gameplay continues independently
↓
macro flow fully stabilizes
↓
INTERNAL STABLE BOUNDARY
Build exact raw read snapshot
↓
Freeze exact FPresentationStateSnapshot
↓
Seal immutable FPresentationResolutionEnvelope exactly once
↓
release active builder before any next BeginResolution
↓
PUBLIC DEFERRED BOUNDARY
originating accepted Request has returned
↓
OnPresentationResolutionReady(sealed Envelope)
↓
OnReadStateReady(BattleId, StateRevision)
↓
Presentation Coordinator / Controller
↓
bounded Envelope queue
↓
play Records in PresentationSequence order
↓
Finished / Skip / timeout / fail-safe
↓
Apply that Envelope.FinalSnapshot
↓
when caught up to newest BattleId/Revision, refresh live input bindings only
↓
unlock input if authoritative Gameplay is request-eligible
```

`OnPresentationResolutionReady` and `OnReadStateReady` are public deferred notifications. Seal is not deferred merely to satisfy their non-reentrancy rule.

### 16.2 Frozen state + latest-only input bindings

Historical display uses only `FPresentationStateSnapshot`. It contains no mutable Gameplay runtime dependency.

The separate live input cache maps RuntimeId/TargetId to weak runtime objects only for the newest caught-up revision. Historical Envelopes never use it.

### 16.3 Resolution lifecycle and fault exception

```text
ordinary validation reject
→ no Resolution

validation succeeds
→ Begin builder before fault-capable preparation/enqueue
→ success: normal processing
→ framework fault: preserve facts, append ResolutionFault last, freeze fault FinalSnapshot, Seal
→ truly side-effect-free non-framework failure: Abort builder
```

There may be only one active builder. It must be sealed/aborted at the internal stable boundary before another Resolution begins. A sealed Envelope pending deferred delivery is not an active builder.

BattleStart begins before fault-capable opening work. Initial Origins are `BattleStart`, `PlayCard`, `EndTurn`, `System`.

### 16.4 Immutable Envelope, Seal and de-duplication identities

A sealed Envelope owns `BattleId`, `ResolutionId`, `Origin`, `FinalStateRevision`, `Records[]` and `FinalSnapshot`.

```text
Envelope identity / Seal+Publish de-dup
= (BattleId, ResolutionId)

Read-state edge de-dup
= (BattleId, StateRevision)
```

One `(BattleId, ResolutionId)` seals/publishes at most one Envelope. A duplicate stable read callback must not re-Seal an Envelope, but `LastPublishedReadStateRevision` must not be used as the Envelope's sole identity.

Freeze/Seal failure:

```text
clear/discard current Presentation builder
↓
Presentation unavailable/disabled for this battle
↓
ordinary Gameplay state and deferred OnReadStateReady continue
↓
never Gameplay ResolutionFault
```

### 16.5 Explicit optional RecordWriter dependency and append failure

RecordWriter/Sink is an optional battle-scoped explicit dependency.

Forbidden:

```text
Action world-searches BattleManager/Recorder
GetAllActorsOfClass
UObject Outer cast to discover Recorder
global/singleton Recorder
Gameplay Runtime owner directly depends on Recorder
```

Nested/reaction Actions in one active Resolution receive the same writer through explicit action-building/dispatch context propagation.

If writer/Presentation recording is absent from the start, Gameplay runs in valid no-history mode and only the final frozen baseline is needed.

If an active writer Append fails:

```text
mark builder invalid
↓
discard all buffered unpublished Records for that Resolution
↓
do not Seal/Publish a partial historical Envelope
↓
Freeze final baseline at stability when possible
↓
PresentationUnavailable / fail-safe catch-up
```

Gameplay Commit, Action Finish and Queue ordering remain unchanged; no Gameplay fault is requested.

### 16.6 Typed mutation facts

```text
ACombatant / UDeckRuntime / UStatusContainer
→ authoritative mutation
→ typed CommitResult / MutationResult

Action / BattleManager
→ Source / Reason / writer context
→ Presentation Record
```

Damage/Block use Before/After commit results. Status Apply/Reduce/Remove use one mutation-result shape. Deck uses real zone-change facts.

`CardPlayed` must preserve exact `EnergyBefore`, `EnergyAfter` and `CostPaid`; playback must not inspect live Energy.

### 16.7 Specific ordering boundaries

```text
Damage commit → Damage Record
GainBlock commit → BlockChanged Record
Turn-start ClearBlock commit → BlockChanged(TurnStartClear)
Opening normalization ClearBlock → no visible Record

Hand → PlayArea + Energy spend commit
→ CardPlayed with Energy history
→ card-effect Records

Shuffle commit
→ Shuffle Record
→ FDeckShuffledEvent
→ reactions
→ RetryDraw
```

### 16.8 Unified PresentationId + visible bootstrap failure

One Battle-layer resolver produces resolved PresentationId for Snapshot, LegalTargets and Records. Validate resolved IDs as non-empty and battle-scoped unique; do not validate only raw authored fields.

Invalid presentation bootstrap enters UI-only `PresentationUnavailable`, not Gameplay `ResolutionFaulted`.

Selected UI-A2A policy:

```text
ViewModel initialization succeeds into PresentationUnavailable
→ Presenter still creates normal HUD Widget
→ input disabled
→ visible development-facing error panel/message
→ Recorder/Controller may remain disabled
→ headless Gameplay remains correct
```

### 16.9 Controller fail-safe playback

Controller owns bounded Envelope backlog. Each Envelope applies its own FinalSnapshot.

Generic PlaybackToken/Skip safety belongs to A2A:

```text
PlayPresentationRecord(Record, Token)
↓
NotifyPresentationFinished(Token)
```

Ignore duplicate/stale/old-Battle/post-Skip callbacks. Missing callback, timeout, Widget destruction or Presentation-disabled mode causes Presentation catch-up/fallback only, never Gameplay fault.

Backlog collapse applies the newest sealed Envelope.FinalSnapshot; it does not repull display state from Gameplay.

### 16.10 A2A Automation gate

Before UI-A2B visible Damage/Block playback begins, tests cover at least:

```text
AcceptedRequestEstablishesResolutionBeforeExecution
OrdinaryValidationRejectionCreatesNoResolution
PostValidationFrameworkFaultSealsFaultResolution
BattleStartBeginsBeforeFaultCapableOpeningWork
SystemResolutionCanBeCreated
EmptyRecordResolutionSealsSafely
FaultRetainsCommittedRecordsAndAppendsResolutionFaultLast
ResolutionSealsBeforeNextRequestCanBegin
PresentationEnvelopeNotificationDoesNotReenterAcceptedRequest
OneActiveResolutionSealsAtMostOnce
DuplicateStablePublishDoesNotDuplicateEnvelope
EnvelopeDedupUsesBattleIdAndResolutionId
FreezeFailureDisablesPresentationWithoutGameplayFault
SealFailureDoesNotLeakBuilderIntoNextResolution
AppendFailureDoesNotSealPartialEnvelope
BattleRestartDoesNotLeakBuilderOrRecords
LateSubscriberDoesNotReplayOldBattle
NoControllerOrPresentationDisabledLeavesGameplayUnchanged
HistoricalFrozenSnapshotAppliesWithoutMutableRuntimeReads
HistoricalEnvelopeCannotUseLiveInputBindings
InputBindingsRefreshOnlyAtNewestMatchingBattleRevision
OnReadStateReadyCannotBypassActivePresentationSequencing
RecordWriterIsOptionalAndExplicit
NestedReactionUsesSameActiveResolutionWriter
RecordAppendFailureDoesNotChangeGameplayFinishOrQueue
ControllerBacklogIsBounded
PlaybackTokenDuplicateAndStaleCompletionIgnored
SkipMissingCallbackTimeoutWidgetLossCatchUpWithoutGameplayFault
ResolvedPresentationIdSharedBySnapshotTargetsAndRecords
InvalidResolvedPresentationIdShowsPresentationUnavailable
PresentationUnavailableStillCreatesErrorCapableHUD
```

Only after this infrastructure gate is green should UI-A2B begin real Damage/Block presentation.