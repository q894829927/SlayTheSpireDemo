# LT-3：Lumen 光路消费者接入边界

日期：2026-09-28。状态：**源码级接入范围已核对；未修改引擎；生产受光未接通**。
本文件是 LT-2 场景连接发布之后的实施契约，不把 opaque GPU 探针当作生产光照。
用户本轮选择**暂不修改引擎**；下列 Engine 接入步骤暂停，不执行修改或迁移。
当前安装版 `E:/Unreal engine/UE_5.8/Engine/Build/InstalledBuild.txt` 存在；项目编译
不会重建 Renderer 模块。修改下面的 Engine Shader/C++ 必须在可构建的 UE 5.8 源码
工作区中完成，并按[总方案](InteriorPortalLightTransport.md)的独立授权边界进行。
本机另有 `E:/Unreal engine/UnrealEngine` 源码树，但其 `Build.version` 是 **5.6.1**；
不能用它编译当前 5.8.1 项目或当作 5.8 的修改目标。

## 已核对的真实数据流

本机 UE 5.8 的 `Engine/Shaders/Private/Lumen/LumenHardwareRayTracingCommon.ush`：

1. `TraceLumenMinimalRay` 有 inline RayQuery 和 RayGen 两支。默认路径可能使用
   `RAY_FLAG_FORCE_OPAQUE`；原有 AnyHit 还处理阴影、lighting channel、前后面和
   自相交。项目探针强制 non-opaque 后扣除支撑的做法不能直接替换这里的材质语义。
2. `SampleLumenMinimalRayHit` 用**原始** `Ray.Origin + Ray.Direction * HitT` 还原命中
   位置，再按命中 `SceneInstanceIndex` 取 MeshCardsIndex/Surface Cache。
   `CreateRayTracedLightingResult` 只保存一个 `TraceHitDistance`，目前没有末段射线。
3. `LumenScreenProbeHardwareRayTracing.usf` 再次用原始射线和 `TraceHitDistance` 算
   命中运动，并把同一距离交给 sky leaking、雾效和 probe depth。
   `LumenRadianceCacheHardwareRayTracing.usf` 也把它写入 probe depth，并用它决定
   继续追踪的起点。只修 AnyHit 会使这些消费者读到错误空间的命中。
4. `LumenSceneDirectLightingHardwareRayTracing.usf` 的 card shadow ray 由本侧
   `ToLight` 和欧氏 `LengthToLight` 构造。穿门后必须对灯的位置/方向建立正确的
   展开路径和有限段遮挡，不能拿原始 `TMax` 直接射向未变换的灯。
5. `LumenScreenProbeTracing.usf`、`LumenReflectionTracing.usf` 的 ScreenTrace/
   MeshSDF，与 `LumenHardwareRayTracingCommon.ush` 的 FarField/HitLighting 是不同
   分支。`LumenRadianceCache.cpp`、`LumenScreenProbeGather.cpp` 的历史复用条件
   目前不包含项目发布的连接代次；`LumenSceneRendering.cpp` 的全局光照传播主要
   根据太阳/天空颜色变化，不会自动因门对重连失效。

## 接口与所有权

项目继续拥有逻辑端点、支撑、旅客源/代理及 GT 封存。Renderer 应定义**通用场景
光路连接输入**，由项目适配器填充，而不是让 Engine 的 Lumen Shader 静态依赖
`Scene.InteriorPortalLighting` 这个游戏私有名字。现有 224B Scene UB 是已验证的
桥接来源；源码集成后只有一个权威场景包，项目诊断和生产 Lumen 读同一代，
不再维护两套可能分叉的端点副本。包应包含 session、连接代次、逻辑椭圆、
精确支撑 render 身份，以及可见旅客切片的源身份与版本。主/辅助/递归 Renderer
均显式消费同一帧封存包；离屏不销毁光路。无效或缺失包走原生 Lumen。

受光追踪返回值至少需要：

| 字段 | 不变量及消费者 |
|---|---|
| 末段 `Origin/Direction/TMin`、段内 `HitT` | 重建真正命中点；Surface Cache、材质与运动向量使用该点和对应实例身份 |
| 累积实际段长、剩余预算、跳数 | probe depth、锥角、衰减/PDF 和终止判断；不计两门在地图中的欧氏间距 |
| 命中实例/三角形、法线、材质 bookmark | 必须来自末段 TLAS，不能混用入口段身份或本侧 Card |
| 每段方向/长度或等价积分摘要 | sky、雾效、点光源和阴影按展开路径计算，不把折线路径伪装成原始直线 |
| session/连接代次/切片版本、完成状态 | 历史失效；预算耗尽或不支持为 unresolved，不能写作无遮挡天空/可见灯 |

