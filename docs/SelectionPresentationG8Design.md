# G8 详细设计：独立伤害数字与精确输入就绪

日期：2026-09-12。状态：**DESIGN PROPOSAL / NOT IMPLEMENTED / NOT SEALED**。

本文件是在 2026-09-10 版 G8 方案基础上的架构 Review 修订与补强。修订前原文已原样备份为：

```text
docs/SelectionPresentationG8Design.pre-review-2026-09-12.md
```

本次仍然**只修改 G8 设计，不代表授权实现 G8**。G0–G7 当前阶段状态继续以
[SelectionPresentationG7SealAmendment.md](SelectionPresentationG7SealAmendment.md)
和对应 execution/validation 记录为准。

G8 的首版目标保持收窄：**只把独立 DamageNumber 变成跨 Resolution 可存活的 cosmetic tail；所有卡牌移动、Selection Group、Draw、正式状态更新和终局仍走现有 Blocking Presentation。**

本轮 Review 结合当前 `main` 的实际代码，把以下边界冻结到设计中，不再留给实现阶段自行决定：

```text
1. PresentationSessionToken 与 detached Damage transaction 的唯一 authority
2. Controller replacement 下的 Session ABA 防护
3. ordinary Skip/backlog catch-up 不得误伤同一 authority 的 SessionToken
4. ordinary active-envelope reconcile 保持同一 Presentation authority/session
5. PresentationOwned 与 DirectBaseline 两类 readiness authority
6. Controller 初始化失败不是合法 DirectBaseline，而是 PresentationUnavailable/fail-safe
7. ReadyToConfirm / PendingSelection / TargetChoice 的真实交互面模型
8. PendingSelection exact request identity 直接复用 BattleId + SelectionBoundaryRevision，不新增第二套 request generation
9. TargetChoice / EndTurn 当前行为与 G8-B shadow parity 的处理方式
10. ReadyToConfirm / TargetChoice 保留切换当前手牌的现有合法交互
11. FastInput deferred click 的 ExpectedCatchUpRevision
12. FastInput 使用“可 Skip 的 authoritative delay”而不是只看真实 Native Widget playback
13. Damage historical reducer/validator 的唯一完整语义
14. Hit / Enemy Attack combatant animation cue 与 detached Damage 的关系
15. G8-C legacy blocking duration 的唯一 timing authority
16. G8-C 使用不可被其它 Blocking Presentation 抵消的 compatibility debt
17. G8-C debt 在 formal commit 后、ViewModel publication 前登记，确保同步 Skip 能看到并清理该 debt
18. ordinary Skip 按旧 Skip 语义清除 staging compatibility debt，但保持 SessionToken
19. replacement 新 Session 在新 binding 建立后、catch-up 前 mint，避免 sessionless-ready 窗口
20. Widget replacement 必须先 retire old playback owner，旧 playback token 不得发送给 new Widget
21. G8-C compatibility wait 与 legacy Blocking Damage timer 使用同一时间域 / pause / time-dilation semantics
22. G8 runtime disable 只关闭 detached feature，不在 authority 未变化时更换 PresentationSessionToken
23. 首版 DamageNumber 由 NativeTick + finite VisualDuration 结束，不额外保留不可达 HardTimeout
```

## 1. 核心模型：两条运行轨 + 四种生命周期

G8 不建立第二套 Gameplay scheduler、第二套 reducer，也不建立“所有 Presentation 都能动态 Blocking/NonBlocking”的万能 visual-job 框架。

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

Detached cosmetic 可以跨已完成 Resolution 存活，但不能导致 Gameplay 并发、reducer 乱序、trigger 重排或 speculative future state。

一句话约束：

> **G8 允许视觉尾巴跨 Resolution 重叠，不允许 authoritative Gameplay/Reducer 生命周期重叠。**

## 2. 当前实现约束与迁移边界

以下是当前 `main` 的实现事实，也是 G8 设计必须适配的边界：

- `BattlePresentationController.cpp` 当前只有一个 active playback；`CompleteActiveRecord()` 负责应用 Record reducer、发布 working snapshot 并推进下一 Record。
- `BattleHUDWidgetBase.h` 的 Record/Group 共用 exact tracked playback owner；这条 G0–G7 Blocking 协议继续保留。
- `BattleHUDWidget.cpp` 当前 Damage playback 同时承担：
  - historical Damage payload 校验；
  - HP/Block 临时显示；
  - `Txt_DamagePresentation`；
  - target opacity hit flash；
  - target `Hit` combatant animation cue；
  - committed non-player Attack 对 player 造成 Damage 时的 source `Attack` cue。
- Controller 当前 Damage `ApplyRecordToWorkingSnapshot()` 只把 `HPAfter / BlockAfter` 写入 working snapshot，historical validation 明显弱于 Widget 侧；G8 不允许在此基础上再增加第三份 detached validator。
- CardPlayed 仍依赖单一 `NativePlayedCardWidget` 和 PlayArea 生命周期，因此首版禁止 card tail detach。
- `BattleHUDViewModel::RefreshLiveInputBindingsIfCaughtUp()` 已对 latest frozen baseline、player-facing read 和 VM BattleId/StateRevision 做 exact guard；G8 必须复用这些权威核验，而不是用 cosmetic job 数量替代。
- Presenter 当前支持 intentional no-history/direct-baseline 模式：Presenter 未启用 committed presentation，或 battle recording 明确关闭时，没有 `UBattlePresentationController`，ViewModel 直接消费 frozen baseline 并恢复 live input；G8 readiness 不能强迫这条路径拥有不存在的 SessionToken。
- Presenter 当前还存在一条不同语义的 fallback：**本应启用 Controller，但 Controller 创建/初始化失败时，代码会退回 sessionless frozen-baseline delivery。** G8 将其视为错误恢复边界，不把它继续归类为合法 DirectBaseline；见 8.3。
- 普通 card target 使用 `EBattleHUDInteractionState::ChoosingTarget`；无目标 card 使用 `ReadyToConfirm`；Gameplay pending card selection 则是独立 authoritative selection lifecycle，并不等同于 `EBattleHUDInteractionState` 中的一个枚举值。
- 当前 `FPendingCardSelectionReadView` 没有显式 request token；现有 `SelectionGeneration` 属于 SelectionArea presentation ownership，并且可能直到第一次点击候选牌时才建立，因此不能作为 PendingSelection readiness 的 request identity。
- 但当前 player-facing pending selection 已经有一个更早建立的 exact Gameplay/read 边界：`AdvancePresentationAtInteractiveSelectionBoundary()` 会为每次真实交互边界推进 `StateRevision`，并把该值写入 `SelectionBoundaryRevision`。G8 直接复用 `BattleId + SelectionBoundaryRevision`，不再新建第二套 `PendingSelectionRequestGeneration`。
- 当前 `ChoosingTarget` 路径仍可能保留 `bCanEndTurn`，而 `RequestEndTurn()` 的输入门没有显式排除 `ChoosingTarget`。本设计将其视为**已有行为缺陷**，见 8.8；不能让 G8-B shadow evaluator 暗中改变该行为。
- 当前 `SelectCardByRuntimeId()` 在 `ReadyToConfirm` / `ChoosingTarget` 下仍允许用户改点另一张合法 Hand card并重建当前 card decision surface；这是现有合法交互，不属于上述 EndTurn bug。
- 当前 FastInput 只保存 `PendingFastCardRuntimeId`，没有冻结 Battle/session/目标 revision，因此 G8-B 必须补 exact deferred request identity。
- 当前 FastInput 的正常路径是“点击 → `SkipPresentation()` → 下一 tick retry”；因此 ordinary Skip 若使 SessionToken 失效，会让 deferred click 永远 stale。
- 当前 FastInput 还只用 `HasActiveNativePresentation()` 判断是否可 fast catch-up；G8-C 可能出现“真实 Widget playback 已结束，但只剩 CompatibilityDebtSeconds”的等待，因此首版必须把“有 Native playback”和“存在可 Skip 的 authoritative presentation delay”分开。
- 当前 `ReconcileActiveEnvelopeToFinalSnapshot()` 可以恢复当前 Envelope 后保留 backlog，并同步进入下一 Envelope；因此普通 reconcile 不能制造 session 空窗或在 `StartNextEnvelope()` 之后才补 mint replacement session。
- 当前 `SkipPresentation()` catch-up 末端可同步触发 `RefreshLiveInputBindingsIfCaughtUp()`；因此 HUD replacement 的新 Session 不能等 catch-up 结束后才建立，否则会出现短暂的 sessionless-ready 窗口。
- 当前 `SetWidget()` 在旧 Widget 上 cancel tracked playback 后会切换 `Widget` 再调用 `SkipPresentation()`；如果 Controller 的 `bWaitingForCompletion / ActivePlaybackToken` 尚未 retire，`CancelActivePlaybackUnit()` 可能再次把**旧 playback token**发送给已经安装的新 Widget。G8 replacement contract 必须消除这个跨-owner call，而不能只依赖 token mismatch 让它碰巧 no-op。
- 当前 Blocking Damage completion 使用 `UWorld::GetTimerManager().SetTimer(...)` 和 `NativePresentationDurationSeconds`；因此 G8-C 若宣称与旧 Blocking pacing 严格等价，compatibility wait 不能自行改用 platform/real-time wall clock。
- G6 的 N-child parallel Group 是**同一 committed group 内的并行视觉**；G8 detached DamageNumber 是**跨已完成 Resolution 的 cosmetic lifetime**。二者不合并。

