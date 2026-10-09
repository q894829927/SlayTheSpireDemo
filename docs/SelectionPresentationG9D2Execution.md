# G9-D2：出牌入场与提前去向衔接

2026-10-09，分支 `codex/g9-buffered-input-detached-cards`，起点 `04d835b`。
D1 COMPLETE／VALIDATED／默认开启。当前 D2 为 COMPLETE／VALIDATED／NATIVE DEFAULT ENABLED；
本轮仅 D2，不进入 E／F，不宣称 G9 封板。保留外部 Native HUD 资产原样，
只改 Native C++、测试及文档；每个完整批次本地提交，不 push。

## 实施契约

按锁定 G9 设计 13.3、13.4、15.12、16.8、17 实施。Controller 只预检当前
封存 Envelope：复制正式候选快照和生命周期，按原顺序使用共享 reducer，
证明唯一、受支持、准确关联的未来 PlayArea 去向。预检不发布、不安装正式
状态，不跨 Envelope 猜测。缺失、歧义、不支持或无法完整证明时保留 Blocking
入场，之后仍可使用 D1。历史真正到达当前游标无效时走现有恢复。

Base 统一准备／激活／清理边界；准备收据带 Session、完整出牌生命周期、
独立视觉 generation、未来去向身份及准备 generation。Native 准备隐藏 job、
准确 Hand 来源、PlayArea 和去向冻结几何／参数，容量继续为 32。准备失败
不得写正式状态或隐藏正式来源；正式安装候选及关联后发布，复核游标／
绑定／Session 后才激活私有入场。提交后失败只清理视觉，不重播 reducer。

Controller 另保留已提交 D2 出牌的准确去向收据，不能以视觉 job 存续证明
关联。未来准确去向按正常游标提交一次，消费正式关联；即使视觉已丢失或
开关关闭也能提交。视觉尚在入场时只记录已正式提交的 pending 去向，入场
结束后连续接入尾部，不跳到去向端点。视觉更新不发布、不完成 Controller，
也不授予输入权限或消费 FIFO。每个 job 独立阶段、时间、几何及强引用。

D2 是完整旅程的准入策略：新入场解耦需 D2 开启且 D1 去向策略可用，才能
完整证明续接；两个开关分别保留，D2 关闭可继续使用 D1。关闭任何相关
策略时清理其私有视觉及待执行输入，后续新入场回退 Blocking；已提交的
去向收据／正式关联保留到消费或历史恢复，不改变 Session／回合 serial。
Skip／恢复／替换／销毁清理失效的准确关联。新的同 RuntimeId 正式所有者
立即退役旧视觉；旧收据不能作用于新对象。

目录旧规则中“可见播放顺序唯一例外是 Group”与此锁定设计冲突；本批按
专用设计优先级明确补充：解耦视觉在自身正式记录提交后独立运行，正式
reducer 顺序不变。未来去向预检不是提前消费未来历史，不新增 Group 例外。

## 批次与自动化门槛

1. 保存本契约及继续点，本批仅文档。
2. 实施 Controller／Base／Native 完整事务、提前去向及清理，规定生成工程、
   UE 5.8 Development Editor 构建、一次聚焦 Automation、必要 Native PIE，
   更新实际证据并本地提交。失败只重跑受影响门槛。
3. 全部 D2 门槛通过后独立默认启用，规定验证并核实生产 Native 默认再提交。

- 当前／未来卡牌共享 reducer，缺失／重复／错误去向及干扰记录均安全回退。
- 准备失败零正式副作用；准备、发布、激活重入的关闭／Skip／替换安全。
- 入场与去向各提交一次；去向早于入场结束时，只在入场完成后开始尾部。
- 视觉丢失、关闭及 GC 不删除必要关联；没有 timer、Controller 或输入债务。
- 多 job、同 RuntimeId 回手及再出牌、容量、旧收据和视口失效保护准确。
- 回归受影响 D1、G9C／R8、G9B／FastInput、DamageNumber、HandInteraction、
  Selection 协议；记录实际范围／结果，不预填或累加重叠测试总数。

## 中文 Native PIE 验收

地图 `/Game/SlayTheSpireDemo/Maps/L_Battle_RuinedCitadel`，生产 Native HUD，
B=true、D1=true、D2 验收期间显式开启。不得修改资产或延长动画冒充时序。

