# 整体架构说明

本文是 [`Architecture.md`](Architecture.md) 的中文同步版，用于描述项目的持久化整体架构。目录级 `AGENTS.md` 文件定义具体实现规则。已封存的 UI-A2 契约记录在 Phase 6UI-A2 系列文档中；当前阶段状态以 `docs/DevelopmentPhases.md` 为准，当前 Native UI 细节以 `docs/Phase6UIA2NNativeHUDRefactor.md` 和 `docs/WBPSavedBlueprintSnapshot.md` 为准，当前选牌表现细节以 `SelectionPresentationG*` 和选牌约束文档为准。较早的 A2 实现/验证文件保留阶段证据，不应被当作当前待办。架构变更时应同步维护中英文两份说明，代码类名、函数名、结构体名和资源路径保持英文原文。

## 当前 Hand 选牌执行

统一的选牌契约定义在 `docs/CardSelectionRefactorConstraints.md`。Effect 组合运行时 `CandidateSource`、数量/模式/取消意图以及作者定义的 `Continuation`。一个共享的延迟 Action 会在 Execute 时捕获权威的 Hand 顺序。Player 模式接收显式注入的交互边界能力，推进决策 revision，封存已经提交的前缀，并在进入 pending Selection 前重新绑定剩余尾部。Random 模式使用同样的捕获机制，但不创建 pending request 或 interactive segment，从而保留规范结果顺序和权威 RNG 消耗。

每一次真实的 Player 决策都对应一个独立的 `(BattleId, StateRevision)` 显示边界，包括连续做出相同选择的情况。Recorded UI 只会在精确追上冻结状态时暴露候选项；no-history 模式则发布匹配的冻结基线，不带历史记录。`PresentationUnavailable` 保留 Gameplay 必须完成的选择，同时维持现有的禁用输入/错误展示面。无效提交会保持一个有效 request 处于 pending 状态；内部 dependency、`BeginSelection`、`Continuation` 或 insertion 失败遵循 Gameplay 框架的 fault policy。Action 负责插入依赖批次并插入 Finish；候选项和 Continuation 都不得驱动 Queue 前进。

### Native 已选卡牌的视觉归属（G5）

生产 Selection 使用一个持久存在的 Canvas-root `SelectionArea` Overlay。每个已选 `RuntimeId` 在其中拥有一个对应的、基于冻结数据的可见对象；它在历史 Hand Widget 中的对象仍作为禁用输入的 Hidden 结构槽位保留。Confirm 会保留 `SelectionArea` 对象及其位置，然后由 G4 SingleRecord 将同一个对象重新挂载到 transition surface。Hand reconciliation 不负责定位或重新创建已经确认的视觉对象。安全的并行播放仍属于 G6 工作范围。

ViewModel 在提交被接受后提交 Confirm，在该事务期间暂存重入产生的 snapshots/outcomes，并准备精确的 G1 recorded/direct receipts。一个 Native HUD 会在公开 ownership 或 snapshot 通知之前同步受影响的 surface；外部 multicast 的注册顺序不影响结果。在更新的 Ready edge 上找不到 correlation 时，执行明确的、仅 UI 的 unavailable recovery；它不会伪造 completion watermark，也不会生成 Gameplay fault。

范围和待完成的视觉验收记录在 `docs/SelectionPresentationG5Execution.md`。

## 1. Battle 执行

```text
CardData / CardInstance
→ CardEffect
→ BattleAction
→ BattleActionQueue
→ typed Operation Spec
→ Modifier Pipeline
→ Commit
→ BattleEvent
→ Trigger collection
→ Reaction BattleActions
→ BattleActionQueue
```

`BattleStateMachine` 控制宏观回合流程。`BattleActionQueue` 控制确定性的执行顺序。Modifier 在 commit 前修改 operation。Event 描述 commit 之后已经发生的事实。Trigger 负责构建排队的反应 Action。

复杂行为必须通过通用组合自然产生。Pommel Strike 只知道已经配置的 Damage/Draw Effect；Defend 只知道 Block；`DeckRuntime` 只知道区域、抽牌和洗牌；Sundial 只知道洗牌 Event。它们都不应知道具体的组合关系。

## 2. 状态归属

### BattleManager

负责 Battle 编排、回合转换、Battle 级 identity/RNG 分配、公开 Query/Request 边界以及稳定的 read 发布。

### Combatants

拥有权威的 HP 和 Block。类型化的 `CommitResult` 将提交前后的事实传递给 Action，同时不让 Combatants 依赖 Presentation。

