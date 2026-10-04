# PHY-4 — 方块速度上限安装与近地板取消

状态：**隔离的原生物理验证通过，生产接入未开始**（2026-09-28）。承接[重力接触前撤销许可](InteriorPortalPhysicsGravityCancellationPHY4.md)，本项建立速度上限的真实 UE 安装路径，并验证半穿越后近地板的取消过程。原重力夹具缺陷及后续更正见下文。

## 所有权与顺序

`FChaosTravellerSpeedCap::Install_GameThread` 在**当前有效、模拟物理的 BodyInstance** 上使用 UE 公开的 `FPhysicsCommand::ExecuteWrite` 和 `FPhysicsInterface::SetMaxLinearVelocity_AssumesLocked`。调用方负责先完成刚体注册和绑定身份；安装成功只表示 GT 命令已写入，不是物理步许可。PT `FChaosStaticClearance` 仍在 PreIntegrate、PostIntegrate、contact modification 和 PostSolve 读取实际 native 上限，任何缺失或变化都拒绝当前许可。驱动速度/力在配置完成后施加，随后才进入受影响子步。

生命周期测试纠正了上一增量的诊断解释。此前夹具只在 PT 直接写 `MaxLinearSpeedSq`，后续 GT 状态同步可能覆盖它；普通碰撞过滤更新不是必然重建刚体。通过 GT 接口安装后，普通过滤更新保留上限且原有过滤通行仍通过。显式 `RecreatePhysicsState` 会创建新物理体，其上限回到 Chaos 默认的近似无限值，旧许可绑定因原生卸载而永久退役；即使新体重新安装同一数值，也不能复活旧绑定。调用方必须以新的注册/绑定身份重新建立许可器。此代码没有自动重新绑定，也不在旧许可器内部悄悄补写上限。

原有普通重力取消测试先用零重力法线运动进入门洞中途，再设置 `-980 cm/s²` 场景重力并加入距方块底面 1 cm 的可碰撞地板。**勘误：该版本夹具的方块重力实际上关闭，故 18/18 的通过只能证明近地板包络拒绝与半穿越接触恢复，不能证明真实重力积分。**后续[连续重力增量](InteriorPortalPhysicsContinuousGravityPHY4.md)为方块开启原生重力、断言它确实向下积分，并以 19/19 回归重新验证取消及无遮挡门洞的连续穿越。

## 验证

- UE 5.8 捆绑 .NET 工程生成 PASS：`Saved/Logs/PortalPhysicsPHY4SpeedCapProjectFiles.log`。
- Development Editor 最终编译 PASS：`Saved/Logs/PortalPhysicsPHY4SpeedCapFinalBuild.log`。
- 当时受影响 `SlayTheSpireDemo.Interior.Portals.PhysicsSolverClearance`：**18/18 PASS、0 失败、0 测试警告**，2026-09-28 13:19 UTC；`Saved/AutomationReports/PortalPhysicsPHY4SpeedCapVerified/index.json`，`Saved/Logs/PortalPhysicsPHY4SpeedCapVerifiedAutomation.log`。重力效果结论按上文勘误；后续真实重力结果以连续重力增量为准。
- 初次换用 GT 接口的 15/18 诊断运行发现睡眠接触岛夹具先设驱动、后装上限会失去首帧速度，同时旧的过滤重建断言不成立。调整顺序并改用真实刚体重建用例后，最终全量受影响回归通过。诊断失败不并入最终通过数。
- 用户修改的 LivingKitchen 地图未编辑，SHA256 仍为 `0436F682DCD867E43ECEA81E89D4916D8B8ECFB2E66CA9321997BC97038BE25C`。

新安装原语虽位于运行时模块，调用方仍仅为编辑器 Chaos 测试夹具。实际地图的方块没有启用这套生产绑定、许可和单一驱动；本项不关闭真实玩法的抽动、穿越或视觉缺陷。持续重力自由穿越由后续增量覆盖；旋转/接触状态、真实世界查询提供者、物理 adapter 与旧写入路径的**原子切换**仍未完成。Core 手工 PIE 验收保持开放。
