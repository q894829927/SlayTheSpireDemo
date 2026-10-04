# 跨门光照：路径连通与后端边界

日期：2026-09-28。状态：**LT-0 定位完成；LT-1 离线参考 14/14 PASS；LT-2 安装版 opaque 可见性与场景连接发布 PASS；生产光照接缝 OPEN**。

本次用户明确要求继续处理跨门光照，授权这个独立工作项；这不改变
FullFidelity 原计划 §2.9 的历史验收范围，也不意味着物理 PHY-4/Core 已完成。
本记录中的工作以 `fd825aca848de01f120bf346badd0eab6f161bc8` 为起点。
已有地图和物理修改单独管理。

## 问题与证据

当前系统连接的是相机射线和合成后的颜色/深度。方块原体和代理处于两个世界位置；
裁切材质保留各自合法的一半。Lumen、阴影和反射仍使用原场景的可见性，没有相同的
门洞连通关系。合成结果中的一条连续面因此可能取得两套不同的间接光照。

[HDR 边界证据](InteriorPortalSceneLinearHDRBoundary.md)已经排除了确定的二次
Tonemap 错误，并在有效的物理注册、双侧 SliceEnabled=1 下确认：BaseColor 接近一致，
正常受光的接缝在主视图 Tonemap 前已经存在；仅诊断时关闭 Lumen 漫反射间接光会大幅
缩小差异。同位置主/辅助视图对照是在隐藏墙、固定 PreExposure 的受控场景中得到近似
一致，不能据此宣称未修改地图中的 Lumen 历史或可见性都正确。

这说明目前不能把所有剩余色差继续归为曝光问题；也没有证据证明每一个差异都只来自
一种 Lumen 内部缺陷。后端验证必须继续保留正常地图对照。

## 物理契约

光照描述的是辐射亮度沿可见路径的传播。理想刚性、等尺寸传送门只改变路径所在的
坐标图，不改变辐射亮度，不额外乘距离、曝光、门面积或“颜色补偿”系数。

- 每个端点使用逻辑平面、椭圆门洞、精确支撑几何身份和连接代次。物理输入和渲染
  快照冻结后再交给后端，不在渲染线程读 Actor，不按相机是否看得到门来删除光路。
- 对一条射线，先找最近的合法门洞穿越，再与该段的最近真实遮挡比较。更近的枪、
  方块、其他墙或孔外支撑必须遮挡；只有对应支撑的合法开孔体积被扣除。
- 厚墙按实际支撑体积减去门洞截面沿法线的挤出体积。不能忽略整块支撑，也不能跳过
  “出口厚度 + 半径 + 1 cm”；门洞边缘或离出口 0.1 cm 的其他物体仍必须挡光。
- 入射来自端点正半空间，朝向负法线。交点映射到另一端的逻辑平面，方向按现有
  `InteriorPortalMath::Rotation` 的刚性半转映射，沿出口正半空间继续。光路两端可互换；
  墙背面单独发起的射线不能进入。恰在逻辑平面的光照采样允许有方向的零长度连接。
- 路程只累计各段实际长度；传送门两端在地图中的欧氏间距不计入衰减。辐射亮度
  传输不采用 inverse-square；点光源采样器使用展开后的总光程、立体角/PDF 和 BRDF。
- 原体/代理是一个物体的两种表示，保留相同材质、法线的刚性映射和源身份，分别只
  提交合法切片。不能把完整原体和完整代理同时当作遮挡，也不能共享一个曝光历史。
- 光照连通的跳数与画面递归深度是不同预算。初始后端上限为 4 跳；预算耗尽标记
  unresolved，不当作无遮挡天空/可见光源。替换/清除端点使相关路径和缓存失效。

连续并不意味着强制两半颜色相等。不同局部环境、真实阴影和不同出射方向允许产生
物理上的变化；验收关注同一个连续材质面在门缝极限位置的解是否来自一致的连通光场。

## UE 5.8 可用能力与限制（实际安装源码核对）

