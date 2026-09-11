# G8 详细设计：独立伤害数字与精确输入就绪

日期：2026-09-12。状态：**DESIGN PROPOSAL / NOT IMPLEMENTED / NOT SEALED**。

本文件是在 2026-09-10 版 G8 方案基础上的架构 Review 修订。修订前原文已原样备份为：

```text
docs/SelectionPresentationG8Design.pre-review-2026-09-12.md
```

本次仍然**只修改 G8 设计，不代表授权实现 G8**。G0–G7 当前阶段状态继续以
[SelectionPresentationG7SealAmendment.md](SelectionPresentationG7SealAmendment.md)
和对应 execution/validation 记录为准。

G8 的首版目标保持收窄：**只把独立 DamageNumber 变成跨 Resolution 可存活的
cosmetic tail；所有卡牌移动、Selection Group、Draw、正式状态更新和终局仍走现有
Blocking Presentation。**

本次 Review 结合当前 `main` 的实际代码补充四类已经不能继续留作“实现时再决定”的边界：

```text
1. PresentationSessionToken 与 detached Damage transaction 的唯一 authority
2. ReadyToConfirm / PendingSelection / TargetChoice 的真实交互面模型
3. FastInput deferred click 的 expected catch-up revision
4. 最近加入的 Hit / Enemy Attack combatant animation cue 与 detached Damage 的关系
```

## 1. 核心模型：两条运行轨 + 四种生命周期

G8 不建立第二套 Gameplay scheduler、第二套 reducer，也不建立“所有 Presentation
都能动态 Blocking/NonBlocking”的万能 visual-job 框架。

长期生产模型只有两条视觉运行轨：

```text
A. Blocking Presentation lane
   Controller → Record/Group playback → exact completion → reducer

B. Detached cosmetic lane
   committed Damage fact → private DamageNumber instance → tick/fade → self cleanup
```

概念上必须继续区分四种生命周期：

```text
Gameplay lifetime
!=
chronological Record/reducer lifetime
!=
detached cosmetic lifetime
!=
interaction readiness lifetime
```

权威顺序永远保持：

```text
A Gameplay / events / triggers / Resolution complete
→ A 的 committed Presentation facts 可以被消费
→ 当 B 的 exact interaction surface ready 时，B request 才可能合法
→ B Gameplay
```

Detached cosmetic 可以跨已完成 Resolution 存活，但不能导致 Gameplay 并发、reducer
乱序、trigger 重排或 speculative future state。

一句话约束：

> **G8 允许视觉尾巴跨 Resolution 重叠，不允许 authoritative Gameplay/Reducer 生命周期重叠。**

## 2. 当前实现约束与迁移边界

以下是当前 `main` 的实现事实，也是 G8 设计必须适配的边界：

- `BattlePresentationController.cpp` 当前只有一个 active playback；`CompleteActiveRecord()`
  负责应用 Record reducer、发布 working snapshot 并推进下一 Record。
- `BattleHUDWidgetBase.h` 的 Record/Group 共用 exact tracked playback owner；这条 G0–G7
  Blocking 协议继续保留。
- `BattleHUDWidget.cpp` 当前 Damage playback 同时承担：
  - historical Damage payload 校验；
  - HP/Block 临时显示；
  - `Txt_DamagePresentation`；
  - target opacity hit flash；
  - target `Hit` combatant animation cue；
  - committed non-player Attack 对 player 造成 Damage 时的 source `Attack` cue。
- CardPlayed 仍依赖单一 `NativePlayedCardWidget` 和 PlayArea 生命周期，因此首版禁止
  card tail detach。
- `BattleHUDViewModel::RefreshLiveInputBindingsIfCaughtUp()` 已对 latest frozen baseline、
  player-facing read 和 VM BattleId/StateRevision 做 exact guard；G8 必须复用这些权威
  核验，而不是用 cosmetic job 数量替代。
- 普通 card target 使用 `EBattleHUDInteractionState::ChoosingTarget`；无目标 card 使用
  `ReadyToConfirm`；Gameplay pending card selection 则是独立 authoritative selection
  lifecycle，并不等同于 `EBattleHUDInteractionState` 中的一个枚举值。
- G6 的 N-child parallel Group 是**同一 committed group 内的并行视觉**；G8 detached
  DamageNumber 是**跨已完成 Resolution 的 cosmetic lifetime**。二者不合并。

## 3. 首版范围

