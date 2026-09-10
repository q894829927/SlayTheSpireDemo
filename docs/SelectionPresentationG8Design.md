# G8 详细设计：独立伤害数字与精确输入就绪

日期：2026-09-10。状态：**DESIGN PROPOSAL / NOT IMPLEMENTED / NOT SEALED**。

本次仅调整 G8 设计文档，不代表授权实现 G8。G0–G7 当前阶段状态仍以
[SelectionPresentationG7SealAmendment.md](SelectionPresentationG7SealAmendment.md)
和对应 execution/validation 记录为准。

本文细化并修订 GroupDesign 第 30 节和 GroupImplementationPlan 第 18 节的未来
G8 候选方案。首版目标保持收窄：**只把独立 DamageNumber 变成跨 Resolution
可存活的 cosmetic tail；所有卡牌移动、Selection Group、Draw、正式状态更新和终局
仍走现有 Blocking Presentation。**

本轮设计额外参考了公开可检索的 Slay the Spire 1 双轨思路：需要控制因果/节奏的视觉
留在主 Action 链，纯 cosmetic effect 使用独立生命周期。这里只吸收架构启发，不把公开
反编译/模组代码视为本项目的权威实现规范。

## 1. 核心模型：两条运行轨 + 四种生命周期

G8 不建立第二套 Gameplay scheduler、第二套 reducer，也不建立“所有 Presentation
都能动态 Blocking/NonBlocking”的万能 job 框架。

长期生产模型只有两条视觉运行轨：

```text
A. Blocking Presentation lane
   Controller → Record/Group playback → exact completion → reducer

B. Detached cosmetic lane
   committed Damage fact → private DamageNumber instance → tick/fade → self cleanup
```

但概念上必须继续区分四种生命周期：

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
→ B request may become legal
→ B Gameplay
```

Detached cosmetic 可以跨已完成 Resolution 存活，但不能导致 Gameplay 并发、reducer
乱序、trigger 重排或 speculative future state。

## 2. 当前实现约束与迁移边界

以下是设计依据，不是 G8 已实现的证据：

- `BattlePresentationController.cpp` 当前只有一个 active playback；`CompleteActiveRecord`
  才应用 reducer、发布工作快照并推进下一 Record。
- `BattleHUDWidgetBase.h` 的 Record/Group 共用一份 exact tracked playback owner；这条
  G0–G7 Blocking 协议继续保留。
- `BattleHUDWidget.cpp` 的 Damage 当前复用 `Txt_DamagePresentation`，并会修改正式目标
  opacity/HP/Block 展示；detached 路径不能继续持有这些正式 surface。
- CardPlayed 仍依赖单一 `NativePlayedCardWidget` 和 PlayArea 生命周期，因此首版禁止
  card tail detach。
- `BattleHUDViewModel::RefreshLiveInputBindingsIfCaughtUp()` 已对 latest frozen baseline、
  player-facing read 和 VM BattleId/StateRevision 做 exact guard；G8 必须复用这些权威
  核验，而不是用 cosmetic job 数量替代。
- Selection/target 是决策模式，不是“全局 input off”。准备好的 Selection/Target surface
  必须允许该模式自己的合法操作，同时继续禁止普通出牌/EndTurn 等旁路请求。

G6 的 N-child parallel Group 是**同一 committed group 内的并行视觉**；G8 detached
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

受击闪烁首版也不 detach。Detached Damage 成功路径只显示独立数字；旧 Blocking fallback
仍保留现有正式受击视觉。因此 PIE 必须记录这个视觉差异，不能宣称完全视觉等价。

### 3.3 体验预期

首版不人为拖长 DamageNumber，也不缩短/跳过仍 Blocking 的 card tail 来制造演示。
如果后续 Blocking 卡牌动画比 DamageNumber 更长，某些牌实际看不到跨牌数字重叠是合法
结果。验收要记录真实 input-ready 时间点和数字是否仍存活，只报告实际收益。

## 4. 所有权与身份

### 4.1 Presentation session authority

`PresentationSessionGeneration` 必须只有一个 mint authority。

建议由 `UBattleHUDPresenter` 在建立一组新的：

```text
BattleManager + ViewModel + PresentationController + HUD Widget
```

绑定会话时单调分配 `PresentationSessionGeneration`，形成不可变：

```text
PresentationSessionToken = BattleId + PresentationSessionGeneration
```

Controller、HUD 和 detached Damage owner 只消费该 token，不各自推导另一套“当前 session”。
HUD/Controller replacement、battle replacement、direct/unavailable 重新建链时，旧 session
先失效，再清理旧 cosmetic。

### 4.2 DamageNumber identity

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
DetachedDamageInstance =
  Token
  + SourceFinalStateRevision      // diagnostic provenance only
  + FrozenDamageVisualSpec
  + GC-tracked transient Widget
  + frozen start position
  + elapsed / duration / hard timeout
  + lifecycle state
```

