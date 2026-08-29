# 当前 WBP 蓝图、配置与 UI 布局快照

快照日期：2026-08-29

## 1. 用途与边界

本文记录 `Content/SlayTheSpireDemo/UI/Widgets` 中当前已经保存到磁盘的 WBP 状态，供后续继续连接蓝图、排查回归和恢复布局时查阅。

本文中的标记含义：

```text
CURRENT SAVED
= 当前 .uasset 已保存的真实状态

PLANNED / NOT WIRED
= 已确定的下一步方案，但当前 .uasset 尚未完成
```

本快照的基础结构由 UE5.8 Python Commandlet 只读加载资产并导出 Graph、Designer 层级和 Canvas Slot 数据。2026-08-29 的本次增量通过 Unreal MCP Editor 写入 `WBP_BattleHUD`，随后完成 Blueprint Compile、资产保存和浮动 PIE 验证；其余未涉及的 WBP 仍沿用原只读快照状态。

`.uasset` 始终是最终事实来源；本文是便于阅读的人工快照。若编辑器中存在尚未保存的改动，它们不属于本快照。

## 2. 当前资产清单

| WBP | 保存时间 | Designer 控件数 | Graph |
|---|---:|---:|---|
| `WBP_BattleHUD` | 2026-08-29 20:22:26 | 75 | `RefreshCombatantPresentations` 73 nodes；`RebuildStatusIcons` 10 nodes；`RefreshOneCombatantPresentation` 18 nodes；`BeginPresentationRecordPlayback` 63 nodes；`EventGraph` 363 nodes |
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
| `OV_PlayArea` | `(0.5, 0.61)` | `(0.5, 0.5)` | `200 × 260` | false | 0 | Hit Test Invisible |
| `Txt_DamagePresentation` | `(0.5, 0.45)` | `(0.5, 0.5)` | `200 × 70` | false | 30 | Collapsed |
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

### 3.5 UI-A2E committed-presentation 连线 — CURRENT SAVED / PARTIAL

当前保存的资产已经把 `BeginPresentationRecordPlayback` 的入口接入 Record Type Switch，并完成了 `CardPlayed`、`CardZoneChanged (PlayArea → destination)`、`Damage` 与 `BlockChanged` 四条可达的异步播放骨架；其余 Record 类型仍保留 C++ immediate fallback，因此尚不能认定为完整可运行的 A2E Router。

新增 Designer 控件：

```text
CanvasPanel_54
└── OV_PlayArea : Overlay
    Anchor     = (0.5, 0.61)
    Alignment  = (0.5, 0.5)
    Size       = 200 × 260
    ZOrder     = 0
    Visibility = Hit Test Invisible
```

新增蓝图成员：

```text
ActivePresentationToken : FPresentationPlaybackToken
ActivePresentationType  : EBattlePresentationRecordType
ActivePresentationTimer : FTimerHandle
PlayedCardWidget        : WBP_BattleCard reference
HiddenHandCardWidget    : WBP_BattleCard reference
bDamageTargetIsPlayer   : bool
```

#### BeginPresentationRecordPlayback 图

资产中的 Graph 名是：

```text
BeginPresentationRecordPlayback
```

Function Entry 在蓝图中显示为父类事件名：

```text
Play Presentation Record(Record, Token) → bool
```

图内当前已经保存并可达的 `CardPlayed` 数据/校验路径是：

```text
Function Entry.then
→ Switch EBattlePresentationRecordType

CardPlayed
→ Break CardPlayed payload
→ 校验 HandIndexBefore >= 0
→ 校验 HandIndexBefore < HB_Hand.ChildrenCount
→ HB_Hand.GetChildAt(HandIndexBefore)
→ Cast WBP_BattleCard
→ CardView.RuntimeId == Payload.Card.RuntimeId
→ PlayCardPresentation(CardPlayed, Token, HandCardWidget)
→ Return true
```