同一传输核先计算最近合法门洞交点和本段最近**材质有效**命中，再比较距离。
只扣除对应支撑在合法挤出孔体内的几何；其他物体包括出口 0.1 cm 障碍始终保留。
原体与代理同源但互补切片，不能同时作为完整遮挡。门洞连接最多 4 跳，独立于
显示递归深度。未穿门时结果必须与 UE 原生追踪逐字段一致，维持禁用/空连接成本路径。

## 垂直切片和产品门槛

实施按依赖顺序进行，每一步都有故障关闭和对照读回；局部成功不宣布色差已修复。

1. **后端输入和结果类型。** Renderer 通用输入接收 LT-2 封存包；添加末段命中、
   累积光程和完成状态。先仅启用 D3D12 HWRT inline 的 surface-cache GI 路径。
   Raw/connected/孔外/背面及 0.1 cm 物体必须与 LT-1、LT-2 对齐。RayGen 分支
   不得误读新结果；尚未实现时保持原生路径并显式标记“不支持跨门”，不能写入
   伪连通缓存。
2. **命中消费者。** Screen Probe 和 Radiance Cache 的 hit position、motion、
   sky、fog、probe depth、锥角与重追起点改为读取实际路径。物理路径长度与末段
   几何位置不得共用一个 `TraceHitDistance`。Surface Cache 按末段实例取卡片，
   对无卡片或过期卡片使用原生回退，不能采样入口墙的卡片。
3. **材质、切片和直接光。** 把旅客源/代理的互补切片接入真实 alpha/AnyHit
   语义，保留 UE lighting channel、双面、自相交和透明策略。Shadow/Light
   Sampling 构造展开后的候选光路并验证每段至目标光源；不额外乘门户增益。
   不能用无视整墙或硬编码出口偏移实现。
4. **其他追踪与历史。** ScreenTrace 的候选命中若在合法门洞之后，不得抢先
   把支撑当作终点；可针对该射线转入已支持的 HWRT 路径。MeshSDF、FarField、
   RayGen/HitLighting、反射、短程遮蔽与软件阴影逐项适配或在产品配置中显式
   禁用该**跨门功能**。不能悄悄将这些分支当作正确连通，也不以全局关 GI
   或改变地图光源换取表面通过。
5. **代次失效。** topology 或切片版本变化时，Lumen Scene/Card 的关联受光、
   Screen Probe temporal history、Radiance Cache 的 probe indirection/atlas 与
   反射历史在各自拥有者边界失效；相同冻结代次在同一帧的所有族保持稳定。
   最初可对关联场景做保守失效，再以真实依赖范围缩小；离屏和相机裁剪不是
   连接代次。检查 reset 期间显存峰值、预算和旧读回退役顺序。

首个可编译里程碑只证明第 1 步的真实 Lumen HWRT 路径读回；生产验收必须完成
该配置下所有实际消费者，并在其他配置上明确功能能力。默认渲染保留原状，
候选用独立开关开启；关闭开关/清空连接恢复 UE 原路径，不能复用候选历史。
在当前“暂不修改引擎”决定下，项目侧继续增加 opaque 探针或最终颜色补偿都不能
替代这些消费者，也不能关闭已知的正常 Lumen 接缝；不以这份设计启动源码构建。

## 预期改动面与验证

预计 Engine 修改集中在 Lumen 共用 HWRT 追踪与结果结构、Screen Probe、
Radiance Cache、Card 直射/阴影、反射对应 Shader；Renderer 场景包绑定、
Lumen Scene/Probe/Cache 历史代次 C++。最终文件集随源码版实际编译确认，
不得在安装版上直接覆盖 Engine Shader 并称为生产交付。项目侧只调整
`InteriorPortalLightConnection` 的适配/切片发布，现有诊断共享同一场景包。

自动门槛：原生空连接逐字段一致；非居中/旋转、厚墙、孔外、背面、出口 0.1 cm
遮挡、源/代理 alpha、0～4 跳及 budget unresolved；主/辅助/递归同代；移动、
清除、重连、支撑替换、旅客切片变化的缓存失效；连续快速转头没有旧光照残留。
按源码引擎的构建方式编译 Engine/Editor，再运行项目规定的生成、Editor 编译和
受影响 Automation，GPU 读回比较 LT-1 数值。手动 PIE 在正常 Lumen 的
`L_Interior_LivingKitchen` 验收半穿越/撤回/完全穿越、双门、斜视、深度 1～4、
前景枪械和性能/显存；视觉未执行时标 `USER ACTION REQUIRED`。只有这一门槛通过
才能关闭生产接缝。本文本身仅经文档 diff/引用核查，不构成上述验证结果。
