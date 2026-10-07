# Native 持牌悬停与结束回合修订

2026-10-07：用户确认包括队列后结束、防重复结束及强制选择清空旧输入的
剩余组合验收均已完成。本清单 B／C／D 记录为 USER_REPORTED_PASS，
不再待验；[G9-B 收口](G9BClosure.md) 为当前阶段状态，下文待验说明为历史。

最新故障修正（基线 `6b0ab9c`）：用户报告从第二回合开始第一张牌播放后
后续牌被拒绝，原因是旧结束凭据没有在新回合就绪时退役。当前生命周期、
实际回归证据及第二／第三回合中文验收见
[第二回合连续出牌修复](G9NextTurnInputFenceFix.md)。下文队尾与悬停约定仍适用。

## 最新用户澄清：攻击瞄准与结束回合队尾屏障

本轮实现基线 `925aff2`。用户澄清后，当前约定为：

1. 任意普通手牌被选中时，包括攻击牌瞄准，其他手牌都不悬停突出、放大
   或抬升。选中的攻击牌自身保持瞄准抬升和箭头；右键取消后恢复普通悬停。
2. 结束回合保留按钮点击前已经确认的出牌命令，按原顺序完成后只结束一次。
   接受按钮点击后禁止追加新出牌；未确认的临时草稿仍需取消，不能替玩家
   补目标或自动确认。已经取出的早期命令若暂时忙碌，其重试仍位于屏障之前。
3. 保留上一批的提交回合凭据防护，禁止重复点击预订未来回合。强制选择
   继续清空未执行输入和旧结束意图，不恢复过期队列。

上述规则取代 `925aff2` 的“取消全部待执行确认牌”和“仅指针持牌抑制悬停”。
使用既有 InputSequence／结束意图序号界定前后；不改变 Gameplay、历史
reducer、Blocking 时序或手牌结构。下文是上一批的执行历史，不能视为新
队尾行为的通过证据。本轮实际验证和最新中文清单记录在文末。

日期：2026-10-06。实现基线 `792f6e2`，最新文档基线 `fee0790`。
继续当前 G9 分支，仅修改 Native C++、测试和文档，不进入 G9-C–F。

## 上一批用户反馈与历史约定（已被上述澄清取代）

用户反馈：上一份清单的 1、3、4、6、7 没有问题，2、5 的原标准也通过。
记录为 **用户人工反馈通过（USER_REPORTED_PASS）**。用户未提供实际运行
提交、G9 配置、视口或录像，不补写这些信息，也不冒充本轮工具验收。

用户新增两条规则，优先于早期 G9-B 文档：

1. 选中一张跟随鼠标的牌时，其余手牌不悬停抬升／放大。取消持牌后恢复
   正常悬停。仅等待出牌的已确认视觉不会阻止玩家检查另一张牌。
2. 结束回合先验权，成功后清空未执行的确认队列、临时草稿和 FastInput
   重试；已提交 Gameplay 的当前牌正常完成。同一回合只提交一次结束请求，
   旧回合演出期间的重复点击不能预订下一回合。

## 上一批职责与修复

手牌面板根据当前选中 RuntimeId 是否处于指针视觉控制，关闭其余牌的
悬停命中，并立即归还已抬升卡牌的渲染变换与绘制层级。槽位、冻结序号和
Slate 基础布局不变；强制选择和单体攻击瞄准保留自己的交互。

结束回合的旧问题来自：Pending 意图在请求前已取出，而 Gameplay 可以
同步进入下一玩家回合；界面仍在播放旧历史时，下一次点击读取到了新回合
权限。输入所有者现在在取出结束意图前保存精确提交凭据，覆盖请求发布
重入和整个显示滞后窗口。新回合与该凭据不同，且已有精确追平的正常输入
就绪证据时，才允许下一次新输入。使用既有 Ready／Presentation／ViewModel
通知，不用时间冷却或 Tick 消费，也不复制 Gameplay 回合状态。

Pending 清理不会抹掉已提交结束凭据；只有当前精确的拒绝回执可释放它。
绑定替换重建输入所有者的凭据范围。结束回合取消队列时递增输入 generation，
避免已取出的忙碌卡牌重新入队，同时保留当前已提交牌的完成等待。

## 上一批验证（925aff2）

规定的工程生成与 UE 5.8 Development Editor 构建通过。初次构建 75.99 秒；
最终受影响夹具构建 6.51 秒通过。日志为
`Saved/Logs/G9EndTurnHoverProjectFiles.log`、`G9EndTurnHoverBuild.log`、
`G9EndTurnHoverFinalProjectFiles.log`、`G9EndTurnHoverFinalBuild.log`。

一次聚焦自动化选择 **77 项**：HandInteraction、G9-A／B、G8-B、FastInput、
R8、冻结 CardPlayed 卡面和受影响的 Selection G0／G4／G5／G6／G7。
结果为 75 项成功、1 项预期 R8 警告、1 项失败，无未运行项目。报告：
`Saved/AutomationReports/G9EndTurnHover/index.json`，日志
`Saved/Logs/G9EndTurnHoverAutomation.log`。新增持牌悬停、重复结束回合、
队列撤销、精确拒绝回执、发布重入及新回合重新可用测试均通过。