`LocalDamageVisualGeneration` 由当前 HUD/session owner 单调分配，不来自时间、指针、TMap
顺序或 Widget 地址。同一 Record 如果在提交前重新尝试 detached prepare，也必须分配新的
visual generation。

该 token 与 `FPresentationPlaybackToken` 类型分离；接口不得互相接受，避免 cosmetic
callback 被误送到 Controller Record completion。

### 4.3 GC 和异步回调

Detached owner 必须通过 `UPROPERTY` 可达容器强持有 transient Widget。Ticker/timer/lambda
只允许捕获 weak owner + exact `DetachedDamageToken`，不能承担 UObject 生命周期。

## 5. Detached Damage 启动事务

保留现有 `PlayPresentationRecord(...)` bool Blocking 合同。G8 只在 Damage Record 上增加一个
**提交前可完全回退**的 detached prepare 路径。

目标切点：

```text
validate current Record identity / payload
→ reducer preflight on snapshot copy
→ detached eligibility + Host/session/geometry validation
→ prepare hidden DamageNumber instance（外部不可见）
→ pre-commit exact cursor/session recheck
→ reducer exactly once
→ publish formal working snapshot
→ post-publication exact session/job recheck
→ activate prepared DamageNumber if still valid
→ dispatch next Record
```

重要合同：

- reducer preflight 复用现有 reducer 规则，不复制第二份 Damage 历史修改逻辑；
- prepare 不修改正式 HP/Block/opacity，不发送 completion，不推进 Controller；
- prepare 不清理任何已存在的合法 cosmetic；
- 正式 snapshot publication 可能同步触发 Skip/replacement/unavailable/reconcile，因此
  **publication 后必须再次核验 exact session + exact prepared token**；
- publication 后如果 prepared job 已被同步取消，只丢弃该数字；Record 已 reduced，绝不
  fallback 重播或再次 reducer；
- activation 必须发生在 reducer publication 之后、下一 Record dispatch 之前的明确切点。

失败分界：

```text
提交前 detached decline
→ 完整 rollback hidden instance
→ 走原 Blocking Damage playback

历史 payload / reducer invalid
→ 现有 active-envelope recovery
→ 不是动画 fallback

提交后 cosmetic activation/finalization failure
→ 只清理 exact private DamageNumber
→ 不重播 Record
→ 不修改 Gameplay / VM / ownership

同步 Skip/replacement/reconcile
→ old session/token invalid
→ 原启动流程停止
→ 不在恢复后的新状态执行旧 fallback
```

## 6. Detached cosmetic 的长期完成语义

G8-D 正式 NonBlocking 后，DamageNumber 是真正 fire-and-forget cosmetic：

```text
DamageNumber finish / timeout / cancel
→ remove own Widget
→ remove own instance
→ END
```

最终生产路径中禁止：

