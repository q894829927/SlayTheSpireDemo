# 当前 WBP 蓝图、配置与 UI 布局快照

快照日期：2026-08-21

## 1. 用途与边界

本文记录 `Content/SlayTheSpireDemo/UI/Widgets` 中当前已经保存到磁盘的 WBP 状态，供后续继续连接蓝图、排查回归和恢复布局时查阅。

本文中的标记含义：

```text
CURRENT SAVED
= 当前 .uasset 已保存的真实状态

PLANNED / NOT WIRED
= 已确定的下一步方案，但当前 .uasset 尚未完成
```

本快照通过 UE5.8 Python Commandlet 只读加载资产并导出 Graph、Designer 层级和 Canvas Slot 数据。没有保存或修改任何 `.uasset`，没有编译项目，也没有运行自动化测试。

`.uasset` 始终是最终事实来源；本文是便于阅读的人工快照。若编辑器中存在尚未保存的改动，它们不属于本快照。

## 2. 当前资产清单

| WBP | 保存时间 | Designer 控件数 | Graph |
|---|---:|---:|---|
| `WBP_BattleHUD` | 2026-08-21 02:27:15 | 73 | `RefreshCombatantPresentations` 73 nodes；`RebuildStatusIcons` 10 nodes；`RefreshOneCombatantPresentation` 18 nodes；`EventGraph` 232 nodes |
| `WBP_BattleCard` | 2026-08-19 22:35:56 | 20 | `EventGraph` 28 nodes |
| `WBP_BattleStatus` | 2026-08-20 19:43:40 | 4 | `SetStatusView` 18 nodes；`SetAtlasVector2D` 5 nodes；`EventGraph` 3 nodes |
| `WBP_BattleTargetButton` | 2026-08-19 18:01:13 | 3 | `EventGraph` 12 nodes |
| `WBP_CombatantPresentation` | 2026-08-21 00:55:38 | 10 | `EventGraph` 25 nodes |
| `WBP_CombatantTooltip` | 2026-08-20 16:48:35 | 6 | `EventGraph` 3 nodes |
| `WBP_StatusTooltip` | 2026-08-20 21:25:07 | 2 | `RebuildTooltip` 10 nodes；`EventGraph` 3 nodes |
| `WBP_StatusTooltipEntry` | 2026-08-20 16:37:42 | 8 | `SetStatusView` 22 nodes；`SetAtlasVector2D` 5 nodes；`EventGraph` 3 nodes |

## 3. WBP_BattleHUD — CURRENT SAVED

### 3.1 根布局

根控件是 `CanvasPanel_54`，设计分辨率下主要 Canvas Slot 如下：

| 控件 | Anchor | Alignment | Size | Auto Size | ZOrder | 初始 Visibility |
|---|---|---|---|---:|---:|---|
| `Img_PlayerCharacter`（旧） | `(0.23, 0.69)` | `(0.5, 1.0)` | `420 × 350` | false | 0 | Collapsed |
| `Img_EnemyCharacter`（旧） | `(0.73, 0.65)` | `(0.5, 1.0)` | `600 × 350` | false | 0 | Collapsed |
| `Combatant_PlayerPresentation` | `(0.23, 0.69)` | `(0.5, 1.0)` | `420 × 365` | false | 1 | Self Hit Test Invisible |
| `Combatant_EnemyPresentation` | `(0.73, 0.65)` | `(0.5, 1.0)` | `650 × 350` | false | 1 | Self Hit Test Invisible |
| `PlayerPanel` | `(0.25, 0.72)` | `(0.5, 0.5)` | `280 × 80` | false | 5 | Self Hit Test Invisible |
| `EnemyPanel` | `(0.72, 0.70)` | `(0.5, 0.5)` | `280 × 80` | false | 5 | Self Hit Test Invisible |
| `EnemyIntentPanel` | `(0.74, 0.27)` | `(0.5, 0.5)` | `70 × 80` | false | 0 | Self Hit Test Invisible |
| `EnergyPanel` | `(0.075, 0.82)` | `(0.5, 0.5)` | `250 × 250` | false | 0 | Self Hit Test Invisible |
| `HB_Hand` | `(0.5, 1.0)` | `(0.5, 1.0)` | `760 × 210` | false | 10 | Self Hit Test Invisible |
| `DrawPilePanel` | `(0.035, 0.94)` | `(0.5, 0.5)` | `70 × 80` | false | 0 | Self Hit Test Invisible |
| `DiscardPilePanel` | `(0.965, 0.94)` | `(0.5, 0.5)` | `70 × 80` | false | 0 | Self Hit Test Invisible |
| `ExhaustPanel` | `(0.965, 0.78)` | `(0.0, 0.0)` | `55 × 55` | false | 0 | Self Hit Test Invisible |
| `Btn_EndTurn` | `(0.90, 0.82)` | `(0.5, 0.5)` | `150 × 55` | false | 0 | Visible |
| `Btn_Confirm` | `(0.46, 0.72)` | `(0.5, 0.5)` | `130 × 45` | false | 0 | Collapsed |
| `Btn_Cancel` | `(0.54, 0.72)` | `(0.5, 0.5)` | `130 × 45` | false | 0 | Collapsed |
| `Txt_Feedback` | `(0.5, 0.53)` | `(0.5, 0.5)` | `400 × 50` | false | 20 | Visible |
| `StatusTooltip_Player` | `(0.3396, 0.5)` | `(0.0, 1.0)` | Auto Size | true | 200 | Collapsed |
| `StatusTooltip_Enemy` | `(0.56, 0.48)` | `(1.0, 0.5)` | Auto Size | true | 200 | Collapsed |
| `Overlay_Terminal` | stretch `(0,0) → (1,1)` | `(0,0)` | fill | false | 100 | Collapsed |