<!-- REVIEW FIX 2026-09-12:
这里删除了额外 PendingSelectionRequestGeneration 的设计。原因是 player-facing selection boundary 已经由
BattleId + SelectionBoundaryRevision 提供单调、第一次点击前就存在、DirectBaseline 也可用的 exact identity。
再加一套 request counter 会形成重复 authority，还需要额外处理 Resolver replacement ABA，没有实际收益。
-->

## 3. 首版范围

### 3.1 唯一 detached / NonBlocking 候选

```text
DamageNumber only
```

DamageNumber 使用 committed/frozen Damage payload 的 `IncomingDamage` 作为展示值，保持当前语义，不暗中改成实际 HP loss。

有效的“完全被 Block 吸收”仍可显示 DamageNumber：

```text
IncomingDamage > 0
HPDamage == 0
BlockedDamage > 0
```

真正 `IncomingDamage == 0` 的 no-op 不作为首版 DamageNumber 新语义；沿用当前 Damage payload historical contract，不为了 G8 扩展历史事实模型。

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

受击 opacity flash 也不 detach。Detached Damage 成功路径不再借用正式 target opacity；旧 Blocking fallback 仍保留现有正式受击视觉。

### 3.3 Combatant animation cue 不属于 DamageNumber job

当前 Damage playback 已承担 target `Hit` 与部分 source `Attack` 动画触发。G8 不能因为跳过旧 `BeginNativeDamagePresentation()` 而丢失这些已有视觉行为。

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

C 不属于 `DetachedDamageInstance`，也不参与 Record completion。它只能请求 combatant presentation animation，不得写 HP/Block、opacity、ViewModel、card ownership 或 input。

### 3.4 体验预期

首版不人为拖长 DamageNumber，也不缩短/跳过仍 Blocking 的 card tail 来制造演示。

如果后续 Blocking 卡牌动画比 DamageNumber 更长，某些牌实际看不到跨牌数字重叠是合法结果。验收记录真实 input-ready 时间点和数字是否仍存活，只报告实际收益。

### 3.5 Non-goal：卡牌 Presentation overlap 与 buffered input 延后到 G9

G8 **不**把 `CardPlayed`、`CardZoneChanged`、PlayArea → destination 或其它 card-tail Presentation 改造成 NonBlocking，也不把当前 FastInput 的 catch-up/Skip 机制改造成通用 buffered card-input scheduler。

因此在 G8 范围内，若上一张牌仍存在真实 Blocking card presentation：

```text
click next card
→ HasSkippablePresentationDelay() == true
→ ordinary SkipPresentation / catch-up
→ exact next-tick retry
```

仍属于允许且预期的行为。**“快速点击下一张牌导致上一张 Blocking 卡牌动画被收尾/跳过”不是 G8 bug。**

G8 明确不要求、也不授权以下体验：

```text
A 的完整 CardPlayed / CardZoneChanged 动画继续播放
+ B 已可被选择/确认/打出
+ A/B 拥有可重叠的独立卡牌视觉生命周期
+ 玩家在 A 尚未到安全 Gameplay boundary 时提前缓存 B 的出牌意图
```

上述能力统一延后到后续 **G9 — Buffered Card Input + Detached Card Presentation** 独立设计。G9 可以在不并发 authoritative Gameplay 的前提下设计：

```text
提前点击 B
→ capture exact buffered intent
→ 不为了接受 B 而 Skip A 的无害 card visual
→ A Gameplay 到达安全 boundary
→ exact revalidate B 的 session / BattleId / revision / RuntimeId / legality
→ 合法后再提交 B Gameplay
```

G9 若进一步允许 `CardPlayed` / `CardZoneChanged` visual tail 跨 Resolution 或跨下一次输入存活，必须独立定义 card Widget ownership、Hand/PlayArea reconciliation、RuntimeId identity、stale visual cleanup、replacement 与 GC 合同；这些均不属于 G8 seal 的 acceptance。

## 4. 权威所有权与身份

### 4.1 Presentation session authority：由 Controller 唯一 mint

Session authority 属于 `UBattlePresentationController`，而不是 Presenter、HUD 或 ViewModel。

理由：Controller 已实际拥有 active envelope、playback queue、CurrentBattleId、Skip、reconcile、presentation unavailable 和 playback generation。Presentation-owned session 的建立/失效必须和这些状态切换保持同一权威边界。

#### 4.1.1 Token 必须防 Controller replacement ABA

仅使用：

```text
BattleId + per-instance SessionGeneration
```

不够，因为 Controller UObject replacement 后新实例若重新从 `1` 开始 mint，会出现旧/新 token 数值相同的 ABA。

首版冻结 identity 为：

```text
PresentationSessionToken =
  BattleId
  + ControllerEpoch
  + PresentationSessionGeneration
```

合同：

```text
ControllerEpoch
- 由 UBattlePresentationController 类级 authority mint
- process lifetime 内单调递增且不得复用
- Controller UObject replacement 必须获得新 epoch
- 不来自 UObject 指针、时间戳、TMap 顺序或内存地址

PresentationSessionGeneration
- 由当前 Controller instance 单调 mint
- 同一 ControllerEpoch 内不得复用
```

允许使用等价实现，只要可以证明：**旧 Controller 产生的任何 PresentationSessionToken 永远不可能与 replacement Controller 的 token 相等。**

HUD / ViewModel readiness evaluator / detached Damage owner 只消费 Controller 给出的 token，不各自推导“当前 session”。

以下事件必须使旧 Session 失效：

```text
Controller 初始化新的有效 Presentation binding
battle replacement
HUD/Controller binding replacement
DirectBaseline authority transition
presentation unavailable transition
Controller shutdown
```

以下事件**本身不得**使 SessionToken 失效：

```text
ordinary SkipPresentation / backlog catch-up
ordinary active-envelope historical/reducer reconcile
Record/group timeout 后按现有 active-envelope recovery contract reconcile
正常 Envelope completion / FinalSnapshot publication
G8 runtime feature enable/disable（只要 Controller/Battle/Widget/authority binding 未变化）
```

理由：这些事件改变的是 chronological playback state 或 feature policy，不是 Presentation authority identity。

只有当 recovery **升级为** HUD/Controller/battle replacement、DirectBaseline transition 或 unavailable transition 时，才通过对应 authority transition 使旧 Session 失效；不定义额外的“recovery session replacement”类别。

因此必须区分：

```text
cancel current-session cosmetics
!=
invalidate PresentationSessionToken

feature disabled
!=
authority replaced
```

真正失效时顺序固定为：

```text
invalidate old session
→ cancel/cleanup old private cosmetics
→ establish new session（若仍存在 PresentationOwned authority）
```

<!-- REVIEW FIX 2026-09-12:
补充“runtime disable 不换 Session”。PresentationSessionToken 表示 Presentation authority/binding 身份，
不是 G8 feature 开关身份。若 Controller/Battle/Widget 都没换，仅关闭 detached 功能却 mint 新 Session，
会让仍然合法的 deferred/readiness credential 无理由 stale。
-->

#### 4.1.2 `SetWidget()` / binding replacement 是明确迁移热点

当前 Blocking 模型允许 Controller 在 replacement 时取消 active tracked playback、安装新 Widget，并通过现有 `SkipPresentation()` 语义 catch up in-flight/backlog。G8 后旧 Widget 还可能拥有合法 detached cosmetics，因此 replacement 不得先覆盖 `Widget` 再尝试“清当前 cosmetic”。

目标顺序必须等价于：

```text
capture old Widget + old SessionToken + old ActivePlaybackToken
→ invalidate old SessionToken
→ exact cleanup old Widget 上属于 old session 的 detached cosmetics
→ cancel exact old Widget tracked Blocking playback（若有）
→ retire Controller old playback ownership：
   - cancel old active timeout
   - bWaitingForCompletion = false
   - ActivePlaybackToken = invalid/default
   - old LocalPlaybackGeneration / equivalent playback guard 继续保证 stale callback 被拒绝
→ detach old Widget
→ install new Widget
→ mint/establish NEW SessionToken for the new PresentationOwned binding
→ 若存在 in-flight/backlog，保留现有 SkipPresentation/catch-up 语义，但 catch-up 不得再把 old playback token 发送给 new Widget
→ reach exact authoritative displayed surface
→ 按 authoritative current surface 重建 readiness
```

硬合同：

```text
旧 Widget 所有的 FPresentationPlaybackToken
不得在 Widget replacement 后传递给 new Widget 的 CancelTrackedPresentationPlayback / finish/cancel surface。
```

允许实现一个用途明确的 Controller helper，例如：

```cpp
void RetireActivePlaybackForWidgetReplacement(
    UBattleHUDWidgetBase* ExpectedOldWidget);
```

或等价内部流程。关键不是 helper 名称，而是**先取消 old owner，再退休 Controller 的 old playback ownership，最后才切到 new owner**。

新 Session 的建立点固定在：

```text
new Widget binding 已安装
+
任何可能同步 RefreshLiveInputBindingsIfCaughtUp() 的 catch-up 之前
```

这不代表“新 Session 已经 caught up”。Session 只表示 authority/binding identity；是否允许输入继续由 exact displayed revision、active/backlog 状态和 readiness evaluator 决定。

如果没有 in-flight/backlog，新 session 可在新 binding 建立后立即参与 readiness；如果需要 catch-up，则**不得在 catch-up 到达 exact authoritative displayed surface 前 grant readiness**。

旧 Widget 的 cleanup 失败只能造成 cosmetic 丢弃/诊断，不得让旧 callback 获得新 session authority。