```text
DamageNumber finish → NotifyPresentationFinished
DamageNumber finish → Controller CompleteActiveRecord
DamageNumber finish → ViewModel mutation
DamageNumber finish → InteractionReady mutation
DamageNumber finish → card ownership mutation
```

即 cosmetic lane 对 authoritative lane **只有单向输入，没有反向 completion 边**。

G8-C 的“detached 但仍保持旧输入等待”只是一项迁移验证阶段，不形成永久
`InteractionPolicy::Blocking` job 类型。具体做法见第 10 节 staging：通过阶段级兼容 gate
临时保持旧输入节奏，验证 reducer/job 解耦；进入 G8-D 后移除该兼容依赖。

## 7. Interaction readiness：以交互模式为核心

### 7.1 Readiness 不是 cosmetic job 状态

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

不建立通用 barrier registry。首版把“barrier”视为 mode-specific readiness policy 的内部
实现细节。

### 7.2 Session 与决策面 identity

Readiness credential 必须包含 exact `PresentationSessionToken`，但不要再造一个和现有
SelectionGeneration 竞争的万能 `SurfaceGeneration`。

建议按 mode 使用已有/最小身份：

```text
NormalPlayerTurn:
  SessionToken + BattleId + StateRevision

PendingSelection:
  SessionToken + BattleId
  + SelectionBoundaryRevision + existing SelectionGeneration

TargetChoice:
  SessionToken + BattleId + StateRevision
  + exact selected-card RuntimeId
  + TargetChoiceGeneration（仅在当前代码缺少可复用 exact target lifecycle identity 时新增）
```

TargetChoiceGeneration 若需要新增，由 ViewModel 在 target surface 建立/替换时单调 mint；
hover、普通 tick、DamageNumber 更新不得改变它。

新 Battle、session replacement、revision replacement、SelectionGeneration replacement、
TargetChoice replacement、recovery 启动都会使旧 credential 失效。请求被拒但 exact 决策面
仍有效时，只重新评估当前 mode，不制造新权威状态。

### 7.3 Mode-specific grant：不能共用一个 finalizer

`RefreshLiveInputBindingsIfCaughtUp()` 当前最终会进入普通 PlayerTurn Idle，并清理 transient
selection state，因此不能作为所有模式共同的最后一步。

统一的是**readiness policy**，不是“grant 动作”。

```text
EvaluateInteractionReadiness(CurrentMode)
  |
  +-- NormalPlayerTurn
  |     → exact baseline/read/VM guard
  |     → existing RefreshLiveInputBindingsIfCaughtUp()
  |
  +-- PendingSelection
  |     → exact SelectionGeneration/boundary/read surface guard
  |     → preserve pending-selection state
  |     → enable only Select/Deselect/Confirm/Cancel contract
  |
  +-- TargetChoice
        → exact target lifecycle/card/read surface guard
        → preserve SelectedCardRuntimeId + LegalTargets
        → enable only target selection/cancel contract
```

不能通过 Pending Selection 的 readiness 顺便开放普通 card-play，也不能调用普通 PlayerTurn
finalizer 把 selection/target transient state 清掉。

### 7.4 Mode matrix

| 当前交互面 | 可开放 | 必须禁止 |
|---|---|---|
| PlayerTurn normal surface ready | 合法选牌/出牌、EndTurn | Query 拒绝的请求 |
| Pending Selection surface 未 ready | 无选择提交 | 普通出牌、Confirm、EndTurn |
| Pending Selection exact surface ready | 选/取消选、合法 Confirm/Cancel | 普通出牌、EndTurn |
| Target choice exact surface ready | 合法目标选择/取消 | 绕过目标流程的普通请求 |
| Terminal / unavailable / recovery | 既有明确恢复控件 | 战斗请求 |

无可打牌时仍可能允许 EndTurn，所以 NormalPlayerTurn readiness 不能以“存在可打牌”为条件。

### 7.5 Shadow readiness gate

