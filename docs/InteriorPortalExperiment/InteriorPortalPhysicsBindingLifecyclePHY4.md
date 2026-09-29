# PHY-4 — GT 绑定代次与事实交接

状态：**值对象协议及协调器回归已验证；只读实际接线已实现，事实/确认及生产写入仍开放**（2026-09-29）。前置原生会话提交为 `d442f15`。

## 协议边界

`FPassageBindingLifecycle` 是单个 traveller 的 GT 侧绑定与事实所有者，不持有 `UObject` 或 native proxy。它为每次绑定分配进程内唯一的 binding epoch，并保存 `(body, solver, binding, pair)` 域。一个 owner 只能有一个活跃绑定或一个正在退役的绑定；在旧 PT 会话交接完成前拒绝打开新域，避免新旧会话同时获得写入机会。生产调用方仍须把取消命令送到物理边界；GT 标记退役本身不撤销 PT 许可。

旧会话退役期间仍接受旧域的已提交 `FTransferFact`，不以新配对或最新输入过滤。`ApplyFact` 用临时 cursor 验证事实，调用上层同步调和一次，仅在调和成功后提交 cursor；拒绝调和不产生确认，重播只返回 Duplicate。回调期间禁止重入修改 owner。确认携带原 body/solver/binding 身份，并且只能用于原 PT journal。

旧 PT 会话通过 `FPassageSessionHandoff` 返回旧身份、配对、最终提交修订号和剩余事实。只有事实已经连续调和、旧 PT journal 已被确认清空、回执身份与最终修订号一致，GT 才结束退役并允许新绑定。回执中只要还有事实，或修订号不连续，就继续等待/重发旧确认。销毁世界前仍须完成或显式接管这一流程；本类型不自动替代世界关闭协议。

这一步没有给 `AInteriorPortalSystem` 安装第二套动作写入。当前生产路径仍由旧 `PhysicsHandle`、关节仲裁、Recovery 等负责；本类型与原生会话的跨线程投递尚未接到实际关卡，也不宣称玩家拿取、穿越或视觉 Core 验收。

## 实际验证与下一步

UE 5.8 捆绑 .NET 工程生成和 Development Editor 编译 PASS。最终受影响 `SlayTheSpireDemo.Interior.Portals.Physics` 自动化在 2026-09-28 UTC **45/45 通过、0 失败，1 项含 2 条既有 foundation 警告**。`BindingLifecycleHandoff` 测试覆盖旧事实调和失败不确认、重播只应用一次、旧 PT journal 排空前不得换代、错误 binding 回执拒绝、同一 body/solver 新代次的 revision 1 不与旧事实混淆，以及调和回调不能重入退役。证据路径：`Saved/Logs/PortalPhysicsPHY4BindingLifecycleFinalProjectFiles.log`、`Saved/Logs/PortalPhysicsPHY4BindingLifecycleFinalBuild.log`、`Saved/AutomationReports/PortalPhysicsPHY4BindingLifecycleFinal/index.json`。本项没有实际地图 PIE；地图 SHA-256 保持 `0436F682DCD867E43ECEA81E89D4916D8B8ECFB2E66CA9321997BC97038BE25C`。

后续 [只读 GT/PT 接线](InteriorPortalPhysicsRealBindingObservePHY4.md) 已传递准备好的绑定、输入、取消和退役回执；事实/确认投递仍待生产写入阶段。随后需证明 held/free 驱动和实际世界许可的支持范围，再一次性切换旧写入。实际地图 Core 自动化与人工 PIE 仍是物理 P1 的收口门槛。
