# 当前 WBP 蓝图、配置与 UI 布局快照

快照日期：2026-08-20

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
| `WBP_BattleHUD` | 2026-08-20 16:48:32 | 69 | `EventGraph` 190 nodes |
| `WBP_BattleCard` | 2026-08-19 22:35:56 | 20 | `EventGraph` 28 nodes |
| `WBP_BattleStatus` | 2026-08-20 10:40:50 | 4 | `EventGraph` 3 nodes |
| `WBP_BattleTargetButton` | 2026-08-19 18:01:13 | 3 | `EventGraph` 12 nodes |
| `WBP_CombatantPresentation` | 2026-08-20 16:01:48 | 5 | `EventGraph` 23 nodes |
| `WBP_CombatantTooltip` | 2026-08-20 16:48:35 | 6 | `EventGraph` 3 nodes |
| `WBP_StatusTooltip` | 2026-08-20 14:53:15 | 2 | `RebuildTooltip` 10 nodes；`EventGraph` 3 nodes |
| `WBP_StatusTooltipEntry` | 2026-08-20 16:37:42 | 8 | `SetStatusView` 22 nodes；`SetAtlasVector2D` 5 nodes；`EventGraph` 3 nodes |

## 3. WBP_BattleHUD — CURRENT SAVED

### 3.1 根布局

根控件是 `CanvasPanel_54`，设计分辨率下主要 Canvas Slot 如下：

| 控件 | Anchor | Alignment | Size | Auto Size | ZOrder | 初始 Visibility |
|---|---|---|---|---:|---:|---|
| `Img_PlayerCharacter` | `(0.23, 0.69)` | `(0.5, 1.0)` | `420 × 350` | false | 0 | Hit Test Invisible |
| `Img_EnemyCharacter` | `(0.73, 0.65)` | `(0.5, 1.0)` | `600 × 350` | false | 0 | Visible |
| `PlayerPanel` | `(0.25, 0.72)` | `(0.5, 0.5)` | `280 × 80` | true | 5 | Self Hit Test Invisible |
| `EnemyPanel` | `(0.72, 0.70)` | `(0.5, 0.5)` | `280 × 80` | true | 0 | Self Hit Test Invisible |
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
| `VB_LegalTargets` | `(0.73, 0.48)` | `(0.5, 0.5)` | `350 × 350` | false | 0 | Collapsed |
| `StatusTooltip` | top-left `(0, 0)` | `(0, 0)` | `100 × 30` | false | 200 | Collapsed |
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

- `Txt_PlayerName`、`Txt_EnemyName`、`WB_PlayerStatuses`、`WB_EnemyStatuses` 已存在于 Designer。
- `Event Battle HUD View Model Changed` 尚未连接这些四个控件，因此它们目前不会随 ViewModel 刷新。
- `WBP_CombatantPresentation` 当前没有作为 Player/Enemy 子控件放入 HUD。
- `WBP_CombatantTooltip` 当前也没有放入 HUD。
- 现有独立 `Img_PlayerCharacter` 和 `Img_EnemyCharacter` 仍然是 HUD 中实际显示的角色图像。

### 3.3 当前 EventGraph 主流程

```text
Event Battle HUD View Model Changed
→ IsValid(ViewModel)
→ 重建 Hand
→ Sequence（当前已有 Then 0 ～ Then 13）
   ├── Draw / Discard / Exhaust 数量
   ├── Energy
   ├── Player HP / HP Percent / Block
   ├── Enemy HP / HP Percent / Block
   ├── Enemy Intent
   ├── Feedback
   ├── Confirm / Cancel / End Turn 可见性和可用状态
   ├── VB_LegalTargets 重建
   └── Victory / Defeat / ResolutionFaulted 终局显示
```

卡牌重建仍使用：

```text
HB_Hand.ClearChildren
→ ForEach ViewModel.HandCards
→ Create WBP_BattleCard
→ 写入 CardView / OwnerHUD
→ AddChildToHorizontalBox
```

目标重建仍使用：

```text
VB_LegalTargets.ClearChildren
→ ForEach ViewModel.LegalTargets
→ Create WBP_BattleTargetButton
→ 写入 TargetView / OwnerHUD
→ AddChildToVerticalBox
```

因此当前正式可用的敌方目标入口仍是 `VB_LegalTargets`，尚未切换成点击敌人角色本体。

### 3.4 下一步已定但尚未连接

```text
PLANNED / NOT WIRED

Sequence 增加 Then 14
→ RefreshCombatantPresentations
→ 用 Player / Enemy 的 FBattleHUDCombatantView 更新角色呈现
→ LegalTargets.PresentationId 匹配 Enemy PresentationId
→ 把 gameplay 给出的 TargetId 传给角色呈现
```

预定的玩家可见布局是：

- 名称显示在各自 HP 条下方，只在角色被检查时显示。
- Enemy 状态说明显示在敌人左侧；Player 状态说明显示在玩家右侧。
- 不使用屏幕顶部中央的通用战斗者 Tooltip。
- 角色绑定目标选择通过 PIE 后，再折叠或移除旧 `VB_LegalTargets`，避免出现两套目标入口。

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

`EventGraph` 当前只有空的 `PreConstruct`、`Construct`、`Tick`，没有状态数据输入和刷新连线。因此它目前只是一个视觉壳，尚未成为 HUD 状态图标的正式生成控件。

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

这是 HUD 当前实际使用的目标按钮路径。

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
→ RequestPrimaryInteraction()

Event Combatant Presentation Changed
→ bTargetSelectionActive AND bLegalTarget
→ Branch
   ├── true  → Border_TargetHighlight = Hit Test Invisible
   └── false → Border_TargetHighlight = Collapsed
```

`CombatantView` 当前虽然已经被 Break，但 `DisplayName`、HP、Block、Statuses 输出尚未连接到任何视觉控件。该 WBP 当前只完成角色图片、交互层和合法目标高亮。

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
→ HUD 重建 VB_LegalTargets
→ 点击 WBP_BattleTargetButton
→ OwnerHUD.SelectTarget(TargetId)

Self-target card
→ ReadyToConfirm
→ HUD Confirm
→ 使用 ViewModel 私有的 gameplay-validated Player target

No-target card
→ ReadyToConfirm
→ HUD Confirm
→ RequestPlayCard(Card, nullptr)
```

`WBP_CombatantPresentation` 的角色本体点击目标路径已经在独立控件中准备好，但由于 HUD 尚未实例化/刷新/绑定它，当前玩家还不会通过角色图片完成选敌。

## 12. 后续修改时的同步清单

每次大幅修改 WBP 后，应同步更新本文的以下部分：

1. WBP 保存时间、Designer 控件数和 Graph node 数。
2. HUD 根 Canvas 的 Anchor、Alignment、Size、ZOrder、Visibility。
3. 新增、删除或改名的 `Is Variable` 控件。
4. EventGraph 的入口、Sequence 分支和 ViewModel 字段映射。
5. 哪些资产已经实例化进 HUD，哪些仍为独立壳。
6. 旧 `VB_LegalTargets` 是否仍是正式目标入口。
7. Player/Enemy 名称和状态说明是否已经接入实际 ViewModel 数据。

下一步具体接线方案见 [Phase6UIA1CombatantInspectionSetup.md](Phase6UIA1CombatantInspectionSetup.md)。该文件描述目标方案；本文描述当前已保存状态，二者不可混用。