### 3.1 唯一 detached / NonBlocking 候选

```text
DamageNumber only
```

DamageNumber 使用 committed/frozen Damage payload 的 `IncomingDamage` 作为展示值，保持
当前语义，不暗中改成实际 HP loss。

有效的“完全被 Block 吸收”仍可显示 DamageNumber：

```text
IncomingDamage > 0
HPDamage == 0
BlockedDamage > 0
```

真正 `IncomingDamage == 0` 的 no-op 不作为首版 DamageNumber 新语义；沿用当前 Damage
payload validation，不为了 G8 扩展历史事实模型。

### 3.2 明确保持 Blocking

首版以下全部保持 G0–G7 行为：

```text
CardPlayed
PlayArea → destination
Hand / SelectionArea → Draw/Discard/Exhaust/Removed
G6 Selection Group
Draw → Hand
Deck shuffle
Selection presentation
Target-choice presentation
formal HP / Block / Energy / Status updates
Terminal / unavailable / recovery
```

受击 opacity flash 也不 detach。Detached Damage 成功路径不再借用正式 target opacity；
旧 Blocking fallback 仍保留现有正式受击视觉。

### 3.3 Combatant animation cue 不属于 DamageNumber job

当前 Damage playback 已承担 target `Hit` 与部分 source `Attack` 动画触发。G8 不能因为
跳过旧 `BeginNativeDamagePresentation()` 而丢失这些最近已经存在的视觉行为。

首版将 Damage 表现拆成三个职责：

```text
A. Formal state
   HP / Block
   → Controller reducer-owned

B. Detached DamageNumber
   → private cosmetic owner

C. Combatant animation cue
   Hit / committed enemy Attack
   → presentation-only best-effort cue
```

C 不属于 `DetachedDamageInstance`，也不参与 Record completion。它只能请求 combatant
presentation animation，不得写 HP/Block、opacity、ViewModel、card ownership 或 input。

### 3.4 体验预期

首版不人为拖长 DamageNumber，也不缩短/跳过仍 Blocking 的 card tail 来制造演示。
如果后续 Blocking 卡牌动画比 DamageNumber 更长，某些牌实际看不到跨牌数字重叠是合法
结果。验收记录真实 input-ready 时间点和数字是否仍存活，只报告实际收益。

## 4. 权威所有权与身份

### 4.1 Presentation session authority：由 Controller 唯一 mint

`PresentationSessionGeneration` 必须只有一个 mint authority；首版明确为：

```text
UBattlePresentationController
```

理由：Controller 已实际拥有 active envelope、playback queue、CurrentBattleId、Skip、
reconcile、direct baseline、presentation unavailable 和 playback generation。Session 的失效
必须和这些状态切换保持同一原子边界，不再让 Presenter 充当间接 mint authority。

不可变身份：

```text
PresentationSessionToken =
  BattleId
  + PresentationSessionGeneration
```

Controller mint token；HUD / ViewModel readiness evaluator / detached Damage owner 只消费它。
它们不得各自推导另一套“当前 session”。

以下事件必须使旧 Session 失效：

```text
Controller 初始化新的有效 Presentation binding
battle replacement
HUD/Controller binding replacement
direct baseline mode transition
presentation unavailable transition
recovery 需要建立新的 presentation session
Controller shutdown
```

顺序固定为：

```text
invalidate old session
→ cancel/cleanup old private cosmetics
→ establish new session（若仍存在 Presentation session）
```

### 4.2 Controller 独占 Record cursor / reducer commit

G8 的最重要 ownership rule：

```text
只有 UBattlePresentationController 可以：
- 推进 ActiveRecordIndex
- Apply/commit Record reducer
- publish formal WorkingSnapshot
- dispatch next Record
- 进入 envelope recovery
```

HUD detached owner 只能：

```text
PrepareDetachedDamageVisual
ActivatePreparedDetachedDamageVisual
CancelDetachedDamageVisual
TickDetachedDamageVisuals
```

HUD 永远不能通过 DamageNumber callback 间接触发：

```text
CompleteActiveRecord
NotifyPresentationFinished
reducer
next Record
readiness grant
```

### 4.3 DamageNumber identity

首版不定义通用 `FPresentationVisualJob`。使用用途明确的小型 identity：

```text
DetachedDamageToken =
  PresentationSessionToken
  + SourceResolutionId
  + PresentationSequence
  + LocalDamageVisualGeneration
```

