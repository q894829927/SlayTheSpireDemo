# Native 持牌拖动与区域释放

日期：2026-10-07，基线 `7c73884`，继续当前 G9 分支。本批随独立本地提交
`feat(native-ui): play dragged pointer cards on release outside hand` 保存。
精确提交用 `git log -1 --format='%h %s'` 核实。仅 Native C++、测试与文档。

## 用户要求与适用范围

点击和按住仍共用按下选牌。对已有的技能／能力指针持牌，按住移动后：

1. 在手牌区外松开，使用现有确认入口尝试出牌；G9 忙碌时确认完整命令入队，
   不提前扣费／移动 Gameplay 卡牌，也不 Skip。
2. 在手牌区内松开，继续跟随指针；之后新的左键确认或右键取消沿用既有逻辑。
3. 单体攻击牌只抬升并瞄准，不进入拖牌，不因松开自动攻击。用户已明确全体／
   随机攻击尚未实现；未来无单体目标的攻击可复用已有 TargetType=None 表现，
   本批不新增这些 Gameplay 效果或资产。

上述要求取代上一批“所有松开都不确认”的完整手势约定。正式指针卡牌在
原生预览按下中将事件／回执转发给 HUD；HUD 调用一次正常选牌并立即捕获，
处理该事件后不再触发叶按钮 OnPressed。攻击／强制选牌继续原 OnPressed。
按钮 OnReleased／OnClicked 仍不请求选牌或出牌；只有 HUD 的有效拖动释放
可以确认一次。
强制选择不进入普通拖动；释放在结束回合、取消或确认控件上不自动出牌，
这些控件须使用新的一次正常点击。

## 职责、几何与生命周期

正式 Hand 提交绑定弱原生按下回执和普通请求；移除、替换、销毁统一解绑。
两路共用既有静止命中身份解析和正常 SelectCard，卡牌只转发、不保存手势。
HUD 成功选中后立即捕获，尚未完成下一次 Slate 更新也不会漏掉空白区移动。
没有第二次选牌请求、卡牌 Widget 拖动状态或新计时器。
移动达到 **8 个 HUD 逻辑像素** 后视为拖动，用于排除短点击抖动；与持续
时长和帧率无关。叶按钮使用 MouseDown 点击方式、不捕获鼠标；HUD 的捕获
属于原始按下，移动不重新捕获，移动与释放由同一 HUD 处理。

手势保存弱 ViewModel／Controller／Panel／Card、Battle、RuntimeId／CardId、
Hand 表面 generation、G9 输入 generation、显示所有权／Session、鼠标用户与
指针编号。历史正常推进不使 G9 草稿过期；替换、取消、关闭、清理或强制选择
使精确手势失效。释放前先清空手势，防请求发布重入；旧释放不能恢复旧命令。
取消后即使手势已退役，HUD 仍释放自己的鼠标捕获并消费旧松开。

“手牌区”使用当前 HB_Hand／FanHand 分配的矩形；持牌中心跟随指针，因此
以释放点判定区域，边界算区内。读取当前几何，兼容 DPI 和视口布局；缺少
有效 Hand 几何时保持持牌，不猜测出牌。移动只更新现有指针渲染变换，不
写正式子列表或基础布局。原位槽位及冻结顺序继续由已有结构提交协议维护。

Self／None 沿用 ConfirmPointerCard → 正常 ViewModel／G9 确认请求；已有需要
Enemy 的技能仍必须落在精确合法敌人表面，不猜测或更换目标。新技能／能力
释放前安装当前指针位置，沿用既有出牌起点／FIFO 化妆凭据。Gameplay、历史
reducer、Blocking 时序、队尾结束、防重、G9 默认关闭均不改变。

## 实际构建与自动化

规定工程生成通过：`Saved/Logs/NativeDragReleaseProjectFiles.log`，5.05 秒。
UE 5.8 Development Editor 构建通过：`NativeDragReleaseBuild.log`，**63.50 秒**。
一次聚焦 **54 项**：**53 成功、1 项预期 R8 警告、0 失败、0 未运行**。
报告：`Saved/AutomationReports/NativeDragRelease/index.json`；日志：
`Saved/Logs/NativeDragReleaseAutomation.log`。实际范围为 R4.CardWidget、
HandInteraction、G9B／G8B、FastInput、R8、CardSelection.Presentation.Input。