校验失败（索引越界、控件类型错误、RuntimeId 不匹配）会返回 `false`，交回 C++ Controller 的 immediate fallback。

`CardZoneChanged` 当前保存的可达路径是：

```text
CardZoneChanged
→ Break CardZoneChanged payload
→ FromZone == PlayArea
→ IsValid(PlayedCardWidget)
→ PlayedCardWidget.CardView.RuntimeId == Payload.Card.RuntimeId
→ PlayCardZoneChangePresentation(CardZoneChanged, Token)
→ Return true
```

该分支只接受 `FromZone = PlayArea` 的记录，用于结束当前 PlayArea 表现卡牌；其他 Zone 变化或任一校验失败均返回 `false`。

其余 `None / ResolutionFault / Victory / Defeat / EnergyChanged / DeckShuffled / StatusChanged` 分支当前仍直接接 `Return false`，交给 C++ Controller 的 immediate fallback。`Damage` 与 `BlockChanged` 已在本次增量中接入正式 Blueprint 播放路径，详见下文。

因此当前保存版本已经有四条可达的 Blueprint 播放入口（CardPlayed、CardZoneChanged、Damage、BlockChanged），但还没有覆盖完整 Record 类型集合。

#### PlayCardPresentation

当前保存的自定义事件流程：

```text
PlayCardPresentation(CardPlayed, Token, HandCardWidget)
→ ActivePresentationToken = Token
→ ActivePresentationType = CardPlayed
→ HiddenHandCardWidget = HandCardWidget
→ HandCardWidget.Visibility = Hidden
→ Break CardPlayed.Card
→ MakePresentationCardView(Card snapshot)
→ Create WBP_BattleCard
    OwningPlayer = GetOwningPlayer
    CardView     = presentation-only card view
    OwnerHUD     = self
→ PlayedCardWidget = created card
→ PlayedCardWidget.Visibility = Hit Test Invisible
→ OV_PlayArea.AddChild(PlayedCardWidget)
→ SetTimerByEvent(0.5s, FinishPresentationRecord)
→ ReturnValue → ActivePresentationTimer
```

这个路径不会通过 Widget 查询 Gameplay；播放卡面来自 Record 内冻结的 `FPresentationCardSnapshot`。

#### PlayDamagePresentation — CURRENT SAVED

新增自定义事件输入：

```text
PlayDamagePresentation
    Damage : FDamagePresentationPayload
    Token  : FPresentationPlaybackToken
```

当前保存的开始播放流程：

```text
PlayDamagePresentation(Damage, Token)
→ ActivePresentationToken = Token
→ ActivePresentationType = Damage
→ Break Damage Presentation Payload
→ TargetPresentationId == ViewModel.Player.PresentationId
→ bDamageTargetIsPlayer = IsPlayer
→ Select(IsPlayer, Combatant_PlayerPresentation,
                   Combatant_EnemyPresentation)
→ SelectedPresentation.RenderOpacity = 0.45
→ Damage.IncomingDamage → ToText(Integer)
→ Txt_DamagePresentation.SetText
→ Txt_DamagePresentation.Visibility = HitTestInvisible
→ Branch(bDamageTargetIsPlayer)
   ├── true（Player）
   │   → Damage.HPAfter + ViewModel.Player.MaxHP
   │      → FormatText "{Current}/{Max}"
   │      → Txt_PlayerHP.SetText
   │   → ToFloat(Damage.HPAfter)
   │      / Max(ToFloat(ViewModel.Player.MaxHP), 1.0)
   │      → PB_PlayerHP.SetPercent
   │   → ToText(Integer)(Damage.BlockAfter)
   │      → Txt_PlayerBlock.SetText
   │   → StartPresentationFinishTimer
   └── false（Enemy）
       → Damage.HPAfter + ViewModel.Enemy.MaxHP
          → FormatText "{Current}/{Max}"
          → Txt_EnemyHP.SetText
       → ToFloat(Damage.HPAfter)
          / Max(ToFloat(ViewModel.Enemy.MaxHP), 1.0)
          → PB_EnemyHP.SetPercent
       → ToText(Integer)(Damage.BlockAfter)
          → Txt_EnemyBlock.SetText
       → StartPresentationFinishTimer
```