所有表中 Position Offset 当前均为 `(0, 0)`。

### 3.2 玩家与敌人面板层级

```text
PlayerPanel : VerticalBox
├── SB_PlayerVitals
│   └── OV_PlayerVitals
│       ├── SB_PlayerHPArea
│       │   └── OV_PlayerHP
│       │       ├── PB_PlayerHP
│       │       └── Txt_PlayerHP
│       └── SB_PlayerBlockBadge
│           └── OV_PlayerBlock
│               ├── Img_PlayerBlock
│               └── Txt_PlayerBlock
├── Txt_PlayerName                 Hidden
└── WB_PlayerStatuses              Self Hit Test Invisible

EnemyPanel : VerticalBox
├── SB_EnemyVitals
│   └── OV_EnemyVitals
│       ├── SB_EnemyHPArea
│       │   └── OV_EnemyHP
│       │       ├── PB_EnemyHP
│       │       └── Txt_EnemyHP
│       └── SB_EnemyBlockBadge
│           └── OV_EnemyBlock
│               ├── Img_EnemyBlock
│               └── Txt_EnemyBlock
├── Txt_EnemyName                  Hidden
└── WB_EnemyStatuses               Self Hit Test Invisible
```

当前状态：

- `Combatant_PlayerPresentation`、`Combatant_EnemyPresentation` 已作为 HUD 的正式角色呈现实例放入根 Canvas。
- 旧 `Img_PlayerCharacter`、`Img_EnemyCharacter` 仍保留在 Designer，但均为 `Collapsed`，不会形成第二套角色图像或命中层。
- `Txt_PlayerName`、`Txt_EnemyName` 默认 `Hidden`；角色 Hover 的 `OnInspectRequested` 会写入当前 `CombatantView.DisplayName` 并显示，`OnInspectCleared` 会隐藏。
- `WB_PlayerStatuses`、`WB_EnemyStatuses` 由 `RefreshCombatantPresentations` 末尾的两次 `RebuildStatusIcons` 刷新。
- `StatusTooltip_Player`、`StatusTooltip_Enemy` 默认 `Collapsed`；Hover 检查时按当前状态数组重建并显示，离开时隐藏。
- `WBP_CombatantTooltip` 仍未实例化进 HUD；当前名称与状态详情使用角色附近的独立控件。

### 3.3 当前 EventGraph 主流程

```text
Event Battle HUD View Model Changed
→ IsValid(ViewModel)
→ 重建 Hand
→ Sequence（当前已有 Then 0 ～ Then 14）
   ├── Draw / Discard / Exhaust 数量
   ├── Energy
   ├── Player HP / HP Percent / Block
   ├── Enemy HP / HP Percent / Block
   ├── Enemy Intent
   ├── Feedback
   ├── Confirm / Cancel / End Turn 可见性和可用状态
   ├── Victory / Defeat / ResolutionFaulted 终局显示
   └── Then 14 → RefreshCombatantPresentations
```

卡牌重建仍使用：

```text
HB_Hand.ClearChildren
→ ForEach ViewModel.HandCards
→ Create WBP_BattleCard
→ 写入 CardView / OwnerHUD
→ AddChildToHorizontalBox
```

