# UI-A2E 后续完整实施与验收步骤

日期：**2026-08-31**

用途：记录从当前进度到 **UI-A2E COMPLETE / SEALED** 的全部剩余步骤，后续实现严格按本文顺序推进，避免遗漏、跳步或提前进入 UI-A3。

> 当前仓库正式验证基线以 `docs/UIA2EBlueprintValidationLog.md` 为准；A2E 总体契约以 `docs/Phase6UIA2EImplementation.md` 为准。

---

## 0. 当前基线

仓库已正式记录并完成 PIE 验收：

```text
CardPlayed              VALIDATED
Damage                  VALIDATED
BlockChanged            VALIDATED
CardZoneChanged         VALIDATED（当前 producer 全路径）
StatusChanged           FULLY VALIDATED
EnergyChanged           VALIDATED
DeckShuffled            VALIDATED
Victory                 VALIDATED
Defeat                  VALIDATED
ResolutionFault         VALIDATED
PresentationUnavailable 与 ResolutionFault 分离 VALIDATED
```

尚未正式封闭：

```text
final-head A2D5 exactly 6
Phase6R 100/100
Shipping exclusion
UI-A2 COMPLETE / SEALED 文档收口
```

当前正在实现的子阶段：

```text
Batch 4 implementation/PIE 已通过 -> final-head seal gates
```

当前已经完成的前置结构：

```text
WBP_BattleStatus
- 保存 CurrentStatusView : FBattleHUDStatusView
- SetStatusView(InStatusView) 时先保存 CurrentStatusView

WBP_BattleHUD
- FindStatusWidgetByIdentity(
    TargetPresentationId,
    StatusId,
    RuntimeSequence
  )
- 精确身份：
    TargetPresentationId
    + StatusId
    + RuntimeSequence
```

在后续更新/减少阶段，禁止只按 `StatusId` 或数组索引定位状态。

---

# 1. 全程必须保持的 A2E 契约

后续所有步骤都必须满足以下规则。

## 1.1 历史状态与活动 Record 的关系

```text
Record
= 当前正在播放的已提交历史事实

ViewModel / WorkingPresentationSnapshot
= 已经完成播放的历史事实
```

因此：

```text
活动 Record 的视觉值
→ 从冻结 Record Payload 读取

Record 完成后
→ NotifyPresentationFinished(Token)
→ Controller reducer 推进 WorkingSnapshot
→ ViewModel 才进入新历史状态
```

Blueprint 不得为了播放当前 Record 而提前修改 ViewModel。

## 1.2 Blueprint 不拥有 Gameplay 真值

禁止：

```text
从 UStatusInstance / UStatusData 查询历史状态
重新计算 Damage / Block / Energy
修改 ViewModel.HP / Block / Energy / Statuses / pile truth
自行决定 Record 顺序
自行制造 Gameplay ResolutionFault
```

允许：

```text
读取冻结 Record Payload
创建/更新 transient visual
启动短异步表现
用精确 Token 完成回调
```

## 1.3 Token 规则

每个 Blueprint 接管的异步 Record 必须满足：

```text
Return true
= Blueprint 确实已经开始有效的异步表现

无效目标 / 找不到精确 Widget / 不支持的子类型
= Return false
= 交给 C++ immediate fallback
```

正常完成：

```text
NotifyPresentationFinished(ActivePresentationToken)
→ 然后清 ActivePresentationToken
```

Cancel：

```text
不得 Notify 正常完成
不得留下当前 Record 的未来视觉状态
```

## 1.4 CardPlayed 能量语义

卡牌费用只属于：

```text
CardPlayed
- EnergyBefore
- EnergyAfter
- CostPaid
```

同一次卡牌支付不能再生成/播放一条重复的 `EnergyChanged`。

后续 `EnergyChanged` 只播放真实独立能量变化，例如 EndTurn 清空或新回合恢复。

## 1.5 Terminal 语义

