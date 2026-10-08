# G9-D1：出牌去向尾部演出解耦

日期：2026-10-09。分支 `codex/g9-buffered-input-detached-cards`，起点 `04124e2`。
G9-C 已 COMPLETE／VALIDATED。当前 D1 为 **IMPLEMENTED / AUTOMATED GATES
PASS / USER ACTION REQUIRED / NATIVE DEFAULT OFF**；不实施 D2／E／F。

## 实施边界与提交批次

1. 先提交本执行约定与阶段继续点，不改代码或资产。
2. 实施 Controller 正式去向事务、Base 受限视觉接口、Native 独立尾部及
   运行时开关；完成规定生成／构建、聚焦自动化、必要 Native PIE 和证据后
   独立本地提交。人工门槛未完成可提交实现，但不能宣称阶段完成。
3. 全部 D1 门槛通过后才独立启用 Native 默认，核实生产启动路径并提交。

CardPlayed 入场仍 Blocking。D1 仅适用 PlayArea→DiscardPile／ExhaustPile／
Removed 的准确生命周期去向。Gameplay／历史仍串行；Selection Group、
同时弃牌及抽牌附着协议沿用已有边界，不推断分组或重建实时 Gameplay。
外部 Native HUD 资产保持原样并排除提交；不修改 Legacy、插件、依赖或
生成文件，不 push。

## 提交与视觉准备事务

Controller 复制当前封存记录、候选快照及生命周期状态，使用共享 reducer
证明历史有效。Native 准备现存 AtPlayArea job 的冻结起终点／参数，准备
收据携带准确 Session、出牌发生、视觉 generation 与去向记录身份。
准备不移动卡牌、不写正式历史、不占用 Blocking 完成回执。

准备后复核封存记录游标／绑定／Session／开关；上下文仍有效的准备失败
走已有 Blocking 路径，历史无效走既有 Presentation 恢复。正式提交同时
安装候选快照及已消费关联，再发布；发布后复核上下文，才激活私有尾部。
提交后失败只能退役准确视觉，不能重跑 reducer 或回退重播。

尾部不注册 Controller 回调、Blocking timer 或输入债务。它按 job 的时间
更新并自行退役，不能授予 Ready、消费输入队列或请求 Gameplay。正式
去向提交后即可推进后续历史／正常 Ready，从而允许 A 尾部与 B 入场共存。
新正式 Hand 所有者优先；旧 token 不得触碰新 Widget。Selection 的临时
隐藏不控制已解耦尾部。

## 运行时开关与清理

D1 提供独立运行时开关，验收前 Native 默认 false；B 默认 true 保持不变。
关闭清理待执行输入、已解耦尾部及未激活准备，不改回合 serial／Session，
也不删除尚待正式去向消费的关联。后续去向回到 Blocking 路径。
Skip、恢复、Session／HUD／Controller／Battle 替换和销毁都清理准确私有
视觉；即使没有活动 Blocking unit，已解耦尾部也不能残留。普通 Envelope
完成保留仍必要的跨强制选择关联，视觉退役不修改正式快照。

## 自动化门槛

- 去向正式提交一次，发布前准备失败零正式副作用；入场仍等待 Blocking。
- 发布重入的 Skip／替换／关闭，以及提交后的激活／视觉失败不能重复提交。
- 尾部完成无 Controller／输入回调；A 尾部、B 入场可并存，GC 安全。
- 运行时关闭保留必要关联；无活动 unit 的 Skip／恢复清理全部尾部。
- 同 RuntimeId 的新正式 Hand 优先，旧回执不影响新 Widget；视口更新只作用
  于私有几何，无法安全显示时只删除视觉。
- 回归受影响 G9C／R8、G9B／G8B／FastInput、G6／Selection、DamageNumber
  及冻结 CardPlayed 卡面。记录实际范围／结果，不预填固定总数。

每个 C++ 批次按项目命令先生成工程，再构建 UE 5.8 Development Editor，
执行一次聚焦 Automation；失败只重跑受影响或尚未完成的门槛。

## 中文 Native PIE 验收

地图：`/Game/SlayTheSpireDemo/Maps/L_Battle_RuinedCitadel`，生产 Native HUD，
B=true，D1=true（验收期间显式启用），D2 尚未实施。

无需修改资产；在新编辑器首次 PIE 的 UE 控制台调用原生运行时入口：

```text
py import unreal; unreal.find_object(None, "/Engine/Transient.UnrealEdEngine_0:GameInstance_0.WBP_BattleHUD_Native_C_0").set_detached_card_destination_d1_enabled(True)
```

该 HUD 路径来自本次 MCP 实际读回；后续 PIE 若实例编号变化，以实际 HUD
路径为准。关闭时将 True 改为 False。开关只改变当前运行策略，不保存资产。

