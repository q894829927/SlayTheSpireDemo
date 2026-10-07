# G9-B 第二回合连续出牌修复

2026-10-07：用户确认剩余组合验收全部完成，含跨回合连续出牌与防重复
结束，本批人工门槛已关闭。当前 [G9-B 收口](G9BClosure.md) 覆盖下文
待验说明；既有自动化和代理窄场景仍保留各自实际范围。

日期：2026-10-06。本批基线 `6b0ab9c`，继续当前 G9 分支，随独立本地提交
`fix(g9-b): retire completed end-turn input fences` 保存。精确提交通过
`git log -1 --format='%h %s'` 核实。本轮仅修改 Native 输入 C++、测试与文档。

## 故障、原因与职责修正

用户报告：结束第一回合后，从第二回合开始，打一张牌后继续选牌会提示
“无法排队：卡牌已入队、队列已满或当前不能出牌”。用户截图与观察记为
用户报告，不补写未提供的运行配置或轨迹。

旧提交回合凭据的阻断判断为“仍是该回合，或当前尚未正常就绪”。到第二
回合追平时判断暂时放行，却保留了第一回合凭据。第二回合第一张牌再次
进入 Resolving 时，旧凭据重新阻断第二回合的后续选牌、确认及结束意图。
这是 `925aff2` 引入的凭据生命周期缺口；`6b0ab9c` 保留了它。此前测试
证明新回合能再次结束，没有覆盖新回合第一张牌播放后继续排队的状态边。

现在提交凭据有明确的单向退役边界：输入所有者在既有合并评估中，确认
同一 Battle 的不同 PlayerTurnSerial 已到达精确、追平且正常就绪的显示
边界后，退役旧凭据一次。凭据存续时持续防重；退役后，后续卡牌演出
不能重新激活它。相同回合、旧历史窗口、强制选择及不完整就绪均不能
释放凭据；拒绝回执仍只能释放自己，绑定替换仍清理所属范围。

这一修正由正式 Ready／Presentation／ViewModel 通知驱动，不增加 Tick
消费、时间冷却或 Gameplay 状态副本。当前结束意图仍保留之前确认牌，
禁止之后追加；强制选择仍清空未执行输入，普通选中牌仍抑制邻牌突出。

## 实际构建与自动化证据

先加入回归测试、保持旧实现，执行规定工程生成与 Editor 构建通过：
`Saved/Logs/G9NextTurnReproProjectFiles.log`（3.81 秒）、
`G9NextTurnReproBuild.log`（10.25 秒）。单独运行
`SlayTheSpireDemo.SelectionPresentation.G9B.Queue.NextTurnContinuousQueue`：
**0 成功、1 失败、0 未运行**，按预期复现第二回合 B／C 被拒绝、队列数为
0、新的结束意图被拒绝。日志：`Saved/Logs/G9NextTurnReproAutomation.log`；
报告：`Saved/AutomationReports/G9NextTurnRepro/index.json`。

修正生命周期后重新按规定生成工程与构建：
`Saved/Logs/G9NextTurnProjectFiles.log`（3.64 秒）、
`G9NextTurnBuild.log`（**54.56 秒**），均通过。一次聚焦自动化 **54 项**，
结果 **53 成功、1 项预期 R8 警告、0 失败、0 未运行**。
报告：`Saved/AutomationReports/G9NextTurn/index.json`；日志：
`Saved/Logs/G9NextTurnAutomation.log`。实际范围：HandInteraction、
SelectionPresentation.G9A／G9B／G8B、Phase6UIA2N.FastInput／R8、
CardSelection.Presentation.Input，没有复用历史测试总数。

新增测试使用正常 HUD／ViewModel 请求，覆盖有历史与无历史模式下的第二、
第三回合：A 开始后忙碌时确认 B 的目标和 C，重复 B 不入队，等待时不
提前提交；三次成本提交后才结束一次，重复点击被拒绝；有历史模式还
断言 CardPlayed 为 A→B→C。原队尾、早期忙碌重试、发布重入、精确拒绝
回执、强制选择和关闭／替换等测试在本次范围内通过。