```text
Victory / Defeat / ResolutionFault
= Gameplay / framework terminal Record

PresentationUnavailable
= Presentation 层失败
= 不是 ResolutionFault
```

两者严禁混用。

---

# 2. StatusChanged Update / Reduction

当前保存资产的真实状态：

```text
StatusChanged creation          VALIDATED
StatusChanged update/reduction  VALIDATED
StatusChanged removal           VALIDATED
```

`WBP_BattleHUD` 已经落地本节的 Blueprint 接线；下面的步骤保留为连线复核和
PIE 验收依据，不应再次执行整张图重写。

## 2.1 复核 PlayStatusChangedPresentation

现有 Custom Event 已保存以下输入：

```text
ExistingStatusWidget
: WBP_BattleStatus Object Reference
```

最终输入：

```text
StatusChanged
Token
ExistingStatusWidget
```

保留前半段：

```text
ActivePresentationToken = Token
→ ActivePresentationType = StatusChanged
→ Break StatusChanged Presentation Payload
→ MakePresentationStatusView(StatusChanged)
```

复核：

```text
Branch(bCreated)
```

### Creation 路径

保持已经验证通过的原路径：

```text
bCreated = true
→ Create WBP_BattleStatus
→ ActiveStatusPresentationWidget = CreatedWidget
→ CreatedWidget.SetStatusView(FrozenStatusView)
→ 加入正确 Player / Enemy WrapBox
→ StartPresentationFinishTimer
```

Creation 调用时：

```text
ExistingStatusWidget = None
```

### Update / Reduction 路径

```text
bCreated = false
→ ActiveStatusPresentationWidget = ExistingStatusWidget
→ ExistingStatusWidget.SetStatusView(
     MakePresentationStatusView(StatusChanged)
   )
→ StartPresentationFinishTimer
```

禁止：

```text
Create WBP_BattleStatus
AddChildToWrapBox
重新计算 Amount
修改 ViewModel.Statuses
```

更新值必须直接来自：

```text
Record.AmountAfter
Record.DescriptionAfter
Record.DisplayName
Record.StatusId
Record.RuntimeSequence
Record icon / atlas metadata
```

---

## 2.2 重构 StatusChanged Router

Router 目标结构（当前保存的 `WBP_BattleHUD` 已实现，以下用于复核）：

```text
StatusChanged
↓
验证 TargetPresentationId = Player / Enemy
↓
TargetKnown?
├ false → Return false
└ true
    ↓
    bRemoved?
    ├ true
    │   → FindStatusWidgetByIdentity(
    │       TargetPresentationId,
    │       StatusId,
    │       RuntimeSequence
    │     )
    │   ↓
    │   Found?
    │   ├ false → Return false
    │   └ true
    │       → PlayStatusChangedPresentation(
    │           Record.StatusChanged,
    │           Token,
    │           FoundStatusWidget
    │         )
    │       → Return true
    │
    └ false
        ↓
        bCreated?
        ├ true
        │   → PlayStatusChangedPresentation(
        │       Record.StatusChanged,
        │       Token,
        │       None
        │     )
        │   → Return true
        │
        └ false
            → FindStatusWidgetByIdentity(
                TargetPresentationId,
                StatusId,
                RuntimeSequence
              )
            ↓
            Found?
            ├ false → Return false
            └ true
                → PlayStatusChangedPresentation(
                    Record.StatusChanged,
                    Token,
                    FoundStatusWidget
                  )
                → Return true
```

关键规则：

```text
找不到精确 runtime status row
→ Blueprint 不得伪装已经开始播放
→ Return false
```

---

## 2.3 修改 StatusChanged Cancel

原 creation-only 的：

```text
ActiveStatusPresentationWidget
→ RemoveFromParent
```

不能直接用于 update，因为 update 时它指向正式状态 Widget。当前保存的 Cancel
已经改为按下面的历史 ViewModel 重建路径处理：