新增 CardDrag 的 4 项测试通过，覆盖 G9 开／关、Self／None、技能／能力及
预留无单体目标攻击、区内保留后新确认、区外一次请求／成本／效果、抖动、
不同指针、槽位保留、取消后同身份重选、替换、关闭、丢失按住、零几何、
显式控件隔离、发布重入、真实 Blocking 历史期间入队后自动执行，以及强制
选择出现后旧释放失效。单体攻击选中后不能进入拖动，仍等待正常目标点击。
自动化不代替真实鼠标捕获和完整视觉时间线。

首次实际 PIE 发现区外松开仍保持持牌，不能计作通过。原因是 UButton
默认 DownAndUp 会捕获鼠标，UE 5.8 Slate 对捕获后的移动／松开只路由到
最末捕获者，不向 HUD 冒泡；直接调用 HUD 手势方法的测试没有覆盖这一点。
修正为卡牌按钮 MouseDown、不拥有捕获，保持 OnPressed 唯一选牌入口，
HUD 在真实移动中拥有捕获。R4 按下／生命周期测试增加该配置断言。

修正后规定工程生成通过（`NativeDragReleaseFinalProjectFiles.log`，7.55 秒），
Editor 构建通过（`NativeDragReleaseFinalBuild.log`，**15.19 秒**）。按钮捕获
配置影响共享卡牌输入，重新执行同一聚焦范围 **54 项**，结果仍为
**53 成功、1 预期 R8 警告、0 失败／未运行**；报告
`Saved/AutomationReports/NativeDragReleaseFinal/index.json`，日志
`Saved/Logs/NativeDragReleaseFinalAutomation.log`。总数不叠加两次运行。
`NativeDragReleasePIE.log` 与 `NativeDragReleaseFinalPIE.log` 均为失败历史。
第二次失败定位到 Slate 的 SetPointerCaptor 每次都会先 ReleaseCapture，
重复 CaptureMouse 会调用 HUD 自己的 CaptureLost 清掉手势。修正后规定
生成／构建通过（`NativeDragReleaseCaptureProjectFiles.log`，5.71 秒；
`NativeDragReleaseCaptureBuild.log`，7.50 秒）；仅受影响的 HandInteraction
和 R4.CardWidget **13/13 通过**，报告 `NativeDragReleaseCapture/index.json`。
这一运行在真实 PIE 证明燃烧区外出牌与区内保留、打击只瞄准，但战吼快速
拖动仍失败，不能作为完整通过或最终证据。

针对战吼的临时日志 `NativeDragReleaseRouteDiagPIE.log` 捕获到正常 Begin
成功，但释放时 dragged=0、capture=0；不是 Gameplay 拒绝，而是移动没有
到达 HUD。根因是等第一次移动才捕获，空白处的透明命中表面须下一次
Slate 更新才生效。最终改为上述正式卡牌的原生按下回执协议、HUD 按下即
捕获，不依赖空白命中更新。新增 PressCaptureRouting 测试验证首次按下
回执携带捕获、精确身份、未认领路径回退及退役隔离。临时诊断日志已从
源码删除，没有新增 Gameplay、资产或依赖。

原生路由测试最初在测试模块链接失败（`NativeDragReleasePressBuild.log`）；
将 SlateCore 对象构造放入现有 Runtime 测试桥接文件，测试模块只接收普通
结果和计数，未新增依赖。修正后规定生成／构建通过（
`NativeDragReleasePressFinalProjectFiles.log`，4.33 秒；
`NativeDragReleasePressFinalBuild.log`，49.62 秒）。按下请求的通知重入还复核
原 ViewModel／Controller／Hand 表面，旧按下不能接管替换后的表面。

最终规定工程生成通过：`NativeDragReleaseGuardProjectFiles.log`，4.32 秒；
UE 5.8 Development Editor 构建通过：`NativeDragReleaseGuardBuild.log`，
**6.42 秒**。最终共享原生路由和解绑影响卡牌输入／Selection，重新执行上面
同一聚焦范围及新增路由测试，共 **55 项：54 成功、1 预期 R8 警告、0 失败／
未运行**。报告 `Saved/AutomationReports/NativeDragReleaseGuard/index.json`，
日志 `Saved/Logs/NativeDragReleaseGuardAutomation.log`。不叠加之前 54／13 项。
本批新增 CardDrag 共 5 项；仅状态／协议自动化不代替真实鼠标输入验收。

## 最终 Native PIE 实际证据

