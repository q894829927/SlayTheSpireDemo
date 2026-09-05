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
- **Phase 7 Relics 7A–7F are complete, validated and sealed.** Current status authority is summarized in `docs/CODEX_GOAL_CHECKPOINT.md`; individual evidence remains in the dedicated Phase 7 implementation/validation documents.
- **Phase 8 Combo Architecture Validation is design-refined and DEFERRED. It is not a blocker for Card Expansion.** Authority: `docs/Phase8ComboArchitectureDesign.md`.
- **Card Upgrade STS-Style Refactor is COMPLETE / VALIDATED / SEALED.** Authority: `docs/CardUpgradeSTSStyleRefactor.md`. The former `FCardUpgradeConfig` foundation is historical and superseded.
- **Card Face Visual Style (CFV) is COMPLETE / USER-ACCEPTED / SEALED.** Authority: `docs/CardFaceVisualStyleImplementation.md`. The sealed model uses orthogonal CardType / CardRarity / CardColor / Upgrade State metadata, a narrow `UCardFaceStyleSet` Presentation configuration asset, Red-only production authoring for this slice, and incremental future color authoring for confirmed multi-class expansion.
- **Production Card Expansion is ACTIVE.** Wave 1A — Exhaust Fact Surface is DESIGN LOCKED / NEXT ACTIVE SLICE / IMPLEMENTATION NOT STARTED. Authority: `docs/CardExpansionWave1AExhaustFactSurface.md`. Phase 8 remains deferred and is not its prerequisite.

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

Status: **COMPLETE / PIE validated; draw orchestration amended by Phase 7C bulk-draw semantics**

Implemented Draw, Hand, Discard and Exhaust zones; deterministic Fisher–Yates shuffle with battle-scoped `FRandomStream`; stable runtime card identity; and queued draw/shuffle continuation.

Durable decisions after the Phase 7C amendment:

- DeckRuntime owns pile truth;
- DrawPile end is top;
- `UDrawCardsAction(N)` owns one bulk Draw-N request and `RemainingDraws`;
- `UDrawCardAction` is the atomic one-card DrawPile→Hand mutation only;
- bulk draw plans queued `DrawCardAction(s) → ShuffleDeckAction → DrawCardsAction(Remaining)` when the current DrawPile cannot satisfy the request;
- a fresh `Draw=0 / Discard=0` bulk request ends without a shuffle;
- a previously planned ShuffleAction may later commit with `MovedCardCount=0` after available DrawPile cards were consumed;
- draw never synchronously shuffles;
- initial battle RNG is consumed across deterministic non-empty shuffles.

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

Status: **COMPLETE for defined Phase 6 scope; DeckShuffled producer semantics amended by Phase 7C bulk draw**

Slices:

- 6A TurnEnd Trigger vertical slice — complete.
- 6B battle turn wiring — complete.
- 6C DeckShuffled Event — complete.
- 6R regression gate and Editor-only test-module extraction — complete.

Durable decisions:

- Events are committed facts; Triggers are read-only Action builders;
- trigger eligibility is snapshot-based and Actions validate live state;
- trigger order is `Priority → RuntimeSequence → LocalTriggerIndex` for the sealed Status-era ordering domain;
- Phase 7B extended Status/Relic sources while preserving the same relative RuntimeSequence ordering;
- reaction batches insert atomically with nested depth-first semantics;
- queue faults enter only at safe points;
- QueueEmpty is non-reentrant;
- player/enemy TurnEnded timing and hand cleanup are Gameplay semantics;
- DeckShuffled emits after a committed gameplay ShuffleAction and before the remaining bulk-draw continuation;
- a legitimately pre-planned zero-card ShuffleAction may emit DeckShuffled with `MovedCardCount=0`;
- a fresh exhausted bulk draw does not create a shuffle;
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

Status: **7A–7F COMPLETE / VALIDATED / SEALED**

Current summary authority: `docs/CODEX_GOAL_CHECKPOINT.md`.

The Phase 7 design and all implemented slices are sealed. Relics remain their own immutable definition + mutable runtime-instance model and are not disguised as Statuses.

### 7A — Relic Runtime

`URelicData`, `URelicInstance`, `URelicContainer`, explicit `ABattleManager` ownership/setup, ordered configured starting Relics and deterministic runtime sequence lifecycle were established and validated.

### 7B — Status + Relic Trigger Sources

`FTriggerRuntimeSource` and the source-neutral Trigger boundary allow Status and Relic trigger definitions to coexist. Existing Status/Relic deterministic ordering remains based on `Priority → RuntimeSequence → LocalTriggerIndex`; no persistent Trigger Registry exists.

### 7C — Sundial + GainEnergyAction

Positive Energy mutation, `UGainEnergyAction`, runtime Relic counter, Sundial trigger/counter behavior and corrected bulk Draw-N semantics were validated. `UDrawCardEffect(DrawCount=N)` builds one `UDrawCardsAction(N)`, while `UDrawCardAction` remains the atomic one-card mutation.

The sealed bulk-draw ordering pattern is:

```text
DrawCardsAction
→ queues Draw / Shuffle / RemainingDraw continuation batch at Queue front

ShuffleDeckAction
→ commit Shuffle
→ Dispatch DeckShuffled

Dispatcher reactions
→ insert ahead of RemainingDraw
```