以及：

```text
DetachedDamageVisualSpec =
  IncomingDamage
  + TargetPresentationId
  + FrozenHostLocalStartPosition
  + finite VisualDuration
  + finite HardTimeout

DetachedDamageInstance =
  Token
  + SourceFinalStateRevision      // diagnostic provenance only
  + FrozenDamageVisualSpec
  + GC-tracked transient Widget
  + Elapsed
  + LifecycleState { Prepared, Active }
```

`LocalDamageVisualGeneration` 由当前 detached owner 单调分配，不来自时间、指针、容器顺序
或 Widget 地址。同一 Record 如果在提交前重新尝试 prepare，也必须分配新的 generation。

该 token 与 `FPresentationPlaybackToken` 类型分离；接口不得互相接受。

### 4.4 容器与 GC

首版优先使用 GC 可达的 deterministic small container，例如：

```text
UPROPERTY(Transient)
TArray<FDetachedDamageInstance> DetachedDamageInstances;
```

首版 defensive ceiling 预计不超过 32，线性 exact-token lookup 足够，不需要依赖 TMap
iteration order。

Transient Widget 必须由 `UPROPERTY` 可达容器强持有。DamageNumber 生命周期优先由 HUD
现有 `NativeTick()` 统一推进，不为每个实例创建独立 Timer/Ticker callback。

## 5. Damage historical reducer：先提取单一纯规则

当前旧 Damage Widget 对 historical Before/After 做了完整校验，但 Controller 的 Damage
`ApplyRecordToWorkingSnapshot` 相对更薄。G8 不能复制一份“detached Damage validator”。

实现前先提取单一纯规则，概念接口：

```cpp
bool TryApplyDamageRecord(
    FPresentationStateSnapshot& Snapshot,
    const FDamagePresentationPayload& Damage);
```

它至少校验：

```text
exact target exists
IncomingDamage > 0
HPBefore == Snapshot.Target.HP
BlockBefore == Snapshot.Target.Block
HPAfter / BlockAfter 范围合法
BlockedDamage == BlockBefore - BlockAfter
HPDamage == HPBefore - HPAfter
BlockedDamage + HPDamage > 0
BlockedDamage + HPDamage <= IncomingDamage
```

成功时只修改传入 snapshot；失败无副作用。

旧 Blocking `CompleteActiveRecord()` 和 G8 detached preflight 必须复用同一规则，避免出现：

```text
Blocking Damage 合法
Detached Damage 非法
```

或反过来的双份语义。

G8 preflight 使用 snapshot copy：

```text
CandidateSnapshot = WorkingPresentationSnapshot
TryApplyDamageRecord(CandidateSnapshot, Payload)
```

成功后正式 commit 直接把已经校验的 CandidateSnapshot 作为新的 working state；**不得再次
执行第二遍 Damage reducer**。这里的 “reducer exactly once” 指同一份规则产生一次正式
state transition，而不是先 mutate production state 再重复一次。

## 6. Detached Damage 启动事务

保留现有 `PlayPresentationRecord(...)` bool Blocking 合同。G8 只在 Controller 的 Damage
Record dispatch 前增加一个可完全回退的 detached transaction。

### 6.1 Controller 侧目标接口

概念上：

```cpp
enum class EDetachedDamageAttemptResult
{
    Declined,   // 尚未提交 authoritative display state，可安全走旧 Blocking Damage
    Consumed    // 已提交，或已经进入 recovery；调用方不得 fallback 重播
};

EDetachedDamageAttemptResult
TryCommitDetachedDamageRecord(const FPresentationRecord& Record);
```

`StartNextRecord()` 的唯一分流：

```text
Damage + G8 enabled
→ TryCommitDetachedDamageRecord
   ├─ Declined → old G0–G7 Blocking Damage path
   └─ Consumed → return；transaction 自己已经推进或 recovery
```

### 6.2 精确事务顺序

```text
1. validate current active-envelope / Record cursor identity
2. CandidateSnapshot = WorkingPresentationSnapshot
3. TryApplyDamageRecord(CandidateSnapshot, Damage)
   └─ invalid → active-envelope recovery；Consumed
4. validate detached eligibility / session / Host / geometry / capacity
5. HUD prepare hidden DamageNumber（外部不可见）
   └─ decline → exact rollback；Declined
6. pre-commit exact recheck：
   active envelope + index + Record identity + SessionToken + prepared token
7. commit CandidateSnapshot as WorkingPresentationSnapshot
8. publish formal working snapshot to ViewModel
9. post-publication exact recheck：
   publication 可能同步触发 Skip/replacement/unavailable/reconcile
10. exact prepared visual 仍有效 → activate DamageNumber
11. exact session/Record 仍允许 → fire Damage combatant cues
12. increment Record cursor / dispatch next Record
```