在上述基线和本批最终未提交 C++ 上，规定构建后启动生产地图，浮动窗口
1433×870（游戏区从标题栏下方开始）。运行实例 Native HUD 的 G9=true，
读取凭据 `Saved/NativeDragReleaseGuardConfig.json`；C++ 默认仍 false，未保存
资产或改写配置。日志 `Saved/Logs/NativeDragReleaseGuardPIE.log`。

- 快速拖出战吼后直接松开，未补新点击，即开始正常出牌／抽牌并进入强制
  选择；候选点击达到 1/1，正常确认后继续历史。拖动没有代替选择确认，
  强制选择时结束回合仍不可点击。
- 燃烧区内松开保持持牌和 ChoosingTarget，RuntimeId=5，能量=5，手牌=4，
  revision=6，未锁定、无反馈；右键正确回原位。随后从原位重新拖出并松开，
  无新确认点击自动出牌，能量 5→4、力量=2、手牌 4→3、revision 6→7，
  恢复 Idle／解锁，持牌／取消表面无残留。
- 单体打击按住移到敌人后松开，仍在手牌区抬升并显示箭头；能量仍 4、
  敌人 HP 仍 150，其他牌不突出。随后正常点击敌人一次才出牌；最终能量=3、
  敌人 HP=142、手牌=2、revision=8、Idle、Selected=-1、解锁、反馈为空。

状态读取凭据分别为 `NativeDragReleaseGuardSkillVM.json`、`InsideVM.json`、
`OutsideVM.json`、`FinalVM.json`，完整文件名前缀均为 NativeDragReleaseGuard，
位于 Saved。截图由实际目标窗口的 Computer Use 状态读取显示；未记录连续
运动录像，不用单帧宣称完成所有瞬时轨迹验收。PIE 已停止，停止查询为 false，
随后关闭本批启动的编辑器；未保存资产。外部 Native HUD 资产 SHA256 保持
`F896F7B59A91D4EC86AD73973451E19012B7523EE7D1D1ECEE2B570894E20F81`，排除提交。

## 中文人工验收清单

生产地图：`/Game/SlayTheSpireDemo/Maps/L_Battle_RuinedCitadel`，Native HUD。
普通持牌支持 G9 开／关；播放期间排队须实际开启 G9。当前阶段不进入 C–F，
不自动启用默认或封板。上面的窄场景已通过；下表仍列完整复验条件。

| 项目 | 操作步骤 | 通过条件 |
|---|---|---|
| A. 区外释放 | 按住燃烧或技能牌，移动到战斗空白处再松开。 | 松开自动确认一次，卡牌从当前位置进入出牌区，正常扣费／结算，没有第二次请求或残留持牌。 |
| B. 区内释放 | 按住指针持牌，在底部手牌区内移动并松开；之后只移动鼠标，再用新左键确认或右键取消。 | 松开不扣费、不出牌；移动仍跟随。新左键确认一次，右键回原位，其他牌不突出。 |
| C. 单体攻击与短点击 | 短按燃烧并松开；另按住打击移动到敌人后松开，再正常点击敌人。 | 短按只选牌，需新点击确认；打击始终在手牌区抬升并显示箭头，不跟随鼠标拖出，不因松开攻击。 |
| D. 忙碌与强制选择 | A 播放时拖出 B 松开，再拖出 C 松开；或拖动时进入强制选择，并在其出现后松开旧手势。 | B／C 各入队一次，按序正常执行；强制选择丢弃旧手势，松开不能普通出牌、自动提交候选或结束回合。 |
| E. 取消与视口 | 按住拖动时右键取消，随后松开左键；再次持牌时改变窗口尺寸，分别在新手牌区内／外松开。 | 旧松开不出牌且鼠标捕获不残留；按当前区域判断，卡牌不闪回或重排，后续点击正常。 |

未完成视觉项目为 **USER ACTION REQUIRED**；反馈记录项目编号、通过／失败、
实际提交、G9 开关和视口。原完整 G9 队列／队尾／强制选择待办继续保留。

尚需人工补验：B 的区内释放后仅移动鼠标，再新左键出牌；C 的短点击与
静止按住；D 的播放期间连续 B／C 拖放排队和另一张牌触发强制选择时清掉
正在按住的手势；E 的仍按住时右键取消、旧左键松开及改变窗口尺寸后的
区域判定。A 的连续运动／无闪回也请在实际操作中确认。记录不宣称 G9-B
完整视觉门槛完成，不进入 G9-C–F 或 G9 封板。
