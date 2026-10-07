# G9-C：共享历史语义与多实例视觉所有权

日期：2026-10-08，分支 `codex/g9-buffered-input-detached-cards`，起点 `2c29025`。
G9-B 已通过并默认启用，不重新要求 B 人工验收。依据锁定的
[G9 设计](SelectionPresentationG9Design.md) 实施 C，不启用 D1／D2 的非阻塞时序。
当前为 **IMPLEMENTED / AUTOMATED GATES PASS / PARTIAL PIE**，C 的全部视觉
门槛尚未关闭，不进入 D1／D2，不宣称 G9 整体封板。

## 实施批次与职责

1. 共享纯 reducer 与 Controller 生命周期关联：正式 Blocking 提交、单记录
   预检、Group 预检及测试候选共用同一语义。失败不部分提交，预检仅使用
   候选快照／生命周期副本，不临时替换正式 Controller 状态。
2. Native 卡牌视觉所有权：Controller 的具体出牌发生标识传给独立视觉 host，
   每个 job 有自己的 generation、Widget、几何、时间、透明度与阶段。
   保留既有 Blocking 完成回执、卡面、运动参数、选择及抽牌附着协议。

每批完成相关测试、证据和文档后独立本地提交，再开始下一批；不 push。
只修改 Native C++／测试／文档，保留外部 Native HUD 资产原状并排除提交。

## 首批历史与生命周期约定

`PresentationCardReducer` 校验卡牌身份／冻结卡面、Hand 索引、能量与成本、
战斗对象身份、区域路线及去向索引。CardPlayed 建立一次发生标识；正式
PlayArea 去向须唯一匹配尚未消费的关联，并且仅消费一次。RichDescription
是已冻结的目标预览表现，不是实例身份。

强制选择可能将一张卡牌的 CardPlayed 与去向分到两个封存 Envelope；关联
必须跨正常 Envelope 完成保留，去向记录位于更晚 Resolution／sequence 时
仍消费原发生。Skip、恢复、替换及销毁清理关联，但不复用本地 generation。
视觉丢失不能代替正式去向消费。无 Widget 的历史 reducer 仍保存准确发生
身份，但没有有效 Session 的标识不得授权视觉 job。

历史错误走既有 Presentation 恢复，不请求 Gameplay ResolutionFault。
视觉资源准备失败继续 decline／Blocking 回退。后续并行只涉及视觉状态，
Gameplay 请求及历史提交始终串行。

## 验证与中文验收

首批聚焦自动化验证原子失败、纯预检、一次提交／消费、缺失／重复／错误
关联、跨强制选择 Resume、同 RuntimeId 再出现、无 Widget 和冻结预览卡面。
回归受影响的 R8、G2／G6、G9-B、Selection 及冻结 CardPlayed 卡面。
每个 C++ 批次按规定先生成工程、构建 UE 5.8 Development Editor，再执行
一次聚焦自动化；失败仅重跑受影响门槛。实际结果随执行补充，不预填通过。

视觉门槛在第二批生产 Native 地图
`/Game/SlayTheSpireDemo/Maps/L_Battle_RuinedCitadel` 执行：

| 项目 | 操作步骤 | 通过条件 |
|---|---|---|
| 普通出牌 Blocking 等价 | 正常打出攻击／技能／能力牌，观察 Hand→PlayArea→去向。 | 从实际持牌位置移动，卡面不闪回，阶段及输入解锁时点保持原规则，无重复或幽灵。 |
| 抽牌及选择交接 | 使用抽牌、消耗选牌或战吼，完成强制选择。 | 原抽牌附着与 Selection 接管正常，已打出的卡牌跨选择保留准确视觉，去向只演出一次。 |
| 取消及旧回调 | 播放时 Skip／恢复，再正常出另一张牌。 | 旧 visual job 退役，不影响新正式 Widget，没有残留视觉或永久锁输入。 |
| 视口与手牌所有权 | 播放期间改变窗口尺寸，继续悬停／出牌。 | 私有 host 不裁切或接管输入，正式手牌身份和排列保持正确，移动状态独立。 |

首批不改变视觉所有权／时序，不以额外 PIE 重复证明纯历史门槛；完整 C
视觉门槛在第二批验证。无法实际完成的项目据实标记 USER ACTION REQUIRED。

## 首批实际证据

规定生成通过：`Saved/Logs/G9CCanonicalProjectFiles.log`，5.08 秒；UE 5.8
Development Editor 构建通过：`G9CCanonicalBuild.log`，135.40 秒。
初轮聚焦日志 `G9CCanonicalAutomation.log` 在 52 项完成后中止：51 成功、
1 旧夹具失败；新重复身份用例向同一数组添加内部元素，触发 UE 自引用
断言，未导出完整报告。改为先复制再添加，生产代码无需为测试数据打补丁。