### 6.3 失败分界

```text
提交前 detached decline
→ rollback hidden instance
→ old Blocking Damage playback

historical payload / reducer invalid
→ existing active-envelope recovery
→ NOT animation fallback

提交后 prepared visual stale / activation failure
→ clean exact private visual only
→ Record 已 reduced，不 fallback，不 replay reducer

同步 Skip/replacement/reconcile
→ old session/token invalid
→ stale transaction stops
→ recovery 后不得执行旧 fallback
```

### 6.4 Combatant animation cue 的提交点

Combatant cue 只允许在：

```text
formal Damage reducer 已 commit
+ ViewModel publication 已发生
+ post-publication exact Session/Record recheck 成功
```

之后触发，并发生在下一 Record dispatch 前。

首版提供用途明确的 HUD API，例如：

```cpp
void PlayCommittedDamageCombatantCues(
    const FPresentationRecord& Record);
```

它只能触发：

```text
target Hit
committed non-player Attack damage to player → source Attack
```

不得修改正式 target opacity。旧 opacity hit flash 只属于 Blocking fallback。

## 7. Detached cosmetic 的长期完成语义

G8-D 正式 NonBlocking 后，DamageNumber 是真正 fire-and-forget cosmetic：

```text
DamageNumber finish / timeout / cancel
→ remove own Widget
→ remove own instance
→ END
```

生产路径禁止：

```text
DamageNumber finish → NotifyPresentationFinished
DamageNumber finish → Controller CompleteActiveRecord
DamageNumber finish → ViewModel mutation
DamageNumber finish → InteractionReady mutation
DamageNumber finish → card ownership mutation
DamageNumber finish → combatant formal-state restoration
```

即 cosmetic lane 对 authoritative lane **只有单向输入，没有反向 completion 边**。

## 8. Interaction readiness：以真实交互面为核心

### 8.1 Readiness 不是 cosmetic job 状态

最终 G8-D 后：

```text
DetachedDamageInstances.Num() > 0
```

本身既不能锁 input，也不能解锁 input。

Input readiness 只取决于：

```text
当前 authoritative Gameplay request mode
+ exact displayed/read-facing revision or selection boundary
+ exact current decision surface identity
+ required Blocking Presentation/recovery 是否完成
+ terminal/unavailable 状态
```

不建立通用 barrier registry。

### 8.2 真实 mode 模型

G8-B 不再把当前系统简化成只有 `Normal / PendingSelection / TargetChoice`。首版 readiness
必须覆盖以下真实交互面：

```text
NormalPlayerTurn
CardReadyToConfirm
CardTargetChoice
PendingCardSelection
Resolving
Terminal
Unavailable
```

其中 PendingCardSelection 是 Gameplay authoritative selection lifecycle，不要求把它新增成
`EBattleHUDInteractionState` 枚举值。

判定优先级固定为：

```text
Terminal / Unavailable
→ Authoritative PendingCardSelection
→ Resolving
→ ChoosingTarget
→ ReadyToConfirm
→ Idle / NormalPlayerTurn
```

特别是：

```text
HasAuthoritativePendingCardSelection() == true
TryGetPendingCardSelectionReadView() == false
```

表示 Gameplay 已经在等选择，但 displayed Presentation 尚未追到候选面。此时：

```text
普通出牌禁止
EndTurn 禁止
selection submit 禁止
仅等待 exact pending-selection read surface ready
```

### 8.3 Readiness credential

首版使用已有/最小身份，不建立万能 `SurfaceGeneration`：

```text
NormalPlayerTurn:
  SessionToken + BattleId + StateRevision

CardReadyToConfirm:
  SessionToken + BattleId + StateRevision
  + exact SelectedCardRuntimeId
  + InteractionState == ReadyToConfirm

CardTargetChoice:
  SessionToken + BattleId + StateRevision
  + exact SelectedCardRuntimeId
  + InteractionState == ChoosingTarget
  + current LegalTargets derived from exact live binding

PendingCardSelection:
  SessionToken + BattleId
  + SelectionBoundaryRevision
  + existing SelectionGeneration / exact pending request identity
```

