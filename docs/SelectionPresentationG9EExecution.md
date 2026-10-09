# G9-E：集成与清理

2026-10-09，分支 `codex/g9-buffered-input-detached-cards`，起点 `3922055`。
D2 COMPLETE／VALIDATED／默认开启，B／D1 默认开启保持。本轮仅 E，不进入
F，不宣称 G9 整体封板。不改资产、Legacy、插件或依赖；保留外部 Native
HUD 资产原 SHA256，排除提交。每个完整批次测试／文档后本地提交，不 push。

## 范围和顺序

按锁定 G9 设计的 E 集成矩阵执行，不增加新功能。复用已验收的 B／C／
D1／D2 状态、数值和视觉证据；只补足组合边界及本次清理影响的证明。

1. 保存本契约、矩阵和继续点，独立文档提交。
2. 收敛输入退役通知及 ViewModel／Controller 替换边界，移除已重复的
   单实例状态；规定生成／Development Editor 构建、一次聚焦自动化、
   必要 Native PIE，记录实际范围／结果并独立提交。
3. 只有 E 门槛全部满足才标记 COMPLETE／VALIDATED。F 另行开始；人工
   无法完成的项目明确 USER ACTION REQUIRED，不能提前封板。

## 本次收敛的边界

- 队列清理同步清除草稿／FIFO／旧 EndTurn，并通知 Native 输入表面；不得
  通过 Tick 或新的 Ready 事件才恢复指针牌／手势。已提交的 Gameplay
  正常完成，提交回合收据仍按已验收 B 规则保留；通知不消费队列。
- 有效 Session 失效同步复核意图凭据并退役过期输入及私有视觉；实际绑定
  退出清空待执行输入。DirectBaseline 的普通 Ready 没有旧有效 Session，
  不清除同回合合法 FIFO／EndTurn。开关关闭保留必要正式
  关联；Skip／恢复折叠对应历史才清理关联。三项开关继续独立控制，不
  新增总开关，不改变 Session 或 PlayerTurnSerial 来实现关闭。
- ViewModel 更换先撤销旧观察／输入绑定、退役旧 tracked 播放及 Controller
  所有权，再安装新模型。旧完成／取消／计时器不能修改新表面。重入期间
  较新的绑定优先，跨回调固定 UObject 存续；新 Controller 由正常绑定接口
  安装，不能把旧战斗 Controller 静默用于新模型。
- Hosted Blocking 完成由当前精确播放 token 在 job 中定位唯一视觉，再
  走现有视觉 token 校验；移除重复的 HUD 活动视觉 token。
- Draw 的准确附着对象只由 IncomingHandAttachment 持有；移除重复抽牌
  Widget 镜像、未使用目标索引与一次性 After 计数成员。保留当前 Blocking
  渲染器及其仍使用的动画参数，保证关闭／资源 decline／独立 R8 路径。

## 自动化集成矩阵

| 边界 | 必须证明的结果 |
|---|---|
| 输入清理＋D1／D2 关闭 | 忙碌期间已有已确认牌和指针草稿，关闭后立即退役表面；原牌去向继续提交一次，后续牌不扣费，Session／回合 serial 不变。 |
| Skip／恢复 | 卡牌视觉、草稿、FIFO、EndTurn 和临时附着清空；正式折叠准确；旧回调不恢复输入或视觉。 |
| HUD／Controller／ViewModel／Battle 替换 | 清理旧所有者，旧完成／取消及延迟评估无效，新模型／手牌身份及正常请求不受影响。 |
| Blocking 与 draw 附着 | 唯一 job 的精确完成、缺失视觉安全完成、旧 token 拒绝；draw 精确接管／取消／GC 无重复 slot／请求。 |
| 同实例／视口／GC／销毁 | 正式 Hand 优先；旧 job 不能复活，缩放或几何失效不消费输入或历史；所有引用与取消准确。 |
| G8／Selection 组合 | FastInput 关闭回退、EndTurn 仲裁／DirectBaseline／ABA，DamageNumber 共存、G6 选择和冻结卡面保持既有协议。 |

复用既有精确证明，回归受影响 G9B／C／D1／D2、Native R8／FastInput／
HandInteraction、G8A／B／D、Selection G0／G4／G5／G6／G7 及
CardSelection.Presentation／冻结 CardPlayed 卡面。报告只记实际执行结果，
不固定总数、不累加重叠报告。失败只重跑受影响门槛。

## 中文生产 PIE 验收

地图 `/Game/SlayTheSpireDemo/Maps/L_Battle_RuinedCitadel`，Native 生产 HUD。

