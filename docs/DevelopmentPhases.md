# Development Phases

2026-10-07 G9-B 收口：用户确认剩余三组组合验收全部完成，修订后的 G9-B
人工门槛均为 USER_REPORTED_PASS；既有构建／自动化证据有效。当前准备
独立默认启用批次，不再重复要求 B 人工验收。G9-C–F 未实施，G9 整体
未封板；[收口与默认启用记录](G9BClosure.md) 覆盖下文旧待验状态。

2026-10-07 验收更新：用户确认 `e62cdbd` 本批持牌拖放人工验收通过，
中文 A–E 项记为 USER_REPORTED_PASS；该扩展的人工待办关闭。构建及
55 项自动化证据复用；未重新执行 UE。原 G9 队尾、防重复结束和含旧结束
意图的强制选择清空时间线仍按对应清单记录，G9 保持 OPT-IN／NOT SEALED，
C++ 默认 false，不进入 C–F。详情见 [拖放验收结果](NativeCardDragRelease.md)。

Native 持牌拖动／区域释放（2026-10-07，基线 `7c73884`）：技能／能力
拖出当前 Hand 区域松开确认一次，区内松开继续跟随；单体攻击只抬升／
瞄准。最终规定生成与 Editor 构建通过（6.42 秒），一次聚焦 55 项通过（含
1 预期 R8 警告），实际视觉范围见 [本批证据](NativeCardDragRelease.md)。
默认仍关闭，不进入 C–F 或封板，下述绝不释放确认规则为历史。

真实鼠标复测暴露叶按钮捕获、重复捕获自清理、首帧空白命中缺口，最终
由正式指针卡牌转发原生按下回执、HUD 立即捕获且移动不重捕获。共享路由
和精确解绑已经实现；实际最终视觉范围及失败历史见本批文档。
最终生产 Native 窄场景通过：战吼区外释放、燃烧区内保留／右键归位及
区外出牌、打击仅瞄准后正常目标点击；完整拖放排队和视口变化等
在实现提交时为 USER ACTION REQUIRED，现已获用户人工确认通过。当时
代理运行实例 G9=true，用户本次未补运行配置；原 G9 其他门槛继续单独记录。

Native 点击／长按统一选牌（基线 `9c78655`）：按下时选中，松开不重复选择
或确认；规定生成／Editor 构建通过（17.97 秒），一次聚焦 50 项通过（含
1 预期 R8 警告）。当前普通指针与 G9 协议不变，实际长按视觉范围按
[本批证据](NativeCardPressSelection.md) 记录，不启用默认、不进入 C–F 或封板。

本批 Native 短按、按住移动后松开、取消和新的点击确认窄场景实际通过；
静止长按时长及忙碌／强制选择完整轨迹仍待复验，不扩大视觉通过声明。

第二回合连续出牌缺陷（基线 `6b0ab9c`）已修正：旧结束凭据在下一回合
精确追平／就绪时正式退役，后续播放不能重新激活。旧实现新增回归先复现
失败，修复后规定生成／Editor 构建通过、一次聚焦 54 项通过（含 1 预期
R8 警告）。第二／第三回合及无历史模式的连续出牌、队尾和防重均有自动化
证据，完整视觉时间线待本次实际记录或用户反馈。阶段仍为 OPT-IN／PARTIAL
PIE／USER ACTION REQUIRED／NOT SEALED；见 [修复证据](G9NextTurnInputFenceFix.md)。

本批生产 Native 第二／第三回合普通连续出牌实际通过，VM 恢复 Idle、输入
解锁且反馈为空；完整播放期间多牌排队／队尾录像仍需人工复验，不以普通
连续点击替代，不改变默认启用或阶段封板条件。

最新用户澄清（基线 `925aff2`）：攻击瞄准同样抑制其他手牌突出；结束回合
保留按钮之前的确认队列并排在其后，只阻止之后追加，继续防重复结束。
当前约定与最新中文复验步骤见 [输入修订](NativeInputHoverAndEndTurnRevision.md)。
下述取消队列的批次记录为历史，不再代表当前行为。

最新澄清已实现：规定工程生成与 Editor 构建通过，最终 5.85 秒；53 个
不同受影响测试均有有效通过证据，其中最终悬停命中调整的 8 项重跑通过。
实际 Native 打击瞄准抑制邻牌突出和取消恢复悬停通过；队列后结束回合、
重复点击完整阶段矩阵、强制选择清空完整轨迹仍待人工验收。默认关闭、
PARTIAL PIE／USER ACTION REQUIRED／NOT SEALED 状态不变。

