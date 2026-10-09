# G9-E：集成与清理

2026-10-09，分支 `codex/g9-buffered-input-detached-cards`，起点 `3922055`。
D2 COMPLETE／VALIDATED／默认开启，B／D1 默认开启保持。本轮仅 E，不进入
F，不宣称 G9 整体封板。不改资产、Legacy、插件或依赖；保留外部 Native
HUD 资产原 SHA256，排除提交。每个完整批次测试／文档后本地提交，不 push。

## 范围和顺序

按锁定 G9 设计的 E 集成矩阵执行，不增加新功能。复用已验收的 B／C／
D1／D2 状态、数值和视觉证据；只补足组合边界及本次清理影响的证明。

1. 保存本契约、矩阵和继续点，独立文档提交。
2. 收敛输入退役通知及 ViewModel／Controller 替换边界，移除已重复的
   单实例状态；规定生成／Development Editor 构建、一次聚焦自动化、
   必要 Native PIE，记录实际范围／结果并独立提交。
3. 只有 E 门槛全部满足才标记 COMPLETE／VALIDATED。F 另行开始；人工
   无法完成的项目明确 USER ACTION REQUIRED，不能提前封板。

## 本次收敛的边界

- 队列清理同步清除草稿／FIFO／旧 EndTurn，并通知 Native 输入表面；不得
  通过 Tick 或新的 Ready 事件才恢复指针牌／手势。已提交的 Gameplay
  正常完成，提交回合收据仍按已验收 B 规则保留；通知不消费队列。
- Session／绑定失效同步退役旧输入及私有视觉。开关关闭保留必要正式
  关联；Skip／恢复折叠对应历史才清理关联。三项开关继续独立控制，不
  新增总开关，不改变 Session 或 PlayerTurnSerial 来实现关闭。
- ViewModel 更换先撤销旧观察／输入绑定、退役旧 tracked 播放及 Controller
  所有权，再安装新模型。旧完成／取消／计时器不能修改新表面。重入期间
  较新的绑定优先，跨回调固定 UObject 存续；新 Controller 由正常绑定接口
  安装，不能把旧战斗 Controller 静默用于新模型。
- Hosted Blocking 完成由当前精确播放 token 在 job 中定位唯一视觉，再
  走现有视觉 token 校验；移除重复的 HUD 活动视觉 token。
- Draw 的准确附着对象只由 IncomingHandAttachment 持有；移除重复抽牌
  Widget 镜像、未使用目标索引与一次性 After 计数成员。保留当前 Blocking
  渲染器及其仍使用的动画参数，保证关闭／资源 decline／独立 R8 路径。

## 自动化集成矩阵

| 边界 | 必须证明的结果 |
|---|---|
| 输入清理＋D1／D2 关闭 | 忙碌期间已有已确认牌和指针草稿，关闭后立即退役表面；原牌去向继续提交一次，后续牌不扣费，Session／回合 serial 不变。 |
| Skip／恢复 | 卡牌视觉、草稿、FIFO、EndTurn 和临时附着清空；正式折叠准确；旧回调不恢复输入或视觉。 |
| HUD／Controller／ViewModel／Battle 替换 | 清理旧所有者，旧完成／取消及延迟评估无效，新模型／手牌身份及正常请求不受影响。 |
| Blocking 与 draw 附着 | 唯一 job 的精确完成、缺失视觉安全完成、旧 token 拒绝；draw 精确接管／取消／GC 无重复 slot／请求。 |
| 同实例／视口／GC／销毁 | 正式 Hand 优先；旧 job 不能复活，缩放或几何失效不消费输入或历史；所有引用与取消准确。 |
| G8／Selection 组合 | FastInput 关闭回退、EndTurn 仲裁／DirectBaseline／ABA，DamageNumber 共存、G6 选择和冻结卡面保持既有协议。 |

复用既有精确证明，回归受影响 G9B／C／D1／D2、Native R8／FastInput／
HandInteraction、G8A／B／D、Selection G0／G4／G5／G6／G7 及
CardSelection.Presentation／冻结 CardPlayed 卡面。报告只记实际执行结果，
不固定总数、不累加重叠报告。失败只重跑受影响门槛。

## 中文生产 PIE 验收

地图 `/Game/SlayTheSpireDemo/Maps/L_Battle_RuinedCitadel`，Native 生产 HUD。

| 操作步骤 | 预期现象／通过条件 |
|---|---|
| 打出带抽牌的牌，播放期间确认下一张牌、再选指针牌草稿；在当前牌尚有 Blocking 历史时关闭 D1 或 D2。 | 草稿立即回手、目标／手势取消，队列不继续出牌；当前牌结算与去向正常，关闭后的后续普通输入仍可用。 |
| 播放期间准备草稿与确认队列，执行 Skip；之后正常选牌出牌。 | 无指针牌、目标箭头、幽灵或重复请求残留；恢复的手牌可交互，数值只提交一次。 |
| 在本批受影响的 draw／Blocking 回退路径完成一次普通出牌、抽牌及选择；缩放观察复用已通过 D2 实际视觉。 | 抽牌对象完成后正常接管，剩余手牌排列稳定，选择／伤害数字与私有卡牌视觉共存，无输入卡死。 |

身份、计数及不可通过界面的替换／销毁组合由自动化证明。已有 D2 窗口
缩放 USER_REPORTED_PASS 及其余未改变的视觉不重复要求人工操作。

## 当前状态

E IN PROGRESS；本契约批次仅文档，没有新构建／测试／PIE。现有编辑器
PID 32476 为上一批自有、只读默认检查后保留的空闲实例；构建前确认
实际进程和资产状态再关闭或重启。不得按旧 PID 管理用户后来启动的实例。
