# Development Phases

This document records project progress, implementation history and durable phase decisions. Current implementation instructions are in the relevant phase documents and directory-level `AGENTS.md` files.

## Current State

- UE5.8 C++ project and runtime module exist.
- Phases 1–6C and the Phase 6R test-module extraction are complete.
- UI-A0 and UI-A1 are complete.
- UI-A2A/A2B/A2C/A2D C++ committed-presentation work is sealed.
- **UI-A2E Unified Blueprint/UMG Playback & PIE Acceptance is complete, validated and sealed.**
- **UI-A2 Basic Committed Presentation is complete, validated and sealed.**
- **UI-A3 Deterministic Immediate Preview is complete, validated and sealed.** Final status authority: `docs/Phase6UIA3Seal.md`.
- **Phase 6UI-A Playable Battle UI is complete, validated and sealed.**
- **Phase 7 Relics is in progress.** The design is sealed; 7A Relic Runtime is implemented and awaiting its focused Build/Automation gate. Active authority: `docs/Phase7RelicsImplementation.md`.

## Phase 1 — Minimal Combat Loop

Status: **COMPLETE / PIE validated**

Implemented HP, Block, Energy, turn flow, enemy attacks, victory/defeat and command rejection after battle end.

## Phase 2 — BattleActionQueue

Status: **COMPLETE / PIE validated**

Implemented queued Damage/Block, explicit `Finish()`, deterministic front/back ordering, one final QueueEmpty and enemy-turn progression only after the queue drains.

Durable decisions:

- one authoritative action executes at a time;
- actions may schedule dependencies but never advance the queue;
- dependent batches required for one logical chain are inserted before the current action finishes.

## Phase 3 — Deck System

Status: **COMPLETE / PIE validated**

Implemented Draw, Hand, Discard and Exhaust zones; deterministic Fisher–Yates shuffle with battle-scoped `FRandomStream`; stable runtime card identity; and queued `Shuffle → RetryDraw`.

Durable decisions:

- DeckRuntime owns pile truth;
- DrawPile end is top;
- one DrawAction is one draw attempt;
- draw never synchronously shuffles;
- initial battle RNG is consumed across deterministic shuffles.

## Phase 4 — Data-Driven Cards

Status: **COMPLETE / PIE validated**

Implemented `UCardData`, `UCardInstance`, reusable effects, PlayArea lifecycle, Energy spending/rejection and the Pommel Strike `PlayCard → Damage → Draw → FinishCardPlay` chain.

Durable decisions:

- definition subobjects are immutable;
- effects capture intent, not future resolved values;
- runtime card object is Gameplay identity and RuntimeId is stable presentation/debug identity;
- cleanup resolves destination at Execute-time;
- invalid actions fail soft and finish.

## Phase 5 — Modifier Framework and Status System

Status: **COMPLETE**

Slices:

- 5A Status Runtime + ApplyStatusAction — complete.
- 5B1 Damage Spec + flat add + Strength — complete.
- 5B2 Damage Ratio + Weak + Vulnerable — complete.
- 5C Block Spec + Dexterity + Frailty — complete.
- 5R Automation regression gate — complete.

Durable decisions:

- StatusData is immutable; StatusInstance owns runtime Amount/RuntimeSequence/Owner;
- reapplication preserves sequence; exact-instance lifecycle reduction uses ReduceStatusAction;
- Damage and Block use typed Execute-time specs/pipelines;
- modifier order is `Phase → Priority → RuntimeSequence → LocalModifierIndex`;
- each integer ratio modifier floors before the next modifier.

## Phase 6 — Battle Events and Triggers

Status: **COMPLETE for defined Phase 6 scope**

Slices:

- 6A TurnEnd Trigger vertical slice — complete.
- 6B battle turn wiring — complete.
- 6C DeckShuffled Event — complete.
- 6R regression gate and Editor-only test-module extraction — complete.

Durable decisions:

- Events are committed facts; Triggers are read-only Action builders;
- trigger eligibility is snapshot-based and Actions validate live state;
- trigger order is `Priority → RuntimeSequence → LocalTriggerIndex`;
- reaction batches insert atomically with nested depth-first semantics;
- queue faults enter only at safe points;
- QueueEmpty is non-reentrant;
- player/enemy TurnEnded timing and hand cleanup are Gameplay semantics;
- DeckShuffled emits after successful commit and before RetryDraw;
- initial setup shuffle is not a DeckShuffled Gameplay event;
- Automation-only sources live in the Editor-only test module.

## Phase 6UI-A — Playable Battle UI

Status: **COMPLETE / VALIDATED / SEALED**

### UI-A0 — Playable Gameplay Boundary

Status: **COMPLETE**

Implemented authoritative turn/Hand lifecycle, formal Query/Request APIs, coherent `(BattleId, StateRevision)` snapshots, non-reentrant Ready publication, committed Enemy Intent and public legal-target selection.

### UI-A1 — Operable Battle HUD

Status: **COMPLETE / manual PIE validated**

Implemented the concrete HUD and formal Enemy/Self-target interaction. Defend resolves through selection of the highlighted Player presentation.

### UI-A2 — Basic Committed Presentation

Status: **COMPLETE / VALIDATED / SEALED**