以下为上一批 `925aff2` 的记录，其取消队列约定已被上述澄清取代：

2026-10-06 用户反馈上一清单 1／3／4／6／7 无问题，2／5 原标准通过。
当前新增范围为持牌时抑制其他悬停、结束回合取消未执行队列，以及防止
旧历史期间的重复点击连续结束未来回合。原人工反馈记录为用户反馈通过；
新增行为独立验收，不启用默认，也不进入 C–F。当前约定与中文清单：
[持牌悬停与结束回合修订](NativeInputHoverAndEndTurnRevision.md)。

本轮修订已实现：工程生成／Editor 构建通过，77 个不同的受影响测试在
初次与单项修复重跑后取得有效通过证据。Native 快速三次结束回合点击
仅推进一次的窄场景通过；持牌悬停、队列取消和完整阶段矩阵仍需新增人工
验收。G9 默认与 C–F／封板状态不变。

Native ordinary-card source/pointer extension (base `de75414`): **IMPLEMENTED /
BUILD PASS / AFFECTED AUTOMATION PASS / PARTIAL PIE / USER ACTION REQUIRED**.
CardPlayed prepares its actual first pose; Skill/Power/untargeted-Attack drafts
follow the mouse, right click restores the fan, and left click forwards Self/None
through existing Requests. Optional FIFO cosmetics preserve confirmed pointer
origins. All 69 distinct selected/added tests have valid passing evidence across
repaired runs. 原清单已获得用户人工通过反馈，新增规则的验收仍待完成。No default
activation or C–F/seal change. [Contract and evidence](NativePointerCardPresentation.md).

G9-B revision batches 2–4: Status front / Relic tail, confirmed input FIFO and
explicit simultaneous TurnEndDiscard are implemented; UE 5.8 Editor builds and
affected Automation gates PASS. Group evidence covers 94 distinct cases across
initial/repaired runs. Native PIE passed FIFO, EndTurn tail, simultaneous discard
and deferred relic reward; queued-mandatory clearing remains USER ACTION REQUIRED.
Native G9 is
OPT-IN / NOT SEALED; original C–F have not started. See
[actual evidence](G9BRevisionExecution.md).

Current forward G9-B scope is the user-approved [confirmed FIFO / relic tails /
turn-end discard amendment](QueuedCardPlayAndRelicTimingAmendment.md), based on
`fe80565`. One visual time line remains pending; the selection-only
contract below is historical. Default off / NOT SEALED; C–F are not started.

Native Hand architecture correction: **IMPLEMENTED / BUILD PASS / AFFECTED AUTOMATION PASS / FOCUSED PIE PASS**. Baseline is `5f7f4fc`, structure ownership `4237526`; the layout batch adds dedicated Hand Slot/Slate arrangement, exact-token geometry protection and tracked-receipt cancellation cleanup. Latest 55-case affected coverage passed across 53 initial successes (one expected warning) and two repaired R5 fixture recoveries. Earlier unaffected panel/frozen-face evidence is reused. Native MCP PIE covered play/draw, Selection handoff, viewport change, hover, playback stop/restart and same-HUD mid-draw/receipt-window Skip with fresh-card interaction. See [refactor evidence](NativeHandStructureRefactor.md). G9-B itself remains default off / PARTIAL PIE / NOT SEALED; its original enabled-input gates still require user action, and C–F remain gated.

This document records project progress, implementation history and durable phase decisions. Current implementation instructions are in the relevant phase documents and directory-level `AGENTS.md` files.

## Current State

- **Amended G9-B — IMPLEMENTED / AUTOMATED GATES PASS / PARTIAL PIE / USER ACTION REQUIRED / OPT-IN (2026-10-06).** Relic tail scheduling, confirmed-card FIFO and explicit simultaneous turn-end discard are committed in `6071c56`, `b105fb1`, `72a564a`. Builds and affected Automation pass. Production Native PIE proves automatic FIFO, queued EndTurn ordering, simultaneous discard and draw/shuffle before relic reward. Mandatory EndTurn isolation is observed; the full queued-mandatory clearing time line remains pending. C++ default stays false; the preserved user asset was tested with G9=true. Original C–F have not started. Authority: [revision execution](G9BRevisionExecution.md), [Native PIE receipt](G9BRevisionNativePIE.md). The earlier selection-only B implementation and repaired Hand regression are historical evidence in [original B execution](SelectionPresentationG9BExecution.md).

