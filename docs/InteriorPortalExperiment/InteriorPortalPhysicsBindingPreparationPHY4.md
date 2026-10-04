# PHY-4 — 实际组件的物理绑定准备

状态：**GT 绑定准备与原生夹具接入已验证；生产 adapter / Core 仍开放**（2026-09-29）。前置物理提交为 `48b753d`；本项的提交身份以 Git 历史为准。

## 所有权与边界

`PreparePhysicsBinding_GameThread` 将已注册方块、两侧支持墙体、同一世界、配对代次和明确的门洞尺寸整理为一份不可获权的绑定快照。它要求当前刚体仍在模拟、注册几何与命令身份一致、两个支持组件具有静态碰撞几何、两侧门洞能够用同一宽高证书表达，且逻辑门平面确实穿过对应支持墙体。方块局部质心从当前 `BodyInstance` 获取；墙体碰撞几何与世界姿态分别冻结，墙厚由实际碰撞跨度推导。

输出命令中的 `IsolatedStaticScope`、`ExitCorridorCertified` 始终为 false。GT 准备成功**不授予**穿墙或传送许可，也不调用速度上限安装器或修改刚体。调用方后续必须在当前刚体上安装原生速度上限，按当前 native proxy 建立 `FChaosStaticClearance`，并由它在 PreIntegrate、PostIntegrate、接触修改及 PostSolve 持续核对真实物理世界。组件重建、墙体变化或失去碰撞后，旧物理绑定必须退役；不能复用一次 GT 查询当作子步证书。

现有 Editor Chaos 夹具已改用同一准备入口，再安装速度上限并绑定原生许可器。因此该入口真正参与先前 19 项物理回归。新增的绑定测试覆盖正常快照、两端尺寸不一致、支持墙体失去碰撞和注册后方块几何改变。旧关卡中的 `PhysicsHandle`、关节仲裁、游戏线程 Recovery 和传送写入仍是唯一玩法路径；本轮没有开启第二套生产写入。

## 实际验证

- UE 5.8 捆绑 .NET 工程生成 PASS：`Saved/Logs/PortalPhysicsPHY4BindingProjectFiles.log`。
- Development Editor 编译 PASS：`Saved/Logs/PortalPhysicsPHY4BindingBuild.log`。
- `SlayTheSpireDemo.Interior.Portals.PhysicsSolverClearance` **20/20 PASS、0 失败、0 测试警告**，2026-09-28 17:05 UTC：`Saved/AutomationReports/PortalPhysicsPHY4BindingDiagnostic/index.json`、`Saved/Logs/PortalPhysicsPHY4BindingDiagnosticAutomation.log`。
- 此次仅运行隔离的原生夹具，无实际关卡 PIE。未编辑 `/Game/House/L_Interior_LivingKitchen`。

下一步仍属于 PHY-4 slice 2：生产所有者需在 GT/物理线程之间持有绑定生命周期、对实际配对变更和 native proxy 重建执行取消/重绑，并统一提交自由/拿取驱动、子步许可、传送事实和消费确认。完成可证明的场景覆盖后，才可一次性切换掉旧写入；随后执行 Core 实际地图物理与人工 PIE 门槛。当前工作不宣称用户报告的方块抽动已经修复。
