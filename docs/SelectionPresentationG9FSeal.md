# G9-F：证据汇总与封板

日期：2026-10-10。分支 `codex/g9-buffered-input-detached-cards`。
审查起点 HEAD `b7570aa`，最终运行时代码 `50b7e24`；两者之间只有文档
修改，当前 Source／Config／Plugins 没有未提交变更。

**G9 COMPLETE／VALIDATED／SEALED，G9-F COMPLETE／VALIDATED。**
Native 唯一生产 UI；缓冲输入、D1 去向、D2 入场三项默认 true，保留各自
运行时关闭及保守 Blocking 路径。封板依据是各阶段已有的有效构建、
受影响自动化、生产 PIE 和用户人工反馈，不是本次新跑的全量报告。

## 范围、文档权限和证据复用

本批只修改文档，按 [验证执行政策](ValidationExecutionPolicy.md) 不重新
构建或运行 Automation／PIE。锁定设计第 14 节的 F 门槛由下列已有证据
满足，没有要求本次纯文档 HEAD 重新执行所有测试的额外门槛。

当前输入契约以 [B 修订](QueuedCardPlayAndRelicTimingAmendment.md)、
[B 收口](G9BClosure.md)、[持牌拖放](NativeCardDragRelease.md) 和
[E 集成](SelectionPresentationG9EExecution.md) 为准。原设计中的单意图、
只缓冲选中、追平后重新确认及旧 Skip／EndTurn 描述属于被修订的历史。
未被修订的 Gameplay 权限、纯 reducer、视觉事务和回退约定继续有效。
各阶段执行文档中的“后续尚未实施／G9 未封板”记录当时状态；当前
总状态由本文件和 `DevelopmentPhases.md` 统一。

F 核对了阶段提交均为当前 HEAD 的祖先、报告实际结果、已记录的生成／
构建成功、早期失败的同名案例后续成功及人工收口来源。没有将不同范围
或重叠报告相加，也没有虚构最终版本一次全量通过。

## 阶段证据总表

报告简称指 `Saved/AutomationReports/<简称>/index.json`，日志位于
`Saved/Logs/`。精确范围、失败和修正顺序以阶段执行记录为准。

| 阶段 | 实现／收口提交 | 构建和自动化依据 | 人工／生产依据及结果 |
|---|---|---|---|
| A：权限基础 | `cafe7bf` | [A 记录](SelectionPresentationG9AExecution.md) 已提交生成／Editor 构建成功；同次 G9A 6、G8B 9、FastInput 2 项共 17 成功。仅复用历史证据。 | A 不启用生产输入或新视觉，没有人工 PIE 门槛。完成。 |
| B：FIFO／布局／遗物／同时弃牌 | 遗物 `6071c56`、FIFO `b105fb1`、弃牌 `72a564a`；收口 `a6b4877`，默认启用 `2c29025` | [修订实施](G9BRevisionExecution.md)、[B 收口](G9BClosure.md)。`G9BDefault` 45 成功；生成／构建 7.31／162.50 秒成功。遗物／Group 独立范围不与默认报告相加。后续 E 回归受影响输入。 | [B 生产 PIE](G9BRevisionNativePIE.md) 证明弃牌／遗物；用户确认拖放、队列后 EndTurn、防重与跨回合、强制选择清空全部完成。代理实测与 USER_REPORTED_PASS 分开保存。完成。 |
| C：共享历史／多实例所有权 | `c69b0ac`、`5f1d4aa`；人工收口 `04124e2` | [C 记录](SelectionPresentationG9CExecution.md)。历史批次和视觉首轮保留有效证据；最后 `G9CVisualBinding` 58 成功、1 预期警告通过、0 失败／未运行，生成／构建 4.31／7.01 秒成功。后续 E 包含 G9C。 | 普通／抽牌／能力／跨选择、Skip、不同视口通过；运动中缩放最后一项用户确认。完成。 |
| D1：独立去向 | `96de67b`；收口 `1cc58de`，默认启用 `98d917a` | [D1 记录](SelectionPresentationG9D1Execution.md)。`G9D1` 87 成功、4 预期警告通过；`G9D1Geometry` 10 成功。默认首轮的失败由 `G9D1EnableFallback` 单项修正成功，生成／构建 3.83／6.96 秒成功；原通过项保留。 | 尾部重叠、EndTurn、能力／选择、关闭／Blocking／Skip 通过；运动尾部缩放用户确认。完成。 |
| D2：独立入场 | `ab07abb`；收口 `a40335f`，默认启用 `a6bc659` | [D2 记录](SelectionPresentationG9D2Execution.md)。早期失败逐项闭合；`G9D2BlockFixture` 2 成功。默认 `G9D2Default` 84 成功、1 预期警告通过、0 失败／未运行；生成／构建 8.73／186.53 秒成功。 | 入场推进历史、多 job／FIFO、一次 EndTurn、战吼回退、关闭保留去向、同实例再出与 Skip 通过；运动中缩放用户确认。C++／Blueprint／生产 HUD 默认三处均 true。完成。 |
| E：集成清理 | `50b7e24`；交付证据 `b7570aa` | [E 记录](SelectionPresentationG9EExecution.md)。`G9ERepair` 86 成功、4 预期警告通过、0 失败／未运行；`G9ENullOwner` 单项成功，生成／构建 7.12／14.09 秒成功。未受修正影响的首轮 HandInteraction、CardSelection 各 13 项日志成功保留；不相加。 | 关闭 D2 即时退役草稿、原牌一次提交、旧队列停止、后续正常出牌；Skip／DamageNumber、战吼 Blocking 抽牌接管、选择及恢复通过。旧运动视口证据复用。完成。 |

