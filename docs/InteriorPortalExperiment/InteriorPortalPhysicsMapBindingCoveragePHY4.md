# PHY-4 — 实际地图绑定覆盖与缩放坐标域

状态：**地图绑定准备及原生 proxy 注册已验证；世界许可和生产物理切换仍开放**（2026-09-29）。前置提交 `067215d`。

在 `/Game/House/L_Interior_LivingKitchen` 的只读 PIE 副本中，两个门均已放置且关联不同的 `UStaticMeshComponent` 支撑。两端孔径均为 `(65,115)`；14 个可放置表面中的目标两墙碰撞几何可提取，唯一已注册物理方块的几何也可提取。最初 `PreparePhysicsBinding_GameThread` 对该方块返回 `UnsupportedSupport`，并非墙体碰撞不受支持：入口墙缩放 `(2.2,0.12,3)`、出口墙缩放 `(0.2,15,7)`，静态几何已将这些缩放烘入顶点；准备步骤却把单位缩放的原生粒子位姿交给 `EvaluatePose`，后者按几何契约正确返回 `ScaleChanged`。这使缩放墙体在 GT 被误拒。

修正明确区分两个域：GT 几何验证使用完整组件变换，使其缩放与烘焙几何一致；交给 Chaos verifier 的 `SupportPose` 继续保持单位缩放，因为它配合已缩放的原生碰撞 bounds。没有改地图尺寸或放宽碰撞许可。修正后，入口墙 normal span 为 `[-12,0]`，出口墙为 `[-20,0]`，该方块的准备结果为 `Ready`，并能在真实 PIE 世界中向 Chaos 注册只读观察回调（`nativeBound=1`）。这证明**可建立绑定**，不等同于物理子步许可、成功穿越或取消恢复。

专用 `SlayTheSpireDemo.Interior.Portals.MapBindingCoverage` 自动化必须以目标地图启动编辑器；它只操作 PIE 副本并断言至少一个已注册 traveller 能完成准备及原生绑定，不保存地图。最终 UE 5.8 工程生成与 Development Editor 编译 PASS；目标地图自动化 **1/1 PASS、0 警告**（2026-09-29 05:46 UTC），通用 `SlayTheSpireDemo.Interior.Portals.Physics` **46/46 PASS、0 失败；1 项既有 foundation 测试带 2 条警告**（05:46 UTC）。证据：`Saved/Logs/PortalPhysicsPHY4MapCoverageProjectFiles.log`、`Saved/Logs/PortalPhysicsPHY4MapCoverageBuildFinal.log`、`Saved/AutomationReports/PortalPhysicsPHY4MapCoverageFinal2/index.json`、`Saved/AutomationReports/PortalPhysicsPHY4MapCoveragePhysicsFinal2/index.json`。中间一次通用前缀运行失败，是专用地图测试当时位于该前缀下、却未打开目标地图；测试现已移到独立前缀，两项最终报告均通过。地图 SHA-256 保持 `0436F682DCD867E43ECEA81E89D4916D8B8ECFB2E66CA9321997BC97038BE25C`。

下一步是让实际世界 provider 对该地图的障碍、接触和支撑关系给出每个物理子步的许可或保守拒绝，并检查缩放墙的 native bounds 与准备快照一致。之后再做 held/free 输入、事实确认与关闭排空；不在本阶段启用第二写入者。视觉 Core PIE 仍留在一次性生产切换之后。
