# Phase 6UI-A2N — Native HUD R0 Baseline

状态：**R0 COMPLETE / VALIDATED；R1 NOT STARTED**  
记录日期：2026-08-31（Asia/Shanghai）

本文件只记录 Native HUD 迁移的可恢复基线。它不改变 Legacy Blueprint、生产配置、Gameplay、Presentation Record/Envelope、Controller、Reducer 或 UI-A3。

## 1. 起点与冻结资产

| 项目 | 值 |
|---|---|
| 当前分支 | `main` |
| R0 起始 HEAD | `4e977f3af3980d7d534867d737a6b78539c92314` (`制定A2N 迁移C++计划`) |
| A2E 行为实现提交 | `81cbfb6af09a52f96ececff597491c5bfcc3665f` |
| A2E 封存文档提交 | `666025c4cc6af2dc1ecf22c51f23810fd8892bb3` |
| 当前生产地图 | `Content/SlayTheSpireDemo/Maps/L_BattleTest.umap` |
| 当前生产 HUD | `Content/SlayTheSpireDemo/UI/Widgets/WBP_BattleHUD.uasset` |
| 生产 `ABattleHUDPresenter::WidgetClass` | `WBP_BattleHUD_C`（当前唯一正式注入点） |
| Native 测试注入 | 预留专用 `L_BattleTest_Native`；R2 创建 Native 资产后，仅覆盖该测试地图 Presenter 实例的 `WidgetClass` |

### Legacy SHA-256（R0 只读核验）

| 资产 | SHA-256 |
|---|---|
| `WBP_BattleHUD.uasset` | `990125C951D52D5F23194D9EB7C079C2F3C514C78A285DF0DDE273B6B1C0F94A` |
| `WBP_BattleCard.uasset` | `1E7579EAFE8BF49AEB953B521604CDE4C442E6580BDEB3E071C210846BC6631F` |
| `WBP_BattleStatus.uasset` | `205180C8DF03DAE5D825AB4428ADD4B90EDFBBBB54F9BFEFE76AF07412DA52D2` |

核验命令为 `Get-FileHash <asset> -Algorithm SHA256`。本次文档编辑不接触上述资产；文档写入前后应再次执行相同命令，三项必须完全一致。

## 2. 已封存验证证据

封存证据均来自当前 A2E/A2 HEAD，R0 不重复运行完整 A2D5 或 Phase6R：

| 证据 | 位置/结果 |
|---|---|
| A2E Blueprint 结构与保存快照 | [`docs/WBPSavedBlueprintSnapshot.md`](WBPSavedBlueprintSnapshot.md)；75 个 Designer 控件、9 个 Blueprint 变量、EventGraph 531 节点 |
| A2E 可见行为与 PIE 日志 | [`docs/UIA2EBlueprintValidationLog.md`](UIA2EBlueprintValidationLog.md) |
| Scenario A–E、active Skip/Cancel、Input Unlock | [`docs/CODEX_GOAL_CHECKPOINT.md`](CODEX_GOAL_CHECKPOINT.md) Batch 4 / Final Seal Evidence |
| stale-token rejection | `CODEX_GOAL_CHECKPOINT.md` Batch 4 临时 Editor-only PIE harness 记录 |
| A2D5 | `Saved/AutomationReports/FinalA2D5/index.json`；exactly 6/6，0 failed，0 notRun |
| Phase6R | `Saved/AutomationReports/FinalSeal_Phase5` 至 `FinalSeal_Phase6UIA2D5`；100/100 PASS |
| Shipping exclusion | `CODEX_GOAL_CHECKPOINT.md` Final Seal Evidence；clean detached worktree Shipping PASS，测试模块未进入 Shipping |
| Legacy PIE smoke（R0 新增） | **PASS**：UE5.8 正式 `L_BattleTest` 浮动 PIE；运行时 Presenter 的 `widgetInstance` 为 `WBP_BattleHUD_C_0`。点击 Strike#6 后选择 Enemy，单个 PlayArea transient 依次完成；最终 Energy `5/5→4/5`、Enemy HP `100/100→94/100`、Discard `0→1`、Hand `5→4`。运行时 ViewModel 为 `stateRevision=5 / interactionState=Idle / bInputLocked=false / outcome=None`；HUD 的 active type/token/timer 与 Card/Status transient references 全部清空。PIE 已停止。 |