旧 Hardening.IdentityAndDirectMode 用例用 nullptr Widget 初始化，却要求
Controller 在无有效 Session 时解锁，与已有 G8 条件不一致。补一个立即
decline 的测试 Widget 以正确验证 recording latch；不修改生产解锁规则。
修复规定生成通过（`G9CCanonicalRepairProjectFiles.log`，3.79 秒），构建
通过（`G9CCanonicalRepairBuild.log`，6.24 秒）。只重跑失败／未完成的
3 个新用例、旧身份用例、冻结 CardPlayed 卡面，并补受影响的 G6：共 9 项，
8 成功、1 预期夹具警告、0 失败／未运行。

报告 `Saved/AutomationReports/G9CCanonicalRepair/index.json`；日志
`Saved/Logs/G9CCanonicalRepairAutomation.log`。首轮未受影响通过项保留，
**60 个不同测试**均有有效通过证据，不表述为最终版本一次 60/60 运行。
精确清单 `Saved/G9CCanonicalPassingCases.txt`。范围含 G9C、R8、G2／G6、
CardSelection.Presentation、G9B、UIA3.CardPlayedRichHandoff、
Wave1CC1.DrawPileTop.Presentation 与 UIA2A.Hardening。
首批无新增视觉 gate，不运行 PIE；独立提交后继续第二批视觉所有权迁移。

## 第二批：Native 独立视觉所有权

实施基线 `c69b0ac` 加本批未提交变更；提交标题为
`refactor(g9-c): own played-card visuals in scoped native jobs`。
HUD 私有 `DetachedCardVFXHost` 是覆盖根 Canvas 的不可交互宿主；正式 Hand
及 `OV_PlayArea` 不再保存主要已出牌视觉。每个 job 保存独立 Widget、原点、
目标、时间、透明度、阶段、生命周期及视觉 generation。Widget／宿主均受
UPROPERTY 保护；不含 UObject 的原点值单独保存，不要求反射类型。

准备阶段最多 32 项，完整准备及上下文复核后才隐藏准确的正式来源并接受
Blocking 回执。容量／资源准备失败走既有 Native R8 Blocking 回退；有效
Controller 的历史或生命周期证明失败直接 decline，不借回退绕过证明。
原 R8 独立 renderer 测试保持有效。旧单实例字段已明确限定为
`NativeBlockingFallbackPlayedCardWidget`；抽牌及 Hand 去向附着仍遵守已有
协议，并非新的并发出牌状态。

入场与去向共享同一个 visual token；完成同时匹配精确 Blocking token，
旧入场回调不能完成去向。视觉丢失只清理 Widget，不消费正式历史关联；
仍由 Controller 完成提交。绑定替换／解绑、Session 失效、Skip／恢复和
销毁即时清理，不依赖下一次 Tick。新正式 Hand 所有者优先，即使禁用输入，
也退役同 RuntimeId 的旧 job。Selection 通过受限宿主接口隐藏／恢复演出，
保留原交互规则，不直接操作“唯一演出牌”。NativeTick 只推进各 job 的视觉。

## 第二批实际构建与自动化

初次生成通过，构建发现原点值不是 USTRUCT，后续编译发现局部 Slot／
Visibility 名称遮蔽 UWidget 成员；修正声明和局部名称，不改变引擎／依赖。
最初成功生成／构建：`G9CVisualCompileRepairProjectFiles.log`（4.41 秒）、
`G9CVisualCompileRepairBuild.log`（21.84 秒）。首轮聚焦报告
`Saved/AutomationReports/G9CVisual/index.json`：**85 成功、1 预期警告、
0 失败／未运行，共 86 项**。范围为 G9C、Native R8、G6、
CardSelection.Presentation、G9B、冻结 CardPlayed 卡面、HandInteraction、
G0／G4／G5／G7、G8B 及 Native FastInput；精确案例以报告为准。

生产诊断仅追加可选 Verbose 日志，规定生成／构建通过
（`G9CVisualDiagnosticBuild.log`，7.20 秒）。之后收敛 Selection 的宿主隐藏
接口：最终生成／构建 `G9CVisualFinalProjectFiles.log`／
`G9CVisualFinalBuild.log`（7.09 秒）通过；只重跑受影响的 G9C.Visual、R8、
G6、CardSelection.Presentation、G4，报告 `G9CVisualFinal/index.json`：
**26 成功、1 预期警告、0 失败／未运行，共 27 项**。