### DeckRuntime

拥有 `DrawPile`、`Hand`、`DiscardPile`、`ExhaustPile` 和 `PlayArea` 的真实状态。`DrawPile` 的末端是牌堆顶。运行时卡牌身份是稳定的 `UCardInstance` identity；`RuntimeId` 是用于 Presentation/debug 的 identity。

### StatusContainer

拥有权威的 Status 成员关系以及 merge/create 决策。`UStatusInstance` 拥有可变的 `Amount`、`RuntimeSequence` 和 `Owner`；`UStatusData` 是不可变的定义数据。

### RelicContainer

拥有 Battle 级 Relic 成员关系。`URelicData` 是不可变的定义数据；`URelicInstance` 拥有可变的运行时状态，例如 Sundial Counter，并与 Status runtime source 共享 Battle 范围的 `RuntimeSequence` 域。

## 3. BattleActionQueue

任意时刻只允许一个权威 Action 执行。顺序和完成状态必须显式表达。Action 可以加入依赖 Action，但绝不能驱动 Queue 前进。

同一逻辑链所需的依赖批次必须在当前 Action 完成前插入。嵌套反应使用排队的深度优先语义。Queue fault 在安全点进入 fault 状态，只广播一次，抑制正常的 `QueueEmpty`，并拒绝后续 mutation。

`QueueEmpty` 是可观察且不可重入的边界。`BattleManager` 会等所有观察者返回后，才继续宏观回合流程。

## 4. Modifier Pipeline

```text
ActionQueue       → 执行时机/顺序
Modifier Pipeline → commit 前的修改/拦截/覆盖/限制
BattleEvent       → commit 后的事实
Trigger           → 构建 Action 的 commit 后反应
```

使用 `FDamageSpec`、`FBlockSpec` 等类型化 spec。避免引入通用的 universal modifier context。

同一 domain 内的确定性排序为：

```text
Phase → Priority → RuntimeSequence → LocalModifierIndex
```

比例计算使用显式的整数 numerator/denominator、安全的中间值，并在每个 Modifier 之后执行 floor。

## 5. Event 与 Trigger

`BattleEvent` 是一个短生命周期、按契约不可变的 value fact。Dispatch/Trigger 代码不得缓存它的引用。

Trigger source 按需收集。资格判断使用 snapshot 语义；构建出的 Action 在 Execute 时校验 live state。Status 和 Relic 的 Trigger 排序为 `Priority → RuntimeSequence → LocalTriggerIndex`。

Trigger 是只读的 builder。它们绝不直接修改 Gameplay，也不驱动 Queue。Event 只在成功 commit 后发出，反应批次会在 source Action 完成前原子插入。

`FDeckShuffledEvent` 只会在 Gameplay 的 `UShuffleDeckAction` 完成 commit 后、剩余 bulk-draw continuation 执行前发生。一次已提交的 shuffle 通常会把 `DiscardPile` 中的卡移入空的 `DrawPile`，但如果该 `ShuffleAction` 是更早的 bulk-draw 步骤在可用 `DrawPile` 被消耗前预先计划的，那么 `MovedCardCount` 可以是 `0`。当 `DrawPile=0 / DiscardPile=0` 时，新发起的 draw request 不会安排 `ShuffleAction`。Battle 初始设置阶段的随机化属于 normalization，不是 Gameplay Event。

## 6. Card 与 Deck Resolution

```text
UCardData
→ UCardInstance
→ UPlayCardAction
→ UCardEffect::BuildActions() const
→ effect Actions
→ UFinishCardPlayAction
→ Execute-time destination resolution
```

Effect 是不可变的共享定义，用于保存基础意图。依赖可变状态的结果在 Action Execute 时解析。`FinishCardPlay` 将权威的卡牌移动交给 `DeckRuntime`。

默认卡牌描述由每个 Effect 按数组顺序贡献的本地化句子组成；适用时，再追加卡牌的 Exhaust keyword。每个贡献独立解析只读的 preview 参数；针对目标的覆盖范围由 `EffectIndex` 限定。共享定义保持不可变，最终 `FText` 遵循现有的 frozen snapshot pipeline。显式 custom-template mode 保留旧的 authored `Description` 契约。详见 `docs/AutomaticCardDescriptions.md`。

抽牌采用两级 Action 模型：