G8-B 不提前解锁。它先计算新 policy 的 shadow result，并与当前实际 input-enabled/mode
结果逐边界比较：

```text
任何未解释 divergence
→ G8-B FAIL
→ 不进入 G8-C
```

只有明确记录为现有 bug 且单独授权修复的差异，才允许不一致。

## 8. TransientVFXHost 与正式 HUD 隔离

运行时在 Native HUD 根 Canvas 下创建独立 `TransientVFXHost`，不复用 Hand、PlayArea、
SelectionArea，也不要求修改 production `.uasset`。

Host 和所有 DamageNumber child：

```text
不参与 hit test
不获得 focus
不参与 drag/drop
不处理 Selection/Target 输入
不成为 formal Hand/Combatant surface
```

创建时从 frozen `TargetPresentationId` 匹配**当前历史目标 surface**，在 prepare 阶段验证：

```text
Battle/session exact
Target surface exists
geometry finite/non-zero
Host geometry valid
AbsoluteToLocal conversion valid
```

然后冻结起点。激活后只更新自己的 translation/opacity；绝不继续追踪 target Widget，也不：

```text
restore target opacity
write formal HP / Block
write ViewModel
change Hand/Card ownership
```

Viewport/DPI/Host geometry 发生无法安全重映射的改变时，首版直接取消当前 session 的
cosmetic instances。正常 historical dirty publication、FinalSnapshot、普通 Hand reconcile
不得清空 Host，否则跨 Resolution tail 无法存在。

## 9. 资源合同与恢复

### 9.1 有限生命周期是硬合同

每个 DamageNumber 必须有：

```text
finite positive visual duration
finite hard timeout > visual duration
idempotent exact cleanup
session-wide cleanup
```

坏配置在 prepare 阶段 decline 到旧 Blocking 路径。

### 9.2 数量上限降级为防御性 sanity ceiling

首版**不把 8 个实例作为正常行为上限，也不做正常 eviction 策略**。

正常资源有界性主要依赖有限 duration + hard timeout + recovery cleanup。

可以保留一个明显高于正常视觉密度的 defensive ceiling，例如：

```text
MaxDetachedDamageNumberInstances = 32   // exact value implementation/PIE 决定
```

它只用于 bug containment，不是 Gameplay/Pacing 规则。

如果 prepare 时已经达到 sanity ceiling：

```text
不删除旧合法 cosmetic
不随机/按 TMap 淘汰
不先 reducer 再发现没容量
→ detached prepare decline
→ 完整回旧 Blocking Damage path
```

因此首版没有 eviction ordering、victim reservation 或容量导致的提交后竞态。

### 9.3 Recovery policy

| 事件 | Detached Damage | authoritative/input lane |
|---|---|---|
| 正常 Record/Envelope/FinalSnapshot 完成 | 合法 instance 继续 | 正常 chronological/readiness |
| 单项 finish/timeout | 只清 exact instance | G8-D 后无输入副作用 |
| active-envelope failure reconcile | 取消当前 session 全部 cosmetic | 保留后续 backlog，走现有 envelope recovery |
| Global Skip / backlog collapse | 全部取消 | 保留现有 catch-up，不伪造 job success |
| HUD/Controller replacement | 先失效 session，再全清 | 新 session 重建 readiness |
| battle replacement/direct/unavailable | 全清旧 session | 走现有模式切换/恢复 |
| terminal | 全清 | 正式终局优先 |

Cosmetic timeout/cancel 不触发 Gameplay ResolutionFault，也不把整个 Presentation 标记
unavailable。损坏 committed Envelope 仍按现有 historical failure contract 处理。

## 10. FastInput 与分阶段实施

### 10.1 FastInput 兼容

保留 `HasActiveNativePresentation()` 当前 Blocking/card 语义，不把 detached DamageNumber
混入该查询。

只有 cosmetic tail 存在时：