<!-- REVIEW FIX 2026-09-12:
replacement 不只要处理 SessionToken，还必须处理旧 FPresentationPlaybackToken 的 owner lifetime。
当前 SetWidget() 先在 PreviousWidget cancel，随后切换 Widget 再 Skip；若 Controller 仍处于 waiting 状态，
CancelActivePlaybackUnit() 可能把 old token 再发给 new Widget。首版明确先 retire old playback ownership，
禁止依赖 new Widget 的 token mismatch 来吸收错误跨-owner call。
-->

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

DetachedDamageInstance =
  Token
  + SourceFinalStateRevision      // diagnostic provenance only
  + FrozenDamageVisualSpec
  + GC-tracked transient Widget
  + Elapsed
  + LifecycleState { Prepared, Active }
```

`LocalDamageVisualGeneration` 由当前 detached owner 单调分配，不来自时间、指针、容器顺序或 Widget 地址。同一 Record 如果在提交前重新尝试 prepare，也必须分配新的 generation。

该 token 与 `FPresentationPlaybackToken` 类型分离；接口不得互相接受。

### 4.4 容器、GC 与 finite lifetime

首版优先使用 GC 可达的 deterministic small container，例如：

```text
UPROPERTY(Transient)
TArray<FDetachedDamageInstance> DetachedDamageInstances;
```

首版 defensive ceiling 预计不超过 32，线性 exact-token lookup 足够，不需要依赖 TMap iteration order。

Transient Widget 必须由 `UPROPERTY` 可达容器强持有。DamageNumber 生命周期由 HUD 现有 `NativeTick()` 统一推进，不为每个实例创建独立 Timer/Ticker callback。

首版**不额外定义 HardTimeout**。原因是 Active instance 在同一个 `NativeTick()` owner 中以 finite positive `VisualDuration` 直接结束并 exact cleanup；在此模型下 `HardTimeout > VisualDuration` 永远不可达，只会增加一份无效状态。

这里的 finite lifetime 是 **finite active-tick lifetime**。任何会使 owner 停止继续 `NativeTick()` 的 HUD deactivation / destruction / replacement 边界，必须在 tick 停止前或 destruction cleanup 中同步清掉全部 detached instances。禁止出现“Widget 仍被引用但已永久不 tick，DamageNumber 因而永久存活”的状态。

如果未来 DamageNumber completion 改成依赖外部异步动画回调、SequencePlayer 或其它可能永久不完成的 owner，再通过独立设计引入 watchdog/hard-timeout；不在 G8 首版提前建模。

## 5. Damage historical reducer：先提取单一完整纯规则

当前旧 Damage Widget 对 historical Before/After 做了较完整校验，但 Controller 的 Damage `ApplyRecordToWorkingSnapshot()` 相对更薄。G8 不能复制一份“detached Damage validator”。

实现 detached infrastructure 前，先提取唯一 historical Damage reducer/validator，概念接口：

```cpp
bool TryApplyDamageRecord(
    FPresentationStateSnapshot& Snapshot,
    const FDamagePresentationPayload& Damage);
```

### 5.1 完整 historical contract

首版不再使用“至少校验”这种开放表述。以下均属于同一 historical reducer contract，Blocking 与 detached preflight 必须共享：

```text
identity / enum:
- exact TargetPresentationId resolves to exactly one combatant in Snapshot
- SourcePresentationId is None or resolves to a known participant
- DamageKind is one of the currently supported committed kinds: Attack / Effect

before-state:
- Snapshot target MaxHP > 0
- IncomingDamage > 0
- HPBefore > 0
- HPBefore <= Snapshot target MaxHP
- BlockBefore >= 0
- HPBefore == Snapshot.Target.HP
- BlockBefore == Snapshot.Target.Block

after-state:
- 0 <= HPAfter <= HPBefore
- 0 <= BlockAfter <= BlockBefore
- BlockedDamage >= 0
- HPDamage >= 0
- BlockedDamage == BlockBefore - BlockAfter
- HPDamage == HPBefore - HPAfter

accounting:
- AccountedDamage = BlockedDamage + HPDamage
- AccountedDamage > 0
- AccountedDamage <= IncomingDamage
```

成功时只修改传入 snapshot：

```text
Target.HP = HPAfter
Target.Block = BlockAfter
Target.bDead = (HPAfter <= 0)
```

失败必须无副作用。

如果未来 Damage model 新增 damage kind、overkill accounting、特殊 shield 或其它语义，必须先修改这一份 historical contract，再修改对应 producer/test；不得只在 detached visual 侧放宽。

### 5.2 Visual eligibility 与 historical validity 严格分离

以下只属于 detached visual eligibility，不得复制进 `TryApplyDamageRecord()`：

```text
PresentationOwned mode + exact current SessionToken
HUD/TransientVFXHost exists
Target presentation surface exists
Widget class/resource exists
geometry finite/non-zero
AbsoluteToLocal conversion valid
VisualDuration 配置为 finite positive
Detached instance capacity 未超过 defensive ceiling
```

因此：

```text
historical invalid
→ active-envelope recovery

historical valid + visual ineligible
→ old Blocking Damage fallback

DirectBaseline/no-history mode
→ detached visual path 不存在
→ 沿该模式现有 direct delivery/input contract
```

不能把“缺 Widget / 几何无效 / 当前是 DirectBaseline”解释为 committed Damage fact 无效。

### 5.3 Blocking 与 detached 必须共享同一 reducer

旧 Blocking `CompleteActiveRecord()` 和 G8 detached preflight 必须复用同一规则，避免出现：

```text
Blocking Damage 合法
Detached Damage 非法
```

或反过来的双份语义。

G8-A 应先让**现有 Blocking Damage path**使用该 reducer 并通过回归测试，再接入 production-disabled detached infrastructure。不要等到 G8-C 才顺手重构 Damage semantics。

G8 preflight 使用 snapshot copy：

```text
CandidateSnapshot = WorkingPresentationSnapshot
TryApplyDamageRecord(CandidateSnapshot, Payload)
```

成功后正式 commit 直接把已经校验的 CandidateSnapshot 作为新的 working state；**不得再次执行第二遍 Damage reducer**。这里的 “reducer exactly once” 指同一份规则产生一次正式 state transition，而不是先 mutate production state 再重复一次。

## 6. Detached Damage 启动事务

保留现有 `PlayPresentationRecord(...)` bool Blocking 合同。G8 只在 Controller 的 Damage Record dispatch 前增加一个可完全回退的 detached transaction。

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
Damage + G8 enabled + PresentationOwned mode
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
7a. G8-C only：CompatibilityDebtSeconds += LegacyDamageBlockingDuration
    - 这一步属于 formal commit bookkeeping，不属于 cosmetic completion
    - 必须发生在 ViewModel publication 之前
8. publish formal working snapshot to ViewModel
9. post-publication exact recheck：
   publication 可能同步触发 Skip/replacement/unavailable/reconcile
10. exact prepared visual 仍有效 → activate DamageNumber
11. exact session/Record 仍允许 → fire Damage combatant cues
12. increment Record cursor / dispatch next Record
```

G8-D 删除 7a；正式 NonBlocking 后 transaction 不再创建任何 legacy timing debt。

<!-- REVIEW FIX 2026-09-12:
明确把 G8-C debt 登记放在 formal commit 后、ViewModel publication 前。
原因是 publication 可能同步触发 Skip。若先 publication、Skip 把 debt 清零、随后 transaction 才 +=D，
就会在已经 Skip 完成后“复活”一笔旧 Damage 等待，破坏 G8-C 与旧 Blocking Skip 的 parity。
-->

### 6.3 失败分界

```text
提交前 detached decline
→ rollback hidden instance
→ old Blocking Damage playback

historical payload / reducer invalid
→ existing active-envelope recovery
→ same PresentationSessionToken remains authoritative
→ recovery 可清理必要 cosmetic，但不得制造 session 空窗
→ preserve later backlog according to existing recovery contract
→ NOT animation fallback

提交后 prepared visual stale / activation failure
→ clean exact private visual only
→ Record 已 reduced，不 fallback，不 replay reducer
→ G8-C 已登记的 compatibility debt 仍代表该 committed Damage，不因 visual failure 自动删除

同步 ordinary Skip/backlog catch-up
→ same PresentationSessionToken 保持有效
→ old prepared/active cosmetic 可按 Skip policy 被 exact cancel
→ G8-C staging compatibility debt 按旧 Skip 语义清零
→ transaction 依 exact Record/cursor recheck 停止或继续
→ 不因 ordinary Skip 人为制造 replacement session

同步 HUD/Controller/battle replacement、unavailable 或 DirectBaseline authority transition
→ old session/token invalid
→ stale transaction stops
→ replacement/transition 后不得执行旧 fallback
```

普通 active-envelope reconcile 若随后同步 `StartNextEnvelope()`，下一 Envelope 继续使用同一有效 SessionToken；不得出现“先 invalidate，下一 Envelope 已开始，之后才 mint replacement session”的中间状态。

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
DamageNumber finish / cancel
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

## 8. Interaction readiness：以真实交互面与 authority source 为核心

### 8.1 Readiness 不是 cosmetic job 状态

最终 G8-D 后：

```text
DetachedDamageInstances.Num() > 0
```

本身既不能锁 input，也不能解锁 input。

Input readiness 只取决于：

```text
当前 authoritative Gameplay request mode
+ 当前 readiness authority source
+ exact displayed/read-facing revision or selection boundary
+ exact current decision surface identity
+ required Blocking Presentation/recovery 是否完成
+ G8-C staging-only compatibility debt 是否已经服务/被合法 Skip
+ terminal/unavailable 状态
```

不建立通用 barrier registry。

### 8.2 真实 mode 模型

G8-B 不再把当前系统简化成只有 `Normal / PendingSelection / TargetChoice`。首版 readiness 必须覆盖以下真实交互面：