失败为旧 `HandToDraw.GenericTransferFinishAndCancel` 夹具没有保留／分配
起点 Slate 几何，被已生效的出牌起点契约拒绝。只修复该夹具的准备条件，
未放宽生产校验。单独重跑 **1/1 通过**：
`Saved/AutomationReports/G9EndTurnHoverRepair/index.json`、
`Saved/Logs/G9EndTurnHoverRepairAutomation.log`。因此本轮 77 个不同测试
均有有效通过证据，不能表述为一次完整 77/77 运行。

MCP 实际启动生产地图 PIE，读回 Native HUD 的 G9=true，来自保留的用户
资产；C++ 默认仍关闭。MCP 返回的点击几何越出可见窗口范围，因此使用
实际 Windows 窗口输入补充验证。生产按钮快速连续三次点击后：演出中按钮
禁用、VM 为 Resolving／锁定；演出结束后 Idle／解锁、能量 5、玩家 HP 65、
手牌 5、显示 Revision 9。日志只出现开场 Revision 4 和一次下一玩家回合
Revision 9，没有持续推进。**仅该快速连续点击场景通过**，不代替其他阶段
的重复点击矩阵或队列取消／持牌悬停完整轨迹。

证据：`Saved/Logs/G9EndTurnHoverPIE.log`、`Saved/G9EndTurnHoverConfig.json`、
`G9EndTurnHoverRepeatedVM.json`、`G9EndTurnHoverFinalVM.json`、
`G9EndTurnHoverSmoke.png` 和本任务的实际窗口截图。PIE 已停止，观察器已
移除，编辑器保持打开。用户 Native 资产 SHA256 与修改前一致，未加入提交。

## 上一批中文人工验收清单（保留为历史，不继续执行 B／D）

生产地图：`/Game/SlayTheSpireDemo/Maps/L_Battle_RuinedCitadel`，Native HUD。
排队和结束回合修订需要实际开启 G9。以下均为 **待人工验收
（USER ACTION REQUIRED）**，旧标准的反馈不能替代新增行为的验证。

| 项目 | 操作步骤 | 通过条件 |
|---|---|---|
| A. 持牌时抑制其他悬停 | 选中战吼或燃烧，依次将鼠标移到其他手牌上；再右键取消并重新悬停。 | 持牌只跟随鼠标；其他牌不突出、放大或抬升。右键取消后恢复普通悬停，顺序和槽位正常。 |
| B. 结束回合取消排队 | A 播放时确认 B、C，随后点击结束回合。 | B、C 不自动出牌，也不支付出牌成本；当前已提交的 A 正常完成，余下手牌按回合结束规则弃牌。 |
| C. 重复点击不连续结束 | 在项目 B 中多次快速点击结束回合，并在旧回合、弃牌、敌人行动及新回合抽牌演出期间再次尝试。 | 只结束一次；新回合不会因旧点击自动结束。新回合完整显示并恢复可操作后，新的明确点击才允许再结束一次。 |
| D. 强制选择回归 | 不提前点击结束回合；排队一张产生强制选择的牌及后续攻击牌。等强制选择出现，尝试结束回合，再选择其他牌完成选择。 | 结束回合不可点击；后续攻击牌不自动执行，完成选择后仍在手牌，同一玩家回合继续。提前点击结束回合的旧时间线已被“取消未执行队列”取代。 |

反馈请记录项目编号、通过／失败、实际提交、G9 开关和视口。轨迹／重复
点击项目建议保留连续录像。本轮不自动启用默认开关，不宣称 G9 封板。

## 最新澄清实现与实际验证（基线 925aff2）

本批随独立本地提交 `fix(g9-b): preserve pre-end-turn plays and suppress aiming hover`
保存；精确提交用 `git log -1 --format='%h %s'` 核实。工作区中的用户 Native
资产仍独立保留，不纳入提交；没有修改资产、配置、Legacy、插件或依赖。

面板统一以普通选中 RuntimeId 控制突出状态，包括 Enemy 攻击牌瞄准。
只改变渲染变换和绘制层级，保留原静止命中条带及切换选牌能力；选中的
攻击牌继续抬升，指针持牌继续由自己的视觉协议控制，取消后恢复悬停。

输入所有者接受结束回合时保留已确认 FIFO 及其绑定 generation，只清理
未确认草稿。结束意图使用同一个输入序号作为屏障；屏障之前已取出但暂时
忙碌的精确命令可以归还队首，之后的命令不能穿过屏障。队列逐张等待
Gameplay 与历史完成后执行，再提交一次结束回合；已有提交凭据继续阻止
发布重入和旧历史窗口中的重复点击。强制选择仍丢弃所有未执行输入。

规定的工程生成和 UE 5.8 Development Editor 构建通过。初次生成 4.10 秒、
构建 6.23 秒；最终保留命中切换后，生成 3.80 秒、构建 **5.85 秒**。
日志：`Saved/Logs/G9EndBarrierProjectFiles.log`、`G9EndBarrierBuild.log`、
`G9EndBarrierFinalProjectFiles.log`、`G9EndBarrierFinalBuild.log`。