| 项目 | 操作步骤 | 通过条件 |
|---|---|---|
| 入场期间历史继续 | 打出攻击牌，观察卡牌仍从原位置向出牌区移动时伤害及后续记录继续。 | 来源准确，入场没有 Skip、传送、闪回、重复牌；正式状态只提交一次。 |
| 提前去向与连续尾部 | 打出效果简单的技能／能力牌，入场期间确认下一张牌或接受结束回合。 | 去向可以先提交；原卡先完成入场，再从出牌区连续进入尾部。多卡共存，无输入债务。 |
| 保守回退与选择 | 打出战吼等跨强制选择的牌，完成选牌确认。 | 无当前封存去向时入场仍 Blocking；选择隔离及之后 D1 去向正常，无卡死。 |
| 清理、同实例与视口 | 入场期间关闭 D2、Skip 或调整窗口；之后再出牌，观察同实例重新抽回。 | 无残留或旧回调影响；必要正式去向仍消费，新正式 Hand 正常交互。 |

未取得实际视觉证据的项目为 USER ACTION REQUIRED；数值／身份／一次性
请求由自动化证明，截图及实际操作证明视觉，不能相互冒充。

## 当前证据

契约基线 `04d835b`，契约提交 `931eb30`，实现提交 `ab07abb`。以下构建／Automation／PIE 均在
`931eb30` 加本批 Native C++／测试工作区上实际执行，D2 默认 false。
实现批次原状态为 IMPLEMENTED／AUTOMATED GATES PASS／PARTIAL PIE。
用户在交付 `557fceb` 后确认最后缩放门槛通过，收口提交 `a40335f`。下述
独立默认开启批次也已通过，当前 D2 COMPLETE／VALIDATED／NATIVE DEFAULT
ENABLED。C／D1／B 原验收不重开，G9 NOT SEALED。

## 实现和自动化证据 — 2026-10-09

Controller 的预检复制当前封存 Envelope 的候选快照与生命周期，调用共享
reducer；Base 限定准备／正式提交后激活／未来去向承接边界。Native job
保存完整旅程、准确来源、阶段和 pending 去向，入场无 Blocking token／
timer。正式收据独立保存；关闭、视觉丢失或 GC 之后仍按自己的游标消费。
新正式 Hand 优先，旧视觉 generation 的收据不能删除新 job。未通过准备
或超过 32 时保留现有 Blocking，历史无效仍走 Presentation 恢复。

| 运行 | 工程生成／Development Editor 构建 | Automation 实际结果与范围 |
|---|---|---|
| 首次代码构建 | 10.81／390.50 秒通过，`G9D2ProjectFiles.log`／`G9D2Build.log` | 尚未测试。 |
| 容量夹具修正 | 9.20／12.70 秒通过，`G9D2FixtureProjectFiles.log`／`G9D2FixtureBuild.log` | `G9D2` 报告 83 项：76 成功、4 带预期警告通过、3 失败。 |
| 夹具目标／历史引用修正 | 8.19／23.56 秒通过，`G9D2RepairProjectFiles.log`／`G9D2RepairBuild.log` | `G9D2Repair` 报告 30 项：29 成功、1 失败，无警告。 |
| 格挡表面补齐 | 7.66／77.36 秒通过，`G9D2BlockFixtureProjectFiles.log`／`G9D2BlockFixtureBuild.log` | 仅受影响 Loss／Capacity 两项重跑，`G9D2BlockFixture` 报告 2/2 成功，无警告。 |

日志在 `Saved/Logs/`，报告在 `Saved/AutomationReports/` 对应目录；原始失败
保留。首轮范围：G9D2、G9D1、G9C、Native R8、G9B、G8B、Native FastInput、
G6、CardSelection.Presentation、G8D／G8A。首轮把 HandInteraction／UIA3
冻结卡面的前缀写错而漏跑；第二轮使用实际前缀补跑这 14 项，同时重跑
受夹具修正影响的 D2／D1／G9C.Visual。第二轮唯一剩余失败因没有配置
Native 格挡表面，记录被正确 decline，无法形成需要验证的中间 Blocking
窗口；补齐表面后只重跑 Loss／Capacity。没有把 decline 改成跳过断言。

