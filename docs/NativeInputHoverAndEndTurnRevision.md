# Native 持牌悬停与结束回合修订

日期：2026-10-06。实现基线 `792f6e2`，最新文档基线 `fee0790`。
继续当前 G9 分支，仅修改 Native C++、测试和文档，不进入 G9-C–F。

## 用户反馈与当前约定

用户反馈：上一份清单的 1、3、4、6、7 没有问题，2、5 的原标准也通过。
记录为 **用户人工反馈通过（USER_REPORTED_PASS）**。用户未提供实际运行
提交、G9 配置、视口或录像，不补写这些信息，也不冒充本轮工具验收。

用户新增两条规则，优先于早期 G9-B 文档：

1. 选中一张跟随鼠标的牌时，其余手牌不悬停抬升／放大。取消持牌后恢复
   正常悬停。仅等待出牌的已确认视觉不会阻止玩家检查另一张牌。
2. 结束回合先验权，成功后清空未执行的确认队列、临时草稿和 FastInput
   重试；已提交 Gameplay 的当前牌正常完成。同一回合只提交一次结束请求，
   旧回合演出期间的重复点击不能预订下一回合。

## 职责与修复

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

## 本轮验证

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

## 中文人工验收清单

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