| 已核对入口 | 实际能力 | 对当前实现的约束 |
|---|---|---|
| SceneViewExtension / ReplacingTonemapper | 取得已经受光的场景线性 HDR | 能修正颜色域，不能重新连接此前的 GI/阴影光线 |
| GlobalClippingPlane | 当前辅助视图的栅格裁剪 | 不能作为 Lumen 光线转向的实现；Lumen 追踪代码没有消费这个门洞连接 |
| GI 插件委托 | 当 DiffuseIndirectMethod=Plugin 时执行替代后端 | 不是向现有 Lumen 添加路径的回调；直接启用会改变整个场景 GI 策略 |
| Lumen HWRT / SDF / ScreenTrace | 使用各自原生可见性和缓存 | 都需要连通关系或声明不支持，不能只改最终合成器 |
| PostTLASBuild + FXRenderingUtils + SceneUniformBufferRef | 项目扩展可在原生 TLAS 建成后绑定场景缓冲并执行自有 inline RayQuery Shader | 可以在安装版上做真实可见性实验；不等于注入了 Lumen 的受光/缓存路径 |
| ISceneRenderer::GetSceneUniforms + 可扩展 SceneUniform | 项目成员在布局冻结前注册，给每个 Renderer 的实际 Scene UB 发布常量 | 已在安装版验证；显式挂载到辅助族，不能跨 RDG 图保留缓冲引用 |
| 现有远端手电筒适配 | 特定直射灯的映射和门洞 LightFunction | 不包含通用间接光；不能据此验收半穿越的 Lumen 接缝 |

源码位置：`Renderer/Private/DeferredShadingRenderer.h` 的 GI 委托，
`Renderer/Private/IndirectLightRendering.cpp` 的 Plugin 分支，
`Shaders/Private/Lumen/LumenHardwareRayTracingCommon.ush` 与
`LumenTracingCommon.ush`。本机 `Engine/Build/InstalledBuild.txt` 存在，是安装版引擎；
有源码文件不等于可以交付重新编译过的 Renderer。

## 选择的架构与执行顺序

保留 Lumen，先在**安装版的项目扩展中**验证真实 TLAS 与门洞光路。已发现公开
`PostTLASBuild_RenderThread`、`GetRayTracingSceneViewRDG`、`GetSceneUniformBufferRef`；
不能因为 GI 插件会替换 Lumen，就断言所有可见性实验必须使用源码引擎。
独立场景 Shader 能使用这些接口，但现有 Lumen Shader 尚未消费项目的连接规则。

若后续接入确实需要修改 Renderer C++，升级到可维护的源码版渲染器扩展；这条路线
仍是待决策的后备方案。本方案把引擎文件修改单列为授权边界；
[Source/AGENTS.md](../../Source/AGENTS.md) 的原文约束是插件、engine-association 和
build-setting 变更需授权，不应省略成所有 engine 变更的直接引文。
用户询问安装版能力及要求“继续”，尚未明确授权修改引擎或提供
源码目录，因此本轮只修改项目工具/C++/Shader。安装版可以重编项目/插件和 Shader；
UBT 的 RulesAssembly 对安装模块设置 bUsePrecompiled，项目编译不会重建 Renderer DLL。
不能通过删除 InstalledBuild.txt 把它当作完整源码工作区。
不复制整套私有 Shader、不硬编码地图端点、不切换全局 GI。地图/玩法接口/门尺寸不变。