## 3. 每个 Record 的 Legacy 可见语义基线

以下是 Native 必须保持的观察契约。所有播放均消费冻结 Record 与匹配的历史快照；不查询 mutable Gameplay 历史对象。

| Record | 有效播放/可见效果 | Finish | Cancel | invalid / token |
|---|---|---|---|---|
| `CardPlayed` | 精确匹配 `RuntimeId`、`CardId`、`HandIndexBefore` 与冻结 Energy Before/After；隐藏正式 Hand 卡，创建 presentation-only 卡并加入 `OV_PlayArea`；不重复生成 `EnergyChanged`。 | 保留 `PlayedCardWidget` 给后续 `CardZoneChanged` 退休；清隐藏 Hand 引用；exact active token Notify 一次并清本 Record 的 timer/type/token。 | 恢复历史 Hand；删除未成立的 PlayArea presentation；不 Notify。 | identity/索引/冻结数据无效返回 false；stale/duplicate callback 无效。 |
| `Damage` | 按 `TargetPresentationId` 定位目标；显示冻结 `IncomingDamage`、`HPBefore/After`、`BlockBefore/After`，不自行推导结果。 | 保留冻结 HP/Block 视觉，隐藏伤害文本并恢复目标 opacity；exact token Notify 一次。 | 隐藏伤害文本、恢复 opacity，并从历史 ViewModel 恢复目标 HP/Block；不 Notify。 | 目标无效、payload 不完整或 token 非当前 active 返回 false/no-op。 |
| `BlockChanged` | 精确目标 + 冻结 Block Before/After 更新正式 Block 文本。 | 保留 After；exact token Notify 一次。 | 恢复目标历史 Block；不 Notify。 | 目标或冻结数据无效返回 false；stale callback no-op。 |
| `EnergyChanged` | 仅接受 `EnergyBefore != EnergyAfter` 且 `Delta == EnergyAfter - EnergyBefore`；使用冻结 `EnergyAfter` 更新正式 `{Current}/{Max}`。 | 保留 After；exact token Notify 一次。 | 从历史 ViewModel 恢复 Energy；不 Notify。 | 无效 delta/目标返回 false；stale callback no-op。 |
| `CardZoneChanged` | 支持 `PlayArea→DiscardPile/ExhaustPile/RemovedPile`、`Hand→DiscardPile`、`DrawPile→Hand`；Hand 路径按 exact RuntimeId/CardId/FromIndex；Draw 使用冻结 Card snapshot、不可交互 transient。 | Discard 不恢复隐藏 Hand；Draw 不主动移除 transient；PlayArea 清理 `PlayedCardWidget`；exact token Notify 一次。 | Hand discard 恢复隐藏 Hand；Draw 移除 transient；PlayArea 清理悬空 transient；不提前改 pile count，不 Notify。 | 未知 zone pair、identity/index/count 不一致返回 false；stale callback no-op。 |
| `DeckShuffled` | 仅接受冻结计数合法且 `DrawBefore=0`、`DiscardBefore=MovedCardCount`、`DrawAfter=MovedCardCount`、`DiscardAfter=0`、总数守恒，并匹配历史 ViewModel Before；更新正式 Draw/Discard count。 | 保留 After；exact token Notify 一次。 | 从历史 ViewModel 恢复 Draw/Discard count；不 Notify。 | 非法计数返回 false；不得伪造逐张 shuffle Record；stale callback no-op。 |
| `StatusChanged` | 精确身份为 `TargetPresentationId + StatusId + RuntimeSequence`；create 创建行，update/reduction 复用同一 exact Widget，remove 隐藏 exact Widget。 | update/remove 保留结果；exact token Notify 一次。 | 从历史 ViewModel 重建 Player/Enemy status rows，不逆向计算 Before；不 Notify。 | exact lookup 失败返回 false；StatusId-only、数组序号或 DisplayName 匹配无效；stale callback no-op。 |
| `Victory` | 仅在 Player 胜、Enemy 已死亡、双方 PresentationId 有效且不同、历史 Outcome 为 None，且前序 Record 已完成后显示“胜利”。 | 保留 terminal overlay；exact token Notify 一次，不在 Finish 隐藏 overlay。 | 调历史 terminal surface；不 Notify。 | 条件或 payload 无效返回 false；stale callback no-op。 |
| `Defeat` | 仅在 Enemy 胜、Player 已死亡、历史 Outcome 为 None，且前序 Record 已完成后显示“战斗失败”。 | 保留 terminal overlay；exact token Notify 一次。 | 调历史 terminal surface；不 Notify。 | 条件或 payload 无效返回 false；stale callback no-op。 |
| `ResolutionFault` | `Reason` 非空、`ExecutedActionCount >= 0`、历史 Outcome 为 None 时显示“战斗结算异常”；不由 PresentationUnavailable 生成。 | 保留 terminal overlay；exact token Notify 一次。 | 调历史 terminal surface；不 Notify。 | payload/Outcome 无效返回 false；stale callback no-op。 |
| `None` | 不是可播放 Record。 | 不开始播放。 | 不执行恢复。 | 始终返回 false。 |