| 操作步骤 | 预期现象／通过条件 |
|---|---|
| 打出带抽牌的牌，播放期间确认下一张牌、再选指针牌草稿；在当前牌尚有 Blocking 历史时关闭 D1 或 D2。 | 草稿立即回手、目标／手势取消，队列不继续出牌；当前牌结算与去向正常，关闭后的后续普通输入仍可用。 |
| 播放期间准备草稿与确认队列，执行 Skip；之后正常选牌出牌。 | 无指针牌、目标箭头、幽灵或重复请求残留；恢复的手牌可交互，数值只提交一次。 |
| 在本批受影响的 draw／Blocking 回退路径完成一次普通出牌、抽牌及选择；缩放观察复用已通过 D2 实际视觉。 | 抽牌对象完成后正常接管，剩余手牌排列稳定，选择／伤害数字与私有卡牌视觉共存，无输入卡死。 |

身份、计数及不可通过界面的替换／销毁组合由自动化证明。已有 D2 窗口
缩放 USER_REPORTED_PASS 及其余未改变的视觉不重复要求人工操作。

## 当前状态

2026-10-10，**E COMPLETE／VALIDATED**。契约提交 `e82595b`；验证时 HEAD
为 `e82595b` 加本批最终 Native C++／测试工作区。实现和本证据一起本地
提交，交付编号由本文件后续交付记录补齐。B／D1／D2 默认保持开启，
F 未开始，G9 整体 NOT SEALED。

## 最终实现

- `DiscardQueuedPlayerInput` 清理后同步通知 Native，立即释放指针姿态、
  草稿／手势和目标表面；通知本身不调度或消费请求。有效 Session 失效
  只复核精确凭据，真正 Shutdown／Widget 退出才清空全部待执行输入。
- ViewModel／Controller 更换先断开旧绑定，再退役 tracked 播放、计时器
  及私有卡牌视觉，最后安装新对象。过渡期间拒绝输入；绑定 generation
  保证同步通知中安装的较新模型优先。跨回调强引用旧／新对象，避免 GC
  后仍使用裸指针。重复的空所有者退出通知安全处理。
- 删除 `ActivePlayedCardVisualToken`、`ActiveNativeDrawnCardWidget`、
  `ActiveNativeDrawCountAfter`、`ActiveNativeCardDestinationIndex`。Blocking
  完成从当前播放 token 定位唯一 job，再使用其精确视觉 token；draw
  只从 `IncomingHandAttachment` 取得附着对象。保留必要 Blocking 回退。
- D1／D2 关闭先退役对应私有视觉，再清理输入表面；正式去向关联仍按
  原契约保留。动画 Tick 不消费输入、不完成 Controller、不重放 reducer。

## 实际构建与自动化证据

所有构建均先按根规则生成工程，再使用 UE 5.8 Development Editor 命令。
日志时间为 UTC 2026-10-09；本地验收日期为 2026-10-10。

| 批次 | 生成／构建秒数 | 实际日志与结果 |
|---|---|---|
| 初版清理 | 8.34／337.07 | `Saved/Logs/G9EProjectFiles.log`、`G9EBuild.log`，成功。 |
| 增补 HUD 替换及 EndTurn 过渡检查 | 5.52／10.74 | `G9EMatrixProjectFiles.log`、`G9EMatrixBuild.log`，成功。 |
| 修正 Session／DirectBaseline 边界及 GC 夹具 | 6.46／332.92 | `G9ERepairProjectFiles.log`、`G9ERepairBuild.log`，成功。另有仅生成的 `G9EFixtureProjectFiles.log`（6.19 秒），不计作构建。 |
| 最终空所有者检查 | 7.12／14.09 | `G9ENullOwnerProjectFiles.log`、`G9ENullOwnerBuild.log`，成功。 |

首轮 `Saved/Logs/G9EAutomation.log` 记录 107 项成功、2 项失败，随后在
新增替换／GC 用例中崩溃，进程返回 3，未导出完整报告，不能称为首轮通过。
两个失败分别为 `G9B.Queue.NextTurnContinuousQueue` 和
`G9B.EndTurnBusyAndDirectBaseline`：初版把无有效旧 Session 的每次 Ready
也当作退出而清空输入。最终实现区分凭据失效和实际绑定退出，保留原断言。
崩溃来自测试夹具在 HUD 解绑后仅用裸指针持有 Controller；夹具改用
`TStrongObjectPtr` 保留旧回调发送者，继续执行真实 GC 和旧回调检查。

修正后只重跑受影响 Session／绑定／卡牌夹具及首轮未完成范围：