按案例身份去重并采用最后有效证据，**97 个不同案例均有通过证据，其中
4 个既有异常输入／空测试牌库案例带预期警告**。不将重叠的 83、30、2
相加，也不宣称最终版本一次 97/97。新增 6 个 D2 案例验证：提前去向／
多 job／GC、纯预检缺失／重复／错误身份／索引／不支持去向、Blocking
回退、发布重入、视觉丢失／关闭时必要关联、去向晚于入场、同 RuntimeId
回手、旧收据、合法容量与全局清理。容量使用真实十张手牌上限，经正常
回合流转、零费消耗牌累积 32 个 job；没有扩大 Gameplay 手数量。

首轮失败均为新增夹具问题：格挡定义错误地使用 None 目标；异常预检临时
替换封存记录数组导致调用方引用失效；最后是缺少格挡文本表面。修正夹具
后通过，没有修改 Gameplay、吞掉日志错误或放宽正式断言。

## 实际 Native PIE 证据 — 2026-10-09

MCP 启动生产地图，Native C++ CDO／生产 Blueprint CDO／初始 HUD 均读回
B=true、D1=true、D2=false；当前 PIE 通过运行时 setter 显式开启 D2，三项
均 true。读回文件 `Saved/G9D2NativeCDO.json`、`G9D2BlueprintCDO.json`、
`G9D2InitialHUD.json`、`G9D2OptInHUD.json`。日志 `Saved/Logs/G9D2PIE.log`。
没有保存资产或改动画时长；Native HUD 外部资产 SHA256 始终保持
`F896F7B59A91D4EC86AD73973451E19012B7523EE7D1D1ECEE2B570894E20F81`，排除提交。

| 观察 | 实际操作与结果 | 证据 |
|---|---|---|
| 入场历史继续 | 真实点击剑柄打击及敌人。来源是原手牌选中位置，卡牌仍向出牌区移动时敌人扣血、抽牌记录继续。 | `ArrivalStart.png`、`OverlapTimeline.json`。 |
| 完整 FIFO 与重叠 | 新 PIE 点击剑柄打击；入场时通过公开 HUD 输入，按 HUD 的冻结目标 Widget ID 确认燃烧和打击。两项均接受，顺序执行，追平后 Idle，能量 2，弃牌 2，敌人 HP 133；视觉 6／5／4 同时存续并各自退役。 | `ConfirmedOverlapTimeline.json`、`ConfirmedOverlap.png`、`ConfirmedOverlapComplete.png`；日志三个独立 Detached arrival。 |
| 入场期间结束回合 | 正常 HUD 出牌请求后第一帧接受 EndTurn；第一次 true、重复 false。原牌效果／去向继续，之后一次结束回合；最终下一回合 Idle、能量 5、无 job／反馈。 | `EndTurnTimeline.json`、`EndTurnBoundary.png`、`EndTurnComplete.png`。 |
| 战吼保守回退 | 真实点击战吼与玩家，日志 Hosted arrival，未进入 D2；强制选择表面 0/1 → 1/1，结束回合禁用。点击确认后选中牌进入抽牌堆，战吼消耗，恢复正常手牌。 | 当次窗口实际观察、日志 Runtime=11 的 Hosted arrival／去向／退役。 |
| 关闭不丢正式去向 | 勾拳入场第一帧关闭 D2，job 立即清空；后续两条状态历史继续，去向最终提交一次，Idle、能量 3、弃牌 1。 | `DisableTimeline.json`、`DisableBoundary.png`、`DisableComplete.png`。 |
| 同实例再出牌与 Skip | 第一轮 Runtime=6 经正常结束回合重新抽回，下一回合通过 HUD 再出；视觉 generation 从 1 到 7。入场期间 Skip，job 清空，显示 Idle、能量 2、弃牌 2，无反馈。 | `SkipTimeline.json`、`SkipBoundary.png`、`SkipComplete.png`；旧回调隔离另由自动化精确证明。 |

截图／JSON 位于 `Saved/G9D2VisualEvidence/`。首条观察脚本最初用 ViewModel
当前 LegalTargets 查 busy-time 目标，后续两项没有确认；该尝试仅证明普通
入场，不当作 FIFO 通过。新 PIE 改为 HUD 冻结目标入口后两项确认成功。
临时脚本只调用公开 HUD 输入、运行时策略及读取冻结 DTO／Widget，不写
Gameplay、不访问私有 job、不改资产；运行日志未发现资产加载或运行错误。