`Select` 的 True/A 分支对应 Player，False/B 分支对应 Enemy。`Txt_DamagePresentation` 是根 Canvas 上的 `TextBlock`，Designer 中为 `Is Variable`，默认 `Collapsed`，字体为居中 Roboto Bold 30；其 Canvas Slot 为 Anchor `(0.5, 0.45)`、Alignment `(0.5, 0.5)`、`200 × 70`、`ZOrder = 30`。Timer 复用了已有的 `FinishPresentationRecord` 委托；该事件只读取 Record payload 和当前 HUD `ViewModel` 的 Player `PresentationId`，不查询或修改 Gameplay 历史状态。

本次新增的目标分支直接消费同一个冻结 `Damage` Record 的 `HPAfter` 与 `BlockAfter`：不重新计算 `HPDamage`，不写回或重算 `ViewModel`。HP 百分比的分母使用对应目标的 `MaxHP`，并通过 `Max(..., 1.0)` 防止零分母。Player/Enemy 两条分支分别更新自己的 HP 文本、血条百分比和格挡文本，然后统一进入 `StartPresentationFinishTimer`。

#### PlayBlockChangedPresentation — CURRENT SAVED

新增自定义事件输入：

```text
PlayBlockChangedPresentation
    BlockChanged : FBlockChangedPresentationPayload
    Token        : FPresentationPlaybackToken
```

当前保存的开始播放流程：

```text
PlayBlockChangedPresentation(BlockChanged, Token)
→ ActivePresentationToken = Token
→ ActivePresentationType = BlockChanged
→ Break Block Changed Presentation Payload
→ BlockChanged.TargetPresentationId
   → Name → String → EqualExactly(String)
   → ViewModel.Player.PresentationId
→ Branch(IsPlayer)
   ├── true（Player）
   │   → ToText(Integer)(BlockChanged.BlockAfter)
   │   → Txt_PlayerBlock.SetText
   │   → StartPresentationFinishTimer
   └── false（Enemy）
       → ToText(Integer)(BlockChanged.BlockAfter)
       → Txt_EnemyBlock.SetText
       → StartPresentationFinishTimer
```

该事件直接消费冻结 Record 中的 `BlockAfter`，不使用 `BlockBefore + BlockDelta` 重算，也不读取或修改 Gameplay `ViewModel`。目标已在 Router 中确认属于 Player 或 Enemy，因此事件内 Player 比较为真时走 Player 文本，否则走 Enemy 文本；两条路径共用现有的 `StartPresentationFinishTimer`。

#### StartPresentationFinishTimer — CURRENT SAVED

为避免 Player/Enemy 两条路径复制完成计时器，新增无输入自定义事件：

```text
StartPresentationFinishTimer
→ SetTimerByEvent(Time = 0.5, Looping = false,
                  Event = FinishPresentationRecord)
→ ReturnValue → ActivePresentationTimer
```

两条目标分支都调用该事件，继续沿用现有 Playback Token、Timer Handle 和完成回调语义。`FinishPresentationRecord → Damage` 未改变：仍只隐藏伤害文本、按 `bDamageTargetIsPlayer` 将对应角色 RenderOpacity 恢复为 `1.0`，再通知 `NotifyPresentationRecordFinished`。`Cancel Presentation Record Playback` 也未恢复 `HPBefore/BlockBefore` 等历史值，只清理 Timer、隐藏伤害文本、恢复角色透明度并清空本地播放状态。

#### Damage Router 校验 — CURRENT SAVED

`BeginPresentationRecordPlayback` 的 `Damage` 分支现在先做正式目标校验：