一次聚焦自动化选择 **53 项**：HandInteraction、SelectionPresentation.G9A／
G9B／G8B、Phase6UIA2N.FastInput／R8、CardSelection.Presentation.Input。
结果 **52 成功、1 项预期 R8 警告、0 失败、0 未运行**。报告：
`Saved/AutomationReports/G9EndBarrier/index.json`；日志：
`Saved/Logs/G9EndBarrierAutomation.log`。保留命中切换的最终调整后，只重跑
受影响的 HandInteraction 与 G9B.StableHandAndHover，**8/8 通过**：
`Saved/AutomationReports/G9EndBarrierHoverFinal/index.json`、
`Saved/Logs/G9EndBarrierHoverFinalAutomation.log`。本批 53 个不同测试均有
有效通过证据，不能表述为最终版本的一次完整 53/53 运行。

覆盖此前 B／C 按序执行、草稿 D 取消、屏障后不追加、早期忙碌命令归还、
重复结束与发布重入、精确拒绝回执、新回合重新可用、强制选择丢弃 C 和
旧结束意图，以及攻击瞄准／指针持牌的邻牌变换和取消恢复。

生产地图实际 Native PIE，用户保留资产的实例 G9=true（读回
`Saved/G9EndBarrierConfig.json`），C++ 默认仍关闭。使用实际窗口输入选中
打击，再从空白处将鼠标拖到其他手牌上：打击保持瞄准抬升及箭头，其他
手牌维持原扇形位置；右键取消后，指针下的邻牌重新正常抬升。**仅此攻击
瞄准悬停／取消场景通过**，截图见本任务工具记录，取消后 VM 为 Idle、
SelectedCardRuntimeId=-1、Energy=5、bInputLocked=false，读回文件
`Saved/G9EndBarrierCancelVM.json`。地图／启动／演出日志为
`Saved/Logs/G9EndBarrierPIE.log`，MCP 启动记录
`Saved/G9EndBarrierStartPIE.json`。

随后尝试播放连击并确认后续牌时，截图刷新间隔内抽牌已推进，手牌位置
发生变化，未取得受控的“动画期间 B／C 入队再结束”轨迹；不将该尝试
记作队尾视觉通过。PIE 已停止并读回 false（`G9EndBarrierPIEStopped.json`），
观察器已移除（`G9EndBarrierUnobserve.json`），编辑器保持打开。
用户资产 SHA256 仍为
`F896F7B59A91D4EC86AD73973451E19012B7523EE7D1D1ECEE2B570894E20F81`。

## 最新中文人工验收清单

生产地图：`/Game/SlayTheSpireDemo/Maps/L_Battle_RuinedCitadel`，Native HUD，
实际开启 G9。本清单取代上文历史 B／D 步骤；未完成项目为
**USER ACTION REQUIRED**，不扩大到 G9-C–F，不自动启用默认或封板。

| 项目 | 操作步骤 | 通过条件 | 当前证据 |
|---|---|---|---|
| A. 攻击瞄准抑制其他悬停 | 选中打击但不点目标，将鼠标依次移到其他牌上；右键取消后再悬停。技能／能力牌可沿用原持牌步骤补充。 | 只有选中攻击牌保持抬升和箭头，其他牌不突出、放大或抬升；取消后普通悬停恢复，顺序和槽位正常。 | 打击的邻牌悬停／取消窄场景实际通过；其他牌型沿用原反馈，扩展复验待反馈。 |
| B. 结束回合排在此前确认牌之后 | A 播放时先完整确认 B、C，再选中 D 但不确认，随后点击结束回合；尝试追加另一张牌。 | B、C 按原顺序正常打出，D 草稿取消，之后不能追加；此前命令完成后只结束一次。若某项已永久失效，按原规则提示并跳过，不自动换目标。 | 自动化通过；完整视觉轨迹待人工验收。 |
| C. 重复点击不预订未来回合 | 在 B 的队列等待、弃牌、敌人行动及新回合抽牌演出期间反复尝试结束回合。等新回合完整恢复可操作后，再明确点击一次。 | 等待与旧演出期间的重复点击都不额外结束；只有新回合恢复后新的一次明确点击才结束下一回合。 | 本批自动化通过；上一批快速三次点击窄场景通过；完整阶段矩阵待人工验收。 |
| D. 强制选择清空旧输入 | A 播放时确认战吼及后续攻击牌，然后接受结束回合；等战吼强制选择出现，尝试结束回合，再选择其他牌完成选择。 | 战吼先于结束意图执行；强制选择清空后续攻击和旧结束意图，结束回合不可点击；完成选择后同一回合继续，旧输入不会恢复。 | 自动化通过；完整视觉轨迹待人工验收。 |

反馈记录项目编号、通过／失败、实际提交、G9 开关和视口；B／C／D 建议
提供连续录像。用户此前 1／3／4／6／7、2／5 原标准的反馈继续保留为
USER_REPORTED_PASS，不能替代本次新增时间线。
