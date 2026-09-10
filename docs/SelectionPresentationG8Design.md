# G8 详细设计：独立伤害数字与精确输入就绪

日期：2026-09-10。状态：**DESIGN PROPOSAL / NOT IMPLEMENTED / NOT SEALED**。

本次用户授权审查所贴方案并记录详细设计，仅涉及文档；不代表授权实现 G8。
审查代码基线：`2e67ce7596d4d5856367ac63d39710516f798dee`。
G0–G7 当前阶段状态以 [G7 状态修订](SelectionPresentationG7SealAmendment.md) 为准。
原方案附件开头的“只讨论、不改文档”属于被审查文本；本次明确写文档的请求优先。

本文细化并修订 GroupDesign 第 30 节和 GroupImplementationPlan 第 18 节的
未来 G8 候选方案。涉及独立 token、首版 allowlist、输入模式与实施顺序时，以本文
为后续实现提案依据；不改变现行 G0–G7 运行合同，也不授予卡牌尾动画 detach 权限。

## 1. 审查结论与修改意见

赞同原方案的核心分离：Gameplay、Record reducer、visual job、interaction readiness
有各自的生命周期。保留现有 Blocking Record/Group 路径、先尝试可回退的 detached
路径，以及首版只放开独立 DamageNumber，都适合当前代码。

需要补齐以下合同，否则架构图正确也可能在运行时产生死锁或旧回调覆盖新状态。

| 原建议 | 本设计调整及原因 |
|---|---|
| Pending Selection、Target choice 都 Blocking | 对普通出牌/EndTurn 阻塞，但准备好的选择/目标操作必须可用；不能把等待玩家选择本身设为全局输入屏障。 |
| readiness 只有 BattleId + Revision | 对外事实仍用这两个值；授权凭据还必须限定 HUD/Controller 会话与决策面 generation，同 revision 重建不能复用旧 readiness。 |
| detached completion 绝不回 Controller | 不得调用 Record completion；但 detached **Blocking** job 释放屏障后，需要请求一次延迟 readiness 重评，否则输入可能永不恢复。 |
| Detached + Blocking 保持原体验 | 只保证输入仍等待；reducer 和后续动画已可提前推进，因此 HP 更新时间、动画重叠与节奏仍会改变，需独立验证。 |
| TryStart 成功即 CompleteActiveRecord | 增加无副作用 prepare、隐藏注册、精确提交与重入校验；不能先显示数字再发现 reducer 无效。 |
| G8-E 才做 recovery/overload | 基础 cleanup、超时、会话隔离和资源上限在第一项生产 job 激活前就必须通过；后期只做综合验证。 |
| 只拆 DamageNumber 即显著提速 | 后续 PlayArea→pile 仍 Blocking，收益受其时长影响。验收必须记录真实重叠，不能承诺每张牌都明显提速。 |

不引入通用 barrier registry、第二套 reducer、Gameplay scheduler 或任意 Record
插件化 job 框架。先实现两个明确用途：私有 DamageNumber 生命周期，以及现有输入
入口的统一 readiness 检查。

## 2. 当前实现证据与迁移点

以下是本次本地源码审查结果，不是 G8 已实现的证明：

- `Source/SlayTheSpireDemo/Presentation/BattlePresentationController.cpp`：
  `StartNextRecord` 设置单一 active token 并等待 Widget；`CompleteActiveRecord`
  才应用 reducer、发布工作快照并前进。`CompleteActiveEnvelope` 应用自身 FinalSnapshot，
  队列空后刷新 live bindings。还有 Skip、reconcile、direct 等刷新入口，不能只改正常结束处。
- `Source/SlayTheSpireDemo/UI/BattleHUDWidgetBase.h`：Record/Group 共用一份
  `FTrackedPresentationPlaybackUnit`。应保留这条经过加固的 Blocking 协议。
- `Source/SlayTheSpireDemo/UI/BattleHUDWidget.cpp`：Damage 使用共享
  `Txt_DamagePresentation` 并把正式目标 opacity 设为 0.45；卡牌还有单一
  `NativePlayedCardWidget`。这些对象不能被旧 detached job 持有并回写。