`PresentationUnavailable` 是 ViewModel 驱动的 UI-only surface，不是 Record，也不进入 `ResolutionFault` Event；Native helper 只能渲染历史 ViewModel 的 availability/error 状态。

所有正常异步完成都必须带当前 exact token 调用 `NotifyPresentationFinished`，每个 Record 只允许一次。Cancel、Skip 后旧回调、旧 Widget 回调和 Timer 回调都不得 Notify 新 token。

## 4. Legacy 子组件依赖

### `WBP_BattleCard`

- Designer 快照：`SB_Card → Btn_Card → OV_Card`，显示名称、类型、描述、费用和卡图。
- 成员：`CardView : FBattleHUDCardView`、`OwnerHUD : WBP_BattleHUD reference`。
- Construct 消费冻结 `CardView`；点击调用 `OwnerHUD.SelectCard(CardView.RuntimeId)`。
- Native 迁移必须移除对具体 HUD WBP 的依赖，但 R0 不修改、不 reparent、不复制 Legacy 资产。
- Presentation-only Card 必须不可交互；正式 Hand Card 才绑定正式输入请求。

### `WBP_BattleStatus`

- Designer：`SB_Status → OV_Status → Img_StatusIcon + Txt_StatusAmount`。
- 成员：`StatusView`、`CurrentStatusView`、`MID_StatusIcon`。
- `SetStatusView` 写入冻结金额与图标材质参数；HUD 以完整三元 identity 定位生命周期。
- Native Status Widget 只保存/暴露 frozen view 与 identity，不拥有 Status 生命周期规则；R0 不修改、不 reparent Legacy 资产。

### 正式 HUD 绑定事实

当前 HUD 的 `WBP_BattleHUD` 父类是 `UBattleHUDWidgetBase`。Presenter 通过 `WidgetClass : TSubclassOf<UBattleHUDWidgetBase>` 创建它；本字段是唯一生产/测试注入点。R0 不添加第二 Controller assembly、不添加运行时 Legacy/Native toggle。

## 5. WBP_BattleHUD Designer 控件盘点

### 5.1 盘点方法与真实导出结果

`Saved/Codex/R0HUDWidgets.json` 与 `Saved/Codex/R0HUDWidgetsResponse.json` 是 UE5.8 MCP UMGToolSet `GetWidgets` 的只读导出。它确认 `widgetCount=75`、`inheritedWidgetCount=0`，并为每项提供真实 `Name`、`Type`、`IsVariable` 与完整 `RefPath`。下表完整覆盖 75 项，不再使用命名推断。

表中基础 `UMG` 类型使用去掉 `UMG.` 的简写；复合 WBP 类型保留完整资产类名。每项的完整 RefPath 见 `Saved/Codex/R0HUDWidgets.json`；共同前缀为 `/Game/SlayTheSpireDemo/UI/Widgets/WBP_BattleHUD.WBP_BattleHUD:WidgetTree.`，末段等于本表 Name。

字段：控件名称｜当前类型｜当前 `Is Variable`｜Native 分类｜Legacy 用途｜Native 用途。

### 5.2 完整控件清单（75）