- **G9-A — COMPLETE / VALIDATED AT ITS SHADOW CHECKPOINT (2026-10-05).** Gameplay player-turn identity, separate EndTurn acceptance/execution evaluation, exact sealed card-target credentials and one HUD-owned buffered-input arbiter passed the Development Editor build and G9-A 6/6, affected G8-B 9/9 and FastInput 2/2 Automation. Authority and historical evidence: [G9-A execution](SelectionPresentationG9AExecution.md). Production activation now follows the separate G9-B gate; G9 as a whole is not sealed.

- **Ruined Citadel battle level — implemented, focused MCP PIE PASS (2026-09-10).**
  The user-supplied image is the battle backdrop in the new `L_Battle_RuinedCitadel`,
  now selected for both editor startup and game default. Native battle assembly is
  retained; template-only lights, fog, floor, PlayerStart and unused scene helpers
  were removed, with the required Native HUD input path preserved. See [level
  configuration and evidence](RuinedCitadelBattleLevel.md).

- **Awakened One monster animation — implemented, build PASS, focused Idle PIE PASS (2026-09-10).**
  The Native combatant Presentation widget now uses the authored Awakened One `Idle_2`, `Hit`
  and `Attack_1` sequences as imported Unreal textures. Enemy Attack playback is requested at
  the committed damage boundary when the enemy damages the player; the profile is Blueprint-
  tunable and remains Presentation-only. The focused visual check uses
  `L_Battle_RuinedCitadel`. See [Awakened One monster animation](AwakenedOneCharacterAnimation.md).

- **Enemy presentation/intent cluster layout — implemented, MCP PIE PASS (2026-09-11).**
  `WBP_BattleHUD_Native` now keeps the enemy presentation and intent in one Overlay; only the
  enemy instance is enlarged around its bottom-center pivot, leaving the player size unchanged.
  The unused legacy character brush widgets were removed from the Native Designer tree so the
  editor preview and PIE each render one image per combatant.
  See the layout amendment in [Awakened One monster animation](AwakenedOneCharacterAnimation.md).

- **Ironclad character animation — implemented, build PASS, Native PIE pending (2026-09-10).**
  The Native combatant Presentation widget now plays baked Idle/Hit frames from
  `UI/images/characters/ironclad`, plus Presentation-only attack lunge, Victory pulse and
  corpse-based Defeat. Timing, lunge, opacity and optional Blueprint frame overrides are exposed
  on the existing combatant Widget Blueprint. See [Ironclad character animation](IroncladCharacterAnimation.md)
  and [Validation](Validation.md). No Gameplay, record schema, Legacy UI or Spine runtime change.

- **Fan Hand / Attack Targeting — implemented, automated gates PASS, Native PIE pending (2026-09-10).** User-requested fan layout, raised/enlarged hover cards, gray/red single-enemy Attack arrow, mouse-right cancellation for ordinary selection and predicted Draw→Hand fan targets. Hand layout tuning is exposed to the Native HUD Blueprint. Imported the two requested local targeting textures. Scope, exact validation evidence and visual checklist: [Hand interaction](HandFanTargetingInteraction.md). Gameplay and G8 scheduling are unchanged.

- **G8 — COMPLETE / VALIDATED / SEALED (2026-09-12).** [G8-F seal](SelectionPresentationG8FSeal.md) is the final authority. DamageNumber-only detached cosmetics do not own readiness or trigger FastInput Skip. Played-card Presentation remains Blocking until the separately staged G9 work changes it.

- **Selection Presentation G0–G5 — COMPLETE / VALIDATED / SEALED (2026-09-09).** G0–G3 established the ownership/correlation/Controller protocol foundation. G4 introduced the generic SingleRecord transition engine; its isolated compatibility path failed visual PIE and remains historical. The coherent G4+G5 production migration activated persistent SelectionArea ownership and exact same-object transition consumption, passed automated gates, and passed the user-confirmed Native `L_BattleTest` PIE gate with no flashback, duplicate, ghost, clipping or stuck input. Authorities: `docs/SelectionPresentationG4Execution.md`, `docs/SelectionPresentationG5Execution.md`, `docs/Validation.md`.

- **Selection Presentation G6/G7 — COMPLETE / VALIDATED / SEALED.** Safe N-child Group playback preserves serial Gameplay and chronological reducer order. G7 cleanup is complete; do not restart G6/G7 from historical status text. Authorities: [G6 execution](SelectionPresentationG6Execution.md), [G7 seal amendment](SelectionPresentationG7SealAmendment.md).