- `Source/SlayTheSpireDemo/UI/BattleHUDViewModel.cpp`：
  `RefreshLiveInputBindingsIfCaughtUp` 已检查最新 frozen baseline、read snapshot 与 VM
  的精确 BattleId/revision。沿用此核验，不能用 job 数量替代它。
- `BattleHUDSelectionWidget.cpp` 中 `HasActiveNativePresentation` 还影响手牌刷新和
  已打出牌隐藏。新增 cosmetic job 不得改变这个旧方法的语义，否则会重新影响 G5/G6。

G6 已有安全并行 Group；它与跨 Resolution 的 cosmetic job 是不同生命周期。
首版不把文本动画硬塞进 G4/G6 卡牌 transition engine。该引擎保持服务卡牌；未来
卡牌扩展才按需要复用。此项明确收窄旧 G8-A 的“必须复用卡牌引擎”措辞。

## 3. 首版范围与体验边界

**NonBlocking allowlist：只有独立 DamageNumber。**

CardPlayed、所有卡牌区域迁移、G6 Selection Group、Draw→Hand、Shuffle、正式
Status/HP/Block/Energy 更新、Terminal 均不因 G8 自动成为 NonBlocking。
受击闪烁本版不迁移：detached 成功路径只显示独立数字；旧 Blocking fallback 保留
原有闪烁。这个视觉差异须纳入 PIE，不能称为完全视觉等价。

数字使用 frozen `IncomingDamage`，保持当前展示含义；不暗中改成 HP 损失。
完全被 Block 吸收和零伤害沿用现有有效 payload 语义，并单独测试。

正常普通出牌链仍是 A Gameplay 完成后才接受 B。交互选择可能发生在 Gameplay
暂停的合法 selection boundary；此时只允许该边界的 continuation request，不要求
整个原始卡牌逻辑先完成，也不允许另开普通出牌 Resolution。这是必须保留的例外。

伤害数字剩余寿命如果短于后续 Blocking 卡牌收尾，玩家可能看不到跨牌重叠。
不延迟 reducer、不拖长 Blocking 动画来制造演示。记录从 Gameplay 完成到输入
就绪的时间，以及就绪时数字是否仍活着；只能就实际支持的路径报告收益。

## 4. 所有者与身份

| 层 | 唯一责任 | 禁止事项 |
|---|---|---|
| Gameplay | 权威提交、请求复核、合法选择边界 | 等待 cosmetic job |
| Controller | Envelope/Sequence 顺序、唯一 reducer、FinalSnapshot、readiness 协调 | 让 job callback 完成 Record |
| HUD 私有 job owner | 强引用 transient widget、tick、精确清理、资源计数 | 改正式 HUD/VM、调用 Gameplay |
| ViewModel | frozen display、latest-only bindings、按请求模式核验输入 | 以历史 job 数据还原当前 Gameplay |

建议由当前 Native HUD 持有一个小型 job owner，不创建世界级单例。数据至少包含：

```text
VisualJobToken =
  PresentationSessionGeneration
  + BattleId + SourceResolutionId + PresentationSequence
  + LocalVisualGeneration

Job = Token + SourceFinalStateRevision（诊断来源，不是输入授权）
      + InteractionPolicy + FrozenDamageVisualSpec
      + GC-tracked transient Widget + elapsed/duration + lifecycle state
```

generation 由所属会话单调分配，不来自时间、指针或容器顺序；HUD/Controller 替换
必须换会话。同 Record 重新尝试分配新 generation。token 与 `FPresentationPlaybackToken`
类型分离，编译接口也不接受混用。若 future Record 有多个视觉子项，再增加显式 ordinal；
本版不预建多特效框架。

`UPROPERTY` 可达 owner/container 持有 UObject；不得只用原生数组或异步 lambda
捕获充当 GC 所有权。延迟回调仅持 weak owner + exact token，销毁后无操作。

## 5. Detached 启动事务与 reducer 顺序

保留旧 `PlayPresentationRecord` bool 合同；新增独立 prepare/commit 封装，具体命名
实现时决定。Controller 不直接绕过 WidgetBase 加固边界进入任意 Blueprint callback。

```text
当前 Record identity / payload / reducer preflight
  → candidate allowlist + 当前 Host/session/geometry 验证
  → prepare 隐藏私有数字、预留槽位、注册 exact job（外部不可见）
  → 复核 active cursor/session 未被同步通知替换
  → Controller 按现有 reducer 路径恰好提交一次
  → 在同次受控提交中发布正式快照和激活数字
  → Controller 前进；job 独立存活
```

