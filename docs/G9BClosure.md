# G9-B 验收收口与默认启用

2026-10-10：G9 当前总状态为 COMPLETE／VALIDATED／SEALED，见
[F 封板](SelectionPresentationG9FSeal.md)。本页保留 B 收口／默认批次证据，
后续阶段未实施及总体验收未封板的描述均为当时状态。

日期：2026-10-07。分支 `codex/g9-buffered-input-detached-cards`。
实现版本为 `e62cdbd`，验收记录起点 HEAD 为 `d2db956`。

当前状态：**G9-B COMPLETE / VALIDATED / NATIVE DEFAULT ENABLED**。
验收收口提交为 `a6b4877`；独立启用提交使用标题
`feat(g9-b): enable validated native buffered input by default`，可由 Git 核实。
G9-C–F 尚未实施，G9 整体 NOT SEALED。

## 人工验收已全部通过

用户先确认本批拖放 A–E 人工验收通过，随后在说明剩余三组完整组合时间线
后明确回复“验收都已完成了”。因此修订后的 G9-B 人工门槛全部关闭，
来源记为 **USER_REPORTED_PASS**，无需再次要求用户重复验收。

| 验收范围 | 通过条件 | 当前结果 |
|---|---|---|
| 持牌拖放 A–E | 区外松开出牌、区内继续跟随、单体攻击只瞄准、播放期间拖放排队、强制选择手势隔离、取消及视口变化均正常。 | 通过（用户确认，原记录 `d2db956`） |
| 队列后结束回合 | 按钮之前确认的 B／C 按序打出，未确认 D 取消，按钮之后禁止追加，队列完成后结束一次。 | 通过（用户确认） |
| 防重复结束及跨回合 | 等待、弃牌、敌人行动和新回合抽牌期间的重复点击不预订未来回合；下一回合正常连续出牌。 | 通过（用户确认） |
| 强制选择清空旧输入 | 排队战吼、后续攻击及结束意图；强制选择出现后清空旧输入，结束回合不可用，完成选择后旧命令不恢复。 | 通过（用户确认） |

同时弃牌和遗物队尾的生产视觉已有
[Native PIE 证据](G9BRevisionNativePIE.md)；数值、身份、顺序、一次性请求及
异常清理沿用已提交的各批自动化证据。最近实现 `e62cdbd` 的聚焦报告为
NativeDragReleaseGuard：55 项，54 成功、1 预期 R8 警告、0 失败／未运行。
不同批次和重叠范围不叠加为新的测试总数。

本次人工通过来自用户反馈，没有补写新的运行配置、截图或录像，也没有
冒充代理重新运行完整时间线。此前专用文档中的待验状态为执行历史，
当前 G9-B 门槛以本收口记录为准。G9-C–F 尚未实施，G9 整体未封板。

## 独立默认启用批次

人工门槛已满足，启用批次基线为收口提交 `a6b4877`。按既定修订计划将
Native 的 `bEnableG9BufferedPlayerInput` C++ 默认改为 true。保留运行时关闭及 G8-B 回退，
不修改 FIFO、Gameplay、历史 reducer 或 Blocking 时序。

按规定先生成工程、构建 Development Editor，再执行受默认影响的聚焦
自动化；生产地图 `/Game/SlayTheSpireDemo/Maps/L_Battle_RuinedCitadel` 读回
Native C++ 默认、生产 Blueprint 默认及实际 HUD 配置，验证真实启动启用。
已有人工通过场景不重复执行。外部 Native HUD 资产保持原样且排除提交。

默认启用检查已完成，随独立本地 commit 保存，不 push；G9-B 默认启用完成。
下一开发阶段为锁定设计的 G9-C：统一卡牌 reducer、生命周期 token 和多实例
视觉所有权，保留 Blocking 时序；不将 G9-B 收口写成 G9 整体 COMPLETE／SEALED。

### 默认启用实际构建与自动化

源代码为 `a6b4877` 加本批 Native 默认开关改动；没有修改测试或新增依赖。
规定工程生成通过：`Saved/Logs/G9BDefaultProjectFiles.log`，7.31 秒。
UE 5.8 Development Editor 构建通过：`Saved/Logs/G9BDefaultBuild.log`，
162.50 秒。一次聚焦 **45/45 成功、0 警告、0 失败、0 未运行**。
范围：SelectionPresentation.G9B、SelectionPresentation.G8B、
Phase6UIA2N.FastInput、HandInteraction，覆盖队列、防重、跨回合、关闭回退、
强制选择及手牌输入。报告 `Saved/AutomationReports/G9BDefault/index.json`，
日志 `Saved/Logs/G9BDefaultAutomation.log`。其他已通过证据按影响范围复用。

### 默认启用生产启动读回

最终启用代码上，通过 UE MCP 在生产地图启动实际 Native PIE（
PlayMode_InViewPort、默认 D3D12），没有临时修改运行时开关或保存资产。

| 检查项 | 实际结果 | 证据 |
|---|---|---|
| Native C++ 默认 | `/Script/SlayTheSpireDemo.Default__BattleHUDWidget` 的 `bEnableG9BufferedPlayerInput=true`。 | `Saved/G9BDefaultNativeCDO.json` |
| 生产 Blueprint 默认 | `WBP_BattleHUD_Native` 默认对象的该字段为 true。 | `Saved/G9BDefaultBlueprintCDO.json` |
| 实际生产 HUD | Native HUD 实例的字段为 true，已绑定生产地图 ViewModel。原生 NativeConstruct 以该字段及完整绑定校验启用仲裁器。 | `Saved/G9BDefaultRuntimeHUD.json`，既有 NativeConstruct 实现 |
| 初始输入就绪 | Idle、Selected=-1、Energy=5、Hand=5、revision=4、未锁定、可结束回合、反馈为空；没有 Native HUD 绑定错误。 | `Saved/G9BDefaultReadyVM.json`、`Saved/Logs/G9BDefaultPIE.log` |
| 测试收尾 | PIE 停止查询为 false，随后关闭本批启动的编辑器，未保存资产。 | `Saved/G9BDefaultStopped.json` |

用户保留的 Native HUD 资产原已启用 G9，启动检查使用该实际资产；本次没有
将其改动纳入提交。前后 SHA256 均为
`F896F7B59A91D4EC86AD73973451E19012B7523EE7D1D1ECEE2B570894E20F81`。
该读回证明本地生产启动配置；没有宣称代理重复执行全部人工时间线，也没有
新增打包／Shipping 验收。生成文件、Saved 证据及无关资产均排除提交。