### 同名旧报告与已关闭失败

- 本地 `G9A/index.json` 写入时间为 2026-09-13，只有 8 项，与
  2026-10-05 已提交 A 记录的 17 项不同；本地对应新日志缺失。旧报告
  明确排除。A 使用 `cafe7bf` 的验收记录和 `Validation.md` 历史证据，
  不声称本次查到该运行的原始日志；后续权限／ABA 回归见 B／E。
- C 原历史批次和 E 首轮曾中止且没有完整 JSON，不伪造缺失报告。
  保留执行记录中的成功日志和定向补跑；E 两个 DirectBaseline 失败在
  `G9ERepair` 精确同名案例成功，GC 夹具崩溃也由新增 E 用例成功闭合。
- `G9D1Enable` 的 `G9B.NativeHandBlockingWithoutTick` 后续成功；
  `G9D2` 的 Capacity／Loss／PurePreflight 三个失败及 `G9D2Repair`
  的 Loss 失败均匹配到后续同名案例成功。原失败报告保留。

F 实际读取的关键通过报告 SHA256 如下，用于区分重名／覆盖文件，不是新运行。

| 报告 | SHA256 |
|---|---|
| `G9BDefault` | `C52F600676402BAABE33C3E7A986F9B1D1631E596DF1D3C6E4655EFCAB0AD059` |
| `G9CVisualBinding` | `43D14A21BFA7CE045CB19685B1060CD1D60687520A995EFB50C76F0061AF1B8D` |
| `G9D2Default` | `295CBC1650B9BD59CCEC2F51FF172E5BD4F8A82178B1A5FB27FF6B4883044ED6` |
| `G9ERepair` | `6A69728597645753FFE29881F57E3DFC61D55E91EEBE66B6EFEFB358456A086A` |
| `G9ENullOwner` | `ABCB94152D942C7EF2598B1861BA0D493EA5AB61DD452015CD64F2E7F53DF7A0` |

## 最终生效契约

1. HUD 唯一输入所有者持有一个草稿、最多 32 项已确认 FIFO 和一个精确
   回合 EndTurn 标记。同一卡牌实例不重复入队；入队不扣费、不移动
   Gameplay 区域、不刷新实时绑定。捕获 revision 记录来源，普通历史
   推进不使 FIFO 整体失效。Battle／回合／Session／绑定 generation、
   卡牌／目标身份仍须有效；消费时解析当前绑定并验权，失效项提示后
   跳过，不替换目标。无历史模式不伪造 Session。
2. 下一张请求等待上一张 Gameplay、遗物反应及 Blocking 历史完成，独立
   视觉可以继续。消费前移除队首，普通忙碌等待。Ready／Presentation／
   ViewModel 通知驱动合并、非重入评估；NativeTick 只处理悬停、箭头和
   视觉，不消费输入或 reducer。
3. EndTurn 先接受 `BattleId + PlayerTurnSerial` 权限，保留此前确认及
   合法忙碌重试，取消未确认草稿／FastInput，禁止后续追加。已有牌完成
   后结束同回合一次；转发复核 Gameplay 权限，不要求显示追平，但不
   越过此前 FIFO。提交收据防止旧显示预订未来回合。强制选择立即
   清空旧输入并禁用 EndTurn，完成选择不恢复旧意图。