```text
UDrawCardEffect(DrawCount = N)
→ UDrawCardsAction(N)                 // bulk intent / orchestration
   ├─ UDrawCardAction                 // atomic one-card DrawPile -> Hand commit
   ├─ UShuffleDeckAction              // committed shuffle + DeckShuffled event
   └─ UDrawCardsAction(Remaining)     // continue the same bulk request
```

`UDrawCardsAction` 拥有 `RemainingDraws`，评估实时的 Hand 容量和各牌堆数量，并计划确定性的 continuation 批次。它绝不直接修改 `DeckRuntime`。`UDrawCardAction` 每次只执行一张卡的移动，也绝不决定洗牌或重试。

当 `DrawPile` 和 `DiscardPile` 都为空时，新 bulk request 会立即结束。如果 bulk request 在消耗当前可用的 `DrawPile` 后仍有未完成的抽牌数量，它会预先计划 `ShuffleDeckAction → DrawCardsAction(Remaining)`。因此，之前已计划的 `ShuffleAction` 之后可能在两个牌堆都为空时执行，并提交 `MovedCardCount=0`；这是一个真实的 Gameplay shuffle fact，不是通用的“空 DrawPile 就洗牌”规则。

抽牌和洗牌绝不能在 Queue 之外同步执行。Battle RNG 只初始化一次，并由已提交的非空 shuffle 以确定性方式消耗。

## 7. Presentation 架构

Gameplay 和 Presentation 是相互独立的时间线：

```text
Gameplay validation / Request
→ Begin Presentation Resolution
→ BattleActionQueue
→ Gameplay Commit
→ immutable typed Presentation Records
→ Gameplay/macro stability
→ freeze exact FPresentationStateSnapshot
→ seal immutable Resolution Envelope
→ deferred public delivery
→ BattlePresentationController
→ ViewModel working state
→ UMG playback
→ apply matching Envelope.FinalSnapshot
```

一个 sealed Envelope 拥有一个 `BattleId`、`ResolutionId`、Origin、有序 Records、`FinalStateRevision` 以及匹配的 `FinalSnapshot`。历史播放绝不从可变 Gameplay 重建过去。

已提交历史卡牌的 projection 只有一个 Presentation-only 边界：

```text
FPresentationCardSnapshot
→ PresentationCardView::MakePresentationOnlyCardView
→ FBattleHUDCardView
```

该 mapper 只用于冻结的 Record/presentation 卡牌，因此始终生成仅供 Presentation 使用、不能用于 Gameplay 出牌的 view。`FCardReadView → FBattleHUDCardView` 仍是另一条由 `TryFreezePresentationStateSnapshot` 所拥有的稳定 current-state freeze 路径；它携带当前 Gameplay legality，不得经过 Presentation-only mapper。Projection 完整性和 identity matching 是两个独立问题：例如 `RichDescription` 这样的显示字段必须在 projection 中保留，而历史 identity predicate 可以有意只比较稳定的 identity 字段。

内部 seal 和公开 notification 是两个不同阶段。Seal 会在下一个 Resolution 开始前释放 builder；`OnPresentationResolutionReady` 和 `OnReadStateReady` 仍然要等到被接受的 Request 返回后再延迟触发。

完整契约见 `docs/Phase6UIA2Implementation.md`；当前 Blueprint/PIE 收口见 `docs/Phase6UIA2EImplementation.md`。

## 8. MVVM 边界

```text
MODEL
BattleManager / Combatants / DeckRuntime / Status runtime / Relic runtime / Enemy Intent / BattleActionQueue

VIEWMODEL
frozen player-facing display state
formal Request forwarding
presentation-only selection/focus
latest-only live input bindings

VIEW
UMG Widgets
```

`FBattleReadSnapshot` 是连贯的当前 Gameplay/read 结构，可以持有弱运行时引用。`FPresentationStateSnapshot` 是某个精确 revision 的冻结显示模型，不依赖可变 Gameplay。

latest-only runtime binding 将当前 `RuntimeId/TargetId` 映射到弱对象，仅用于 Presentation 追上后提交正式 Request。它们不是历史状态。

## 9. Public Read 与 Request 边界

正常 UI 使用正式的 Query/Request API。Query 只是建议性的；Request 会重新校验当前权威状态。`AcceptedForResolution` 不代表播放已经完成。

Widget 不使用 `QueueEmpty`/idle 作为公开完成协议。`BattleManager` 发布连贯的 Battle 级 Ready edge。公开 notification 绝不会在发起该 accepted Request 的调用返回前重入触发。