```text
Switch EBattlePresentationRecordType → Damage
→ Break Damage Presentation Payload(Record.Damage)
→ TargetPresentationId == ViewModel.Player.PresentationId
→ TargetPresentationId == ViewModel.Enemy.PresentationId
→ OR
→ Branch
   ├── false → Return false
   └── true
       → PlayDamagePresentation(Record.Damage, FunctionEntry.Token)
       → Return true
```

两个 `PresentationId` 比较使用 `Name → String` 后的 `EqualExactly(String)`，保持精确匹配语义。目标既不匹配 Player 也不匹配 Enemy 时不会启动错误的异步 Timer，并将 Record 交回 C++ immediate fallback。

#### BlockChanged Router 校验 — CURRENT SAVED

`BeginPresentationRecordPlayback` 的 `BlockChanged` 分支现在先做正式目标校验：

```text
Switch EBattlePresentationRecordType → BlockChanged
→ Break Block Changed Presentation Payload(Record.BlockChanged)
→ TargetPresentationId == ViewModel.Player.PresentationId
→ TargetPresentationId == ViewModel.Enemy.PresentationId
→ OR
→ Branch
   ├── false → Return false
   └── true
       → PlayBlockChangedPresentation(Record.BlockChanged, FunctionEntry.Token)
       → Return true
```

两个 `PresentationId` 比较均使用 `Name → String` 后的 `EqualExactly(String)`。只有 Player/Enemy 任一匹配时才把该异步 Record 交给 Blueprint；未匹配时返回 `false`，不会启动错误的计时器。

#### FinishPresentationRecord

当前保存的完成分流：

```text
FinishPresentationRecord
→ Switch ActivePresentationType

Damage
→ Txt_DamagePresentation.Visibility = Collapsed
→ bDamageTargetIsPlayer
   ├── true  → Combatant_PlayerPresentation.RenderOpacity = 1
   └── false → Combatant_EnemyPresentation.RenderOpacity = 1
→ NotifyPresentationRecordFinished

BlockChanged
→ NotifyPresentationRecordFinished

CardPlayed
→ HiddenHandCardWidget = None
→ NotifyPresentationRecordFinished

CardZoneChanged
→ IsValid(PlayedCardWidget)
   ├── valid   → RemoveFromParent → PlayedCardWidget = None
   └── invalid → 直接继续
→ NotifyPresentationRecordFinished
```

其余 Record Type 在该 Switch 上没有完成分支。`BlockChanged` 完成分支不恢复 `BlockBefore`，只通过统一通知结束当前播放。

当前 `Damage` 已有成对的开始/收尾路径：开始事件把目标角色的 RenderOpacity 设为 `0.45` 并启动 `FinishPresentationRecord` Timer；完成事件按 `bDamageTargetIsPlayer` 将对应角色恢复为 `1.0`，再通知 Controller。`CardZoneChanged` 已被 Begin 图接受，但它的播放事件目前只启动短计时器，实际表现清理发生在完成分支。

#### NotifyPresentationRecordFinished

当前保存的统一回调：

```text
NotifyPresentationRecordFinished
→ ClearAndInvalidateTimerByHandle(ActivePresentationTimer)
→ NotifyPresentationFinished(ActivePresentationToken)
→ ActivePresentationType = None
→ ActivePresentationToken = default
```

`CardPlayed`、`CardZoneChanged`、`Damage` 和 `BlockChanged` 的完成计时都最终写入 `ActivePresentationTimer`，因此完成/取消路径可以清理当前 Timer。

当前已保存 `Cancel Presentation Record Playback` 的 Blueprint override。它会先清理 Timer，再恢复 `HiddenHandCardWidget` 的可见性、移除有效的 `PlayedCardWidget`，隐藏 `Txt_DamagePresentation`，将 Player/Enemy 两个角色的 RenderOpacity 恢复为 `1.0`，最后清空临时引用、Active Type 和 Active Token。事件收到的 Token 当前未在 Blueprint 内再次比较；调用边界仍由基类 Controller 的当前 Token 校验负责。

