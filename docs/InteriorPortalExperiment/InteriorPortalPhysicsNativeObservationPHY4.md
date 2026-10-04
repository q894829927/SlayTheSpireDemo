# PHY-4 — 真实子步的只读许可前置检查

状态：**原生绑定校验与保守拒绝已在实际地图验证；生产通行许可和物理切换仍开放**（2026-09-29）。前置 HEAD `166962a`。

`FPortalPhysicsBindingBridge` 仍只拥有一个真实 Chaos 观察回调和一个 PT `FChaosPassageSession`。每次收到当前绑定域的输入，回调在 `PreIntegrate` 读取原生方块位姿、速度及加速度，构造实际 solver frame、序号和 `DeltaSeconds`，调用现有 `FChaosStaticClearance::Certify_Internal`，把原因、绑定问题、组件编号和物理步标识送回 GT。没有消费 proof、修改接触、安装速度上限、施力、移动物体或传送；旧 Gameplay 路径仍是唯一写入者。无新输入时不颁发观察结果，绑定变化和 proxy 注销沿原有退役回执处理。

检查顺序明确分为两层。首先核对命令域、方块与两端支撑的原生形状、碰撞关系、局部 bounds、位姿、COM 和墙体法向跨度；失配时给出结构化 `BindingChanged` 子原因并退役。之后才检查物理步时间、原生速度上限、门洞及场景。这样 `InvalidInterval` 或 `UnsupportedMotion` 表示**本次原生绑定检查已通过，但该步仍未获通行许可**。GT 缩放烘焙与 Chaos 单精度碰撞 bounds 可有舍入差异：比较上限为两倍 float 相对精度且不超过 `0.01 cm`；真实入口墙观测差为 `0.000002384186 cm`，旧固定 `0.000001 cm` 容差会使其反复误退役。UE 的 `1/60 s` 子步也会以近似值传递；时间预算只容纳最高 `0.0000001 s` 的表示误差，实际步长仍用于所有 reach/sweep 计算，明显超时仍拒绝。

目标地图 `/Game/House/L_Interior_LivingKitchen` 的只读 PIE 副本连续观察到 3 次真实子步、`0` 次绑定失配。最终运行的最后一步 `DeltaSeconds=0.016666699`，在允许的微小表示误差内进入速度上限检查，返回 `UnsupportedMotion`：旧写入者未安装 native hard speed cap。另一次运行的最后一步为 `0.016666900 s`，超过 `MaxStepSeconds=0.016666667` 及表示误差容差，返回 `InvalidInterval`。两者都是正确的保守拒绝。**尚未证明该地图能扫描到 `Clear`，也未证明真实场景障碍、接触和支撑绕过可获许可**；不得将这一步描述为物理穿越验收。

UE 5.8 捆绑 .NET 工程生成及 Development Editor 最终编译 PASS。最终地图 `MapBindingCoverage` **1/1 PASS、0 警告**；受影响 `SlayTheSpireDemo.Interior.Portals.Physics` **46/46 PASS、0 失败，其中既有 foundation 项有 2 条警告**。测试覆盖真实地图连续子步和绑定零失配、无速度上限拒绝、明显超时仍不放行。证据：`Saved/Logs/PortalPhysicsPHY4NativeObservationProjectFiles.log`、`Saved/Logs/PortalPhysicsPHY4NativeObservationBuildFinal5.log`、`Saved/AutomationReports/PortalPhysicsPHY4NativeObserveMapFinal5/index.json`、`Saved/AutomationReports/PortalPhysicsPHY4NativeObservePhysicsFinal5/index.json`。用户地图未保存，SHA-256 仍为 `0436F682DCD867E43ECEA81E89D4916D8B8ECFB2E66CA9321997BC97038BE25C`。

下一步必须在单一生产物理所有权方案内处理实际 solver 步长与硬速度上限，接入 held/free 共用输入，再让同一 provider 的完整场景检查形成逐步许可。旧写入者和新写入者只能在一次原子切换中交接；随后实现 fact/ack 排空与 Core 自动化、人工 PIE。当前观察结果不授权提前放行墙体碰撞。