| 控件名称 | 当前类型 | 当前 Is Variable | Native 分类 | Legacy 用途 | Native 用途 |
|---|---|---:|---|---|---|
| `CanvasPanel_54` | CanvasPanel | false | Designer-only | 根布局 | 保留布局；C++ 不需绑定 |
| `Img_PlayerCharacter` | Image | false | Designer-only | 旧角色图，Collapsed | 保留视觉回退，不绑定 |
| `Img_EnemyCharacter` | UMG.Image | true | Designer-only | 旧角色图，Collapsed | 保留视觉回退，不绑定 |
| `Combatant_PlayerPresentation` | `WBP_CombatantPresentation` | true | Required BindWidget | 玩家角色、目标/检查事件 | 绑定角色原生 Widget |
| `Combatant_EnemyPresentation` | `WBP_CombatantPresentation` | true | Required BindWidget | 敌人角色、目标/检查事件 | 绑定角色原生 Widget |
| `PlayerPanel` | UMG.VerticalBox | false | Designer-only | 玩家面板布局 | 仅 Designer |
| `EnemyPanel` | UMG.VerticalBox | false | Designer-only | 敌人面板布局 | 仅 Designer |
| `EnemyIntentPanel` | UMG.VerticalBox | true | BindWidgetOptional | 敌人意图布局 | 可选意图表面 |
| `EnergyPanel` | UMG.Overlay | false | Designer-only | 能量布局 | 仅 Designer |
| `HB_Hand` | HorizontalBox | true | Required BindWidget | 正式 Hand 卡容器 | 创建/查找 Hand 卡 |
| `DrawPilePanel` | UMG.Overlay | false | Designer-only | 抽牌堆布局 | 仅 Designer |
| `DiscardPilePanel` | UMG.Overlay | false | Designer-only | 弃牌堆布局 | 仅 Designer |
| `ExhaustPanel` | UMG.Overlay | false | Designer-only | Exhaust 布局 | 仅 Designer |
| `Btn_EndTurn` | Button | true | Required BindWidget | EndTurn 输入 | 绑定 EndTurn |
| `Btn_Confirm` | Button | true | Required BindWidget | Confirm 输入 | 绑定 Confirm |
| `Btn_Cancel` | Button | true | Required BindWidget | Cancel 输入 | 绑定 Cancel |
| `Txt_Feedback` | TextBlock | true | Required BindWidget | Feedback 文案 | 刷新反馈 |
| `OV_PlayArea` | Overlay | true | Required BindWidget | PlayArea transient 容器 | 管理 presentation-only Card |
| `Txt_DamagePresentation` | UMG.TextBlock | true | Required BindWidget | 冻结伤害文字 | Native 副本直接绑定；Legacy 无需修改 |
| `StatusTooltip_Player` | Widget/tooltip surface | true | BindWidgetOptional | 玩家状态 tooltip | 可选 tooltip |
| `StatusTooltip_Enemy` | Widget/tooltip surface | true | BindWidgetOptional | 敌人状态 tooltip | 可选 tooltip |
| `Overlay_Terminal` | Overlay | true | Required BindWidget | 终局遮罩 | 显示 Terminal |
| `SB_PlayerVitals` | SizeBox | false | Designer-only | 玩家 vitals 尺寸 | 仅 Designer |
| `OV_PlayerVitals` | Overlay | false | Designer-only | 玩家 HP/Block 层 | 仅 Designer |
| `SB_PlayerHPArea` | SizeBox | false | Designer-only | 玩家 HP 区域 | 仅 Designer |
| `OV_PlayerHP` | Overlay | false | Designer-only | 玩家 HP 层 | 仅 Designer |
| `PB_PlayerHP` | ProgressBar | true | Required BindWidget | 玩家 HP percent | 更新 HP 百分比 |
| `Txt_PlayerHP` | TextBlock | true | Required BindWidget | 玩家 HP 文本 | 更新 HP 文本 |
| `SB_PlayerBlockBadge` | UMG.SizeBox | true | Designer-only | 玩家 Block 尺寸 | 仅 Designer |
| `OV_PlayerBlock` | Overlay | false | Designer-only | 玩家 Block 层 | 仅 Designer |
| `Img_PlayerBlock` | Image | false | Designer-only | 玩家 Block 图标 | 仅 Designer |
| `Txt_PlayerBlock` | TextBlock | true | Required BindWidget | 玩家 Block 文本 | 更新 Block |
| `OV_PlayerMeta` | Overlay | false | Designer-only | 玩家名称/状态层 | 仅 Designer |
| `Txt_PlayerName` | TextBlock | true | BindWidgetOptional | Hover 名称 | 可选检查表面 |
| `WB_PlayerStatuses` | WrapBox | true | Required BindWidget | 玩家正式状态容器 | 重建状态行 |
| `SB_EnemyVitals` | SizeBox | false | Designer-only | 敌人 vitals 尺寸 | 仅 Designer |
| `OV_EnemyVitals` | Overlay | false | Designer-only | 敌人 HP/Block 层 | 仅 Designer |
| `SB_EnemyHPArea` | SizeBox | false | Designer-only | 敌人 HP 区域 | 仅 Designer |
| `OV_EnemyHP` | Overlay | false | Designer-only | 敌人 HP 层 | 仅 Designer |
| `PB_EnemyHP` | ProgressBar | true | Required BindWidget | 敌人 HP percent | 更新 HP 百分比 |
| `Txt_EnemyHP` | TextBlock | true | Required BindWidget | 敌人 HP 文本 | 更新 HP 文本 |
| `SB_EnemyBlockBadge` | UMG.SizeBox | true | Designer-only | 敌人 Block 尺寸 | 仅 Designer |
| `OV_EnemyBlock` | Overlay | false | Designer-only | 敌人 Block 层 | 仅 Designer |
| `Img_EnemyBlock` | Image | false | Designer-only | 敌人 Block 图标 | 仅 Designer |
| `Txt_EnemyBlock` | TextBlock | true | Required BindWidget | 敌人 Block 文本 | 更新 Block |
| `OV_EnemyMeta` | Overlay | false | Designer-only | 敌人名称/状态层 | 仅 Designer |
| `Txt_EnemyName` | TextBlock | true | BindWidgetOptional | Hover 名称 | 可选检查表面 |
| `WB_EnemyStatuses` | WrapBox | true | Required BindWidget | 敌人正式状态容器 | 重建状态行 |
| `Img_DrawPile` | Image | false | Designer-only | Draw pile 图 | 仅 Designer |
| `Img_DrawShadow` | Image | false | Designer-only | Draw pile 阴影 | 仅 Designer |
| `Img_DrawBadgeBG` | Image | false | Designer-only | Draw badge 背景 | 仅 Designer |
| `Img_DrawBadgeBG_1` | Image | false | Designer-only | Draw badge 背景实例 | 仅 Designer |
| `Img_DiscardPile` | Image | false | Designer-only | Discard pile 图 | 仅 Designer |
| `Img_DiscardShadow` | Image | false | Designer-only | Discard pile 阴影 | 仅 Designer |
| `Img_EnemyIntent` | Image | false | Designer-only | 敌人意图图标 | 仅 Designer或由可选表面承载 |
| `Img_EnergyBackground` | Image | false | Designer-only | Energy 背景 | 仅 Designer |
| `Img_ExhaustPile` | Image | false | Designer-only | Exhaust 图 | 仅 Designer |
| `OV_DrawPile` | Overlay | false | Designer-only | Draw pile 层 | 仅 Designer |
| `OV_DrawBadge` | Overlay | false | Designer-only | Draw badge 层 | 仅 Designer |
| `OV_DiscardPile` | Overlay | false | Designer-only | Discard pile 层 | 仅 Designer |
| `OV_DiscardBadge` | Overlay | false | Designer-only | Discard badge 层 | 仅 Designer |
| `SB_DrawBadge` | SizeBox | false | Designer-only | Draw badge 尺寸 | 仅 Designer |
| `SB_DiscardBadge` | SizeBox | false | Designer-only | Discard badge 尺寸 | 仅 Designer |
| `SB_IntentIcon` | SizeBox | false | Designer-only | Intent 图标尺寸 | 仅 Designer |
| `ScaleBox_IntentIcon` | ScaleBox | false | Designer-only | Intent 图标缩放 | 仅 Designer |
| `Txt_DrawCount` | TextBlock | true | Required BindWidget | Draw count | 更新 Draw count |
| `Txt_DiscardCount` | TextBlock | true | Required BindWidget | Discard count | 更新 Discard count |
| `Txt_ExhaustCount` | TextBlock | true | Required BindWidget | Exhaust count | 更新 Exhaust count |
| `Txt_EnemyIntent` | TextBlock | true | BindWidgetOptional | Enemy intent 文案 | 可选刷新 |
| `Txt_Energy` | TextBlock | true | Required BindWidget | Energy `{Current}/{Max}` | 更新冻结 Energy |
| `Txt_Outcome` | TextBlock | true | Required BindWidget | Terminal 文案 | 更新终局文案 |
| `BG_Terminal` | UMG.Border | false | Designer-only | Terminal 背景 | 仅 Designer |
| `Txt_Cancel` | TextBlock | false | Designer-only | Cancel 按钮文字 | Button 样式由 Designer |
| `Txt_Confirm` | TextBlock | false | Designer-only | Confirm 按钮文字 | Button 样式由 Designer |
| `Txt_EndTurn` | UMG.TextBlock | true | Designer-only | EndTurn 按钮文字 | Button 样式由 Designer |