```text
NormalPlayerTurn
CardReadyToConfirm
CardTargetChoice
PendingCardSelection
Resolving
Terminal
Unavailable
```

其中 PendingCardSelection 是 Gameplay authoritative selection lifecycle，不要求把它新增成 `EBattleHUDInteractionState` 枚举值。

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

### 8.3 Readiness authority source

首版 readiness 必须显式区分两种合法 authority source：

```text
A. PresentationOwned
   - 存在有效 UBattlePresentationController
   - display chronology 由 Controller 拥有
   - readiness credential 必须携带 exact PresentationSessionToken

B. DirectBaseline
   - 只用于 intentional no-history/direct delivery
   - 典型来源：Presenter 明确禁用 committed presentation，或 battle recording 明确关闭
   - 不存在 PresentationSessionToken
   - ViewModel 直接消费 exact frozen baseline/read edge
   - detached Damage path 不启用
```

不得为了统一接口而给 DirectBaseline 伪造 `SessionToken = 0`、dummy epoch 或虚假 Controller session。

以下不属于合法 DirectBaseline：

```text
Presentation 本应启用
+ Presentation available
+ recording enabled
+ Controller 创建或 Initialize 失败
```

该情况定义为 **Presentation initialization failure**：

```text
→ 不允许静默 SetPresentationDisplayOwned(false) 后恢复普通 live input
→ 进入 PresentationUnavailable / 等价 fail-safe
→ battle request 保持禁止
→ 暴露明确诊断
```

因此 G8 实现前必须修正当前 Presenter 的 silent fallback。该修复属于 authority contract prerequisite，不是 G8 detached 功能本身。

`presentation unavailable` 不是 DirectBaseline 正常可交互模式；它继续按 unavailable contract 锁定 battle request。

### 8.4 PendingSelection：直接复用 interactive boundary 作为 exact request identity

Pending card selection 已经是 Gameplay-authoritative lifecycle，但首版**不新增** `PendingSelectionRequestGeneration`。Player-facing request identity 直接冻结为：

```text
PendingSelectionRequestIdentity =
  BattleId
  + SelectionBoundaryRevision
```

建议使用明确的小型 value type，避免写路径只携带 revision：

```cpp
struct FPendingSelectionRequestIdentity
{
    int64 BattleId = 0;
    int64 SelectionBoundaryRevision = 0;

    bool IsValid() const
    {
        return BattleId > 0 && SelectionBoundaryRevision > 0;
    }
};
```

具体类型名可以调整，但所有 read/transient/submit/cancel surface 必须传递或等价校验**完整二元 identity**，不得把 `SelectionBoundaryRevision` 单独当成全局唯一值。

原因是当前 player selection 创建顺序已经保证：

```text
DeferredSelectionAction
→ AdvancePresentationAtInteractiveSelectionBoundary()
   → AdvanceStateRevision()
   → Writer.SelectionBoundaryRevision = StateRevision
→ enqueue USelectionRequestAction with that writer
→ USelectionResolver::BeginSelection()
```

因此 exact `SelectionBoundaryRevision`：

```text
- 在第一次 pending-selection click 之前已经存在
- 同一 BattleId 内由 StateRevision 单调前进，不应复用
- intentional DirectBaseline/no-history 路径同样存在
- 与当前 displayed/read edge 天然可比较
- 不依赖 Presentation SelectionGeneration
```

实现要求：

```text
USelectionRequestAction / USelectionResolver
- 当前 active player-facing pending request 必须保存 exact PendingSelectionRequestIdentity
- BeginSelection 成功时冻结该 identity
- resolve / legal cancel / fault cleanup 后清除

FPendingCardSelectionReadView
- 暴露 exact PendingSelectionRequestIdentity（或明确的 BattleId + SelectionBoundaryRevision 字段）

ViewModel transient selected-card set
- 必须记录自己属于哪个 exact PendingSelectionRequestIdentity
- identity 变化时先清空旧 transient selected set
- 不再只用 SelectionSource + RequiredCount + CandidateRuntimeIds 判断“是不是同一个请求”

SubmitPendingCardSelection / SubmitPendingSelectionCancel
- 必须携带 ExpectedPendingSelectionRequestIdentity
- Gameplay submit boundary 先 exact-match BattleId + SelectionBoundaryRevision，再解析 RuntimeIds / cancel policy
- stale BattleId 或 stale boundary 的 submit/cancel 都必须拒绝且不得作用于下一请求
```

现有 Presentation `SelectionGeneration` 与该 identity 职责不同：

```text
BattleId + SelectionBoundaryRevision
→ Gameplay/read pending request exact identity
→ request 第一次 click 前就存在

SelectionGeneration
→ SelectionArea presentation ownership identity
→ 可以在第一次用户选择后才建立
```

因此第一次 pending-selection click **不得**要求尚不存在的 `SelectionGeneration`。Presentation lifecycle 一旦建立，后续 visual transition / Confirm 继续附加 exact `SelectionGeneration` 校验，但它不能替代 Gameplay/read request boundary。

两个连续 request 即使：

```text
SelectionSource 相同
RequiredCount 相同
CandidateRuntimeIds 完全相同
```

只要 `PendingSelectionRequestIdentity` 不同，就必须视为两个不同请求；旧 transient selection、旧 callback、旧 submit 都不得跨 identity 复用。

<!-- REVIEW FIX 2026-09-12:
上一版为 PendingSelection 新增了独立 RequestGeneration。这里改为直接复用 BattleId + SelectionBoundaryRevision。
理由有两点：第一，interactive boundary 本来就在 BeginSelection 之前创建，因此没有“第一次点击时 identity 尚不存在”的问题；
第二，额外 generation 会重复表达同一个 Gameplay/read 边界，并引入额外 counter、Resolver replacement ABA 与同步成本。
本次进一步把 BattleId + SelectionBoundaryRevision 固定成一个完整 value identity；read、transient UI set、submit/cancel
必须使用同一二元 identity，避免实现阶段只传 revision 而在 battle replacement 后弱化 stale-write 防护。
-->

### 8.5 Readiness credential

首版使用已有/最小身份，不建立万能 `SurfaceGeneration`。

PresentationOwned：

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
  SessionToken
  + exact PendingSelectionRequestIdentity
  + exact pending read surface
  + optional existing SelectionGeneration when presentation selection lifecycle has already begun
```

DirectBaseline：

```text
NormalPlayerTurn / CardReadyToConfirm / CardTargetChoice:
  BattleId + exact StateRevision
  + 对应 exact SelectedCardRuntimeId / InteractionState / LegalTargets（按 mode）
  + bPresentationDisplayOwned == false

PendingCardSelection:
  exact PendingSelectionRequestIdentity
  + exact direct read surface
  + optional existing SelectionGeneration when presentation selection lifecycle has already begun
```

**G8 首版不新增 `TargetChoiceGeneration`。** 当前 target request 是同步决策面，没有具体的跨帧 target callback 需要防 ABA。只有未来真实新增 deferred target callback 时，才以独立设计增加 generation。

新 Battle、ControllerEpoch replacement、PresentationOwned session replacement、authority source replacement、revision/boundary replacement、selected-card replacement都会使对应旧 credential 失效。普通 active-envelope reconcile 不替换 SessionToken，但其 displayed revision/read surface 变化仍会让不匹配的旧 credential stale。

### 8.6 Mode-specific grant

`RefreshLiveInputBindingsIfCaughtUp()` 当前会回到普通 PlayerTurn Idle，因此不能作为所有模式共同 finalizer。统一的是 evaluator，不是 grant 动作。

```text
EvaluateInteractionReadiness(CurrentMode, AuthoritySource)
  |
  +-- NormalPlayerTurn
  |     → exact baseline/read/VM guard
  |     → existing normal live-binding refresh
  |
  +-- CardReadyToConfirm
  |     → preserve SelectedCardRuntimeId
  |     → allow Confirm / Cancel
  |     → allow switching to another currently legal Hand card and rebuild decision surface
  |     → EndTurn follows existing baseline contract
  |
  +-- CardTargetChoice
  |     → preserve SelectedCardRuntimeId + LegalTargets
  |     → allow target choose / cancel
  |     → allow switching to another currently legal Hand card and rebuild decision surface
  |     → EndTurn explicitly forbidden after prerequisite bugfix
  |
  +-- PendingCardSelection
        → exact PendingSelectionRequestIdentity / read guard
        → if SelectionGeneration exists, also validate exact presentation ownership generation
        → preserve pending-selection transient set only for the same exact identity
        → only Select/Deselect/Confirm/Cancel for that request