**G8 首版不新增 `TargetChoiceGeneration`。** 当前 target request 是同步决策面，没有具体的
跨帧 target callback 需要防 ABA。只有未来真实新增 deferred target callback 时，才以独立
设计增加 generation。

新 Battle、session replacement、revision replacement、SelectionGeneration replacement、
selected-card replacement、recovery 启动都会使旧 credential 失效。

### 8.4 Mode-specific grant

`RefreshLiveInputBindingsIfCaughtUp()` 当前会回到普通 PlayerTurn Idle，因此不能作为所有模式
共同 finalizer。统一的是 evaluator，不是 grant 动作。

```text
EvaluateInteractionReadiness(CurrentMode)
  |
  +-- NormalPlayerTurn
  |     → exact baseline/read/VM guard
  |     → existing normal live-binding refresh
  |
  +-- CardReadyToConfirm
  |     → preserve SelectedCardRuntimeId
  |     → only Confirm/Cancel + currently legal EndTurn policy
  |
  +-- CardTargetChoice
  |     → preserve SelectedCardRuntimeId + LegalTargets
  |     → only target choose/cancel
  |
  +-- PendingCardSelection
        → exact pending request / SelectionGeneration / boundary/read guard
        → preserve pending-selection transient set
        → only Select/Deselect/Confirm/Cancel for that request
```

不得通过某一个 surface ready 顺便开放另一个 surface 的请求。

### 8.5 Mode matrix

| 当前交互面 | 可开放 | 必须禁止 |
|---|---|---|
| Normal PlayerTurn ready | 合法选牌/出牌、合法 EndTurn | Query 拒绝请求 |
| Card ReadyToConfirm | 当前牌 Confirm/Cancel；EndTurn 依既有行为 | 另一普通出牌、target bypass |
| Card TargetChoice | 合法目标选择/取消 | 绕过目标流程的普通请求 |
| Pending Selection surface 未 ready | 无选择提交 | 普通出牌、Confirm、EndTurn |
| Pending Selection exact surface ready | 选/取消选、合法 Confirm/Cancel | 普通出牌、EndTurn |
| Resolving | 无普通 battle request | 普通出牌、EndTurn、旧 surface continuation |
| Terminal / unavailable | 既有恢复/终局控件 | battle request |

无可打牌时仍可能允许 EndTurn，所以 NormalPlayerTurn readiness 不能以“存在可打牌”为条件。

### 8.6 Shadow readiness gate

G8-B 不提前解锁。新 evaluator 先只产生 shadow result，并与现有实际 input/mode 逐边界
比较：

```text
任何未解释 divergence
→ G8-B FAIL
→ 不进入 G8-C
```

只有明确记录为现有 bug 且单独授权修复的差异，才允许不一致。

## 9. FastInput：deferred click 使用 ExpectedCatchUpRevision

保留 `HasActiveNativePresentation()` 当前 Blocking/card 语义，不把 detached DamageNumber
混入该查询。

只有 cosmetic tail 存在时：

```text
正常点击不能触发 SkipPresentation
不能因为点击而清空 DamageNumber
```

仍存在真正 Blocking Presentation 时，既有 fast catch-up 可保留；但首版把当前单一
`PendingFastCardRuntimeId` 提升为 exact deferred request：

```text
PendingFastCardRequest =
  PresentationSessionToken
  + BattleId
  + ExpectedCatchUpRevision
  + RuntimeId
```

`ExpectedCatchUpRevision` 不是点击发生时的旧 displayed revision，而是点击被接受为 fast
catch-up 时捕获的**目标最新 frozen baseline revision**。

示例：

```text
click 时 displayed revision = 100
latest sealed frozen baseline = 102
→ ExpectedCatchUpRevision = 102
```

Skip 后 retry 仅在以下条件全部满足时发生：

```text
same exact SessionToken
same BattleId
Current displayed/read revision == ExpectedCatchUpRevision
no authoritative PendingCardSelection
exact normal player-turn interaction surface
RuntimeId 仍是当前 displayed/live hand 中的合法身份
```

如果 catch-up 过程中又出现 revision 103：

```text
CurrentRevision = 103
ExpectedCatchUpRevision = 102
→ stale deferred click 丢弃
```