StatusChanged Cancel 改为从当前历史 ViewModel 重建正式状态区：

```text
ActivePresentationType == StatusChanged
↓
RebuildStatusIcons(
  ViewModel.Player.Statuses,
  WB_PlayerStatuses
)
↓
RebuildStatusIcons(
  ViewModel.Enemy.Statuses,
  WB_EnemyStatuses
)
↓
ActiveStatusPresentationWidget = None
```

这样同时覆盖：

```text
Creation Cancel
→ 删除尚未提交 reducer 的 transient 新状态

Update Cancel
→ 把临时 AmountAfter 恢复为历史 ViewModel 中的旧值
```

Cancel 不得调用正常完成 Notify。

---

## 2.4 StatusChanged Finish 保持规则

正常 Finish：

```text
StatusChanged
→ NotifyPresentationRecordFinished
→ ActiveStatusPresentationWidget = None
```

不要在 Notify 前：

```text
RemoveFromParent
恢复 AmountBefore
重新 RebuildStatusIcons
```

正确表现：

```text
Weak 2
→ transient 显示 Weak 4
→ 0.5s
→ Notify
→ reducer 推进 ViewModel 为 Weak 4
→ HUD 正常刷新
→ 仍显示 Weak 4
```

禁止出现：

```text
2 → 4 → 2 → 4
```

---

## 2.5 Compile + Save

依赖顺序：

```text
先 Compile + Save WBP_BattleStatus
再 Compile + Save WBP_BattleHUD
```

必须 0 Errors。

如果 HUD 报：

```text
Could not find variable CurrentStatusView
```

优先重新编译 `WBP_BattleStatus`，然后 Refresh / 重建 HUD 中的 `Get CurrentStatusView` 节点。

---

## 2.6 PIE：Increase / Reapply

示例：

```text
Weak 2
→ 再次施加同 runtime identity 的 Weak
→ Record:
   bCreated=false
   bRemoved=false
   AmountBefore=2
   AmountAfter=4
```

预期：

```text
Weak 2
→ Weak 4
→ 保持异步表现窗口
→ Notify
→ 正式 HUD 仍 Weak 4
```

验收：

```text
状态 Widget 数量仍为 1
没有重复 Weak
没有 2→4→2→4 闪回
最终正常继续后续 Record
最终回 Idle
```

---

## 2.7 PIE：Reduction / TurnEndDecay

示例：

```text
Weak 4 → Weak 3
```

预期：

```text
同一个精确 Widget
→ Amount 显示 3
→ 异步窗口
→ Notify
→ ViewModel reducer 后仍为 3
```

验收：

```text
不创建第二个状态图标
不错误匹配其他 RuntimeSequence
无 4→3→4→3 闪回
最终流程继续
```

通过后更新验证文档：

```text
StatusChanged update/reduction = VALIDATED
```

---

# 3. StatusChanged Removal

只有 Update/Reduction PIE 通过后进入。

当前 HUD 已保存本节的最小 Removal 实现，并已按下述身份规则完成可视 PIE
验收。除非后续回归证明缺陷，不要重复添加 Router 或播放节点。

## 3.1 Router 接管 bRemoved=true

Removal 仍使用精确身份：

```text
TargetPresentationId
+ StatusId
+ RuntimeSequence
```

Router：

```text
bRemoved = true
→ FindStatusWidgetByIdentity(...)
→ Found?
   ├ false → Return false
   └ true  → 开始 Removal async playback
```

---

## 3.2 Removal 临时表现

推荐最小实现：

```text
ActivePresentationToken = Token
ActivePresentationType = StatusChanged
ActiveStatusPresentationWidget = FoundWidget
→ FoundWidget 临时 Collapsed / Hidden
→ StartPresentationFinishTimer
```

不要立刻修改 ViewModel.Statuses。

正常 Finish：

```text
Notify
→ reducer 正式移除状态
→ HUD 重建
→ 状态正式消失
```

