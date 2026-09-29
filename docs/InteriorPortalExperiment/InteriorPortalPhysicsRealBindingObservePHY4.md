# PHY-4 — 真实 GT/PT 绑定观察接线

状态：**只读接线与 Chaos 回调生命周期已验证；生产物理切换未开始**（2026-09-29）。前置原生会话提交为 `d442f15`。

`AInteriorPortalSystem` 的 traveller 注册顺序拥有一组并行的 `FPortalPhysicsBindingBridge`。开关 `portal.PhysicsBindingObserve` 默认为 `0`；设为 `1` 后，系统在 `TG_PrePhysics` 用当前注册的 body、两端 support、逻辑门面、孔径和 pair generation 生成 `FPhysicsBindingRequest`。GT 准备步骤必须通过几何、COM、静态支撑和等孔径检查，才能绑定真实 Chaos proxy。未支持的墙体或几何被拒绝，不把未证实的范围解释为可通行。

桥接器为每个绑定分配 body/solver/binding/pair 域，在真实 `PreIntegrate` 回调观察子步，并把带域的观察和退役回执送回 GT。门对、proxy、支撑、几何或 COM 变化会请求取消；回调对每次 dispatch 重送取消，直到旧 PT 会话返回匹配的空 journal 和最终 revision，才可重新绑定。traveller 注销、重建初始列表和 `EndPlay` 注销回调。世界关闭时的直接注销只适用于本阶段无物理写入、无事实的观察器；生产写入路径必须实现完整的事实排空和接管。

初始接线阶段**不执行** clearance 检查、接触修改、力、速度上限、位姿设置或传送事务；旧 Gameplay 路径仍是唯一动作写入者。随后 [真实子步只读检查](InteriorPortalPhysicsNativeObservationPHY4.md) 已接入 `PreIntegrate`：它调用 verifier 并回传类型化的拒绝原因，但不消费 proof 或实施物理许可。当前回调仍不产生 transfer fact，因此事实投递/确认协议仍仅有值对象测试，不能把这一步当成 held/free 驱动或实际地图通行范围证明。开关用于诊断接线，不是物理切换开关。

UE 5.8 捆绑 .NET 工程生成与 Development Editor 编译通过。新 `PhysicsSolverClearance.RealBindingObserveLifecycle` 在真实 TaskGraph Chaos solver 中验证子步到 GT 回执、门对变化先退役后换代、取消后注销和无物理写入，**1/1 通过，无警告**。受影响的 `SlayTheSpireDemo.Interior.Portals.Physics` 为 **46/46 通过，0 失败；1 项既有 foundation 测试带 2 条警告**，报告时间 2026-09-28 18:18 UTC。证据：`Saved/Logs/PortalPhysicsPHY4RealBridgeProjectFiles.log`、`Saved/Logs/PortalPhysicsPHY4RealBridgeBuildFinal.log`、`Saved/Logs/PortalPhysicsPHY4RealBridgeFocused.log`、`Saved/Logs/PortalPhysicsPHY4RealBridgeFinal.log`、`Saved/AutomationReports/PortalPhysicsPHY4RealBridgeFinal/index.json`。用户地图未编辑，SHA-256 为 `0436F682DCD867E43ECEA81E89D4916D8B8ECFB2E66CA9321997BC97038BE25C`。本只读接线没有视觉验收门槛，也没有声称实际地图 PIE 已运行。

下一步先证明真实地图中的支撑/方块哪些能完成绑定并明确拒绝原因，然后让实际世界 provider 证明许可范围或保守拒绝。此后才实现 held/free 共同驱动、事实与确认跨线程流转、关卡关闭排空；完成这些前不得启用新写入者。最终一次性撤旧写入并进行 Core 自动化和人工 PIE。
