# Native 点击与长按统一选牌

2026-10-07：点击／持牌拖放及 G9-B 剩余组合验收已全部获用户确认通过，
当前见 [G9-B 收口](G9BClosure.md)，下文待验状态为当时历史。

最新扩展（2026-10-07，基线 `7c73884`）：技能／能力持牌拖出手牌区松开
自动确认，区内松开继续跟随；用户明确单体攻击只抬升／瞄准，不拖动。
按钮松开仍不重复选牌，最新释放协议、证据和中文清单见
[持牌拖动与区域释放](NativeCardDragRelease.md)。下文“松开绝不出牌”为上一批
历史约定及证据，不再作为本批区外拖动的标准。

日期：2026-10-06。基线 `9c78655`，继续当前分支。本批随独立本地提交
`feat(native-ui): select cards on press for click and hold` 保存，精确提交用
`git log -1 --format='%h %s'` 核实。只修改 Native C++、测试和文档。

## 行为与输入边界

用户要求点击和长按选牌逻辑相同。卡牌的选牌请求现在只订阅原生 UButton
的 OnPressed：左键按下即走既有 OnBattleCardRequested → HUD 选牌入口。
短按和持续按住没有不同的模式、时间阈值或重复定时器。OnReleased／
OnClicked 不订阅选牌逻辑，松开不会再次选中、取消、确认、排队或出牌。
一次手势的按下与松开不能跨越两个输入状态边界。

保留现有 HandleCardClicked 反射函数名称及请求负载，不做公共接口／资产
重命名。NativeConstruct 唯一绑定，NativeDestruct 移除同一个按下绑定。
原生按钮本身负责按下／释放状态，HUD 仍唯一处理普通选牌、强制选择和
G9 草稿／确认队列；卡牌 Widget 不拥有 Gameplay 或第二套选牌状态。

攻击牌按下后继续瞄准，技能／能力牌继续跟随指针；松开仍保留选中状态，
后续新的目标点击或左键确认才按既有规则出牌，右键取消仍归还原位。
已选中牌抑制邻牌突出、结束回合队尾与防重、强制选择隔离、G9 关闭回退
均沿用既有规则，不改变 Gameplay、历史 reducer 或 Blocking 时序。

## 实际构建与自动化

按规定生成工程文件与 UE 5.8 Development Editor 构建通过：
`Saved/Logs/NativePressSelectionProjectFiles.log`（5.38 秒）、
`NativePressSelectionBuild.log`（**17.97 秒**）。一次聚焦自动化 **50 项**：
**49 成功、1 项预期 R8 警告、0 失败、0 未运行**。
报告：`Saved/AutomationReports/NativePressSelection/index.json`；日志：
`Saved/Logs/NativePressSelectionAutomation.log`。

实际范围为 Phase6UIA2N.R4.CardWidget、HandInteraction、
SelectionPresentation.G9B／G8B、Phase6UIA2N.FastInput／R8、
CardSelection.Presentation.Input。新增
`HandInteraction.CardPress.ClickAndHoldSelection` 通过，验证攻击／技能／
能力的按下请求、精确 RuntimeId、提示性不可出牌状态不越权阻止选牌、
按下后 DTO 改变时释放不请求新身份、短按释放无重复、无效身份拒绝、
重复构建唯一绑定及销毁／重建隔离。自动化验证 UButton 事件出口，不把
合成事件作为真实持续长按的视觉证据。

## 本批实际 Native PIE

修复后构建，MCP 实际启动生产地图的浮动窗口 PIE，D3D12，窗口截图
1433×870。用户保留资产的实例 G9=true，读回
`Saved/NativePressSelectionConfig.json`；C++ 默认 false，没有修改资产或
动画参数。日志：`Saved/Logs/NativePressSelectionPIE.log`。

实际窗口输入按住打击从原手牌位置移动到敌人上方后松开：卡牌已选中，
瞄准箭头仍在，能量 5、敌人 HP=150，没有自动攻击。右键取消后，对同一
牌短按，得到相同选中／箭头状态，松开没有反向取消，邻牌保持原位。

按住燃烧移动到空白区域并松开：卡牌继续跟随指针，未自动打出。松开后
读回 ChoosingTarget、SelectedCardRuntimeId=5、Energy=5、bInputLocked=false、
Revision=4、反馈为空（`Saved/NativePressSelectionReleasedVM.json`）。右键
取消归位，随后短按燃烧仍只建立一次选中；新的左键确认后正常出牌。
完成读回 Idle、SelectedCardRuntimeId=-1、Energy=4、bInputLocked=false、
Revision=5、反馈为空，手牌数 4、力量 Amount=2，符合一次正常出牌：
`Saved/NativePressSelectionConfirmedVM.json`。截图见本任务实际工具记录。

**短按、按住移动／松开、右键取消和新点击确认的窄场景通过**。按住移动
使用工具的单次拖动手势，未测量持续一秒以上的静止长按；完整长按时长、
技能牌及强制选择／忙碌时间线仍待人工复验，不补写通过。PIE 已停止并
读回 false（`Saved/NativePressSelectionPIEStopped.json`），编辑器已关闭。
未创建观察器，未保存资产。用户 Native HUD 资产 SHA256 保持为
`F896F7B59A91D4EC86AD73973451E19012B7523EE7D1D1ECEE2B570894E20F81`，
不纳入提交；Saved／生成文件不纳入提交，没有 push。

## 中文人工验收清单

生产地图：`/Game/SlayTheSpireDemo/Maps/L_Battle_RuinedCitadel`，Native HUD。
普通选牌支持 G9 开／关；播放期间排队须实际开启 G9。C++ 默认仍 false，
不进入 G9-C–F，不改变原阶段待办或封板状态。

| 项目 | 操作步骤 | 通过条件 |
|---|---|---|
| A. 攻击牌短按与长按 | 短按攻击牌，右键取消；再按住同一攻击牌至少一秒，移动鼠标后松开，最后点敌人。 | 两种按法都选中并显示瞄准箭头，邻牌不突出；松开后仍选中，不扣费；新的敌人点击只出牌一次。 |
| B. 技能／能力短按与长按 | 短按战吼或燃烧并右键取消；再按住移动到空白区域后松开，等待一秒，随后右键取消或用新的左键确认。 | 按住时牌跟随指针；松开不自动打出，也不重新选牌；取消归还原位，新的左键确认按原规则执行一次。 |
| C. 忙碌与强制选择 | G9 开启时在 A 播放中按住 B、选目标确认；另在强制选择中按住候选牌并松开，再使用确认按钮。 | B 不重复入队；强制候选只改变一次选择，不因松开反向取消或自动提交；结束回合继续不可点击。 |

上文仅记录实际完成的窄场景。未完成的完整长按／忙碌／强制选择轨迹为
**USER ACTION REQUIRED**；原 G9 队列及第二／第三回合待办继续保留。