这样不会出现：

```text
消失 → 再出现 → 再消失
```

---

## 3.3 Removal Cancel

继续使用统一 StatusChanged Cancel：

```text
从当前历史 ViewModel.Player/Enemy.Statuses
重新 RebuildStatusIcons
```

因此被临时隐藏的状态会正确恢复。

---

## 3.4 PIE：Removal

示例：

```text
Weak 1
→ TurnEndDecay / Removed
→ AmountBefore=1
→ AmountAfter=0
→ bRemoved=true
```

验收：

```text
只移除精确 RuntimeSequence
临时消失可见
Notify 后保持消失
没有 disappear→reappear→disappear
没有误删同 StatusId 的其他 runtime row
最终回 Idle / 后续 Record 正常
```

通过后：

```text
StatusChanged = FULLY VALIDATED
```

---

# 4. EnergyChanged

完整 StatusChanged 通过后再做。

## 4.1 Router

将：

```text
EnergyChanged → Return false
```

改为正式异步路径。

无效 Payload/不支持情况仍 Return false。

---

## 4.2 最小可见表现

使用冻结 Record：

```text
EnergyBefore
EnergyAfter
Delta / Reason（按实际 Payload 字段）
```

HUD 临时显示：

```text
EnergyAfter
```

然后：

```text
StartPresentationFinishTimer
→ Notify
→ reducer 推进正式 Energy
```

禁止：

```text
EnergyBefore + Delta 自己重算最终值
修改 ViewModel Energy
```

---

## 4.3 卡费重复保护

再次确认：

```text
CardPlayed 的 CostPaid
不应再由 EnergyChanged 重复表现一次
```

PIE 必须检查一次普通打牌：

```text
Energy 5
→ Cost 1 CardPlayed
→ 直接看到 4
→ 不再随后出现重复 5→4 EnergyChanged
```

---

## 4.4 EndTurn 能量 PIE

至少验证：

```text
玩家回合结束真实能量清空
→ EnergyChanged

下一玩家回合真实能量恢复
→ EnergyChanged
```

要求每次只对应真实变化一次。

通过后：

```text
EnergyChanged = VALIDATED
```

---

# 5. CardZoneChanged 剩余转区表现

当前只有：

```text
PlayArea -> Destination
```

已经验证。

A2E EndTurn 全链仍需要覆盖实际生产出来的剩余转区 Record，至少包括项目当前真实存在的：

```text
Hand -> Discard / Exhaust / other destination
DrawPile -> Hand
以及其他 EndTurn / Draw 过程中可见的真实 CardZoneChanged
```

## 5.1 Router 扩展

不要把所有 ZoneChanged 都强行套用 `PlayedCardWidget`。

按 `FromZone / ToZone` 分流：

```text
PlayArea -> Destination
→ 继续使用 PlayedCardWidget retirement

Hand -> Destination
→ 对应 Hand / pile 数量表现

DrawPile -> Hand
→ 对应 Draw pile / Hand 数量变化表现
```

第一版只要求最小确定性可见变化，不要求卡牌飞行动画。

---

## 5.2 冻结数据原则

如果 Payload 已提供：

```text
Card snapshot
FromZone
ToZone
zone counts / indices
```

直接使用 Payload。

不要根据当前实时 Hand 数组推导历史转区结果。

---

## 5.3 PIE

完整 EndTurn 中检查：

```text
手牌弃置按 Record 顺序发生
抽牌按 DrawPile -> Hand Record 顺序发生
pile / hand 数量不提前跳最终值
每个异步 Record 完成后再推进 ViewModel
```

---

# 6. DeckShuffled

## 6.1 Router

将：

```text
DeckShuffled → Return false
```

改为短异步表现。

---

## 6.2 最小视觉

第一版无需复杂洗牌动画。

最低要求：

```text
显示短 shuffle cue / feedback
同时根据冻结 Payload 显示正确 pile transition
```

