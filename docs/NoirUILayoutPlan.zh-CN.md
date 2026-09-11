# Noir 战斗 UI：Need 素材与静态 WBP 布局

日期：2026-09-11。状态：**静态 WBP 已搭建、编译并保存**。

本文件记录当前能用的静态布局。它不代表卡牌运行时数据、交互流程或 PIE 已完成验收。

## 素材边界

本轮只允许使用以下源目录中的文件：

`Content/SlayTheSpireDemo/UI/noir/images/Need/`

导入后的贴图统一放在 `/Game/SlayTheSpireDemo/UI/noir/Textures/`。未使用 `images/split`、`UI/noir` 根目录或其他目录的图片。没有正式用途的 Need 图片只保留为备用，不强行塞入当前画面。

| Need 源文件 | 导入贴图 | 当前用途 |
|---|---|---|
| `透明背景上的哥特式月光峡谷.png` | `T_Noir_Backdrop_MoonCanyon` | 全屏黑底上的峡谷与月亮背景 |
| `ChatGPT_Image_2026年9月11日_12_35_22.png` | `T_Noir_PlayerIronclad` | 左侧玩家静态立绘 |
| `红眼尖喙铠甲哨卫.png` | `T_Noir_EnemyCultSentinel` | 右侧敌人静态立绘 |
| `斜角漫画对话面板.png` | `T_Noir_PlayerInfoPanel` | 左下玩家信息底板 |
| `ChatGPT Image 2026年9月11日 13_30_32.png` | `T_Noir_EnemyInfoPanel` | 右上敌人信息横板 |
| `ChatGPT Image 2026年9月11日 14_02_10.png` | `T_Noir_EnemyAttackIntentPanel` | 敌人攻击意图红色斜角面板 |
| `磨损黑曜罗盘徽记.png` | `T_Noir_Compass` | 敌人背后的罗盘装饰 |
| `黑白锐角游戏界面面板.png` | 暂未使用 | 当前没有可用的状态栏专用素材；状态格暂用无贴图黑框 |
| `复古街头风红色“TURN”标志.png` | `T_Noir_TurnRed` | 中央回合横幅 |
| `红黑潮流漫画风斜切横幅.png` | `T_Noir_EndTurnBanner` | 右下结束回合装饰 |
| `尖刺斗篷战士剪影.png` | `T_Noir_PlayerSilhouette` | 备用剪影，不再作为当前玩家主图 |
| `image-gen-1(20260910-185635).png` | `T_Noir_TurnWhite` | 结束回合组合中的白色 TURN 字样 |
| `image-gen-2(20260910-185639).png` | `T_Noir_EndWhite` | 结束回合组合中的 END 字样 |

新加入的素材已经按用途接入：`斜角漫画对话面板.png` 是玩家信息栏，`ChatGPT Image 2026年9月11日 13_30_32.png` 是敌人信息栏，`ChatGPT Image 2026年9月11日 14_02_10.png` 是敌人攻击意图面板。信息文字、血条、格挡和意图文字仍是独立 UMG 控件，方便以后接 Gameplay 的冻结视图。

## 当前 WBP

主资产：

`/Game/SlayTheSpireDemo/UI/Widgets/Noir/WBP_BattleHUD_Noir`

- 父类为 `BattleHUDSelectionWidget`，根节点为 `CanvasPanel`。
- `HB_Hand` 保留在 `Canvas_Root` 直接子级，满足现有 FanHand 初始化的父级要求。
- `OV_Backdrop`、`OV_Combatants`、`OV_GraphicFrames`、`OV_BattleInfo`、`OV_HandArea`、`OV_ActionLayer`、`OV_TooltipLayer`、`OV_PlayArea`、`OV_PresentationFX` 和 `Overlay_Terminal` 继续保留。
- 玩家展示使用 `WBP_CombatantPresentation_Noir`；敌人展示使用 `WBP_CombatantPresentation_Noir_Enemy`。两个模板都只绑定 Need 目录导入的角色贴图。
- 编辑器默认文字使用可读的静态示例值：`IRONCLAD // IRONCLAD`、`HP 64 / 80`、`BLOCK 12`、`CULT SENTINEL`、`HP 87 / 100`、`INTENT // ATTACK 18`、`3 / 3`。
- `Txt_DamagePresentation`、`Txt_Feedback` 和 `Txt_Outcome` 的默认占位内容隐藏；它们的绑定仍保留，后续事件可以再显示。