旧点击不得被带入更新的 Gameplay state，也不得在新 SelectionGeneration/target surface 重放。

## 10. TransientVFXHost 与正式 HUD 隔离

运行时在 Native HUD 根 Canvas 下创建独立 `TransientVFXHost`，不复用 Hand、PlayArea、
SelectionArea，也不要求修改 production `.uasset`。

Host 和所有 DamageNumber child：

```text
HitTestInvisible
不获得 focus
不参与 drag/drop
不处理 Selection/Target 输入
不成为 formal Hand/Combatant surface
```

prepare 从 frozen `TargetPresentationId` 匹配当前历史目标 surface，并验证：

```text
Battle/session exact
Target surface exists
geometry finite/non-zero
Host geometry valid
AbsoluteToLocal conversion valid
```

随后冻结 Host-local 起点。激活后只更新自己的 translation/opacity；绝不继续追踪 target
Widget，也不：

```text
restore target opacity
write formal HP / Block
write ViewModel
change Hand/Card ownership
```

Viewport/DPI/Host geometry 发生无法安全重映射的改变时，首版直接取消当前 session 的
cosmetic instances。普通 historical dirty publication、FinalSnapshot 和 Hand reconcile 不得
清空 Host。

### 10.1 Tick 生命周期

优先复用 `UBattleHUDWidget::NativeTick()`：

```text
UpdateNativeCardAnimation
UpdateHandInteraction
UpdateDetachedDamageNumbers
```

每个 Active DamageNumber：

```text
Elapsed += max(DeltaTime, 0)
→ update translation/opacity
→ VisualDuration 到期：exact cleanup
→ HardTimeout 到期：exact forced cleanup
```

Prepared instance 不得进入可见 tick，直到 Controller 完成正式 publication 后显式 Activate。

## 11. 资源合同与恢复

### 11.1 有限生命周期是硬合同

每个 DamageNumber 必须有：

```text
finite positive visual duration
finite hard timeout > visual duration
idempotent exact cleanup
session-wide cleanup
```

坏配置在 prepare 阶段 decline 到旧 Blocking path。

### 11.2 Defensive sanity ceiling

首版不把实例上限当正常 pacing 规则，也不做 eviction。可以保留明显高于正常密度的：

```text
MaxDetachedDamageNumberInstances = 32   // exact value implementation/PIE 决定
```

达到 ceiling 时：

```text
不删除旧合法 cosmetic
不随机/按容器顺序淘汰
不先 reducer 再发现没容量
→ detached prepare decline
→ old Blocking Damage path
```

### 11.3 Recovery policy

| 事件 | Detached Damage | authoritative/input lane |
|---|---|---|
| 正常 Record/Envelope/FinalSnapshot 完成 | 合法 instance 继续 | 正常 chronological/readiness |
| 单项 finish/timeout | 只清 exact instance | 无输入副作用 |
| active-envelope failure reconcile | invalidate session + 清本 session cosmetic | 保留后续 backlog，走现有 recovery |
| Global Skip/backlog collapse | invalidate/清相关 cosmetic | 保留 sealed catch-up，不伪造 visual success |
| HUD/Controller replacement | 旧 session 先失效再全清 | 新 session 重建 readiness |
| battle replacement/direct/unavailable | 全清旧 session | 走现有模式切换/恢复 |
| terminal | 全清 | 正式终局优先 |

Cosmetic timeout/cancel 不触发 Gameplay `ResolutionFault`，也不把整个 Presentation 标记
unavailable。损坏 committed Envelope 仍按现有 historical failure contract 处理。

## 12. 分阶段实施

### 12.1 G8-A — 基础设施与单一 Damage reducer（生产不启用）

实现/验证：

```text
Controller-owned PresentationSessionToken authority
single TryApplyDamageRecord historical reducer/validator
DetachedDamageToken / VisualSpec / Instance
GC-safe deterministic owner/container
TransientVFXHost
hidden prepare + exact rollback
NativeTick lifetime + finite duration + hard timeout
sanity ceiling pre-commit decline
session-wide cleanup
PlayCommittedDamageCombatantCues API boundary
post-publication stale rejection harness
```

Production Damage 仍全部走旧 Blocking path。无 early input，无 detached production activation。

### 12.2 G8-B — Exact interaction evaluator + FastInput shadow

实现统一 evaluator 和 mode-specific grant contract，但只 shadow 对照旧行为：