```

切换另一张 Hand card 属于**重建当前 card decision surface**，不是“同时开放第二个 surface”。旧 selected-card credential 立即 stale，新卡必须重新通过当前 exact live binding/query。

不得通过某一个 surface ready 顺便开放另一个不相关 surface 的请求。

### 8.7 Mode matrix

| 当前交互面 | 可开放 | 必须禁止 |
|---|---|---|
| Normal PlayerTurn ready | 合法选牌/出牌、合法 EndTurn | Query 拒绝请求 |
| Card ReadyToConfirm | 当前牌 Confirm/Cancel；切换另一张合法 Hand card；EndTurn 依既有行为 | target bypass、旧 selected-card continuation |
| Card TargetChoice | 合法目标选择/取消；切换另一张合法 Hand card | EndTurn、target bypass、旧 selected-card continuation |
| Pending Selection surface 未 ready | 无选择提交 | 普通出牌、Confirm、EndTurn |
| Pending Selection exact surface ready | 选/取消选、合法 Confirm/Cancel | 普通出牌、EndTurn、旧 identity submit/cancel |
| Resolving | 无普通 battle request | 普通出牌、EndTurn、旧 surface continuation |
| Terminal / unavailable | 既有恢复/终局控件 | battle request |

无可打牌时仍可能允许 EndTurn，所以 NormalPlayerTurn readiness 不能以“存在可打牌”为条件。

### 8.8 G8-B 前置修复：TargetChoice / EndTurn 现有行为

当前 `SelectCardByRuntimeId()` 进入 `ChoosingTarget` 后仍可能计算 `bCanEndTurn=true`，而 `RequestEndTurn()` / `CanAcceptSelectionInput()` 没有显式排除 `ChoosingTarget`。因此当前代码可能允许：

```text
ChoosingTarget
→ EndTurn
```

本设计把这定义为**已有 input-routing bug**，不是 G8 新行为。

在 G8-B shadow parity 建立 baseline 之前，必须以独立、明确授权的 bugfix 修复并单独验证：

```text
ChoosingTarget
→ target choose / cancel / switch to another legal Hand card
→ EndTurn disabled/rejected
```

修复应同时覆盖 ViewModel request boundary 与 HUD button enablement，不能只隐藏按钮，也不得误删当前已有的“切换手牌重建 decision surface”能力。

G8-B shadow parity 的比较基线是**该 bugfix 之后的行为**。禁止让 shadow evaluator 自己产生 divergence 后再把 divergence 解释为“顺便修 bug”。

`CardReadyToConfirm` 的 EndTurn 行为首版仍按现有 contract 保留；若以后希望改变，必须独立设计/测试，不捎带进 G8。

### 8.9 Shadow readiness gate

G8-B 不提前解锁。新 evaluator 先只产生 shadow result，并与经过 8.8 前置修复后的实际 input/mode 逐边界比较：

```text
任何未解释 divergence
→ G8-B FAIL
→ 不进入 G8-C
```

只有明确记录、独立授权并已经纳入新 baseline 的行为修复，才允许与更早历史实现不同。

Shadow parity 必须覆盖 PresentationOwned 与合法 DirectBaseline 两种 authority source；DirectBaseline 不得因为“无 SessionToken”被误判为永不 ready。

Controller initialization failure 的 fail-safe 不参加 DirectBaseline parity；它必须保持 unavailable/input locked。

## 9. FastInput：deferred click 使用 ExpectedCatchUpRevision

`HasActiveNativePresentation()` 继续保持当前语义：它只表示真实、tracked Native Record/Group/card playback 是否存在，**不把 detached DamageNumber 或 G8-C compatibility debt 塞入该查询。**

但 FastInput 是否允许调用 `SkipPresentation()` 不能再只看 `HasActiveNativePresentation()`。首版增加一个 Controller-authoritative 概念，例如：

```cpp
bool HasSkippablePresentationDelay() const;
```

等价语义：

```text
true:
- 存在可被 Skip 的 active Blocking Presentation
- 或存在需要 Skip/collapse 的 sealed/backlog chronology
- 或 G8-C 正在服务 CompatibilityDebtSeconds > 0 的 staging-only wait

false:
- 只有 detached DamageNumber cosmetic tail
- 已经没有 chronology/backlog/debt 需要 catch-up
- DirectBaseline 没有 Controller-owned presentation delay
```

FastInput 的 catch-up gate 使用：

```text
HasSkippablePresentationDelay()
+ Outcome == None
+ 当前输入处于对应 Resolving / staging-wait locked surface
```

而不是把 `HasActiveNativePresentation()` 当成唯一条件。

只有 cosmetic tail 存在时：

```text
HasActiveNativePresentation() == false
HasSkippablePresentationDelay() == false
→ 正常点击不能触发 SkipPresentation
→ 不能因为点击而清空 DamageNumber
```

G8-C 只剩 compatibility debt 时：

```text
HasActiveNativePresentation() == false
CompatibilityDebtSeconds > 0
HasSkippablePresentationDelay() == true
→ 玩家 fast click 仍可沿旧 Damage timer 语义 Skip
→ Skip 清 debt
→ next-tick exact retry
```

<!-- REVIEW FIX 2026-09-12:
引入 HasSkippablePresentationDelay 概念而不修改 HasActiveNativePresentation 的含义。
原因是 G8-C 会出现“没有真实 Native Widget playback，但仍在等待 legacy compatibility debt”的阶段；
如果 FastInput 仍只看 HasActiveNativePresentation，旧 Damage timer 原本能被 click→Skip 跳过，而 G8-C debt-only 阶段反而不能 Skip，破坏 staging parity。
同时不能把 detached cosmetic 计入该 gate，否则 G8-D 又会因为纯视觉尾巴错误触发 Skip。
-->

仍存在真正 Blocking Presentation 或 G8-C skippable debt 时，既有 fast catch-up 可保留；但首版把当前单一 `PendingFastCardRuntimeId` 提升为 exact deferred request：

```text
PendingFastCardRequest =
  PresentationSessionToken
  + BattleId
  + ExpectedCatchUpRevision
  + RuntimeId
```

FastInput deferred request 只存在于 PresentationOwned 模式；DirectBaseline 没有需要 `SkipPresentation()` catch-up 的 Controller chronology，也不伪造 SessionToken。

`ExpectedCatchUpRevision` 不是点击发生时的旧 displayed revision，而是点击被接受为 fast catch-up 时捕获的**目标最新 frozen baseline revision**。

示例：

```text
click 时 displayed revision = 100
latest sealed frozen baseline = 102
→ ExpectedCatchUpRevision = 102
```

ordinary FastInput Skip 的关键合同：

```text
click captures SessionToken S
→ HasSkippablePresentationDelay() == true
→ SkipPresentation / backlog collapse within same Controller/Battle/binding
→ SessionToken remains S
→ current-session detached cosmetics may be canceled by Skip policy
→ G8-C compatibility debt for the skipped chronology is cleared by Skip policy
→ next-tick retry validates S + ExpectedCatchUpRevision
```

因此 ordinary Skip **不得**仅为了 catch-up 而 mint 新 SessionToken，也不得让 G8-C staging debt 阻止一个在旧 Blocking path 中本来会被 Skip 立即 catch-up 的 deferred request。

Skip 后 retry 仅在以下条件全部满足时发生：

```text
same exact SessionToken（含 ControllerEpoch）
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

如果 Skip 过程中发生了真正的 Controller/HUD/battle/authority replacement：

```text
SessionToken changes or disappears
→ deferred click stale
→ no replay
```

旧点击不得被带入更新的 Gameplay state，也不得在新 ControllerEpoch、新 PendingSelection identity 或新 target surface 重放。

## 10. TransientVFXHost 与正式 HUD 隔离

运行时在 Native HUD 根 Canvas 下创建独立 `TransientVFXHost`，不复用 Hand、PlayArea、SelectionArea，也不要求修改 production `.uasset`。

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

随后冻结 Host-local 起点。激活后只更新自己的 translation/opacity；绝不继续追踪 target Widget，也不：

```text
restore target opacity
write formal HP / Block
write ViewModel
change Hand/Card ownership
```

Viewport/DPI/Host geometry 发生无法安全重映射的改变时，首版直接取消当前 session 的 cosmetic instances。普通 historical dirty publication、FinalSnapshot 和 Hand reconcile 不得清空 Host。

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
→ Elapsed >= finite VisualDuration
   → exact idempotent cleanup
```

Prepared instance 不得进入可见 tick，直到 Controller 完成正式 publication 后显式 Activate。

首版不使用 per-instance Timer/Ticker，也不使用额外 HardTimeout。Session cleanup、Widget deactivation/destruction、replacement cleanup 是 VisualDuration 之外的强制清理边界。

## 11. 资源合同与恢复

### 11.1 有限生命周期是硬合同

每个 DamageNumber 必须有：

```text
finite positive VisualDuration
idempotent exact cleanup
session-wide cleanup
Widget deactivation/destruction cleanup
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

### 11.3 Recovery / Skip policy

| 事件 | Detached Damage | Session / authoritative/input lane |
|---|---|---|
| 正常 Record/Envelope/FinalSnapshot 完成 | 合法 instance 继续 | 正常 chronological/readiness；Session 保持 |
| 单项 finish | 只清 exact instance | 无输入副作用 |
| ordinary Global Skip / backlog collapse | exact cancel 当前 session cosmetic；G8-C compatibility debt 清零 | **保持同一 SessionToken**；完成 sealed catch-up；FastInput 可按同一 token retry |
| G8-C debt-only fast Skip | 按 Skip policy 清当前 session cosmetic；debt 清零 | **保持同一 SessionToken**；即使无 ActiveNativePresentation 也可通过 skippable-delay gate catch up |
| ordinary active-envelope historical/reducer reconcile | exact cleanup recovery 需要取消的 cosmetic | **保持同一 SessionToken**；保留后续 backlog；可同步进入下一 Envelope |
| recovery 升级为 HUD/Controller/battle replacement | old-session cosmetic 全清 | 由 replacement contract invalidate old session 并建立新 authority |
| HUD replacement | 先失效 old session；old Widget exact cleanup/cancel；**retire old playback owner/token**；安装 new Widget；catch-up 前建立 new SessionToken | new Widget 永不接收 old playback token；新 binding/session 下完成必要 catch-up，再重建 readiness |
| Controller replacement | old ControllerEpoch 永久失效；旧 cosmetic 全清 | 新 ControllerEpoch/session 重建 readiness |
| battle replacement | 全清旧 session | 新 battle 建立新 authority/readiness |
| intentional DirectBaseline mode transition | invalidate old Presentation session + 全清 detached cosmetic | 切换为 DirectBaseline authority；不建立伪 SessionToken |
| Controller initialization failure | 无 detached production session 可继续 | 进入 PresentationUnavailable/等价 fail-safe；不得 silent DirectBaseline |
| G8 runtime feature disable（authority 未变化） | clear all detached cosmetic；停止新 detached activation；清 staging debt | **保持同一 PresentationSessionToken**；后续 Damage 回 Blocking path；exact readiness recheck |
| presentation unavailable | invalidate/清旧 session cosmetic | unavailable contract，禁止 battle request |
| terminal | 全清 | 正式终局优先 |