- A2A committed-presentation infrastructure — C++ validated.
- A2B Damage + Block — C++ validated.
- A2C Card + Energy + Zone + Shuffle — C++ validated.
- A2D Status + Terminal — C++/Automation sealed.
- A2E unified Blueprint/UMG playback and PIE — **COMPLETE / VALIDATED / SEALED**.
- A2N Native HUD ownership migration — **R0-R13 COMPLETE / VALIDATED; R14-A COMPLETE / VALIDATED; R14-B NOT REQUIRED / NOT AUTHORIZED**. R12 cut production `L_BattleTest` over to `WBP_BattleHUD_Native` in isolated commit `de788c5`, then passed cutover-head WBP, A2D5 6/6, Phase6R 100/100, clean Shipping and production-map manual PIE Gates. Native HUD is the production default; Legacy HUD/Card/Status assets remain retained. The deprecated Legacy assets were later relocated, without deletion or runtime reactivation, to `/Game/SlayTheSpireDemo/UI/Out/Legacy/`. R13-M1 completed the post-cutover Native-only dependency stabilization change `fe7fe4e`, retained zero production Legacy HUD/Card/Status dependencies, and passed its formal stabilization gates. R14-A then removed confirmed-unreferenced Native C++ helpers and zero-reference Blueprint migration residue; `WBP_BattleCard_Native`, `WBP_BattleStatus_Native`, and `WBP_BattleHUD_Native` passed compile/save/reopen, focused R4/R9 and R13 asset reference Automation, Editor Build, and production-map PIE smoke. Commit `8a609659ba138c922fe64bbfd08bca44b05ca8d6` is the Native Blueprint residue cleanup. `L_BattleTest_Native` is intentionally retained as a non-production migration/regression map. R14-B remains a separately authorized destructive Legacy removal boundary. See `docs/R13NativeHUDStabilization.md` and `docs/R14ASafeCleanupValidation.md`.

The sealed C++ path includes immutable Records/Envelopes, exact frozen snapshots, explicit optional RecordWriter propagation, bounded FIFO delivery/backlog, PlaybackToken fail-safety, exact Status identity and formal terminal/fault history.

Read:

- `docs/Phase6UIA2Implementation.md`
- `docs/Phase6UIA2DImplementation.md`
- `docs/Phase6UIA2D5SourceReview.md`
- `docs/Phase6UIA2EImplementation.md`
- `docs/UIA2ERemainingSteps.zh-CN.md`
- `docs/Phase6UIA2NNativeHUDRefactor.md`

### UI-A3 — Deterministic Immediate Preview

Status: **COMPLETE / VALIDATED / SEALED**

Final status authority: `docs/Phase6UIA3Seal.md`.

- A3-1 Dynamic Text — **COMPLETE / VALIDATED / SEALED**.
- A3-2 Target-Specific Current-State Preview — **COMPLETE / VALIDATED / SEALED**.
- A3-3 Energy + Target-Aware Legality — **COMPLETE / VALIDATED / SEALED**.
- A3-4 ViewModel Transient Preview Lifecycle — **COMPLETE / REVALIDATED / SEALED**.
- A3-5 Native card-face Preview + combined A2/A3 PIE — **COMPLETE / VALIDATED / SEALED**.
- A3-5 RichText per-value comparison styling — **COMPLETE / VALIDATED / SEALED**.

Locked boundary:

```text
A3 = pre-commit read-only current-state supported Operation values
A2 = post-commit playback of immutable committed facts
```

First-version target-specific Preview covers current Damage, Self Block, Energy and legality without predicting final HP, Trigger/Relic reactions, draw/shuffle outcomes or terminal state.

Final acceptance includes restored production `CardPlayed` animation, Preview-only notification ownership, target-specific Native card-face Preview, and RichText comparison styling that colors only the affected numeric semantic value. Strength-modified Damage was manually confirmed in PIE; Dexterity/Frailty Block RichText is covered by passing focused Automation because no playable Dexterity-granting card currently exists.

Read:

- `docs/Phase6UIA3Implementation.md` — historical implementation plan and durable A3 contracts
- `docs/Phase6UIA3CardFacePreviewAmendment.md` — final A3-5 UX/ownership amendment
- `docs/Phase6UIA3Seal.md` — final acceptance/status authority

## Phase 7 — Relics

Status: **IN PROGRESS — 7A IMPLEMENTED / VALIDATION PENDING**

Active implementation authority: `docs/Phase7RelicsImplementation.md`.

The Phase 7 design is sealed. First validation remains Sundial; Abacus is optional. Phase 7 introduces the first non-Status Trigger source and therefore extracts only the smallest source-neutral boundary needed for Status and Relic triggers to coexist. Relics remain their own immutable definition + mutable runtime-instance model and must never be disguised as Statuses.

Current 7A implementation contains `URelicData`, `URelicInstance`, `URelicContainer`, explicit `ABattleManager` ownership/setup, ordered configured starting Relics and focused RuntimeSequence lifecycle tests. Starting Relics are instantiated during `StartBattle()` after the battle RuntimeSequence allocator reset; the Relic getter is not a lazy initialization path. 7B Dispatcher work, Sundial, positive Energy gain and Relic UI have not started.

The first full gameplay vertical slice remains Sundial driven by the already-committed `FDeckShuffledEvent`. Initial battle setup shuffle remains excluded. A3 does not expand to predict Relic reactions in Phase 7.

## Phase 8 — Combo Architecture Validation

Status: **PLANNED AFTER PHASE 7**

Validate two upgraded Pommel Strikes plus Sundial without special-case combination code. The interaction must emerge from generic card, draw, shuffle, event, modifier and action rules and be visually understandable through the playable UI.

## Phase 6UI-B — Advanced UX / Tooling

Status: **PLANNED AFTER PHASE 8**

Advanced preview, Keyword/CardText presentation, developer overlay, presentation timeline tooling, controller/accessibility work and responsive layout belong here unless required earlier for basic playability or diagnosis.

## Presentation Polish

Status: **PLANNED LAST**

Drag/drop, fast-play shortcuts, final hand layout, target arrows, animation refinement, VFX/SFX and speed/skip polish remain non-authoritative Presentation work.