```text
NormalPlayerTurn
CardReadyToConfirm
CardTargetChoice
PendingCardSelection
Resolving
Terminal/Unavailable
```

同时实现/验证：

```text
ExpectedCatchUpRevision deferred fast-card request
stale Session/Battle/revision/request rejection
HasAuthoritativePendingCardSelection 与 visible read surface 的两阶段 gate
```

所有现有 refresh/recovery 入口接受统一 policy 审计；任何未解释 divergence 阻止下一阶段。

### 12.3 G8-C — Damage detached staging，仍保持旧输入节奏

首次让真实 Damage Record 使用 Controller-owned detached transaction：

```text
formal HP/Block = Controller reducer-owned
DamageNumber = private detached visual only
Hit/Attack = best-effort committed cue
```

本阶段为了单独验证 reducer/job 解耦，可以保留旧 Damage 输入等待时间，但**不得通过
DamageNumber finish callback 来释放输入**。

使用 Controller-owned compatibility deadline：

```text
Damage commit time = T
CompatibilityInputNotBefore = T + LegacyDamageBlockingDuration
```

这条 deadline 是 staging-only input policy；DamageNumber 自己 finish/timeout 仍然只 cleanup。

因此即使 G8-C：

```text
cosmetic finish
X→ readiness mutation
```

仍然成立。

G8-C 允许 HP/Block publication 时机和动画重叠节奏相对旧 Damage playback 改变，必须单独
Automation/PIE，不能用“输入仍锁着”宣称视觉完全等价。

### 12.4 G8-D — DamageNumber 真正 NonBlocking

删除 `CompatibilityInputNotBefore` 的 G8-C staging gate。

此后 DamageNumber completion 对 Controller/ViewModel/readiness 完全无反向影响。当 exact
CurrentMode surface ready 时，即使旧 DamageNumber 仍 alive，也允许对应合法输入。

必须证明：

```text
A Gameplay/Resolution 已完成
A DamageNumber 仍 alive
→ B Request 可以被真正接受
→ B Gameplay 只在 A 完成后开始
→ A cosmetic 后续 finish 不影响 B
```

### 12.5 G8-E — 集成验证

覆盖：

```text
连续/多段 Damage
fully-blocked Damage
player Hit cue
enemy Hit cue
enemy Attack cue
rapid legal card input
ReadyToConfirm confirm/cancel
Target choose/cancel
Draw → Pending Selection
Pending Selection surface catch-up
EndTurn
Skip
active-envelope recovery
HUD replacement
battle replacement
unavailable/direct
terminal
viewport/DPI change
sanity ceiling
GC
```

基础 cleanup 必须在首次 production detached activation 前已经通过，不等到 G8-E 才补。

### 12.6 G8-F — 证据与封存

Build + affected focused Automation + Native `L_BattleTest` PIE 全部通过后，才能标记
implemented / validated / sealed。

G8 feature 关闭时，新 Record 必须完整退回 G0–G7 Blocking path。若运行中关闭：

```text
invalidate current detached session
→ clear private cosmetics
→ clear staging compatibility state
→ 按当前 exact authoritative surface 重评输入
```

已 reduced Record 不重放。

## 13. 验收标准

### 13.1 AUTOMATED GATES

| 测试主题 | 必须证明的外部合同 |
|---|---|
| A→B authoritative 时序 | A Resolution 完成后 B 才 accepted；A DamageNumber 可仍 alive；无并发 Gameplay |
| Single Damage reducer | Blocking/G8 preflight 复用同一历史规则；无双份 Damage 语义 |
| Reducer determinism | G8 on/off、cosmetic tick/finish 时机不改变 committed Record/trigger/reducer/FinalSnapshot 顺序 |
| Controller transaction authority | HUD 不推进 Record、不 commit reducer、不通过 cosmetic callback completion |
| One-way cosmetic lane | G8-C/D 的 finish/timeout 都不调用 Controller/VM/readiness/ownership mutation |
| Exact session/token | 旧 HUD/session callback 不能删除新 session instance，也不能影响新 input surface |
| Prepare transaction | pre-commit failure 无外部副作用并回 Blocking；post-publication stale 只丢 visual，不重播 reducer |
| Damage combatant cues | detached 成功仍保持正确 target Hit / enemy source Attack；cue 不影响 formal state |
| Mode-specific readiness | Normal/ReadyToConfirm/Target/PendingSelection grant 不串模式 |
| Pending selection boundary | authoritative pending 已存在但 read surface 未 ready 时，普通出牌/EndTurn/submit 全禁止 |
| Shadow parity | G8-B shadow 与旧实际 input/mode 无未解释 divergence |
| FastInput expected revision | retry 只在 exact ExpectedCatchUpRevision；更老/更新 revision 都丢 stale click |
| Read-before-envelope | latest read 提前、Envelope 尚未 chronological catch-up 时不能开放普通输入 |
| Damage isolation | old cosmetic 不恢复 opacity、不回写 HP/Block、不改 Hand/target state |
| Lifetime/GC | finite duration/hard timeout、GC-safe owner、重复 cleanup 幂等、session cleanup 正确 |
| Sanity ceiling | ceiling 在 pre-commit decline；不 eviction、不重复 reducer |
| Recovery | reconcile/Skip/direct/unavailable/terminal/replacement 后无 ghost cosmetic、无永久锁 |
| G0–G7 regression | Selection ownership、G6 Group、SingleRecord fallback、exact cancellation/recovery 不受影响 |