| 项目 | 操作步骤 | 通过条件 |
|---|---|---|
| 尾部与后续出牌重叠 | 播放 A 时确认 B，观察 A 飞往弃牌后 B 开始入场。 | A 未被 Skip；A 尾部与 B 入场共存，A 去向数值只增加一次。 |
| 尾部期间结束回合 | A 正式去向提交后、尾部尚在移动时接受结束回合。 | 按既有同回合权限执行一次；尾部不锁输入或控制 Controller，正式手牌正确。 |
| 消耗／移除及选择 | 打出能力牌、战吼或消耗选牌，完成必要选择。 | 原入场及选择交接正常，正式去向先提交，私有尾部正常清理，无幽灵或卡死。 |
| 清理与回退 | 播放期间关闭 D1 或调用已有 Skip，再正常出牌；调整视口。 | 无残留、旧回调、重复请求；关闭后去向回到 Blocking，正式状态不回滚。 |

工具无法取得精确运动帧内证据的项目标记 USER ACTION REQUIRED。截图
证明画面，自动化证明数值／身份／一次性请求，不互相冒充。

## 当前证据

契约批次 `a211cd2` 仅保存执行约定，无新 UE 运行。实现已提交为 `96de67b`
（`feat(g9-d1): detach committed card destination tails`）；下列为该代码
批次实际证据。后续交付编号记录仅改文档，无新 UE 构建、自动化或 PIE。

### 实现及协议验证

实现验证基线为 `a211cd2` 加本批代码。正式卡牌视觉 token 移到 Presentation
的共享类型头，Base 与 Controller 不依赖具体 Native job 的类型定义。
准备收据额外带去向身份及独立 preparation generation，避免重试后的旧
收据清理新准备。Controller 仅在候选快照及已消费关联安装后开放准确的
已提交收据；Base 拒绝提前激活。原 Damage 与 D1 共用准确游标复核和推进
方法，不引入时间债务。正式事务跨回调固定 UObject GC 存续。

Native Blocking 与 D1 共用去向几何准备；D1 job 自行结束，无 timer 或
Controller 回调。关闭只清理已解耦尾部和准备，不删除必要正式关联。
原 C 测试的真实 Controller／Native HUD 夹具抽为共享测试头，并增加多牌
及不同去向参数；没有新增运行时测试入口或 Gameplay 内容组合分支。

规定生成 `Saved/Logs/G9D1ProjectFiles.log`：通过，10.06 秒。
Development Editor 构建 `G9D1Build.log`：通过，313.27 秒。
首轮聚焦报告 `Saved/AutomationReports/G9D1/index.json`：**87 成功、
4 带警告通过、0 失败／未运行，共 91 项**。实际范围为 G9D1、G9C、
Native R8、G9B、G8B、FastInput、G6、CardSelection.Presentation、G8D、
G8A、冻结 CardPlayed 卡面及 HandInteraction；精确案例以报告为准。
警告为一个 R8 无效身份预期拒绝及三个 G8A 空测试牌组提示。

复核补充正式发布后视口失效，以及非有限私有几何的安全清理：规定生成
`G9D1GeometryProjectFiles.log`（7.12 秒）、构建 `G9D1GeometryBuild.log`
（9.07 秒）通过。仅重跑受影响的 G9D1 与 G9C.Visual，报告
`Saved/AutomationReports/G9D1Geometry/index.json`：**10 成功、0 警告／
失败／未运行**。未受影响首轮证据复用，91 个不同案例有有效证据，不能
把两轮相加或宣称最后版本一次 91/91。

六项新增用例包含多个实际边界：入场 Blocking、准确准备／禁止提前激活、
纯准备、资源 decline、A 尾部与 B 入场／GC、私有结束零发布、发布／激活
重入 Skip／关闭／替换、提交后视觉或视口失效、三种正式去向、无活动
unit 的全局清理、关闭保留关联、正常回合流程同 RuntimeId 再抽回并出牌。

### 生产启动检查与当前视觉限制

UE MCP 在生产地图启动实际 Native 浮动窗口 PIE，默认 D3D12／SM6。
公开读回 `Saved/G9D1InitialHUD.json` 确认 B=true、D1=false，并绑定生产
ViewModel；未临时改开关。日志 `Saved/Logs/G9D1PIE.log`。这只证明加载与
启动配置，不证明本次尾部重叠视觉行为。

computer-use 首次激活窗口失败；重新获取窗口后截图确认 Windows 锁屏。
按该技能引用的操作规则停止窗口输入，没有尝试解锁或绕过锁屏。
本轮未实际出牌、未启用 D1、未取得新的尾部视觉通过证据；上述四项
Native PIE 均为 **USER ACTION REQUIRED**。已通过的 C／B 不重复要求。
用户已收到解锁提示；解锁后从这些具体门槛继续，不重跑未受影响自动化。

本次启动的 PIE 已由 MCP 停止，查询为 false，编辑器已关闭；没有资产保存。
证据为 `G9D1StoppedPIE.json`、`G9D1PIEStopped.json`、`G9D1EditorClosed.json`。
外部 Native HUD 资产 SHA256 保持
`F896F7B59A91D4EC86AD73973451E19012B7523EE7D1D1ECEE2B570894E20F81`，
排除提交。D1 视觉门槛通过前不启用默认、不进入 D2，不宣称阶段完成或封板。