### 角色信息栏拆分

玩家信息栏已按可复用职责拆成四个静态 WBP：

```text
WBP_PlayerInfoPanel
└─ ScaleBox_Root (ScaleToFit / Both)
   └─ SizeBox_Root (520×330 基准)
      └─ Overlay_Root
         ├─ Image_BG
         ├─ CanvasPanel_Content
         │  ├─ Text_PlayerName / Image_NameSlash
         │  ├─ HorizontalBox_HPText
         │  ├─ WBP_HealthBar
         │  ├─ HorizontalBox_Block
         │  └─ WBP_StatusPanel
         │     └─ 5×2 WBP_StatusChip
WBP_HealthBar
├─ Image_Background
└─ ProgressBar_HP
WBP_EnemyInfoPanel
└─ ScaleBox_Root (ScaleToFit / Both)
   └─ SizeBox_Root (535×180 基准)
      └─ Overlay_Root
         ├─ Image_BG (T_Noir_EnemyInfoPanel)
         ├─ CanvasPanel_Content
         │  ├─ Text_EnemyName
         │  ├─ HorizontalBox_HPText
         │  └─ WBP_EnemyHealthBar
         ├─ HorizontalBox_Block (Collapsed)
         └─ WBP_StatusPanel (Collapsed)
WBP_EnemyHealthBar
├─ Image_Background
└─ ProgressBar_HP (87%)
WBP_CombatantPresentation_Noir_Enemy
└─ SizeBox_Root (440×650 基准)
   └─ Overlay_Root
      ├─ Img_EnemyCompass (440×440，居中，透明度 0.85)
      ├─ Img_Character
      └─ Btn_Interaction
WBP_StatusChip
├─ Image_BG
└─ HorizontalBox_Content
   ├─ Image_StatusIcon
   └─ Text_StackCount
```

当前静态默认尺寸为 `WBP_PlayerInfoPanel=520×330`、`WBP_StatusPanel=390×92`、`WBP_StatusChip=68×38`。`ScaleBox_Root` 使用 `ScaleToFit` 和 `Both`，内部 `SizeBox_Root` 保持 `520×330` 基准，因此主界面槽位改变宽高时会等比缩放并保留留白，不会拉伸变形。玩家信息栏根部 `SizeBox_Root` 同时设置了宽高的最小值、覆盖值和最大值，保证独立打开 WBP 时不会被背景贴图的原始期望尺寸撑大；嵌入 HUD 时由外层 ScaleBox 适配 Canvas Slot。`Image_BG` 使用 `Render Transform.Translation X=-30` 抵消 Need 底板原图左侧透明留白，使可见卡片边缘贴近面板左边界，文字与血条位置保持不变。状态面板按五列两排摆放九个普通状态格，最后一格使用 `WBP_StatusChip_Overflow` 显示 `+2`。状态格的 `Image_BG` 当前使用 UMG 无贴图 `RoundedBox` 黑框，占位不引用任何图片；后续有合适的 Need 素材时再替换。`WBP_HealthBar` 的轨道和填充也使用无贴图 `RoundedBox`，高度 18、四角半径 9（半高圆角），`ProgressBar` 使用 `Scale` 填充样式，因此两端保持半圆。血条默认显示 `64 / 80` 的 80% 红色填充；角色名、HP、Block 和状态格都还是独立控件，尚未接入运行时 Setter 或 Gameplay 数据。

HUD 中原有的 `WBP_PlayerVitals` 保留为隐藏的 Native BindWidget 兼容层，继续承载 `PB_PlayerHP`、`Txt_PlayerHP`、`Txt_PlayerBlock` 和 `WB_PlayerStatuses` 等旧绑定；可见的玩家信息栏改为新增的 `WBP_PlayerInfoPanel`，不会破坏 Native HUD 初始化。

敌人信息栏使用独立的 `WBP_EnemyInfoPanel`，内部固定 `535×180` 基准并通过 `ScaleBox` 等比适配 Canvas Slot。名称、`HP 87 / 100` 和 `WBP_EnemyHealthBar` 作为静态示例值放在敌人底板的可见区域内；`HorizontalBox_Block` 与 `WBP_StatusPanel` 当前折叠，避免在素材尚未齐全时显示错误状态资源。HUD 中旧的 `EnemyFrame`、`WBP_EnemyVitals` 只保留给 Native BindWidget 兼容，已设为隐藏；可见底板由 `WBP_EnemyInfoPanel` 提供。