preflight 复用同一 reducer 规则在快照副本验证，不写第二份 Damage 修改逻辑。
prepare 不移除正式 UI、不释放现有 job、不发送完成通知、不推进队列。
所有对外 publication 前完成内部一致状态，不能依赖 multicast 订阅顺序。
正常激活要位于“当前 reducer 提交之后、下一 Record dispatch 之前”的明确切点；
不要直接从原 `CompleteActiveRecord` 返回后激活，因为该函数会继续推进后续 Record。

失败分界：

- **提交前 decline**：完整 rollback，不留 widget/timer/barrier，然后走旧 Blocking 路径。
- **历史 payload/reducer 不合法**：沿现有 active-envelope reconcile；不是动画 fallback。
- **提交后 job 失败**：只丢弃私有数字；绝不重播 Record 或再次应用 reducer。
- **同步 Skip/replacement/reconcile**：精确 session/cursor 失效就终止原启动流程；
  不在恢复完成的新状态上继续执行旧 fallback。

Normal finish 不调用 `NotifyPresentationFinished`，不伪造成功 token。
job timeout/cancel 幂等删除自己的 timer/widget/entry。若其持有 Blocking 屏障，删除后
发送带 session 的 readiness-dirty 通知，并在延迟安全点重评；该通知不推进 Record。

## 6. 输入就绪：事实、凭据与操作模式

`InteractionReady(BattleId, Revision)` 表示精确当前决策面已追上权威公开状态。
它不是永远为真的历史最大值，也不是 Selection completion watermark。

内部建议凭据：`BattleId + Revision + PresentationSessionGeneration + SurfaceGeneration
+ InteractionMode`。SurfaceGeneration 只用于实际决策面失效/替换，不因每帧数字 tick
或普通 hover 递增。新请求接受、revision/battle 变化、HUD 替换、恢复启动均使旧凭据失效。
请求被拒且同一边界保持有效时，重新评估并恢复该边界输入。

首版仅在 chronological Controller 已追到最新合法边界、无待处理 Envelope 时授予
普通出牌 readiness；只有 detached jobs 可以留下。队列空本身不是充分条件：公开
Envelope 尚未投递时，必须由现有 baseline/read exact guards 拒绝旧 revision。

判定需要同时满足：

1. 当前模式被 Gameplay 正式 Query/Request 合同允许；不读取 mutable 对象重建历史。
2. VM 显示、latest frozen baseline、player-facing read 精确匹配。
3. 所有该模式必需的正式 Hand/target/selection surface 已完成同步且可交互。
4. 无 blocking playback、recovery 或未解除的 detached Blocking job 屏障。
5. 不在 Terminal / PresentationUnavailable；当前 session/surface 未被替换。

只有通过这些检查才调用既有 live binding refresh；refresh 仍保留内部 exact guard。
所有正常结束、ReadStateReady、Skip、reconcile、direct mode、初始化/替换的输入恢复
入口都必须走统一 policy，避免旁路。直接模式没有 job 也需要表面和请求模式核验。
不要每次 job tick 调用 refresh；它会清理瞬态选择状态。

| 交互模式 | 可以开放 | 必须禁止 |
|---|---|---|
| PlayerTurn 普通决策面 ready | 合法选牌、出牌、EndTurn | Query 不允许的请求 |
| Pending Selection 表面尚未追上 | 无选择提交 | 普通出牌、选择确认、EndTurn |
| Pending Selection 精确表面 ready | 选/取消选、合法 Confirm/Cancel | 普通出牌、EndTurn |
| Target choice 精确表面 ready | 合法目标选择及取消 | 绕过目标流程的新普通请求 |
| Terminal / unavailable / recovery | 既有明确允许的恢复控件 | 战斗请求 |

因此不能用“所有 Pending Selection 存在时 readiness=false”实现；它会让等待玩家的
屏障永远无法解除。也不能要求“有一张可打出的牌”才授予普通决策面 readiness：
无可打牌时仍可能允许 EndTurn。

旧 NonBlocking job 完成不更改 readiness；新请求造成的正常锁定不是旧 job relock。
同 revision 替换/故障可以使当前凭据失效，故不能宣称 readiness 全局单调。