Cosmetic finish/cancel 不触发 Gameplay `ResolutionFault`，也不把整个 Presentation 标记 unavailable。损坏 committed Envelope 仍按现有 historical failure contract 处理。

## 12. 分阶段实施

### 12.0 明确的 prerequisite 行为修复与 regression baseline

G8 实现过程中有两项**有意改变当前 G0–G7 实际行为**的 prerequisite fix：

```text
P1. ChoosingTarget / EndTurn routing fix
    ChoosingTarget 时 EndTurn 从“当前可能被允许”改为 request boundary + HUD 双重禁止

P2. Controller initialization failure fail-safe
    本应启用 committed Presentation 但 Controller Initialize 失败时，
    从“silent sessionless DirectBaseline fallback”改为 PresentationUnavailable / input locked
```

这两项必须独立测试并记录；完成后，它们成为新的 regression baseline。

后文提到的：

```text
G0–G7 regression
```

统一解释为：

> **除 P1/P2 这两项已明确授权的 prerequisite 行为修复外，其余 sealed G0–G7 外部合同不得被 G8 改变。**

<!-- REVIEW FIX 2026-09-12:
补充这一节是为了消除验收语义冲突。文档一方面要求 G0–G7 regression，另一方面又明确要求修复 TargetChoice/EndTurn
和 Controller-init fallback；如果不把这两项列为 baseline exception，测试阶段会同时要求“行为必须保持”和“行为必须改变”。
-->

### 12.1 G8-A — 基础设施与单一 Damage reducer（生产不启用）

实现顺序必须先消除 Damage 双份 semantics，再搭 detached infrastructure：

```text
A1. 提取 single TryApplyDamageRecord historical reducer/validator
A2. 让当前 Blocking Damage path 使用同一 reducer，并通过现有回归
A3. Controller-owned ControllerEpoch + PresentationSessionToken authority
A4. ordinary Skip 与 ordinary active-envelope reconcile 保持 session；只有 authority replacement/transition 才 stale
A5. 修正 Controller initialization failure：禁止 silent DirectBaseline fallback，进入 fail-safe
A6. DetachedDamageToken / VisualSpec / Instance
A7. GC-safe deterministic owner/container
A8. TransientVFXHost
A9. hidden prepare + exact rollback
A10. NativeTick + finite VisualDuration + exact cleanup
A11. HUD deactivation/destruction / session / replacement cleanup
A12. Widget replacement：cancel old owner 后 retire old playback ownership/token，再安装 new Widget
A13. replacement new binding 建立后先 mint new Session，再执行可能同步 refresh 的 catch-up
A14. sanity ceiling pre-commit decline
A15. PlayCommittedDamageCombatantCues API boundary
A16. post-publication stale rejection harness
```

Production Damage 仍全部走旧 Blocking path。无 early input，无 detached production activation。

### 12.2 G8-B — Exact interaction evaluator + FastInput shadow

前置条件：

```text
- 8.8 的 TargetChoice/EndTurn bugfix 已独立完成并成为 baseline
- player-facing PendingSelection 已端到端使用 exact PendingSelectionRequestIdentity
- Controller initialization failure 已不再 masquerade 为 DirectBaseline
```

实现统一 evaluator 和 mode-specific grant contract，但只 shadow 对照 baseline 行为：

```text
AuthoritySource:
- PresentationOwned
- DirectBaseline

Modes:
- NormalPlayerTurn
- CardReadyToConfirm
- CardTargetChoice
- PendingCardSelection
- Resolving
- Terminal/Unavailable
```

同时实现/验证：

```text
PendingSelectionRequestIdentity = BattleId + SelectionBoundaryRevision
Pending transient selected set 按 exact identity 隔离
Pending submit/cancel 带 ExpectedPendingSelectionRequestIdentity
ExpectedCatchUpRevision deferred fast-card request
ordinary FastInput Skip preserves exact SessionToken
HasSkippablePresentationDelay 与 HasActiveNativePresentation 语义分离
stale ControllerEpoch/Session/Battle/revision/boundary rejection
HasAuthoritativePendingCardSelection 与 visible read surface 的两阶段 gate
ReadyToConfirm/TargetChoice 切换另一张合法 Hand card 的 parity
DirectBaseline sessionless readiness parity
Controller init failure stays unavailable/input locked
```

所有现有 refresh/recovery 入口接受统一 policy 审计；任何未解释 divergence 阻止下一阶段。

### 12.3 G8-C — Damage detached staging，严格保持旧输入等待总量

首次让真实 Damage Record 使用 Controller-owned detached transaction：

```text
formal HP/Block = Controller reducer-owned
DamageNumber = private detached visual only
Hit/Attack = best-effort committed cue
```

本阶段为了单独验证 reducer/job 解耦，必须保留旧 Damage Blocking path 对最终 input-ready 时间贡献的**总等待量**，但**不得通过 DamageNumber finish callback 来释放输入**。

Controller 持有 staging-only：

```text
CompatibilityDebtSeconds >= 0
D = LegacyDamageBlockingDuration
```

每次 detached Damage 正式 commit，在 **formal snapshot commit 之后、ViewModel publication 之前**：

```text
CompatibilityDebtSeconds += D
```

关键规则：**其它 Blocking Presentation 消耗的真实时间不得抵消 Damage debt。**

因此以下旧路径：

```text
Damage1(D)
→ OtherBlocking(X)
→ Damage2(D)
→ input ready

旧总 Presentation wait = D + X + D
```

G8-C 必须得到同样的最终 input-ready 总量：

```text
detached Damage1: debt += D
→ OtherBlocking consumes X, debt remains D
→ detached Damage2: debt += D, total debt = 2D
→ chronological lane reaches otherwise-input-ready boundary
→ service 2D compatibility wait
→ input ready

总 wait = X + 2D
```

不能使用：

```text
CompatibilityInputNotBefore = max(Now, oldDeadline) + D
```

并让其它 Blocking 时间自然“吃掉”旧 debt；那会在 `Damage → Blocking → Damage` 混合序列中比旧 Blocking path 提前开放输入。

#### 12.3.1 Compatibility debt 的服务规则

Debt 只在 Controller 已经达到“若忽略 G8-C staging debt，本来可以 grant 当前 exact interaction surface”的边界时被服务：

```text
chronological active envelope/backlog 已 catch up
required Blocking playback 已完成
recovery 已完成
exact authority/read surface 本来已 ready
CompatibilityDebtSeconds > 0
→ Controller-owned compatibility wait
→ HasSkippablePresentationDelay() == true
→ debt 在 legacy Blocking Damage timer 的同一时间域内递减到 0
→ 再重做 exact readiness recheck
→ 合法才 grant
```

“同一时间域”是硬合同：

```text
G8-C compatibility wait
必须与 legacy Blocking Damage 的 UWorld/TimerManager timer 具有等价：
- game-time progression
- pause semantics
- time-dilation semantics
- world lifetime / teardown semantics
```

首版不得用独立的：

```text
FPlatformTime / real-time wall clock / process uptime
```

来服务 debt，除非 legacy Blocking Damage timer 也迁移到同一个新时间域并通过等价回归。文档中的 `CompatibilityDebtSeconds` 表示 duration debt，不授权实现使用真实世界 wall-clock。

如果 compatibility wait 期间又出现新的 authoritative chronology/recovery：

```text
pause/freeze remaining debt
→ 先处理新的 chronological/recovery work
→ 再次到达 otherwise-ready boundary 后继续服务剩余 debt
```

这里的“pause/freeze”同样必须尊重上述 legacy timer 时间域；不得在 Controller 暂停 debt service 时偷偷按 real-time 扣减。

这样 debt 只代表“旧 Damage Blocking 本来占用、但 detached 后被拿掉的串行 Presentation 时间”，不会被其它 Blocking job 重复消费。

#### 12.3.2 ordinary Skip 对 debt 的语义

当前 G0–G7 `SkipPresentation()` 会直接 collapse active/backlog Presentation，并立即尝试 catch-up；旧 Blocking Damage 的剩余 timer 也会被 Skip 绕过。

因此 G8-C 中 ordinary Global Skip / FastInput catch-up 必须：

```text
允许 HasSkippablePresentationDelay() 在 debt-only wait 时返回 true
→ cancel current-session detached cosmetics according to Skip policy
→ clear CompatibilityDebtSeconds for the collapsed chronology
→ keep same PresentationSessionToken
→ perform sealed catch-up
→ retry FastInput only through exact ExpectedCatchUpRevision guards
```

即：**Skip 清 debt，但不换 Session。**

不得出现：

```text
old path: Damage timer active → click → Skip → immediate catch-up
G8-C: only debt remains → HasActiveNativePresentation false → click cannot Skip
```

也不得出现：

```text
old path: Skip 后立即 catch-up
G8-C: Skip 后还额外等待 previously accumulated Damage debt
```

这两种情况都会破坏 staging parity。

#### 12.3.3 LegacyDamageBlockingDuration 只有一个 timing authority

G8-C 不允许在 Controller 新复制一个 `0.5f` 或其它 magic literal。