使用实际 Payload 提供的：

```text
MovedCardCount
DrawCountBefore / After
DiscardCountBefore / After
```

如果当前字段名与此不同，以实际 C++ struct 为准。

禁止从当前 Deck 容器重新推算历史洗牌数量。

---

## 6.3 PIE

制造 DrawPile 不足、需要 Discard 回洗的场景：

```text
Discard pile 有牌
Draw pile 不足
→ DeckShuffled
→ DrawPile / DiscardPile 数量正确变化
→ 后续 DrawPile -> Hand 继续
```

验收：

```text
洗牌 Record 顺序正确
不提前显示最终抽牌结果
不重复洗牌表现
最终 pile 数量与 ViewModel 一致
```

通过后：

```text
DeckShuffled = VALIDATED
```

---

# 7. Terminal：Victory / Defeat / ResolutionFault

三个终局 Record 可以在同一阶段完成，但必须保持语义分离。

## 7.1 Victory Router + Visual

```text
Victory
→ ActivePresentationToken = Token
→ ActivePresentationType = Victory
→ 显示 Victory terminal treatment
→ 短异步完成
→ Notify
```

不能因为 Gameplay 已经 lethal 就提前从 ViewModel 直接弹 Victory。

顺序必须是：

```text
lethal Damage / followups / zone facts 完成
→ Victory Record 开始
→ Notify Victory
→ terminal reducer
→ ViewModel 正式 Terminal
```

---

## 7.2 Defeat Router + Visual

同理：

```text
Defeat
→ 先完成导致死亡的可见事实
→ 再播放 Defeat
→ Notify
→ reducer 正式 Terminal
```

---

## 7.3 ResolutionFault Router + Visual

```text
ResolutionFault
→ 明确 framework/gameplay resolution fault UI
→ 不伪装成 Victory / Defeat
→ Notify
→ terminal reducer
```

必须与：

```text
PresentationUnavailable
```

保持完全不同的路径。

---

## 7.4 Terminal Cancel / stale callback

要求：

```text
旧 Token / stale Token
→ 不得终结新的 Envelope

Cancel
→ 清 transient terminal visual
→ 不发送正常完成 Notify
```

---

## 7.5 PIE：Victory

至少：

```text
CardPlayed
→ lethal Damage
→ followups
→ CardZoneChanged(PlayArea -> Destination)
→ Victory
```

确认：

```text
死亡/伤害事实先可见
Victory 后出现
Victory 不抢在 Damage 前
终局只完成一次
```

---

## 7.6 PIE：Defeat

通过 Enemy Damage 或当前可配置路径制造玩家死亡：

```text
导致死亡的 Damage 先播放
→ Defeat
→ Terminal
```

---

## 7.7 PIE：ResolutionFault vs PresentationUnavailable

必须分别验证：

```text
真实 ResolutionFault
→ fault terminal surface

Presentation-only failure
→ PresentationUnavailable / collapse / reconcile
→ Gameplay ownership 不变
→ 不显示 ResolutionFault terminal
```

通过后：

```text
Victory         VALIDATED
Defeat          VALIDATED
ResolutionFault VALIDATED
```

---

# 8. 全局 Cancel / Reconcile 收尾

这是 A2E Seal 前必须做的架构检查。

当前最终保存 HUD SHA-256 `990125C9...` 已完成本节接线、Compile/Save、重载核对
和一次独立架构审查。审查最初发现的 cleanup 回环、尾部断链、Damage 默认值反向、
Hand discard 未恢复四个 P1 均已修复；定向复审无剩余 P0/P1。

所有已经直接覆盖 HUD 控件的 Record 都要检查取消后不会留下“未来值”。

## 8.1 Card transient

Cancel：

```text
恢复 HiddenHandCardWidget
移除 PlayedCardWidget
清引用
```

---

## 8.2 Damage

Cancel 必须保证：