## 7. TransientVFXHost 与视觉隔离

运行时在实际 Native Canvas 下创建独立 Host，不复用 Hand、PlayArea 或 SelectionArea。
Host 和所有子数字均不参与 hit testing、focus、拖放或选择按钮事件；层级需验证不会
遮挡确认/目标控件的可读性。数字使用独立 TextBlock 或专用轻量 Widget。

创建时从 frozen TargetPresentationId 匹配当前历史目标表面，验证 Battle/session 和
非零 geometry，以 AbsoluteToLocal 转到 Host 坐标，冻结起点。后续只按时间更新自己的
translation/opacity；不追踪当前目标指针，不恢复目标 opacity，不写正式 HP/Block。
目标缺失或 geometry 未 ready 时 decline 到 Blocking；不以世界 Actor 位置猜历史位置。

Viewport/DPI/Host geometry 改变时首版取消当前 cosmetic jobs，避免冻结局部坐标漂移。
正式 HUD dirty reconcile 和正常 FinalSnapshot publication 不清空 Host；否则跨 Resolution
的尾动画无法存活。只有明确 recovery 边界才全清理。

## 8. 资源限制与恢复

首版建议总上限 8，作为 Presentation 配置常量而非 Gameplay 规则；实际值可根据 PIE
可读性调整。每项 duration 有有限正值及硬 timeout，坏配置拒绝。先不做 pooling。

满载时 prepare 先验证新 job；新 **NonBlocking** job 成功提交时才淘汰最旧
NonBlocking job，顺序使用本地 creation generation。不得按 TMap 遍历或墙钟选最旧。
若没有可淘汰对象或当前阶段仅 Blocking，则 decline 到旧 Blocking 路径。
Blocking job 不被容量策略强行驱逐，避免把资源压力隐式变成提前解锁。

| 事件 | job 处理 | Controller/输入处理 |
|---|---|---|
| 正常 FinalSnapshot / Envelope 完成 | 保留合法私有 job | 正常顺序完成与 readiness 核验 |
| 单项 finish/timeout | 只清理 exact job | NonBlocking 无输入副作用；Blocking 仅通知重评 |
| active-envelope 失败 reconcile | 首版取消当前会话全部 cosmetic jobs | 仍保留合法后续 Envelope，应用该 Envelope 自身 FinalSnapshot |
| Global Skip / backlog collapse | 全部取消 | 保留现有明确 catch-up 合同，不制造 job success |
| HUD/Controller 替换或销毁 | 旧会话先失效再全部取消 | 新表面 readiness 重建；旧回调无效 |
| battle 替换 / direct-mode 切换 / unavailable | 全部取消旧会话 job | 走对应现有恢复/输入模式 |
| terminal | 清理尾动画 | 正式终局显示优先，不让旧数字覆盖结束画面 |

所有取消路径不触发 Gameplay ResolutionFault。NonBlocking job 错误只记有界诊断，
不将整个 Presentation 标记 unavailable。损坏历史 Envelope 仍按既有失败合同处理。

## 9. FastInput 兼容

保留 `HasActiveNativePresentation` 当前 blocking/card 语义，新增独立 job 查询。
只有 NonBlocking job 存在时，正常输入不能触发 Skip，不能清空尾动画。
仍有 Blocking presentation 时，既有 fast catch-up 可继续工作，但延迟点击重试要核验
原 Battle/session 与 RuntimeId、最新 Query、当前 selection/target mode；过期点击不得
在新战斗或新选择边界被重放。必要时丢弃重试，不能伪造请求有效性。

## 10. 实施阶段与回退

以下是建议执行切片，不表示本次开始实施。采用本文编号，替代原第 18 节 G8-A–F
的工作划分，保留原文作为历史候选。

1. **G8-A：私有 job 基础。** 独立 token、GC、Host、事务 decline、精确 cleanup、
   bounded capacity/timeout 全部先测试。生产仍走旧路径，无 early input。
2. **G8-B：统一 readiness。** 覆盖所有 refresh 入口和模式/session 失效；用影子判定
   对照旧行为，不提前解锁。Selection/target 正常可操作为硬门槛。