## 1920×1080 静态坐标

画布按 1920×1080 设计。Canvas 的固定锚点中，`Offset Right` 和 `Offset Bottom` 表示宽度和高度；它们不是右下角坐标。

| 层 / 控件 | 左 / 上 / 宽 / 高 | ZOrder | 用途 |
|---|---:|---:|---|
| `Img_BaseBlack` + `Img_FarMoon` | 0 / 0 / 1920 / 1080 | 0 | 黑底与峡谷背景 |
| `OV_Combatants` | 0 / 0 / 1920 / 1080 | 10 | 角色展示层；玩家槽位左内边距 0、上 130，和 `Canvas_Root` 左边对齐；敌人从右侧上 160 对齐 |
| `WBP_PlayerInfoPanel` | 左下锚点；偏移 0 / -460 / 520 / 330（1920×1080 时等效位置 0 / 620 / 520 / 330） | 31 | 可见玩家角色信息栏，控件边界和 `Canvas_Root` 左边对齐，包含血条和 5×2 状态区；与底部抽/弃/消耗计数保持间距 |
| `PlayerFrame` / `WBP_PlayerVitals` | 30 / 700 / 610 / 310 | 30 | 隐藏的旧 Native BindWidget 兼容层 |
| `WBP_EnemyInfoPanel` | 右上锚点；偏移 -559 / 50 / -24 / 230（等效 535×180，右侧留 24） | 31 | 可见敌人名字、HP 与圆角血条信息栏；使用 `T_Noir_EnemyInfoPanel` |
| `EnemyFrame` / `WBP_EnemyVitals` | 原坐标保留 | 25–30 | 隐藏的旧 Native BindWidget 兼容层 |
| `WBP_CombatantPresentation_Noir_Enemy.Img_EnemyCompass` | 敌人组件内部 Overlay 居中；440×440 | 组件首子级 | 使用 `T_Noir_Compass`，绘制在敌人立绘之后，透明度 0.85 |
| `EnemyIntentLayer` | 1360 / 215 / 400 / 180 | 24 | 敌人攻击意图统一 Overlay；底图与文字面板同层级，避免分离移动 |
| `Img_EnemyIntentPanel` / `EnemyIntentPanel` | `OverlaySlot_0` / `OverlaySlot_1` | 24 | 底图填满统一层；文字面板内边距为 30 / 25 / 30 / 0，绘制在底图上方 |
| `YourTurnComposite` | 中心锚点 (0.5 / 0.25)；偏移 -300 / 0 / 600 / 400 | 74 | 中央回合提示组合；自适应居中，不再使用 100×30 的左上角占位尺寸 |
| `Img_YourTurnBackdrop` / `Img_TurnLogo_Red` / `Img_YourLogo` | `Overlay_Root` 全填充；顺序为背景 → TURN → YOUR；YOUR 上移 70、TURN 下移 75，二者缩放 0.78 | 组件内部 | 统一的 `Img_` 命名，白色 YOUR 位于红色 TURN 之上 |
| `WBP_StatusTrack_Player` | 55 / 982 / 585 / 68 | 31 | 旧 Native 状态绑定条，静态新栏使用 `WBP_StatusPanel` |
| `WBP_StatusTrack_Enemy` | 1375 / 340 / 495 / 90 | 31 | 敌人状态条 |
| `HB_Hand` | 锚点 X 0.12–0.88，底部偏移 -270 | 80 | 手牌安全区；空手时不生成占位卡 |
| `OV_HandArea` | 0 / 0 / 1920 / 1080 | 40 | 能量和抽/弃/消耗计数 |
| `EndTurnComposite` | 1520 / 820 / 400 / 260 | 46 | 右下结束回合统一 Overlay；红色底板、黑色棱形、END、TURN、SPACE 同一层级组合 |
| `Btn_EndTurn` | 1529 / 865 / 360 / 200 | 50 | 保留 Native 点击绑定，作为组合上方的低透明命中区 |
| 顶栏文字 | 左 40 / 上 24；右 1510 / 上 24 | 75 | 战斗标题与 ACT/FLOOR 示例 |

底部手牌参数保留为当前静态可读的起点：`HandCardSize=(180,250)`、`HandFanMaxHorizontalStep=130`、`HandMoveUpPixels=10`、`HandFanBaseVerticalOffset=8`、`HandFanEdgeVerticalDrop=24`。运行时 FanHand 会重新安装交互面，因此以后调手牌要同时检查这些暴露参数。