- `SelectionPresentation.G9B/G9C/G9D1/G9D2/G9E`；
- `Phase6UIA2N.R5/R8/FastInput`；
- `SelectionPresentation.G8A/G8B/G8D/G0/G4/G6/G7`；
- `UIA3.CardPlayedRichHandoff`，均带 `SlayTheSpireDemo.` 前缀。

报告 `Saved/AutomationReports/G9ERepair/index.json`：**90 项，86 成功、
4 项带既有预期警告通过、0 失败、0 未运行**；对应
`Saved/Logs/G9ERepairAutomation.log`。警告为 R8 无效身份拒绝及三个 G8A
空起始牌组夹具，不是新增运行时故障。新增 6 项 E 集成用例全部成功。
首轮日志中的 `HandInteraction` 13 项、`CardSelection.Presentation` 13 项
（含 G5）成功，后续修正没有影响它们的契约或夹具，保留该有效证据。
不与后续报告相加。

最终只补 `G9E.BindingBattleAndDestructionMatrix` 的空所有者重复通知：
`Saved/AutomationReports/G9ENullOwner/index.json`，**1 项成功，0 警告／
失败／未运行**；`Saved/Logs/G9ENullOwnerAutomation.log`。其余已通过
范围继续有效，不累加或宣称重复全量验收。

## 本批生产 Native PIE 结果

UE MCP 启动生产 `L_Battle_RuinedCitadel`，实际 HUD 无启动覆盖读回
B=true／D1=true／D2=true。临时观察器只通过正常 HUD 请求及公开运行时
开关操作，读取冻结 DTO、正式 Widget 和私有 host 的公开子项；没有
修改 Gameplay、资产、历史、动画参数或速率。身份和次数以自动化为准，
画面只证明本次组合表面。首个观察器因 Python 属性名错误在任何请求前
失败；纠正为 `input_locked` 后重建观察，不把该失败计作有效验收。

| 操作步骤及实际观察 | 结果与证据 |
|---|---|
| 剑柄打击播放／抽牌期间确认打击，再选燃烧草稿，关闭 D2。调用前草稿有指针变换；调用内恢复零偏移／原比例，host 立即清空。A 完成时能量 4、弃牌 1，打击仍在 Hand；重新确认后能量 3、弃牌 2、Idle，无残留视觉。 | 通过。`Saved/G9EVisualEvidence/DisableD2Timeline.json`、`DisableD2Complete.png`。`AfterBoundary` 截图是异步捕获，已包含随后正常打击入场，不单独作为关闭瞬间证据。D1 关闭的同类协议由新增自动化证明。 |
| 同样的确认牌＋燃烧草稿期间执行 Skip。调用内为 Idle、能量 4、弃牌 1、host 空；目标框／取消表面消失，草稿回到扇形槽。之后正常打击支付一次，能量 3、弃牌 2，剩余手牌可悬停。 | 通过。`SkipTimeline.json`、`SkipDraft.png`、`SkipImmediateBoundary.png`、`SkipComplete.png`。草稿截图显示伤害数字 9 与私有卡牌视觉共存；清理画面已人工查看。 |
| Skip 后正常打出战吼，完成 Blocking 抽牌；点击已抽入的防御并确认强制选择。结束回合保持禁用；确认后遮罩、选择视觉和旧附着退役，能量 3、弃牌 2、消耗 1，Hand 为 `2,5,9,8`，Idle、输入解锁、反馈为空、host 空。 | 通过。`Saved/G9EConfirmWarcry.json`、`Saved/G9ESelectionChosenSnapshot.json`；`SkipTimeline.json` 的 `Warcry_selection_complete`；`WarcryChosen.jpg`、`WarcryComplete.png`，已查看实际画面。 |

以上图片／时间线位于 `Saved/G9EVisualEvidence/`，编辑器日志为
`Saved/Logs/G9EPIE.log`。旧 D2 运动中窗口缩放 USER_REPORTED_PASS 复用；
未改变的 B／C／D1／D2 人工门槛不重开。本批没有剩余人工待办。

PIE 已停止并读回 false；自有编辑器 PID 21352 经核对命令行后在 All Saved
状态关闭，没有资产保存。未发现本次资产／Blueprint 加载错误。外部
`WBP_BattleHUD_Native.uasset` 的 SHA256 仍为
`F896F7B59A91D4EC86AD73973451E19012B7523EE7D1D1ECEE2B570894E20F81`，
保留且排除提交。`Saved/` 的临时脚本、报告、截图、日志及生成文件均不提交。