Therefore:

```text
Shuffle
→ reactions
→ remaining draw continuation
```

This ordering is a durable precedent for later typed resolution-local authored continuations.

### 7D — Relic Read / Frozen / Native UI

Relic read/frozen presentation and Native HUD integration are complete, validated and sealed. Presentation remains read-only with respect to Gameplay authority.

### 7E — Relic Reaction Composition

Generic Relic Effect composition, fail-closed reaction construction, dependent Action insertion, presentation writer propagation and live membership validation are complete, validated and sealed.

### 7F — Relic Counter Metadata Unification

Relic count threshold metadata uses the CountTrigger as the authored threshold authority; production Sundial counter metadata/assets were migrated and validated. 7F is complete, validated and sealed.

Initial battle setup shuffle remains excluded from `DeckShuffled` Gameplay events. A3 does not predict Relic reactions.

## Phase 8 — Combo Architecture Validation

Status: **DESIGN REFINED / DEFERRED / NOT A BLOCKER FOR CARD EXPANSION**

Authority: `docs/Phase8ComboArchitectureDesign.md`.

Phase 8 design and existing Production PIE evidence are retained, but implementation is postponed.

When resumed, Automation uses an authored transient Draw-2 card definition rather than depending on production Pommel Strike content:

```text
Transient UCardData
→ generic Effects including UDrawCardEffect(DrawCount=2)
→ real PlayCard / Action path
→ real Shuffle
→ DeckShuffled Event
→ Sundial reaction
→ remaining draw continuation
```

The already-observed production Pommel Strike Draw-2 scenario remains separate sticky PIE evidence.

Phase 8 may be resumed later as an integration gate after card architecture has expanded.

## Card Expansion / Upgrade Foundation

Status: **UPGRADE REFACTOR COMPLETE / VALIDATED / SEALED; PRODUCTION CARD EXPANSION ACTIVE — WAVE 1A DESIGN LOCKED / IMPLEMENTATION NOT STARTED**

Upgrade authority: `docs/CardUpgradeSTSStyleRefactor.md`. `docs/CardUpgradeFoundationDesign.md` and the former `FCardUpgradeConfig` implementation are historical context, not current implementation instructions.

Card-expansion authority chain:

- `docs/IroncladCardArchitecturePlan.md`
- `docs/IroncladCardArchitecturePlanWave1Amendment.md`
- `docs/CardExpansionWave1AExhaustFactSurface.md`

The sealed ordinary-card model is one immutable `UCardData`, one `Effects[]` composition, typed Base/Upgraded values and the sole runtime `bUpgraded` bit. Upgrade names/colors remain presentation formatting of frozen state. Do not reopen this model for Card Expansion or restore the former upgrade configuration fields. Repeatable upgrade remains outside this sealed ordinary-card scope.

Phase 8 is not a prerequisite for Card Expansion. Production Card Expansion is now active at Wave 1A. Wave 1B/1C/1D, Card Trigger Source Expansion, multi-enemy work and Phase 8 remain outside the current slice unless separately authorized.

## Card Face Visual Style

Status: **COMPLETE / USER-ACCEPTED / SEALED**

Authority: `docs/CardFaceVisualStyleImplementation.md`.

Execution / acceptance evidence:

- `docs/CFV1Validation.md`
- `docs/CFV2CardFaceShellExecution.md`
- `docs/CFV3StyleSetResolverExecution.md`
- `docs/CFV4ProductionStyleSetExecution.md`
- `docs/CFV5VisualAcceptance.md`

The sealed CFV model keeps `CardType`, `CardRarity`, `CardColor` and Upgrade State orthogonal. `CardColor` is semantic card metadata rather than character identity; Rarity remains shared across colors/classes; `CardType` derives a Presentation-only Attack/Skill/Power visual shape. The Native card consumes frozen metadata through the pure resolver and narrow `UCardFaceStyleSet` Presentation configuration asset.

Current Red production authoring is accepted. Future card content must consume this sealed metadata / resolver / StyleSet / Widget contract. Additional CardColor assets may be authored incrementally when real multi-class content requires them; normal card expansion does not reopen CFV architecture or rerun sealed CFV gates unless those contracts actually change.

## Card Trigger Source Expansion

Status: **DESIGN DRAFT / FUTURE INDEPENDENT FOUNDATION SLICE / IMPLEMENTATION NOT AUTHORIZED**

Authority: `docs/CardTriggerSourceExpansionDesign.md`.

This slice must be implemented independently before Sentinel/Card-trigger consumers. It adds a typed Card trigger-source provider and a deterministic comparison key while preserving sealed Status/Relic ordering. The former Phase-8 prerequisite has been superseded by the current Card Expansion ordering amendment; removing that prerequisite does not authorize this slice.

## Phase 6UI-B — Advanced UX / Tooling

Status: **PLANNED LATER / CARD FOUNDATION AS APPLICABLE**

Advanced preview, Keyword/CardText presentation, developer overlay, presentation timeline tooling, controller/accessibility work and responsive layout belong here unless required earlier for basic playability or diagnosis.

## Presentation Polish

Status: **PLANNED LAST**

Drag/drop, fast-play shortcuts, final hand layout, target arrows, animation refinement, VFX/SFX and speed/skip polish remain non-authoritative Presentation work.