```text
正常输入不能触发 SkipPresentation
不能因为点击而清空 DamageNumber
```

仍存在真正 Blocking Presentation 时，既有 fast catch-up 可保留。但 deferred click identity
必须至少冻结并复核：

```text
BattleId
PresentationSessionToken
source revision / exact interaction surface identity
InteractionMode
RuntimeId
```

过期点击不得在新 battle、新 SelectionGeneration 或新 target surface 被重放。

### 10.2 G8-A — Detached Damage 基础（生产不启用）

实现/验证：

```text
PresentationSessionToken authority
DetachedDamageToken
GC-safe owner/container
TransientVFXHost
hidden prepare + exact rollback
finite duration + hard timeout
sanity ceiling pre-commit decline
session-wide cleanup
post-publication stale rejection test harness
```

Production Record 仍全部走旧 Blocking path，无 early input。

### 10.3 G8-B — Mode-specific shadow readiness

实现统一 readiness evaluator 和 mode-specific grant contract，但只 shadow 对照旧行为：

```text
NormalPlayerTurn
PendingSelection
TargetChoice
Terminal/Unavailable/Recovery
```

所有现有 refresh/recovery 入口必须接受统一 policy 审计；任何未解释 divergence 阻止下一阶段。

### 10.4 G8-C — Damage 拆分，detached staging

首次让真实 Damage Record 使用 detached prepare/commit 路径：

```text
formal HP/Block = reducer-owned
DamageNumber = private detached visual only
```

但此阶段**不把 `InteractionPolicy::Blocking` 写进 job 类型**。为了单独验证 reducer/job
解耦，可以用阶段级 `bG8AllowEarlyInteraction=false` 兼容 gate 临时保持旧输入等待；
该 gate 只存在于 staging，不成为最终架构语义。

若兼容 gate 需要在最后一个 DamageNumber 清理后重新评估输入，该通知只用于 staging
readiness reevaluation，永远不能推进 Record/reducer。进入 G8-D 后删除这条依赖。

G8-C 明确允许 HP/Block publication 时机和视觉重叠节奏相对旧 Damage playback 改变，
因此必须单独 Automation/PIE；不能用“输入仍锁着”宣称完全等价。

### 10.5 G8-D — DamageNumber 真正 NonBlocking

删除 G8-C 的 early-interaction compatibility gate。

此后 DamageNumber completion 对 Controller/ViewModel/readiness 完全无反向影响。
当 exact CurrentMode surface ready 时，即使旧 DamageNumber 仍 alive，也允许对应合法输入。

必须证明：

```text
A Gameplay/Resolution 已完成
A DamageNumber 仍 alive
→ B Request 可以被真正接受
→ B Gameplay 只在 A 完成后开始
→ A cosmetic 后续 finish 不影响 B
```

### 10.6 G8-E — 集成验证

覆盖：