必须先把当前 Blocking Damage 使用的 legacy duration 提取为一个**唯一、可测试的共享 timing contract**，例如：

```text
GetLegacyDamageBlockingDuration()
```

或等价配置入口，并满足：

```text
old Blocking Damage timer
G8-C 每次 CompatibilityDebtSeconds increment
→ 读取同一 timing authority
```

Timing parity 包含两层，缺一不可：

```text
Duration source parity:
old Blocking Damage duration == debt increment D

Clock-domain parity:
old Blocking Damage finish timer 与 compatibility wait
使用等价的 game-time / pause / time-dilation / world teardown 语义
```

Compatibility debt 必须在以下边界清理：

```text
ordinary Global Skip / backlog collapse
session invalidation
battle replacement
DirectBaseline / unavailable / terminal transition
feature runtime disable
G8-D migration
```

普通 active-envelope reconcile **不因 reconcile 本身清 debt**；如果 reconcile 继续保留 backlog/session，则 debt 继续属于同一 staging chronology。只有 reconcile 升级成上述 replacement/transition 时，才按对应边界清理。

进入 G8-D 时：

```text
remove CompatibilityDebtSeconds / compatibility wait dependency
→ readiness 不再读取 LegacyDamageBlockingDuration
→ HasSkippablePresentationDelay 不再因为 DamageNumber/cosmetic tail 为 true
→ DamageNumber duration 继续只控制 cosmetic lifetime
```

因此即使 G8-C：

```text
cosmetic finish
X→ compatibility debt mutation
X→ readiness mutation
```

仍然成立。

G8-C 允许 HP/Block publication 时机和动画重叠节奏相对旧 Damage playback 改变；它只要求最终 input-ready 的 legacy Damage 等待贡献保持等价。必须单独 Automation/PIE，不能用“输入仍锁着”宣称视觉完全等价。

### 12.4 G8-D — DamageNumber 真正 NonBlocking

删除 G8-C 的 `CompatibilityDebtSeconds` / compatibility wait。

此后 DamageNumber completion 对 Controller/ViewModel/readiness 完全无反向影响。当 exact CurrentMode surface ready 时，即使旧 DamageNumber 仍 alive，也允许对应合法输入。

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
Damage → OtherBlocking → Damage mixed chronology
fully-blocked Damage
player Hit cue
enemy Hit cue
enemy Attack cue
rapid legal card input
ReadyToConfirm confirm/cancel + switch selected Hand card
Target choose/cancel + switch selected Hand card
TargetChoice EndTurn rejection
Draw → Pending Selection
Pending Selection first click before SelectionGeneration exists
back-to-back PendingSelection with same source/candidates but different PendingSelectionRequestIdentity
stale PendingSelection submit/cancel against newer identity rejection
Pending Selection surface catch-up
EndTurn
FastInput exact ExpectedCatchUpRevision
ordinary FastInput Skip keeps same SessionToken and clears G8-C debt
G8-C debt-only wait is still fast-skippable without pretending a Native Widget playback exists
Skip with real replacement makes deferred click stale
active-envelope reconcile keeps same SessionToken and backlog continuity
HUD replacement: old playback owner/token retired before new Widget；new Session exists before catch-up；catch-up ordering exact
Controller replacement / ControllerEpoch ABA
battle replacement
DirectBaseline sessionless readiness
Controller initialization failure stays unavailable
G8 runtime disable keeps SessionToken when authority binding is unchanged
unavailable/direct transition
terminal
viewport/DPI change
HUD deactivation/destruction cleanup
sanity ceiling
GC
G8-C multi-hit / mixed-blocking compatibility debt
G8-C pause/time-dilation clock-domain parity with legacy Blocking Damage timer
```

基础 cleanup 必须在首次 production detached activation 前已经通过，不等到 G8-E 才补。

### 12.6 G8-F — 证据与封存

Build + affected focused Automation + Native `L_BattleTest` PIE 全部通过后，才能标记 implemented / validated / sealed。

G8 feature 关闭时，新 Record 必须完整退回 G0–G7 Blocking path。若运行中关闭，且 Presentation authority/binding 未变化：

```text
KEEP current PresentationSessionToken
→ stop accepting new detached Damage transactions
→ clear private cosmetics
→ clear staging compatibility debt
→ subsequent Damage uses old Blocking path
→ 按当前 exact authority source / surface 重评输入
```

只有运行时关闭 G8 的同时发生真实：

```text
HUD/Controller/battle replacement
DirectBaseline transition
presentation unavailable transition
```

才由对应 authority contract invalidate SessionToken。

已 reduced Record 不重放。DirectBaseline 本身没有 detached session 可失效。

<!-- REVIEW FIX 2026-09-12:
这里把“runtime disable → invalidate detached session”改为“runtime disable → 保持 PresentationSessionToken”。
Session 是 Presentation authority identity，不是 detached feature identity。单纯关闭 G8 只应撤销 cosmetic/debt policy，
不能让未发生 replacement 的同一 Controller binding 看起来像换了一个 authority。
-->

## 13. 验收标准

### 13.1 AUTOMATED GATES

| 测试主题 | 必须证明的外部合同 |
|---|---|
| A→B authoritative 时序 | A Resolution 完成后 B 才 accepted；A DamageNumber 可仍 alive；无并发 Gameplay |
| Single Damage reducer | Blocking/G8 preflight 复用同一完整 historical rule；无双份 Damage 语义 |
| Damage contract coverage | DamageKind/source/before/after/accounting invariant 只由同一 reducer 定义；visual eligibility 不复制 historical semantics |
| Reducer determinism | G8 on/off、cosmetic tick/finish 时机不改变 committed Record/trigger/reducer/FinalSnapshot 顺序 |
| Controller transaction authority | HUD 不推进 Record、不 commit reducer、不通过 cosmetic callback completion |
| One-way cosmetic lane | G8-C/D 的 finish 都不调用 Controller/VM/readiness/ownership mutation |
| Exact session/token | 旧 HUD/session callback 不能删除新 session instance，也不能影响新 input surface |
| Ordinary Skip session continuity | same Controller/Battle/binding 的 Skip/backlog catch-up 保持 SessionToken；FastInput retry 可继续 exact 校验 |
| Ordinary reconcile session continuity | active-envelope historical/reducer recovery 保持 SessionToken；后续 backlog 可直接继续，不出现 session 空窗 |
| Replacement staleness | HUD/Controller/battle/authority replacement 后旧 SessionToken 与 deferred click 必须 stale |
| Controller replacement ABA | replacement Controller 的 ControllerEpoch/token 不可能与旧 Controller token 相等；旧 callback 全部 stale |
| Widget replacement playback ownership | old Widget cancel 后 Controller retire old waiting/token/timeout；new Widget 永不收到 old FPresentationPlaybackToken |
| Widget replacement ordering | old session 先失效并在 old Widget cleanup；old playback owner retire；new binding 后、catch-up 前已有 new Session；catch-up 前不得 grant readiness |
| DirectBaseline readiness | intentional DirectBaseline 无 Controller/SessionToken 时仍可按 exact frozen baseline/read edge 正常获得合法 readiness；不伪造 session |
| Controller-init failure fail-safe | 本应启用 Presentation 但 Controller 初始化失败时不得恢复普通 DirectBaseline input；必须 unavailable/input locked |
| Prepare transaction | pre-commit failure 无外部副作用并回 Blocking；post-publication stale 只丢 visual，不重播 reducer |
| G8-C debt commit ordering | detached Damage formal commit 后、ViewModel publication 前登记 debt；同步 Skip/replacement 能看到并正确清理该 debt |
| Damage combatant cues | detached 成功仍保持正确 target Hit / enemy source Attack；cue 不影响 formal state |
| Mode-specific readiness | Normal/ReadyToConfirm/Target/PendingSelection grant 不串模式 |
| Pending request exact identity | player-facing pending request 使用完整 BattleId + SelectionBoundaryRevision；back-to-back 同内容 request 仍是不同 identity |
| Pending submit/cancel exactness | stale BattleId 或 stale SelectionBoundaryRevision 的 submit/cancel 都不能作用到当前较新的 pending request |
| Pending transient-set isolation | PendingSelectionRequestIdentity 改变时旧 PendingCardSelectionRuntimeIds 必须清空，即使 source/count/candidates 完全相同 |
| Pending request vs Presentation generation | 第一次 pending click 不依赖尚不存在的 SelectionGeneration；visual lifecycle 建立后再附加校验 SelectionGeneration |
| Card surface switch parity | ReadyToConfirm/ChoosingTarget 下可切换另一张合法 Hand card；旧 selected-card credential 立即 stale |
| TargetChoice input contract | ChoosingTarget 允许 target choose/cancel/合法换卡；EndTurn 在 request boundary 与 HUD 均禁止 |
| Pending selection boundary | authoritative pending 已存在但 read surface 未 ready 时，普通出牌/EndTurn/submit 全禁止 |
| Shadow parity | G8-B shadow 与 8.8 bugfix 后 baseline input/mode 无未解释 divergence；覆盖 PresentationOwned/合法 DirectBaseline |
| FastInput expected revision | retry 只在 exact ExpectedCatchUpRevision；更老/更新 revision 都丢 stale click |
| FastInput skippable-delay gate | debt-only wait 可 FastInput Skip；纯 detached cosmetic tail 不可触发 Skip；不污染 HasActiveNativePresentation 语义 |
| FastInput session replacement | ControllerEpoch/session/battle/authority 变化后旧 deferred click 不重放 |
| Read-before-envelope | latest read 提前、Envelope 尚未 chronological catch-up 时不能开放普通输入 |
| Damage isolation | old cosmetic 不恢复 opacity、不回写 HP/Block、不改 Hand/target state |
| G8-C timing authority | Blocking Damage 与 compatibility debt increment 使用同一 legacy duration source；无复制 magic literal |
| G8-C clock-domain parity | compatibility wait 与 legacy Blocking Damage finish timer 使用等价 game-time / pause / time-dilation / teardown 语义；不得独立 real-time 计时 |
| G8-C multi-hit debt | N 个 detached Damage 产生 N×legacy duration debt |
| G8-C mixed chronology debt | Damage→Blocking→Damage 的其它 Blocking 时间不得抵消 Damage debt；最终 input-ready 总 wait 与旧串行 path 等价 |
| G8-C Skip debt | ordinary Skip/debt-only fast Skip 清除被 collapse chronology 的 compatibility debt，但保持 SessionToken |
| G8-C reconcile debt | same-session active-envelope reconcile 不无故清 debt；replacement/transition 才按边界清理 |
| G8 runtime disable identity | authority/binding 未变化时关闭 G8 清 cosmetic/debt 但保持 SessionToken；后续 Damage 回 Blocking |
| G8-D timing independence | readiness 不再依赖 LegacyDamageBlockingDuration、compatibility debt 或 DamageNumber duration |
| Lifetime/GC | finite VisualDuration、GC-safe owner、重复 cleanup 幂等、session/Widget deactivation/destruction cleanup 正确 |
| Sanity ceiling | ceiling 在 pre-commit decline；不 eviction、不重复 reducer |
| Recovery | reconcile/Skip/direct/unavailable/terminal/replacement 后无 ghost cosmetic、无永久锁 |
| Prerequisite baseline fixes | TargetChoice/EndTurn 与 Controller-init fail-safe 两项行为修复按 12.0 独立通过并成为新 baseline |
| G0–G7 regression | **除 12.0 明确列出的 prerequisite fixes 外**，Selection ownership、G6 Group、SingleRecord fallback、exact cancellation/recovery 等 sealed contract 不受影响 |

主时序测试使用真实 Gameplay Request + committed Envelope。可注入 clock 只用于确定性边界，
不得以模拟 job 存活冒充真实 PIE 体验。若注入 clock 覆盖 G8-C compatibility wait，必须模拟 legacy timer 的同一时间域语义，而不是引入测试专用 real-time 行为。

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
无目标牌 ReadyToConfirm 的 Confirm/Cancel 与切换另一张合法 Hand card
Target choose/cancel 与切换另一张合法 Hand card
TargetChoice 时 EndTurn 明确不可用
Draw → PendingSelection 未 ready 与 ready 两个边界
第一次 pending-selection click 在 SelectionGeneration 尚未创建时仍可合法建立 presentation lifecycle
连续两个内容相同的 pending request 具有不同 PendingSelectionRequestIdentity，旧 transient set/callback/submit 不可重放
跨 Battle 的相同 SelectionBoundaryRevision 不得让旧 pending submit/cancel 命中新 Battle request
G6 multi-select 同时播放
FastInput ordinary Skip 后保持同 SessionToken，并只在 exact target revision 重放
G8-C 只剩 compatibility debt、没有 ActiveNativePresentation 时仍可由 fast click 按旧语义 Skip
只有 detached DamageNumber tail 时 fast click 不触发 Skip
G8-C ordinary Skip 后不残留 compatibility debt
revision 前后变化或真实 replacement 时 deferred click 丢弃
ordinary active-envelope reconcile 后 SessionToken 不变，后续 backlog 继续
intentional DirectBaseline 无 SessionToken 时正常选牌/Target/ReadyToConfirm/EndTurn
模拟 Controller initialization failure 时进入 unavailable，不恢复普通 input
HUD replacement / Controller replacement 后旧 cosmetic 与旧 deferred click 均失效
HUD replacement 时 old playback token 不会被发送给 new Widget
HUD replacement 需要 catch-up 时，新 Session 在 catch-up 前已经存在，但 exact surface ready 前仍不开放 input
G8-C 连续多段 Damage 的 compatibility debt 与旧串行 Damage 等价
G8-C Damage → OtherBlocking → Damage 的最终 input-ready 时间与旧串行 path 等价
G8-C 在 pause / time-dilation 场景下与 legacy Blocking Damage timer 的等待语义一致
G8 runtime disable（无 authority replacement）后 SessionToken 不变、cosmetic/debt 清理、Damage 回 Blocking
HUD deactivation/destruction / Skip / terminal / viewport-DPI 变化后无残留
```