官方说明：[Shader 开发](https://dev.epicgames.com/documentation/unreal-engine/shader-development-in-unreal-engine)、
[插件 Shader](https://dev.epicgames.com/documentation/unreal-engine/overview-of-shaders-in-plugins-unreal-engine)、
[源码引擎构建](https://dev.epicgames.com/documentation/unreal-engine/building-unreal-engine-from-source)。

1. **LT-0：归因。** 保留当前有效 HDR/GI 对照；已完成引擎扩展点核查。
2. **LT-1：路径参考。** 项目工具提供离线双精度参考解，覆盖有厚度的支撑开孔、近端
   遮挡、刚性映射、距离守恒、反向路径、切片和有限跳数。它是 GPU 后端的比较依据，
   不进入 Gameplay 或生产渲染，不会改变当前屏幕颜色。
3. **LT-2：后端接入验证。** 先完成项目自有 opaque HWRT 可见性实验：两端 raw/connected/
   ellipse-outside/back 共 8 条射线，精确支撑索引、逻辑门洞、近出口薄物体及停止/重启。
   与 LT-1 比较实际路径距离和遮挡身份；不改正常 GI。实验不评估任意材质 opacity、
   旅客 AnyHit、阴影、Nanite 所有模式或软件 SDF，不能被当作生产跨门光照。
   再确认项目侧场景级冻结连接缓冲与 Lumen 消费入口；确需引擎改动时单独授权升级。
4. **LT-3：受光与缓存。** 将已验证的连接用于间接光追踪和阴影/光源可见性，解决
   切片 AnyHit、支撑开孔、SurfaceCache/RadianceCache 原生遮挡与接缝处失效。只改变
   射线方向不够：命中后的受光缓存也必须来自同一连接代次。反射使用相同路径规则，
   不以颜色合成复用来代替反射光路。软件路径要实现或形成明确的产品支持配置。
5. **LT-4：生产验收。** 只有正常光照的半穿越、撤回、穿越后和双向移动都通过，才
   关闭当前缺陷；显示递归 1～4、光路预算、显存/性能、端点替换和退役独立检查。

后端资源归 Renderer 场景实例所有，不归某一层 ViewState。所有主/辅助/递归视图消费
同一代冻结场景连接；TSR 和曝光仍逐层独立。生命周期按 render queue 顺序发布/退役，
缓存键至少包括连接代次、支撑/旅客切片身份和后端配置。离屏不注销真实光路。

这是一条必要的、可验证的迁移顺序，不承诺仅添加一个 GI 回调即可解决完整 Lumen。
未接通生产受光/缓存之前，定位、参考和 GPU 可见性实验都不能关闭生产接缝。

## 验证、回退和清理

LT-1：UE 自带 Python 执行 `-B tools/test_portal_light_transport_reference.py --report
Saved/AutomationReports/PortalLightTransportReference/tests.json`，**14/14 PASS**；
`portal_light_transport_reference.py --report .../paths.json` 记录 raw 支撑命中 44 cm，
连通后出口薄物体命中 56.1 cm。测试另覆盖出口内 0.1 cm 遮挡、正反向、厚墙斜射、
零长度定向接缝、材料切片不生成封口、1e9 cm 平移和跳数 0～4。
这些是离线验证，不冒充 GPU、PIE 或物理系统验证。

下列为 2026-09-27 单帧探针的历史证据；新动态发布协议见后文。
安装版 GPU 探针按项目规定完成生成/编译；源码只使用公开 RenderCore/Renderer 接口，
没有私有头目录、依赖、EngineAssociation 或引擎修改。新增 Shader 只写诊断缓冲，不写
SceneColor/SceneDepth，不替换 Tonemap/GI；扩展仅显式启动时存在，主视图单次提交和读回。
实际 D3D12 PIE 执行 `py tools/validate_portal_light_visibility.py`：**3 个场景、每场景
8 条原生 GPU 射线读回完成，completed=true、error=null**，结束 UTC 2026-09-27 14:55:03。
准确证据：`Saved/AutomationReports/PortalLightVisibility/sequence.json`、
`Saved/Logs/PortalLightVisibilityPIE.log`。支撑 persistent indices 为 783/784；这是本次
冻结场景的身份，不能硬编码到实现或下一次运行。

| 实际场景 | 原生 GPU 结果 |
|---|---|
| baseline | raw 两端分别命中自己支撑，约 50.00025/49.99025 cm；connected 两端均穿过 1 次连接，100 cm 范围无遮挡；孔外与背面均命中各自支撑 |
| thin_exit_blocker | 出口逻辑平面前 0.1 cm 放置 0.1 cm 厚的另一物体（PIE 普通 Cube）。蓝侧连通射线命中它，光程 50.09965 cm、1 跳；橙侧 raw 与 connected 都先命中它，49.79964 cm、0 跳，三者命中身份 591 一致 |
| blocker_removed_restart | 停止观察器、还原物体和重新启动后，两端连通及支撑遮挡恢复 baseline；不是旧读回结果复用 |

探针用最远的非支撑、可见且参与 RayTracing 的普通 PIE StaticMeshActor 作为临时
遮挡物（本次 Actor_393），精确还原 Mesh、Transform 和碰撞模式；整个 PIE 丢弃，不
保存地图。没有通过忽略支撑或把出口附近的其他物体一起删除来取得 PASS。
仅评估 opaque triangle 可见性，不做任意 opacity/旅客切片 AnyHit、Nanite 所有模式、
BRDF、灯采样、Lumen 受光缓存或阴影。此前冻结端点需保持静止，移动/替换需重新冻结；
这个限制已由下述场景连接发布协议替代，历史读回不能冒充新协议验收。
旧 22/22 回归不是新探针的验收计数。初始 C++/Shader 绑定和探针脚本错误在这个完整
通过的运行前已修正；不把初始失败的 baseline-only 读回计入这次三场景通过证据。

最终同一构建 D3D12 C++ Automation：MainViewOwnership + ViewExtensionLifecycle
**2/2 PASS，测试 warnings/errors 均为 0**，UTC 14:57:18。
`Saved/AutomationReports/PortalLightVisibilityAutomation/index.json`、
`Saved/Logs/PortalLightVisibilityAutomation.log`。不与旧 22/22 或离线 14/14 相加。

### LT-2 场景连接发布协议 — 2026-09-28

`InteriorPortalLightConnection` 是渲染侧读模型。Gameplay 仍拥有端点和连接；没有新增
玩法公开接口，也没有移动地图资产或改变门尺寸。当前支持项目既有的一个显式门对。

- 一个 World 的门对共享一个连接发布者，FullFidelity 生命周期和可选观察器持有引用；
  注册表只有弱引用，不保留 Actor 所有权。主视图通过正常扩展收集取得发布者；
  辅助/递归族必须显式挂载，不能假设继承主视图扩展。
- GT 在一帧首个 Renderer 的创建边界封存逻辑端点身份、刚性帧、椭圆尺寸和支撑组件
  身份；该帧后续 Renderer 取得同一快照，帧中变化在下一帧生效。离屏不删除连接。
  非刚性、非等尺寸、自连接或无效支撑发布空连接，不沿用上一张有效门洞。
- RT 在静态 Primitive 更新后解析 persistent index，每个冻结帧只构建一份常量包；
  主/辅助/递归 Renderer 通过公开 `GetSceneUniforms().Set` 各自发布同一包。没有跨图
  保存 RDG 指针，也没有在 RT 解引用 Actor 或 Component。
- 逻辑端点身份使用不可变 ObjectKey，光路有效性由端点和支撑决定。外观 Surface 只是
  可选的候选排除身份；隐藏/退役外观代理不得断开合法连接。缺失时发布无效 Surface
  索引。清除/重连测试暴露并修正了外观与光路的耦合。
- 端点移动、替换、清除/重连和支撑/外观 render proxy 身份变化推进资源代次。普通
  遮挡物移动由本次 TLAS 的几何体现，不伪装成门拓扑变化。owner session 区分发布者
  生命周期；停止/重启观察器不能重建生产 owner 或复用旧读回。
- Scene UB 新成员为 **224 bytes 常量**，默认 Count=0、没有 RDG 资源。已有模块的
  PostConfigInit 时序在布局冻结前注册；首次改变布局会让受影响的 Shader/DDC 重新
  编译。没有改引擎、模块依赖、加载期、Association 或 BuildSettings。
- 中心存为高/低位，GPU 用 UE DoubleFloat 转入 **实际 TLAS** 的平移坐标，不能使用
  辅助相机的原点代替它。场景数据不依赖裁剪、TSR 或曝光。
- 探针从真正 Scene UB 读取数据，不能再从私有参数传入端点来绕过发布。
  `portal.CaptureLightVisibilityProbe` 选择命令之后完整的 GT 帧，避开部分排队的旧帧；
  一次请求读回该帧主/辅助族的 frame/session/generation/身份和射线。

这仅是端点连接的发布边界，**不是完整受光缓存键**：旅客切片身份/版本、材质 opacity、
光路配置及原生几何/受光缓存失效仍未接入。Scene UB 有数据不代表 Lumen 已使用它。
默认空连接不发起探针射线；正常 GI/阴影保持原路径，观察器只写诊断缓冲。

按规定生成工程及最终编辑器编译 PASS（5.41 / 19.76 s）：
`Saved/Logs/PortalLightConnection{ProjectFiles,Build}.log`。
实际 D3D12 LivingKitchen PIE：**10 个场景、30 个视图读回，completed=true、error=null**，
结束 UTC 2026-09-27 18:07:50；`Saved/AutomationReports/PortalLightConnection/sequence.json`、
`Saved/Logs/PortalLightConnectionPIE.log`。所有族的帧号、session、代次、精确索引一致，
每条射线的遮挡状态/身份/跳数一致，距离差 <0.01 cm。240 条读回槽包括清除后 8 条
显式 inactive 结果，不把它们声称为 240 次有效光路追踪。

| 场景 | 视图数 | 资源代次 | 端点数 |
|---|---:|---:|---:|
| baseline | 3 | 1 | 2 |
| offscreen | 1 | 1 | 2 |
| endpoint_moved（0.1 cm） | 3 | 2 | 2 |
| support_replaced | 3 | 3 | 2 |
| restored | 3 | 4 | 2 |
| thin_exit_blocker | 3 | 4 | 2 |
| cleared | 1 | 5 | 0 |
| relinked（外观仍隐藏） | 3 | 6 | 2 |
| recursive_depth4 | 5 | 7 | 2 |
| observer_restarted | 5 | 7 | 2 |

整个运行 owner session=1；薄遮挡仍命中 50.09965 cm，保留近出口其他物体。
support_replaced 是直接改变 PIE 绑定的身份测试，不是合法放置验收：未删除原墙，
蓝侧连通射线仍命中该墙约 50.00975 cm，橙侧约 49.99025 cm；不会继续沿用旧开孔身份。
递归场景是已验证的双门对向临时布局，5 族为主视图加 4 个辅助族；不把它等同于
4 跳间接光照。端点及支撑布局只改 PIE 实例，退出时丢弃。地图散列未变。
首次验证分别暴露错误的普通几何代次断言、半排队帧观察请求、Python 重连属性名称和
外观代理耦合；它们已修正，不计入最终完整通过证据。

D3D12 C++ Automation 按影响范围执行：FullFidelity + MainViewOwnership +
ViewExtensionLifecycle + ColorSampleExposure 的 **21 个既有测试通过，测试警告/错误 0**；
同一 23 项运行里的两项新 LightConnection 测试夹具误用了 abstract UObject，一项
失败、一项带警告，不能把这次运行写为 23/23 PASS。证据
`Saved/AutomationReports/PortalLightConnectionAutomation/index.json`、
`Saved/Logs/PortalLightConnectionAutomation.log`，UTC 18:09:45。
改用非抽象的临时 StaticMeshComponent 身份后，仅重编 Editor 测试模块（6.33 s，
`Saved/Logs/PortalLightConnectionTestFixtureBuild.log`），新 LightConnection **2/2 PASS、
测试警告/错误 0**，UTC 18:15:28；证据
`Saved/AutomationReports/PortalLightConnectionFinalAutomation/index.json`、
`Saved/Logs/PortalLightConnectionFinalAutomation.log`。Runtime DLL/Shader 未变，保留
上述完整 GPU 读回和已通过的 21 个回归，不合并成一次最终 23/23 运行。

### 下一项：真正受光消费者

[LT-3 Lumen 接入边界](InteriorPortalLumenIntegrationLT3.md)已把本机 UE 5.8 的
命中坐标、光程、Card 阴影和 Probe/Radiance 历史消费者列成可审阅接口与分阶段门槛。
这是源码核对和设计，未修改引擎，也没有改变下面的生产接缝状态。
用户已选择暂不修改引擎，因此 LT-3 Engine 接入暂停；保留 LT-2 诊断和已知缺陷，
不把项目侧探针或后处理补偿作为替代修复。

现在继续 Lumen 追踪、命中受光、SurfaceCache/RadianceCache 和阴影的消费者。
已核对 `LumenHardwareRayTracingCommon.ush` 的 `FLumenTraceRayInlineCallback::OnAnyHit` /
`TraceLumenMinimalRay`，以及 `CalculateSurfaceCacheLighting` / `SampleLumenMinimalRayHit`。
当前返回单一 Ray 的 HitT，并用原世界位置和 MeshCardsIndex 取缓存；跨门路径必须保留
末段命中坐标、法线、实例身份及累积光程，不能把映射后的 HitT 塞回原始 Ray。
ScreenTrace、SDF、FarField、RayGen/HitLighting、阴影与反射有独立分支；先列清窄配置
及不能绕过的消费者，再准备最小可审阅改动。项目 Scene UB 不能替代这些消费者。
项目 GPU 可见性入口存在，并不证明这些内部消费者已经有足够的公开接入点。
如果需要引擎 Shader 或 C++ 的改动，先给出可审阅的修改范围，按 Source/AGENTS 授权
执行。源码版是需要重编引擎 C++ 时的构建路线，不能把“安装版”三个字当作能力结论。
本轮未修改引擎，也未申请/下载源码引擎。

**USER ACTION REQUIRED**：正常光照 LivingKitchen 手持方块半穿越/撤回/完全穿越，
快速转头、左右斜视、门洞出屏和双门同屏；新 PIE 分别 Ping-Pong 0/1，递归 1～4。
需要接缝和时序画面的记录，且没有背面透墙、丢门、枪械遮挡或曝光回归。当前后端未
实现 LT-3，不应要求用户现在把已知仍存在的色差验收为通过。

回退是切换至原渲染后端并显式保留 lighting-unconnected 状态；不能把失效的光路当作
亮天空，也不能关闭全局 GI 或改材质来获得表面上的 PASS。

本轮用户明确请求清理临时文件，但再次删除约 30.09 GiB 失败全资源 GPU 导出仍被
自动审批拒绝，理由仅 `blocked by policy`。该确切目录仍保留：
`Saved/GPUDumps/SlayTheSpireDemo-WindowsEditor-2026.09.27-18.13.32`。
不使用其他工具绕过此拒绝。已清理 10 个其他一次性脚本，共 14,162 bytes，并核实
均不存在：8 个 `PreparePortal{BasePassGPU,ColorGPU,ColorPartition,Directed,LightingParity,
LightingRoot,Registered,TSRGPU}Probe.py`，以及 `SummarizePortalHDRLightingColors.py`、
`SummarizePortalSceneLinearHDR.py`。保留可信 JSON、日志、截图及必要的可复现探针。
地图 SHA256 仍为 `0436F682DCD867E43ECEA81E89D4916D8B8ECFB2E66CA9321997BC97038BE25C`。
本轮两次初始 Shader 启动失败产生的崩溃包和 Shader 调试文件，另一次文件级清理也被
自动审批拒绝（同为 `blocked by policy`，命令未执行）：
`Saved/Crashes/UECC-Windows-32889F7944AA8985A39AD9BA6B3ECF94_0000`、
`Saved/Crashes/UECC-Windows-CF0587424B623B2D121EDEB574AD5302_0000`、
`Saved/ShaderDebugInfo/PCD3D_SM6/Global/FProbeCS/0`。这些仍保留，不重试其他删除方式。
