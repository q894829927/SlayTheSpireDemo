# 小屋彩绘玻璃

蓝图：`/Game/House/StainedGlass/Blueprint/BP_StainedGlassWindow`。

地图：`/Game/House/L_Interior_LivingKitchen`。四个 `SG_Window_Glass_*` 实例位于原小屋北墙，Outliner 文件夹为 `House/StainedGlass`。保留原窗框，只替换原四块玻璃；没有向东侧洋馆放置实例。

## 调整方法

选中实例，在 Details 搜索下列参数：

| 参数 | 用途 | 默认值 |
| --- | --- | --- |
| GlassBrightness | 玻璃自发光，按当前地图曝光调节 | 4 |
| Opacity | 玻璃视觉密度 | 0.72 |
| ColorFade | 褪色程度，0 到 1 | 0.15 |
| DirtAmount | 边缘污垢与粗糙度 | 0.35 |
| CrackAmount | 细裂纹对粗糙度的影响 | 0.18 |
| NormalStrength | 手工玻璃微波纹 | 0.15 |
| Roughness | 基础粗糙度 | 0.22 |
| ProjectionIntensity | 白天投影流明 | 3500 |
| NightProjectionIntensity | 夜晚投影流明 | 500 |
| ProjectionScale | Light Function 缩放 | (1,1,1) |
| DayNightController | 地图现有昼夜控制器引用 | 已给四个实例赋值 |

蓝图 Construction Script 为每个实例创建玻璃动态材质并应用参数。Event Graph 每 0.25 秒读取显式指定的昼夜控制器，更新投影强度；不查找全场景 Actor、不在 Tick 创建材质。未指定控制器时维持 Construction Script 中的投影强度。

组件中的 `ProjectionSpotLight` 可以调整角度、位置和锥角。窗口局部 X 为宽度、Z 为高度，向局部 -Y 投光。基础尺寸 240 × 169 cm，现有四个实例按窗洞宽度适配。保留原来的全局月光作为基础照明，蓝图中的聚光灯只负责彩色投影，并开启阴影以限制穿墙漏光。

## 素材与实现范围

- `Texture/T_SG_Rose_Color`：原创玫瑰与鸢尾彩窗图案，原图保存在 `Content/House/StainedGlass/SourceArt`。
- `Material/M_SG_Glass_Master`：双面 Thin Translucent、Surface Forward Shading。
- `Material/MI_SG_Rose`：基础材质实例；实例上的标量值由蓝图参数覆盖。
- `Material/M_SG_Lead`：不透明旧铅材质。边框和中央圆饰有实际几何厚度；细部花纹铅线由纹理表现。
- `Material/M_LF_StainedGlass`：同一原画经 gamma/亮度处理后用于投影。

本版采用程序化污垢、细裂纹与微波纹，没有另行生成打包 Mask 和 Normal 图片。裂纹改变表面粗糙度，不代表玻璃实际破洞。附件中的时代切换、诅咒剧情和碎玻璃替换未纳入本次小屋版本。

项目 Rendering 设置启用 RGB Light Function Atlas（`r.LightFunctionAtlas.Format=1`）、Deferred 与 Volumetric Fog 的 Atlas 支持。RGB 格式设置会触发编辑器重启后的着色器重编译。投影材质只有 UV/纹理/颜色运算，显式标记 Atlas Compatible，避免 Custom 表达式被保守判定后退回灰度路径。

聚光灯位于窗中心上方 65 cm、向室内偏移 14 cm，俯角 25°、外锥角 28°，用于绕过客厅沙发背。亮度为当前曝光下的美术设置，未使用附件中未经此地图验证的数值作为固定物理标准。透明玻璃本身不会自动产生此彩色投影；图案投射由独立聚光灯实现。

## 验证

蓝图严格编译通过，材质和地图已保存；重启确认 RGB Atlas CVar 为 1。实际地图检查确认四个实例、每个 37 个网格组件和一盏聚光灯，全部位于小屋，原玻璃已替换。

PIE 中验证四个实例的白天/夜晚投影强度及动态材质亮度。视觉检查确认彩窗适配窗洞，并在关闭室内灯的夜景中看到红蓝金色投影落在茶几、地毯和墙面。测试关闭灯只发生在 PIE，退出后编辑器场景的原开关状态恢复。证据：`Saved/SGPlacement.json`、`Saved/SGFinalChecks.json`、`Saved/SGNightGame.png`。没有修改 C++，未执行原生编译或打包验证。