正式角色目标刷新使用：

```text
RefreshCombatantPresentations
→ RefreshOneCombatantPresentation(PlayerPresentation, ViewModel.Player)
→ RefreshOneCombatantPresentation(EnemyPresentation, ViewModel.Enemy)
→ RebuildStatusIcons(ViewModel.Player.Statuses, WB_PlayerStatuses)
→ RebuildStatusIcons(ViewModel.Enemy.Statuses, WB_EnemyStatuses)
```

`VB_LegalTargets` 已从 Designer 删除，Sequence `Then 12` 当前没有连接。正式目标入口已切换为点击 Player / Enemy 的 `WBP_CombatantPresentation`。

### 3.4 RefreshOneCombatantPresentation — CURRENT SAVED

```text
Input:
    PresentationWidget
    CombatantView

IsValid(ViewModel)
→ IsValid(PresentationWidget)
→ CombatantView.PresentationId
→ ViewModel.TryGetLegalTargetByPresentationId(PresentationId)
   ├── Found
   └── OutTarget.TargetId

ViewModel.InteractionState == ChoosingTarget
→ bChoosingTarget

SetPresentationData(
    Target                   = PresentationWidget,
    InCombatantView          = CombatantView,
    bInTargetSelectionActive = bChoosingTarget,
    bInLegalTarget           = Found,
    InTargetId               = OutTarget.TargetId,
    bInTargetHighlighted     = bChoosingTarget AND Found
)
```

该函数只按 `PresentationId` 把 Gameplay 已发布的 `TargetId` 映射到对应角色，不自行决定合法性。`SelectTarget` 仍会在正式 Request 路径中做权威复验。

两个角色的目标请求连接均已保存：

```text
Combatant_PlayerPresentation.OnTargetRequested(TargetId)
→ WBP_BattleHUD.SelectTarget(TargetId)

Combatant_EnemyPresentation.OnTargetRequested(TargetId)
→ WBP_BattleHUD.SelectTarget(TargetId)
```

当前图中还有两组断开的历史节点：

1. `EventGraph` 中旧 `ClearChildren → ForEach LegalTargets → Create WBP_BattleTargetButton`，其入口 `ClearChildren.execute` 未连接。
2. `RefreshCombatantPresentations` 中旧的 Player/Enemy 手动遍历与临时变量映射，Function Entry 已改接新 helper，因此旧组没有执行入口。

它们不会运行，也不会影响当前目标选择。后续可在蓝图编辑器中框选删除以降低维护噪音；不要把 Sequence `Then 12` 重新接回旧路径。

## 4. WBP_BattleCard — CURRENT SAVED

### 4.1 Designer

```text
SB_Card
└── Btn_Card
    └── OV_Card
        ├── BG_Card
        ├── VB_CardContent
        │   ├── Txt_CardName
        │   ├── SB_CardArt
        │   │   └── Img_CardArt
        │   └── Txt_CardType
        ├── SB_Description
        │   └── Txt_CardDescription
        └── SB_Cost
            └── OV_Cost
                ├── Img_CostGlow
                ├── Img_CostOuter
                ├── Img_CostRing
                ├── Img_CostBase
                ├── Img_CostSwirl
                ├── Img_CostBG
                └── Txt_Cost
```

蓝图成员：

```text
CardView : FBattleHUDCardView
OwnerHUD : WBP_BattleHUD reference
```

### 4.2 EventGraph

```text
PreConstruct
→ Break CardView
→ Sequence
   ├── DisplayName → Txt_CardName
   ├── Cost → ToText → Txt_Cost
   ├── Description → Txt_CardDescription
   ├── CardType → Switch ECardType → Txt_CardType
   └── CardArt → Img_CardArt.SetBrushFromTexture

Btn_Card.OnClicked
→ OwnerHUD.SelectCard(CardView.RuntimeId)
```

动态伤害、格挡和状态数值已经通过 `CardView.Description` 的最终 `FText` 进入控件；WBP 不自行计算 Gameplay 数值。

## 5. WBP_BattleStatus — CURRENT SAVED

Designer：

```text
SB_Status
└── OV_Status
    ├── Img_StatusIcon
    └── Txt_StatusAmount
```

`SetStatusView` 已连接 `Amount` 文本和 Atlas 图标参数；HUD 的 `RebuildStatusIcons` 会为 Player / Enemy 当前状态创建该控件。它已是 HUD 状态小图标的正式生成控件。