## 最后一项人工门槛已关闭：USER_REPORTED_PASS

实施批次无法用逐步窗口操作在 0.5 秒入场窗口内确定完成缩放，当时正确保留
人工待办，没有延长动画或用 C／D1 旧视觉证据代替本次 D2。实际同实例回手后
可交互已观察；“旧 job 尚未结束时新正式所有者获胜”由自动化证明。

| 操作步骤 | 预期现象／通过条件 | 需要的反馈 |
|---|---|---|
| 生产 PIE 显式开启 D2。打出一张牌，在牌仍向出牌区移动或尚未完成续接时立即拖动窗口边缘改变大小；动画结束后悬停并再次打牌。 | 原卡随视口正确定位，入场到尾部连续，无跳到端点、闪回、重复牌、裁切或残留；其余手牌排列正常，后续悬停／出牌正常。 | 用户在 `557fceb` 交付后回复“验证通过”，记为 USER_REPORTED_PASS；本项关闭。 |

实施批次保留浮动 PIE，D2=true 仅该运行实例，源码／Blueprint 默认仍 false。
最后读回 `Saved/G9D2ManualReadyHUD.json`／`G9D2ManualReadyVM.json`：
Idle、未锁定、可结束回合、无反馈、能量 2；手牌有剑柄打击、防御、坚毅、
怒火。临时 Python 观察器与 Slate observer 均已退出，编辑器 PID 80756 保留。
2026-10-09 用户针对唯一剩余缩放待办回复“验证通过”。不虚构新的截图、
配置或构建记录；原自动化、构建及其余视觉证据继续有效，全部 D2 人工门槛
关闭。下一独立批次启用 Native 默认、规定验证并读回生产配置；E／F 未开始，
G9 整体不封板。默认开启前新启动 PIE 仍需显式 setter，不能保存资产来开启。

## 独立 Native 默认开启批次 — 2026-10-09

人工收口提交 `a40335f` 后，Native 的 `bEnableDetachedCardArrivalD2` 默认设为
true，保留现有运行时关闭入口；通用 Base／Controller 不改默认，仍由生产
Native 绑定同步策略。共享 C／D1 卡牌夹具显式关闭 D2，D2 案例自行开启，
继续验证原 Blocking 断言，没有把旧用例改成解耦断言。

规定先生成工程，再构建 Development Editor，两项通过：8.73／186.53 秒。
日志 `Saved/Logs/G9D2DefaultProjectFiles.log`／`G9D2DefaultBuild.log`。实际一次
聚焦 Automation **85 项：84 成功、1 带预期警告通过、0 失败／未运行**；
警告为原 R8 异常身份拒绝案例。范围为 G9D2、G9D1、G9C.Visual、Native R8、
G9B、G8B、Native FastInput、HandInteraction、G6、CardSelection.Presentation、
UIA3.CardPlayedRichHandoff；日志 `Saved/Logs/G9D2DefaultAutomation.log`，报告
`Saved/AutomationReports/G9D2Default/index.json`。不与前一批 97 个案例累加。
已通过的动画视觉不重跑，也不再要求用户重复验收。

本批验证在 `a40335f` 加最终两处 C++／测试修改的工作区实际执行。用户已
关闭原编辑器后才构建，新启动自有编辑器 PID 32476，生产地图没有临时
setter 或属性覆盖。MCP 三处读回 Native C++ CDO、生产 Blueprint CDO、
实际 HUD 均为 B=true／D1=true／D2=true；ViewModel 初始 Idle、未锁定、可
结束回合、无反馈、能量 5。读回文件（均在 `Saved/`）：
`G9D2DefaultNativeCDO.json`、`G9D2DefaultBlueprintCDO.json`、
`G9D2DefaultRuntimeHUD.json`、`G9D2DefaultReadyVM.json`。
日志 `Saved/Logs/G9D2DefaultPIE.log`。已停止 PIE 并查询 false，文件
`G9D2DefaultStopPIE.json`／`G9D2DefaultStopped.json`；编辑器空闲保留。

没有保存资产，外部 Native HUD SHA256 保持原值并排除提交；生成文件、
日志和报告只保留在忽略目录。不 push。全部 D2 门槛关闭，COMPLETE／
VALIDATED／NATIVE DEFAULT ENABLED；E／F 未开始，G9 整体 NOT SEALED。