记录默认 DamageNumber `VisualDuration`、实际 input-ready 时间、legacy Blocking Damage duration 来源、legacy timer 时间域、G8-C accumulated debt 和后续 Blocking card-tail duration。

若默认时序没有实际跨牌视觉重叠，应如实记录，不通过人为延迟 cosmetic 或缩短 Blocking 路径伪造收益。

## 14. 本次设计冻结点与未授权范围

本次修改仅调整设计，未修改 C++、未运行 UE Build/Automation/PIE，也没有新增运行验收证据。

当前首版冻结候选：

```text
Blocking Presentation lane 保持 G0–G7，但以 12.0 两项明确 prerequisite fix 作为新的 regression baseline exception
Detached cosmetic lane 首版只有 DamageNumber
Controller 是 PresentationOwned session mint + detached transaction + reducer commit 唯一 authority
PresentationSessionToken 包含不可跨 Controller replacement 复用的 ControllerEpoch
ordinary Skip/backlog catch-up 保持同一 SessionToken；cancel cosmetic 不等于 invalidate session
ordinary active-envelope reconcile 保持同一 SessionToken；recovery 只有升级成 authority replacement/transition 才 stale
G8 runtime enable/disable 本身不改变 Presentation authority；binding 未变化时保持同一 SessionToken
HUD/Controller/battle/authority replacement 才使旧 session/deferred request stale
DirectBaseline 只表示 intentional sessionless direct authority；不得伪造 SessionToken
Controller initialization failure 不得 silent fallback 到 DirectBaseline，必须 fail-safe/unavailable
HUD replacement 必须先 invalidate old session、cleanup/cancel old owner、retire old playback token/timeout/waiting state、安装 new binding、mint new Session，再执行可能同步 refresh 的 catch-up
old FPresentationPlaybackToken 不得跨 Widget replacement 发送给 new Widget
需要 catch-up 的 replacement 在 exact authoritative surface 到达前不得 grant readiness
Damage historical validation 提取为单一、完整 reducer/validator
historical validity 与 visual eligibility 严格分离
现有 Blocking Damage 必须先迁移到同一 reducer，再进入 detached implementation
HUD detached owner 永不推进 Record/reducer/readiness
最终 DamageNumber completion 对 authoritative/input lane 无反向边
G8-C 每个 detached Damage 在 formal commit 后、publication 前增加 CompatibilityDebtSeconds
G8-C 每个 detached Damage 只增加 compatibility debt，不使用会被其它 Blocking 时间抵消的 last-deadline 算法
G8-C debt 只在 chronological/recovery 已 otherwise-ready 后服务；其它 Blocking Presentation 不抵消 debt
G8-C compatibility wait 与 legacy Blocking Damage timer 使用同一时间域 / pause / time-dilation / world lifetime semantics
FastInput 使用 HasSkippablePresentationDelay 概念；debt-only wait 可 Skip，pure cosmetic tail 不可 Skip
ordinary Skip 清除被 collapse chronology 的 G8-C debt，但保持 SessionToken
G8-C legacy duration 只有一个 timing authority，不复制 0.5f magic literal
G8-D readiness 与 legacy debt / cosmetic duration 完全解耦
Interaction readiness 覆盖 Normal / ReadyToConfirm / Target / PendingSelection
PendingSelection 保持独立 authoritative lifecycle，不硬塞入 InteractionState enum
PendingSelection exact identity 使用完整 BattleId + SelectionBoundaryRevision，不新增第二套 request generation
read / transient set / submit / cancel 都必须校验同一完整 PendingSelectionRequestIdentity，不允许只传 revision
SelectionBoundaryRevision 与 Presentation SelectionGeneration 类型/职责分离，第一次 click 不形成循环依赖
首版不新增 TargetChoiceGeneration
ReadyToConfirm / TargetChoice 保留切换另一张合法 Hand card 的现有行为
ChoosingTarget / EndTurn 当前行为视为前置独立 bugfix，不由 shadow evaluator 偷改
FastInput deferred request 使用 exact ExpectedCatchUpRevision + exact SessionToken
Damage detached 成功必须保留 committed Hit / enemy Attack combatant cues
combatant cue 不属于 DamageNumber job，也不参与 Record completion
prepare/reducer/debt/publication/activation 有明确顺序与 pre/post exact recheck
DamageNumber 由 NativeTick + finite VisualDuration 管理；任何停止 tick 的 owner deactivation/destruction 边界必须强制 cleanup
首版不额外定义不可达 HardTimeout
数量 ceiling 只是高位 bug containment；首版无 eviction
card tail / opacity hit flash / status VFX 不在首版 detached 范围
G8 不要求 CardPlayed / CardZoneChanged 视觉尾巴与下一次卡牌输入重叠；Blocking card presentation 上的 FastInput 仍可按现有 Skip/catch-up 语义收尾当前动画
buffered card input、保留上一张完整卡牌动画并同时接受下一张牌、以及 detached card presentation 统一延后到 G9 独立设计
```

CardPlayed、PlayArea card tail、Selection transitions、Draw→Hand、G6 Group、status VFX、combatant animation 是否未来获得更广泛 overlap/detach，需要新的独立设计与 acceptance；不因 G8 DamageNumber seal 自动获得授权。