```text
Txt_DamagePresentation Collapsed
Player / Enemy RenderOpacity = 1
HP / Block 视觉回到当前历史 ViewModel
```

不能留下当前未完成 Damage 的 `HPAfter / BlockAfter`。

推荐最终把 Player/Enemy vitals 刷新封装成可复用函数，Cancel 时从 ViewModel 重画。

---

## 8.3 BlockChanged

Cancel 后 Block 必须恢复当前历史 ViewModel.Block。

---

## 8.4 StatusChanged

统一：

```text
RebuildStatusIcons(ViewModel.Player.Statuses)
RebuildStatusIcons(ViewModel.Enemy.Statuses)
```

---

## 8.5 EnergyChanged

Cancel 后 Energy 文本/条恢复当前历史 ViewModel Energy。

---

## 8.6 CardZoneChanged / DeckShuffled

Cancel 后：

```text
Hand / pile counts / zone visuals
```

必须恢复到当前历史 ViewModel，而不是保留尚未 reducer 的目标值。

---

## 8.7 Terminal

Cancel/stale token 不得留下错误终局 overlay。

---

## 8.8 Widget lost / ViewModel changed

结合 C++ 已有：

```text
HandleViewModelChanged
Widget lost
exact token ownership
```

检查 Blueprint Cancel 的视觉恢复与 C++ cancellation 不冲突。

核心验收：

```text
Cancel
≠ Notify
Cancel
≠ mutate ViewModel
Cancel
= 回到“已完成历史事实”对应的视觉状态
```

---

# 9. UI-A2E PIE 全链验收

Scenario A-E 已在真实 PIE 中通过。额外的 Editor-only Automation PIE 使用正式
`ViewModel->RequestEndTurn()` 产生真实 Envelope，在 Controller 已持有有效 active
token 后调用正式 `WidgetInstance->SkipPresentation()`，验证了 Cancel 无正常 Notify、
stale token 拒绝、FinalSnapshot reconcile、后续真实请求正常完成，以及 catch-up 后
Idle/input unlock。临时 harness 已删除，标准 Editor build 成功且 Source 无 diff。

所有单切片通过后，运行完整真实 WBP/PIE 场景。

## 9.1 Scenario A：普通攻击卡

```text
选择 Strike
→ Enemy
→ CardPlayed
→ Damage
→ CardZoneChanged
→ FinalSnapshot
→ input unlock
```

检查：

```text
不提前跳 FinalSnapshot
Energy 卡费只表现一次
Record 顺序正确
最终 Idle
```

---

## 9.2 Scenario B：Damage + Status

Uppercut-like：

```text
CardPlayed
→ Damage
→ Weak Applied
→ Vulnerable Applied
→ CardZoneChanged
```

再施加一次：

```text
Weak / Vulnerable Update
→ 精确 runtime row 更新
→ 不重复创建
```

再覆盖 Reduction / Removal。

---

## 9.3 Scenario C：完整 EndTurn Macro Envelope

按项目实际配置覆盖：

```text
EnergyChanged end-turn clear
→ Hand discard CardZoneChanged Records
→ StatusChanged TurnEndDecay
→ Enemy Block clear
→ Enemy Damage
→ EnergyChanged player-turn restore
→ Player Block clear
→ DeckShuffled（如触发）
→ DrawPile -> Hand CardZoneChanged Records
```

验收：

```text
一个 Envelope 中多 Record 类型严格按 producer 顺序播放
每个 Record 精确 Token 完成后才进入下一个
ViewModel 始终只代表已完成历史事实
```

---

## 9.4 Scenario D：Victory + Defeat

确认：

```text
lethal fact 可见
→ terminal record
→ terminal reducer
→ final terminal state
```

Victory/Defeat 都必须各自真实跑一次。

---

## 9.5 Scenario E：ResolutionFault / PresentationUnavailable

分别跑：

```text
真实 framework ResolutionFault
```

以及：