> 注：上述 75 项及其完整 `RefPath` 以 UE5.8 MCP 只读 JSON 为准；Legacy 三资产未修改。`Txt_DamagePresentation` 已确认 `UMG.TextBlock` / `IsVariable=true`，Native duplicate 可直接声明 Required BindWidget。

### 5.3 盘点结论

R0 控件盘点已完成：75 项、真实类型、`IsVariable` 与 `RefPath` 均由 UE5.8 MCP 只读导出确认；统计为 `IsVariable=true` 33 项、`false` 42 项。按 Native 运行需要分类为 Required BindWidget 23 项、BindWidgetOptional 6 项、Designer-only 46 项。`Txt_DamagePresentation` 的真实 `IsVariable=true` 已纠正旧快照/计划中的错误假定。该纠正不需要修改 Legacy WBP。

## 6. 非生产 Native 注入决策

锁定专用迁移测试地图方案：

```text
正式 L_BattleTest / production Presenter
    WidgetClass = WBP_BattleHUD

R2 迁移测试 L_BattleTest_Native / 该地图 Presenter 实例
    WidgetClass = WBP_BattleHUD_Native
```

- 仍使用 `ABattleHUDPresenter::WidgetClass` 作为唯一注入点。
- 复用现有 ViewModel、Controller 和 Presenter assembly。
- 不增加运行时 Legacy/Native 开关，不在玩家可见配置中暴露切换。
- R0 不创建 `L_BattleTest_Native`，因为 `WBP_BattleHUD_Native` 尚未存在；这里只锁定方案。
- 生产 `L_BattleTest`、生产 Presenter 与 `DefaultEngine.ini` 保持 Legacy。
- R1 只能建立向后兼容 native hook；R2 才创建 Native WBP 与迁移测试地图。

## 7. R0 Gate 与下一步

| Gate | 当前状态 |
|---|---|
| 起始 HEAD/hash 记录 | PASS |
| A2E 封存证据引用 | PASS |
| Legacy 三资产 hash 只读核验 | PASS；文档写入后重复核验仍与冻结值一致 |
| Legacy PIE smoke | PASS；正式 `L_BattleTest` / `WBP_BattleHUD_C` / Strike `5→4` Energy、Enemy `100→94`、Idle、输入解锁 |
| Designer 清单 75/75、真实类型、Is Variable 与 RefPath | PASS；UE5.8 MCP UMGToolSet `GetWidgets` 只读导出，true=33 / false=42 |
| Native 测试注入方案锁定 | PASS（专用 `L_BattleTest_Native`，R2 创建） |
| 正式生产配置仍为 Legacy | PASS（未修改） |
| R1 | NOT STARTED |

R0 的资产基线、75/75 控件盘点、生产注入核查与正式 Legacy PIE smoke 已完成并通过。下一精确动作是 R1：只建立向后兼容的 Native ViewModel changed extension hook，并先证明 Legacy `BP_OnViewModelChanged` 路径保持不变；不得修改 Legacy WBP 或提前进入 R2。