4. 显式 Skip／恢复／运行时关闭和实际绑定退出退役对应待执行输入与
   私有视觉，已提交 Gameplay 正常完成。有效 Session 失效复核凭据；
   无旧 Session 的 DirectBaseline 普通 Ready 不误清同回合队列。模型／
   Controller 更换先解绑、清理再安装，较新重入绑定优先；旧计时器／
   token 不修改新表面。输入清理同步恢复指针牌和手势。
5. Hand 按 `(BattleId, RuntimeId)` 保留冻结顺序、Widget／slot／Slate
   身份和 Hidden 历史槽位；唯一结构入口完整准备后提交。Slate 面板
   首次布局按当前尺寸计算基础几何，悬停不写结构，移动几何按精确
   token 保护；抽牌由 GC 安全 IncomingHandAttachment 唯一管理并接管。
6. 共享纯 reducer 服务 Blocking、解耦和 Group 预检。正式发生关联与
   视觉 generation 分离；去向只消费一次，视觉丢失不消费关联。D1 准备
   后提交去向，D2 完整证明当前封存 Envelope 的唯一受支持去向后提交
   入场；提前去向在入场结束后连续接尾部。准备失败保留 Blocking，提交
   后失败只清视觉；历史错误走 Presentation 恢复，不请求 Gameplay
   ResolutionFault。新正式 Hand 优先于同 RuntimeId 旧 job。
7. 私有 host 不可交互，job 容量 32，GC 安全并有独立几何／时钟／阶段。
   Native B／D1／D2 默认开启；D2 准入同时要求 D1、D2 开启，无法完整
   证明仍 Blocking。通用 Base／Controller 保持显式策略，必要 Blocking
   渲染器保留；关闭视觉不删除尚待正式去向消费的关联。
8. Status 反应原子插入队首，Relic 事件反应插入队尾；实际收益和演出
   位于本张全部效果／去向之后、下一张请求之前，不中断动态抽牌
   continuation。事件排序和 pre-commit Modifier 不变，不检查遗物 ID。
   TurnEndDiscard 显式冻结成员，完整预检后沿各自轨道同时弃牌；Gameplay
   和 reducer 逐条提交，缺失或干扰安全串行降级。

## 中文封板检查结果

| 检查内容 | 结果 |
|---|---|
| 阶段依赖和提交可追溯。 | 通过，表中实现／收口提交均为 `b7570aa` 祖先。 |
| 构建、受影响自动化、失败闭合及默认生产读回有独立证据。 | 通过，原报告／日志含义保留，不累加总数。 |
| 输入、弃牌／遗物、Blocking／独立视觉及运动视口人工门槛闭合。 | 通过，代理实测与 USER_REPORTED_PASS 分开，无剩余 G9 人工待办。 |
| 设计当前状态／原则、英中文架构、阶段、Validation 和检查点一致。 | 本批统一为最终契约和封板状态，旧历史保留。 |
| F 只改文档，无新代码、资产、Legacy、插件、依赖或运行验证。 | 通过，文档链接和 diff 检查通过后独立本地提交，不 push。 |

## 封板边界与交付

范围是 G9 Native C++／测试与上述已接受的本地生产配置，不声称新增
Blueprint 图、打包或 Shipping 验证，不关闭卡牌扩展、角色动画等独立
历史任务。未单独提供 HEAD／配置的人工反馈保留这种限制，不能补写成
代理实测；后续受影响改动继续按项目政策验证和独立提交。

外部 `Content/SlayTheSpireDemo/UI/Widgets/WBP_BattleHUD_Native.uasset`
保留且排除提交；实际生产配置沿用默认批次／E 读回，不将未提交资产
写成已封板提交的资产。F 前后 SHA256 保持
`F896F7B59A91D4EC86AD73973451E19012B7523EE7D1D1ECEE2B570894E20F81`。
报告、日志、截图、临时审查及生成文件均留在忽略目录。E 已停止 PIE 并
关闭自有编辑器，F 不启动编辑器。

本文件随 F 文档批次提交，提交后只补交付编号；运行时代码仍为 `50b7e24`。
本次没有启动 G9 之外的新开发阶段，后续目标由用户另行选择。
