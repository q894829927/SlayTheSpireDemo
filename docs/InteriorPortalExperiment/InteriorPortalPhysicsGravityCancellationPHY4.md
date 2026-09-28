# PHY-4 — 重力接触前撤销穿门许可

状态：**隔离的 Chaos 物理子步验证通过；尚未接入生产关卡**（2026-09-28）。本项延续[PHY-4 物理执行](InteriorPortalPhysicsPHY4.md)的窄范围证明器，不改地图、玩法公开接口或引擎，也不恢复旧的逐帧拉回方案。

## 发现与架构决定

通过已启用的 Unreal MCP 只读检查 `/Game/House/L_Interior_LivingKitchen`：`Portal_TestCube` 使用 `PhysicsActor`、启用模拟和重力，质量覆盖为 12 kg；它的底面初始高于地面约 15 cm。因此无重力、无接触夹具不能代表实际方块的落地阶段。MCP 未修改或保存关卡，也未执行 PIE 视觉验收。

原生诊断夹具试图验证方块在一个 Chaos 步内被重力推进到地板。**后续审计发现，该夹具虽然设置了场景重力，却保留了 `SetEnableGravity(false)`；因此当时的观测不能证明重力驱动的接触时序。**仅按提交的沿门法线预测轨迹检查仍有漏掉重力及未知力可达接触的架构风险；contact modification 只能是额外防线。修正后的实际重力子步证据见[连续重力增量](InteriorPortalPhysicsContinuousGravityPHY4.md)。

现在由持有许可的刚体在 Chaos 上安装**原生最大线速度平方**，并在每个子步读取真实 native particle。以本地碰撞体最大旋转半径、速度上限乘子步时间、门边距构造包络：它覆盖重力、未知力和任意姿态可能到达的空间。该包络必须位于合法门洞侧向/高度内，同时源侧及映射后的目标侧不能与任何实际可碰撞障碍相交。原生上限不存在、过大、在步骤间变化，或持久约束出现时，证明器在放行之前拒绝；contact modification 还会重新检查上限与约束。

Chaos 的速度上限只约束积分，不约束碰撞和关节求解位移。因此有潜在地面接触的步骤**不获得穿墙许可**；不能据此推断“有重力且接地的方块已经支持穿门”。活跃互相接触的岛、关节、CCD、一般旋转与边缘取消仍待覆盖。生产 adapter 必须在物理刚体创建后，通过 UE 游戏线程物理接口安装上限；PT 许可器只读取并逐阶段验证。早期夹具直接写 PT 上限，后续 GT 状态同步可能覆盖它；当时把这一现象归因于普通碰撞过滤更新是不准确的。经后续生命周期测试确认：GT 安装的上限在普通过滤更新中保留，真正重建刚体时恢复默认值、旧绑定退役，需新绑定并重新安装。详见[速度上限生命周期](InteriorPortalPhysicsSpeedCapLifecyclePHY4.md)。

## 自动验证

- UE 5.8 捆绑 .NET 项目文件生成：PASS，`Saved/Logs/PortalPhysicsPHY4GravityProjectFiles.log`。
- Development Editor 最终编译：PASS，`Saved/Logs/PortalPhysicsPHY4GravityFinalBuild.log`。
- `SlayTheSpireDemo.Interior.Portals.PhysicsSolverClearance` 最终完整受影响运行：**17/17 PASS，0 失败，0 测试警告**，2026-09-28 12:49 UTC；`Saved/AutomationReports/PortalPhysicsPHY4GravityVerified/index.json` 与 `Saved/Logs/PortalPhysicsPHY4GravityVerifiedAutomation.log`。
- 高重力近地板测试在单步及双子步均于首次 PreIntegrate 得到 `SourceBlocked`，没有禁用门墙支撑碰撞或发布传送事实。**勘误：当时方块本身关闭重力，故“高重力触地”的说法不能由该 17/17 运行支持。**后续[连续重力增量](InteriorPortalPhysicsContinuousGravityPHY4.md)显式开启重力、断言实际向下积分并重新验证该拒绝。原有过滤通行、远处活跃刚体、睡眠接触岛、运动学及转移生命周期回归同批通过。原夹具中的“过滤后丢失 PT 直写上限”应按上文修正解释。
- 先前诊断失败和修复中两次失败保留为诊断证据，不能并入最终通过数。地图 SHA256 保持 `0436F682DCD867E43ECEA81E89D4916D8B8ECFB2E66CA9321997BC97038BE25C`。

当前代码只在编辑器 TaskGraph 测试夹具中挂接新许可器。此增量没有实际关卡的生产 adapter、真实方块穿门 PIE 或人工视觉验收，因此不宣称用户报告的物理抽动/穿门显示缺陷已关闭。真实重力取消和自由穿门的有效证据见后续连续重力增量；生产原子切换和 Core 门槛仍待完成。