```text
连续/多段 Damage
fully-blocked Damage
rapid legal card input
Draw → Selection
Target choose/cancel
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

不是到 G8-E 才补基础 cleanup；基础 cleanup 必须在首次 production detached activation 前已经通过。

### 10.7 G8-F — 证据与封存

Build + affected focused Automation + Native `L_BattleTest` PIE 全部通过后才能标记 implemented/
validated/sealed。

G8 feature 关闭时，新 Record 必须完整退回 G0–G7 Blocking path。若运行中关闭，先失效
当前 detached session、清 private cosmetic，再按当前 exact authoritative surface 重评输入；
已 reduced Record 不重放。

## 11. 验收标准

### 11.1 AUTOMATED GATES

| 测试主题 | 必须证明的外部合同 |
|---|---|
| A→B authoritative 时序 | A Gameplay/Resolution 完成后 B 才 accepted；A DamageNumber 可仍 alive；无并发 Gameplay |
| Reducer determinism | G8 on/off、不同 cosmetic tick/finish 时机不改变 committed Record/trigger/reducer/FinalSnapshot 顺序 |
| One-way cosmetic lane | G8-D 后 Damage finish/timeout 不调用 Controller/VM/readiness/ownership mutation |
| Exact session/token | 旧 HUD/session callback 不能删除新 session instance，也不能影响新 input surface |
| Prepare transaction | 每个 pre-commit failure 无外部副作用并回 Blocking；post-publication stale 只丢 visual，不重播 reducer |
| Mode-specific readiness | Normal/Selection/Target grant 不串模式；Selection/Target transient state 不被 normal finalizer 清掉 |
| Shadow parity | G8-B shadow 与旧实际 input/mode 结果无未解释 divergence |
| Read-before-envelope | latest read 提前、Envelope 尚未 chronological catch-up 时不能开放普通输入 |
| Selection/target | surface 未 ready 禁止提交；ready 后只开放对应 continuation，不开放普通请求 |
| Damage isolation | old cosmetic 不恢复 opacity、不回写 HP/Block、不改 current Hand/target state |
| Lifetime/GC | finite duration/hard timeout、GC-safe owner、重复 cleanup 幂等、session-wide cleanup 正确 |
| Sanity ceiling | ceiling 在 pre-commit 阶段导致 detached decline；不 eviction、不重复 reducer |
| Recovery | reconcile/Skip/direct/unavailable/terminal/replacement 后无 ghost cosmetic、无永久锁 |
| G0–G7 regression | Selection ownership、G6 Group、SingleRecord fallback、exact cancellation/recovery 不受 cosmetic lane 影响 |

主时序测试使用真实 Gameplay Request + committed Envelope。测试 clock/timer injection 只用于
可控边界，不以模拟 job 存活冒充真实 PIE 体验。

### 11.2 MANUAL PIE GATES

Native `L_BattleTest` 至少验证：

```text
打出攻击牌 A
→ DamageNumber 出现
→ 在 A 数字仍可见时，exact normal surface ready
→ 立即合法打出 B
→ A 数字继续自己的生命周期
→ B 正常 CardPlayed / Gameplay
```

同时观察：

```text
HP/Block 不回跳
formal target opacity 不被旧数字恢复/覆盖
Hand 不闪回/重复
点击不会因为只有 cosmetic tail 而触发 Skip
无 ghost DamageNumber
无 stuck input
```

另验证：Draw→Selection 必须先等正确 Hand/Selection surface 后才能选择/确认；Target
choose/cancel 模式不被 old cosmetic finish 清理；G6 multi-select 仍保持同时播放；Skip、
终局、窗口/DPI 变化后无残留。

记录默认 DamageNumber duration、实际 input-ready 时间、后续 Blocking card-tail duration。
若默认时序没有实际跨牌视觉重叠，应如实记录，不通过人为延迟 cosmetic 或缩短 Blocking
路径伪造体验收益。

## 12. 本次设计冻结点与未授权范围

本次修改仅调整设计，未修改 C++、未运行 UE Build/Automation/PIE，也没有新增运行验收证据。

当前首版冻结候选：

```text
Blocking Presentation lane 保持 G0–G7
Detached cosmetic lane 首版只有 DamageNumber
最终 DamageNumber completion 对 authoritative/input lane 无反向边
InteractionMode 是 readiness 核心；不建通用 barrier registry
readiness grant 必须 mode-specific
Presentation session 有单一 mint authority
prepare/reducer/publication/activation 有两次 exact recheck
有限 lifetime + hard timeout 是资源硬合同
数量 ceiling 只是高位 bug-containment，不是正常 pacing 上限
首版无 eviction
card tail / hit flash / status VFX 不在首版范围
```

CardPlayed、PlayArea card tail、Selection transitions、Draw→Hand、G6 Group 等未来是否变成
cosmetic/overlapped path，需要新的独立设计与 acceptance，不因 G8 DamageNumber seal 自动获得
授权。