3. **G8-C：Damage 拆分，detached Blocking。** 正式数值 reducer-owned，数字只写自己。
   验证前后快照与 job 的提交边界、blocking job 结束后的 readiness 重评。
   明确允许视觉节奏改变，不宣称与 G7 完全等价。
4. **G8-D：仅 DamageNumber NonBlocking。** 前述失败路径全部通过后启用；验证真正
   A/B 请求顺序和视觉重叠，修正 FastInput 分类。
5. **G8-E：集成验证。** 多命中、零/吸收伤害、draw→selection、目标取消、EndTurn、
   skip/替换/终局/过载组合。不是到此才补基础安全性。
6. **G8-F：证据与封存。** 相关 Automation + Native PIE，通过才更新 implemented/sealed。

功能关闭时，新 Record 走原 Blocking 路径；若已在运行中，先取消私有 jobs、废弃旧
readiness 凭据并重评当前状态，不重放已 reduced 的 Record。无资产或 Gameplay 迁移，
回退不要求恢复 Legacy。

## 11. 验收标准

### AUTOMATED GATES（待实现后执行）

| 测试主题 | 必须证明的外部合同 |
|---|---|
| A→B 完整时序 | A Gameplay 已完成，A FinalSnapshot/reducer 正确；A job 仍活着时 B 真正被 Request 接受；无并发 Gameplay；不能只断言 input=false/true |
| 历史一致性 | 开关 G8、不同 tick 步长、延迟/重复 job callback 下，同初态/seed/输入序列得到相同权威结果及 Record 顺序 |
| exact callback | A 完成/超时不能修改 B 的 token、HP/Block、Hand owner、输入状态；同 Battle 同 revision 换 HUD 也拒绝旧回调 |
| 启动失败 | prepare 每个失败点无副作用 fallback；同步 Skip/replacement 不重复 reducer；提交后失败不重播 |
| readiness 旁路 | 最新 read 提前于 Envelope 投递时不解锁；所有 refresh 入口遵守模式；同 revision surface replacement 失效 |
| Selection/target | Draw 表面未追上不能选择；追上后 Confirm 可用且普通出牌仍拒绝；目标选择/取消不会被 job finish 清掉 |
| Blocking job | reducer 可前进但输入等待；finish/timeout 能精确释放屏障，旧 generation 不能释放新屏障 |
| 资源/GC | job 存活时 GC 不丢 widget；重复清理无害；上限、有限时长、确定淘汰顺序成立；满载 Blocking decline |
| recovery | active-envelope reconcile 保留后续 backlog；Skip、direct、unavailable、terminal、replacement 无残留 job/barrier |
| G0–G7 回归 | 只重跑直接受影响的 Controller/VM/input/正式 surface 合同；卡牌组顺序及 ownership 不被 cosmetic 查询影响 |

测试用真实 Gameplay Request 和 committed Envelope 证明主链；注入时钟/回调只用于
可控边界。协议测试可保留长寿命 job，不能以此冒充默认时长的真实 PIE 收益。

### MANUAL PIE GATES（实施后 USER ACTION REQUIRED）

Native `L_BattleTest`：攻击 A 的数字尚存时正常打 B，A 数字继续、B 正常执行；HP/Block
不回跳、Hand 不闪、点击不触发多余 Skip。记录默认数字时长、后续卡牌 Blocking 时长
和实际输入恢复点。若默认流程无重叠，应如实记录并调整设计评估，不能假称完成体验目标。

再检查：连续多命中数字可读；Draw→Selection 先等 Hand 就位，再能选择并确认；G6
同时消耗无回退；目标取消正常；快速点击/Skip、终局和窗口缩放无数字残留、无永久锁。

## 12. 本次交付与未决事项

本次仅源码/文档审查并写入提案，未修改代码、未运行 UE build/Automation/PIE，也没有
新的运行验收证据。文档检查按 ValidationExecutionPolicy 的 documentation-only 规则。

首版已明确收窄到 DamageNumber，不把卡牌尾动画、hit flash 或 status VFX 留作隐含任务。
后续实现前需据实际交互面选择 readiness 凭据的最小字段和具体 API；不得删减本文的
模式区分、同 revision 替换失效、事务 fallback 和单次 reducer 合同。数字时长及上限 8
属于待 PIE 评估的视觉参数，不是确定性 Gameplay 参数。