## 6. WBP_BattleTargetButton — CURRENT SAVED

Designer：

```text
SB_TargetButton
└── Btn_Target
    └── Txt_TargetName
```

蓝图成员：

```text
TargetView : FBattleHUDTargetView
OwnerHUD   : WBP_BattleHUD reference
```

EventGraph：

```text
PreConstruct
→ Break TargetView
→ DisplayName → Txt_TargetName

Btn_Target.OnClicked
→ OwnerHUD.SelectTarget(TargetView.TargetId)
```

该资产仍保留，但 HUD 当前不再实例化它。它是旧 `VB_LegalTargets` 路径的遗留资产，不是当前正式目标入口。

## 7. WBP_CombatantPresentation — CURRENT SAVED

父类：`UBattleHUDCombatantPresentationWidgetBase`

蓝图成员：

```text
CharacterTexture
```

### 7.1 Designer

```text
SizeBox_Root                    Self Hit Test Invisible
└── Overlay_Root               Self Hit Test Invisible
    ├── Img_Character          Hit Test Invisible，Fill
    ├── Btn_Interaction        Visible，Fill，Focusable interaction layer
    └── Border_TargetHighlight Collapsed，顶层视觉高亮
        └── Canvas_TargetCorners
            ├── Corner_TL      44 × 44
            ├── Corner_TR      44 × 44
            ├── Corner_BL      44 × 44
            └── Corner_BR      44 × 44
```

`Border_TargetHighlight` 的正确输入规则是：

```text
默认 Visibility = Collapsed
显示时 Visibility = Hit Test Invisible
不能截获 Btn_Interaction 的鼠标或焦点输入
```

需要在 Designer 中保持可供 Graph 引用的控件：

```text
Img_Character
Btn_Interaction
Border_TargetHighlight
```

如果重建控件或丢失引用，必须勾选 `Is Variable` 后再连接 Graph。只设置 Visibility 不能替代 `Is Variable`。

### 7.2 EventGraph

```text
PreConstruct
→ Img_Character.SetBrushFromTexture(CharacterTexture, MatchSize = false)

Btn_Interaction.OnHovered
→ SetPointerInspectionActive(true)

Btn_Interaction.OnUnhovered
→ SetPointerInspectionActive(false)

Btn_Interaction.OnClicked
→ bTargetSelectionActive AND bLegalTarget
→ true: RequestPrimaryInteraction()
→ false: no-op

Event Combatant Presentation Changed
→ bTargetHighlighted
→ Branch
   ├── true  → Border_TargetHighlight = Hit Test Invisible
   └── false → Border_TargetHighlight = Collapsed
```

角色名称、HP、Block 与状态列表仍由 `WBP_BattleHUD` 周边控件显示；该 WBP 负责角色图片、Hover/Focus 检查、目标点击和四角高亮。

## 8. WBP_CombatantTooltip — CURRENT SAVED BUT UNUSED

Designer：

```text
Border_Root
└── VB_Root
    ├── Txt_CombatantName
    ├── Txt_CombatantHP
    ├── Txt_CombatantBlock
    └── WBP_StatusTooltip
```

当前 `EventGraph` 只有空的 `PreConstruct`、`Construct`、`Tick`：

- 没有 `SetCombatantView` 函数。
- 没有 Name / HP / Block / Statuses 的赋值连线。
- 没有被 `WBP_BattleHUD` 实例化。

该资产目前是未接入的通用 Tooltip 壳。按照当前仿《杀戮尖塔》的 UI 决定，近期不把它作为屏幕中央/顶部的大面板；名称和状态详情优先绑定在对应角色附近。资产先保留，以后若需要固定检查、手柄焦点详情或开发工具再复用。

## 9. WBP_StatusTooltip — CURRENT SAVED

Designer：

```text
SB_Tooltip
└── VB_StatusEntries
```

`VB_StatusEntries` 必须保持可供 Graph 引用；若重建控件，要勾选 `Is Variable`。

`RebuildTooltip(Statuses)`：

```text
VB_StatusEntries.ClearChildren
→ ForEach Statuses
→ Create WBP_StatusTooltipEntry（Owning Player = GetOwningPlayer）
→ Entry.SetStatusView(Array Element)
→ VB_StatusEntries.AddChildToVerticalBox
→ 新 Slot Padding.Bottom = 6
```