## 当前层级关系

绘制顺序为：黑底和峡谷 → 罗盘与角色 → 玩家/敌人信息板 → 血条、格挡、状态与意图 → 中央回合提示 → 手牌与底部计数 → `EndTurnComposite` 结束回合组合 → 结束回合按钮 → 终局和提示浮层。`EndTurnComposite` 内部按 `EndTurnDecoration → Img_EndBlackDiamond → Img_EndLogo → Img_TurnLogo → Txt_EndTurnHotkey` 叠放；玩家信息板内部再按 `Image_BG → CanvasPanel_Content` 分层，状态区保持透明并由状态格自己绘制底板。

装饰图片均为 `HitTestInvisible` 或不参与命中测试；角色点击仍由已有交互按钮处理。静态装配没有创建新的 Gameplay 状态，也没有把 UI 数值写回 Gameplay。

## 已执行验证

- 通过 Unreal MCP 编译并保存 `WBP_BattleHUD_Noir`、`WBP_PlayerInfoPanel`、`WBP_HealthBar`、`WBP_StatusPanel`、`WBP_StatusChip`、`WBP_StatusChip_Overflow` 以及两个 Noir 角色模板。
- 通过 `GetWidgets` 核对 HUD 可见玩家实例已经引用 `WBP_PlayerInfoPanel`，敌人实例引用 `WBP_CombatantPresentation_Noir_Enemy`；状态面板为五列两排，最后一格为 `WBP_StatusChip_Overflow`。
- 通过 `AssetTools.get_dependencies` 核对新角色信息栏的贴图依赖只来自 Need 导入贴图（玩家底板 `T_Noir_PlayerInfoPanel`）；状态格没有贴图依赖，使用 UMG 无贴图黑框；HUD 其余贴图也只来自 Need 导入贴图。
- 在 Unreal Editor Designer 中检查了 1920×1080 静态构图和独立 `WBP_StatusPanel`：背景填满画布，左右角色与信息板不溢出，玩家栏的半圆端血条、Block、两排状态格和 `+2` 溢出格均可见。
- 将玩家展示槽位的左内边距从 `55` 调整为 `0`，并将玩家信息栏 Canvas Slot 的左偏移从 `30` 调整为 `0`；Designer 中确认两者的控件边界以 `Canvas_Root` 左边为基准。
- 对玩家信息底板设置 `Image_BG` 的 `Translation X=-30`，在独立 WBP 与 HUD 中确认可见底板左边缘贴近父面板边界。
- 新建并接入 `WBP_EnemyInfoPanel` 与 `WBP_EnemyHealthBar`，使用 `T_Noir_EnemyInfoPanel` 和无贴图圆角血条；HUD 中旧敌人底板与旧敌人信息控件已隐藏，避免与新面板重影。
- 将罗盘放进 `WBP_CombatantPresentation_Noir_Enemy` 的 `Overlay_Root`，作为首个子级并居中显示，使用 Need 素材导入的 `T_Noir_Compass`；HUD 外层原 `Img_EnemyCompass` 已折叠，避免重复绘制。敌人信息栏和攻击意图仍在 HUD 上层。
- 将 `YourTurnComposite` 移到中心锚点，三个子节点统一命名为 `Img_YourTurnBackdrop`、`Img_TurnLogo_Red`、`Img_YourLogo`，并调整为“背景 → TURN → YOUR”的绘制顺序；在 Designer 中确认中央回合提示可见。
- 新建 `EndTurnComposite` Overlay，将 `EndTurnDecoration`、`Img_EndBlackDiamond`、`Img_EndLogo`、`Img_TurnLogo` 和 `Txt_EndTurnHotkey` 放到同一父级；HUD 已通过 Unreal MCP 编译、保存，并在 Designer 中确认右下角叠层构图可见。
- 保留并恢复隐藏的 `WBP_PlayerVitals` 兼容层及 `PB_PlayerHP`、`Txt_PlayerHP`、`Txt_PlayerBlock` 等 BindWidget 名称，确保 `WBP_BattleHUD_Noir` 仍可通过 Native HUD 编译。
- 将 HUD 中玩家信息栏临时拉伸为 `700×260` 做非等比检查，面板保持等比缩放并出现留白；随后恢复基准槽位 `520×330` 并保存。

本轮没有启动 PIE，也没有宣告卡牌生成、目标选择、动态状态、分辨率适配或打包验收完成；这些留到静态布局稳定后再逐项验证。