#### 当前 A2E 状态结论

```text
A1 Hand / HUD / target selection wiring
= 保持原有 CURRENT SAVED 状态

A2E PlayPresentationRecord Router
= Entry 已连接 Record Switch
= CardPlayed 已接入异步播放骨架
= CardZoneChanged（仅 FromZone=PlayArea）已接入异步清理骨架
= Damage 已接入目标校验、异步播放和完成回调
= Damage IncomingDamage 已接入 `Txt_DamagePresentation` 动态显示
= Damage HPAfter/BlockAfter 已按目标写入 HP 文本、血条百分比和格挡文本
= BlockChanged 已按目标写入冻结的 BlockAfter 格挡文本
= HP 百分比使用 `ToFloat(HPAfter) / Max(ToFloat(MaxHP), 1.0)`，未重算伤害或修改 ViewModel
= Player/Enemy 共用 `StartPresentationFinishTimer`（0.5 秒，非循环，完成事件为 `FinishPresentationRecord`）
= CardPlayed / CardZoneChanged / Damage Timer Handle 已保存
= Cancel Playback 清理骨架已保存
= Energy / Shuffle / Status / Terminal 仍未接入 Blueprint 播放
= 尚未完成全部 Record routing
= 已完成 Blueprint compile 与资产保存；本轮未新增 BlockChanged PIE acceptance（既有 Strike/Damage PIE 记录仍保留）
```

普通 Strike 的浮动 PIE 验证记录（2026-08-29）：

```text
选中 Strike → 选中 Enemy
→ Enemy HP 100 → 94
→ Energy 5 → 4
→ InteractionState 回到 Idle
→ Damage 完成回调后 Enemy RenderOpacity 恢复为 1.0
```

本轮 `HPAfter/BlockAfter` 路径的浮动 PIE 验证（2026-08-29）：

```text
启动浮动 PIE → 初始 Enemy HP 100/100、Player Energy 5/5
→ 点击 Strike → 点击 Enemy
→ Enemy HP 文本/血条显示 94/100（来自 Damage.HPAfter）
→ Player Energy 变为 4/5
→ PIE 会话正常结束，未观察到 Blueprint Compile 或运行时错误
```

有格挡的 Damage 验证（同日第二个浮动 PIE 会话）：

```text
点击 Defend → Player 获得 5 Block
→ 点击结束回合，Enemy 对 Player 造成 IncomingDamage = 5
→ Gameplay 日志确认 blocked = 5、hpDamage = 0、HP = 80/80、Block = 0
→ 该 Record 的 HPAfter = 80、BlockAfter = 0 对应 Player 分支
→ 0.5 秒播放完成后 HUD 保持 80/80、Block 0，未把 IncomingDamage 误当成 HPDamage
```

Slate 快照调用本身晚于 0.5 秒 Timer，因此中间帧不作为独立截图证据；冻结字段消费方式由已保存节点图和上述 Gameplay 结果共同确认。

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

本次更新确认原有 A1 HUD/目标选择线路仍保存在资产中，并将 Damage 与 BlockChanged 播放路径、`Txt_DamagePresentation` 动态伤害文本和 Player/Enemy 格挡文本更新记录为 `CURRENT SAVED`。A2E 整体仍是 `CURRENT SAVED / PARTIAL`：Record Switch 入口、CardPlayed、CardZoneChanged、Damage、BlockChanged、Timer Handle 和 Cancel 清理已保存，但 Energy/Shuffle/Status/Terminal 等完整 Record routing 仍未完成。当前 `WBP_BattleHUD` 已重新 Compile 并保存；本轮未重新运行 PIE，文中此前浮动 PIE 的 Strike/Damage 验证记录仍为 Enemy HP `100 → 94`、Energy `5 → 4`、播放完成后 RenderOpacity 恢复为 `1.0`。本文记录磁盘上的真实节点、数据线和执行线，不替代后续完整 A2E acceptance。

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