普通 `EventGraph` 的 `PreConstruct`、`Construct`、`Tick` 当前为空。

## 10. WBP_StatusTooltipEntry — CURRENT SAVED

### 10.1 Designer

```text
Border_Entry
└── VerticalBox_38
    ├── HB_Title
    │   ├── SB_StatusIcon
    │   │   └── Img_StatusIcon
    │   ├── Txt_StatusTitle
    │   └── Txt_StatusAmount
    └── Txt_StatusDescription
```

蓝图成员：

```text
StatusView     : FBattleHUDStatusView
MID_StatusIcon : MaterialInstanceDynamic reference
```

以下控件必须保持 `Is Variable`，因为函数图直接引用：

```text
Img_StatusIcon
Txt_StatusTitle
Txt_StatusAmount
Txt_StatusDescription
```

### 10.2 SetStatusView

```text
SetStatusView(View)
→ 保存 StatusView
→ Break FBattleHUDStatusView
→ DisplayName → Txt_StatusTitle.SetText
→ Description → Txt_StatusDescription.SetText
→ Amount → ToText → Txt_StatusAmount.SetText
→ Branch bUseAtlasIcon
   ├── false
   │   └── Img_StatusIcon.Visibility = Collapsed
   └── true
       ├── Img_StatusIcon.Visibility = Visible
       ├── Img_StatusIcon.GetDynamicMaterial
       ├── 保存 MID_StatusIcon
       ├── SetAtlasVector2D("UVOffset", UVOffset)
       ├── SetAtlasVector2D("UVScale", UVScale)
       ├── SetAtlasVector2D("TrimOffset", TrimOffset)
       └── SetAtlasVector2D("TrimScale", TrimScale)
```

### 10.3 SetAtlasVector2D

```text
Input ParameterName + Vector2D Value
→ BreakVector2D
→ MakeLinearColor(R = X, G = Y)
→ MID_StatusIcon.SetVectorParameterValue(ParameterName, LinearColor)
```

状态 Tooltip 只显示 ViewModel 已生成的最终 `Description`，不在 WBP 中重新计算 Weak、Vulnerable、Frailty 等规则百分比。

## 11. 当前玩家可见交互与目标状态

```text
Enemy-target card
→ 选牌
→ ViewModel 进入 ChoosingTarget，并发布 Enemy legal target
→ RefreshOneCombatantPresentation 按 PresentationId 匹配 Enemy
→ Enemy 显示四角高亮
→ 点击 Enemy
→ Combatant_EnemyPresentation.OnTargetRequested(TargetId)
→ HUD.SelectTarget(TargetId)

Self-target card
→ ViewModel 进入 ChoosingTarget，并发布 Player legal target
→ RefreshOneCombatantPresentation 按 PresentationId 匹配 Player
→ Player 显示四角高亮
→ 点击 Player
→ Combatant_PlayerPresentation.OnTargetRequested(TargetId)
→ HUD.SelectTarget(TargetId)

No-target card
→ ReadyToConfirm
→ HUD Confirm
→ RequestPlayCard(Card, nullptr)
```

`None` 仍使用确认按钮；`Self` 与 `Enemy` 均使用角色本体选择。HUD 不硬编码 Player/Enemy 的 `TargetId`，只使用 ViewModel 当前 public legal set 中的映射结果。

本次只读结构检查确认保存资产上的执行线和数据线完整，但按用户约束没有编译 Blueprint、运行 PIE 或自动化。因此本文记录为“连线结构正确”，不替代运行时 PIE 验证。

## 12. 后续修改时的同步清单

每次大幅修改 WBP 后，应同步更新本文的以下部分：

1. WBP 保存时间、Designer 控件数和 Graph node 数。
2. HUD 根 Canvas 的 Anchor、Alignment、Size、ZOrder、Visibility。
3. 新增、删除或改名的 `Is Variable` 控件。
4. EventGraph 的入口、Sequence 分支和 ViewModel 字段映射。
5. 哪些资产已经实例化进 HUD，哪些仍为独立壳。
6. 旧 `VB_LegalTargets` 是否仍是正式目标入口（当前：否；Designer 已删除，旧节点入口已断开）。
7. Player/Enemy 名称和状态说明是否已经接入实际 ViewModel 数据。

下一步具体接线方案见 [Phase6UIA1CombatantInspectionSetup.md](Phase6UIA1CombatantInspectionSetup.md)。该文件描述目标方案；本文描述当前已保存状态，二者不可混用。