- **Selection Presentation production repair — historical / superseded by sealed G0–G5 architecture.** The earlier shared-selection subclass, explicit confirmation and compatibility visual handoff work remains useful implementation history, but its standalone manual gate is no longer the current forward authority. Current production behavior is governed by the sealed Selection Presentation lifecycle and `docs/CardSelectionPresentationConstraints.md`.

- **Card Selection Refactor — implemented foundation retained.** Execute-time current-Hand capture, shared interactive boundary and explicit failure disposition remain the Gameplay-side selection authority. Selection Presentation G0–G8 is the sealed baseline; G9 follows its dedicated staged input/played-card contracts. Do not reopen the refactor solely because older checkpoint text described standalone manual acceptance as pending.

- **Automatic Card Descriptions — implemented / automated gates PASS / manual PIE pending (2026-09-08).** User-requested refactor; Effect-ordered localized Chinese descriptions are the default, with automatic Exhaust and per-effect preview arguments. Editor build and 18 focused tests passed; after Chinese status-name asset edits, the affected existing-assets test alone passed again. Scope and acceptance: `docs/AutomaticCardDescriptions.md`.

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
- **Production Card Expansion is ACTIVE.** Wave 1A, Wave 1B, Wave 1C-A and Wave 1C-B are COMPLETE / VALIDATED / SEALED. Wave 1C-C0 — Select-Exhaust Generalization is COMPLETE / VALIDATED / SEALED (`docs/CardExpansionWave1CC0Execution.md`). Wave 1C-C1 Hand→DrawPileTop / Warcry capability work was implemented and merged to `main` by PR #18 (`ffbc164905a875bea5c9ab3dfe0a07df5068b8cc`); existing records do not establish a final standalone C1 seal, so do not label C1 SEALED without explicit final acceptance evidence. The former True Grit C1 plan is superseded and inactive.

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
- `docs/UIA2ERemainingSteps.zh-CN.md` — historical A2E execution record; not a current pending-work list
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

Status: **UPGRADE REFACTOR COMPLETE / VALIDATED / SEALED; PRODUCTION CARD EXPANSION ACTIVE — WAVE 1A/1B/1C-A/1C-B/C0 SEALED; C1 IMPLEMENTED + MERGED TO MAIN / FINAL STANDALONE SEAL NOT RECORDED**

Upgrade authority: `docs/CardUpgradeSTSStyleRefactor.md`. `docs/CardUpgradeFoundationDesign.md` and the former `FCardUpgradeConfig` implementation are historical context, not current implementation instructions.

Card-expansion authority chain:

- `docs/IroncladCardArchitecturePlan.md`
- `docs/IroncladCardArchitecturePlanWave1Amendment.md`
- `docs/CardExpansionWave1AExhaustFactSurface.md` — Wave 1A (sealed)
- `docs/CardExpansionWave1BTargetedExhaustPrimitive.md` — Wave 1B (sealed)
- `docs/CardExpansionWave1CSelectionPrimitive.md` — Wave 1C-A / 1C-B (complete / validated / sealed; merged to `main` by PR #16)
- `docs/CardExpansionWave1CC0Execution.md` — Wave 1C-C0 final seal record
- `docs/CardExpansionWave1CC1WarcryHandToDrawPileTopPlan.md` + amendments — historical C1 design authority; implementation merged by PR #18

The sealed ordinary-card model is one immutable `UCardData`, one `Effects[]` composition, typed Base/Upgraded values and the sole runtime `bUpgraded` bit. Upgrade names/colors remain presentation formatting of frozen state. Do not reopen this model for Card Expansion or restore the former upgrade configuration fields. Repeatable upgrade remains outside this sealed ordinary-card scope.

Phase 8 is not a prerequisite for Card Expansion. Production Card Expansion has sealed Wave 1A, Wave 1B, Wave 1C-A, Wave 1C-B and Wave 1C-C0. C1 moved the exact Hand→DrawPileTop / post-Draw current-Hand selection capability forward and was merged to `main` by PR #18 (`ffbc164905a875bea5c9ab3dfe0a07df5068b8cc`). The original True Grit C1 plan is superseded; True Grit remains a thin future content consumer of already-generalized primitives. Because the repository does not contain a final standalone C1 user seal record, merge status must not be converted into a fabricated `COMPLETE / VALIDATED / SEALED` claim. Wave 1D, Card Trigger Source Expansion, multi-enemy work and Phase 8 remain outside the current slice unless separately authorized.

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