主时序测试使用真实 Gameplay Request + committed Envelope。可注入 clock 只用于确定性边界，
不得以模拟 job 存活冒充真实 PIE 体验。

### 13.2 MANUAL PIE GATES

Native `L_BattleTest` 至少验证：

```text
打出攻击牌 A
→ Damage formal HP/Block 正确更新
→ DamageNumber 出现
→ target Hit cue 正常
→ exact normal surface ready 时 A 数字仍可见
→ 立即合法打出 B
→ A 数字继续自己的生命周期
→ B 正常 CardPlayed / Gameplay
```

敌人回合至少验证：

```text
committed enemy Attack damage to player
→ enemy Attack cue 正常
→ player Hit cue 正常
→ DamageNumber 正常
→ formal HP/Block 不被 cosmetic 回写
```

同时观察：

```text
HP/Block 不回跳
formal target opacity 不被旧数字恢复/覆盖
Hand 不闪回/重复
只有 cosmetic tail 时点击不触发 Skip
无 ghost DamageNumber
无 stuck input
```

另验证：

```text
无目标牌 ReadyToConfirm 的 Confirm/Cancel
Target choose/cancel
Draw → PendingSelection 未 ready 与 ready 两个边界
G6 multi-select 同时播放
Skip / terminal / viewport-DPI 变化后无残留
```

记录默认 DamageNumber duration、实际 input-ready 时间和后续 Blocking card-tail duration。
若默认时序没有实际跨牌视觉重叠，应如实记录，不通过人为延迟 cosmetic 或缩短 Blocking
路径伪造收益。

## 14. 本次设计冻结点与未授权范围

本次修改仅调整设计，未修改 C++、未运行 UE Build/Automation/PIE，也没有新增运行验收证据。

当前首版冻结候选：

```text
Blocking Presentation lane 保持 G0–G7
Detached cosmetic lane 首版只有 DamageNumber
Controller 是 session mint + detached transaction + reducer commit 唯一 authority
Damage historical validation 提取为单一 reducer/validator
HUD detached owner 永不推进 Record/reducer/readiness
最终 DamageNumber completion 对 authoritative/input lane 无反向边
G8-C 兼容输入等待由 Controller deadline 持有，不由 cosmetic finish 释放
Interaction readiness 覆盖 Normal / ReadyToConfirm / Target / PendingSelection
PendingSelection 保持独立 authoritative lifecycle，不硬塞入 InteractionState enum
首版不新增 TargetChoiceGeneration
FastInput deferred request 使用 exact ExpectedCatchUpRevision
Damage detached 成功必须保留 committed Hit / enemy Attack combatant cues
combatant cue 不属于 DamageNumber job，也不参与 Record completion
Presentation session 由 Controller 单一 mint
prepare/reducer/publication/activation 有 pre/post exact recheck
DamageNumber 由 NativeTick 管理有限 lifetime + hard timeout
数量 ceiling 只是高位 bug containment；首版无 eviction
card tail / opacity hit flash / status VFX 不在首版 detached 范围
```

CardPlayed、PlayArea card tail、Selection transitions、Draw→Hand、G6 Group、status VFX、
combatant animation 是否未来获得更广泛 overlap/detach，需要新的独立设计与 acceptance；
不因 G8 DamageNumber seal 自动获得授权。