## 本批生产 Native 实际观察

使用修复后构建，MCP 启动生产地图的浮动窗口 PIE，D3D12，窗口截图为
1433×870。保留的用户 Native 资产实例 G9=true，读回
`Saved/G9NextTurnConfig.json`；没有修改默认开关、动画参数或资产。
启动记录：`Saved/G9NextTurnStartPIE.json`；演出与 Ready 日志：
`Saved/Logs/G9NextTurnPIE.log`。

实际窗口输入先结束第一回合。第二回合打出燃烧契约、完成强制选择，随后
依次打出重击、盛怒、双重打击、打击，未出现无法排队提示。完成后 VM
为 Idle、SelectedCardRuntimeId=-1、Energy=1、bInputLocked=false、
LastFeedback 为空、Revision=15；玩家 HP=65、敌人 HP=114、手牌数=1。
读回：`Saved/G9NextTurnSecondRoundVM.json`，实际截图见本任务工具记录。

再正常结束第二回合。第三回合依次确认并打出剑柄打击、打击，操作仍
正常；完成后 VM 为 Idle、Energy=3、bInputLocked=false、LastFeedback
为空、Revision=22，玩家 HP=60、敌人 HP=99、手牌数=5。读回：
`Saved/G9NextTurnThirdRoundVM.json`。日志显示开场 Revision=4、第二回合
就绪 Revision=9、第三回合就绪 Revision=20，两次回合推进均来自明确点击。

**第二／第三回合普通连续出牌的窄场景通过。** 截图刷新间隔内动画、抽牌
及布局已经推进，实际点到的后续牌随之变化，因此本次不宣称在 A 演出
期间完整确认 B／C 的受控队列录像通过，也不将普通连续点击代替该门槛。
第二／第三回合播放期间排队、队列后结束的完整视觉轨迹继续待人工复验。

PIE 已停止并读回 false：`Saved/G9NextTurnPIEStopped.json`；观察器移除：
`G9NextTurnUnobserve.json`。编辑器已关闭，未保存资产。用户 Native 资产
SHA256 与批次前一致，仍为
`F896F7B59A91D4EC86AD73973451E19012B7523EE7D1D1ECEE2B570894E20F81`，
排除在提交之外。生成工程、构建和 Saved 本地证据不纳入提交，没有 push。

## 中文人工验收清单

生产地图：`/Game/SlayTheSpireDemo/Maps/L_Battle_RuinedCitadel`，Native HUD，
实际开启 G9。C++ 默认仍关闭，G9-B 保持 OPT-IN／NOT SEALED，不进入 C–F。
本批普通连续出牌观察已通过；下列播放期间队列完整时间线仍为
**USER ACTION REQUIRED**，自动化不代替完整视觉时间线。

| 项目 | 操作步骤 | 通过条件 |
|---|---|---|
| A. 第二回合继续出牌 | 结束第一回合，等待第二回合手牌完整出现。打出 A，在它播放时选中 B 并确认目标，再确认 C；不提前点结束回合。 | B、C 可确认并按序自动执行；不会误报无法排队。重复点击同一已确认牌不产生第二次提交。 |
| B. 第三回合继续出牌 | 正常结束第二回合，第三回合重复 A 的操作。 | 连续出牌仍正常，旧结束回合凭据不会在下一次播放时恢复。 |
| C. 保留此前牌再结束一次 | 第二或第三回合 A 播放时确认 B／C，再点击结束回合；等待、弃牌和敌人演出期间重复点击，并尝试追加其他牌。 | B／C 按原顺序正常打出，后续追加被拒绝；只结束一次，新回合不会自动结束。 |

尚未完成的完整视觉轨迹标记 **USER ACTION REQUIRED**。反馈记录项目编号、
通过／失败、实际提交、G9 开关及视口，排队时间线建议保留连续录像。
其他阶段待办继续使用 [最新输入修订清单](NativeInputHoverAndEndTurnRevision.md)。
