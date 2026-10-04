# PHY-4 — 连续重力与有界侧向穿门

状态：**隔离的原生 Chaos 子步验证通过；生产接入与 Core PIE 仍开放**（2026-09-28）。此项承接[速度上限生命周期](InteriorPortalPhysicsSpeedCapLifecyclePHY4.md)，只修改固定姿态刚体的穿门许可条件和测试夹具。

## 发现与契约

`FNativeScene::Box` 默认关闭重力。此前两项“重力接触/部分穿越取消”测试设置了场景重力，却没有给被测方块启用刚体重力；其通过只证明了近地板时的保守拒绝与接触恢复，**不能作为真实重力积分的证据**。现在两项测试均显式开启方块重力，并断言受影响子步的预测位置确实向下移动。高重力近地板和普通重力半穿越取消仍通过。

新连续重力用例在无遮挡门洞的源侧与目标侧放置远离运动范围的地板，让方块以 `-980 cm/s²` 真实重力运动。初次诊断时，PreIntegrate 许可为 Clear，但 PostIntegrate 因旧的“速度及加速度只能沿门法线”条件返回 UnsupportedMotion，无法传送。这个方向限制与已使用的原生线速度上限、整步包络和完整门洞/场景扫描重复且矛盾。

许可器现在接受任意方向的**有界线性平移**，仍要求姿态固定、无角速度和角加速度、无持久约束，并在每阶段核对真实刚体、原生速度上限、实际位移不超过本子步速度上限可达距离及命令的最大位移。整个可达球及从源侧到映射后目标侧的扫掠体必须位于合法门洞并避开可碰撞障碍；接触修改和 PostSolve 继续撤销被新接触或状态变化污染的许可。许可失败时恢复普通墙体碰撞，取消向墙内的法线速度而保留重力的切向分量；不使用游戏线程逐帧位姿回滚。

这种约束允许自由方块在重力下连续穿过无遮挡洞口，同时近地板、门框边缘仍拒绝。它**不**证明着地支撑状态可跨门搬运、旋转中的刚体可穿门，或真实地图已启用这套许可器；这些属于后续 PHY-4 生产接入与 Core 验收。

## 验证

- UE 5.8 捆绑 .NET 工程生成 PASS：`Saved/Logs/PortalPhysicsPHY4ContinuousGravityFinalProjectFiles.log`。
- Development Editor 编译 PASS：`Saved/Logs/PortalPhysicsPHY4ContinuousGravityFinalBuild.log`。
- `SlayTheSpireDemo.Interior.Portals.PhysicsSolverClearance` **19/19 PASS、0 失败、0 测试警告**，2026-09-28 13:41 UTC：`Saved/AutomationReports/PortalPhysicsPHY4ContinuousGravityFinal/index.json`、`Saved/Logs/PortalPhysicsPHY4ContinuousGravityFinalAutomation.log`。覆盖单步/双子步的连续重力传送各一次、真实重力近地板拒绝与半穿越取消、有界侧向运动允许、虽当前方块在门内但下一步可达范围碰到门框时拒绝，以及原有速度上限/接触/生命周期回归。
- 初次诊断 18/19 PASS、1 失败仅用于定位法线限制；修复后的中间和最终运行均为 19/19。不要将诊断失败并入最终通过数。
- `/Game/House/L_Interior_LivingKitchen` 未编辑，SHA256 `0436F682DCD867E43ECEA81E89D4916D8B8ECFB2E66CA9321997BC97038BE25C`。

原生许可器仍只由 Editor 测试夹具连接；真实地图的物理方块仍走旧生产路径。下一步是实际世界场景/拓扑提供者、固定姿态以外的明确支持或安全拒绝、生产 adapter 与旧写入路径的原子切换，随后完成 Core 物理与人工 PIE 验收。用户已暂停引擎光照修改，互不混入。