最后检查修复绑定清理入口：规定生成
`Saved/Logs/G9CVisualBindingProjectFiles.log`（4.31 秒）及构建
`G9CVisualBindingBuild.log`（7.01 秒）通过。最终受影响报告
`Saved/AutomationReports/G9CVisualBinding/index.json`：**58 成功、1 预期
警告、0 失败／未运行，共 59 项**，为上一范围加 G9B、G8B 和 FastInput。
四个新增视觉用例证明实际 Controller 路径、GC、32 项准备／容量回退、
纯准备、旧 token、视觉丢失后的正式提交、禁用输入的新正式所有者、
解绑不调用 Tick 及 Session 替换隔离。

各轮唯一警告均是 R8.InvalidIdentityZeroSideEffects 的预期拒绝日志。
未受影响的首轮通过证据复用，本批 **86 个不同案例有有效证据**；不能把
86、27、59 相加，也不能表述为最终代码一次 86/86。首批历史证据另列。

## 第二批生产 Native PIE 与剩余人工门槛

最终实际运行基线为 `c69b0ac` 加本批代码，UE 5.8 Development Editor，
生产 `L_Battle_RuinedCitadel`，浮动窗口，D3D12／SM6；HUD 实例
`bEnableG9BufferedPlayerInput=true`。未保存资产、未改变默认开关。
UE MCP 负责会话及公开状态读回，computer-use 技能负责真实鼠标操作。
日志 `Saved/Logs/G9CVisualFinalPIE.log`；截图在对话中实际观察，不将自动化
或静态截图冒充完整连续帧验收。

| 项目 | 实际操作与观察 | 结果 |
|---|---|---|
| 普通攻击／抽牌 | 打出剑柄打击，观察从实际位置入场、抽两张并进入弃牌；剩余牌正常排列，最终 Idle／解锁。 | 聚焦通过 |
| 技能／能力及实际来源 | 能力牌从鼠标释放位置入场并移除；战吼同样从当前持牌位置入场。 | 聚焦通过 |
| 跨选择及去向 | 战吼选择期间按原规则隐藏，但同一 Widget 仍附着 `DetachedCardVFXHost.CanvasPanelSlot_2`；缩小窗口后确认回顶，Runtime=11／Life=3／Visual=3 的去向接入 Resolution=5，随后 Phase=3 退役。 | 聚焦通过 |
| 播放中 Skip 后再出牌 | 临时 Saved Python 探针观察 Resolving 后 0.2 秒调用已有 HUD.SkipPresentation；日志确认 Runtime=2／Visual=4 在 Entering 阶段退役，随后打击使用 Visual=5 正常完成并清理。 | 聚焦通过 |
| 最终状态 | 新牌完成后 Energy=1、DiscardCount=3、Idle、未锁定、反馈为空，无残留演出牌。 | 读回及画面通过 |
| 不同视口与保留 job | 1433×870、1283×773、1163×707 三种窗口下排列／选择／后续出牌正常，选择暂停期间保留同一宿主槽位。 | 聚焦通过 |
| 卡牌仍移动时缩放 | 缩放动作后的画面正确，但未取得动作落在尚未完成运动帧内的精确证据。 | **USER ACTION REQUIRED** |

临时探针 `Saved/G9CVisualPIESkip.py` 仅调用现有 HUD 的公开 Skip 接口，
一次触发后自行注销；未改 Gameplay、正式历史、资产或插件。
公开读回保存为 `G9CVisualFinalBinding.json`、`G9CVisualFinalAfterDraw.json`、
`G9CVisualFinalRetainedWarcry*.json`、`G9CVisualFinalAfterWarcry.json`、
`G9CVisualFinalAfterSkip.json`、`G9CVisualFinalAfterFreshPlay.json`。
日志同时确认主路径 Hosted arrival／destination；没有将 Blocking 回退的
正常画面当作新宿主证据。停止 PIE 后 `IsPIERunning=false`，关闭本次编辑器。

剩余人工检查只属于本次 C，不重开已通过的 B：

1. 在生产地图打出剑柄打击或其他带抽牌的牌，卡牌仍在移动时调整 PIE 窗口
   尺寸，随后悬停并继续出牌。
2. 通过条件：移动卡牌无裁切、突跳、重复或幽灵；剩余手牌正常排列并可
   继续选择，演出完成后正常解锁。反馈是否通过，并注明实际测试的提交。

本项未通过前，C 保持 PARTIAL PIE，不进入 D1／D2，不宣称 COMPLETE 或
G9 SEALED。运行时输入默认仍为已验收的 B=true。
