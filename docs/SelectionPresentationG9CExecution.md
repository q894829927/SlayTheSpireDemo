# G9-C：共享历史语义与多实例视觉所有权

日期：2026-10-08，分支 `codex/g9-buffered-input-detached-cards`，起点 `2c29025`。
G9-B 已通过并默认启用，不重新要求 B 人工验收。依据锁定的
[G9 设计](SelectionPresentationG9Design.md) 实施 C，不启用 D1／D2 的非阻塞时序。
当前为 **IN PROGRESS**，尚未宣称 C 验收完成或 G9 整体封板。

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