```text
Presentation-only playback failure
```

检查两者 UI/ownership 完全分离。

---

# 10. Input Unlock 最终验收

A2E 结束的标准不是“按钮最后能点”。

必须满足：

```text
Gameplay 可能已稳定
↓
Presentation backlog 仍存在
↓
正常输入保持 locked
↓
最后一个 Record callback / safe fallback
↓
Apply Envelope.FinalSnapshot
↓
Controller catch up 到最新 display revision
↓
RefreshLiveInputBindingsIfCaughtUp
↓
Gameplay Query/Request 正式 eligible
↓
正常输入 unlock
```

Playback 期间重复：

```text
打牌
EndTurn
Target submit
```

不得启动第二个玩家请求。

---

# 11. A2E 最终 Seal 检查表

只有以下全部成立才能封闭 A2E：

```text
[x] CardPlayed validated
[x] Damage validated
[x] BlockChanged validated
[x] CardZoneChanged 所有当前可见必要转区 validated
[x] StatusChanged creation validated
[x] StatusChanged update/reduction validated
[x] StatusChanged removal validated
[x] EnergyChanged validated
[x] DeckShuffled validated
[x] Victory validated
[x] Defeat validated
[x] ResolutionFault validated

[x] Blueprint 只消费 frozen Record
[x] Blueprint 不查询 mutable historical Gameplay
[x] Blueprint 不修改 authoritative ViewModel truth
[x] 所有异步接管都只完成精确 Token
[x] invalid/unsupported case 正确 Return false fallback
[x] Cancel 不 Notify
[x] Cancel 后无 future visual 残留
[x] producer Record order 保持
[x] WorkingSnapshot 只在 Record 完成后推进
[x] Envelope.FinalSnapshot 精确 reconcile
[x] input 仅在 Controller catch-up + live binding refresh 后解锁
[x] PresentationUnavailable 与 ResolutionFault 分离

[x] Scenario A 普通卡 PIE PASS
[x] Scenario B 状态卡 PIE PASS
[x] Scenario C EndTurn macro PIE PASS
[x] Scenario D Victory/Defeat PIE PASS
[x] Scenario E ResolutionFault/PresentationUnavailable PIE PASS
```

全部通过后：

```text
UI-A2E Unified Blueprint Playback = COMPLETE / SEALED
UI-A2 = COMPLETE / SEALED
```

然后才允许继续：

```text
A3-1 Dynamic Text SEALED
↓
A3-2 Target-Specific Current-State Preview
↓
A3-3 Energy + Target-Aware Legality
↓
A3-4 ViewModel Transient Preview Lifecycle
↓
A3-5 Minimal UMG + A2/A3 Combined PIE
```

---

# 12. 后续执行顺序速查

后续每次只推进一个可验收切片：

```text
① PlayStatusChangedPresentation 支持 ExistingStatusWidget
↓
② StatusChanged Router 支持 update/reduction
↓
③ StatusChanged Cancel 恢复历史状态列表
↓
④ PIE Increase + Reduction
↓
⑤ StatusChanged Removal
↓
⑥ PIE Removal
↓
⑦ EnergyChanged
↓
⑧ PIE Energy / EndTurn energy
↓
⑨ CardZoneChanged 剩余 Hand/Draw 转区
↓
⑩ DeckShuffled
↓
⑪ 完整 EndTurn PIE
↓
⑫ Victory / Defeat / ResolutionFault
↓
⑬ Terminal PIE + PresentationUnavailable separation
↓
⑭ 全局 Cancel / Reconcile hardening
↓
⑮ 全 A2E Scenario A-E PIE
↓
⑯ 更新 Validation Log / Snapshot / Roadmap
↓
⑰ UI-A2E COMPLETE / SEALED
↓
⑱ UI-A2 COMPLETE / SEALED
```

原则：**前一切片没有完成 Compile + Save + 必要 PIE 验收，不进入下一切片。**
