# UI-A2E 详细实施步骤（逐节记录）

日期：**2026-08-30**

用途：从当前 UI-A2E 进度开始，按实际 Blueprint 操作顺序逐节记录到 **UI-A2E COMPLETE / SEALED**。本文不是概要路线图，而是可直接照着 UE5.8 Blueprint 编辑器操作的详细施工手册。

记录规则：

```text
每次只记录一节
→ 当前节完成/确认后
→ 用户回复“下一节”
→ 再追加下一节详细步骤
→ 直到 UI-A2E 全部实施、PIE 验收和文档收口完成
```

当前正式验证基线：

```text
CardPlayed              VALIDATED
Damage                  VALIDATED
BlockChanged            VALIDATED
CardZoneChanged         VALIDATED（PlayArea -> Destination）
StatusChanged creation  VALIDATED
```

当前已经具备的 Status 前置结构：

```text
WBP_BattleStatus
- CurrentStatusView : FBattleHUDStatusView
- SetStatusView(InStatusView) 会先保存 CurrentStatusView

WBP_BattleHUD
- FindStatusWidgetByIdentity(
    TargetPresentationId,
    StatusId,
    RuntimeSequence
  )
- 精确身份：TargetPresentationId + StatusId + RuntimeSequence
```

---

# 第一节：让 `PlayStatusChangedPresentation` 同时支持 Creation 与 Update/Reduction

## 1.1 本节目标

当前 `PlayStatusChangedPresentation` 只服务于：

```text
StatusChanged
bCreated = true
bRemoved = false
```

也就是首次创建状态，例如：

```text
Enemy 原本没有 Weak
→ StatusChanged
→ Weak 2
```

本节要把它扩展为：

```text
bCreated = true
→ 继续使用现有 Creation 路径
→ 创建新的 WBP_BattleStatus

bCreated = false
→ 使用 Router 之后传入的 ExistingStatusWidget
→ 更新这个已经存在的 WBP_BattleStatus
→ 不创建第二个状态图标
```

本节只修改 `PlayStatusChangedPresentation` 本身。

**本节不要修改：**

```text
StatusChanged Router
CancelPresentationRecordPlayback
FinishPresentationRecord
Removal 路径
EnergyChanged
```

这些会在后续章节分别处理。

---

## 1.2 修改前确认

打开：

```text
WBP_BattleHUD
```

在 `My Blueprint` 中找到当前用于状态表现的 Custom Event：

```text
PlayStatusChangedPresentation
```

修改前应至少有两个输入：

```text
StatusChanged : FStatusChangedPresentationPayload
Token         : FPresentationPlaybackToken
```

并且现有 Creation 流程大致为：

```text
PlayStatusChangedPresentation(StatusChanged, Token)
→ ActivePresentationToken = Token
→ ActivePresentationType = StatusChanged
→ Break Status Changed Presentation Payload
→ Make Presentation Status View
→ Create Widget WBP_BattleStatus
→ ActiveStatusPresentationWidget = Created Widget
→ CreatedWidget.SetStatusView(StatusView)
→ 判断目标 Player / Enemy
→ Add Child 到 WB_PlayerStatuses / WB_EnemyStatuses
→ StartPresentationFinishTimer
```

这条 Creation 路径已经通过 PIE，后续修改时不要破坏它。

---

## 1.3 给 Custom Event 新增 `ExistingStatusWidget` 输入

### 操作

1. 在 Event Graph 中点击：

```text
PlayStatusChangedPresentation
```

2. 在右侧 `Details` 面板找到：

```text
Inputs
```

3. 点击 `+` 新增一个输入。

4. 名称填写：

```text
ExistingStatusWidget
```

5. 类型选择：

```text
WBP_BattleStatus
Object Reference
```

注意必须是：

```text
WBP_BattleStatus Object Reference
```

不要选成：

```text
Class Reference
Soft Object Reference
UserWidget Object Reference
```

### 修改后的 Event 输入

最终 Custom Event 顶部应有：

```text
PlayStatusChangedPresentation
├─ StatusChanged
├─ Token
└─ ExistingStatusWidget
```

其中 `ExistingStatusWidget` 的用途固定为：

```text
Creation：None
Update/Reduction：FindStatusWidgetByIdentity 找到的精确 Widget
```

---

## 1.4 保持 Token 与 ActivePresentationType 的现有执行顺序

不要移动现有前两步。

执行白线应继续保持：

```text
PlayStatusChangedPresentation
↓
Set ActivePresentationToken = Token
↓
Set ActivePresentationType = StatusChanged
```

数据线：

```text
Event.Token
→ Set ActivePresentationToken.Value
```

`ActivePresentationType` 设置为：

```text
StatusChanged
```

原因：从 Blueprint 正式开始接管这个 Record 起，它就必须拥有当前精确 Token 和 Record 类型，后面的 timer / Finish / Cancel 都依赖这两个变量。

不要改成：

```text
先改 Widget
最后才 Set Token
```

也不要在 Update 路径中额外创建第二份 Token 状态。

---

## 1.5 保留一个 `Break Status Changed Presentation Payload`

从 `StatusChanged` 输入继续使用已有：

```text
Break Status Changed Presentation Payload
```

如果当前节点已经存在，直接保留，不需要再放第二个 Break。

本节后面至少会使用这些字段：

```text
TargetPresentationId
bCreated
bRemoved（本节不处理，但不要删）
```

以及 `Make Presentation Status View` 会直接消费完整 `StatusChanged` Payload。

建议：

```text
一个 Break 节点
+ 普通 Reroute Node 整理数据线
```

不要为了整理线条把整个 Payload `Promote to Variable`。

---

## 1.6 保留并复用 `Make Presentation Status View`

当前已有 C++ BlueprintPure helper：

```text
Make Presentation Status View
```

输入：

```text
StatusChanged
```

输出：

```text
FBattleHUDStatusView
```

这个输出已经包含冻结 Record 的：

```text
StatusId
RuntimeSequence
DisplayName
DescriptionAfter
AmountAfter
bUseAtlasIcon
UVOffset
UVScale
TrimOffset
TrimScale
```

### 本节接法

保留：

```text
Event.StatusChanged
→ Make Presentation Status View.StatusChanged
```

从 Return Value 拉一个普通 Reroute Node，作为统一的：

```text
FrozenStatusView
```

然后：

```text
FrozenStatusView
├─ Creation 的 SetStatusView
└─ Update/Reduction 的 SetStatusView
```

这样 Creation 和 Update 使用的是同一份冻结 Record DTO。

### 禁止

不要在 Update 路径做：

```text
AmountBefore + Delta
CurrentStatusView.Amount + 某个值
ViewModel.Statuses 中重新取 Amount
```

最终显示值必须直接使用 helper 已经生成的 `AmountAfter`。

---

## 1.7 在公共前半段加入 `Branch(bCreated)`

当前：

```text
Set ActivePresentationType = StatusChanged
```

后面原本直接进入 Creation。

现在插入：

```text
Branch
```

### Condition

从：

```text
Break Status Changed Presentation Payload
→ bCreated
```

连接到：

```text
Branch.Condition
```

形成：

```text
Set ActivePresentationType = StatusChanged
↓
Branch(bCreated)
├─ True  = Creation
└─ False = Update / Reduction
```

注意：本节的 `False` 暂时统一代表 Update/Reduction。

`bRemoved=true` 的正式 Removal 分流将在 Router 的后续章节完成，因此本节不要在这里设计 Removal。

---

## 1.8 `True` 分支：重新接回原 Creation 路径

`Branch.True` 必须接回原来已经验证通过的：

```text
Create Widget WBP_BattleStatus
```

不要重建整条 Creation 图，只需要把原先进入 `Create Widget` 的白线改接到：

```text
Branch.True
```

### Creation 执行链保持

```text
Branch.True
↓
Create Widget WBP_BattleStatus
↓
Set ActiveStatusPresentationWidget
↓
SetStatusView
↓
Player / Enemy 分流
↓
Add Child
↓
StartPresentationFinishTimer
```

### `Create Widget`

Class：

```text
WBP_BattleStatus
```

Owning Player：

```text
Get Owning Player
```

如果当前 Creation 路径已经存在 `Cast To WBP_BattleStatus`，保持现状即可；不要为了这一节额外重构。

### `ActiveStatusPresentationWidget`

Creation 中应继续设置：

```text
ActiveStatusPresentationWidget
= 新创建的 WBP_BattleStatus
```

这个引用用于当前活动状态表现的 Finish / Cancel 生命周期。

### Creation 的 `SetStatusView`

Target：

```text
新创建的 WBP_BattleStatus
```

InStatusView：

```text
FrozenStatusView
```

即：

```text
Make Presentation Status View.ReturnValue
→ WBP_BattleStatus.SetStatusView.InStatusView
```

### Player / Enemy 容器

继续沿用已经验证过的逻辑：

```text
TargetPresentationId == ViewModel.Player.PresentationId
```

Player：

```text
WB_PlayerStatuses.AddChild(ActiveStatusPresentationWidget)
```

Enemy：

```text
WB_EnemyStatuses.AddChild(ActiveStatusPresentationWidget)
```

本节不要修改 Target 判断策略。

### Creation 最后

继续：

```text
StartPresentationFinishTimer
```

当前公共时长保持：

```text
0.5 s
Looping = false
```

---

## 1.9 `False` 分支：建立 Update / Reduction 路径

这是本节新增的核心。

从：

```text
Branch(bCreated).False
```

拉白色执行线，放置：

```text
Set ActiveStatusPresentationWidget
```

### Value

连接：

```text
Event.ExistingStatusWidget
→ Set ActiveStatusPresentationWidget.Value
```

因此：

```text
bCreated = false
↓
ActiveStatusPresentationWidget = ExistingStatusWidget
```

此处的 `ExistingStatusWidget` 将来由 Router 调：

```text
FindStatusWidgetByIdentity
```

后传进来。

本节暂时只准备接收它。

---

## 1.10 Update 路径调用 `ExistingStatusWidget.SetStatusView`

从 Event 输入：

```text
ExistingStatusWidget
```

蓝色 pin 拖出，搜索：

```text
Set Status View
```

选择属于 `WBP_BattleStatus` 的函数。

节点应为：

```text
SetStatusView
Target       : WBP_BattleStatus
InStatusView : FBattleHUDStatusView
```

### Target

连接：

```text
ExistingStatusWidget
→ SetStatusView.Target
```

### InStatusView

连接：

```text
FrozenStatusView
→ SetStatusView.InStatusView
```

### 执行线

连接：

```text
Set ActiveStatusPresentationWidget
↓
ExistingStatusWidget.SetStatusView
```

最终 Update 核心：

```text
ExistingStatusWidget
      │
      ├→ ActiveStatusPresentationWidget
      │
      └→ SetStatusView.Target

FrozenStatusView
      └→ SetStatusView.InStatusView
```

---

## 1.11 Update 路径绝对不要创建或重新 Add Child

Update / Reduction 路径中**不允许出现**：

```text
Create Widget WBP_BattleStatus
Add Child To Wrap Box
Remove From Parent
```

原因：

```text
ExistingStatusWidget
```

本身已经是：

```text
WB_PlayerStatuses
或
WB_EnemyStatuses
```

中的正式状态行。

例如：

```text
Weak 2
```

收到更新 Record：

```text
AmountBefore = 2
AmountAfter = 4
bCreated = false
bRemoved = false
```

正确做法是：

```text
同一个 Weak Widget
SetStatusView(Weak 4)
```

不是：

```text
旧 Weak 2
+ 新建 Weak 4
```

否则会出现重复状态图标。

---

## 1.12 Update 路径最后启动公共 Finish Timer

从：

```text
ExistingStatusWidget.SetStatusView
```

的白色执行输出连接：

```text
StartPresentationFinishTimer
```

因此 Update/Reduction 完整执行链应为：

```text
Branch.False
↓
Set ActiveStatusPresentationWidget = ExistingStatusWidget
↓
ExistingStatusWidget.SetStatusView(FrozenStatusView)
↓
StartPresentationFinishTimer
```

不要在 Timer 之前：

```text
NotifyPresentationFinished
RebuildStatusIcons
修改 ViewModel
```

这些都不属于当前活动 Record 的 Blueprint playback。

---

## 1.13 本节完成后的完整逻辑图

完成后 `PlayStatusChangedPresentation` 应可抽象为：

```text
PlayStatusChangedPresentation(
    StatusChanged,
    Token,
    ExistingStatusWidget
)
↓
Set ActivePresentationToken = Token
↓
Set ActivePresentationType = StatusChanged
↓
Break StatusChanged Presentation Payload
↓
Make Presentation Status View(StatusChanged)
↓
Branch(bCreated)

├─ True：Creation
│    ↓
│  Create Widget WBP_BattleStatus
│    ↓
│  ActiveStatusPresentationWidget = CreatedWidget
│    ↓
│  CreatedWidget.SetStatusView(FrozenStatusView)
│    ↓
│  Target == Player ?
│    ├─ true  → WB_PlayerStatuses.AddChild
│    └─ false → WB_EnemyStatuses.AddChild
│    ↓
│  StartPresentationFinishTimer
│
└─ False：Update / Reduction
     ↓
   ActiveStatusPresentationWidget = ExistingStatusWidget
     ↓
   ExistingStatusWidget.SetStatusView(FrozenStatusView)
     ↓
   StartPresentationFinishTimer
```

---

## 1.14 数据来源检查

本节完成后，Creation 与 Update 都必须满足：

```text
StatusId        ← Record.StatusId
RuntimeSequence ← Record.RuntimeSequence
DisplayName     ← Record.DisplayName
Description     ← Record.DescriptionAfter
Amount          ← Record.AmountAfter
Icon metadata   ← Record frozen atlas fields
```

这些数据统一由：

```text
Make Presentation Status View(StatusChanged)
```

生成。

Blueprint 不应读取：

```text
UStatusInstance
UStatusData
当前 Gameplay Combatant status object
```

来重建当前 Record 的视觉内容。

---

## 1.15 本节禁止改动的内容

为了控制变量，本节不要顺手修改：

```text
FindStatusWidgetByIdentity
StatusChanged Router
CancelPresentationRecordPlayback
FinishPresentationRecord
NotifyPresentationRecordFinished
RebuildStatusIcons
Removal
EnergyChanged
```

特别是不要因为 Update 现在会修改正式 Widget，就提前在本节乱改 Cancel。

Cancel 会在独立章节集中处理，否则很难判断错误来自 playback 还是 cancellation。

---

## 1.16 Compile 顺序

本节主要修改 `WBP_BattleHUD`。

但因为 HUD 依赖 `WBP_BattleStatus.SetStatusView` 和 `CurrentStatusView`，推荐顺序固定为：

```text
1. 打开 WBP_BattleStatus
2. Compile
3. 确认 0 Errors
4. Save
5. 打开 WBP_BattleHUD
6. Compile
7. 确认 0 Errors
8. Save
```

如果 HUD 再出现：

```text
Could not find a variable named "CurrentStatusView"
```

处理顺序：

```text
先重新 Compile + Save WBP_BattleStatus
→ 回 HUD
→ 对 Get CurrentStatusView 执行 Refresh Node
→ 若仍失败，删除该 Get 节点
→ 从 Cast To WBP_BattleStatus 的 As WBP_BattleStatus pin 重新拉 Get CurrentStatusView
→ 再 Compile HUD
```

不要拆掉已经验证正确的 `FindStatusWidgetByIdentity` 循环结构。

---

## 1.17 本节结构验收清单

本节结束前逐项检查：

```text
[ ] PlayStatusChangedPresentation 有 ExistingStatusWidget 输入
[ ] ExistingStatusWidget 类型是 WBP_BattleStatus Object Reference
[ ] ActivePresentationToken 仍由 Token 设置
[ ] ActivePresentationType 仍设为 StatusChanged
[ ] 使用一个冻结 MakePresentationStatusView 输出
[ ] bCreated 进入 Branch
[ ] True 仍走原 Creation 路径
[ ] Creation 仍 Create WBP_BattleStatus
[ ] Creation 仍 AddChild 到正确 Player / Enemy WrapBox
[ ] Creation 仍 StartPresentationFinishTimer
[ ] False 不 Create Widget
[ ] False 把 ExistingStatusWidget 保存为 ActiveStatusPresentationWidget
[ ] False 调 ExistingStatusWidget.SetStatusView
[ ] False 的 InStatusView 使用 FrozenStatusView
[ ] False 不 AddChild
[ ] False 最后 StartPresentationFinishTimer
[ ] 没有修改 ViewModel.Statuses
[ ] 没有自己计算 AmountAfter
[ ] WBP_BattleStatus Compile 0 Errors
[ ] WBP_BattleHUD Compile 0 Errors
```

---

## 1.18 本节完成判定

本节只要求：

```text
Blueprint 结构完成
+ Compile 0 Errors
+ Save
```

**本节不做 Update PIE。**

原因是 Router 还没有把 `bCreated=false` 的 Record 正式送入这条路径。

因此本节结束时的状态应记为：

```text
PlayStatusChangedPresentation
Creation path             WIRED / previously VALIDATED
Update/Reduction path     WIRED / NOT ROUTED YET
StatusChanged Router      creation-only
PIE update/reduction      NOT RUN
```

下一节才处理：

```text
StatusChanged Router
→ 区分 creation / update-reduction / removal
→ update-reduction 调 FindStatusWidgetByIdentity
→ Found 才把 ExistingStatusWidget 传给 PlayStatusChangedPresentation
```

---

# 第二节：重构 `StatusChanged Router`，正式接通 Update / Reduction

## 2.1 本节目标

第一节只是让：

```text
PlayStatusChangedPresentation
```

具备了两个内部播放能力：

```text
Creation
Update / Reduction
```

但 `BeginPresentationRecordPlayback` 中的 `StatusChanged` Router 仍然只允许：

```text
bCreated = true
bRemoved = false
```

进入 Blueprint async playback。

所以当前真实运行时仍是：

```text
StatusChanged creation
→ Blueprint async

StatusChanged update / increase / reduction
→ Router false
→ C++ immediate fallback
```

本节要把 Router 改成三类明确分流：

```text
Creation
→ Blueprint async

Update / Reduction
→ 先按精确身份找到已有 WBP_BattleStatus
→ 找到后 Blueprint async

Removal
→ 本节暂时仍 Return false
→ 下一阶段再实现
```

本节结束时，不做 Cancel 重构，也不做 PIE；只要求 Router 正式接通且 Blueprint Compile 0 Errors。

---

## 2.2 修改位置

打开：

```text
WBP_BattleHUD
```

进入承接 C++ 播放请求的 Blueprint override / function：

```text
Play Presentation Record
```

其底层 Blueprint 名称对应：

```text
BeginPresentationRecordPlayback
```

找到：

```text
Switch on EBattlePresentationRecordType
```

然后找到分支：

```text
StatusChanged
```

本节只修改这个 `StatusChanged` case。

不要改：

```text
CardPlayed
Damage
BlockChanged
CardZoneChanged
EnergyChanged
DeckShuffled
Victory
Defeat
ResolutionFault
```

---

## 2.3 修改前 Router 应有的结构

当前 creation-only 逻辑大致是：

```text
Switch.StatusChanged
↓
Break Status Changed Presentation Payload
↓
TargetPresentationId == Player.PresentationId
TargetPresentationId == Enemy.PresentationId
↓
OR
↓
TargetKnown

bCreated
AND
NOT bRemoved
↓
IsCreated

TargetKnown
AND
IsCreated
↓
Branch(IsValidCreation)

├ false
│  → Return false
│
└ true
   → PlayStatusChangedPresentation(
       Record.StatusChanged,
       Token
     )
   → Return true
```

第一节以后，调用节点可能已经自动多出：

```text
ExistingStatusWidget
```

对象输入 pin。

如果 Creation 调用节点的这个 pin 目前为空，这是正确的。

---

## 2.4 保留现有 `Break Status Changed Presentation Payload`

不要重新拆一份 Payload。

继续使用当前：

```text
Record.StatusChanged
→ Break Status Changed Presentation Payload
```

本节需要至少显示这些字段：

```text
TargetPresentationId : Name
StatusId             : Name
RuntimeSequence      : Integer64
bCreated             : Boolean
bRemoved             : Boolean
```

如果 Break 节点现在只展开了部分 pin：

1. 点击 `Break Status Changed Presentation Payload`。
2. 展开高级/隐藏 pin，或使用节点上的 pin 选项。
3. 保证上面五个字段都可以拉线。
4. 其他字段不需要为了本节全部显示。

不要把整个 Payload `Promote to Variable`。

本节应尽量继续用：

```text
Break 节点
+ 普通 Reroute Node
```

整理长数据线。

---

## 2.5 现有 Player / Enemy 目标验证保持不变

当前 Router 已经验证过：

```text
TargetPresentationId
```

必须属于当前：

```text
ViewModel.Player.PresentationId
或
ViewModel.Enemy.PresentationId
```

这部分逻辑不要重写。

如果当前使用：

```text
Name → String
→ Equal Exactly (String)
```

就继续使用。

如果你的当前保存版本已经稳定使用：

```text
Equal (Name)
```

也不要为了这一节再转换。

最终只需要保留一个 Boolean：

```text
TargetKnown
```

其语义必须是：

```text
TargetKnown
= IsPlayerTarget OR IsEnemyTarget
```

---

## 2.6 把旧的 `IsValidCreation` 合并判断拆开

旧 Router 可能存在类似：

```text
bCreated
AND
NOT bRemoved
→ IsCreated

TargetKnown
AND
IsCreated
→ IsValidCreation
→ Branch
```

这套结构只适合 creation-only。

本节建议不要继续在这个 Boolean AND 链上硬加条件，而是改成清晰的执行树：

```text
TargetKnown?
↓
bRemoved?
↓
bCreated?
↓
Creation 或 Update
```

这样后面 Removal 接入时不会再次拆 Router。

### 操作原则

可以删除或断开旧的：

```text
NOT bRemoved
bCreated AND !bRemoved
TargetKnown AND IsCreated
```

但**不要删除**：

```text
Player target comparison
Enemy target comparison
TargetKnown 的 OR
Break StatusChanged Payload
原 Creation 调用节点
```

---

## 2.7 第一层 Branch：验证 `TargetKnown`

从：

```text
Switch.StatusChanged
```

的白色执行输出接到一个新的：

```text
Branch
```

Condition 连接：

```text
TargetKnown
```

也就是：

```text
Switch.StatusChanged
↓
Branch(TargetKnown)
```

### False

`False` 必须：

```text
Return false
```

因为未知目标时 Blueprint 不能安全接管表现。

推荐新建一个独立的 Return Node：

```text
Return
Found/ReturnValue = false
```

具体输出名称以当前函数为准；`BeginPresentationRecordPlayback` 的函数返回值是 Boolean，所以核心是：

```text
Return false
```

不要在未知目标时：

```text
PlayStatusChangedPresentation
```

也不要自行映射到 Enemy。

### True

继续进入第二层：

```text
bRemoved?
```

---

## 2.8 第二层 Branch：先隔离 Removal

从：

```text
Branch(TargetKnown).True
```

接第二个：

```text
Branch
```

Condition：

```text
BreakStatusChanged.bRemoved
```

结构：

```text
TargetKnown = true
↓
Branch(bRemoved)
```

### True：Removal

本节**不要实现 Removal**。

直接：

```text
Return false
```

原因：

```text
bRemoved = true
```

的视觉策略和 Update 不同，它需要对精确状态行做临时隐藏/移除表现，并且 Cancel 要能恢复。

如果在本节把 Removal 错送进 Update 路径，`MakePresentationStatusView` 会生成 `AmountAfter = 0` 的 View，但 Widget 仍存在，语义不等价于“状态被移除”。

因此本节锁定：

```text
bRemoved = true
→ Return false
→ C++ immediate fallback
```

### False

只有：

```text
bRemoved = false
```

才进入下一层：

```text
bCreated?
```

---

## 2.9 第三层 Branch：区分 Creation 与 Update / Reduction

从：

```text
Branch(bRemoved).False
```

接第三个：

```text
Branch
```

Condition：

```text
BreakStatusChanged.bCreated
```

最终：

```text
TargetKnown = true
AND bRemoved = false
↓
Branch(bCreated)

├ true  → Creation
└ false → Update / Reduction
```

这里的 `false` 就是本节要新接通的更新路径。

---

## 2.10 Creation 分支：调用原 `PlayStatusChangedPresentation`

`Branch(bCreated).True` 接回已经存在的 Creation 调用节点：

```text
PlayStatusChangedPresentation
```

调用参数必须是：

```text
StatusChanged
← Record.StatusChanged

Token
← BeginPresentationRecordPlayback Function Entry.Token

ExistingStatusWidget
← None
```

### `ExistingStatusWidget = None` 怎么做

Creation 不需要已有状态 Widget。

因此 Creation 调用节点上的：

```text
ExistingStatusWidget
```

对象 pin **保持未连接** 即可。

其默认值就是：

```text
None
```

不要：

```text
创建一个假 WBP_BattleStatus 再传进去
```

也不要把：

```text
ActiveStatusPresentationWidget
```

旧值传进去。

Creation 的新 Widget 仍由 `PlayStatusChangedPresentation` 内部自己创建。

---

## 2.11 Creation 调用后必须 `Return true`

Creation 调用的白色执行输出继续接：

```text
Return true
```

语义：

```text
Blueprint 已成功开始 0.5s async playback
→ 告诉 C++ 当前 Record 已被 Blueprint 接管
```

因此：

```text
PlayStatusChangedPresentation
↓
Return true
```

不要：

```text
调用后 Return false
```

否则会出现 Blueprint 已经开始 timer，而 C++ 又立即 fallback 的双重推进风险。

---

## 2.12 Update / Reduction 分支：放置 `FindStatusWidgetByIdentity`

从：

```text
Branch(bCreated).False
```

拉白色执行线。

搜索并放置函数：

```text
FindStatusWidgetByIdentity
```

这个函数已经在前一阶段完成并检查过，其输入是：

```text
TargetPresentationId
StatusId
RuntimeSequence
```

输出是：

```text
Found
StatusWidget
```

本节不要修改这个函数内部。

---

## 2.13 给 `FindStatusWidgetByIdentity` 接三个精确身份字段

### TargetPresentationId

连接：

```text
Break Status Changed Presentation Payload.TargetPresentationId
→ FindStatusWidgetByIdentity.TargetPresentationId
```

### StatusId

连接：

```text
Break Status Changed Presentation Payload.StatusId
→ FindStatusWidgetByIdentity.StatusId
```

### RuntimeSequence

连接：

```text
Break Status Changed Presentation Payload.RuntimeSequence
→ FindStatusWidgetByIdentity.RuntimeSequence
```

注意：

```text
RuntimeSequence
```

必须保持：

```text
Integer64
```

不要转成普通 `Integer`。

最终精确身份必须完整是：

```text
TargetPresentationId
+ StatusId
+ RuntimeSequence
```

不能省略任何一个。

---

## 2.14 为什么不能只找 `StatusId`

不要把 Update 简化为：

```text
Find Weak
→ 更新第一个 Weak
```

正式身份是：

```text
TargetPresentationId
+ StatusId
+ RuntimeSequence
```

例如未来同一角色上存在两个逻辑上不同的 runtime status instance，即使它们共享：

```text
StatusId = Weak
```

也不能通过数组顺序或第一个匹配项决定历史 Record 应更新谁。

本节 Router 只把 Record 的正式 identity 原样传给已经完成的查找函数。

---

## 2.15 在 Find 后新增 `Branch(Found)`

从：

```text
FindStatusWidgetByIdentity
```

的白色执行输出接：

```text
Branch
```

Condition：

```text
FindStatusWidgetByIdentity.Found
```

形成：

```text
FindStatusWidgetByIdentity(...)
↓
Branch(Found)
```

### False

必须：

```text
Return false
```

### True

才允许：

```text
PlayStatusChangedPresentation
```

---

## 2.16 `Found = false` 时为什么必须 fallback

如果 Record 说：

```text
Weak RuntimeSequence = 17
AmountBefore = 2
AmountAfter = 4
```

但当前历史 HUD 中找不到：

```text
Target + Weak + RuntimeSequence 17
```

Blueprint 没有资格：

```text
随便找另一个 Weak
创建一个新的 Weak
假装播放成功
```

所以：

```text
Found = false
→ Return false
```

这会把该 Record 交还 C++ immediate fallback / reconciliation 机制，而不是让 Blueprint 构造错误历史视觉。

这是 A2E 的 fail-safe，不是错误。

---

## 2.17 `Found = true` 时调用 Update 路径

从：

```text
Branch(Found).True
```

放置或复制一个：

```text
PlayStatusChangedPresentation
```

参数接法：

### StatusChanged

```text
Record.StatusChanged
→ PlayStatusChangedPresentation.StatusChanged
```

最好直接从：

```text
Break Presentation Record
或当前 Record.StatusChanged struct pin
```

拉完整 Payload。

不要自己重新 `Make StatusChanged Payload`。

### Token

连接：

```text
Function Entry.Token
→ PlayStatusChangedPresentation.Token
```

这个 Token 必须是 Controller 传给当前 Record 的原 Token。

不要使用：

```text
ActivePresentationToken 的旧值
默认 Token
重新 Make Token
```

### ExistingStatusWidget

连接：

```text
FindStatusWidgetByIdentity.StatusWidget
→ PlayStatusChangedPresentation.ExistingStatusWidget
```

这是本节最关键的新数据线。

因此 Update 调用完整是：

```text
PlayStatusChangedPresentation(
    StatusChanged = Record.StatusChanged,
    Token = FunctionEntry.Token,
    ExistingStatusWidget = FoundStatusWidget
)
```

---

## 2.18 Update 调用后必须 `Return true`

与 Creation 相同：

```text
PlayStatusChangedPresentation
↓
Return true
```

原因：第一节中 Update 路径会：

```text
保存 ActivePresentationToken
→ 更新 ExistingStatusWidget
→ 启动 0.5s timer
```

所以此时 Blueprint 已经真正接管异步 Record。

必须返回：

```text
true
```

否则 C++ 会认为 Blueprint 没有接管。

---

## 2.19 推荐的最终 Router 执行结构

本节结束后，`StatusChanged` case 应接近：

```text
Switch on EBattlePresentationRecordType
└─ StatusChanged
    ↓
  Break Status Changed Presentation Payload
    ↓
  IsPlayerTarget
  IsEnemyTarget
    ↓
  OR = TargetKnown
    ↓
  Branch(TargetKnown)

  ├─ false
  │    ↓
  │  Return false
  │
  └─ true
       ↓
     Branch(bRemoved)

     ├─ true
     │    ↓
     │  Return false
     │  // Removal 仍未接管
     │
     └─ false
          ↓
        Branch(bCreated)

        ├─ true：Creation
        │    ↓
        │  PlayStatusChangedPresentation(
        │      Record.StatusChanged,
        │      FunctionEntry.Token,
        │      None
        │  )
        │    ↓
        │  Return true
        │
        └─ false：Update / Reduction
             ↓
           FindStatusWidgetByIdentity(
               TargetPresentationId,
               StatusId,
               RuntimeSequence
           )
             ↓
           Branch(Found)

           ├─ false
           │    ↓
           │  Return false
           │
           └─ true
                ↓
              PlayStatusChangedPresentation(
                  Record.StatusChanged,
                  FunctionEntry.Token,
                  FoundStatusWidget
              )
                ↓
              Return true
```

---

## 2.20 推荐的布局方式

为了后续 Removal 继续扩展，建议把三个阶段从左到右排开：

```text
[目标验证]
TargetKnown
      ↓
[bRemoved]
      ↓
[bCreated]
      ↓
[Creation] 或 [Find Identity → Update]
```

数据节点：

```text
Break StatusChanged Payload
```

放在 Router 上方。

长数据线使用普通：

```text
Reroute Node
```

不要为了排版新增持久 Blueprint 变量，例如：

```text
CurrentStatusChangedPayload
PendingStatusId
PendingRuntimeSequence
```

这些都没有必要，并会增加 stale state 风险。

---

## 2.21 Return Node 的使用建议

Blueprint Function 可以存在多个 Return Node。

本节为了可读性，推荐直接放多个明确 Return：

```text
Unknown Target      → Return false
Removal             → Return false
Update Find Failed  → Return false
Creation Started    → Return true
Update Started      → Return true
```

这样比为了省 Return Node 而拉很长的 Exec 线更容易检查。

如果当前图已经有稳定的 Return 节点并且你能清晰复用，也可以保留现状。

核心不是 Return 节点数量，而是每个分支语义必须准确。

---

## 2.22 不要在 Router 中调用 `SetStatusView`

Router 的责任只有：

```text
验证
分类
定位精确 Widget
决定是否接管
把数据交给 PlayStatusChangedPresentation
```

不要把视觉更新逻辑散落到 Router：

```text
FindStatusWidgetByIdentity
→ 直接 SetStatusView
→ 再调用 PlayStatusChangedPresentation
```

这是错误分层。

正确分层：

```text
Router
→ PlayStatusChangedPresentation
→ 由 Presentation Event 自己更新视觉 + timer
```

这样 Creation 与 Update 的 token / timer 生命周期保持统一。

---

## 2.23 不要在 Router 中提前改 `ActivePresentationToken`

Router 不需要做：

```text
Set ActivePresentationToken
Set ActivePresentationType
```

这些已经由：

```text
PlayStatusChangedPresentation
```

统一负责。

如果 Router 先 Set，随后查找失败又 Return false，就可能留下并未真正接管的活动 Token。

所以严格保持：

```text
Router 只验证
→ 确认可播放后
→ 调 PlayStatusChangedPresentation
→ 由它设置 Active 状态
```

---

## 2.24 本节不处理 Cancel

Router 接通后，Update 已经能修改一个正式的：

```text
WBP_BattleStatus
```

但此时旧 Cancel 逻辑可能还是 creation-only：

```text
ActiveStatusPresentationWidget
→ RemoveFromParent
```

这对 Update 不安全。

**本节不要为了“顺手修好”而混着改。**

下一节将单独重构：

```text
CancelPresentationRecordPlayback
```

为 StatusChanged 使用历史 ViewModel 重建状态列表。

因此本节完成后：

```text
Router = 已接通
Cancel = 尚未为 Update 加固
PIE = 暂时不要跑 Update acceptance
```

---

## 2.25 本节也不处理 Removal

即使 `FindStatusWidgetByIdentity` 已经能找到要移除的精确 Widget，本节仍然锁定：

```text
bRemoved = true
→ Return false
```

不要把 Removal 临时塞进：

```text
bCreated = false
```

的 Update 路径。

Removal 会单独处理：

```text
临时隐藏/移除视觉
→ timer
→ Notify
→ reducer 正式删除
```

并且需要 Cancel 恢复策略。

---

## 2.26 Compile 前检查调用节点签名

第一节给 `PlayStatusChangedPresentation` 新增了：

```text
ExistingStatusWidget
```

所以 Router 中可能有旧调用节点发生签名刷新。

Compile 前逐个检查：

### Creation 调用

```text
StatusChanged        已连接
Token                已连接
ExistingStatusWidget 未连接（None）
```

### Update 调用

```text
StatusChanged        已连接
Token                已连接
ExistingStatusWidget 已连接 FoundStatusWidget
```

如果旧 Creation 调用节点显示 orphan pin / 黄警告：

1. 右键该 `PlayStatusChangedPresentation` 调用节点。
2. 选择：

```text
Refresh Node
```

3. 再检查三个输入。

如果 Refresh 后仍异常，可以删除调用节点并从 Custom Event/function 名称重新创建调用节点，但不要删除 Custom Event 本体。

---

## 2.27 Compile + Save

本节只修改：

```text
WBP_BattleHUD
```

但仍建议先确认依赖：

```text
WBP_BattleStatus
```

已经是 Compile Successful。

然后：

```text
1. Compile WBP_BattleHUD
2. 查看 Compiler Results
3. 确认 0 Errors
4. Save
```

如果报错集中在：

```text
PlayStatusChangedPresentation
```

优先检查新增的 `ExistingStatusWidget` pin 是否导致旧调用节点 stale。

如果报错集中在：

```text
FindStatusWidgetByIdentity
```

不要立即拆函数；先确认：

```text
WBP_BattleStatus 已 Compile + Save
Get CurrentStatusView 节点已经 Refresh
RuntimeSequence 仍是 Integer64
```

---

## 2.28 本节静态结构验收清单

Compile 成功后逐项核对：

```text
[ ] Switch.StatusChanged 仍进入专用 Router
[ ] Break Payload 可读取 TargetPresentationId
[ ] Break Payload 可读取 StatusId
[ ] Break Payload 可读取 RuntimeSequence(int64)
[ ] Break Payload 可读取 bCreated
[ ] Break Payload 可读取 bRemoved

[ ] TargetKnown = Player 或 Enemy
[ ] TargetKnown=false → Return false

[ ] TargetKnown=true 后先判断 bRemoved
[ ] bRemoved=true → Return false
[ ] Removal 没有误进 Update

[ ] bRemoved=false 后判断 bCreated
[ ] bCreated=true → Creation 调用
[ ] Creation ExistingStatusWidget=None
[ ] Creation 调用后 Return true

[ ] bCreated=false → FindStatusWidgetByIdentity
[ ] Find 输入 TargetPresentationId 正确
[ ] Find 输入 StatusId 正确
[ ] Find 输入 RuntimeSequence 为 Integer64

[ ] Found=false → Return false
[ ] Found=true → Update 调用
[ ] Update ExistingStatusWidget=FoundStatusWidget
[ ] Update Token=Function Entry.Token
[ ] Update StatusChanged=Record.StatusChanged
[ ] Update 调用后 Return true

[ ] Router 没有直接 SetStatusView
[ ] Router 没有直接修改 ViewModel
[ ] Router 没有提前 Set ActivePresentationToken
[ ] Router 没有开始 Removal
[ ] WBP_BattleHUD Compile 0 Errors
[ ] WBP_BattleHUD 已 Save
```

---

## 2.29 本节完成后的运行时语义

完成后 Router 的支持矩阵应变成：

```text
StatusChanged creation
TargetKnown=true
bCreated=true
bRemoved=false
→ Blueprint async

StatusChanged update/increase/reduction
TargetKnown=true
bCreated=false
bRemoved=false
exact widget found
→ Blueprint async

StatusChanged update/increase/reduction
exact widget not found
→ Return false
→ C++ fallback

StatusChanged removal
bRemoved=true
→ Return false
→ C++ fallback

Unknown target
→ Return false
→ C++ fallback
```

---

## 2.30 本节完成判定

本节只要求：

```text
Router 结构完成
+ FindStatusWidgetByIdentity 正式接入 Update / Reduction
+ Creation 仍保持原行为
+ Removal 仍 fallback
+ Compile 0 Errors
+ Save
```

**仍然不要进行正式 Update/Reduction PIE acceptance。**

原因：下一节还需要先修正：

```text
CancelPresentationRecordPlayback
```

否则一旦 Update async playback 被取消，旧 Cancel 可能会把正式状态 Widget 直接 `RemoveFromParent`，造成 HUD 与当前历史 ViewModel 不一致。

第二节结束后的状态应记为：

```text
PlayStatusChangedPresentation
Creation              WIRED / previously VALIDATED
Update/Reduction      WIRED

StatusChanged Router
Creation              ROUTED
Update/Reduction      ROUTED
Removal               FALLBACK
Unknown target        FALLBACK
Find failure          FALLBACK

Cancel hardening      NOT DONE
PIE update/reduction  NOT RUN
```

下一节处理：

```text
CancelPresentationRecordPlayback
→ StatusChanged Cancel 不再简单 RemoveFromParent
→ 从当前历史 ViewModel 重建 Player / Enemy status rows
→ 同时覆盖 Creation Cancel 与 Update Cancel
→ 不调用正常完成 Notify
```

---

# 第三节：加固 `CancelPresentationRecordPlayback`，让 StatusChanged Cancel 正确恢复历史状态

## 3.1 本节目标

第一、二节完成后，`StatusChanged` 已经具备：

```text
Creation
Update / Reduction
```

两类 Blueprint async playback。

问题在于旧 Cancel 是为 Creation 设计的。当前保存快照中的状态取消逻辑是：

```text
IsValid(ActiveStatusPresentationWidget)
→ RemoveFromParent
→ ActiveStatusPresentationWidget = None
```

Creation 时这样做可以删除“尚未正式进入历史 ViewModel 的临时新状态”；但 Update 时：

```text
ActiveStatusPresentationWidget
```

指向的是已经存在于 `WB_PlayerStatuses` / `WB_EnemyStatuses` 中的正式历史状态行。

如果仍然 `RemoveFromParent`，例如：

```text
历史 ViewModel = Weak 2
当前 Record 临时播放 = Weak 4
发生 Cancel
```

旧逻辑会把这个正式 Weak Widget 直接删除，HUD 就会变成“没有 Weak”，而历史 ViewModel 明明仍是 `Weak 2`。

因此本节必须把 StatusChanged Cancel 改为：

```text
不依赖 ActiveStatusPresentationWidget 自己恢复
→ 直接从当前历史 ViewModel.Statuses 重建 Player / Enemy 两个正式状态区
```

本节完成后，Creation Cancel 与 Update Cancel 使用同一套恢复策略。

---

## 3.2 为什么 Cancel 必须使用“当前历史 ViewModel”

A2E 的时序契约是：

```text
正在播放的 Record
= 已经提交、但 reducer 还没有推进到 ViewModel 的当前历史事实

ViewModel
= 只包含已经完成播放的历史事实
```

所以在当前 StatusChanged Record 尚未正常 Notify 前：

### Creation

```text
Record：无状态 → Weak 2
播放期间 HUD 临时显示 Weak 2
ViewModel 仍然是“无 Weak”
```

Cancel 时从 ViewModel 重建：

```text
Weak 2 transient 消失
→ 正确恢复“无 Weak”
```

### Update / Reduction

```text
Record：Weak 2 → Weak 4
播放期间 HUD 临时显示 Weak 4
ViewModel 仍然是 Weak 2
```

Cancel 时从 ViewModel 重建：

```text
Weak 4 transient visual 被替换
→ 正确恢复 Weak 2
```

因此 Cancel 不需要保存第二套：

```text
AmountBefore
PreviousStatusView
PreviousDescription
```

历史 ViewModel 已经是唯一恢复来源。

---

## 3.3 本节修改位置

打开：

```text
WBP_BattleHUD
```

进入 Event Graph，找到父类 Blueprint override：

```text
Cancel Presentation Record Playback
```

底层对应：

```text
CancelPresentationRecordPlayback(Token)
```

当前保存版本已经包含通用清理骨架，包括：

```text
ClearAndInvalidateTimerByHandle(ActivePresentationTimer)
恢复 HiddenHandCardWidget
移除 PlayedCardWidget
隐藏 Txt_DamagePresentation
Player / Enemy RenderOpacity 恢复 1.0
清理 ActiveStatusPresentationWidget
清空临时引用
ActivePresentationType = None
ActivePresentationToken = default
```

本节只替换“Status 状态控件清理”这一段。

不要重写整条 Cancel。

---

## 3.4 先保留 Cancel 前半段通用清理

以下现有行为全部保留：

```text
Cancel Event
↓
ClearAndInvalidateTimerByHandle(ActivePresentationTimer)
↓
若 HiddenHandCardWidget 有效则恢复可见
↓
若 PlayedCardWidget 有效则 RemoveFromParent
↓
Txt_DamagePresentation = Collapsed
↓
Player / Enemy RenderOpacity = 1.0
```

特别是 Timer 必须继续优先清掉。

原因：Cancel 已经发生后，旧 `FinishPresentationRecord` 不能在 0.5 秒后再次触发正常完成。

不要把：

```text
ClearAndInvalidateTimerByHandle
```

移动到 Status 重建之后。

---

## 3.5 找到并删除旧的 Status `RemoveFromParent` 路径

找到当前这一段：

```text
Get ActiveStatusPresentationWidget
↓
Is Valid
├ valid
│   ↓
│ Remove From Parent
│
└ invalid
    ↓
继续

→ Set ActiveStatusPresentationWidget = None
```

本节要移除的核心节点是：

```text
Remove From Parent(ActiveStatusPresentationWidget)
```

Update/Reduction 接入之后，这个节点不再安全。

### 必须确认

删除/断开后，Status Cancel 路径中不能再出现：

```text
ActiveStatusPresentationWidget → RemoveFromParent
```

不要保留成：

```text
Creation 时 RemoveFromParent
Update 时 Rebuild
```

因为 Cancel Event 当前没有保存“这是 creation 还是 update”的独立可靠成员状态，而且完全没有必要增加第二套状态。

统一从 ViewModel 重建更简单，也更符合 reducer 时序。

---

## 3.6 在清空 `ActivePresentationType` 之前判断当前类型

拖出：

```text
Get ActivePresentationType
```

从 enum pin 拉线，搜索等于比较：

```text
==
```

选择对应 enum 的 Equality 节点，把另一端设为：

```text
StatusChanged
```

得到：

```text
IsStatusChangedCancel
= ActivePresentationType == StatusChanged
```

注意这个判断必须发生在：

```text
Set ActivePresentationType = None
```

之前。

如果先清 Type，再判断，结果永远不会是 `StatusChanged`。

---

## 3.7 推荐用一个 `Sequence` 解决执行线汇合

Blueprint 的执行输入不能简单把两个 Branch 输出硬接到同一个普通节点。

为了让：

```text
StatusChanged 时先恢复状态列表
然后所有 Record 都继续执行公共清理
```

推荐在旧 Status 清理位置放：

```text
Sequence
```

结构：

```text
前面的通用 Cancel 清理
↓
Sequence
├ Then 0 → StatusChanged 专用恢复
└ Then 1 → 公共尾部清理
```

`RebuildStatusIcons` 是同步 Blueprint function，不是 Latent 节点，因此：

```text
Then 0
```

完整执行结束后才会执行：

```text
Then 1
```

这能避免为了“合并白线”复制大量公共清理节点。

---

## 3.8 `Sequence.Then 0`：Branch 判断 StatusChanged

从：

```text
Sequence.Then 0
```

接：

```text
Branch
```

Condition：

```text
IsStatusChangedCancel
```

最终：

```text
Sequence.Then 0
↓
Branch(ActivePresentationType == StatusChanged)
```

### False

保持不连接即可。

这表示当前取消的是：

```text
CardPlayed
Damage
BlockChanged
CardZoneChanged
未来其他 Record
```

就不需要重建 Status WrapBox。

### True

进入 ViewModel 有效性检查。

---

## 3.9 `True` 后检查 `ViewModel` 是否有效

拖出：

```text
Get ViewModel
```

使用带执行 pin 的：

```text
Is Valid
```

宏。

连接：

```text
Branch(IsStatusChangedCancel).True
→ IsValid(ViewModel).Exec

ViewModel
→ IsValid.InputObject
```

### Is Valid

执行 Player / Enemy 状态重建。

### Is Not Valid

本节建议**不要**再回退到：

```text
RemoveFromParent(ActiveStatusPresentationWidget)
```

因为我们无法判断该引用是 transient creation widget，还是正式 update widget。

若 ViewModel 已无效，没有可靠历史来源可以进行局部恢复。此时让专用恢复分支结束，后面的公共清理仍会清空 Blueprint transient reference；正常 Widget 销毁/后续完整刷新负责收口。

这比“猜测性删除一个正式状态行”安全。

---

## 3.10 取得 `ViewModel.Player.Statuses`

从有效的 `ViewModel` 获取：

```text
Player
```

其类型是：

```text
FBattleHUDCombatantView
```

可以：

```text
Get ViewModel
→ Get Player
→ Break Battle HUD Combatant View
→ Statuses
```

只需要使用：

```text
Statuses
```

数组 pin。

不要读取 Gameplay Combatant，也不要从 `ActiveStatusPresentationWidget.CurrentStatusView` 反推出旧状态。

---

## 3.11 第一次调用 `RebuildStatusIcons`：恢复 Player

放置当前 WBP 已有函数：

```text
RebuildStatusIcons
```

输入连接：

```text
Statuses
← ViewModel.Player.Statuses

StatusContainer / WrapBox
← WB_PlayerStatuses
```

具体参数显示名以当前函数节点为准；语义必须是：

```text
RebuildStatusIcons(
    ViewModel.Player.Statuses,
    WB_PlayerStatuses
)
```

执行线：

```text
IsValid(ViewModel).Is Valid
↓
RebuildStatusIcons(Player)
```

该函数会按当前历史数组重建正式 Player 状态行。

---

## 3.12 取得 `ViewModel.Enemy.Statuses`

同样从 ViewModel 获取：

```text
Enemy
→ Break Battle HUD Combatant View
→ Statuses
```

得到：

```text
ViewModel.Enemy.Statuses
```

不要复用 Player 的 Statuses 数组。

---

## 3.13 第二次调用 `RebuildStatusIcons`：恢复 Enemy

再放一个：

```text
RebuildStatusIcons
```

连接：

```text
Statuses
← ViewModel.Enemy.Statuses

StatusContainer / WrapBox
← WB_EnemyStatuses
```

执行线：

```text
RebuildStatusIcons(Player)
↓
RebuildStatusIcons(Enemy)
```

最终 StatusChanged 专用恢复是：

```text
ActivePresentationType == StatusChanged
↓
IsValid(ViewModel)
↓
RebuildStatusIcons(ViewModel.Player.Statuses, WB_PlayerStatuses)
↓
RebuildStatusIcons(ViewModel.Enemy.Statuses, WB_EnemyStatuses)
```

---

## 3.14 为什么建议 Player / Enemy 两边都重建

理论上当前 StatusChanged Record 只会针对一个 Target。

但 Cancel 的职责是：

```text
把 Presentation UI 恢复到当前历史 ViewModel 的完整正式状态
```

两边一起重建有几个好处：

```text
不需要在 Cancel 再保存 TargetPresentationId
不需要判断 ActiveStatusPresentationWidget 属于哪个 WrapBox
Creation / Update / Reduction 共用一个恢复路径
未来 Removal 也能直接复用
不会依赖数组 index 或 Widget parent
```

当前只有 Player + Enemy 两个正式 combatant status container，因此成本很低。

---

## 3.15 不要用 `RefreshCombatantPresentations` 替代这两次重建

虽然：

```text
RefreshCombatantPresentations
```

最终也会调用两次 `RebuildStatusIcons`，本节仍推荐直接调用：

```text
RebuildStatusIcons(Player.Statuses, WB_PlayerStatuses)
RebuildStatusIcons(Enemy.Statuses, WB_EnemyStatuses)
```

原因是 Cancel 当前只需要恢复 Status visual。

不要顺便刷新：

```text
HP
Block
Target Selection
Combatant Presentation
其他 ViewModel UI
```

这样能减少 Cancel 的副作用范围，后续统一 A2E cancellation review 也更容易审计。

---

## 3.16 `Sequence.Then 1`：统一清空 Status transient reference

从：

```text
Sequence.Then 1
```

接：

```text
Set ActiveStatusPresentationWidget
```

Value 保持：

```text
None
```

也就是：

```text
ActiveStatusPresentationWidget = None
```

这样无论：

```text
当前是不是 StatusChanged
ViewModel 是否有效
Creation 还是 Update
```

最终都不会保留 stale status widget reference。

注意：这里是**清引用**，不是 `RemoveFromParent`。

---

## 3.17 `Then 1` 后继续原来的公共尾部清理

`Set ActiveStatusPresentationWidget = None` 之后，继续接回当前已经存在的公共 Cancel 尾部，例如：

```text
HiddenHandCardWidget = None
PlayedCardWidget = None
bDamageTargetIsPlayer = false（若当前已有）
ActivePresentationType = None
ActivePresentationToken = default
```

具体变量顺序保持当前保存版本即可。

本节不要为了排版重写所有通用变量。

核心顺序要求只有两个：

```text
① Status 恢复发生在 ActivePresentationType 清空之前
② ActiveStatusPresentationWidget 最终清为 None
```

---

## 3.18 Cancel 路径绝对不要调用正常完成 Notify

检查整个：

```text
Cancel Presentation Record Playback
```

不能出现：

```text
NotifyPresentationFinished
NotifyPresentationRecordFinished
FinishPresentationRecord
```

Cancel 的语义是：

```text
停止旧视觉所有权
恢复到已完成的历史 ViewModel
清理 Blueprint transient state
```

不是：

```text
把被取消的 Record 当作正常播放完成
```

Controller 会负责之后的 generation/token/reconciliation 逻辑。

---

## 3.19 不要在 Blueprint 内重新比较传入的 Cancel Token

当前保存快照中：

```text
Cancel Presentation Record Playback(Token)
```

收到的 `Token` 没有在 Blueprint 内再次比较。

本节继续保持这一点。

调用边界的“当前 Token 是否仍属于该 Widget”已经由基类 C++ Controller 负责。

不要新增：

```text
IncomingToken == ActivePresentationToken ?
```

然后自己决定是否执行 Cancel。

这会复制一套 ownership 判定，并可能和 C++ generation 语义漂移。

---

## 3.20 正常 Finish 路径保持不变

本节只改 Cancel。

检查：

```text
FinishPresentationRecord
```

中的 `StatusChanged` case 仍保持：

```text
StatusChanged
→ NotifyPresentationRecordFinished
→ ActiveStatusPresentationWidget = None
```

正常完成时不要：

```text
RebuildStatusIcons(AmountBefore)
RemoveFromParent ActiveStatusPresentationWidget
恢复 CurrentStatusView 旧值
```

正常 Update 的正确时序仍是：

```text
历史 Weak 2
→ Record 临时显示 Weak 4
→ timer
→ Notify exact token
→ reducer 把 ViewModel 推进到 Weak 4
→ 正常 HUD refresh
→ 最终仍 Weak 4
```

如果 Finish 主动恢复旧值，会产生：

```text
2 → 4 → 2 → 4
```

闪回。

---

## 3.21 Creation Cancel 的预期语义

完成本节后，如果 Creation playback 被 Cancel：

```text
历史 ViewModel：无 Weak
当前 transient：Weak 2
```

执行：

```text
Cancel
→ Clear Timer
→ ActivePresentationType == StatusChanged
→ Rebuild Player / Enemy Statuses from ViewModel
→ transient Weak 2 被正式列表重建覆盖掉
→ ActiveStatusPresentationWidget = None
→ 不 Notify
```

最终：

```text
HUD = 无 Weak
ViewModel = 无 Weak
```

正确。

---

## 3.22 Update / Reduction Cancel 的预期语义

如果 Update playback 被 Cancel：

```text
历史 ViewModel：Weak 2
当前 transient visual：Weak 4
```

执行：

```text
Cancel
→ Clear Timer
→ Rebuild Player / Enemy Statuses from ViewModel
→ Weak 4 被历史 Weak 2 替换
→ ActiveStatusPresentationWidget = None
→ 不 Notify
```

最终：

```text
HUD = Weak 2
ViewModel = Weak 2
```

正确。

Reduction 同理，例如：

```text
历史 Weak 4
临时 Weak 3
Cancel
→ 恢复 Weak 4
```

---

## 3.23 本节完成后的推荐完整结构

状态相关部分可抽象为：

```text
Cancel Presentation Record Playback(Token)
↓
ClearAndInvalidateTimerByHandle(ActivePresentationTimer)
↓
原有 Card / Damage / Opacity 通用清理
↓
Sequence

├─ Then 0
│    ↓
│  Branch(ActivePresentationType == StatusChanged)
│
│  ├─ false
│  │    → 无操作
│  │
│  └─ true
│       ↓
│     IsValid(ViewModel)
│
│     ├─ Is Not Valid
│     │    → 无猜测性 RemoveFromParent
│     │
│     └─ Is Valid
│          ↓
│        RebuildStatusIcons(
│            ViewModel.Player.Statuses,
│            WB_PlayerStatuses
│        )
│          ↓
│        RebuildStatusIcons(
│            ViewModel.Enemy.Statuses,
│            WB_EnemyStatuses
│        )
│
└─ Then 1
     ↓
   ActiveStatusPresentationWidget = None
     ↓
   原有公共 transient reference 清理
     ↓
   ActivePresentationType = None
     ↓
   ActivePresentationToken = default
```

整个 Cancel 路径：

```text
没有 Notify
没有修改 ViewModel
没有重新计算 Status
没有 ActiveStatusPresentationWidget.RemoveFromParent
```

---

## 3.24 Compile + Save

本节只修改：

```text
WBP_BattleHUD
```

操作：

```text
1. Compile WBP_BattleHUD
2. 查看 Compiler Results
3. 确认 0 Errors
4. Save
```

常见错误检查：

### `RebuildStatusIcons` 参数类型不匹配

确认：

```text
Player.Statuses / Enemy.Statuses
= Array<FBattleHUDStatusView>
```

容器分别是：

```text
WB_PlayerStatuses
WB_EnemyStatuses
```

### `ViewModel.Player` / `Enemy` 取不到 Statuses

使用：

```text
Get Player / Get Enemy
→ Break Battle HUD Combatant View
→ Statuses
```

不要从 Gameplay 对象取。

### 编译提示旧 `RemoveFromParent` 链断开

如果你删除了节点但留下孤立执行线，清掉旧线即可；不要为了消警告把它重新接回。

---

## 3.25 本节静态验收清单

Compile 成功后逐项检查：

```text
[ ] Cancel 一开始仍清 ActivePresentationTimer
[ ] HiddenHandCardWidget 恢复逻辑未被破坏
[ ] PlayedCardWidget 清理逻辑未被破坏
[ ] Damage text / RenderOpacity 恢复逻辑未被破坏

[ ] Status Cancel 不再 ActiveStatusPresentationWidget.RemoveFromParent
[ ] 在 ActivePresentationType 清空前判断 == StatusChanged
[ ] 只有 StatusChanged 才执行 status list rebuild
[ ] StatusChanged Cancel 检查 ViewModel 有效性
[ ] Player 使用 ViewModel.Player.Statuses
[ ] Player 重建到 WB_PlayerStatuses
[ ] Enemy 使用 ViewModel.Enemy.Statuses
[ ] Enemy 重建到 WB_EnemyStatuses
[ ] 没有从 UStatusInstance / UStatusData 查询
[ ] 没有修改 ViewModel.Statuses

[ ] ViewModel 无效时不猜测性删除正式 status row
[ ] ActiveStatusPresentationWidget 最终清为 None
[ ] ActivePresentationType 最终清为 None
[ ] ActivePresentationToken 最终清为 default
[ ] Cancel 没有调用 NotifyPresentationFinished
[ ] Cancel 没有调用 NotifyPresentationRecordFinished
[ ] Cancel 没有调用 FinishPresentationRecord

[ ] FinishPresentationRecord 的 StatusChanged 正常完成路径未被改坏
[ ] WBP_BattleHUD Compile 0 Errors
[ ] WBP_BattleHUD 已 Save
```

---

## 3.26 本节完成判定

本节要求：

```text
StatusChanged Cancel hardening 完成
+ Creation / Update / Reduction 统一从历史 ViewModel 恢复
+ 不再直接 RemoveFromParent ActiveStatusPresentationWidget
+ Cancel 不 Notify
+ Compile 0 Errors
+ Save
```

本节仍然以**静态结构 + Compile**为主，不把 Update/Reduction 标记为 VALIDATED。

第三节结束后的状态应记为：

```text
StatusChanged creation          VALIDATED（既有证据）
StatusChanged update/reduction  WIRED / NOT PIE VALIDATED
StatusChanged removal           FALLBACK

Router                          WIRED
Exact identity lookup           WIRED
Cancel reconciliation           WIRED
PIE update/reduction            NOT RUN
```

下一节进入第一次正式运行时验收：

```text
StatusChanged Update / Increase PIE
+ StatusChanged Reduction / TurnEndDecay PIE
→ 检查同一 Widget、无重复、无闪回、exact-token completion、最终 Idle
```

---

# 第四节：PIE 验收 `StatusChanged` Update / Increase 与 Reduction / TurnEndDecay

## 4.1 本节目标

前三节已经完成了结构层面的三件事：

```text
PlayStatusChangedPresentation
→ 已能更新 ExistingStatusWidget

StatusChanged Router
→ 已能按 TargetPresentationId + StatusId + RuntimeSequence
  找到精确状态行后把 update/reduction 送入 Blueprint async playback

CancelPresentationRecordPlayback
→ StatusChanged Cancel 已能从历史 ViewModel.Statuses 重建正式状态列表
```

本节第一次对这条新路径做正式运行时验收。

必须至少覆盖：

```text
A. Increase / Reapply
   例如 Weak 2 → Weak 4

B. Reduction / TurnEndDecay
   例如 Weak 4 → Weak 3
```

本节通过后，才允许把：

```text
StatusChanged update/reduction
```

从：

```text
WIRED / NOT PIE VALIDATED
```

提升为：

```text
VALIDATED
```

本节**不实现 Removal**。如果实际测试中遇到 `AmountAfter = 0 && bRemoved = true`，那属于下一节。

---

## 4.2 开始 PIE 前的编译与保存检查

先退出正在运行的 PIE。

依次确认：

```text
WBP_BattleStatus
→ Compile
→ 0 Errors
→ Save

WBP_BattleHUD
→ Compile
→ 0 Errors
→ Save
```

然后重新打开 `WBP_BattleHUD`，快速确认以下三处没有被编辑器刷新成 orphan node：

```text
① FindStatusWidgetByIdentity
② PlayStatusChangedPresentation 的 ExistingStatusWidget pin
③ Cancel Presentation Record Playback 中两次 RebuildStatusIcons
```

如果任一处出现黄警告或 orphan pin，先修复并重新 Compile；不要带着 Blueprint compile warning 进入本节验收。

---

## 4.3 本节测试数据原则

优先使用项目里**真实配置、真实 Gameplay 流程会产生的 StatusChanged Record**。

推荐选择：

```text
Weak
Vulnerable
或项目中其他能稳定重复施加、并且会自然衰减的状态
```

不要为了本节临时在 Blueprint 中伪造：

```text
Make StatusChanged Presentation Payload
手写 AmountBefore / AmountAfter
直接调用 PlayStatusChangedPresentation
```

原因：本节要验证的是整条链：

```text
Gameplay Commit
→ frozen Presentation Record
→ Controller
→ Blueprint Router
→ exact identity lookup
→ async visual
→ exact-token Notify
→ reducer
→ ViewModel refresh
```

只手调 Custom Event 无法证明 Router、Token 和 reducer 链是通的。

---

## 4.4 先做一次 Creation 回归，建立可更新的正式状态行

启动 PIE。

先对目标施加一个当前没有的状态，例如：

```text
Enemy：无 Weak
→ 使用真实卡牌/效果施加 Weak 2
```

这一条应仍走已经验证过的 Creation 路径：

```text
StatusChanged
bCreated = true
bRemoved = false
```

视觉预期：

```text
目标原本没有状态图标
→ Weak 2 出现在正确 Player/Enemy 的 WrapBox
→ 保持约 0.5 秒的 async playback
→ Notify 完成
→ reducer 推进 ViewModel
→ 正式列表刷新后 Weak 2 仍然存在
```

本轮只作为回归检查；如果 Creation 因本轮修改坏掉，先停止，不要继续做 Update 测试。

---

## 4.5 Creation 回归必须确认“只有一个正式状态行”

第一次状态创建完成后，肉眼检查目标状态区：

```text
WB_PlayerStatuses
或
WB_EnemyStatuses
```

同一个状态应只有一个图标。

例如：

```text
Weak × 1 Widget
Amount = 2
```

不能已经出现：

```text
Weak 2
Weak 2
```

如果 Creation 本身已经重复，Update 验收结果没有意义，应先回查 Creation Finish / HUD rebuild。

---

## 4.6 准备 Increase / Reapply 场景

保持同一个目标、同一个已经存在的 runtime status instance，再次触发会增加层数/持续量的真实效果。

示例：

```text
当前正式 HUD：Weak 2
当前历史 ViewModel：Weak 2

再次施加 Weak 2
→ 预期 Gameplay 产生更新 Record
→ 最终 AmountAfter = 4
```

具体数值不要求一定是 `2 → 4`，项目真实规则如果是：

```text
Weak 1 → 2
Weak 2 → 3
Vulnerable 2 → 4
```

都可以。

验收的关键不是固定数字，而是：

```text
AmountBefore = A
AmountAfter  = B
A != B
bCreated     = false
bRemoved     = false
RuntimeSequence 与现有正式状态相同
```

---

## 4.7 确认 Increase Record 真的是 Update，而不是第二次 Creation

通过当前项目已有日志、Blueprint Debugger、Record 调试输出或你现有的 Presentation 调试信息，确认该 Record 至少满足：

```text
Record.Type = StatusChanged
bCreated = false
bRemoved = false
AmountBefore = A
AmountAfter = B
```

并且：

```text
StatusId
RuntimeSequence
TargetPresentationId
```

与当前 HUD 正式状态行身份一致。

如果第二次施加实际产生：

```text
bCreated = true
```

那说明 Gameplay 规则是在创建新的 runtime instance；这不能用于本节 Update 验收，应换一个确实会更新原实例的状态/效果。

---

## 4.8 Increase 开始时应命中 `FindStatusWidgetByIdentity`

当 `bCreated=false && bRemoved=false` 的 Record 到来时，运行时预期路径是：

```text
StatusChanged Router
↓
TargetKnown = true
↓
bRemoved = false
↓
bCreated = false
↓
FindStatusWidgetByIdentity(
  TargetPresentationId,
  StatusId,
  RuntimeSequence
)
↓
Found = true
↓
PlayStatusChangedPresentation(
  ExistingStatusWidget = FoundStatusWidget
)
↓
Return true
```

如果你使用 Blueprint Debugger，可以在：

```text
FindStatusWidgetByIdentity
或 Branch(Found)
```

观察一次。

但不要为了验收新增持久 Debug 变量。

---

## 4.9 Increase 的核心视觉要求：更新同一个 Widget

Update playback 开始后，应该看到：

```text
原正式状态：Weak A
↓
同一个位置/同一个状态图标
↓
Amount 变为 B
```

不能看到：

```text
Weak A + Weak B 两个图标同时存在
```

也不能看到：

```text
旧 Weak A 被删掉
→ 新 Weak B 重新出现在列表末尾
```

本阶段要求是：

```text
ExistingStatusWidget.SetStatusView(FrozenStatusView)
```

即更新已经找到的精确 Widget。

---

## 4.10 Increase 的冻结数据检查

临时显示值必须等于：

```text
Record.AmountAfter
```

同时 `WBP_BattleStatus.CurrentStatusView` 应被新的 frozen view 覆盖，因此其：

```text
StatusId
RuntimeSequence
DisplayName
Description
Amount
Icon metadata
```

都来自当前 Record DTO。

不要以“最终数字看起来对”作为唯一证据。

如果实际代码中存在：

```text
Current Amount + Delta
AmountBefore + 某个值
```

即使结果碰巧等于 B，也不满足 A2E frozen Record 契约。

---

## 4.11 Increase 的 0.5 秒 async 窗口

Update 路径应该与 Creation 共用：

```text
StartPresentationFinishTimer
Time = 0.5
Looping = false
```

在这段时间内：

```text
HUD 已显示 AmountAfter = B
ViewModel 仍代表上一条已完成历史，即 Amount = A
```

不要要求 ViewModel 在 timer 开始时立即变成 B。

这正是 A2E 的历史播放时序。

---

## 4.12 Increase 正常完成后的顺序

约 0.5 秒后预期：

```text
FinishPresentationRecord
↓
StatusChanged case
↓
NotifyPresentationRecordFinished
↓
NotifyPresentationFinished(ActivePresentationToken)
↓
Controller 接受精确 Token
↓
StatusChanged reducer 推进 WorkingSnapshot / ViewModel
↓
正式 HUD refresh
↓
ActiveStatusPresentationWidget = None
```

最终正式 HUD 必须仍显示：

```text
Amount = B
```

不能回到 A。

---

## 4.13 Increase 最重要的闪回检查

完整肉眼序列应该是：

```text
A
→ B
→ B
```

其中最后两个 B 分别代表：

```text
B（当前 Record transient visual）
B（Notify 后正式 ViewModel rebuild）
```

禁止出现：

```text
A
→ B
→ A
→ B
```

如果出现 `A → B → A → B`，重点检查：

```text
FinishPresentationRecord 是否恢复了 AmountBefore
Notify 之前是否调用了 RebuildStatusIcons
Event Battle HUD View Model Changed 是否过早按旧 ViewModel 重建
```

正常 Finish 不应主动恢复旧状态。

---

## 4.14 Increase 完成后检查继续播放与 Idle

该 StatusChanged Record 完成后：

```text
后续 Presentation Record 必须继续正常播放
```

例如卡牌还可能继续进入：

```text
CardZoneChanged(PlayArea -> Destination)
```

最终整个 Envelope 结束后：

```text
Presentation backlog 清空
→ Controller catch-up
→ 正常回 Idle / 可交互状态
```

不能出现：

```text
状态数值已经更新
但 Presentation 一直卡在 Resolving
```

这通常意味着 Token 没有正常完成或 Router 返回值与实际 async ownership 不一致。

---

## 4.15 Increase 验收清单

至少逐项确认：

```text
[ ] 第二次施加产生 StatusChanged
[ ] bCreated=false
[ ] bRemoved=false
[ ] TargetPresentationId 正确
[ ] StatusId 正确
[ ] RuntimeSequence 与已有状态一致
[ ] AmountBefore=A
[ ] AmountAfter=B
[ ] Router 走 Update/Reduction 分支
[ ] FindStatusWidgetByIdentity Found=true
[ ] 只更新一个已有 Widget
[ ] 没有创建第二个状态图标
[ ] 显示值直接等于 AmountAfter
[ ] async 窗口可见
[ ] exact-token completion 正常
[ ] Notify 后最终仍为 B
[ ] 没有 A→B→A→B 闪回
[ ] 后续 Record 能继续
[ ] 最终回 Idle / 正常交互
```

只要其中一项失败，就暂时不要把 Update 标记为 VALIDATED。

---

## 4.16 准备 Reduction / TurnEndDecay 场景

接下来验证同一条 Update 路径能处理“减少但尚未移除”。

优先使用项目真实的：

```text
TurnEndDecay
```

或其他会让状态 Amount 减少、但结果仍大于 0 的规则。

示例：

```text
当前 Weak 4
→ EndTurn
→ Weak 3
```

要求实际 Record 是：

```text
bCreated = false
bRemoved = false
AmountBefore = A
AmountAfter = B
B < A
B > 0
```

如果当前状态只剩 `1`，下一次衰减直接变成 `0` 并产生 `bRemoved=true`，那是 Removal，不适合作为本节 Reduction 验收。

应先准备一个足够大的 Amount，让本次衰减仍保留状态。

---

## 4.17 Reduction 的身份检查

Reduction 仍必须保持同一个 runtime identity：

```text
TargetPresentationId
StatusId
RuntimeSequence
```

都应与正式状态行一致。

特别是：

```text
RuntimeSequence 不应因为 Amount 减少而变化
```

如果 RuntimeSequence 改变，`FindStatusWidgetByIdentity` 正确行为应该是找不到旧行并 fallback；不能为了让视觉“看起来能动”而改成只按 StatusId 匹配。

---

## 4.18 Reduction 的运行时路径

预期仍然是：

```text
StatusChanged
↓
TargetKnown=true
↓
bRemoved=false
↓
bCreated=false
↓
FindStatusWidgetByIdentity
↓
Found=true
↓
PlayStatusChangedPresentation
↓
ExistingStatusWidget.SetStatusView(FrozenStatusView)
↓
StartPresentationFinishTimer
```

也就是说 Increase 和 Reduction **不需要两套 Blueprint Event**。

它们都只是：

```text
已有精确 status row 的 AmountAfter / DescriptionAfter 更新
```

---

## 4.19 Reduction 的视觉检查

假设：

```text
A = 4
B = 3
```

正确显示序列：

```text
Weak 4
→ Weak 3
→ 保持 Weak 3
```

不能：

```text
Weak 4
→ 新增一个 Weak 3
```

不能：

```text
Weak 4
→ Weak 3
→ Weak 4
→ Weak 3
```

也不能：

```text
Weak 4
→ 状态图标直接消失
```

因为本条 Record 的：

```text
bRemoved = false
```

状态仍然存在。

---

## 4.20 Reduction 完成后的 reducer / rebuild 检查

约 0.5 秒后：

```text
Notify exact token
→ Controller reducer 推进 ViewModel
→ ViewModel.Statuses 中该 runtime row Amount 变成 B
→ HUD 正常 rebuild
```

最终：

```text
同一个状态仍存在
Amount = B
```

不要在 `FinishPresentationRecord` 中恢复 A。

---

## 4.21 Reduction 完成后继续观察完整 EndTurn Record 顺序

如果 Reduction 是 EndTurn 产生的，那么它通常处于一个包含多个 Record 的大 Envelope 中。

因此不要在看到 Weak 变成 B 后立即停止 PIE。

继续观察：

```text
后续 BlockChanged
Damage
EnergyChanged（当前仍可能 fallback）
CardZoneChanged
DeckShuffled（当前仍可能 fallback）
Draw 相关 Record
```

以项目实际 producer 顺序为准。

本节只要求 StatusChanged reduction 不打断这条宏流程。

最终必须：

```text
不卡 Resolving
不丢后续 Record
能够进入下一个正式交互阶段
```

---

## 4.22 Reduction 验收清单

```text
[ ] 触发真实 Reduction / TurnEndDecay
[ ] bCreated=false
[ ] bRemoved=false
[ ] AmountBefore=A
[ ] AmountAfter=B
[ ] B < A
[ ] B > 0
[ ] TargetPresentationId 正确
[ ] StatusId 正确
[ ] RuntimeSequence 保持不变
[ ] FindStatusWidgetByIdentity Found=true
[ ] 更新同一个状态 Widget
[ ] 没有重复图标
[ ] 没有错误移除状态
[ ] 显示值直接使用 AmountAfter
[ ] 0.5s async 正常
[ ] exact-token completion 正常
[ ] Notify 后最终仍为 B
[ ] 没有 A→B→A→B 闪回
[ ] 后续 EndTurn Records 继续
[ ] 最终不挂 Resolving
```

---

## 4.23 明确区分 Reduction 与 Removal

这一点必须单独确认。

### Reduction

```text
AmountBefore = 4
AmountAfter  = 3
bRemoved     = false
```

本节应由 Blueprint Update 路径接管。

### Removal

```text
AmountBefore = 1
AmountAfter  = 0
bRemoved     = true
```

当前 Router 仍应：

```text
Return false
→ C++ fallback
```

本节不要因为看到 `1 → 0` 没有进入 Update 动画，就判定本节失败。

`1 → 0` 是下一节正式要实现的 Removal。

---

## 4.24 再做一次 Creation 回归

Increase 和 Reduction 都通过后，再用另一个当前不存在的状态，或重新开一局，做一次 Creation：

```text
无状态
→ StatusChanged bCreated=true
→ 状态出现
→ Notify
→ 正式状态保持
```

目的不是重新证明 Creation 全套，而是确认第一节加入：

```text
Branch(bCreated)
```

之后没有把原本已经验证过的 Creation 路径破坏。

至少确认：

```text
Creation 仍只创建一个 Widget
Creation ExistingStatusWidget=None 不影响运行
Creation Finish 后状态不闪回
```

---

## 4.25 Cancel 的运行时抽查策略

第三节已经完成 Cancel 结构加固，但是否能在 PIE 中稳定触发 Cancel 取决于项目当前是否已有正式入口。

如果当前项目有可重复触发的正式路径，例如：

```text
SkipPresentation()
Widget 被 Controller 主动替换/失去
合法的 presentation generation 取消
```

则建议额外做一次 Update Cancel：

```text
历史 Weak A
→ Update transient 显示 Weak B
→ 在 0.5s 内触发正式 Cancel
```

预期：

```text
Cancel 清 Timer
→ 从历史 ViewModel.Statuses 重建
→ HUD 恢复 Weak A
→ 不 Notify 被取消的 Record
```

如果当前 PIE 没有可靠、可控的 Cancel 触发入口，**不要临时在蓝图中人为调用 Cancel Event 伪造测试**。

此时把 Cancel runtime 验收保留到后面的“全局 Cancel / Reconcile 收尾”章节统一完成；本节的主线 Update/Reduction 验收仍以正常 async completion 为主。

---

## 4.26 如果 `Found=false`，按这个顺序排查

Update/Reduction Record 明明存在，但 Router 返回 false 时，不要先改身份契约。

按顺序检查：

```text
1. 当前目标 WrapBox 是否已经有正式状态 Widget
2. WBP_BattleStatus.SetStatusView 是否保存 CurrentStatusView
3. CurrentStatusView.StatusId 是否等于 Record.StatusId
4. CurrentStatusView.RuntimeSequence 是否等于 Record.RuntimeSequence
5. TargetPresentationId 是否选择了正确 Player / Enemy WrapBox
6. RuntimeSequence 是否仍是 Integer64
7. 当前正式状态列表是否被某次提前 rebuild 换掉
```

禁止“修复”为：

```text
只比较 StatusId
只拿第 0 个 Child
找不到就 Create Widget
```

这些都会破坏 exact identity。

---

## 4.27 如果出现重复状态图标，按这个顺序排查

出现：

```text
Weak A
Weak B
```

通常意味着 Update 被误当成 Creation 或 Update 分支里仍有 AddChild。

检查：

```text
1. Record.bCreated 是否真的是 false
2. Router bCreated=false 是否进入 FindStatusWidgetByIdentity
3. Found=true 后 ExistingStatusWidget 是否真的连接 FoundStatusWidget
4. PlayStatusChangedPresentation.False 是否还有 Create Widget
5. Update False 是否还有 AddChildToWrapBox
```

Update 路径正确时：

```text
Widget 数量不增加
```

---

## 4.28 如果出现数值闪回，按这个顺序排查

出现：

```text
A → B → A → B
```

重点检查：

```text
1. FinishPresentationRecord.StatusChanged 是否恢复 AmountBefore
2. Finish 前是否 RebuildStatusIcons
3. NotifyPresentationRecordFinished 的顺序是否被改坏
4. Controller reducer 是否在 exact token completion 后才推进
5. Event Battle HUD View Model Changed 是否在 reducer 前收到旧 snapshot refresh
```

Blueprint 正常 Finish 只能：

```text
保持 B
→ Notify
→ 清 ActiveStatusPresentationWidget reference
```

不能主动恢复 A。

---

## 4.29 如果播放卡在 Resolving，按这个顺序排查

如果状态视觉已经变成 B，但后续不继续：

```text
1. StartPresentationFinishTimer 是否真的执行
2. ActivePresentationTimer 是否有效
3. FinishPresentationRecord 是否被 Timer 调到
4. Finish 的 StatusChanged case 是否进入 NotifyPresentationRecordFinished
5. Notify 时 ActivePresentationToken 是否仍是当前 Token
6. Notify 前是否错误地先清空 Token
7. Router 是否 Return true 但实际上没有启动 Timer
```

锁定规则仍是：

```text
NotifyPresentationFinished(ActivePresentationToken)
必须先发生
→ 然后才清 ActivePresentationToken
```

---

## 4.30 本节证据记录模板

本节通过后，把实际观察结果记录到：

```text
docs/UIA2EBlueprintValidationLog.md
```

建议按以下格式写真实数据，不要填假示例：

```text
StatusChanged Update / Increase PIE
Target               = <Player/Enemy>
StatusId              = <实际状态>
RuntimeSequence       = <实际值>
AmountBefore          = <A>
AmountAfter           = <B>
bCreated              = false
bRemoved              = false
WidgetCountBefore     = <N>
WidgetCountDuring     = <N>
WidgetCountAfter      = <N>
Result                = PASS
FlashbackObserved     = No
DuplicateObserved     = No
ReturnedToIdle        = Yes

StatusChanged Reduction / TurnEndDecay PIE
Target               = <Player/Enemy>
StatusId              = <实际状态>
RuntimeSequence       = <实际值>
AmountBefore          = <A>
AmountAfter           = <B>
bCreated              = false
bRemoved              = false
Result                = PASS
FlashbackObserved     = No
DuplicateObserved     = No
ReturnedToIdle        = Yes
```

只有 owner 实际确认的值才能写进正式验证日志。

---

## 4.31 本节最终验收条件

要把：

```text
StatusChanged update/reduction
```

标记为 VALIDATED，至少必须同时满足：

```text
[ ] Creation 回归正常

[ ] Increase / Reapply 产生真实 bCreated=false bRemoved=false Record
[ ] Increase 精确身份匹配成功
[ ] Increase 更新同一个 Widget
[ ] Increase 无重复图标
[ ] Increase 无 A→B→A→B 闪回
[ ] Increase exact-token completion 成功
[ ] Increase 后续 Records 正常
[ ] Increase 最终回 Idle / 正常交互

[ ] Reduction / TurnEndDecay 产生真实 bCreated=false bRemoved=false Record
[ ] Reduction AmountAfter > 0
[ ] Reduction 精确身份匹配成功
[ ] Reduction 更新同一个 Widget
[ ] Reduction 无重复图标
[ ] Reduction 不误移除
[ ] Reduction 无 A→B→A→B 闪回
[ ] Reduction exact-token completion 成功
[ ] Reduction 后续 Records 正常
[ ] Reduction 最终不挂 Resolving
```

本节通过后状态更新为：

```text
StatusChanged creation          VALIDATED
StatusChanged update/reduction  VALIDATED
StatusChanged removal           NOT WIRED / FALLBACK
```

A2E 整体仍然是：

```text
PARTIAL
```

不能提前标记 COMPLETE / SEALED。

---

## 4.32 本节完成后下一步

下一节开始实现：

```text
StatusChanged Removal
```

目标是：

```text
bRemoved = true
→ 仍用 TargetPresentationId + StatusId + RuntimeSequence 精确定位已有状态 Widget
→ transient removal presentation
→ 0.5s async
→ exact-token Notify
→ reducer 正式从 ViewModel.Statuses 删除
→ final rebuild 后保持消失
→ Cancel 时从历史 ViewModel 恢复
```

下一节会详细记录：

```text
Router 如何从 bRemoved=true fallback 改为正式 async
Removal 应该隐藏还是移除 Widget
Finish 如何避免“消失→出现→消失”闪回
Cancel 如何复用第三节已经完成的历史状态重建
PIE 如何验证 Amount 1 → 0 的真实 Removal
```

---

# 第五节：实现 `StatusChanged Removal` 的 Blueprint async playback

## 5.1 本节目标与进入条件

只有第四节的 Update / Reduction PIE 已经实际通过后，才进入本节施工。

本节目标是把当前仍然走 C++ immediate fallback 的：

```text
StatusChanged
bRemoved = true
```

正式接入 Blueprint async playback。

本节只做**结构实现 + Compile + Save**，不在本节把 Removal 标记为 VALIDATED。真实 `Amount 1 → 0` PIE 验收放到下一节。

本节完成后目标状态：

```text
StatusChanged creation          VALIDATED
StatusChanged update/reduction  VALIDATED
StatusChanged removal           WIRED / NOT PIE VALIDATED
```

---

## 5.2 Removal 的冻结 Record 语义

Removal 不是“普通 update 把 Amount 显示成 0”。

正式 Removal Record 至少应满足项目实际 producer 的：

```text
Record.Type             = StatusChanged
TargetPresentationId    = 目标角色
StatusId                = 被移除状态
RuntimeSequence         = 被移除 runtime instance
AmountBefore            = A
AmountAfter             = 0
bRemoved                = true
```

通常：

```text
bCreated = false
```

但 Blueprint 不应依赖 `bCreated=false` 才识别 Removal；在 Router 与播放事件中应让：

```text
bRemoved
```

具有更高分流优先级。

原因是 Removal 的视觉语义是：

```text
状态行消失
```

而不是：

```text
状态行继续存在，只把数字改成 0
```

---

## 5.3 本节沿用的既有能力

本节不新增第二套状态 identity。

继续复用已经完成的：

```text
FindStatusWidgetByIdentity(
  TargetPresentationId,
  StatusId,
  RuntimeSequence
)
```

完整身份仍然是：

```text
TargetPresentationId
+ StatusId
+ RuntimeSequence
```

同时继续复用：

```text
ActivePresentationToken
ActivePresentationType
ActivePresentationTimer
ActiveStatusPresentationWidget
StartPresentationFinishTimer
FinishPresentationRecord
NotifyPresentationRecordFinished
```

不要新增：

```text
RemovedStatusId
RemovedStatusRuntimeSequence
RemovedStatusIndex
PendingRemovedStatusWidget
```

除非后续出现真实需求；当前都没有必要。

---

## 5.4 为什么本节选择 `Set Visibility = Collapsed`

本节锁定最小 Removal 表现为：

```text
找到精确 ExistingStatusWidget
→ Set Visibility(Collapsed)
→ 保持约 0.5s async ownership
→ Notify
→ reducer 正式删除
→ ViewModel rebuild 后状态仍不存在
```

这里选择 `Collapsed`，而不是在播放开始时直接：

```text
RemoveFromParent
```

原因：

1. `Collapsed` 明确是 Presentation-only 临时视觉状态，Widget 对象仍在当前 WrapBox 中。
2. Cancel 时第三节的 `RebuildStatusIcons` 可以无条件恢复历史正式列表，不需要判断 Widget 是否已经脱离 Parent。
3. Finish 前不需要重新创建任何 status row。
4. 不会把“临时视觉删除”和“authoritative reducer 删除”混成同一件事。

本阶段也不需要淡出动画；A2E 只要求 deterministic minimal playback。

---

## 5.5 修改 `PlayStatusChangedPresentation` 的分流顺序

打开：

```text
WBP_BattleHUD
→ Event Graph
→ PlayStatusChangedPresentation
```

第一节结束后，这个 Event 当前大致是：

```text
PlayStatusChangedPresentation
↓
ActivePresentationToken = Token
↓
ActivePresentationType = StatusChanged
↓
Break StatusChanged Payload
↓
Branch(bCreated)
├ true  → Creation
└ false → Update / Reduction
```

本节要改为：

```text
先判断 bRemoved
再判断 bCreated
```

最终顺序：

```text
PlayStatusChangedPresentation
↓
ActivePresentationToken = Token
↓
ActivePresentationType = StatusChanged
↓
Break StatusChanged Payload
↓
Branch(bRemoved)

├ true  → Removal
└ false
    ↓
  Branch(bCreated)
  ├ true  → Creation
  └ false → Update / Reduction
```

这一步非常关键。

不要写成：

```text
Branch(bCreated)
False → Branch(bRemoved)
```

虽然正常 producer 下也可能能工作，但正式语义应明确让 Removal 优先。

---

## 5.6 保留 Creation / Update 的原逻辑，只移动入口白线

新增 `Branch(bRemoved)` 后，不要重做第一节已经完成的两条路径。

操作方式：

1. 断开原来进入 `Branch(bCreated)` 的白色执行线。
2. 在其前面插入新的 `Branch`。
3. 新 Branch 的 Condition 接：

```text
Break StatusChanged Presentation Payload.bRemoved
```

4. 新 Branch 的 `False` 接回原来的：

```text
Branch(bCreated)
```

这样：

```text
bRemoved=false
```

时，Creation 与 Update/Reduction 行为完全不变。

不要改：

```text
Creation 的 Create Widget
Creation 的 AddChild
Update 的 ExistingStatusWidget.SetStatusView
Update 的 FrozenStatusView
```

---

## 5.7 新建 Removal 执行分支

从：

```text
Branch(bRemoved).True
```

开始拉白色执行线。

第一节点使用现有成员变量的 Set：

```text
Set ActiveStatusPresentationWidget
```

Value 连接：

```text
Event.ExistingStatusWidget
→ Set ActiveStatusPresentationWidget.Value
```

也就是：

```text
Removal
↓
ActiveStatusPresentationWidget = ExistingStatusWidget
```

Removal 和 Update 一样，都操作 Router 已经按 exact identity 找到的正式状态 Widget。

---

## 5.8 Removal 不调用 `SetStatusView(AmountAfter=0)`

在 Removal 分支里不要调用：

```text
ExistingStatusWidget.SetStatusView(
  MakePresentationStatusView(StatusChanged)
)
```

因为 `AmountAfter=0` 的 Removal Record 最终语义不是“显示一个 0 层状态”。

正确视觉是：

```text
状态行不可见
```

所以 Removal 分支应保持当前 Widget 的 identity / view 数据不动，只改变 Presentation visibility。

这也使 Cancel 时更容易理解：历史状态仍由 ViewModel rebuild 恢复。

---

## 5.9 从 `ExistingStatusWidget` 调用 `Set Visibility`

从 Event 输入：

```text
ExistingStatusWidget
```

蓝色对象 pin 拉线，搜索：

```text
Set Visibility
```

选择 Widget 自带的：

```text
Set Visibility
```

Target：

```text
ExistingStatusWidget
```

In Visibility 设置为：

```text
Collapsed
```

执行线：

```text
Set ActiveStatusPresentationWidget
↓
Set Visibility(Collapsed)
```

不要设为：

```text
Visible
Hit Test Invisible
Self Hit Test Invisible
```

本阶段就是要让状态行从视觉和布局中暂时消失。

---

## 5.10 Removal 分支最后启动公共 Timer

从：

```text
ExistingStatusWidget.SetVisibility(Collapsed)
```

白色执行输出连接：

```text
StartPresentationFinishTimer
```

因此 Removal Event 分支完整应为：

```text
Branch(bRemoved).True
↓
Set ActiveStatusPresentationWidget = ExistingStatusWidget
↓
ExistingStatusWidget.SetVisibility(Collapsed)
↓
StartPresentationFinishTimer
```

当前公共时长仍保持：

```text
0.5 s
Looping = false
```

不要在这里直接 Notify。

---

## 5.11 `PlayStatusChangedPresentation` 完整三分支结构

本节完成后，Event 应抽象成：

```text
PlayStatusChangedPresentation(
  StatusChanged,
  Token,
  ExistingStatusWidget
)
↓
ActivePresentationToken = Token
↓
ActivePresentationType = StatusChanged
↓
Break StatusChanged Payload
↓
Branch(bRemoved)

├ true：Removal
│    ↓
│  ActiveStatusPresentationWidget = ExistingStatusWidget
│    ↓
│  ExistingStatusWidget.SetVisibility(Collapsed)
│    ↓
│  StartPresentationFinishTimer
│
└ false
     ↓
   Branch(bCreated)

   ├ true：Creation
   │    ↓
   │  Create WBP_BattleStatus
   │    ↓
   │  ActiveStatusPresentationWidget = CreatedWidget
   │    ↓
   │  SetStatusView(FrozenStatusView)
   │    ↓
   │  AddChild to exact target WrapBox
   │    ↓
   │  StartPresentationFinishTimer
   │
   └ false：Update / Reduction
        ↓
      ActiveStatusPresentationWidget = ExistingStatusWidget
        ↓
      ExistingStatusWidget.SetStatusView(FrozenStatusView)
        ↓
      StartPresentationFinishTimer
```

三个分支都统一由同一个 Timer / Finish 生命周期管理。

---

## 5.12 修改 Router：`bRemoved=true` 不再直接 Return false

回到：

```text
WBP_BattleHUD
→ BeginPresentationRecordPlayback
→ Switch.StatusChanged
```

第二节当前结构中：

```text
TargetKnown=true
↓
Branch(bRemoved)
├ true  → Return false
└ false → Branch(bCreated)
```

本节要把：

```text
bRemoved=true → Return false
```

替换为：

```text
bRemoved=true
→ FindStatusWidgetByIdentity
→ Branch(Found)
→ Found=true 才开始 Removal async
```

---

## 5.13 在 `bRemoved=true` 分支放第二个 `FindStatusWidgetByIdentity`

从：

```text
Branch(bRemoved).True
```

拉白色执行线，放置：

```text
FindStatusWidgetByIdentity
```

这里允许与 Update 分支各有一个调用节点。

不要为了减少一个函数节点而强行把 Removal 与 Update 的执行白线合流，导致 Router 难以阅读。

Removal Find 的输入仍是：

```text
TargetPresentationId
StatusId
RuntimeSequence
```

---

## 5.14 Removal Find 的三个数据输入

连接：

```text
BreakStatusChanged.TargetPresentationId
→ FindStatusWidgetByIdentity.TargetPresentationId

BreakStatusChanged.StatusId
→ FindStatusWidgetByIdentity.StatusId

BreakStatusChanged.RuntimeSequence
→ FindStatusWidgetByIdentity.RuntimeSequence
```

再次确认：

```text
RuntimeSequence = Integer64
```

不要因为 Removal 最终会消失，就放宽 identity。

“要删哪个状态”反而更必须精确。

---

## 5.15 Removal Find 后新增 `Branch(Found)`

从 Removal 的：

```text
FindStatusWidgetByIdentity
```

白色执行输出接：

```text
Branch
```

Condition：

```text
Found
```

### False

连接：

```text
Return false
```

### True

进入 Removal 调用。

因此：

```text
bRemoved=true
↓
Find exact identity
↓
Found?
├ false → Return false
└ true  → PlayStatusChangedPresentation
```

---

## 5.16 找不到精确状态时禁止“猜删”

如果 Removal Record 指定：

```text
Target = Enemy
StatusId = Weak
RuntimeSequence = 27
```

但 WrapBox 中找不到 exact row，Blueprint 不允许：

```text
删除第一个 Weak
删除最后一个 Child
按数组 index 删除
直接 ClearChildren
```

正确行为必须是：

```text
Found=false
→ Return false
→ 交给 C++ fallback / reconciliation
```

这是 Removal 最重要的安全边界之一。

---

## 5.17 `Found=true` 时调用同一个 `PlayStatusChangedPresentation`

从：

```text
Branch(RemovalFound).True
```

放置/复制调用节点：

```text
PlayStatusChangedPresentation
```

参数接法：

```text
StatusChanged
← Record.StatusChanged

Token
← BeginPresentationRecordPlayback Function Entry.Token

ExistingStatusWidget
← Removal Find.StatusWidget
```

即：

```text
PlayStatusChangedPresentation(
  StatusChanged = Record.StatusChanged,
  Token = 当前 Record Token,
  ExistingStatusWidget = exact Found Widget
)
```

Event 内部会因为：

```text
bRemoved=true
```

自动走 Removal 分支。

---

## 5.18 Removal 调用后必须 `Return true`

调用完成后白线接：

```text
Return true
```

因为此时 Blueprint 已经：

```text
保存 Token
保存 ActivePresentationType
隐藏状态 Widget
启动 Timer
```

如果错误地 Return false，会产生：

```text
Blueprint async 已开始
+ C++ immediate fallback 又推进
```

的双重 ownership 风险。

---

## 5.19 修改后的 StatusChanged Router 完整结构

本节结束后应接近：

```text
StatusChanged
↓
TargetKnown?

├ false
│  → Return false
│
└ true
   ↓
 Branch(bRemoved)

 ├ true：Removal
 │    ↓
 │  FindStatusWidgetByIdentity(
 │    TargetPresentationId,
 │    StatusId,
 │    RuntimeSequence
 │  )
 │    ↓
 │  Branch(Found)
 │  ├ false → Return false
 │  └ true
 │      ↓
 │    PlayStatusChangedPresentation(
 │      Record.StatusChanged,
 │      Token,
 │      FoundStatusWidget
 │    )
 │      ↓
 │    Return true
 │
 └ false
      ↓
    Branch(bCreated)

    ├ true：Creation
    │  → PlayStatusChangedPresentation(..., None)
    │  → Return true
    │
    └ false：Update / Reduction
       → FindStatusWidgetByIdentity(...)
       → Found?
          ├ false → Return false
          └ true
             → PlayStatusChangedPresentation(..., FoundWidget)
             → Return true
```

这样三种 StatusChanged 都有明确路由。

---

## 5.20 Finish 路径本节原则上不需要新增分支

打开：

```text
FinishPresentationRecord
```

当前 `StatusChanged` case 已经是：

```text
StatusChanged
→ NotifyPresentationRecordFinished
→ ActiveStatusPresentationWidget = None
```

本节保持这个结构。

不要为了 Removal 增加：

```text
Set Visibility(Visible)
RemoveFromParent
RebuildStatusIcons
恢复 AmountBefore
```

正常 Removal 时，Widget 已经 Collapsed，应该一直保持消失，直到 reducer 后正式 ViewModel rebuild 删除它。

---

## 5.21 为什么 Finish 不能把 Widget 恢复 Visible

Removal 正常序列应该是：

```text
历史 Weak 1
→ Removal Record 开始
→ Weak Widget Collapsed
→ 0.5s
→ Notify exact token
→ reducer 从 ViewModel.Statuses 删除 Weak
→ HUD rebuild
→ Weak 不存在
```

正确视觉序列：

```text
显示
→ 消失
→ 继续消失
```

如果 Finish 在 Notify 前执行：

```text
SetVisibility(Visible)
```

会出现：

```text
显示
→ 消失
→ 再出现
→ reducer 后再消失
```

即错误的：

```text
disappear → reappear → disappear
```

所以正常 Finish 不做恢复。

---

## 5.22 为什么 Finish 也不需要 `RemoveFromParent`

本节使用：

```text
Collapsed
```

后，旧 Widget 仍在 WrapBox 中，但这不是问题。

Notify 后 reducer 会推进 ViewModel；当前 HUD 的正常状态刷新已经会：

```text
RebuildStatusIcons(ViewModel.Statuses, WrapBox)
```

由正式历史数组重建整个状态区。

所以不需要在 Finish 额外：

```text
RemoveFromParent(ActiveStatusPresentationWidget)
```

避免 Blueprint 维护第二套“正式删除”逻辑。

---

## 5.23 Cancel 无需再次修改

第三节已经把 StatusChanged Cancel 加固为：

```text
Cancel
→ Clear Timer
→ ActivePresentationType == StatusChanged
→ 从当前历史 ViewModel.Player.Statuses 重建 Player
→ 从当前历史 ViewModel.Enemy.Statuses 重建 Enemy
→ ActiveStatusPresentationWidget = None
→ 不 Notify
```

这套逻辑天然覆盖 Removal。

Removal 播放期间：

```text
历史 ViewModel = Weak 1
视觉 Widget = Collapsed
```

如果 Cancel：

```text
RebuildStatusIcons from ViewModel
→ Weak 1 重新创建并可见
```

因此本节不要添加：

```text
Removal Cancel 专用变量
SetVisibility(Visible) on ActiveStatusPresentationWidget
```

统一 rebuild 更可靠。

---

## 5.24 Removal Cancel 的预期时序

用于理解，不在本节强制 PIE：

```text
历史：Weak 1
↓
Removal Record active
↓
Weak Widget Collapsed
↓
发生 Cancel
↓
Timer 被清除
↓
ViewModel 仍然是 Weak 1
↓
RebuildStatusIcons
↓
Weak 1 恢复可见
↓
不 Notify 被取消 Record
```

最终：

```text
HUD = Weak 1
ViewModel = Weak 1
```

与历史一致。

---

## 5.25 不修改 `CurrentStatusView`

Removal 分支隐藏 Widget 时，不需要改：

```text
WBP_BattleStatus.CurrentStatusView
```

它继续保留该正式 row 原来的：

```text
StatusId
RuntimeSequence
AmountBefore 对应 view
```

这样在 active removal 窗口中，如果需要调试 identity，仍然可以看到原 row 身份。

不要把它改成默认 struct，也不要把 `Amount` 改成 0。

---

## 5.26 不修改 ViewModel / Gameplay

本节整个 Removal Blueprint 中禁止：

```text
Remove Index from ViewModel.Statuses
Clear ViewModel.Statuses
设置 Combatant status amount = 0
调用 Gameplay RemoveStatus
查询 UStatusInstance 再决定是否删除
```

Blueprint 只做：

```text
对 Record 已指定的 exact Widget 做 transient Collapsed
```

正式删除仍由 Controller reducer 在 exact-token completion 后完成。

---

## 5.27 不新增新的 Timer

Removal 继续使用：

```text
StartPresentationFinishTimer
```

不要再创建：

```text
RemovalTimer
StatusRemovalTimer
```

统一 Active Timer 很重要，因为：

```text
CancelPresentationRecordPlayback
```

只需要清当前：

```text
ActivePresentationTimer
```

即可停止所有当前异步表现。

---

## 5.28 Compile 前检查 `Set Visibility` Target

Compile 前确认 Removal 分支中：

```text
Set Visibility.Target
```

是：

```text
ExistingStatusWidget
```

或者已经保存到：

```text
ActiveStatusPresentationWidget
```

的同一个精确对象。

不要误连：

```text
WB_PlayerStatuses
WB_EnemyStatuses
PlayerPanel
EnemyPanel
```

否则会把整个状态区或面板隐藏掉。

---

## 5.29 Compile + Save 顺序

本节只需要改：

```text
WBP_BattleHUD
```

但仍建议先确认：

```text
WBP_BattleStatus
→ Compile Successful
```

然后：

```text
1. Compile WBP_BattleHUD
2. 查看 Compiler Results
3. 确认 0 Errors
4. Save
```

如果 `PlayStatusChangedPresentation` 调用节点因签名/图结构刷新出现 warning：

```text
Refresh Node
```

然后重新检查：

```text
Creation ExistingStatusWidget=None
Update ExistingStatusWidget=FoundWidget
Removal ExistingStatusWidget=FoundWidget
```

---

## 5.30 本节静态结构验收清单

```text
[ ] PlayStatusChangedPresentation 先判断 bRemoved
[ ] bRemoved=false 才进入原 bCreated Branch
[ ] Creation 原路径未被改坏
[ ] Update/Reduction 原路径未被改坏

[ ] Removal 使用 ExistingStatusWidget
[ ] Removal 设置 ActiveStatusPresentationWidget=ExistingStatusWidget
[ ] Removal SetVisibility Target 是 exact status widget
[ ] Removal Visibility=Collapsed
[ ] Removal 不 SetStatusView(AmountAfter=0)
[ ] Removal 不 Create Widget
[ ] Removal 不 AddChild
[ ] Removal 不 RemoveFromParent
[ ] Removal 最后 StartPresentationFinishTimer

[ ] Router bRemoved=true 不再直接 Return false
[ ] Removal Router 调 FindStatusWidgetByIdentity
[ ] Removal Find 使用 TargetPresentationId
[ ] Removal Find 使用 StatusId
[ ] Removal Find 使用 RuntimeSequence(int64)
[ ] Removal Found=false → Return false
[ ] Removal Found=true → PlayStatusChangedPresentation
[ ] Removal ExistingStatusWidget=FoundStatusWidget
[ ] Removal Token=当前 Function Entry.Token
[ ] Removal 调用后 Return true

[ ] Finish StatusChanged 没有恢复 Visible
[ ] Finish StatusChanged 没有 RemoveFromParent
[ ] Finish StatusChanged 仍 Notify 后清 ActiveStatusPresentationWidget
[ ] Cancel 仍从历史 ViewModel 重建 Statuses
[ ] Blueprint 没有修改 ViewModel.Statuses
[ ] Blueprint 没有调用 Gameplay remove status
[ ] WBP_BattleHUD Compile 0 Errors
[ ] WBP_BattleHUD 已 Save
```

---

## 5.31 本节完成后的支持矩阵

结构完成后应是：

```text
StatusChanged Creation
→ ROUTED / async / previously VALIDATED

StatusChanged Update / Increase / Reduction
→ ROUTED / async / VALIDATED（前提：第四节已通过）

StatusChanged Removal
→ exact identity found
→ ROUTED / async / NOT PIE VALIDATED

StatusChanged Removal
→ exact identity not found
→ Return false / C++ fallback

Unknown Target
→ Return false / C++ fallback
```

---

## 5.32 本节完成判定

本节结束只允许记录：

```text
StatusChanged removal = WIRED / NOT PIE VALIDATED
```

不能直接写：

```text
VALIDATED
```

因为还没有真实观察：

```text
AmountBefore > 0
AmountAfter = 0
bRemoved = true
```

的运行时行为。

下一节将专门做：

```text
StatusChanged Removal PIE Acceptance
```

重点验证：

```text
真实 bRemoved=true Record
→ exact identity Found=true
→ exact Widget 消失
→ 0.5s async
→ exact-token Notify
→ reducer 正式删除
→ final HUD 保持消失
→ 不出现 disappear→reappear→disappear
→ 不误删同 StatusId 的其他 runtime row
→ 后续 Records 继续
→ 最终不挂 Resolving
```

---

# 第六节：PIE 验收 `StatusChanged Removal`

## 6.1 本节目标

第五节已经把：

```text
StatusChanged
bRemoved = true
```

从 C++ immediate fallback 接入 Blueprint async playback。

本节只做**真实运行时验收**，不再改 Removal 的核心结构。只有本节通过后，才能把：

```text
StatusChanged removal
```

从：

```text
WIRED / NOT PIE VALIDATED
```

提升为：

```text
VALIDATED
```

本节的最低验收对象是一条真实 producer 产生的：

```text
AmountBefore = A，A > 0
AmountAfter  = 0
bRemoved     = true
```

的 `StatusChanged` Record。

---

## 6.2 PIE 前先确认第五节结构没有被编辑器刷新破坏

退出 PIE，按顺序：

```text
WBP_BattleStatus
→ Compile
→ 0 Errors
→ Save

WBP_BattleHUD
→ Compile
→ 0 Errors
→ Save
```

然后快速检查四处：

```text
① StatusChanged Router 的 bRemoved=true 分支
② Removal 的 FindStatusWidgetByIdentity
③ PlayStatusChangedPresentation 的 bRemoved=true 分支
④ Cancel Presentation Record Playback 的 StatusChanged rebuild
```

尤其确认：

```text
Removal Set Visibility.Target = ExistingStatusWidget
Visibility = Collapsed
```

而不是误接成整个 `WB_PlayerStatuses` / `WB_EnemyStatuses`。

---

## 6.3 必须使用真实 Gameplay 产生 Removal

本节不要手工：

```text
Make StatusChanged Payload
直接调用 PlayStatusChangedPresentation
直接 SetVisibility(Collapsed)
```

必须通过项目当前真实 Gameplay 规则，让状态自然从正值归零并被移除。

最常见测试方式是：

```text
先建立一个会自然衰减的状态
→ 将其减到 1
→ 再触发一次真实衰减
→ 产生 1 → 0 Removal
```

具体状态以项目当前配置为准，例如 Weak / Vulnerable 或其他真实会产生 `bRemoved=true` 的状态。

如果某状态归零时 producer 不产生 `StatusChanged bRemoved=true`，不要强行拿它验收；换一个实际支持 Removal Record 的状态。

---

## 6.4 先建立一个“正式存在”的状态行

进入 PIE 后先通过真实操作创建状态，例如：

```text
Enemy：无 Weak
→ 施加 Weak
→ Creation 完成
→ 正式 HUD 出现 Weak A
```

必须先等 Creation 的 async playback 完整结束，使：

```text
ViewModel.Statuses
```

已经正式包含这个状态。

不要在 Creation 的 0.5 秒 transient 窗口中立刻触发 Removal；那不是本节要验证的正常历史链。

---

## 6.5 把状态准备到最后一层，但先不要移除

通过真实 reapply / decay，让目标状态最终来到：

```text
Amount = 1
```

或项目规则中的最后一个正值。

此时应确认：

```text
HUD 中状态仍可见
ViewModel 中该状态仍存在
CurrentStatusView.StatusId 正确
CurrentStatusView.RuntimeSequence 正确
```

如果前一步是 Reduction：

```text
2 → 1
```

先等待该 Reduction 完整 Notify 并回到稳定状态，再触发最后的 Removal。

---

## 6.6 记录 Removal 前的精确身份

在触发最后一次衰减前，记录当前正式状态行：

```text
TargetPresentationId = <实际值>
StatusId             = <实际状态>
RuntimeSequence      = <实际 int64>
Amount               = 1（或实际最后正值）
```

本节重点不是只看“状态最后消失了”，而是证明 Blueprint 删除表现作用于 Record 指定的 exact runtime row。

如果使用 Blueprint Debugger，可以观察 `CurrentStatusView`；不要为了测试新增持久成员变量。

---

## 6.7 触发真实 Removal Record

执行会让该状态归零的真实操作，例如：

```text
EndTurn
→ TurnEndDecay
→ Weak 1 → 0
```

或项目真实的其他 Remove 原因。

确认实际 Presentation Record 至少满足：

```text
Record.Type          = StatusChanged
TargetPresentationId = 前面记录的目标
StatusId             = 前面记录的状态
RuntimeSequence      = 前面记录的 runtime sequence
AmountBefore         = A，A > 0
AmountAfter          = 0
bRemoved             = true
```

`Reason` 使用 producer 实际值，不要要求必须是某个固定枚举/文本；本节只记录真实值。

---

## 6.8 `bRemoved=true` 必须优先进入 Removal Router

运行时预期：

```text
Switch.StatusChanged
↓
TargetKnown = true
↓
Branch(bRemoved)
↓ true
FindStatusWidgetByIdentity(...)
```

不能继续落到：

```text
Branch(bCreated)
```

也不能走 Update / Reduction 的 `SetStatusView(AmountAfter=0)`。

如果 Debugger 显示 Removal 进入了 Update 路径，立即停止验收，回查第五节的分流顺序。

---

## 6.9 Removal 的 exact identity 必须 `Found=true`

`FindStatusWidgetByIdentity` 输入必须与 Record 一致：

```text
TargetPresentationId
StatusId
RuntimeSequence(int64)
```

预期：

```text
Found = true
StatusWidget = 当前正式状态行
```

如果 `Found=false`，不要为了通过测试放宽成只按 `StatusId`。

按顺序排查：

```text
1. 当前 WrapBox 中状态行是否已经正式存在
2. CurrentStatusView 是否保存正确身份
3. StatusId 是否一致
4. RuntimeSequence 是否一致
5. TargetPresentationId 是否选中了正确 Player / Enemy WrapBox
6. 是否在 Removal 前发生了意外 Rebuild，导致 runtime identity 不一致
```

找不到 exact row 时 Router 正确行为仍是：

```text
Return false
→ C++ fallback
```

但这种情况不能作为 Removal Blueprint async 验收 PASS。

---

## 6.10 Removal 开始时只允许精确状态行消失

`Found=true` 后：

```text
PlayStatusChangedPresentation
→ ActiveStatusPresentationWidget = FoundWidget
→ SetVisibility(Collapsed)
```

视觉上应看到：

```text
目标状态图标消失
```

同时不能发生：

```text
整个 Player 状态区消失
整个 Enemy 状态区消失
另一个不同 StatusId 的图标消失
同目标其他状态全部清空
```

如果目标同时有多个状态，建议保留至少一个其他状态作为旁证，例如：

```text
Weak 1 + Vulnerable 2
→ 移除 Weak
→ Vulnerable 2 必须仍可见
```

---

## 6.11 Removal 期间不能短暂显示 `0`

正确视觉序列：

```text
Weak 1
→ Weak 直接消失
```

不能看到：

```text
Weak 1
→ Weak 0
→ 消失
```

如果出现 `Weak 0`，检查：

```text
Removal 分支是否错误调用 ExistingStatusWidget.SetStatusView(FrozenStatusView)
```

第五节锁定的 Removal 最小表现是：

```text
Collapsed
```

而不是把 `AmountAfter=0` 渲染出来。

---

## 6.12 Removal 的 0.5 秒 async 窗口语义

Removal 开始后约 0.5 秒内应处于：

```text
HUD：exact status row 已 Collapsed
历史 ViewModel：该 status row 仍然存在
Controller：当前 Removal Record 仍由 Blueprint 持有
```

这不是数据不一致，而是 committed-presentation 的正常历史播放窗口。

在 Notify 之前，不应该由 Blueprint 主动从 `ViewModel.Statuses` 删除该状态。

---

## 6.13 Removal 期间 Widget 可以仍在 WrapBox 中

因为第五节使用的是：

```text
SetVisibility(Collapsed)
```

所以在 0.5 秒窗口里：

```text
WrapBox Child 数量可能仍包含该 Widget
```

这本身不是失败。

本节视觉要求是：

```text
目标状态不可见且不占布局
```

正式 Widget 数组/WrapBox 的 authoritative 重建发生在：

```text
Notify
→ reducer
→ ViewModel refresh
```

之后。

不要因为 Child 仍存在就临时加入 `RemoveFromParent`。

---

## 6.14 约 0.5 秒后必须 exact-token 正常完成

预期链：

```text
ActivePresentationTimer
↓
FinishPresentationRecord
↓
StatusChanged case
↓
NotifyPresentationRecordFinished
↓
NotifyPresentationFinished(ActivePresentationToken)
↓
Controller 接受当前 exact token
```

关键检查：

```text
Notify 时 ActivePresentationToken 仍然是当前 Removal Token
```

不能先：

```text
ActivePresentationToken = default
```

再 Notify。

---

## 6.15 Notify 后 reducer 才正式删除状态

exact-token completion 被接受后，预期：

```text
StatusChanged reducer
→ 从当前 WorkingSnapshot / ViewModel 对应 combatant statuses 中删除 exact runtime row
```

随后正常 HUD refresh：

```text
RebuildStatusIcons(ViewModel.Player.Statuses, WB_PlayerStatuses)
或
RebuildStatusIcons(ViewModel.Enemy.Statuses, WB_EnemyStatuses)
```

最终 authoritative HUD 中：

```text
该状态不存在
```

这一步才是正式数据层面的删除结果。

---

## 6.16 最重要检查：不能出现“消失 → 再出现 → 再消失”

Removal 正确视觉序列必须是：

```text
可见
→ Collapsed
→ 保持不存在
```

禁止：

```text
可见
→ Collapsed
→ Visible
→ 再消失
```

也就是不能出现：

```text
disappear → reappear → disappear
```

如果出现闪回，重点检查：

```text
1. FinishPresentationRecord.StatusChanged 是否 SetVisibility(Visible)
2. Finish 是否在 Notify 前 RebuildStatusIcons(old ViewModel)
3. 是否在 Notify 前手动调用 RefreshCombatantPresentations
4. 是否有旧的 Cancel/cleanup 路径错误触发
5. reducer 推进前是否收到旧 ViewModel refresh
```

正常 Finish 不恢复状态行。

---

## 6.17 最终 WrapBox 中不应保留正式可见的旧行

Notify + reducer + HUD rebuild 完成后，检查：

```text
被移除的 StatusId / RuntimeSequence
```

不再出现在目标正式状态列表。

如果只能观察视觉，至少确认状态持续不存在；如果可通过 Blueprint Debugger / 当前 ViewModel 状态数组观察，则确认数组中 exact runtime row 已不存在。

不要把“旧 Widget 只是 Collapsed 但 authoritative ViewModel 仍保留它”误判为 PASS。

最终必须是：

```text
ViewModel 也已经删除
```

---

## 6.18 其他状态必须保持不变

如果目标有其他状态，例如：

```text
Weak 1
Vulnerable 2
```

Removal 只针对 Weak，则最终应是：

```text
Vulnerable 2
```

不能因为 HUD rebuild 或错误 `ClearChildren` 逻辑导致其他状态丢失。

如果其他状态在 reducer 后也根据真实 Gameplay Record 发生变化，则以实际后续 Records 为准；本条 Removal 自身不能越权删除它们。

---

## 6.19 同 `StatusId` 多 runtime row 的精确性抽查（若项目可构造）

如果当前 Gameplay 允许同一 combatant 同时存在：

```text
StatusId 相同
RuntimeSequence 不同
```

的两条状态，则这是最强 identity 验收。

例如：

```text
Weak seq=17
Weak seq=24
```

Removal Record 指定：

```text
Weak seq=17
```

则只允许 `seq=17` 消失，`seq=24` 必须保留。

如果当前 Gameplay 设计不允许这种并存，不需要为了测试修改规则。本项记录为：

```text
Not Applicable / 当前规则不可构造
```

即可，不能因此阻塞基本 Removal 验收。

---

## 6.20 继续观察 Removal 后面的 Records

如果 Removal 来自 EndTurn / TurnStart 等大 Envelope，不要在状态消失后立刻停止 PIE。

继续观察后续 Record 是否按 producer 顺序推进。

可能包括：

```text
BlockChanged
Damage
CardZoneChanged
EnergyChanged（若此时仍 fallback）
DeckShuffled（若此时仍 fallback）
其他 StatusChanged
```

以项目实际输出为准。

必须满足：

```text
Removal 不吞掉后续 Record
Removal 不导致重复播放
最终不会卡在 Resolving
```

---

## 6.21 最终必须恢复到正常交互 / Idle

完整 Envelope 处理结束后确认：

```text
Presentation backlog 正常耗尽
Controller 不再等待 Removal Token
HUD 与 ViewModel 一致
输入状态按正常游戏流程恢复
```

不能出现：

```text
状态已经消失
但 EndTurn 后永远不能继续操作
```

若发生，优先检查：

```text
Router 是否 Return true 但 Timer 没启动
Finish 是否没有进入 StatusChanged case
Notify 是否使用了错误/默认 Token
Notify 是否在清 Token 之后执行
```

---

## 6.22 Removal Cancel 运行时抽查（有正式入口时执行）

如果项目当前存在可靠正式 Cancel 入口，可以额外验证：

```text
历史 ViewModel：Weak 1
↓
Removal 开始
↓
Weak Widget Collapsed
↓
0.5 秒内触发正式 Cancel
```

预期：

```text
Clear Timer
→ StatusChanged Cancel
→ RebuildStatusIcons from historical ViewModel
→ Weak 1 恢复可见
→ 不 Notify 被取消 Removal
```

最终：

```text
HUD = Weak 1
ViewModel = Weak 1
```

如果当前没有可控的正式 Cancel 入口，不要为了本节在 Blueprint 中硬调用 Cancel。把运行时 Cancel 证明留到后面的统一 Cancel/Reconcile 验收。

---

## 6.23 如果 Removal 仍走 fallback，排查顺序

看到 `bRemoved=true` Record 但没有约 0.5 秒 async 窗口时，依次检查：

```text
1. TargetKnown 是否为 true
2. bRemoved Branch 是否接到了 Removal Find
3. Removal Find 是否 Found=true
4. Found=true 是否调用 PlayStatusChangedPresentation
5. ExistingStatusWidget 是否接 FoundStatusWidget
6. 调用后是否 Return true
7. Event 内 bRemoved=true 是否进入 Collapsed 分支
8. StartPresentationFinishTimer 是否执行
```

不要通过“把所有 Removal 都 Return true”绕过 Find 校验。

---

## 6.24 如果状态没有消失，排查顺序

如果 Router 已命中但图标仍可见：

```text
1. SetVisibility 是否真的执行
2. Target 是否是 ExistingStatusWidget
3. Visibility 是否是 Collapsed
4. 是否误对另一个 WBP_BattleStatus 执行
5. 紧接着是否有旧 ViewModel rebuild 把状态重新创建
```

特别注意：

```text
Collapsed
```

不是 `Hidden` 的替代要求；本节锁定的是 `Collapsed`，让布局也立即收缩。

---

## 6.25 如果状态消失后马上又出现，排查顺序

如果看到：

```text
消失 → 很快重新出现
```

先区分它是否最后又消失。

如果是：

```text
消失 → 出现 → 消失
```

重点查 Finish/旧 ViewModel rebuild。

如果是：

```text
消失 → 出现并一直存在
```

重点查：

```text
1. reducer 是否真正应用 bRemoved
2. RuntimeSequence 是否匹配 reducer 中的 exact row
3. Notify 是否被 Controller 接受
4. 是否实际走了 fallback 而 Blueprint visual 与 reducer 时序不一致
```

不要在 Blueprint Finish 中再手动删除 ViewModel 来掩盖 reducer 问题。

---

## 6.26 如果误删了其他状态，排查顺序

如果 Removal 后其他状态也消失：

```text
1. SetVisibility Target 是否错误接到 WrapBox
2. 是否调用了 ClearChildren
3. 是否错误调用 RebuildStatusIcons(empty array)
4. Player / Enemy Statuses 是否接反
5. exact identity lookup 是否选择了错误目标容器
```

正常 Removal Event 自身只应该：

```text
Collapse 一个 exact WBP_BattleStatus
```

---

## 6.27 Creation / Update / Reduction 回归抽查

Removal 验收通过后，不需要重新完整跑第四节，但至少做最小 smoke：

```text
Creation：无状态 → 状态出现
Update：A → B，B > 0
Reduction：B → C，C > 0
Removal：C → 0
```

确认第五节新增的：

```text
Branch(bRemoved)
```

没有破坏 `bRemoved=false` 的旧路径。

最低视觉序列：

```text
无 → A → B → C → 无
```

其中不能出现重复图标或错误闪回。

---

## 6.28 本节正式验收清单

要把 Removal 标为 VALIDATED，至少满足：

```text
[ ] 使用真实 Gameplay producer 产生 Removal
[ ] Record.Type=StatusChanged
[ ] AmountBefore>0
[ ] AmountAfter=0
[ ] bRemoved=true

[ ] TargetPresentationId 正确
[ ] StatusId 正确
[ ] RuntimeSequence 正确且保持 exact identity
[ ] TargetKnown=true
[ ] Router 进入 bRemoved=true 分支
[ ] FindStatusWidgetByIdentity Found=true
[ ] FoundWidget 是正确状态行

[ ] Removal 不调用 SetStatusView(AmountAfter=0)
[ ] Removal 不显示 0 层状态
[ ] Removal SetVisibility=Collapsed
[ ] 只有目标状态行消失
[ ] 其他无关状态保持正常

[ ] StartPresentationFinishTimer 正常执行
[ ] 约 0.5s async ownership 正常
[ ] Finish 进入 StatusChanged case
[ ] exact-token Notify 正常
[ ] Notify 前未清 ActivePresentationToken

[ ] reducer 后 ViewModel 正式删除 exact runtime row
[ ] HUD rebuild 后状态持续不存在
[ ] 没有 disappear→reappear→disappear 闪回
[ ] 没有误删其他 status row
[ ] 后续 Records 继续
[ ] 最终不挂 Resolving
[ ] 最终回到正常交互/下一阶段

[ ] Creation smoke 仍正常
[ ] Update/Reduction smoke 仍正常
```

---

## 6.29 正式验证日志模板

本节通过后，再把**实际观察值**写入：

```text
docs/UIA2EBlueprintValidationLog.md
```

建议格式：

```text
StatusChanged Removal PIE
Target                 = <Player/Enemy>
TargetPresentationId   = <实际值>
StatusId                = <实际状态>
RuntimeSequence         = <实际 int64>
AmountBefore            = <实际 A>
AmountAfter             = 0
bCreated                = <实际值>
bRemoved                = true
Reason                  = <实际 Reason>
ExactWidgetFound        = Yes
VisibilityDuringPlayback= Collapsed
ZeroAmountRendered      = No
ReappearFlashObserved   = No
OtherStatusAffected     = No
ExactTokenCompleted     = Yes
RemovedFromFinalViewModel = Yes
ReturnedToIdle          = Yes
Result                  = PASS
```

如果某字段当前调试输出不可见，不要编造数值；可以记录：

```text
Not Observed
```

然后只对已实际证明的项做结论。

---

## 6.30 本节完成后的 StatusChanged 状态

本节完全通过后，StatusChanged 三类都可以记为：

```text
StatusChanged creation          VALIDATED
StatusChanged update/reduction  VALIDATED
StatusChanged removal           VALIDATED
```

这意味着：

```text
StatusChanged Blueprint Playback = COMPLETE for A2E scope
```

但**不代表整个 UI-A2E 完成**。

后面还至少有：

```text
EnergyChanged
DeckShuffled
Terminal：Victory / Defeat / ResolutionFault
统一 Cancel / Reconcile 收尾
A2E 全链 PIE Acceptance
最终文档 Seal
```

---

## 6.31 本节完成判定

只有 owner 实际确认上述 Removal PIE 通过后，才允许把 validation log 改为：

```text
StatusChanged removal = VALIDATED
```

如果只完成蓝图结构、没有 PIE，则保持：

```text
WIRED / NOT PIE VALIDATED
```

如果 PIE 中某项失败，则记录失败现象并回到对应结构修复，不要提前进入 EnergyChanged。

---

## 6.32 下一节

下一节开始实现：

```text
EnergyChanged Blueprint Playback
```

重点会记录：

```text
哪些 Energy 变化应由 EnergyChanged Record 表现
CardPlayed cost 为什么不能重复产生 EnergyChanged 表现
如何只消费冻结 EnergyBefore / EnergyAfter
如何更新 EnergyPanel 而不修改 ViewModel
如何接 Router / Timer / Finish / Cancel
如何保持 terminal energy nuance
最后如何做 Energy PIE 验收
```

---

# 第七节：实现并验收 `EnergyChanged` Blueprint Playback

## 7.1 本节目标与进入条件

只有第六节的 `StatusChanged Removal` PIE 已经由 owner 实际确认通过后，才进入本节施工。

本节目标是把当前仍然在 `BeginPresentationRecordPlayback` 中直接 `Return false`、交给 C++ immediate fallback 的：

```text
EnergyChanged
```

正式接入 Blueprint async playback，并完成一次真实 PIE 验收。

本节结束后，只有在真实运行时验证通过的情况下，才允许把：

```text
EnergyChanged
```

从：

```text
NOT WIRED
```

提升为：

```text
VALIDATED
```

本节同时锁定三个重要边界：

```text
1. 卡牌支付 Cost 的能量事实只属于 CardPlayed
2. 独立 EnergyChanged 只表现真实独立能量变化
3. Blueprint 只消费冻结 Record，不修改 ViewModel / Gameplay
```

---

## 7.2 先确认正式 Payload 契约

当前 C++ `FEnergyChangedPresentationPayload` 的字段只有：

```text
EnergyBefore : int32
EnergyAfter  : int32
Delta        : int32
```

因此本节 Blueprint 的事实来源必须固定为这三个字段。

对一条合法 EnergyChanged，正常关系应为：

```text
Delta = EnergyAfter - EnergyBefore
```

例如：

```text
EnergyBefore = 0
EnergyAfter  = 3
Delta        = +3
```

或：

```text
EnergyBefore = 3
EnergyAfter  = 2
Delta        = -1
```

但视觉最终值不要通过：

```text
EnergyBefore + Delta
```

重新计算。

正式显示值直接使用：

```text
EnergyAfter
```

`Delta` 只用于一致性检查、调试和将来可选的 `+N / -N` 辅助效果。

---

## 7.3 再次锁定：CardPlayed Cost 不是 EnergyChanged

这是本节最重要的语义边界之一。

当前 `FCardPlayedPresentationPayload` 已经冻结：

```text
EnergyBefore
EnergyAfter
CostPaid
```

因此一次普通 1 费卡牌：

```text
Energy 5 → 4
CostPaid = 1
```

这次支付事实已经属于：

```text
CardPlayed
```

**不得再为同一张卡、同一次 Cost 支付生成或表现一条：**

```text
EnergyChanged 5 → 4
```

否则同一个能量消耗会被表现两次。

正式规则保持：

```text
卡牌 Cost
→ CardPlayed.EnergyBefore / EnergyAfter / CostPaid
→ 只有一份事实

独立能量变化
→ EnergyChanged.EnergyBefore / EnergyAfter / Delta
→ 另一份事实
```

如果 PIE 中发现“打出 1 费牌以后紧跟一条完全对应 Cost 的 `EnergyChanged -1`”，不要在 Blueprint Router 里用时序启发式过滤它。

不要写：

```text
如果上一条是 CardPlayed，就忽略 EnergyChanged
```

这种逻辑会掩盖 producer 的事实重复。

正确处理是：

```text
记录为 producer / committed-record contract 问题
→ 回 C++ producer 修复
```

Blueprint 不负责猜测哪条 Record 是重复的。

---

## 7.4 哪些变化才属于 `EnergyChanged`

本节只接管项目真实 producer 已经提交的独立 `EnergyChanged` Record，例如可能来自：

```text
TurnStart / turn refill
独立 GainEnergy 操作
独立 LoseEnergy 操作
其他非 Card Cost 的实际能量变化
```

具体来源以项目当前真实 producer 为准。

不要为了本节自行假设某个操作一定会生成 EnergyChanged；先以真实 Record 为证据。

同时继续遵守：

```text
No-op = no Presentation Record
```

例如某个操作执行后：

```text
EnergyBefore = 3
EnergyAfter  = 3
```

正式 producer 应当不产生可见 `EnergyChanged`。

本节 Router 会增加防御性检查，避免 Blueprint 对明显 no-op / 不一致 payload 启动一个假的 0.5 秒表现。

---

## 7.5 先定位当前 HUD 已有 Energy 显示，不创建第二套 UI

打开：

```text
WBP_BattleHUD
```

进入：

```text
Event Battle HUD View Model Changed
```

当前主刷新 Sequence 已经有一条负责：

```text
ViewModel.Energy
→ 更新 EnergyPanel 中现有能量文本/显示控件
```

本节第一步不是新建 `Txt_EnergyPresentation`。

而是找到**当前正式 HUD 已经使用的那个 Energy 数值显示控件**。

由于当前快照没有锁定该子控件的实际变量名，本文后续统一用逻辑别名：

```text
EnergyValueText
```

表示：

```text
“Event Battle HUD View Model Changed 中当前已经被用于显示 Energy 的那个 TextBlock/数值控件”
```

注意：

```text
EnergyValueText 只是本文逻辑别名
```

不要为了和文档一致而强行把现有 Widget 改名。

---

## 7.6 如何确认自己找到了正确 Energy 控件

在 `Event Battle HUD View Model Changed` 中，从：

```text
Get ViewModel
→ Get Energy
```

沿数据线向右追踪。

最终应落到当前 EnergyPanel 下某个：

```text
Set Text
或等价能量数值刷新节点
```

选择这个节点的 Target，确认 Designer 中它确实属于：

```text
EnergyPanel
```

并且当前正常 HUD 的 Energy 数字就是由它显示。

本节所有 transient EnergyAfter 和 Cancel restore 都复用这个正式控件。

不要：

```text
新增第二个 EnergyPanel
新增一份覆盖在原 EnergyPanel 上的 Energy Text
把 Txt_DamagePresentation 当能量文本复用
```

---

## 7.7 如果当前 Energy UI 是单纯数字，直接复用

如果当前正式 HUD 是：

```text
3
```

这种单纯当前能量数字，那么本节最直接：

```text
EnergyAfter
→ ToText(Integer)
→ EnergyValueText.SetText
```

即可。

这是推荐的最小路径。

---

## 7.8 如果当前 Energy UI 是 `Current / Max`，先确认格式来源

如果你在当前 Blueprint 中发现 Energy 显示实际是：

```text
3 / 3
```

并且 Current 和 Max 被拼成同一个 TextBlock，则不要凭空从 Gameplay 查询 MaxEnergy。

先检查正常 HUD 当前是如何构造这个文本的。

本节锁定：

```text
当前 Record 的“变化事实”只能来自 FEnergyChangedPresentationPayload
```

因此：

- `Current` 必须用 `EnergyAfter`；
- 不允许为了得到 Max 去查询 Gameplay BattleState / Character / AbilitySystem；
- 如果 Max 是独立静态 TextBlock，不需要碰它；
- 如果 Max 与 Current 强耦合在同一个文本且没有安全的 presentation-only 格式入口，先停止此小步，检查现有 UI 架构再接，不要现场发明一个 live gameplay query。

Cancel reconciliation 阶段可以使用当前历史 `ViewModel` 恢复正式 UI，因为 Cancel 的职责就是回到历史 ViewModel；但 active Record 的新 Energy 值仍必须来自 `EnergyAfter`。

---

## 7.9 新建 `PlayEnergyChangedPresentation` Custom Event

在：

```text
WBP_BattleHUD
→ Event Graph
```

新建 Custom Event：

```text
PlayEnergyChangedPresentation
```

不要带：

```text
A2E
Phase6
```

等阶段前缀。

新增两个输入：

```text
EnergyChanged : FEnergyChangedPresentationPayload
Token         : FPresentationPlaybackToken
```

类型必须是：

```text
EnergyChanged → Energy Changed Presentation Payload struct
Token         → Presentation Playback Token struct
```

---

## 7.10 Event 开始先保存当前 Token

从：

```text
PlayEnergyChangedPresentation
```

白色执行线接：

```text
Set ActivePresentationToken
```

Value：

```text
Event.Token
→ ActivePresentationToken
```

所以开头是：

```text
PlayEnergyChangedPresentation(EnergyChanged, Token)
↓
ActivePresentationToken = Token
```

与现有 Damage / Block / Status 路径保持同一 ownership 模式。

---

## 7.11 第二步设置 `ActivePresentationType = EnergyChanged`

从：

```text
Set ActivePresentationToken
```

继续接：

```text
Set ActivePresentationType
```

枚举值选择：

```text
EnergyChanged
```

形成：

```text
PlayEnergyChangedPresentation
↓
ActivePresentationToken = Token
↓
ActivePresentationType = EnergyChanged
```

不要等更新完文本以后才设置 Type。

Finish / Cancel 都依赖这个 Type 判断当前视觉所有者。

---

## 7.12 放置 `Break Energy Changed Presentation Payload`

从 Event 的：

```text
EnergyChanged
```

struct pin 拖线，选择：

```text
Break Energy Changed Presentation Payload
```

需要看到：

```text
EnergyBefore
EnergyAfter
Delta
```

建议只放一个 Break 节点，长数据线使用普通 Reroute Node。

不要把 Payload `Promote to Variable`。

---

## 7.13 Active playback 只把 `EnergyAfter` 写到正式 Energy 控件

从：

```text
Break EnergyChanged.EnergyAfter
```

拉：

```text
ToText (Integer)
```

然后连接到前面定位到的正式能量数值控件：

```text
EnergyValueText.SetText
```

数据链：

```text
EnergyAfter
→ ToText(Integer)
→ EnergyValueText.SetText.InText
```

执行链：

```text
Set ActivePresentationType = EnergyChanged
↓
EnergyValueText.SetText
```

如果当前正常 Energy 显示已经有一个稳定的、只消费 Current Energy 的格式函数，可以复用它；但传入的 Current 必须是：

```text
EnergyAfter
```

---

## 7.14 禁止用 `EnergyBefore + Delta` 得到最终显示值

即使：

```text
EnergyBefore + Delta == EnergyAfter
```

也不要把 Blueprint 写成：

```text
EnergyBefore
+ Delta
→ SetText
```

正式 A2E 语义要求：

```text
Record 已经冻结了最终可见事实
→ UI 直接消费最终字段
```

因此：

```text
最终显示 = EnergyAfter
```

`Delta` 不是计算输入。

后续如果要做：

```text
+2
-1
```

浮字动画，`Delta` 可以作为辅助视觉字段使用；本节不需要额外实现。

---

## 7.15 不修改 ViewModel.Energy

整个 Event 中禁止：

```text
Set ViewModel.Energy
修改 Battle State Energy
调用 GainEnergy
调用 SpendEnergy
调用任何 Gameplay Energy API
```

本节的：

```text
EnergyValueText.SetText(EnergyAfter)
```

只是 Presentation transient override。

真正的历史 ViewModel 推进发生在：

```text
exact-token completion
→ Controller reducer
```

之后。

---

## 7.16 更新 EnergyAfter 后启动公共 Finish Timer

从：

```text
EnergyValueText.SetText
```

白色执行输出接：

```text
StartPresentationFinishTimer
```

因此完整 Event 最小路径：

```text
PlayEnergyChangedPresentation(EnergyChanged, Token)
↓
ActivePresentationToken = Token
↓
ActivePresentationType = EnergyChanged
↓
Break EnergyChanged
↓
EnergyAfter
→ ToText(Integer)
→ EnergyValueText.SetText
↓
StartPresentationFinishTimer
```

继续使用现有：

```text
Time = 0.5
Looping = false
Event = FinishPresentationRecord
```

不要新增 `EnergyTimer`。

---

## 7.17 EnergyChanged 不需要目标身份校验

`FEnergyChangedPresentationPayload` 当前没有：

```text
TargetPresentationId
```

因为它表达的是当前战斗中的 Energy 资源变化，而不是 Player / Enemy 状态行变化。

所以 Router 不需要：

```text
TargetKnown
Player.PresentationId
Enemy.PresentationId
```

也不要现场猜一个 Target。

Energy 的结构校验重点是：

```text
确实发生变化
+ Delta 自洽
```

---

## 7.18 修改 `BeginPresentationRecordPlayback` 的 `EnergyChanged` case

打开：

```text
WBP_BattleHUD
→ BeginPresentationRecordPlayback
```

在：

```text
Switch on EBattlePresentationRecordType
```

找到当前：

```text
EnergyChanged
```

旧保存版本目前是：

```text
EnergyChanged
→ Return false
```

本节把它改成正式 Router。

先不要碰：

```text
DeckShuffled
Victory
Defeat
ResolutionFault
```

---

## 7.19 Router 先 Break `Record.EnergyChanged`

从当前完整 `Record` 的：

```text
EnergyChanged
```

struct pin 接：

```text
Break Energy Changed Presentation Payload
```

至少使用：

```text
EnergyBefore
EnergyAfter
Delta
```

Router 不需要保存成员变量。

---

## 7.20 第一项防御校验：必须是真实变化

放一个：

```text
Not Equal (Integer)
```

连接：

```text
A ← EnergyBefore
B ← EnergyAfter
```

得到：

```text
HasEnergyChange
= EnergyBefore != EnergyAfter
```

这与 A2 的：

```text
No-op → no Presentation Record
```

一致。

如果一个异常 Record 是：

```text
3 → 3
Delta = 0
```

Blueprint 不应该为它启动 0.5 秒空表现。

---

## 7.21 第二项防御校验：`Delta` 必须自洽

从：

```text
EnergyAfter
```

减去：

```text
EnergyBefore
```

使用：

```text
Integer - Integer
```

得到：

```text
ExpectedDelta = EnergyAfter - EnergyBefore
```

再用：

```text
Equal (Integer)
```

比较：

```text
ExpectedDelta == Delta
```

得到：

```text
DeltaConsistent
```

注意：这个运算只用于验证 payload 一致性。

它**不是**在重新计算最终 Energy 显示。

最终 UI 仍直接消费 `EnergyAfter`。

---

## 7.22 两个校验做 AND

放：

```text
AND Boolean
```

连接：

```text
A ← HasEnergyChange
B ← DeltaConsistent
```

得到：

```text
CanPlayEnergyChanged
```

然后从：

```text
Switch.EnergyChanged
```

白色执行线接：

```text
Branch(CanPlayEnergyChanged)
```

---

## 7.23 Router False 必须 `Return false`

如果：

```text
EnergyBefore == EnergyAfter
```

或者：

```text
Delta != EnergyAfter - EnergyBefore
```

则：

```text
Branch.False
→ Return false
```

这意味着 Blueprint 拒绝接管一个明显无意义或不一致的 Record，让 Controller 走已有 fallback/reconcile。

不要：

```text
修正 Delta
Clamp EnergyAfter
自己猜正确 Energy
```

Blueprint 不是 Record 修复器。

---

## 7.24 Router True 调 `PlayEnergyChangedPresentation`

从：

```text
Branch(CanPlayEnergyChanged).True
```

调用：

```text
PlayEnergyChangedPresentation
```

参数：

```text
EnergyChanged
← Record.EnergyChanged

Token
← BeginPresentationRecordPlayback Function Entry.Token
```

不要自己重新 Make 一个 EnergyChanged Payload。

Token 必须使用当前 Record 的原 Token。

---

## 7.25 调用后必须 `Return true`

调用之后：

```text
PlayEnergyChangedPresentation
↓
Return true
```

因为 Event 已经：

```text
保存 Active Token
设置 Active Type
覆盖 Energy visual
启动 Timer
```

这就是明确的 async ownership。

如果调用后错误 `Return false`，会造成：

```text
Blueprint 已开始异步
+ C++ 又 immediate fallback
```

的双推进风险。

---

## 7.26 EnergyChanged Router 完整结构

最终应接近：

```text
Switch on EBattlePresentationRecordType
└─ EnergyChanged
    ↓
  Break EnergyChanged Payload
    ↓
  HasEnergyChange
  = EnergyBefore != EnergyAfter
    ↓
  ExpectedDelta
  = EnergyAfter - EnergyBefore
    ↓
  DeltaConsistent
  = ExpectedDelta == Delta
    ↓
  AND
    ↓
  Branch(CanPlayEnergyChanged)

  ├ false
  │   → Return false
  │
  └ true
      ↓
    PlayEnergyChangedPresentation(
      Record.EnergyChanged,
      FunctionEntry.Token
    )
      ↓
    Return true
```

Router 中不要直接 `SetText`。

保持：

```text
Router = validation + routing
Event  = visual + timer ownership
```

---

## 7.27 给 `FinishPresentationRecord` 增加 `EnergyChanged` case

打开：

```text
FinishPresentationRecord
```

找到：

```text
Switch on ActivePresentationType
```

为：

```text
EnergyChanged
```

增加完成执行线。

最小正确路径只有：

```text
EnergyChanged
→ NotifyPresentationRecordFinished
```

不需要额外恢复文本。

---

## 7.28 正常 Finish 绝对不要恢复 `EnergyBefore`

假设：

```text
历史 ViewModel Energy = 0
Record EnergyChanged 0 → 3
```

播放开始：

```text
HUD transient = 3
ViewModel historical = 0
```

正确完成：

```text
HUD 3
→ Notify
→ reducer ViewModel = 3
→ normal HUD refresh
→ HUD 仍 3
```

正确视觉序列：

```text
0 → 3 → 3
```

禁止在 Finish 做：

```text
EnergyValueText.SetText(EnergyBefore)
```

否则变成：

```text
0 → 3 → 0 → 3
```

闪回。

---

## 7.29 `NotifyPresentationRecordFinished` 顺序保持不变

现有公共完成事件继续保持：

```text
ClearAndInvalidateTimerByHandle(ActivePresentationTimer)
↓
NotifyPresentationFinished(ActivePresentationToken)
↓
ActivePresentationType = None
↓
ActivePresentationToken = default
```

关键点：

```text
Notify 必须使用仍然有效的 ActivePresentationToken
```

不要在 Energy 分支里提前：

```text
ActivePresentationToken = default
```

---

## 7.30 EnergyChanged Cancel 必须恢复“历史 ViewModel Energy”

和 Status update 一样，EnergyChanged active playback 会把正式 HUD 控件临时改成未来 Record 的 `EnergyAfter`。

因此 Cancel 不能只：

```text
Clear Timer
→ 清 Token
```

否则例如：

```text
历史 ViewModel Energy = 0
transient EnergyAfter = 3
发生 Cancel
```

HUD 可能继续残留：

```text
3
```

这与历史 ViewModel 不一致。

所以本节必须给 `Cancel Presentation Record Playback` 增加 EnergyChanged 专用 reconciliation。

---

## 7.31 扩展第三节已有 Cancel `Sequence`

第三节建议的 Cancel 状态恢复结构是：

```text
公共清理
↓
Sequence
├ Then 0 → StatusChanged restore
└ Then 1 → 公共尾部清理
```

本节建议给这个 Sequence 增加一个输出。

选中 `Sequence`，点击：

```text
Add pin
```

使其变成：

```text
Then 0
Then 1
Then 2
```

重新安排为：

```text
Then 0 → StatusChanged restore（保持原逻辑）
Then 1 → EnergyChanged restore（本节新增）
Then 2 → 原公共尾部清理
```

也就是把原来接在 `Then 1` 的：

```text
ActiveStatusPresentationWidget = None
→ 清其他 transient refs
→ ActivePresentationType = None
→ ActivePresentationToken = default
```

整体移到：

```text
Then 2
```

不要复制一份公共尾部。

---

## 7.32 `Sequence.Then 1` 判断当前是否 `EnergyChanged`

从：

```text
Get ActivePresentationType
```

做枚举比较：

```text
ActivePresentationType == EnergyChanged
```

得到：

```text
IsEnergyChangedCancel
```

从：

```text
Sequence.Then 1
```

接：

```text
Branch(IsEnergyChangedCancel)
```

### False

保持不连接。

### True

进入 ViewModel 有效性检查。

注意仍然必须在：

```text
ActivePresentationType = None
```

之前判断。

---

## 7.33 Energy Cancel 检查 `ViewModel` 有效性

从：

```text
Branch(IsEnergyChangedCancel).True
```

接带执行 pin 的：

```text
Is Valid
```

Object：

```text
ViewModel
```

### Is Valid

恢复历史 Energy。

### Is Not Valid

不进行猜测性恢复，让公共尾部继续清理 Blueprint transient ownership。

不要保存一个额外的 `PreviousEnergy` 作为 fallback。

---

## 7.34 Cancel 时用 `ViewModel.Energy` 恢复正式 HUD

在：

```text
IsValid(ViewModel).Is Valid
```

路径中：

```text
Get ViewModel.Energy
→ 使用当前正常 HUD 相同的 Energy 文本格式
→ EnergyValueText.SetText
```

如果正式 Energy 控件是单纯数字：

```text
ViewModel.Energy
→ ToText(Integer)
→ EnergyValueText.SetText
```

这里使用 `ViewModel.Energy` 是正确的，因为 Cancel 的正式语义就是：

```text
回到当前已完成历史 ViewModel
```

不要在 Cancel 用当前 Record 的：

```text
EnergyAfter
```

那会继续保留未来视觉。

---

## 7.35 为什么 Cancel 不需要缓存 `EnergyBefore`

理论上当前 Record：

```text
EnergyBefore = A
EnergyAfter  = B
```

看起来可以缓存 A 再恢复。

但本项目已经采用统一历史 reconciliation：

```text
ViewModel = 当前已经完成播放的历史事实
```

所以 Cancel 直接恢复：

```text
ViewModel.Energy
```

比新增：

```text
ActiveEnergyBefore
PreviousEnergy
```

更可靠。

这也与 StatusChanged Cancel 的策略一致。

---

## 7.36 Cancel 仍然不能 Notify

新增 Energy restore 后，再检查整个：

```text
Cancel Presentation Record Playback
```

仍然不能出现：

```text
NotifyPresentationFinished
NotifyPresentationRecordFinished
FinishPresentationRecord
```

正确 Cancel：

```text
Clear Timer
→ 必要的 Presentation visual restore
→ 清 transient refs / Type / Token
→ 不 Notify
```

不要把取消的 Energy Record 当正常完成。

---

## 7.37 本节完成后的 Cancel Sequence

状态与能量部分最终可抽象成：

```text
Cancel Presentation Record Playback
↓
Clear ActivePresentationTimer
↓
原 Card / Damage 等公共视觉清理
↓
Sequence

├ Then 0：StatusChanged restore
│   → ActiveType == StatusChanged ?
│   → ViewModel valid ?
│   → Rebuild Player Statuses
│   → Rebuild Enemy Statuses
│
├ Then 1：EnergyChanged restore
│   → ActiveType == EnergyChanged ?
│   → ViewModel valid ?
│   → EnergyValueText = ViewModel.Energy
│
└ Then 2：公共尾部
    → ActiveStatusPresentationWidget = None
    → 清其他 transient refs
    → ActivePresentationType = None
    → ActivePresentationToken = default
```

---

## 7.38 不要把 CardPlayed Cost 接到 EnergyChanged Event

本节完成后，搜索 Blueprint 中：

```text
PlayEnergyChangedPresentation
```

它只应该由：

```text
BeginPresentationRecordPlayback
→ Switch.EnergyChanged
```

调用。

不要从：

```text
PlayCardPresentation
CardPlayed Finish
Card click / ConfirmSelectedCard
```

直接调用它。

否则会重新制造 Cost 双重表现。

---

## 7.39 CardPlayed 本身的 Energy 显示边界

如果当前 CardPlayed PIE 中，正常 ViewModel/reducer 已经能让 Energy 从：

```text
5 → 4
```

正确显示，就保持现状。

本节不要为了“统一动画”额外把 CardPlayed 的 Cost 转换成 EnergyChanged。

如果后续发现 CardPlayed transient 期间 Energy 显示时机需要更精确，应在：

```text
CardPlayed playback
```

内部直接消费它自己的冻结：

```text
CardPlayed.EnergyAfter
```

进行表现，而不是制造第二条 EnergyChanged。

这属于 CardPlayed 自己的表现细化，不改变当前 Record 契约。

---

## 7.40 Terminal Energy nuance 保持不变

本节不要给 terminal 分支加入：

```text
Energy = 0
Energy = MaxEnergy
Energy = 某个默认值
```

Victory / Defeat / ResolutionFault 只应该表现 terminal 自己的冻结事实。

如果 terminal 之前确实有一条独立能量变化：

```text
EnergyChanged A → B
→ Terminal
```

则 EnergyChanged 正常播放并完成后，HUD 保持 B，然后再进入 Terminal。

如果 terminal 前没有真实 EnergyChanged：

```text
不要为了 terminal UI 人工制造一条 EnergyChanged
```

同样，致死卡牌支付的 Cost 已经属于 `CardPlayed`，不能为了 Victory 再补一条同值 EnergyChanged。

---

## 7.41 Compile 前静态检查

在 Compile 前检查：

```text
PlayEnergyChangedPresentation
├ EnergyChanged 输入类型正确
├ Token 输入类型正确
├ 先 Set ActivePresentationToken
├ 再 Set ActivePresentationType=EnergyChanged
├ 显示值直接来自 EnergyAfter
└ 最后 StartPresentationFinishTimer
```

Router：

```text
Switch.EnergyChanged
→ HasEnergyChange
→ DeltaConsistent
→ Branch
→ true 调 Event + Return true
→ false Return false
```

Finish：

```text
EnergyChanged
→ NotifyPresentationRecordFinished
```

Cancel：

```text
ActiveType == EnergyChanged
→ ViewModel valid
→ 正式 Energy 控件恢复 ViewModel.Energy
```

---

## 7.42 Compile + Save

本节只需要修改：

```text
WBP_BattleHUD
```

操作：

```text
1. Compile WBP_BattleHUD
2. 查看 Compiler Results
3. 确认 0 Errors
4. Save
```

如果 `Break Energy Changed Presentation Payload` 找不到：

```text
先确认项目 C++ 已经是包含 FEnergyChangedPresentationPayload BlueprintType 的当前版本
→ 重新编译 C++ / 重启 Editor（按当前开发环境方式）
→ 再创建 Break 节点
```

不要用三个普通 Integer 变量假装替代正式 struct。

---

## 7.43 静态结构验收清单

```text
[ ] 已定位并复用当前正式 Energy UI 控件
[ ] 没有新建第二套 EnergyPanel
[ ] PlayEnergyChangedPresentation 已创建
[ ] EnergyChanged 参数类型正确
[ ] Token 参数类型正确
[ ] ActivePresentationToken=Token
[ ] ActivePresentationType=EnergyChanged
[ ] Break EnergyChanged 可见 EnergyBefore/EnergyAfter/Delta
[ ] Active visual 直接使用 EnergyAfter
[ ] 没有 EnergyBefore+Delta 重算最终值
[ ] 没有修改 ViewModel.Energy
[ ] 没有调用 Gameplay Energy API
[ ] Event 最后 StartPresentationFinishTimer

[ ] Router EnergyBefore!=EnergyAfter
[ ] Router 检查 Delta==EnergyAfter-EnergyBefore
[ ] Router invalid/no-op → Return false
[ ] Router valid → PlayEnergyChangedPresentation
[ ] Router 使用 Function Entry.Token
[ ] Router 调用后 Return true

[ ] Finish 有 EnergyChanged case
[ ] Finish 不恢复 EnergyBefore
[ ] Finish 只进入统一 Notify

[ ] Cancel 在清 ActiveType 前判断 EnergyChanged
[ ] Cancel 使用历史 ViewModel.Energy 恢复正式控件
[ ] Cancel 不 Notify
[ ] 公共 Type/Token 清理仍在最后

[ ] CardPlayed 没有调用 PlayEnergyChangedPresentation
[ ] 没有为卡牌 Cost 增加第二套 EnergyChanged 表现
[ ] Terminal 没有强制覆盖 Energy
[ ] WBP_BattleHUD Compile 0 Errors
[ ] WBP_BattleHUD 已 Save
```

---

# 第七节 PIE 验收部分

## 7.44 PIE 验收必须使用真实 `EnergyChanged` producer

结构 Compile 通过后，进入 PIE。

首先需要找到项目当前确实会生成独立 `EnergyChanged` 的真实流程。

优先候选：

```text
TurnStart / 能量刷新
独立 GainEnergy
独立 LoseEnergy
```

以实际 Record 为准。

不要为了验收临时：

```text
手工 Make EnergyChanged Payload
直接调用 PlayEnergyChangedPresentation
```

因为那不能验证：

```text
producer
→ committed record
→ Router
→ Token
→ reducer
```

完整链。

---

## 7.45 先记录真实 EnergyChanged 三字段

捕获一条真实 Record 后，记录：

```text
EnergyBefore = A
EnergyAfter  = B
Delta        = D
```

必须确认：

```text
A != B
D == B - A
```

例如真实运行可能是：

```text
0 → 3
Delta = +3
```

但不要把该示例数值写成项目固定规则。

正式验证日志只能写实际观察值。

---

## 7.46 观察 Router 是否真正进入 Blueprint async

合法 Record 到来时预期：

```text
Switch.EnergyChanged
↓
HasEnergyChange=true
↓
DeltaConsistent=true
↓
PlayEnergyChangedPresentation
↓
Return true
```

如果仍 immediate fallback，按顺序检查：

```text
1. Switch.EnergyChanged 白线是否仍接旧 Return false
2. HasEnergyChange 是否为 true
3. DeltaConsistent 是否为 true
4. PlayEnergyChangedPresentation 是否实际执行
5. Event 是否 StartPresentationFinishTimer
6. 调用节点之后是否 Return true
```

---

## 7.47 正常 Energy 变化的视觉序列必须是 `A → B → B`

假设真实 Record：

```text
A → B
```

播放开始后：

```text
HUD EnergyValueText = B
```

在 0.5 秒 async 窗口中：

```text
HUD transient = B
历史 ViewModel = A
```

约 0.5 秒后：

```text
Notify exact token
→ reducer ViewModel Energy = B
→ normal HUD refresh
```

最终：

```text
HUD = B
```

完整序列：

```text
A → B → B
```

---

## 7.48 禁止出现 `A → B → A → B` 闪回

如果看到：

```text
A
→ B
→ A
→ B
```

按顺序排查：

```text
1. Finish.EnergyChanged 是否错误恢复 EnergyBefore
2. Finish 是否在 Notify 前按旧 ViewModel 重刷 Energy
3. 是否错误调用整个 Event Battle HUD View Model Changed
4. Notify / reducer 顺序是否被改坏
```

正常 Finish 对 Energy 不做视觉恢复。

---

## 7.49 验收 exact-token completion

Energy 数字变成 B 还不够。

必须继续观察：

```text
StartPresentationFinishTimer
→ FinishPresentationRecord
→ EnergyChanged case
→ NotifyPresentationRecordFinished
→ NotifyPresentationFinished(ActivePresentationToken)
```

并确认：

```text
后续 Record 能继续播放
Presentation 不挂 Resolving
最终能够进入正常交互 / 下一阶段
```

如果 B 已显示但流程卡住，重点查 Timer 和 Token。

---

## 7.50 CardPlayed Cost 去重回归必须单独做

选择一张真实有 Cost 的卡，例如项目中的普通 1 费卡。

记录打出前后的 Energy：

```text
A → B
CostPaid = A - B
```

检查 committed Presentation Records。

正确结果：

```text
CardPlayed
- EnergyBefore = A
- EnergyAfter  = B
- CostPaid     = A-B
```

并且**不能再存在一条仅用于同一次卡费的：**

```text
EnergyChanged
EnergyBefore = A
EnergyAfter  = B
Delta        = B-A
```

视觉也只能发生一次 Cost 对应的能量变化。

---

## 7.51 如果发现 Card Cost 重复 EnergyChanged，判定 producer 失败

如果观察到：

```text
CardPlayed 5 → 4 CostPaid=1
紧接着
EnergyChanged 5 → 4 Delta=-1
```

本节不要通过 Blueprint 做：

```text
if LastRecord == CardPlayed → Ignore
```

也不要让 EnergyChanged Router `Return false` 作为长期“修复”。

这属于：

```text
同一个可见事实被 committed 两次
```

需要回 C++ producer 修复并重新跑相关测试。

只有 producer 去重以后，EnergyChanged 才能标 VALIDATED。

---

## 7.52 验收 no-op 不产生 Energy Record

在项目真实可构造的情况下，观察一次不会改变 Energy 的操作。

预期：

```text
EnergyBefore == EnergyAfter
→ producer 不产生 EnergyChanged
```

如果异常 producer 仍生成：

```text
EnergyChanged A → A Delta=0
```

本节 Router 应：

```text
HasEnergyChange=false
→ Return false
```

但正式修复目标仍是 producer 不生成 no-op Record。

不要把 Router fallback 当成 producer no-op 合法化。

---

## 7.53 Energy Cancel 的 PIE 抽查

如果项目当前有正式、可控的 Cancel 入口，例如：

```text
SkipPresentation()
合法 generation replacement
Widget ownership cancellation
```

则额外测试：

```text
历史 Energy = A
↓
EnergyChanged transient = B
↓
0.5 秒内触发正式 Cancel
```

预期：

```text
Timer 清除
→ ActiveType == EnergyChanged
→ ViewModel valid
→ EnergyValueText 恢复 ViewModel.Energy = A
→ 不 Notify 被取消 Record
```

最终：

```text
HUD = A
历史 ViewModel = A
```

如果没有正式可控 Cancel 入口，不要临时在 Blueprint 中硬调 Cancel；把 runtime Cancel 证明留到统一 Cancel/Reconcile 章节。

---

## 7.54 Terminal Energy nuance 的 PIE 抽查

如果可以稳定构造：

```text
某个真实 EnergyChanged
→ 后续 Victory / Defeat
```

继续观察 terminal 前后的 Energy。

要求：

```text
EnergyChanged 完成后 = Record.EnergyAfter
→ Terminal 出现时不被强制改成 0 / Max / 默认值
```

如果 lethal card 本身支付 Cost：

```text
Cost 仍属于 CardPlayed
```

不能因为后面紧跟 Victory 又生成一份相同 EnergyChanged。

---

## 7.55 EnergyChanged 正式 PIE 验收清单

只有以下主线条件全部满足后，才允许标 `EnergyChanged = VALIDATED`：

```text
[ ] 使用真实 producer 得到独立 EnergyChanged Record
[ ] EnergyBefore=A
[ ] EnergyAfter=B
[ ] A!=B
[ ] Delta=B-A

[ ] Router 接管合法 Record
[ ] Router 对 no-op / inconsistent payload 不启动假 async
[ ] PlayEnergyChangedPresentation 使用当前 Record Token
[ ] ActivePresentationType=EnergyChanged
[ ] HUD 最终值直接使用 EnergyAfter
[ ] 没有 EnergyBefore+Delta 重算最终值
[ ] 没有修改 ViewModel / Gameplay
[ ] 0.5s async ownership 正常
[ ] exact-token completion 正常
[ ] 最终 ViewModel Energy=B
[ ] 最终 HUD Energy=B
[ ] 没有 A→B→A→B 闪回
[ ] 后续 Records 正常继续
[ ] 最终不挂 Resolving

[ ] CardPlayed Cost 仍只由 CardPlayed 记录
[ ] 没有同一次 Cost 的重复 EnergyChanged
[ ] no-op 不形成正式可见 EnergyChanged
[ ] Terminal 不强制重写 Energy

[ ] 若运行了 Cancel 抽查：Cancel 恢复历史 ViewModel Energy
[ ] Cancel 不 Notify
```

---

## 7.56 正式验证日志模板

本节实际通过后，把真实观察结果写入：

```text
docs/UIA2EBlueprintValidationLog.md
```

建议记录：

```text
EnergyChanged PIE
SourceFlow             = <实际触发流程>
EnergyBefore           = <A>
EnergyAfter            = <B>
Delta                  = <D>
DeltaConsistent        = Yes
FrozenEnergyAfterShown = Yes
AsyncWindowObserved    = Yes
FlashbackObserved      = No
ExactTokenCompleted    = Yes
FinalViewModelEnergy   = <B>
FinalHUDEnergy         = <B>
ReturnedToIdle         = Yes
Result                 = PASS

Card Cost Duplication Regression
Card                   = <实际卡牌>
CardPlayedEnergyBefore = <A>
CardPlayedEnergyAfter  = <B>
CostPaid               = <实际值>
DuplicateEnergyChanged = No
Result                 = PASS
```

如果某个调试字段没有实际观察到，写：

```text
Not Observed
```

不要编造。

---

## 7.57 本节完成后的 A2E 状态

只有 owner 实际确认本节 PIE 通过后，状态才更新为：

```text
CardPlayed              VALIDATED
Damage                  VALIDATED
BlockChanged            VALIDATED
CardZoneChanged          VALIDATED（目前至少 PlayArea → Destination 已验证）
StatusChanged creation  VALIDATED
StatusChanged update/reduction  VALIDATED
StatusChanged removal   VALIDATED
EnergyChanged           VALIDATED
```

整个 UI-A2E 此时仍然：

```text
PARTIAL
```

不能提前 COMPLETE / SEALED。

---

## 7.58 本节完成判定

如果只接完 Blueprint，但没有真实 PIE：

```text
EnergyChanged = WIRED / NOT PIE VALIDATED
```

如果 PIE 通过但 Card Cost 出现重复 Record：

```text
EnergyChanged = NOT VALIDATED
→ 先修 producer duplication
```

只有：

```text
结构正确
+ Compile 0 Errors
+ real EnergyChanged PIE 通过
+ Card Cost duplication regression 通过
```

才能：

```text
EnergyChanged = VALIDATED
```

---

## 7.59 下一节

下一节继续处理：

```text
补齐剩余 CardZoneChanged Blueprint Playback
```

当前已验证的是：

```text
PlayArea → Destination
```

下一节将先以实际 producer 的 Zone 变化集合为准，逐类检查哪些 `CardZoneChanged` 仍在 fallback，再决定最小可视化与 Router：

```text
DrawPile / DiscardPile / Hand / Exhaust / PlayArea
```

重点继续保持：

```text
冻结 Card snapshot
精确 RuntimeId
不查询 mutable Gameplay card
不重复 DeckShuffled 的事实
正确 Timer / Token / Cancel
无错误的手牌/牌堆闪回
```

---

# 第八节：补齐剩余 `CardZoneChanged` Blueprint Playback

## 8.1 本节目标与进入条件

只有第七节的 `EnergyChanged` Blueprint 结构与真实 PIE 验收已经通过后，才进入本节实际施工。

本节目标不是重新实现已经验证过的：

```text
PlayArea → Destination
```

而是先以当前 C++ producer 与 Controller reducer 的真实支持集合为准，补齐仍在 fallback 的 `CardZoneChanged`。

当前仓库代码审计后，正式 producer 会提交的 `CardZoneChanged` 家族是：

```text
DrawPile → Hand
Hand → DiscardPile
PlayArea → DiscardPile
PlayArea → ExhaustPile
PlayArea → RemovedPile
```

其中：

```text
PlayArea → Destination
```

已经有 Blueprint async playback，并且当前验证基线中该切片已经 `VALIDATED`。

所以本节真正新增的只有：

```text
A. DrawPile → Hand
B. Hand → DiscardPile
```

本节结束后，只有在真实 PIE 全部通过时，才允许把“当前 producer 集合内的 `CardZoneChanged`”记为完整验证。

---

## 8.2 为什么先做 producer 审计，而不是对所有 `ECardZone` 两两组合做 Router

当前 `ECardZone` 枚举包含：

```text
DrawPile
Hand
PlayArea
DiscardPile
ExhaustPile
RemovedPile
```

但这不意味着所有：

```text
FromZone → ToZone
```

都属于当前正式 Presentation Record。

当前 `DeckRuntime` 的 commit API 与 Action producer 已经把实际语义限制得很清楚。

### Draw

`DrawCardAction`：

```text
TryDrawTopCardCommit
→ DrawPile → Hand
→ 生成 CardZoneChanged
```

### EndTurn / 显式弃牌

`DiscardCardAction`：

```text
TryDiscardCardCommit
→ Hand → DiscardPile
→ 生成 CardZoneChanged
```

### 卡牌结算完成

`FinishCardPlayAction`：

```text
PlayArea → DiscardPile
PlayArea → ExhaustPile
PlayArea → RemovedPile
→ 生成 CardZoneChanged
```

因此本节 Router 只支持当前 producer 确实会提交的组合。

不要为了“看起来完整”提前添加：

```text
Hand → ExhaustPile
Hand → RemovedPile
DiscardPile → Hand
DiscardPile → DrawPile
DrawPile → DiscardPile
PlayArea → DrawPile
```

等当前 producer 不存在的路径。

未知组合继续：

```text
Return false
```

由 C++ fallback / reconciliation 处理。

---

## 8.3 `Hand → PlayArea` 明确不属于 `CardZoneChanged`

这一点必须单独锁定。

当前 `PlayCardAction` 在 Gameplay 中确实执行：

```text
Hand → PlayArea
```

但 committed-presentation 层没有再为这一步生成 `CardZoneChanged`。

它已经由：

```text
CardPlayed
```

完整承担可见事实，包括：

```text
Card snapshot
HandIndexBefore
PlayAreaIndexAfter
EnergyBefore
EnergyAfter
CostPaid
```

所以本节绝对不要新增：

```text
Hand → PlayArea
→ CardZoneChanged Blueprint Playback
```

也不要在 PIE 中期待同一次打牌出现：

```text
CardPlayed
+ Hand→PlayArea CardZoneChanged
```

如果以后实际 committed records 出现这种重复，应先判断为 producer contract 回归，而不是让 Blueprint 两边都播。

---

## 8.4 `DiscardPile → DrawPile` 属于 `DeckShuffled`，不是 `CardZoneChanged`

当前 `ShuffleDeckAction` 在成功回洗时提交：

```text
DeckShuffled
```

Payload 冻结：

```text
MovedCardCount
DrawCountBefore
DrawCountAfter
DiscardCountBefore
DiscardCountAfter
```

因此：

```text
DiscardPile → DrawPile
```

的整体事实属于下一节的：

```text
DeckShuffled Blueprint Playback
```

本节的 Draw 路径只处理：

```text
DrawPile → Hand
```

不要在 `CardZoneChanged` Event 里：

```text
把 DiscardCount 清零
把 DrawCount 加上整堆弃牌
显示 Shuffle cue
```

否则会与 `DeckShuffled` 重复。

---

## 8.5 `PlayArea → Hand` rollback 不是正常 CardZoneChanged presentation

`PlayCardAction` 在极端的 energy commit 失败路径中可能执行 Gameplay rollback：

```text
PlayArea → Hand
```

这是为了恢复 Gameplay 原子性。

当前该 rollback 不作为正常可见 `CardZoneChanged` Record 提交。

因此 Router 中看到：

```text
FromZone = PlayArea
ToZone   = Hand
```

时，不要把它塞进已经验证的 PlayArea retirement 路径。

正式行为：

```text
Return false
```

当前正常 PlayArea destination 仍只接受：

```text
DiscardPile
ExhaustPile
RemovedPile
```

---

## 8.6 先确认 `FCardZoneChangedPresentationPayload` 的冻结字段

当前 Payload 是：

```text
Card      : FPresentationCardSnapshot
FromZone  : ECardZone
ToZone    : ECardZone
FromIndex : int32
ToIndex   : int32
```

其中 `Card` 已冻结：

```text
RuntimeId
CardId
DisplayName
Cost
CardType
TargetType
Description
CardArt
```

本节所有新增 card visual 都必须从这份冻结 snapshot 读取。

禁止：

```text
通过 RuntimeId 回 Gameplay DeckRuntime 查 UCardInstance
从当前 live Hand 查询 CardData
重新读取 mutable CurrentCost
从 Gameplay 卡牌对象重新生成描述文本
```

---

## 8.7 本节不在 active window 手算 pile count

注意当前 `FCardZoneChangedPresentationPayload` **没有冻结**：

```text
DrawCountBefore / After
DiscardCountBefore / After
ExhaustCountBefore / After
```

因此本节第一版不要为了让数字立即跳变，自己做：

```text
ViewModel.DrawCount - 1
ViewModel.DiscardCount + 1
ViewModel.ExhaustCount + 1
```

然后覆盖正式 pile 数字。

这会让 Blueprint 再维护一套 pile reducer。

本节锁定更小、更安全的表现策略：

```text
CardZoneChanged active window
→ 只表现“哪张卡从手牌消失 / 哪张卡进入手牌”
→ pile 数字保持当前历史 ViewModel 值

Notify exact token
→ Controller reducer 正式推进 WorkingSnapshot
→ ViewModel refresh
→ pile 数字一次更新到新的正式值
```

这样 Cancel 也不需要额外保存或恢复 pile 数字。

---

## 8.8 当前 Controller reducer 的正式 `CardZoneChanged` 语义

当前 `ApplyRecordToWorkingSnapshot` 对 `CardZoneChanged` 的支持也是明确的。

### DrawPile → Hand

```text
要求：
DrawCount > 0
Hand 中不存在同 RuntimeId
ToIndex 合法

应用：
DrawCount -= 1
HandCards.Insert(Card, ToIndex)
```

### Hand → DiscardPile

```text
要求：
Hand 中找到相同 RuntimeId
实际 HandIndex == Record.FromIndex
CardId 匹配

应用：
HandCards.RemoveAt(HandIndex)
DiscardCount += 1
```

### PlayArea → Destination

```text
DiscardPile → DiscardCount += 1
ExhaustPile → ExhaustCount += 1
RemovedPile → 不增加 HUD pile count
```

本节 Blueprint 应尽量在开始 async 前做与这些历史事实一致的可视前置检查，而不是“先播，等 reducer 再发现不对”。

---

## 8.9 保留现有 `PlayArea → Destination` 已验证路径

当前 `BeginPresentationRecordPlayback → CardZoneChanged` 已经有：

```text
FromZone == PlayArea
→ IsValid(PlayedCardWidget)
→ PlayedCardWidget.CardView.RuntimeId == Payload.Card.RuntimeId
→ PlayCardZoneChangePresentation
→ Return true
```

本节不要删除这套路径，也不要把它改造成 Draw / Hand 通用事件。

原因：

```text
PlayArea retirement
```

依赖：

```text
PlayedCardWidget
```

而 Draw / Hand discard 的视觉对象完全不同。

本节采用“保留旧 Event + 新增两个专用 Event”的方式，降低对已验证路径的回归风险。

---

## 8.10 建议给现有 PlayArea Router 补一个 `ToZone` 白名单

为了与当前 producer / reducer 完全一致，在原：

```text
FromZone == PlayArea
```

判断后，可以增加：

```text
ToZone == DiscardPile
OR ToZone == ExhaustPile
OR ToZone == RemovedPile
```

得到：

```text
IsSupportedPlayAreaDestination
```

最终 PlayArea path 条件变为：

```text
FromZone == PlayArea
AND IsSupportedPlayAreaDestination
```

如果你希望本轮最小化改动，也可以保留现有已验证路径不动，只在外围新 Router 中确保只有这三个 ToZone 会进入旧路径。

本节推荐后者：

```text
外围分类先验证 zone pair
→ 再进入原 PlayArea widget 校验
```

这样原核心节点不需要重搭。

---

# 第八节 A：新增手牌 Widget 精确查找 helper

## 8.11 为什么需要 `FindHandCardWidgetByRuntimeId`

Draw 路径需要确认：

```text
当前正式 Hand 中还不存在该 RuntimeId
```

Hand → Discard 路径需要确认：

```text
找到的确实是 Record 指定的 RuntimeId
而且它当前所在 index == Record.FromIndex
```

如果每个 Router 分支都单独复制 `ForLoop + Cast + RuntimeId`，图会很快变乱。

因此本节先新建一个纯视觉查找 helper：

```text
FindHandCardWidgetByRuntimeId
```

这个函数只遍历当前 `HB_Hand` 的 `WBP_BattleCard` 子控件，不访问 Gameplay Deck。

---

## 8.12 创建函数签名

在：

```text
WBP_BattleHUD
→ My Blueprint
→ Functions
```

新建：

```text
FindHandCardWidgetByRuntimeId
```

输入：

```text
RuntimeId : Integer
```

输出：

```text
Found      : Boolean
CardWidget : WBP_BattleCard Object Reference
ChildIndex : Integer
```

不要把 RuntimeId 做成 Name 或 String。

当前 Card RuntimeId 的正式类型是 `int32`。

---

## 8.13 添加函数 Local Variables

建议使用：

```text
SearchFound      : Boolean
FoundCardWidget  : WBP_BattleCard Object Reference
FoundChildIndex  : Integer
```

初始化：

```text
SearchFound     = false
FoundCardWidget = None
FoundChildIndex = -1
```

这些是 Function Local，不是 Blueprint member。

不要为了查一次手牌创建：

```text
CurrentHandSearchRuntimeId
PendingHandWidget
```

之类持久变量。

---

## 8.14 先取 `HB_Hand.GetChildrenCount`

函数执行开始后：

```text
Get HB_Hand
→ Get Children Count
```

然后：

```text
ChildrenCount > 0
→ Branch
```

### False

使用一个独立 Return Node：

```text
Found      = false
CardWidget = None
ChildIndex = -1
```

### True

进入：

```text
For Loop With Break
```

不要尝试把 False 与 Loop Completed 的两条执行线硬接到同一个 Return exec pin。

Blueprint Function 可以放多个 Return Node。

---

## 8.15 `For Loop With Break` 范围

连接：

```text
First Index = 0
Last Index  = ChildrenCount - 1
```

Loop Body：

```text
HB_Hand.GetChildAt(Index)
→ Cast To WBP_BattleCard
```

`Cast Failed` 不做任何事，当前循环继续下一项。

正常 HUD 中应全部是 `WBP_BattleCard`，但 helper 不需要因此假设所有 Child 都一定可 Cast。

---

## 8.16 Cast 成功后比较 `CardView.RuntimeId`

从：

```text
As WBP_BattleCard
```

读取：

```text
CardView
```

再：

```text
Break Battle HUD Card View
→ RuntimeId
```

比较：

```text
Child.CardView.RuntimeId == Input.RuntimeId
```

得到：

```text
SameRuntimeId
```

接：

```text
Branch(SameRuntimeId)
```

---

## 8.17 找到后保存结果并 Break

`Branch.True`：

```text
Set FoundCardWidget = As WBP_BattleCard
↓
Set FoundChildIndex = Loop.Index
↓
Set SearchFound = true
↓
ForLoopWithBreak.Break
```

不要继续搜索并覆盖第一次精确匹配。

RuntimeId 理论上应唯一；如果视觉层已经有重复 RuntimeId，后续 reducer 也会认为状态存在问题。

---

## 8.18 Loop Completed 返回 Local 结果

`ForLoopWithBreak.Completed` 接第二个 Return Node：

```text
Found      ← SearchFound
CardWidget ← FoundCardWidget
ChildIndex ← FoundChildIndex
```

最终 helper 语义：

```text
HB_Hand 中存在指定 RuntimeId
→ Found=true
→ 返回 exact WBP_BattleCard + 当前 child index

不存在
→ Found=false
→ None / -1
```

---

## 8.19 Helper 静态验收

编译前检查：

```text
[ ] 只遍历 HB_Hand
[ ] 不读取 ViewModel.HandCards 来创建新真值
[ ] 不访问 DeckRuntime
[ ] 不访问 UCardInstance
[ ] 比较 CardView.RuntimeId
[ ] 找到后记录 ChildIndex
[ ] 使用 ForLoopWithBreak
[ ] 空 Hand 返回 false/None/-1
[ ] RuntimeId 找不到返回 false/None/-1
```

保存后暂时不要改其他 helper。

---

# 第八节 B：实现 `Hand → DiscardPile`

## 8.20 新建 `PlayHandDiscardPresentation`

在 `WBP_BattleHUD → Event Graph` 新建 Custom Event：

```text
PlayHandDiscardPresentation
```

输入：

```text
CardZoneChanged : FCardZoneChangedPresentationPayload
Token           : FPresentationPlaybackToken
HandCardWidget  : WBP_BattleCard Object Reference
```

这个 Event 只负责已经被 Router 精确验证过的：

```text
Hand → DiscardPile
```

不要让它处理 Draw 或 PlayArea。

---

## 8.21 Event 开头保存 Token / Type

执行链：

```text
PlayHandDiscardPresentation
↓
Set ActivePresentationToken = Token
↓
Set ActivePresentationType = CardZoneChanged
```

顺序与当前其他异步 Event 保持一致。

不要在隐藏卡牌后才设置 Token。

---

## 8.22 复用现有 `HiddenHandCardWidget`

当前 HUD 已有：

```text
HiddenHandCardWidget : WBP_BattleCard reference
```

CardPlayed 已经用它保存“当前被临时隐藏的正式 Hand 卡牌”。

Hand → DiscardPile 的需求完全相同：

```text
当前 Record active 时
→ 临时隐藏一个已经存在的正式 Hand Widget
```

A2E 一次只播放一个 Record，所以本节可以直接复用该变量。

连接：

```text
Event.HandCardWidget
→ Set HiddenHandCardWidget
```

不需要新增：

```text
ZoneDiscardHiddenWidget
```

---

## 8.23 把 exact Hand Widget 设为 `Collapsed`

从：

```text
HandCardWidget
```

调用：

```text
Set Visibility
```

设置：

```text
Collapsed
```

推荐 `Collapsed` 而不是 `Hidden`，因为本条 Record 的最小可见语义就是：

```text
这张卡离开 Hand
```

`Collapsed` 会让 HorizontalBox 立即收缩。

Cancel 时现有 `HiddenHandCardWidget` 恢复逻辑再把它设回 `Visible`。

---

## 8.24 Hand discard active window 不改 DiscardCount

Event 中不要做：

```text
DiscardCount + 1
→ SetText
```

本条 0.5 秒 active window 只表现：

```text
exact hand card 消失
```

历史 ViewModel 与 pile 数字保持旧值。

Notify 后 C++ reducer 才会：

```text
HandCards.RemoveAt(exact index)
DiscardCount += 1
```

然后正常 HUD refresh 更新正式数字。

---

## 8.25 最后启动公共 Timer

完整 Event：

```text
PlayHandDiscardPresentation(
  CardZoneChanged,
  Token,
  HandCardWidget
)
↓
ActivePresentationToken = Token
↓
ActivePresentationType = CardZoneChanged
↓
HiddenHandCardWidget = HandCardWidget
↓
HandCardWidget.SetVisibility(Collapsed)
↓
StartPresentationFinishTimer
```

继续使用：

```text
0.5 s
Looping = false
FinishPresentationRecord
```

不要新增 HandDiscard Timer。

---

## 8.26 Router：识别 `Hand → DiscardPile`

回到：

```text
BeginPresentationRecordPlayback
→ Switch.CardZoneChanged
```

从当前 `Break Card Zone Changed Presentation Payload` 取得：

```text
FromZone
ToZone
Card
FromIndex
ToIndex
```

创建：

```text
FromZone == Hand
```

和：

```text
ToZone == DiscardPile
```

做：

```text
AND
```

得到：

```text
IsHandToDiscard
```

---

## 8.27 `Hand → DiscardPile` 先用 RuntimeId 找 exact Widget

从 Payload.Card：

```text
Break Presentation Card Snapshot
→ RuntimeId
```

调用：

```text
FindHandCardWidgetByRuntimeId(RuntimeId)
```

预期必须：

```text
Found = true
```

否则：

```text
Return false
```

不要找不到就：

```text
GetChildAt(FromIndex) 然后不管 RuntimeId
```

也不要新建一张“要丢弃的卡”来伪装历史 Hand。

---

## 8.28 再验证 helper 返回的 `ChildIndex == FromIndex`

Controller reducer 对 Hand discard 的正式条件包含：

```text
当前 hand index == Record.FromIndex
```

所以 Blueprint Router 也检查：

```text
Find.ChildIndex == Payload.FromIndex
```

如果不相等：

```text
Return false
```

不要为了让动画继续而忽略 FromIndex。

这对 EndTurn 多张手牌连续弃置尤其重要。

---

## 8.29 再验证 `CardId`

从：

```text
Found.CardWidget.CardView
→ CardId
```

与：

```text
Payload.Card.CardId
```

做精确 Name 相等比较。

得到：

```text
SameCardId
```

最终 Hand discard 接管条件至少是：

```text
IsHandToDiscard
AND Found
AND ChildIndex == FromIndex
AND RuntimeId 匹配（helper 已保证）
AND CardId 匹配
```

如果当前项目习惯用 `Name → String → EqualExactly`，保持一致也可以；不要做模糊文本比较。

---

## 8.30 `Found + Index + CardId` 全部通过后才调用 Event

调用：

```text
PlayHandDiscardPresentation
```

参数：

```text
CardZoneChanged
← Record.CardZoneChanged

Token
← Function Entry.Token

HandCardWidget
← FindHandCardWidgetByRuntimeId.CardWidget
```

之后：

```text
Return true
```

任一校验失败：

```text
Return false
```

不要先隐藏 Widget 再做最后一个校验。

---

# 第八节 C：实现 `DrawPile → Hand`

## 8.31 新增专用成员 `ZoneChangedDrawnCardWidget`

DrawPile → Hand 与 Hand discard 不同：

```text
历史 Hand 中还没有这张卡
```

所以 active window 需要根据冻结 Card snapshot 临时创建一张视觉卡。

在 `WBP_BattleHUD` 新建成员：

```text
ZoneChangedDrawnCardWidget
: WBP_BattleCard Object Reference
```

默认：

```text
None
```

这个引用只负责：

```text
当前 DrawPile → Hand Record 创建的 transient WBP_BattleCard
```

不要复用：

```text
PlayedCardWidget
```

因为 `PlayedCardWidget` 的生命周期属于 PlayArea presentation。

---

## 8.32 新建 `PlayDrawToHandPresentation`

在 Event Graph 新建：

```text
PlayDrawToHandPresentation
```

输入：

```text
CardZoneChanged : FCardZoneChangedPresentationPayload
Token           : FPresentationPlaybackToken
```

本 Event 只由已经验证过的：

```text
DrawPile → Hand
```

Router 调用。

---

## 8.33 Event 开头保存 Token / Type

执行链：

```text
PlayDrawToHandPresentation
↓
ActivePresentationToken = Token
↓
ActivePresentationType = CardZoneChanged
```

与所有当前 async playback 保持一致。

---

## 8.34 从 Payload 取得冻结 Card snapshot

从：

```text
CardZoneChanged
```

使用一个：

```text
Break Card Zone Changed Presentation Payload
```

取得：

```text
Card
```

然后把完整 frozen snapshot 送入已有 C++ helper：

```text
Make Presentation Card View
```

不要拆出 DisplayName / Cost 后自己重新 Make HUD Card View。

---

## 8.35 `Make Presentation Card View` 的重要语义

该 helper 会把冻结：

```text
RuntimeId
CardId
DisplayName
Cost
CardType
TargetType
Description
CardArt
```

复制到 `FBattleHUDCardView`。

同时强制：

```text
bGameplayPlayable = false
UnplayableReason = empty
```

所以 Draw active window 里的这张卡只是 Presentation copy，不是 live gameplay hand card。

这正是本节需要的行为。

---

## 8.36 按当前 CardPlayed 已验证方式 `Create Widget WBP_BattleCard`

复用当前项目已有创建方式：

```text
Create Widget
Class = WBP_BattleCard
Owning Player = Get Owning Player
```

如果当前 Create Widget 节点已经暴露：

```text
CardView
OwnerHUD
```

spawn pins，则按当前 CardPlayed 路径相同方式连接：

```text
CardView ← Make Presentation Card View.ReturnValue
OwnerHUD ← self
```

不要为 Draw 再设计第二套 WBP 卡牌结构。

---

## 8.37 保存 transient Draw Widget

`Create Widget` 成功后：

```text
Set ZoneChangedDrawnCardWidget = CreatedWidget
```

这个引用用于 Cancel 清理和 Finish 后清引用。

---

## 8.38 transient Draw 卡禁止输入

对新创建的：

```text
ZoneChangedDrawnCardWidget
```

设置：

```text
Visibility = Hit Test Invisible
```

原因：

```text
Record active window 中
ViewModel 尚未正式包含这张 Hand 卡
```

即使 CardView 已经 `bGameplayPlayable=false`，本节仍建议关闭命中，避免 Hover / click 等交互路径提前把它当正式 Hand 项。

Notify 后正式 ViewModel rebuild 会创建正常可交互的新 Hand Widget。

---

## 8.39 当前 producer 是“追加到 Hand 末尾”，所以第一版只支持 append

当前 `DeckRuntime::TryDrawTopCardCommit` 的 `ToIndex` 是：

```text
ToIndex = Hand.Num()
→ Hand.Add(Card)
```

因此当前 producer 的 Draw Record 一定是：

```text
ToIndex == 当前历史 Hand 数量
```

为了不依赖 `Insert Child At` 的复杂布局行为，本节第一版锁定：

```text
Router 要求 Payload.ToIndex == HB_Hand.GetChildrenCount()
```

通过后 Event 使用：

```text
HB_Hand.AddChildToHorizontalBox(ZoneChangedDrawnCardWidget)
```

即可得到正确顺序。

如果未来 producer 改为任意 index 插入：

```text
ToIndex != 当前 Hand Count
```

当前 Blueprint 应：

```text
Return false
```

而不是把卡错误追加到末尾。

以后再针对新的 producer 契约扩展 Insert 语义。

---

## 8.40 Draw Event 添加到 `HB_Hand`

执行链：

```text
Create WBP_BattleCard
↓
Set ZoneChangedDrawnCardWidget
↓
Set Visibility(Hit Test Invisible)
↓
HB_Hand.AddChildToHorizontalBox(ZoneChangedDrawnCardWidget)
↓
StartPresentationFinishTimer
```

不要在 Event 中：

```text
HB_Hand.ClearChildren
```

也不要从 `ViewModel.HandCards` 全量重建 Hand。

当前 Record 只增加这一张 transient card。

---

## 8.41 Draw active window 不改 DrawCount

本 Event 不更新：

```text
Draw pile 数字
Discard pile 数字
Exhaust pile 数字
```

0.5 秒窗口中允许：

```text
视觉 Hand 已出现新卡
历史 DrawCount 仍是旧值
```

因为这正是“Record 正在播放、reducer 尚未推进”的 Presentation 窗口。

Notify 后 Controller 会：

```text
DrawCount -= 1
HandCards.Insert(...)
```

随后正式 HUD refresh 一次性把 pile 数字与 Hand truth 推进。

---

## 8.42 Router：识别 `DrawPile → Hand`

在 `Switch.CardZoneChanged`：

```text
FromZone == DrawPile
AND
ToZone == Hand
```

得到：

```text
IsDrawToHand
```

只有该组合才进入本节 Draw 路径。

---

## 8.43 Draw Router 先验证 frozen Card 基础身份

从 Payload.Card：

```text
RuntimeId
CardId
```

最低验证：

```text
RuntimeId != -1
CardId != None
```

这里的 `-1` 对应 `INDEX_NONE`。

如果任一失败：

```text
Return false
```

不要创建一个缺失正式 identity 的 transient card。

---

## 8.44 Draw Router 检查该 RuntimeId 当前 Hand 中不存在

调用：

```text
FindHandCardWidgetByRuntimeId(Payload.Card.RuntimeId)
```

Draw 的正确前置状态应该是：

```text
Found = false
```

如果：

```text
Found = true
```

说明当前 Hand visual 已经有相同 RuntimeId。

此时：

```text
Return false
```

不要再创建第二张相同 RuntimeId 卡牌。

---

## 8.45 Draw Router 检查 `ViewModel.DrawCount > 0`

当前 Controller reducer 对 Draw 的第一项历史条件是：

```text
WorkingPresentationSnapshot.DrawCount > 0
```

Blueprint 这里可以读取当前历史：

```text
ViewModel.DrawCount
```

做：

```text
ViewModel.DrawCount > 0
```

这是安全的，因为它只是验证“上一条已完成历史是否允许这条 Draw Record 开始播放”。

不要通过 ViewModel 去查询要抽到哪张卡；卡牌身份仍完全来自 frozen Record.Card。

---

## 8.46 Draw Router 检查 append-only `ToIndex`

取：

```text
HB_Hand.GetChildrenCount
```

比较：

```text
Payload.ToIndex == ChildrenCount
```

只有 true 才接管。

最终 Draw 接管条件至少是：

```text
IsDrawToHand
AND RuntimeId valid
AND CardId valid
AND RuntimeId not already in Hand
AND ViewModel.DrawCount > 0
AND ToIndex == HB_Hand.ChildrenCount
```

否则：

```text
Return false
```

---

## 8.47 Draw Router True 调 Event 并 `Return true`

调用：

```text
PlayDrawToHandPresentation
```

参数：

```text
CardZoneChanged
← Record.CardZoneChanged

Token
← Function Entry.Token
```

然后：

```text
Return true
```

只有 Event 实际创建了 transient card 并启动 Timer 时才允许 true。

---

# 第八节 D：重构 `CardZoneChanged` Router 总树

## 8.48 推荐的最终执行分类顺序

为了最大程度保留现有 PlayArea 路径，推荐从：

```text
Switch.CardZoneChanged
```

开始按以下顺序：

```text
先验证 Payload.Card 基础 identity
↓
FromZone == PlayArea ?
├ true
│  → ToZone 是否 Discard / Exhaust / Removed
│  → existing PlayedCardWidget validation
│  → existing PlayCardZoneChangePresentation
│  → Return true
│
└ false
   ↓
   IsHandToDiscard ?
   ├ true
   │  → FindHandCardWidgetByRuntimeId
   │  → exact index + CardId validation
   │  → PlayHandDiscardPresentation
   │  → Return true
   │
   └ false
      ↓
      IsDrawToHand ?
      ├ true
      │  → no duplicate RuntimeId
      │  → ViewModel.DrawCount > 0
      │  → ToIndex == HB_Hand.ChildrenCount
      │  → PlayDrawToHandPresentation
      │  → Return true
      │
      └ false
         → Return false
```

---

## 8.49 顶层 `CardPayloadValid`

推荐在三类 zone pair 分流之前先做：

```text
RuntimeId != -1
AND CardId != None
```

得到：

```text
CardPayloadValid
```

`false`：

```text
Return false
```

`true`：

```text
进入 zone pair 分类
```

这样不会在三条路径里重复相同基础验证。

---

## 8.50 不要用一个巨大 `OR` 把所有 CardZoneChanged 直接接管

错误示例：

```text
FromZone != ToZone
→ PlayCardZoneChangePresentation
→ Return true
```

这会错误接管：

```text
Hand → PlayArea
DiscardPile → DrawPile
PlayArea → Hand rollback
以及未来未实现组合
```

正确做法是明确列出当前 producer family。

Router 是历史 Record 的“能力声明”，不是“只要是 CardZoneChanged 都能播”。

---

## 8.51 `Return false` 是正式 fail-safe，不是失败动画

对于：

```text
未知 zone pair
Card identity 无效
Hand exact Widget 找不到
FromIndex 不匹配
CardId 不匹配
Draw RuntimeId 已经存在
DrawCount <= 0
Draw ToIndex 不是当前 append index
```

Blueprint 都应该：

```text
Return false
```

不要：

```text
猜一个 child
创建一个替代 card
ClearChildren
强行修正 ToIndex
```

Controller 会进入 immediate fallback/reducer/reconcile 路径。

---

# 第八节 E：Finish 生命周期调整

## 8.52 当前 `CardZoneChanged` Finish 的已验证职责

当前 `FinishPresentationRecord → CardZoneChanged` 对 PlayArea slice 会：

```text
如果 PlayedCardWidget 有效
→ RemoveFromParent
→ PlayedCardWidget = None
→ NotifyPresentationRecordFinished
```

这条行为继续保留。

Draw / Hand discard 不应该把这条职责改掉。

---

## 8.53 Hand discard 正常 Finish 不恢复被 Collapsed 的 Widget

Hand → Discard active window：

```text
历史 Hand 有 Card X
→ Card X transient Collapsed
```

正常 Finish 时不要：

```text
HiddenHandCardWidget.SetVisibility(Visible)
```

否则会出现：

```text
可见
→ 消失
→ 又出现
→ reducer 后再消失
```

正确序列：

```text
可见
→ Collapsed
→ Notify
→ reducer 从 HandCards 正式删除
→ HUD rebuild
→ 继续不存在
```

---

## 8.54 Draw 正常 Finish 不 Remove transient card

DrawPile → Hand active window：

```text
历史 Hand 不含 Card X
→ transient Card X 加入 HB_Hand
```

正常 Finish 前不要：

```text
ZoneChangedDrawnCardWidget.RemoveFromParent
```

否则会产生：

```text
无 Card X
→ Card X 出现
→ Card X 消失
→ reducer rebuild 后又出现
```

即错误的：

```text
appear → disappear → appear
```

正确序列：

```text
无 Card X
→ transient Card X 出现
→ Notify
→ reducer 正式 Insert Card X
→ HUD 同一 tick 重建正式 Card X
→ 视觉保持存在
```

---

## 8.55 推荐 Finish 结构

`FinishPresentationRecord → CardZoneChanged` 保持 PlayArea cleanup，然后统一 Notify。

推荐结构概念上是：

```text
CardZoneChanged
↓
若 PlayedCardWidget 有效
→ RemoveFromParent
→ PlayedCardWidget = None
↓
NotifyPresentationRecordFinished
↓
HiddenHandCardWidget = None
↓
ZoneChangedDrawnCardWidget = None
```

这里：

```text
清引用 ≠ 改视觉
```

所以 Hand discard 不会被重新显示，Draw transient 也不会被主动 Remove。

当前 `NotifyPresentationFinished` 的 C++ forwarding 是 deferred，因此不会在 Blueprint 当前调用栈内重入并抢先启动下一条 Record；仍保持现有统一完成结构，不要自己做同步 next-record 调用。

---

## 8.56 如果当前 Finish 图存在执行线汇合问题

不要把：

```text
IsValid(PlayedCardWidget).Valid
IsValid(PlayedCardWidget).Invalid
```

两根白线硬接到一个普通节点输入。

可以沿用当前已经工作的结构，或者使用：

```text
Sequence
```

例如：

```text
CardZoneChanged
→ Sequence

Then 0
→ Branch(IsValid(PlayedCardWidget))
   └ true → RemoveFromParent → PlayedCardWidget=None

Then 1
→ NotifyPresentationRecordFinished

Then 2
→ HiddenHandCardWidget=None
→ ZoneChangedDrawnCardWidget=None
```

`Then 0` 的 false 不接即可。

不要为了合并 exec 线把这些引用 Promote 成新的生命周期状态。

---

# 第八节 F：Cancel / Reconcile

## 8.57 Hand discard Cancel 复用 `HiddenHandCardWidget` 恢复

当前 Cancel 已经有 CardPlayed 所需的：

```text
IsValid(HiddenHandCardWidget)
→ 恢复 Visibility
→ 清引用
```

本节 Hand discard 复用同一变量，因此该恢复路径天然也覆盖：

```text
Hand → DiscardPile active visual
```

例如：

```text
历史 Hand：A B C
↓
Record：B → Discard
↓
B transient Collapsed
↓
Cancel
↓
B Visible
```

最终仍是历史：

```text
A B C
```

而不是提前丢牌。

---

## 8.58 Draw Cancel 必须移除 transient Draw Widget

在 `Cancel Presentation Record Playback` 的通用 Card cleanup 区增加：

```text
Get ZoneChangedDrawnCardWidget
→ Is Valid
```

### Valid

```text
Remove From Parent
→ ZoneChangedDrawnCardWidget = None
```

### Invalid

继续公共清理。

这样：

```text
历史 Hand：A B
↓
Draw transient：A B C
↓
Cancel
↓
C 被 RemoveFromParent
↓
回到 A B
```

不需要全量重建 Hand。

---

## 8.59 CardZone Cancel 不恢复 pile 数字，因为本节没有提前覆盖它们

本节 active playback 没有写：

```text
DrawCount Text
DiscardCount Text
ExhaustCount Text
```

所以 Cancel 不需要：

```text
重新 Set DrawCount
重新 Set DiscardCount
```

它们一直就是历史 ViewModel 的值。

这是本节“不手算 pile count”的一个重要收益。

---

## 8.60 Cancel 仍然不能 Notify

检查新增 Draw transient cleanup 后，整个：

```text
Cancel Presentation Record Playback
```

仍不能出现：

```text
NotifyPresentationFinished
NotifyPresentationRecordFinished
FinishPresentationRecord
```

Cancel 只做：

```text
停止 Timer
恢复/移除当前 transient visual
清引用
清 Type / Token
```

不要把取消的 ZoneChanged 当正常完成。

---

## 8.61 本节不把 PlayArea Cancel 的全局架构问题提前宣告完成

当前本节重点是新增：

```text
DrawPile → Hand Cancel
Hand → DiscardPile Cancel
```

PlayArea → Destination 的正常 Finish 已验证，但整个 A2E 的 cancellation / stale visual 最终仍会在后面的：

```text
全局 Cancel / Reconcile 收尾
```

重新统一审计。

所以本节不要因为新增两条 Cancel cleanup 就提前写：

```text
All CardZoneChanged cancellation SEALED
```

只能记录当前新 slice 的结构和实际运行证据。

---

# 第八节 G：Compile + Save

## 8.62 Compile 前节点检查

先检查新 helper：

```text
FindHandCardWidgetByRuntimeId
```

再检查 Event：

```text
PlayHandDiscardPresentation
PlayDrawToHandPresentation
```

最后检查：

```text
BeginPresentationRecordPlayback.CardZoneChanged
FinishPresentationRecord.CardZoneChanged
Cancel Presentation Record Playback
```

确认没有 orphan pin。

---

## 8.63 推荐 Compile 顺序

```text
1. WBP_BattleCard
   → Compile
   → 0 Errors
   → Save

2. WBP_BattleHUD
   → Compile
   → 0 Errors
   → Save
```

如果 HUD 中 `CardView` 变量访问出现 stale node：

```text
先重新 Compile WBP_BattleCard
→ 回 HUD Refresh Node
→ 仍失败再从 As WBP_BattleCard pin 重新拉 Get CardView
```

不要拆掉 Router 的 zone pair 结构。

---

## 8.64 静态结构验收清单

```text
[ ] 当前 producer 集合已明确
[ ] Hand→PlayArea 没有被当 CardZoneChanged
[ ] Discard→Draw 没有被当 CardZoneChanged
[ ] PlayArea→Hand rollback 没有被正常接管

[ ] FindHandCardWidgetByRuntimeId 已创建
[ ] helper 只遍历 HB_Hand
[ ] helper 返回 exact RuntimeId Widget + ChildIndex

[ ] PlayArea→Destination 原路径仍存在
[ ] PlayArea 只接受 Discard/Exhaust/Removed

[ ] Hand→Discard Router 使用 exact RuntimeId
[ ] Hand→Discard 检查 ChildIndex==FromIndex
[ ] Hand→Discard 检查 CardId
[ ] Hand→Discard 找不到 exact Widget → Return false
[ ] Hand→Discard Event 保存 Token
[ ] ActiveType=CardZoneChanged
[ ] HiddenHandCardWidget=exact widget
[ ] exact widget Visibility=Collapsed
[ ] Hand→Discard 不提前改 DiscardCount
[ ] Hand→Discard 最后 StartPresentationFinishTimer

[ ] ZoneChangedDrawnCardWidget 已创建
[ ] Draw Router 只接受 DrawPile→Hand
[ ] Draw RuntimeId!=-1
[ ] Draw CardId!=None
[ ] Draw 当前 Hand 不存在相同 RuntimeId
[ ] Draw ViewModel.DrawCount>0
[ ] Draw ToIndex==HB_Hand.ChildrenCount
[ ] Draw Event 使用 Make Presentation Card View
[ ] Draw transient bGameplayPlayable=false
[ ] Draw transient Hit Test Invisible
[ ] Draw 使用 AddChildToHorizontalBox
[ ] Draw 不提前改 DrawCount
[ ] Draw 最后 StartPresentationFinishTimer

[ ] Finish Hand discard 不恢复 Visible
[ ] Finish Draw 不 Remove transient card
[ ] Finish PlayArea cleanup 保持
[ ] Finish 仍 exact-token Notify

[ ] Cancel Hand discard 恢复 HiddenHandCardWidget
[ ] Cancel Draw transient RemoveFromParent
[ ] Cancel 不修改 ViewModel
[ ] Cancel 不 Notify

[ ] WBP_BattleHUD Compile 0 Errors
[ ] WBP_BattleHUD 已 Save
```

---

# 第八节 PIE 验收部分

## 8.65 PIE 验收原则：必须使用真实 producer

本节不能用手工：

```text
Make CardZoneChanged Payload
直接调用 PlayDrawToHandPresentation
直接调用 PlayHandDiscardPresentation
```

必须验证完整：

```text
DeckRuntime commit
→ Action producer
→ frozen Card snapshot
→ Presentation Record
→ Controller
→ Blueprint Router
→ async visual
→ exact-token Notify
→ reducer
→ ViewModel refresh
```

---

## 8.66 PIE A：先验证一条真实 `DrawPile → Hand`

可使用项目当前真实：

```text
TestDrawCard
Opening Hand draw
TurnStart draw
```

中任一稳定路径。

捕获 Record，至少确认：

```text
Type      = CardZoneChanged
FromZone  = DrawPile
ToZone    = Hand
RuntimeId = <实际值>
CardId    = <实际值>
ToIndex   = <实际值>
```

同时记录开始前：

```text
Historical Hand Count = H
Historical DrawCount  = D
```

当前 producer 应满足：

```text
ToIndex = H
D > 0
```

---

## 8.67 Draw active window 的正确视觉

Record 开始时预期：

```text
FindHandCardWidgetByRuntimeId(RuntimeId)
→ Found=false
```

然后：

```text
MakePresentationCardView(frozen snapshot)
→ Create transient WBP_BattleCard
→ Add to HB_Hand end
```

0.5 秒窗口内：

```text
视觉 Hand = H + 1 张
新卡身份/名称/费用/卡图来自 frozen Record
新卡不可 Gameplay click
历史 ViewModel.HandCards 仍是 H 张
正式 DrawCount 数字仍是 D
```

这是预期 Presentation 历史窗口，不是 bug。

---

## 8.68 Draw 完成后必须保持新卡存在

约 0.5 秒后：

```text
Finish CardZoneChanged
→ 不 Remove ZoneChangedDrawnCardWidget
→ exact-token Notify
→ reducer DrawCount D→D-1
→ reducer Insert frozen card into HandCards
→ HUD rebuild
```

最终：

```text
Hand Count = H + 1
DrawCount   = D - 1
```

视觉序列必须是：

```text
无新卡
→ 新卡出现
→ 新卡保持存在
```

禁止：

```text
无
→ 出现
→ 消失
→ 再出现
```

---

## 8.69 Draw 不能产生重复 RuntimeId

最终正式 Hand 中：

```text
RuntimeId = 当前 Draw Record.RuntimeId
```

只能出现一次。

如果看到两个完全相同 RuntimeId 的视觉卡：

```text
停止验收
```

检查：

```text
1. Draw Router 的 Found=false 前置是否失效
2. transient 是否在 Notify 前错误复制了两次
3. Event 是否被调用两次
4. reducer refresh 前是否又手工重建了一张
```

不要通过隐藏其中一张掩盖重复问题。

---

## 8.70 Draw exact-token completion 与后续 Record

新卡出现后继续观察：

```text
StartPresentationFinishTimer
→ FinishPresentationRecord.CardZoneChanged
→ NotifyPresentationRecordFinished
→ NotifyPresentationFinished(ActivePresentationToken)
```

并确认：

```text
后续 Record 继续
最终不挂 Resolving
```

如果 Opening Hand 有多条 Draw Record，应看到每条按 committed 顺序逐个完成，而不是一次跳最终 5 张。

---

## 8.71 Opening Hand 多 Draw 顺序抽查

如果开局实际有：

```text
Draw #1
Draw #2
Draw #3
...
```

每条 `DrawPile → Hand` 都应：

```text
Card N transient 出现
→ Notify
→ 正式 Hand 接纳 Card N
→ 下一条 Draw 才开始
```

要求：

```text
Hand 0→1→2→3...
DrawCount 每条完成后逐步下降
```

不能：

```text
第一条 Record 播放时 Hand 直接跳到最终完整 Opening Hand
```

---

## 8.72 PIE B：验证真实 `Hand → DiscardPile`

优先使用：

```text
TestDiscardCard
```

或项目当前正常 EndTurn discard 流程。

捕获一条 Record：

```text
Type      = CardZoneChanged
FromZone  = Hand
ToZone    = DiscardPile
RuntimeId = <实际值>
CardId    = <实际值>
FromIndex = <实际值>
```

开始前记录：

```text
Historical Hand Count    = H
Historical DiscardCount = C
```

---

## 8.73 Hand discard Router 必须命中 exact row

预期：

```text
FindHandCardWidgetByRuntimeId(RuntimeId)
→ Found=true
→ ChildIndex == FromIndex
→ CardId match
```

任一不匹配时正确行为是：

```text
Return false
```

不能拿别的 Hand card 顶替。

---

## 8.74 Hand discard active window 正确视觉

Event 开始后：

```text
exact Hand Widget
→ Collapsed
```

0.5 秒窗口：

```text
视觉 Hand 中该卡已经消失
其他 Hand cards 保持
历史 ViewModel.HandCards 仍包含该卡
DiscardCount 数字仍为历史 C
```

不要要求 pile count 在 active 开始时提前变成：

```text
C + 1
```

本节设计就是让正式数字在 reducer 后更新。

---

## 8.75 Hand discard 正常完成不能闪回

正确：

```text
Card X 可见
→ Card X Collapsed
→ Notify
→ reducer 正式 Remove
→ HUD rebuild
→ Card X 继续不存在
```

禁止：

```text
可见
→ 消失
→ 又出现
→ 再消失
```

如果出现这种闪回，检查：

```text
1. Finish 是否恢复 HiddenHandCardWidget Visible
2. Notify 前是否按旧 ViewModel 重建 Hand
3. Cancel 是否被错误触发
```

---

## 8.76 Hand discard 完成后的正式值

Notify 后 Controller reducer 应推进：

```text
Hand Count    H → H - 1
DiscardCount  C → C + 1
```

最终：

```text
目标 RuntimeId 不在 Hand
其他 Hand cards 顺序正确
DiscardCount 正确
```

---

## 8.77 PIE C：完整 EndTurn 多张 Hand discard

这是本节最重要的组合验收之一。

准备至少：

```text
Hand 中 2 张以上卡
```

然后正常：

```text
EndTurn
```

当前 BattleManager 会基于实际 Hand 构建多个 `DiscardCardAction`，每个动作各自提交一条：

```text
Hand → DiscardPile
```

观察每条 Record 依次推进。

---

## 8.78 为什么每条 Record 都必须重新使用自己的 `FromIndex`

例如开始 Hand：

```text
[A, B, C]
```

第一条丢 A 后，正式 WorkingSnapshot 变成：

```text
[B, C]
```

下一条 Record 的 `FromIndex` 是 producer 在当时 Gameplay commit 后冻结的 index。

所以 Router 每次都必须使用：

```text
当前 Record.FromIndex
+ 当前已完成历史 HB_Hand
```

不能缓存第一条 Record 开始前的 Hand index map。

正确 sequential reducer 会让每条 index 与当前历史一致。

---

## 8.79 EndTurn 多 discard 视觉要求

如果有三张剩余手牌，预期类似：

```text
A B C
→ A 消失
→ Notify / formal rebuild
→ B C
→ B 消失
→ Notify / formal rebuild
→ C
→ C 消失
→ Notify / formal rebuild
→ empty
```

实际丢弃顺序以 producer Record 为准。

不能第一条 discard 一开始就：

```text
HB_Hand.ClearChildren
```

直接显示 empty。

---

## 8.80 EndTurn pile 数字必须逐条正式推进

因为本节不提前覆盖 pile count，预期：

```text
DiscardCount
C
→ 第一条 Notify 后 C+1
→ 第二条 Notify 后 C+2
→ 第三条 Notify 后 C+3
```

而不是：

```text
第一条 discard active 时直接跳最终 C+3
```

这能直接证明 A2E reducer 仍按 Record 顺序推进。

---

## 8.81 PIE D：PlayArea → Destination 回归

新增 Router 后必须重新做一次最小 smoke：

```text
打一张正常进入 Discard 的卡
→ CardPlayed
→ effects
→ CardZoneChanged PlayArea→DiscardPile
```

确认：

```text
原 PlayedCardWidget retirement 仍正常
CardZoneChanged 仍 Return true
Finish 仍 Remove PlayedCardWidget
后续 reducer 正常
```

再至少抽查一张：

```text
Exhaust / Removed destination
```

如果当前卡组可稳定构造。

不要因为新增 Draw / Hand 路径把原 validated slice 破坏。

---

## 8.82 PIE E：CardPlayed 不得新增 `Hand → PlayArea CardZoneChanged`

检查一次普通打牌 committed records。

正确：

```text
CardPlayed
→ Damage / Block / Status / followups
→ CardZoneChanged PlayArea→Destination
```

不应该额外出现：

```text
CardZoneChanged Hand→PlayArea
```

如果出现，先作为 producer duplication 排查。

Blueprint Router 即使看到它，也应该：

```text
Return false
```

而不是播第二次 Hand 离场。

---

## 8.83 PIE F：Shuffle → Draw 的边界抽查

制造：

```text
DrawPile empty
DiscardPile 有牌
需要继续 Draw
```

当前真实 Action 顺序是：

```text
DeckShuffled
→ Retry Draw
→ CardZoneChanged DrawPile→Hand
```

本节时 `DeckShuffled` 如果还没接 Blueprint，仍可能 immediate fallback。

这不是本节失败。

关键要求：

```text
CardZoneChanged Draw path
```

不要自己重复：

```text
DiscardPile→DrawPile
```

的数量变化。

当 Draw Record 真正开始时，历史 ViewModel 应已经包含前一条 `DeckShuffled` reducer 的结果，因此：

```text
ViewModel.DrawCount > 0
```

校验才能通过。

---

## 8.84 Draw Cancel 运行时抽查（若有正式入口）

如果能稳定触发正式 Cancel：

```text
历史 Hand = H
↓
Draw transient 新增 Card X
↓
0.5 秒内 Cancel
```

预期：

```text
Timer 清除
→ ZoneChangedDrawnCardWidget.RemoveFromParent
→ Hand 回到历史 H
→ DrawCount 仍是历史值
→ 不 Notify
```

最终 ViewModel 没有提前包含 Card X。

---

## 8.85 Hand discard Cancel 运行时抽查（若有正式入口）

```text
历史 Hand 含 Card X
↓
Hand→Discard active
↓
Card X Collapsed
↓
0.5 秒内 Cancel
```

预期：

```text
Timer 清除
→ HiddenHandCardWidget 恢复 Visible
→ Hand 回到历史状态
→ DiscardCount 仍是历史值
→ 不 Notify
```

如果当前没有可靠 Cancel 入口，不要在 Blueprint 中硬调 Cancel 伪造证据；留到全局 Cancel / Reconcile 章节。

---

## 8.86 如果 Draw transient 出现但随后闪一下消失，排查顺序

看到：

```text
新卡出现
→ 消失
→ 又出现
```

检查：

```text
1. Finish.CardZoneChanged 是否 Remove ZoneChangedDrawnCardWidget
2. Notify 前是否 ClearChildren(HB_Hand)
3. 是否在 Finish 前调用整个 ViewModel Hand rebuild
4. Draw Event 是否错误把 transient 放进临时 Overlay 而不是 HB_Hand
```

正常 Finish 不主动删除 transient draw card。

---

## 8.87 如果 Hand discard 卡消失后又回来，排查顺序

```text
1. Finish 是否 Set HiddenHandCardWidget Visible
2. 当前是否错误进入 Cancel
3. Notify 前是否收到旧 ViewModel refresh
4. Router 是否 Return true 但 Timer 未正确 ownership
```

正常 Finish 不恢复该卡。

---

## 8.88 如果下一条 discard 找不到 RuntimeId，排查顺序

完整 EndTurn 中第二/第三条 Record 出现：

```text
FindHandCardWidgetByRuntimeId = false
```

先检查：

```text
1. 上一条 discard exact-token 是否真正完成
2. 上一条 reducer 后 HB_Hand 是否按 WorkingSnapshot rebuild
3. 当前 Record.FromIndex 是否对应新的 Hand
4. 当前 Card.RuntimeId 是否与正式 Hand 中卡一致
5. 是否提前 ClearChildren 或删除了额外卡牌
```

不要把 helper 改成只按 CardId 找。

RuntimeId 才是实例身份。

---

## 8.89 如果 Draw Router 因 `ToIndex` 不匹配 fallback

先记录：

```text
Record.ToIndex
HB_Hand.ChildrenCount
```

当前 producer 应满足：

```text
ToIndex == ChildrenCount
```

如果不满足：

```text
不要强行 AddChild 到末尾
```

先判断是否 producer 行为已经变化，或当前 Hand visual 没有跟上历史 ViewModel。

只有明确更新了 producer 契约后，才扩展 Blueprint 的任意 index insert。

---

## 8.90 如果 pile 数字在 active window 没立即变化，不算失败

本节明确设计为：

```text
card visual 先表现
pile numeric truth 在 Notify/reducer 后正式推进
```

因为 `CardZoneChanged` Payload 当前没有冻结 pile before/after count。

不要为了“看起来同步”临时加：

```text
DiscardCount + 1
DrawCount - 1
```

下一节 `DeckShuffled` 有正式 frozen counts，届时才使用它自己的 payload 直接显示对应 pile transition。

---

## 8.91 本节正式 PIE 验收清单

只有以下条件全部满足，才可以把当前 producer 集合的 `CardZoneChanged` 视为完整验证：

```text
[ ] DrawPile→Hand 使用真实 producer Record
[ ] Draw RuntimeId / CardId 正确
[ ] Draw ToIndex == 当前 Hand append index
[ ] Draw 开始前 exact RuntimeId 不在 Hand
[ ] Draw transient 使用 frozen Card snapshot
[ ] Draw transient 不可 Gameplay 交互
[ ] Draw 只新增一张卡
[ ] Draw active 不手算 pile count
[ ] Draw exact-token completion 正常
[ ] Draw reducer 后 DrawCount 正确 -1
[ ] Draw reducer 后 Hand 正式包含 exact RuntimeId
[ ] Draw 无 appear→disappear→appear 闪回

[ ] Hand→Discard 使用真实 producer Record
[ ] exact RuntimeId 找到
[ ] ChildIndex == FromIndex
[ ] CardId 匹配
[ ] 只 Collapsed exact Widget
[ ] 其他 Hand cards 不受影响
[ ] active 不手算 DiscardCount
[ ] exact-token completion 正常
[ ] reducer 后 exact card 正式离开 Hand
[ ] reducer 后 DiscardCount 正确 +1
[ ] 无 disappear→reappear→disappear 闪回

[ ] EndTurn 多 discard 按 Record 顺序逐条可见
[ ] pile 数字不提前跳最终值
[ ] 每条完成后下一条才开始
[ ] 最终不挂 Resolving

[ ] PlayArea→Destination 回归正常
[ ] CardPlayed 没有重复 Hand→PlayArea CardZoneChanged
[ ] CardZoneChanged 不重复 DeckShuffled 事实

[ ] 若做 Cancel：Draw Cancel 恢复历史 Hand
[ ] 若做 Cancel：HandDiscard Cancel 恢复历史 Hand
[ ] Cancel 不 Notify
```

---

## 8.92 正式验证日志模板

本节实际通过后，再把真实值写入：

```text
docs/UIA2EBlueprintValidationLog.md
```

建议：

```text
CardZoneChanged DrawPile -> Hand PIE
RuntimeId                 = <实际值>
CardId                    = <实际值>
FromIndex                 = <实际值>
ToIndex                   = <实际值>
HandCountBefore           = <H>
DrawCountBefore           = <D>
TransientCardShown        = Yes
DuplicateRuntimeId        = No
GameplayInputEnabled      = No
PileCountChangedEarly     = No
ExactTokenCompleted       = Yes
FinalHandCount            = <H+1>
FinalDrawCount            = <D-1>
FlashbackObserved         = No
ReturnedToIdle            = Yes
Result                    = PASS

CardZoneChanged Hand -> DiscardPile PIE
RuntimeId                 = <实际值>
CardId                    = <实际值>
FromIndex                 = <实际值>
HandCountBefore           = <H>
DiscardCountBefore        = <C>
ExactWidgetFound          = Yes
ExactIndexMatched         = Yes
TransientCollapsed        = Yes
OtherHandCardAffected     = No
PileCountChangedEarly     = No
ExactTokenCompleted       = Yes
FinalHandCount            = <H-1>
FinalDiscardCount         = <C+1>
FlashbackObserved         = No
ReturnedToIdle            = Yes
Result                    = PASS

CardZoneChanged EndTurn Sequential Discard PIE
RecordsObserved           = <实际条数>
SequentialPlayback        = Yes
PileCountAdvancedPerRecord= Yes
FinalHandMatchesViewModel = Yes
FinalDiscardMatchesViewModel = Yes
Result                    = PASS
```

没有实际观察到的字段写：

```text
Not Observed
```

不要编造。

---

## 8.93 本节完成后的验证状态

如果结构已经接完但还没 PIE：

```text
CardZoneChanged PlayArea→Destination  VALIDATED
CardZoneChanged DrawPile→Hand         WIRED / NOT PIE VALIDATED
CardZoneChanged Hand→DiscardPile      WIRED / NOT PIE VALIDATED
```

只有真实 PIE 全通过后，才可以写：

```text
CardZoneChanged PlayArea→Destination  VALIDATED
CardZoneChanged DrawPile→Hand         VALIDATED
CardZoneChanged Hand→DiscardPile      VALIDATED

CardZoneChanged current producer set  FULLY VALIDATED
```

这里的“current producer set”很重要。

它不代表未来新增任意 `ECardZone` 组合时 Blueprint 自动支持。

---

## 8.94 本节完成后 A2E 仍未 Seal

即使当前 `CardZoneChanged` producer 集合全部通过，UI-A2E 仍然至少还有：

```text
DeckShuffled
Victory
Defeat
ResolutionFault
全局 Cancel / Reconcile 收尾
完整 EndTurn / Draw / terminal 全链 PIE
最终文档 Seal
```

不能提前进入 UI-A3。

---

## 8.95 下一节

下一节处理：

```text
DeckShuffled Blueprint Playback
```

将继续严格使用 frozen payload：

```text
MovedCardCount
DrawCountBefore
DrawCountAfter
DiscardCountBefore
DiscardCountAfter
```

下一节重点：

```text
如何在不查询 mutable DeckRuntime 的情况下表现 shuffle
如何让 Draw / Discard pile 数字只使用 frozen before/after
如何接 Router / Timer / Finish / Cancel
如何保证 DeckShuffled → DrawPile→Hand 的 Record 顺序
如何避免 CardZoneChanged 重复 shuffle 事实
如何做 DrawPile empty + DiscardPile refill 的真实 PIE 验收
```

---

# 第九节：实现并验收 `DeckShuffled` Blueprint Playback

## 9.1 本节目标与进入条件

只有第八节当前 producer 集合中的 `CardZoneChanged` 已经完成结构接线并经过对应 PIE 验收后，才进入本节。

本节目标是把当前仍然由 C++ immediate fallback 处理的：

```text
DeckShuffled
```

正式接入 `WBP_BattleHUD` 的 Blueprint async playback，并完成真实的：

```text
DrawPile 为空
DiscardPile 有牌
→ Shuffle
→ Retry Draw
```

全链 PIE 验收。

本节完成后，在真实 PIE 通过的前提下，允许把：

```text
DeckShuffled = VALIDATED
```

但整个 UI-A2E 仍然保持：

```text
PARTIAL
```

因为后续还存在 Terminal、全局 Cancel/Reconcile、全链 Acceptance 与最终 Seal。

---

## 9.2 先锁定本节正式 Payload

当前 `FDeckShuffledPresentationPayload` 只有五个冻结字段：

```text
MovedCardCount
DrawCountBefore
DrawCountAfter
DiscardCountBefore
DiscardCountAfter
```

因此本节的所有活动视觉必须只以这五个字段为当前 Record 的事实来源。

禁止为了显示洗牌后的牌堆数量去查询：

```text
DeckRuntime
DrawPile TArray
DiscardPile TArray
UCardInstance
Gameplay BattleManager 的实时牌堆
```

Blueprint 不拥有 Gameplay Deck 真值。

---

## 9.3 当前真实 producer 的 shuffle 前置条件

当前 `UDeckRuntime::ShuffleDiscardIntoDrawPileCommit()` 的 committed shuffle 条件是：

```text
DiscardPile.Num() > 0
AND DrawPile.Num() == 0
```

如果：

```text
DiscardPile 为空
```

或者：

```text
DrawPile 非空
```

commit 会被跳过，不应产生正式 `DeckShuffled` Record。

成功时当前实现固定执行：

```text
MovedCardCount       = DiscardCountBefore
DrawCountBefore      = 0
DrawCountAfter       = MovedCardCount
DiscardCountAfter    = 0
```

并保留总卡数：

```text
DrawCountBefore + DiscardCountBefore
==
DrawCountAfter + DiscardCountAfter
```

这些关系是本节 Router 可以做的防御性校验基础。

---

## 9.4 当前真实 Action 顺序

当 `DrawCardAction` 发现：

```text
DrawPile empty
DiscardPile non-empty
```

当前代码不是直接 draw，而是把一个 continuation batch 放到队头：

```text
ShuffleDeckAction
→ RetryDrawAction
```

所以 committed Presentation 顺序应是：

```text
DeckShuffled
→ CardZoneChanged(DrawPile → Hand)
```

本节必须保留这个顺序。

不要在 `DeckShuffled` Event 中顺便创建下一张 Hand 卡；下一张卡属于下一条 `CardZoneChanged` Record。

---

## 9.5 当前 Controller reducer 的正式语义

`ApplyRecordToWorkingSnapshot` 对 `DeckShuffled` 的行为是：

```text
先要求：
WorkingSnapshot.DrawCount
== Record.DrawCountBefore

WorkingSnapshot.DiscardCount
== Record.DiscardCountBefore

然后：
WorkingSnapshot.DrawCount
= Record.DrawCountAfter

WorkingSnapshot.DiscardCount
= Record.DiscardCountAfter
```

也就是说 Blueprint active playback 发生时：

```text
ViewModel / Working historical state
仍然是 Before
```

Blueprint 临时显示：

```text
After
```

exact-token Notify 后 reducer 才正式进入 After。

正确视觉原则与 Damage、Block、Energy、Status 完全一致：

```text
Before
→ transient After
→ Notify
→ formal After
```

而不是：

```text
Before
→ After
→ Before
→ After
```

---

# 第九节 A：定位正式 Draw / Discard 数量控件

## 9.6 不新建第二套牌堆计数 UI

当前 `WBP_BattleHUD` Designer 已经存在：

```text
DrawPilePanel
DiscardPilePanel
ExhaustPanel
```

并且：

```text
Event Battle HUD View Model Changed
```

已经会根据 ViewModel 更新 Draw / Discard / Exhaust 数量。

本节不增加：

```text
Txt_ShuffleDrawCount
Txt_ShuffleDiscardCount
第二套 DrawPilePanel
第二套 DiscardPilePanel
```

而是直接复用正常 HUD 正在使用的两个计数 TextBlock。

---

## 9.7 如何定位 Draw pile 的正式数字控件

打开：

```text
WBP_BattleHUD
→ Event Graph
→ Event Battle HUD View Model Changed
```

找到读取：

```text
ViewModel.DrawCount
```

的那条数据线。

沿线追到最终的：

```text
Set Text
```

节点。

该 `Set Text.Target` 对应的 TextBlock 就是本节正式 Draw pile 数字控件。

本文后续把它称为逻辑别名：

```text
DrawPileCountText
```

注意：

```text
DrawPileCountText 只是本文逻辑别名
```

不要为了文档一致强行改 Designer 变量名。

---

## 9.8 如何定位 Discard pile 的正式数字控件

同样找到：

```text
ViewModel.DiscardCount
```

对应的正常 HUD `Set Text`。

本文逻辑别名：

```text
DiscardPileCountText
```

本节只有这两个正式计数控件会被 DeckShuffled transient override。

不要碰：

```text
ExhaustCount
HandCount
Energy
HP / Block
```

---

## 9.9 记录正常 HUD 的文本格式

如果当前正常控件只是纯数字：

```text
5
```

那么后面直接：

```text
int32
→ ToText(Integer)
→ SetText
```

如果当前正常 UI 是带前缀的格式，例如：

```text
Draw 5
Discard 3
```

不要为了本节另写一种格式。

应复用当前已经存在的格式方式，只把 Current Count 输入替换成 Record 的冻结 `After`。

本节后续所有“SetText(After)”都表示：

```text
按当前正式 HUD 的同一种格式显示 After
```

---

# 第九节 B：建立 `PlayDeckShuffledPresentation`

## 9.10 新建 Custom Event

在：

```text
WBP_BattleHUD
→ Event Graph
```

新建：

```text
PlayDeckShuffledPresentation
```

不要在生产 Blueprint 名中加入：

```text
A2E
Phase6
```

新增两个输入：

```text
DeckShuffled : FDeckShuffledPresentationPayload
Token        : FPresentationPlaybackToken
```

确认是 Object/Struct 正确类型，不要拆成五个独立 Integer 输入。

---

## 9.11 Event 第一件事保存精确 Token

执行链：

```text
PlayDeckShuffledPresentation
↓
Set ActivePresentationToken
```

Value：

```text
Event.Token
→ ActivePresentationToken
```

不要在先改 pile 数字以后才保存 Token。

从 Blueprint 接管这条 Record 的第一刻起，就必须明确 ownership。

---

## 9.12 第二步设置 Active Type

继续：

```text
Set ActivePresentationToken
↓
Set ActivePresentationType
```

值选择：

```text
DeckShuffled
```

得到：

```text
ActivePresentationType = DeckShuffled
```

Finish 和 Cancel 都会依赖这个值。

---

## 9.13 Break 正式 Payload

从 Event 输入：

```text
DeckShuffled
```

拖出：

```text
Break Deck Shuffled Presentation Payload
```

确认五个 pin：

```text
MovedCardCount
DrawCountBefore
DrawCountAfter
DiscardCountBefore
DiscardCountAfter
```

建议整个 Event 只用一个 Break 节点。

不要 `Promote to Variable` 保存整个 Payload。

---

## 9.14 Draw pile transient 直接显示 `DrawCountAfter`

从：

```text
DrawCountAfter
```

接当前正式 HUD 的 count 格式，再接：

```text
DrawPileCountText.SetText
```

如果当前格式是纯数字：

```text
DrawCountAfter
→ ToText(Integer)
→ DrawPileCountText.SetText
```

禁止：

```text
ViewModel.DrawCount + MovedCardCount
```

也禁止：

```text
DrawCountBefore + MovedCardCount
```

来重新构造最终显示值。

虽然当前 producer 下数值应相同，但 frozen final fact 已经存在：

```text
DrawCountAfter
```

直接消费它。

---

## 9.15 Discard pile transient 直接显示 `DiscardCountAfter`

从：

```text
DiscardCountAfter
```

接正常 HUD 格式，再：

```text
DiscardPileCountText.SetText
```

执行线建议：

```text
Set ActivePresentationType = DeckShuffled
↓
DrawPileCountText.SetText(DrawCountAfter)
↓
DiscardPileCountText.SetText(DiscardCountAfter)
```

两个 SetText 在同一帧完成即可。

本节不需要做逐张牌从右侧飞到左侧的动画。

---

## 9.16 `MovedCardCount` 本节只用于校验，不用于重算 UI

当前最小视觉只需要：

```text
Draw pile 数字 → After
Discard pile 数字 → After
```

所以 `MovedCardCount` 不必额外显示成浮字。

它的正式用途是 Router 校验：

```text
MovedCardCount > 0
```

以及验证 pile transition 自洽。

不要做：

```text
DrawAfter = DrawBefore + MovedCardCount
```

作为显示值。

---

## 9.17 最后启动公共 Finish Timer

从：

```text
DiscardPileCountText.SetText
```

继续接：

```text
StartPresentationFinishTimer
```

最终 Event：

```text
PlayDeckShuffledPresentation(DeckShuffled, Token)
↓
ActivePresentationToken = Token
↓
ActivePresentationType = DeckShuffled
↓
Break DeckShuffled
↓
DrawPileCountText = DrawCountAfter
↓
DiscardPileCountText = DiscardCountAfter
↓
StartPresentationFinishTimer
```

继续使用现有公共：

```text
0.5 秒
Looping = false
Event = FinishPresentationRecord
```

不要新建 `ShuffleTimer`。

---

# 第九节 C：实现 `DeckShuffled` Router

## 9.18 找到当前 fallback case

打开：

```text
WBP_BattleHUD
→ BeginPresentationRecordPlayback
```

找到：

```text
Switch on EBattlePresentationRecordType
→ DeckShuffled
```

当前保存版本仍是：

```text
DeckShuffled
→ Return false
```

本节只改这个 case。

不要顺手修改：

```text
Victory
Defeat
ResolutionFault
```

---

## 9.19 Router 先 Break `Record.DeckShuffled`

从输入 `Record` 的：

```text
DeckShuffled
```

struct pin 拉：

```text
Break Deck Shuffled Presentation Payload
```

Router 需要全部五个字段。

建议保持：

```text
Router = validation + routing
Event  = visual + timer ownership
```

不要直接在 Router 里 SetText。

---

## 9.20 校验一：所有 count 非负

分别判断：

```text
MovedCardCount >= 0
DrawCountBefore >= 0
DrawCountAfter >= 0
DiscardCountBefore >= 0
DiscardCountAfter >= 0
```

但是 committed shuffle 必须实际移动牌，所以进一步要求：

```text
MovedCardCount > 0
```

推荐最后组合成：

```text
CountsNonNegative
AND HasMovedCards
```

如果任何 count 为负或 MovedCardCount 为 0：

```text
Return false
```

不要 Clamp。

---

## 9.21 校验二：当前 producer 要求 `DrawCountBefore == 0`

当前 `ShuffleDiscardIntoDrawPileCommit` 在 Draw pile 非空时不会 commit。

所以 Router 增加：

```text
DrawCountBefore == 0
```

得到：

```text
DrawWasEmpty
```

这不是为了永远限制未来所有 shuffle 设计，而是对当前正式 producer contract 做显式能力声明。

如果未来 producer 改为允许 partial reshuffle，再同步修改这一节契约与测试。

---

## 9.22 校验三：`DiscardCountBefore == MovedCardCount`

当前成功 shuffle 会把整个 Discard pile 移到 Draw pile：

```text
MovedCardCount = DiscardCountBefore
```

放一个：

```text
Equal(Integer)
```

连接：

```text
DiscardCountBefore
==
MovedCardCount
```

得到：

```text
MovedAllDiscard
```

---

## 9.23 校验四：`DiscardCountAfter == 0`

当前 commit 完成后：

```text
DiscardPile.Reset()
```

所以：

```text
DiscardCountAfter == 0
```

得到：

```text
DiscardBecameEmpty
```

---

## 9.24 校验五：Draw After 与 Moved 数量一致

当前：

```text
DrawCountBefore = 0
```

且整个 Discard 进入 Draw。

因此应满足：

```text
DrawCountAfter == MovedCardCount
```

得到：

```text
DrawReceivedMovedCards
```

如果以后 producer 支持 DrawBefore 非零，此校验也要随着 producer contract 更新；当前版本保持严格。

---

## 9.25 校验六：总牌数守恒

额外做一次通用一致性检查：

```text
BeforeTotal
= DrawCountBefore + DiscardCountBefore
```

```text
AfterTotal
= DrawCountAfter + DiscardCountAfter
```

比较：

```text
BeforeTotal == AfterTotal
```

得到：

```text
PileTotalPreserved
```

这个检查不依赖 shuffle 的具体动画。

---

## 9.26 校验七：当前 ViewModel 必须匹配 Before

在 Blueprint 接管这条 Record 之前，当前正式历史 ViewModel 应仍处于：

```text
DrawCountBefore
DiscardCountBefore
```

所以先：

```text
IsValid(ViewModel)
```

再比较：

```text
ViewModel.DrawCount == DrawCountBefore
```

和：

```text
ViewModel.DiscardCount == DiscardCountBefore
```

组合为：

```text
HistoricalPileCountsMatch
```

如果不匹配：

```text
Return false
```

不要把 ViewModel 强制改成 Record.Before。

---

## 9.27 组合最终 `CanPlayDeckShuffled`

将前面布尔条件组合：

```text
HasMovedCards
AND CountsNonNegative
AND DrawWasEmpty
AND MovedAllDiscard
AND DiscardBecameEmpty
AND DrawReceivedMovedCards
AND PileTotalPreserved
AND HistoricalPileCountsMatch
```

得到：

```text
CanPlayDeckShuffled
```

Blueprint 中如果 `AND Boolean` pin 太多，可以分成两级：

```text
PayloadShapeValid
HistoricalStateValid
```

最后：

```text
PayloadShapeValid AND HistoricalStateValid
```

不要为了视觉整齐把这些结果 Promote 成 Blueprint member。

---

## 9.28 Router False 必须 `Return false`

从：

```text
Branch(CanPlayDeckShuffled).False
```

接：

```text
Return false
```

不允许：

```text
自己修正 After count
忽略 Before mismatch
把错误 Record 当正常 shuffle 播放
```

`Return false` 会让 Controller 使用已有 immediate fallback/reconcile 机制。

---

## 9.29 Router True 调 Event

`Branch.True`：

```text
PlayDeckShuffledPresentation
```

参数：

```text
DeckShuffled
← Record.DeckShuffled

Token
← BeginPresentationRecordPlayback Function Entry.Token
```

Token 必须原样传递。

不要重新 Make 一个 Playback Token。

---

## 9.30 调用 Event 后 `Return true`

Event 已经：

```text
保存 Token
设置 Active Type
写入 frozen After visual
启动 Timer
```

所以 Router 必须：

```text
PlayDeckShuffledPresentation
↓
Return true
```

不能 Event 已启动后再 `Return false`。

---

## 9.31 Router 最终结构

最终可抽象为：

```text
Switch.DeckShuffled
↓
Break DeckShuffled Payload
↓
IsValid(ViewModel)
↓
验证：
- MovedCardCount > 0
- 所有 count >= 0
- DrawCountBefore == 0
- DiscardCountBefore == MovedCardCount
- DrawCountAfter == MovedCardCount
- DiscardCountAfter == 0
- BeforeTotal == AfterTotal
- ViewModel.DrawCount == DrawCountBefore
- ViewModel.DiscardCount == DiscardCountBefore
↓
Branch(CanPlayDeckShuffled)

├ false
│  → Return false
│
└ true
   → PlayDeckShuffledPresentation(
       Record.DeckShuffled,
       Token
     )
   → Return true
```

---

# 第九节 D：Finish 生命周期

## 9.32 给 `FinishPresentationRecord` 增加 `DeckShuffled` case

打开：

```text
FinishPresentationRecord
```

找到：

```text
Switch on ActivePresentationType
```

为：

```text
DeckShuffled
```

接到统一：

```text
NotifyPresentationRecordFinished
```

本条 Finish 不需要 Remove Widget，也不需要恢复 pile count。

---

## 9.33 正常 Finish 不恢复 Before

假设真实 Record：

```text
Draw 0
Discard 5
→ shuffle
Draw 5
Discard 0
```

active playback：

```text
HUD transient = Draw 5 / Discard 0
ViewModel historical = Draw 0 / Discard 5
```

正常 Finish 必须：

```text
保持 Draw 5 / Discard 0
→ Notify
→ reducer formal = Draw 5 / Discard 0
→ normal HUD refresh
→ 仍 Draw 5 / Discard 0
```

正确序列：

```text
0/5 → 5/0 → 5/0
```

禁止：

```text
0/5 → 5/0 → 0/5 → 5/0
```

因此 Finish 中不要写：

```text
DrawPileCountText = DrawCountBefore
DiscardPileCountText = DiscardCountBefore
```

---

## 9.34 继续使用公共 Notify 顺序

公共完成逻辑继续保持：

```text
ClearAndInvalidateTimerByHandle(ActivePresentationTimer)
↓
NotifyPresentationFinished(ActivePresentationToken)
↓
ActivePresentationType = None
↓
ActivePresentationToken = default
```

不要在 DeckShuffled case 里提前清 Token。

exact token completion 仍然是 reducer 推进的唯一正常入口。

---

# 第九节 E：Cancel / Reconcile

## 9.35 为什么 DeckShuffled Cancel 必须恢复 Before 历史视觉

DeckShuffled active playback 会直接把正式 pile count Text 临时覆盖成 After。

例如：

```text
历史 ViewModel：Draw 0 / Discard 5
transient：Draw 5 / Discard 0
```

如果 0.5 秒内发生正式 Cancel，而 Cancel 只清 Timer/Token：

```text
HUD 会残留 Draw 5 / Discard 0
```

这属于未完成 Record 的未来视觉泄漏。

所以必须 restore。

---

## 9.36 Cancel restore 使用当前历史 `ViewModel`

和 Status、Energy 的策略一致：

```text
Cancel
= 返回当前已经完成的历史 ViewModel
```

因此不要缓存：

```text
ActiveDrawCountBefore
ActiveDiscardCountBefore
```

本节直接使用：

```text
ViewModel.DrawCount
ViewModel.DiscardCount
```

恢复正式 UI。

---

## 9.37 在公共 Cancel Sequence 增加 DeckShuffled restore

如果前面 Status / Energy 已经使用 `Sequence` 做类型化恢复，则继续新增一个 pin。

结构示意：

```text
Cancel Presentation Record Playback
↓
Clear ActivePresentationTimer
↓
Card / Damage 等现有 cleanup
↓
Sequence

Then 0 → StatusChanged restore
Then 1 → EnergyChanged restore
Then 2 → DeckShuffled restore
Then 3 → 公共尾部清理
```

如果你当前实际 Sequence pin 顺序不同，不必为了和文档编号一致重排全部节点。

唯一硬性要求：

```text
DeckShuffled restore
必须发生在 ActivePresentationType 被清成 None 之前
```

---

## 9.38 判断当前 Cancel 是否属于 DeckShuffled

从：

```text
Get ActivePresentationType
```

比较：

```text
== DeckShuffled
```

得到：

```text
IsDeckShuffledCancel
```

接：

```text
Branch
```

False：

```text
不做任何 pile restore
```

True：

```text
检查 ViewModel IsValid
```

---

## 9.39 ViewModel 有效时恢复两个 count

True 路径：

```text
IsValid(ViewModel)
```

Valid：

```text
ViewModel.DrawCount
→ 正常 Draw count 格式
→ DrawPileCountText.SetText
```

然后：

```text
ViewModel.DiscardCount
→ 正常 Discard count 格式
→ DiscardPileCountText.SetText
```

不要恢复 `MovedCardCount`，它不是 HUD state。

---

## 9.40 ViewModel 无效时不要猜

如果 Cancel 时：

```text
ViewModel invalid
```

不要使用：

```text
0 / 0
默认 deck size
Record.Before
Gameplay DeckRuntime
```

去“修复” UI。

只让公共 ownership cleanup 继续。

Controller / Widget lost 路径会负责更高层 reconcile。

---

## 9.41 Cancel 仍然绝对不能 Notify

检查整个 Cancel graph，不允许新增：

```text
NotifyPresentationFinished
NotifyPresentationRecordFinished
FinishPresentationRecord
```

正确：

```text
Clear Timer
→ restore historical visual
→ clear transient references/type/token
→ no Notify
```

---

# 第九节 F：与 `CardZoneChanged` 的边界

## 9.42 DeckShuffled 不生成逐卡 `DiscardPile → DrawPile` 视觉

当前 shuffle 是一个聚合事实：

```text
DeckShuffled
MovedCardCount=N
DiscardCount N→0
DrawCount 0→N
```

本节不要人为拆成：

```text
N 条 CardZoneChanged(DiscardPile → DrawPile)
```

因为当前 producer 没有这些 Record。

这样做会制造不存在的 Presentation history。

---

## 9.43 `DeckShuffled` 不创建 Hand 卡牌

在：

```text
DeckShuffled active
```

期间 Hand 仍保持历史状态。

下一张卡只有在下一条：

```text
CardZoneChanged DrawPile → Hand
```

开始时，才使用第八节的 transient draw card 视觉。

因此 `PlayDeckShuffledPresentation` 中不要调用：

```text
Make Presentation Card View
Create WBP_BattleCard
AddChildToHorizontalBox
```

---

## 9.44 Shuffle 后的 Draw Record 使用已经推进的 formal pile history

正确顺序：

```text
历史：Draw 0 / Discard N
↓
DeckShuffled active：视觉 Draw N / Discard 0
↓
DeckShuffled Notify
↓
reducer：ViewModel Draw N / Discard 0
↓
下一条 CardZoneChanged DrawPile→Hand
```

所以第八节 Draw Router 中的：

```text
ViewModel.DrawCount > 0
```

此时应该成立。

如果不成立，优先检查上一条 DeckShuffled 是否 exact-token 完成，而不是放宽 Draw Router。

---

## 9.45 Draw Record active 时不要再次应用 shuffle 数量

下一条 Draw active 只表现：

```text
新卡进入 Hand
```

按照第八节设计，pile 数字仍保持上一条 formal shuffle 后的：

```text
Draw N / Discard 0
```

直到 Draw Notify 后 reducer 才正式成为：

```text
Draw N-1 / Discard 0
```

所以整个可见序列应该是：

```text
0/N
→ N/0            // DeckShuffled active + formal
→ N/0 + 新卡出现 // Draw CardZoneChanged active
→ N-1/0          // Draw reducer formal
```

不要出现：

```text
0/N → N/0 → 0/N → N-1/0
```

也不要：

```text
0/N → N-1/0
```

跳过 shuffle 的独立视觉事实。

---

# 第九节 G：Compile + Save

## 9.46 Compile 前静态检查

先检查 Event：

```text
PlayDeckShuffledPresentation
├ DeckShuffled struct 输入
├ Token struct 输入
├ ActivePresentationToken = Token
├ ActivePresentationType = DeckShuffled
├ Draw visual = DrawCountAfter
├ Discard visual = DiscardCountAfter
└ StartPresentationFinishTimer
```

Router：

```text
DeckShuffled
→ payload invariant validation
→ ViewModel Before match
→ true: Event + Return true
→ false: Return false
```

Finish：

```text
DeckShuffled
→ NotifyPresentationRecordFinished
```

Cancel：

```text
ActiveType == DeckShuffled
→ ViewModel valid
→ DrawText = ViewModel.DrawCount
→ DiscardText = ViewModel.DiscardCount
→ no Notify
```

---

## 9.47 Compile 顺序

本节只需要修改：

```text
WBP_BattleHUD
```

操作：

```text
1. Compile
2. Compiler Results 确认 0 Errors
3. Save
```

如果找不到：

```text
Break Deck Shuffled Presentation Payload
```

先确认 C++ 当前 `PresentationTypes.h` 已经被 Editor 加载，并且 `FDeckShuffledPresentationPayload` 是 BlueprintType。

不要用五个普通 Integer 自己替代正式 Payload。

---

## 9.48 静态验收清单

```text
[ ] 复用正式 Draw pile count 控件
[ ] 复用正式 Discard pile count 控件
[ ] 没有创建第二套 pile UI

[ ] PlayDeckShuffledPresentation 已创建
[ ] 输入 DeckShuffled 类型正确
[ ] 输入 Token 类型正确
[ ] ActivePresentationToken=Token
[ ] ActivePresentationType=DeckShuffled
[ ] Break Payload 五字段完整
[ ] Draw visual 直接使用 DrawCountAfter
[ ] Discard visual 直接使用 DiscardCountAfter
[ ] 没有用 Before + Moved 重算最终显示
[ ] 没有访问 DeckRuntime / UCardInstance
[ ] 没有修改 ViewModel
[ ] Event 最后 StartPresentationFinishTimer

[ ] Router MovedCardCount>0
[ ] Router 所有 count 非负
[ ] Router DrawCountBefore==0
[ ] Router DiscardBefore==Moved
[ ] Router DrawAfter==Moved
[ ] Router DiscardAfter==0
[ ] Router BeforeTotal==AfterTotal
[ ] Router ViewModel.DrawCount==DrawBefore
[ ] Router ViewModel.DiscardCount==DiscardBefore
[ ] invalid → Return false
[ ] valid → Event + Return true

[ ] Finish 不恢复 Before
[ ] Finish 统一 exact-token Notify
[ ] Cancel 恢复历史 ViewModel.DrawCount
[ ] Cancel 恢复历史 ViewModel.DiscardCount
[ ] Cancel 不 Notify

[ ] DeckShuffled 不创建 Hand card
[ ] DeckShuffled 不伪造 Discard→Draw CardZoneChanged
[ ] WBP_BattleHUD Compile 0 Errors
[ ] WBP_BattleHUD 已 Save
```

---

# 第九节 PIE 验收部分

## 9.49 必须使用真实 shuffle producer

PIE 不允许：

```text
手工 Make FDeckShuffledPresentationPayload
直接调用 PlayDeckShuffledPresentation
```

必须走完整真实链：

```text
DeckRuntime
→ DrawCardAction 发现 Draw 为空
→ ShuffleDeckAction
→ committed DeckShuffled Record
→ Controller
→ Blueprint Router
→ async visual
→ exact-token Notify
→ reducer
→ Retry Draw
→ CardZoneChanged DrawPile→Hand
```

---

## 9.50 准备可稳定触发 shuffle 的局面

目标历史状态：

```text
DrawCount = 0
DiscardCount = N
N > 0
Hand 未满
```

然后触发一次真实 draw 请求。

可以通过正常打牌/弃牌/回合流程把 Draw pile 消耗完；不要为了验收直接改 ViewModel 数字。

如果项目当前有正式测试入口可以快速构造该局面，可以使用，但仍必须让最终 shuffle 由真实 `ShuffleDeckAction` commit。

---

## 9.51 捕获真实 DeckShuffled Record

记录实际：

```text
MovedCardCount       = M
DrawCountBefore      = DB
DrawCountAfter       = DA
DiscardCountBefore   = CB
DiscardCountAfter    = CA
```

当前 producer 正常应观察到：

```text
M > 0
DB = 0
CB = M
DA = M
CA = 0
DB + CB = DA + CA
```

正式 validation log 只记录实际数值，不要把示例 N 当固定值。

---

## 9.52 验证 Router 真正接管

合法 Record 到达：

```text
Switch.DeckShuffled
↓
PayloadShapeValid=true
HistoricalPileCountsMatch=true
↓
PlayDeckShuffledPresentation
↓
Return true
```

如果仍走 fallback，依次检查：

```text
1. Switch.DeckShuffled 是否仍连旧 Return false
2. ViewModel 是否有效
3. ViewModel.DrawCount 是否等于 DrawCountBefore
4. ViewModel.DiscardCount 是否等于 DiscardCountBefore
5. MovedCardCount 是否 > 0
6. 五个 invariant 是否都成立
7. Event 是否实际启动 Timer
8. Event 后是否 Return true
```

---

## 9.53 验证 shuffle active visual

假设真实历史：

```text
Draw 0
Discard 4
```

Record：

```text
Moved=4
Draw 0→4
Discard 4→0
```

播放开始后应该立即看到：

```text
Draw 4
Discard 0
```

此时：

```text
ViewModel historical
仍可能是 Draw 0 / Discard 4
```

这是正确的 transient override。

---

## 9.54 active window 中 Hand 不应提前变化

在 DeckShuffled 这 0.5 秒中确认：

```text
Hand 没有因为 shuffle 本身新增下一张牌
```

正确：

```text
pile count 已经显示 After
Hand 仍是旧 Hand
```

如果新卡与 shuffle 同时出现，检查是否误把 Draw 的 CardZoneChanged 视觉塞进 `PlayDeckShuffledPresentation`。

---

## 9.55 exact-token completion

等待 Timer：

```text
StartPresentationFinishTimer
→ FinishPresentationRecord
→ DeckShuffled case
→ NotifyPresentationRecordFinished
→ NotifyPresentationFinished(ActivePresentationToken)
```

确认：

```text
reducer 正式推进 DrawCountAfter / DiscardCountAfter
```

随后 HUD 正常 refresh 后数字仍保持 After。

不能闪回 Before。

---

## 9.56 验证下一条 Retry Draw 顺序

DeckShuffled 正常完成后，下一条应是：

```text
CardZoneChanged
FromZone = DrawPile
ToZone   = Hand
```

这时第八节 Draw playback 才创建 frozen card transient。

验证：

```text
DeckShuffled exact-token completion
发生在
DrawPile→Hand playback 开始之前
```

不要只看肉眼结果，要确认 Record / Token 顺序。

---

## 9.57 验证完整 pile 数字序列

假设：

```text
Shuffle 前：Draw 0 / Discard 4
```

期望：

```text
阶段 1：历史
Draw 0 / Discard 4

阶段 2：DeckShuffled active
Draw 4 / Discard 0

阶段 3：DeckShuffled formal
Draw 4 / Discard 0

阶段 4：DrawPile→Hand active
Draw 4 / Discard 0
Hand transient +1

阶段 5：Draw reducer formal
Draw 3 / Discard 0
Hand formal +1
```

重点检查没有：

```text
0/4 → 4/0 → 0/4 → 4/0
```

也没有 pile 数字在 Draw active 一开始就提前变成 3。

---

## 9.58 验证 `MovedCardCount` 不产生重复视觉事实

当前 minimal UI 不单独显示：

```text
+4 cards
```

是允许的。

本节可见事实已经由：

```text
DrawCountAfter
DiscardCountAfter
```

明确表达。

重点确认没有额外 N 次：

```text
Discard→Draw zone animation
```

也没有额外修改 Hand。

---

## 9.59 no-op / skipped shuffle 回归

至少理解并尽可能观察两种情况：

```text
A. DiscardPile empty
B. DrawPile non-empty
```

当前 `ShuffleDiscardIntoDrawPileCommit` 应不 commit，因此正式 Record 流中不应出现 `DeckShuffled`。

如果异常出现：

```text
MovedCardCount=0
```

Router 应：

```text
Return false
```

但长期修复仍应回 producer，不能把 0-card shuffle 当合法 Presentation。

---

## 9.60 Cancel PIE 抽查（若有正式入口）

如果项目当前能稳定通过正式入口触发 Cancel：

```text
历史：Draw 0 / Discard N
↓
DeckShuffled active：Draw N / Discard 0
↓
0.5 秒内 Cancel
```

预期：

```text
Timer 清除
→ ActiveType == DeckShuffled
→ ViewModel valid
→ Draw text 恢复 ViewModel.DrawCount = 0
→ Discard text 恢复 ViewModel.DiscardCount = N
→ 不 Notify
```

如果没有可靠的正式 Cancel 入口，不要在蓝图里人工调用 Cancel Event 伪造证据。

把 runtime Cancel 证明留到后面的全局 Cancel/Reconcile 章节。

---

## 9.61 如果出现 `After → Before → After` 闪回

按顺序排查：

```text
1. Finish.DeckShuffled 是否恢复了 Before
2. Notify 前是否主动调用了整套 HUD refresh
3. 是否在 Timer 完成前读取 ViewModel.Before 重画 pile count
4. Notify / reducer 顺序是否被破坏
```

正常 Finish 对 pile visual 不做 restore。

---

## 9.62 如果 Shuffle 完成后 Draw Router 报 `ViewModel.DrawCount <= 0`

按顺序检查：

```text
1. DeckShuffled Router 是否 Return true
2. StartPresentationFinishTimer 是否执行
3. Finish.DeckShuffled 是否进入 Notify
4. Active Token 是否 exact
5. Controller ApplyRecordToWorkingSnapshot 是否成功
6. ViewModel refresh 后 DrawCount 是否已经等于 DrawCountAfter
7. 下一条 Draw 是否真的在上一条完成后才开始
```

不要直接删除第八节的：

```text
ViewModel.DrawCount > 0
```

校验。

那会掩盖跨 Record reducer 没推进的问题。

---

## 9.63 如果 pile 数字变了但流程卡住

检查：

```text
1. Event 最后是否真的 StartPresentationFinishTimer
2. Timer handle 是否被其他路径清掉
3. Finish switch 是否有 DeckShuffled case
4. DeckShuffled case 是否进入统一 Notify
5. Notify 前 ActivePresentationToken 是否仍有效
6. Router 是否 Return true 但 Event 中途断线
```

“数字正确”不等于 async ownership 正确。

---

## 9.64 正式 PIE 验收清单

```text
[ ] 使用真实 producer 构造 DrawPile empty + DiscardPile non-empty
[ ] 实际产生 DeckShuffled Record
[ ] MovedCardCount > 0
[ ] DrawCountBefore == 0
[ ] DiscardCountBefore == MovedCardCount
[ ] DrawCountAfter == MovedCardCount
[ ] DiscardCountAfter == 0
[ ] 总牌数守恒

[ ] Router Historical counts 与 ViewModel Before 匹配
[ ] Router 正式 Return true
[ ] ActivePresentationToken 使用当前 Token
[ ] ActivePresentationType=DeckShuffled

[ ] active visual 直接显示 DrawCountAfter
[ ] active visual 直接显示 DiscardCountAfter
[ ] active 不修改 Hand
[ ] active 不查询 mutable DeckRuntime
[ ] active 不修改 ViewModel
[ ] active 不伪造逐卡 Discard→Draw Record

[ ] 0.5s async 正常
[ ] exact-token completion 正常
[ ] reducer 后 ViewModel Draw/Discard == After
[ ] HUD formal refresh 后仍保持 After
[ ] 无 After→Before→After 闪回

[ ] 下一条 Retry Draw 在 shuffle 完成后开始
[ ] 下一条 Record 是 DrawPile→Hand（实际流程存在时）
[ ] Draw active 不重复 shuffle 事实
[ ] Draw reducer 后 DrawCount 再 -1
[ ] 最终 Hand / Draw / Discard 与 ViewModel 一致
[ ] 最终不挂 Resolving

[ ] skipped/no-op shuffle 不形成正常 visible Record
[ ] 若执行 Cancel：恢复历史 ViewModel counts
[ ] Cancel 不 Notify
```

---

## 9.65 正式验证日志模板

真实 PIE 通过后，再写入：

```text
docs/UIA2EBlueprintValidationLog.md
```

建议格式：

```text
DeckShuffled PIE
SourceFlow               = <实际触发流程>
MovedCardCount           = <M>
DrawCountBefore          = <DB>
DrawCountAfter           = <DA>
DiscardCountBefore       = <CB>
DiscardCountAfter        = <CA>
HistoricalCountsMatched  = Yes
TotalCountPreserved      = Yes
FrozenAfterShown         = Yes
HandChangedDuringShuffle = No
ExactTokenCompleted      = Yes
FlashbackObserved        = No
FinalViewModelDraw       = <DA>
FinalViewModelDiscard    = <CA>
RetryDrawObserved        = Yes/No
ReturnedToIdle           = Yes
Result                   = PASS

DeckShuffled -> DrawPileToHand Sequence
ShuffleCompletedBeforeDraw = Yes
DrawRuntimeId               = <实际值或 Not Observed>
PileCountChangedEarly       = No
FinalDrawCount              = <实际值>
FinalHandMatchesViewModel   = Yes
Result                      = PASS
```

没有实际观察到的字段写：

```text
Not Observed
```

不要编造。

---

## 9.66 本节完成后的 A2E 状态

只有 owner 实际确认 PIE 通过后才允许：

```text
DeckShuffled = VALIDATED
```

此时当前已完成集合应包括前面已经实际验证的：

```text
CardPlayed
Damage
BlockChanged
StatusChanged 全生命周期
EnergyChanged
CardZoneChanged current producer set
DeckShuffled
```

未实际验收的项不能因为写进本文而自动升级状态。

整个 UI-A2E 仍：

```text
PARTIAL
```

---

## 9.67 本节完成判定

如果只完成 Blueprint 接线、Compile + Save，没有真实 shuffle PIE：

```text
DeckShuffled = WIRED / NOT PIE VALIDATED
```

如果真实 Record 能产生，但出现：

```text
payload invariant 不成立
Before 与历史 ViewModel 不匹配
shuffle/draw 顺序错误
重复 CardZoneChanged shuffle 事实
闪回
Token 不完成
```

则：

```text
DeckShuffled = NOT VALIDATED
```

先修复对应问题，不提前进入 Seal。

---

## 9.68 下一节

下一节开始处理三个 terminal Record：

```text
Victory
Defeat
ResolutionFault
```

下一节会详细记录：

```text
如何复用当前 Overlay_Terminal
如何让 terminal visual 只由 terminal Record 启动
如何区分 Victory / Defeat / ResolutionFault
为什么 PresentationUnavailable 不能显示 ResolutionFault
如何接 Router / Active Token / Timer / Finish / Cancel
如何保证 lethal 顺序：
CardPlayed
→ Damage
→ followups
→ CardZoneChanged(PlayArea→Destination)
→ Victory
如何做 Defeat 与 ResolutionFault 的真实 PIE
如何避免 terminal 视觉提前由 Final ViewModel 抢跑
```

---

# 第十节：实现并验收 Terminal Blueprint Playback（`Victory / Defeat / ResolutionFault`）

## 10.1 本节目标与进入条件

只有第九节 `DeckShuffled` 已完成结构接线，并在真实 shuffle 流程中完成对应 PIE 验收后，才进入本节。

本节一次完成三种 terminal Record：

```text
Victory
Defeat
ResolutionFault
```

但三者只是在同一施工阶段完成，**语义仍必须严格分离**。

本节结束后，只有 owner 对三类真实流程分别完成 PIE 验收，才允许记录：

```text
Victory         VALIDATED
Defeat          VALIDATED
ResolutionFault VALIDATED
```

本节还必须单独证明：

```text
PresentationUnavailable
!=
ResolutionFault
```

Presentation 层自身失效、超时、collapse/reconcile、Widget lost 等问题不能伪装成 Gameplay `ResolutionFault`。

---

## 10.2 当前 terminal Record 的冻结 Payload

### Victory / Defeat 共用 `FTerminalPresentationPayload`

当前字段：

```text
WinnerPresentationId
DefeatedPresentationId
```

### ResolutionFault 使用 `FResolutionFaultPresentationPayload`

当前字段：

```text
Reason
ExecutedActionCount
LastActionName
```

Blueprint active playback 只能使用这些冻结字段和当前 presentation ViewModel 做历史一致性验证。

禁止查询：

```text
ACombatant live object
BattleManager 当前实时 BattleState
Gameplay ActionQueue 当前内部状态
UCardInstance / UStatusInstance
```

也禁止 Blueprint 自己推导：

```text
HP <= 0
→ 我猜 Victory
```

Terminal 类型已经由 committed Record 明确给出。

---

## 10.3 Controller 已经锁定 terminal 必须位于 Envelope 最后

当前 Controller 的 terminal envelope 校验要求：

```text
一个 Envelope 最多存在一个 terminal Record
```

并且如果存在：

```text
terminal Record 必须是 Envelope 最后一条 Record
```

因此 Blueprint 不负责重新排序 terminal。

尤其是 lethal card：

```text
CardPlayed
→ Damage
→ followups
→ CardZoneChanged(PlayArea → Destination)
→ Victory
```

前面的 Record 必须逐条播放并完成，最后才轮到 Victory。

Blueprint 不得因为看到 Enemy 已经 bDead，就提前显示胜利层。

---

## 10.4 Controller 对 Victory 的正式 preflight 语义

当前 Controller 要求 Victory 至少满足：

```text
WinnerPresentationId   非 None
DefeatedPresentationId 非 None
Winner != Defeated

Winner   == Player.PresentationId
Defeated == Enemy.PresentationId

当前 WorkingSnapshot.Enemy.bDead == true
FinalSnapshot.Enemy.bDead == true

FinalSnapshot.BattleState == Victory
FinalSnapshot.Outcome     == Victory
```

Blueprint 不需要重复访问 Gameplay 做第二套权威判定，但可以在 Router 中对当前 presentation ViewModel 做同方向的最小防御检查。

---

## 10.5 Controller 对 Defeat 的正式 preflight 语义

Defeat 对称要求：

```text
Winner   == Enemy.PresentationId
Defeated == Player.PresentationId

当前 WorkingSnapshot.Player.bDead == true
FinalSnapshot.Player.bDead == true

FinalSnapshot.BattleState == Defeat
FinalSnapshot.Outcome     == Defeat
```

因此 Blueprint Defeat Router 不要把 Winner/Defeated 方向写反。

---

## 10.6 Controller 对 ResolutionFault 的正式 preflight 语义

ResolutionFault 至少要求：

```text
Reason 非空
ExecutedActionCount >= 0
FinalSnapshot.BattleState == ResolutionFaulted
FinalSnapshot.Outcome     == ResolutionFaulted
```

它表达的是：

```text
Gameplay / framework resolution 本身进入 fault terminal
```

不是：

```text
Presentation Widget 播放失败
Blueprint 节点没有找到
Timer 超时
PresentationUnavailable
```

这条边界在本节全程保持不变。

---

# 第十节 A：先审计并复用现有 Terminal UI

## 10.7 现有正式 Overlay 必须复用

当前 `WBP_BattleHUD` Designer 已经存在：

```text
Overlay_Terminal
```

当前初始状态：

```text
Collapsed
```

并且主刷新：

```text
Event Battle HUD View Model Changed
```

已经存在：

```text
Victory / Defeat / ResolutionFaulted
```

的终局显示逻辑。

所以本节**不要新增第二套全屏 terminal overlay**。

禁止：

```text
Overlay_Terminal_A2E
Overlay_VictoryTransient
Overlay_DefeatTransient
Overlay_FaultTransient
```

统一复用：

```text
Overlay_Terminal
```

---

## 10.8 先在现有 ViewModel refresh 中找到 terminal 文本控件

进入：

```text
WBP_BattleHUD
→ Event Battle HUD View Model Changed
```

沿：

```text
ViewModel.Outcome
```

或当前 terminal 分支向右追踪。

找到当前正式终局 UI 用于显示标题/说明的 TextBlock。

因为当前保存快照只明确锁定了 `Overlay_Terminal`，没有可靠锁定其内部所有 TextBlock 的实际变量名，所以本文使用两个逻辑别名：

```text
TerminalTitleText
TerminalDetailText
```

含义：

```text
TerminalTitleText
= 当前正式 terminal surface 用来显示 Victory / Defeat / Fault 主标题的 TextBlock

TerminalDetailText
= 如果当前正式 surface 已经存在，用来显示额外说明的 TextBlock
```

如果当前 UI 只有一个 TextBlock：

```text
只使用 TerminalTitleText
```

不要为了文档强行新增一个 Detail Text。

---

## 10.9 记录当前正式 Outcome 到 UI 的映射

先确认当前正常 HUD 在 ViewModel 已经 terminal 时显示什么。

至少记录：

```text
Outcome=Victory
→ Overlay_Terminal Visible
→ 标题文本 = 当前正式胜利文案

Outcome=Defeat
→ Overlay_Terminal Visible
→ 标题文本 = 当前正式失败文案

Outcome=ResolutionFaulted
→ Overlay_Terminal Visible
→ 标题文本 = 当前正式故障文案

Outcome=None
→ Overlay_Terminal Collapsed
```

本节 transient terminal 视觉必须尽可能与这套正式文案一致。

目的：

```text
Record active transient surface
→ Notify
→ formal ViewModel terminal refresh
```

前后看起来是同一个 terminal surface，而不是两套不同 UI。

---

## 10.10 不从 FinalSnapshot 提前刷新 terminal

Blueprint 没有必要、也不应读取 Envelope.FinalSnapshot 来决定 terminal。

本节只从当前 Record type 进入：

```text
Switch.Victory
Switch.Defeat
Switch.ResolutionFault
```

因此不要增加：

```text
Get FinalSnapshot.Outcome
→ 提前 Set Overlay_Terminal Visible
```

Terminal 的开始时机就是：

```text
Controller 正式调到这条 terminal Record
```

---

# 第十节 B：建议抽出 `RefreshTerminalSurfaceFromViewModel`

## 10.11 为什么建议抽 helper

后面 Cancel 需要恢复：

```text
当前已经完成历史 ViewModel 对应的 terminal surface
```

如果 Cancel 直接写：

```text
Overlay_Terminal = Collapsed
```

在正常“pre-terminal Record 被取消”的情况下通常成立，但架构上不够稳健。

更统一的方式是把主 ViewModel refresh 中现有 terminal 绘制逻辑抽成一个函数：

```text
RefreshTerminalSurfaceFromViewModel
```

这个函数：

```text
只读 ViewModel
只更新 terminal UI
不改 ViewModel
不访问 Gameplay
没有 Delay / Timer
```

---

## 10.12 创建函数

在 `WBP_BattleHUD → Functions` 新建：

```text
RefreshTerminalSurfaceFromViewModel
```

无输入、无输出。

开头：

```text
IsValid(ViewModel)
```

如果 Invalid：

```text
直接 Return
```

不要猜默认 terminal 状态。

---

## 10.13 根据 `ViewModel.Outcome` 刷新正式 terminal surface

读取：

```text
ViewModel.Outcome
```

使用：

```text
Switch on EBattleHUDOutcome
```

### None

```text
Overlay_Terminal.SetVisibility(Collapsed)
```

### Victory

```text
TerminalTitleText = 当前正式 Victory 文案
Overlay_Terminal = Visible
```

### Defeat

```text
TerminalTitleText = 当前正式 Defeat 文案
Overlay_Terminal = Visible
```

### ResolutionFaulted

```text
TerminalTitleText = 当前正式 ResolutionFaulted 文案
Overlay_Terminal = Visible
```

如果有 `TerminalDetailText`，在正式 Outcome refresh 中按当前既有行为处理；不要在 helper 中凭空创建 gameplay fault reason，因为 `FPresentationStateSnapshot` 当前没有保存 fault Reason 字段。

---

## 10.14 把主 ViewModel Changed 的旧 terminal 逻辑替换为 helper 调用

在：

```text
Event Battle HUD View Model Changed
```

找到原本负责终局显示的 Sequence 分支。

保留它在原来的执行位置，只把内部重复节点替换为：

```text
RefreshTerminalSurfaceFromViewModel
```

这样正式 ViewModel 更新的行为不变，只是 terminal 绘制逻辑集中到一个函数。

不要改变其他 Sequence pin 顺序。

---

# 第十节 C：实现 Victory transient playback

## 10.15 新建 `PlayVictoryPresentation`

在 Event Graph 新建 Custom Event：

```text
PlayVictoryPresentation
```

输入：

```text
Terminal : FTerminalPresentationPayload
Token    : FPresentationPlaybackToken
```

---

## 10.16 Victory Event 保存 Token / Type

执行链：

```text
PlayVictoryPresentation
↓
Set ActivePresentationToken = Token
↓
Set ActivePresentationType = Victory
```

不要在显示 overlay 后再设置 ownership。

---

## 10.17 Victory 使用与正式 Outcome 一致的文案

`FTerminalPresentationPayload` 没有“标题字符串”。

因此 Victory 的标题属于 UI presentation formatting，可以使用当前正式 terminal surface 已经使用的固定本地化 FText，例如：

```text
Victory
```

或项目当前已经存在的中文/英文胜利文案。

关键要求：

```text
transient Victory 标题
== formal Outcome=Victory 标题
```

不要根据 Enemy 名称拼出一个新的临时文案，除非当前正式 UI 本来就这么做。

---

## 10.18 Victory 显示 Overlay

执行：

```text
TerminalTitleText.SetText(VictoryFormalText)
↓
Overlay_Terminal.SetVisibility(Visible)
↓
StartPresentationFinishTimer
```

如果已有 `TerminalDetailText`：

```text
按当前正式 Victory UI 的状态设置
```

不要把 Winner/Defeated ID 直接显示给普通玩家，除非本项目 UI 本来就是调试界面。

---

# 第十节 D：实现 Defeat transient playback

## 10.19 新建 `PlayDefeatPresentation`

输入：

```text
Terminal : FTerminalPresentationPayload
Token    : FPresentationPlaybackToken
```

执行：

```text
PlayDefeatPresentation
↓
ActivePresentationToken = Token
↓
ActivePresentationType = Defeat
↓
TerminalTitleText = 正式 Defeat 文案
↓
Overlay_Terminal = Visible
↓
StartPresentationFinishTimer
```

与 Victory 一样，文案格式应与正式 `Outcome=Defeat` refresh 相同。

---

# 第十节 E：实现 ResolutionFault transient playback

## 10.20 新建 `PlayResolutionFaultPresentation`

输入：

```text
ResolutionFault : FResolutionFaultPresentationPayload
Token           : FPresentationPlaybackToken
```

执行开头：

```text
ActivePresentationToken = Token
↓
ActivePresentationType = ResolutionFault
```

---

## 10.21 ResolutionFault 主标题使用正式故障文案

设置：

```text
TerminalTitleText
= 当前正式 Outcome=ResolutionFaulted 的标题文案
```

然后：

```text
Overlay_Terminal = Visible
```

---

## 10.22 `Reason` 的显示规则

Payload 中冻结了：

```text
Reason : FString
```

如果当前 `Overlay_Terminal` **已经有正式 Detail Text**，可以在 active playback 中：

```text
Reason
→ String To Text
→ TerminalDetailText.SetText
```

这属于直接消费冻结 Record，允许。

如果当前 terminal surface 没有 Detail Text：

```text
不要为了本节强制新增一个复杂 fault panel
```

最低要求仍是：

```text
正确显示 ResolutionFault terminal 类型
```

同时 `Reason` 必须至少用于 Router 合法性校验和调试日志。

特别注意：如果 active 时显示 Reason，而 formal ViewModel refresh 在 Notify 后会清空该 Detail Text，则会产生“详细原因突然消失”的视觉变化。出现这种结构时优先选择：

```text
active 与 formal 使用同一稳定内容
```

不要为了短暂显示更多信息制造 Finish flicker。

---

## 10.23 `ExecutedActionCount / LastActionName` 第一版不要求玩家可见

这两个字段主要用于 fault 诊断：

```text
ExecutedActionCount
LastActionName
```

第一版可以只用于：

```text
Router validation
PIE / validation log
```

除非当前正式 terminal UI 已经存在调试详情区域，否则不要为了 A2E 新增复杂 debug layout。

---

## 10.24 ResolutionFault 最后启动公共 Timer

完整最小 Event：

```text
PlayResolutionFaultPresentation
↓
ActivePresentationToken = Token
↓
ActivePresentationType = ResolutionFault
↓
TerminalTitleText = formal fault title
↓
[可选：TerminalDetailText = frozen Reason]
↓
Overlay_Terminal = Visible
↓
StartPresentationFinishTimer
```

---

# 第十节 F：Victory Router

## 10.25 找到当前 Victory fallback

打开：

```text
BeginPresentationRecordPlayback
```

在 Record Type Switch 中找到：

```text
Victory
```

当前保存版本仍是：

```text
Victory → Return false
```

本节改为显式 validation + async route。

---

## 10.26 Victory Router 先验证 ViewModel

```text
IsValid(ViewModel)
```

Invalid：

```text
Return false
```

Valid：继续。

---

## 10.27 Break `Record.Terminal`

从 Record：

```text
Terminal
```

拖：

```text
Break Terminal Presentation Payload
```

取得：

```text
WinnerPresentationId
DefeatedPresentationId
```

---

## 10.28 Victory identity 校验

比较：

```text
WinnerPresentationId
== ViewModel.Player.PresentationId
```

以及：

```text
DefeatedPresentationId
== ViewModel.Enemy.PresentationId
```

并检查：

```text
WinnerPresentationId != None
DefeatedPresentationId != None
WinnerPresentationId != DefeatedPresentationId
```

---

## 10.29 Victory 当前历史死亡事实校验

因为导致致死的 Damage Record 已经在 Victory 之前完成 reducer，当前 historical ViewModel 应该已经有：

```text
ViewModel.Enemy.bDead == true
```

所以 Router 再检查：

```text
Enemy.bDead
```

这不是 Gameplay live query；它是已经完成的 presentation history。

不要通过：

```text
Enemy.HP <= 0
```

自己重新推导 bDead。

直接用冻结/历史 ViewModel 已发布的 `bDead`。

---

## 10.30 Victory 前历史 Outcome 必须仍为 None

读取：

```text
ViewModel.Outcome
```

检查：

```text
Outcome == None
```

这能防止 Blueprint 对已经 formal-terminal 的 HUD 再启动第二次 terminal async。

---

## 10.31 组合 `CanPlayVictory`

至少：

```text
ViewModel valid
AND Winner == Player
AND Defeated == Enemy
AND IDs non-none/distinct
AND Enemy.bDead
AND ViewModel.Outcome == None
```

False：

```text
Return false
```

True：

```text
PlayVictoryPresentation(Record.Terminal, Token)
→ Return true
```

---

# 第十节 G：Defeat Router

## 10.32 Defeat identity 方向必须与 Victory 对称

`Switch.Defeat`：

```text
IsValid(ViewModel)
↓
Break Record.Terminal
```

校验：

```text
WinnerPresentationId
== ViewModel.Enemy.PresentationId

DefeatedPresentationId
== ViewModel.Player.PresentationId

Winner / Defeated non-none
Winner != Defeated

ViewModel.Player.bDead == true
ViewModel.Outcome == None
```

False：

```text
Return false
```

True：

```text
PlayDefeatPresentation(Record.Terminal, Token)
→ Return true
```

不要复制 Victory 图后忘记交换 Player/Enemy。

---

# 第十节 H：ResolutionFault Router

## 10.33 `Switch.ResolutionFault` 不使用 Terminal payload

ResolutionFault 使用：

```text
Record.ResolutionFault
```

而不是：

```text
Record.Terminal
```

从 struct 拉：

```text
Break Resolution Fault Presentation Payload
```

取得：

```text
Reason
ExecutedActionCount
LastActionName
```

---

## 10.34 检查 `Reason` 非空

对 FString 使用当前 Blueprint 可用的字符串空检查，例如：

```text
Is Empty
→ NOT
```

得到：

```text
HasFaultReason
```

必须：

```text
true
```

如果 Reason 为空：

```text
Return false
```

不要自己填：

```text
"Unknown Error"
```

然后假装 Record 合法。

---

## 10.35 检查 `ExecutedActionCount >= 0`

使用：

```text
Greater or Equal (Integer)
```

```text
ExecutedActionCount >= 0
```

得到：

```text
ExecutedCountValid
```

`LastActionName` 可以是 None，因此不要强制它非 None。

---

## 10.36 检查当前历史还没有 formal terminal Outcome

```text
IsValid(ViewModel)
AND ViewModel.Outcome == None
```

如果 ViewModel 已经是 Victory/Defeat/ResolutionFaulted：

```text
Return false
```

---

## 10.37 ResolutionFault Router 最终结构

```text
Switch.ResolutionFault
↓
IsValid(ViewModel)
↓
Break Record.ResolutionFault
↓
HasFaultReason
AND ExecutedActionCount >= 0
AND ViewModel.Outcome == None
↓
Branch

├ false
│  → Return false
│
└ true
   → PlayResolutionFaultPresentation(
       Record.ResolutionFault,
       Token
     )
   → Return true
```

不要在这里检查：

```text
PresentationUnavailable
```

它不是 ResolutionFault 的“另一种入口”。

---

# 第十节 I：三个 terminal Finish

## 10.38 给 `FinishPresentationRecord` 增加三条 case

在：

```text
Switch on ActivePresentationType
```

增加/连接：

```text
Victory
Defeat
ResolutionFault
```

三条都进入统一：

```text
NotifyPresentationRecordFinished
```

---

## 10.39 正常 terminal Finish 不要隐藏 Overlay

这是本节最关键的生命周期规则之一。

错误：

```text
Victory active
→ Overlay Visible
→ Timer
→ Finish: Overlay Collapsed
→ Notify
→ reducer formal Victory
→ ViewModel refresh: Overlay Visible
```

会形成：

```text
Visible → Collapsed → Visible
```

闪回。

正确：

```text
terminal active
→ Overlay Visible
→ Timer
→ Notify
→ reducer formal terminal
→ ViewModel refresh 继续保持 Overlay Visible
```

所以三类 terminal 的正常 Finish 都不要：

```text
SetVisibility(Collapsed)
```

---

## 10.40 正常 Finish 不把标题恢复成旧文本

同理不要：

```text
TerminalTitleText = 空
TerminalTitleText = previous
```

在 Notify 前做视觉恢复。

transient terminal 与 formal terminal 应连续衔接。

---

## 10.41 Terminal 仍使用 exact Token

公共完成顺序继续：

```text
Clear Timer
→ NotifyPresentationFinished(ActivePresentationToken)
→ 清 ActivePresentationType
→ 清 ActivePresentationToken
```

不要在 Victory/Defeat/Fault case 自己 Make 一个 Token。

---

# 第十节 J：Terminal Cancel / Reconcile

## 10.42 terminal Cancel 不能留下“未来终局”

假设历史仍是：

```text
Outcome=None
```

terminal Record active 后：

```text
Overlay_Terminal = Visible
```

此时若发生正式 Cancel，该 Record 没有正常完成，HUD 必须回到当前历史 ViewModel。

不能把终局层继续留着。

---

## 10.43 在 Cancel Sequence 增加 Terminal restore

在已有：

```text
StatusChanged restore
EnergyChanged restore
DeckShuffled restore
Card transient cleanup
...
```

之外增加 terminal restore 分支。

判断：

```text
ActivePresentationType == Victory
OR ActivePresentationType == Defeat
OR ActivePresentationType == ResolutionFault
```

得到：

```text
IsTerminalPresentationCancel
```

注意必须在：

```text
ActivePresentationType = None
```

之前判断。

---

## 10.44 Terminal Cancel 调 `RefreshTerminalSurfaceFromViewModel`

True：

```text
RefreshTerminalSurfaceFromViewModel
```

正常 active terminal 的历史 ViewModel 应还是：

```text
Outcome=None
```

因此 helper 会：

```text
Overlay_Terminal = Collapsed
```

如果未来某种合法路径下历史 ViewModel 已经 formal terminal，helper 也会按其真实 Outcome 恢复，而不是盲目 Collapse。

---

## 10.45 Cancel 不 Notify

和所有其他 Record 相同：

```text
Cancel
→ 清 Timer
→ reconcile visual
→ 清 ownership
→ 不 Notify
```

禁止 terminal Cancel 调：

```text
NotifyPresentationFinished
FinishPresentationRecord
```

---

# 第十节 K：PresentationUnavailable 与 ResolutionFault 必须彻底分离

## 10.46 PresentationUnavailable 是 Presentation 层 fail-safe

当前 HUD interaction state 中有独立：

```text
PresentationUnavailable
```

它表示 committed presentation 本身不可用或需要 collapse/reconcile。

它不是：

```text
ResolutionFaulted Outcome
```

所以本节绝对不要写：

```text
InteractionState == PresentationUnavailable
→ PlayResolutionFaultPresentation
```

---

## 10.47 Blueprint Router 返回 false 也不能制造 ResolutionFault

例如：

```text
找不到 Widget
Payload visual validation 失败
某个 Record Blueprint 不支持
```

正确行为：

```text
Return false
→ Controller immediate fallback / reconcile
```

不是：

```text
Make ResolutionFault Record
显示 ResolutionFault terminal
```

Gameplay resolution fault 必须来自正式 committed `ResolutionFault` Record。

---

## 10.48 Presentation timeout 也不是 Gameplay ResolutionFault

如果某个 Blueprint async：

```text
Return true
```

但没有按时 Notify，Controller timeout 属于 Presentation playback failure/recovery 范畴。

不要在 timeout/cancel callback 中显示：

```text
ResolutionFault
```

除非当前 Envelope 本身真的包含正式 `ResolutionFault` Record。

---

# 第十节 L：Terminal Energy 与其他 HUD 状态

## 10.49 terminal Event 不修改 Energy

三种 terminal Event 中不要写：

```text
Energy = 0
Energy = MaxEnergy
Energy = default
```

如果 terminal 前已经有独立 EnergyChanged，它应在 terminal Record 前完成。

如果是 lethal card cost，能量事实仍属于：

```text
CardPlayed
```

Terminal 不重复处理。

---

## 10.50 terminal Event 不重画 HP/Block/Status

导致 terminal 的前置 Record 已经负责这些事实。

例如 Victory：

```text
Damage
→ Enemy HPAfter=0
→ reducer formal enemy dead
→ ...
→ Victory
```

Victory 只处理 terminal surface，不再：

```text
Enemy HP = 0
Enemy bDead = true
清 Block
清 Status
```

否则会重复前置 Record 的事实。

---

# 第十节 M：Compile + Save

## 10.51 Compile 前结构检查

确认存在：

```text
RefreshTerminalSurfaceFromViewModel
PlayVictoryPresentation
PlayDefeatPresentation
PlayResolutionFaultPresentation
```

Router：

```text
Victory
Defeat
ResolutionFault
```

都已经从旧 `Return false` 改为明确 validation + Event + true。

Finish：

```text
Victory / Defeat / ResolutionFault
→ Notify
```

Cancel：

```text
terminal active
→ RefreshTerminalSurfaceFromViewModel
→ no Notify
```

---

## 10.52 推荐 Compile 顺序

本节主要修改：

```text
WBP_BattleHUD
```

操作：

```text
1. Compile WBP_BattleHUD
2. Compiler Results = 0 Errors
3. Save
```

如果新增 helper 后旧 EventGraph terminal 节点被断开，先确认：

```text
原 Sequence terminal pin
→ RefreshTerminalSurfaceFromViewModel
```

仍有执行入口。

---

## 10.53 静态验收清单

```text
[ ] 复用 Overlay_Terminal
[ ] 没有新增第二套 terminal overlay
[ ] 已定位正式 terminal title/detail 控件
[ ] transient 文案与 formal Outcome 文案一致

[ ] RefreshTerminalSurfaceFromViewModel 只读 ViewModel
[ ] Outcome=None → Collapsed
[ ] Victory/Defeat/ResolutionFaulted → Visible + 对应正式文案
[ ] 主 ViewModel Changed 仍在原位置调用 terminal refresh

[ ] PlayVictoryPresentation 保存 Token
[ ] ActiveType=Victory
[ ] 显示 Overlay
[ ] 启动公共 Timer

[ ] PlayDefeatPresentation 保存 Token
[ ] ActiveType=Defeat
[ ] 显示 Overlay
[ ] 启动公共 Timer

[ ] PlayResolutionFaultPresentation 保存 Token
[ ] ActiveType=ResolutionFault
[ ] 使用正式 fault 文案
[ ] Reason 若显示则直接来自 frozen payload
[ ] 启动公共 Timer

[ ] Victory Router Winner=Player
[ ] Victory Router Defeated=Enemy
[ ] Victory Router Enemy.bDead=true
[ ] Victory Router Outcome=None

[ ] Defeat Router Winner=Enemy
[ ] Defeat Router Defeated=Player
[ ] Defeat Router Player.bDead=true
[ ] Defeat Router Outcome=None

[ ] ResolutionFault Router Reason 非空
[ ] ExecutedActionCount>=0
[ ] Outcome=None
[ ] LastActionName 不被错误强制非 None

[ ] invalid terminal payload → Return false
[ ] valid route → Event + Return true

[ ] normal Finish 不 Collapse Overlay
[ ] normal Finish 不恢复旧标题
[ ] exact-token Notify

[ ] Cancel terminal → 从历史 ViewModel 恢复 terminal surface
[ ] Cancel 不 Notify

[ ] PresentationUnavailable 不调用 ResolutionFault Event
[ ] Blueprint visual failure 不制造 ResolutionFault
[ ] Terminal 不修改 Energy/HP/Block/Status
[ ] WBP_BattleHUD Compile 0 Errors
[ ] Save 完成
```

---

# 第十节 PIE 验收 A：Victory

## 10.54 必须使用真实 lethal Gameplay 流程

不要：

```text
直接调用 PlayVictoryPresentation
手工 Make Terminal payload
```

必须通过真实战斗把 Enemy 打到死亡，并让 committed presentation 产生 Victory。

优先用一张能够稳定 lethal 的卡牌。

---

## 10.55 捕获 lethal Envelope 的真实 Record 顺序

至少记录：

```text
CardPlayed
→ Damage
→ [实际 followups]
→ CardZoneChanged(PlayArea → Destination)
→ Victory
```

如果中间有 StatusChanged / BlockChanged 等实际 followup，以真实 producer 顺序为准。

硬性要求：

```text
Victory 必须是该 terminal Envelope 最后一条 Record
```

---

## 10.56 Victory 开始前 Enemy 死亡事实必须已经 formal 可见

观察 Victory Record 开始前：

```text
Enemy HP 已经是 lethal Damage.HPAfter
Enemy bDead 已经由前一条 Damage reducer 推进
Played card 已完成 PlayArea→Destination
```

不能出现：

```text
Victory overlay 先出现
然后 Enemy HP 才变 0
```

---

## 10.57 Victory active visual

Victory Router 应：

```text
Winner = Player
Defeated = Enemy
Enemy.bDead = true
Outcome=None
→ Return true
```

然后：

```text
Overlay_Terminal Visible
TerminalTitleText = formal Victory 文案
```

在 0.5 秒 active window 中 historical ViewModel.Outcome 仍可能是 None，这是正确的。

---

## 10.58 Victory exact-token completion

等待：

```text
Timer
→ Finish.Victory
→ Notify exact token
→ terminal reducer
→ ViewModel.Outcome=Victory
→ InteractionState=Terminal（按当前 formal refresh）
```

最终 Overlay 继续保持 Visible。

不应出现：

```text
Visible → Collapsed → Visible
```

---

# 第十节 PIE 验收 B：Defeat

## 10.59 使用真实玩家死亡流程

通过当前正常 EnemyTurn / Damage 路径让 Player 死亡。

记录造成死亡的最后一条 Damage：

```text
HPBefore
HPAfter=0 或实际 lethal after
```

然后确认：

```text
Damage 完成
→ [实际后续可见 facts]
→ Defeat
```

---

## 10.60 Defeat Router 与 visual

确认：

```text
Winner = Enemy
Defeated = Player
Player.bDead = true
Outcome=None
```

进入：

```text
PlayDefeatPresentation
```

显示正式 Defeat terminal surface。

不能复用 Victory 的 Winner/Defeated 判断方向。

---

## 10.61 Defeat 完成后保持 formal terminal

```text
Defeat active Visible
→ Notify
→ ViewModel.Outcome=Defeat
→ formal terminal refresh
→ 仍 Visible
```

无 terminal overlay 闪回，无重复完成。

---

# 第十节 PIE 验收 C：ResolutionFault

## 10.62 必须使用项目已有的真实 ResolutionFault 测试入口

优先使用项目现有 dev/automation 可控 fault 注入路径，而不是在 Blueprint 手工构造 Record。

目标是证明完整链：

```text
Gameplay / framework resolution fault
→ committed ResolutionFault Record
→ Controller
→ Blueprint Router
→ fault terminal visual
→ exact-token Notify
→ formal ResolutionFaulted Outcome
```

---

## 10.63 记录真实 fault payload

至少记录：

```text
Reason
ExecutedActionCount
LastActionName
```

要求：

```text
Reason 非空
ExecutedActionCount >= 0
```

`LastActionName=None` 本身不构成失败。

---

## 10.64 ResolutionFault visual 不得显示 Victory/Defeat 文案

确认：

```text
ActivePresentationType = ResolutionFault
Overlay_Terminal Visible
TerminalTitleText = formal ResolutionFaulted 文案
```

如果有 detail：

```text
Reason 来自 frozen payload
```

不能显示：

```text
Victory
Defeat
Presentation unavailable
```

混合语义。

---

# 第十节 PIE 验收 D：PresentationUnavailable 区分测试

## 10.65 单独触发 Presentation-only failure/recovery 场景

使用项目当前已有、合法的 presentation fail-safe 测试路径，例如：

```text
Presentation unavailable
presentation freeze failure 测试入口
widget lost / presentation ownership loss
```

以当前项目可稳定触发的正式入口为准。

不要为了测试直接 Make ResolutionFault。

---

## 10.66 预期结果

Presentation-only failure 应进入：

```text
PresentationUnavailable
或 Controller collapse/reconcile
```

但：

```text
不得产生 Gameplay ResolutionFault terminal Record
不得调用 PlayResolutionFaultPresentation
不得把 ViewModel.Outcome 写成 ResolutionFaulted
```

这条测试是 A2E Seal 前的重要语义隔离证据。

---

# 第十节 PIE 验收 E：Terminal Cancel / stale callback

## 10.67 如果有正式 Cancel 入口，抽查 terminal active cancel

例如：

```text
historical Outcome=None
↓
Victory transient Overlay Visible
↓
在 0.5 秒内合法 Cancel
```

预期：

```text
Timer 清理
→ RefreshTerminalSurfaceFromViewModel
→ historical Outcome=None
→ Overlay Collapsed
→ no Notify
```

不要让取消的 terminal Record把 ViewModel 推进成 Terminal。

---

## 10.68 stale Token 不能完成新的 terminal

如果当前测试设施能验证 stale callback：

```text
旧 LocalPlaybackGeneration / 旧 Token
→ NotifyPresentationFinished
```

Controller 不应把它当成当前 terminal completion。

Blueprint 只保存和回传当前 `ActivePresentationToken`，不自己绕过 exact-token gate。

如果没有可靠 PIE 入口，本项留到下一节全局 Cancel/Reconcile 中统一验证。

---

## 10.69 Victory 验收清单

```text
[ ] 使用真实 lethal gameplay
[ ] Victory Record 真实产生
[ ] Victory 是 terminal Envelope 最后一条
[ ] Damage / followups / CardZoneChanged 先完成
[ ] Winner=Player
[ ] Defeated=Enemy
[ ] Enemy.bDead 已 formal
[ ] Outcome 在 Victory active 前仍 None
[ ] Router Return true
[ ] Overlay Visible
[ ] 文案正确
[ ] exact-token completion
[ ] formal Outcome=Victory
[ ] Overlay 不闪回
[ ] terminal 只完成一次
```

---

## 10.70 Defeat 验收清单

```text
[ ] 使用真实 Player lethal Damage
[ ] Defeat Record 真实产生
[ ] 致死 Damage 先完成
[ ] Winner=Enemy
[ ] Defeated=Player
[ ] Player.bDead 已 formal
[ ] Router Return true
[ ] Overlay Visible
[ ] Defeat 文案正确
[ ] exact-token completion
[ ] formal Outcome=Defeat
[ ] Overlay 不闪回
```

---

## 10.71 ResolutionFault 验收清单

```text
[ ] 使用真实 framework/gameplay fault 路径
[ ] ResolutionFault Record 真实产生
[ ] Reason 非空
[ ] ExecutedActionCount>=0
[ ] Router Return true
[ ] ActiveType=ResolutionFault
[ ] fault 文案正确
[ ] 没有 Victory/Defeat 混淆
[ ] exact-token completion
[ ] formal Outcome=ResolutionFaulted
[ ] Overlay 不闪回
```

---

## 10.72 PresentationUnavailable 隔离验收清单

```text
[ ] 使用真实 presentation-only fail-safe 路径
[ ] 没有生成 Gameplay ResolutionFault Record
[ ] 没有调用 PlayResolutionFaultPresentation
[ ] 没有错误显示 ResolutionFault terminal
[ ] 进入 PresentationUnavailable / collapse / reconcile
[ ] Gameplay terminal ownership 未被伪造
```

---

## 10.73 正式验证日志模板

实际 PIE 通过后，再把 owner 真正观察到的数据写进：

```text
docs/UIA2EBlueprintValidationLog.md
```

建议：

```text
Victory PIE
WinnerPresentationId       = <实际值>
DefeatedPresentationId     = <实际值>
EnemyDeadBeforeVictory     = Yes
VictoryLastRecord          = Yes
CardZoneCompletedBefore    = Yes
OverlayShown               = Yes
ExactTokenCompleted        = Yes
TerminalFlashbackObserved  = No
FinalOutcome               = Victory
Result                     = PASS

Defeat PIE
WinnerPresentationId       = <实际值>
DefeatedPresentationId     = <实际值>
PlayerDeadBeforeDefeat     = Yes
DefeatLastRecord           = Yes
OverlayShown               = Yes
ExactTokenCompleted        = Yes
TerminalFlashbackObserved  = No
FinalOutcome               = Defeat
Result                     = PASS

ResolutionFault PIE
Reason                     = <实际 Reason>
ExecutedActionCount        = <实际值>
LastActionName             = <实际值或 None>
OverlayShown               = Yes
ExactTokenCompleted        = Yes
FinalOutcome               = ResolutionFaulted
Result                     = PASS

PresentationUnavailable Separation
Trigger                    = <实际路径>
ResolutionFaultRecordMade  = No
ResolutionFaultVisualShown = No
PresentationUnavailable    = Yes/按实际 fail-safe
Result                     = PASS
```

没有观察到的字段写：

```text
Not Observed
```

不要编造。

---

## 10.74 本节完成后的状态

只有三类 terminal PIE 和 PresentationUnavailable 区分证据都通过后，才允许：

```text
Victory         VALIDATED
Defeat          VALIDATED
ResolutionFault VALIDATED
```

此时 UI-A2E 仍然不能直接 Seal。

后面还必须做：

```text
全局 Cancel / Reconcile 审计
完整 A2E End-to-End PIE Acceptance
Input unlock / caught-up 验收
最终文档收口与 Seal
```

---

## 10.75 下一节

下一节进入：

```text
全局 Cancel / Reconcile 收尾
```

将统一审计此前所有直接覆盖 HUD 的 transient playback：

```text
CardPlayed / CardZoneChanged
Damage
BlockChanged
StatusChanged
EnergyChanged
DeckShuffled
Victory / Defeat / ResolutionFault
```

重点记录：

```text
每种 Record Cancel 后应该恢复什么历史 ViewModel 状态
哪些 transient Widget 应 Remove
哪些正式 Widget 只应 restore
为什么 Cancel 不能 Notify
Widget lost / SkipPresentation / generation 变化如何统一
stale token 如何被阻断
如何确保不存在未完成 Record 的未来视觉残留
```

---

# 第十一节：全局 `Cancel / Reconcile` 收尾审计

## 11.1 本节目标与进入条件

只有前十节对应的 Blueprint 路由、播放事件和局部 PIE 验收都已经按计划完成后，才进入本节。

本节不是新增一种 Presentation Record，而是做 UI-A2E 在最终全链验收前必须完成的一次生命周期收口：

```text
任何正在播放的 Blueprint transient visual
如果因为 Skip / Collapse / ViewModel 外部推进 / Timeout / PresentationUnavailable / 防御性替换而被取消
→ 必须停止当前 Timer
→ 必须撤销或重建临时视觉
→ 必须回到“当前历史 ViewModel 所代表的状态”或安全清理掉无法从 ViewModel 重建的 transient
→ 绝不能调用 NotifyPresentationFinished
→ 绝不能留下旧 Token、旧 Widget 引用或旧未来值
```

最终目标是保证所有正式 Record 都遵守同一套生命周期：

```text
正常完成：
Before
→ transient After
→ exact-token Notify
→ Controller reducer
→ formal After

取消：
Before
→ transient After
→ Cancel
→ historical / reconciled visual
→ 不 Notify
→ 等 Controller 决定 collapse / skip / 新状态
```

本节完成后仍然不能立刻宣布 UI-A2E SEALED；下一节还要跑整条 A2E Scenario Acceptance。

---

# 第十一节 A：先锁定 C++ 基类已经提供的取消语义

## 11.2 不在 Blueprint 里重新实现 Token 权威判断

当前 `UBattleHUDWidgetBase` 已经负责：

```text
PlayPresentationRecord
→ CancelTrackedPresentationPlayback()   // 防御性清理旧 visual
→ TrackedPresentationPlaybackToken = 新 Token
→ BeginPresentationRecordPlayback(...)
```

而：

```text
CancelTrackedPresentationPlayback
```

会先：

```text
bHasTrackedPresentationPlayback = false
TrackedPresentationPlaybackToken = default
```

再调用 Blueprint：

```text
Cancel Presentation Record Playback(CancelledToken)
```

因此 Blueprint Cancel 事件里**不要**自行尝试判断：

```text
CancelledToken == ActivePresentationToken ?
```

作为是否清理的唯一条件。

基类已经在进入 Blueprint 之前清掉 tracked ownership，旧 Notify 即使晚到也不能把新 Token 清掉。

Blueprint 的职责是：

```text
清 transient visual
恢复历史显示
清 Blueprint 自己的 ActivePresentation* 状态
```

不是重新实现 Controller ownership。

---

## 11.3 `HandleViewModelChanged` 的顺序必须理解清楚

当前 C++：

```text
HandleViewModelChanged
→ 如果不是正常 completion / explicit Skip 的 suppression 窗口
   → CancelTrackedPresentationPlayback()
→ BP_OnViewModelChanged()
```

所以当一次非正常状态推进发生时，顺序是：

```text
先 Cancel 当前 transient
再让正常 HUD 用新 ViewModel 重绘
```

这意味着 Cancel 中使用：

```text
ViewModel.Player
ViewModel.Enemy
ViewModel.Energy
ViewModel.DrawCount
ViewModel.DiscardCount
ViewModel.Statuses
```

时，读到的是当前历史显示状态，而不是正在播放 Record 的未来 `After`。

这一点正是本节所有 restore 的依据。

---

## 11.4 `SkipPresentation` 的特殊顺序

当前 Widget 侧：

```text
SkipPresentation
→ CancelTrackedPresentationPlayback()
→ Controller.SkipPresentation()
```

Controller 随后会 collapse 到 newest frozen FinalSnapshot。

因此 Blueprint Cancel 必须做到：

```text
立即停止旧 transient
```

随后 Controller 会把 ViewModel 推到最终快照，正常 HUD 再刷新。

不要在 Cancel 中自行查询最新 Gameplay 状态，也不要自行跳到 FinalSnapshot。

---

## 11.5 `NativeDestruct` 不会调用 Blueprint Cancel

当前基类在 `NativeDestruct` 中只：

```text
清 tracked token
→ Controller.NotifyWidgetLost(this)
```

不会再执行：

```text
Cancel Presentation Record Playback
```

这是正确设计，因为 Widget 已经离开树。

所以本节不要为了“覆盖 Widget Lost”去修改 Blueprint Destroy / Destruct 事件来补 Notify 或补 Cancel。

Widget Lost 的 authoritative catch-up 由 Controller 负责。

---

# 第十一节 B：把 Cancel 改成按 `ActivePresentationType` 分流

## 11.6 当前旧 Cancel 的问题

早期版本的 `Cancel Presentation Record Playback` 是一条“全部一起清”的线：

```text
Clear Timer
→ 恢复 HiddenHandCardWidget
→ Remove PlayedCardWidget
→ Hide Damage text
→ 两个角色 Opacity = 1
→ Remove ActiveStatusPresentationWidget
→ 清 refs / type / token
```

这种写法在只有少数 Record 时还能工作。

但完成 A2E 全类型后会有两个问题：

1. 某些 Record 的 transient 是“改正式 HUD 值”，不能只 Hide / Remove Widget。
2. 某些引用在不同 Record 中语义不同，例如 `ActiveStatusPresentationWidget`：
   - Creation 时是临时新增行；
   - Update 时是正式历史行；
   - Removal 时也是正式历史行。

所以本节要把 Cancel 改成：

```text
公共清 Timer
↓
Switch ActivePresentationType
↓
每个 Record 做自己的 restore / cleanup
↓
统一清本地 Active 状态
```

---

## 11.7 Cancel 第一节点：无条件清 Timer

打开：

```text
WBP_BattleHUD
→ Event Graph
→ Cancel Presentation Record Playback
```

事件入口后第一步保留或新增：

```text
Clear and Invalidate Timer by Handle
```

Handle：

```text
ActivePresentationTimer
```

必须在任何视觉恢复之前先清。

原因：

```text
Cancel 已发生
→ 旧 FinishPresentationRecord 不应该再晚到
```

即使 C++ exact-token 可以防止 stale completion 破坏 Controller，新 Timer 仍可能修改 Blueprint 自己的 Widget，所以必须本地清理。

---

## 11.8 第二节点：`Switch on EBattlePresentationRecordType`

从：

```text
Get ActivePresentationType
```

放置：

```text
Switch on EBattlePresentationRecordType
```

至少处理：

```text
CardPlayed
Damage
BlockChanged
EnergyChanged
CardZoneChanged
DeckShuffled
StatusChanged
Victory
Defeat
ResolutionFault
```

`None` 直接走统一尾部清理即可。

不要把十种类型继续堆进一个长 Sequence。

---

# 第十一节 C：`CardPlayed` Cancel

## 11.9 恢复被隐藏的 Hand Widget

`PlayCardPresentation` 开始时会：

```text
HiddenHandCardWidget = HandCardWidget
→ HandCardWidget.Visibility = Hidden
```

所以 `CardPlayed` Cancel 分支先：

```text
IsValid(HiddenHandCardWidget)
```

Valid：

```text
HiddenHandCardWidget.SetVisibility(Visible)
```

随后：

```text
HiddenHandCardWidget = None
```

这里恢复的是当前历史 Hand 中原本就存在的卡。

不要 Create 新卡。

---

## 11.10 删除尚未正式成立的 PlayArea transient

同一个 `CardPlayed` active window 中：

```text
PlayedCardWidget
```

是根据冻结 `CardPlayed.Card` 创建的 presentation-only Widget。

Cancel 时：

```text
IsValid(PlayedCardWidget)
→ RemoveFromParent
→ PlayedCardWidget = None
```

因此 CardPlayed cancel 最终：

```text
隐藏的正式 Hand 卡重新可见
+
临时 PlayArea 卡消失
```

这正好回到 CardPlayed 之前。

---

# 第十一节 D：`Damage` Cancel

## 11.11 先清通用 Damage 特效

`Damage` 分支必须继续：

```text
Txt_DamagePresentation.Visibility = Collapsed
Combatant_PlayerPresentation.RenderOpacity = 1.0
Combatant_EnemyPresentation.RenderOpacity = 1.0
```

这里建议两个角色都恢复 `1.0`，不要只恢复上一次命中的目标；Cancel 应是幂等安全清理。

---

## 11.12 不能只恢复透明度，必须恢复 HP / Block 历史值

Damage active playback 已经直接把冻结：

```text
HPAfter
BlockAfter
```

写进正式 HUD：

```text
Txt_PlayerHP / Txt_EnemyHP
PB_PlayerHP / PB_EnemyHP
Txt_PlayerBlock / Txt_EnemyBlock
```

所以 Cancel 如果只做：

```text
Hide Damage text
Opacity = 1
```

会留下未来 HP / Block。

本节必须补 restore。

根据：

```text
bDamageTargetIsPlayer
```

分流。

Player：

```text
ViewModel.Player.HP
ViewModel.Player.MaxHP
ViewModel.Player.Block
```

重新写：

```text
Txt_PlayerHP = "{HP}/{MaxHP}"
PB_PlayerHP.Percent = HP / Max(MaxHP, 1)
Txt_PlayerBlock = Block
```

Enemy 同理：

```text
ViewModel.Enemy.HP
ViewModel.Enemy.MaxHP
ViewModel.Enemy.Block
```

不要使用：

```text
Damage.HPBefore
Damage.BlockBefore
```

因为 Cancel 事件没有保存完整 Damage payload；而且全局 cancel contract 应以当前历史 ViewModel 为 reconcile 来源。

---

## 11.13 建议抽出一个 Vitals 恢复 helper

如果 Player / Enemy HP/Block 格式节点很长，建议在 `WBP_BattleHUD` 新建函数：

```text
RefreshCombatantVitalsFromViewModel
```

输入：无。

函数内部：

```text
IsValid(ViewModel)
→ Player HP / Percent / Block
→ Enemy HP / Percent / Block
```

然后：

```text
Damage Cancel
→ RefreshCombatantVitalsFromViewModel
```

后面的 `BlockChanged Cancel` 也可以复用它。

注意函数只读 ViewModel，不写 ViewModel。

---

# 第十一节 E：`BlockChanged` Cancel

## 11.14 记录 BlockChanged 的目标身份

当前 `PlayBlockChangedPresentation` 会根据：

```text
TargetPresentationId
```

决定写 Player 还是 Enemy 的 `BlockAfter`。

为了 Cancel 能知道刚才覆盖的是哪一边，增加 Blueprint 成员：

```text
bBlockTargetIsPlayer : Boolean
```

默认：

```text
false
```

在 `PlayBlockChangedPresentation` 的目标判断完成后，先：

```text
Set bBlockTargetIsPlayer = IsPlayer
```

再写 BlockAfter。

不要在 Cancel 时重新猜目标。

---

## 11.15 BlockChanged Cancel 恢复历史 Block

如果已经做了：

```text
RefreshCombatantVitalsFromViewModel
```

那么 `BlockChanged` Cancel 直接调用该 helper 即可。

如果不抽 helper，则：

```text
Branch(bBlockTargetIsPlayer)
```

Player：

```text
ViewModel.Player.Block
→ ToText(Integer)
→ Txt_PlayerBlock.SetText
```

Enemy：

```text
ViewModel.Enemy.Block
→ ToText(Integer)
→ Txt_EnemyBlock.SetText
```

不要恢复成 `BlockBefore` 常量变量，也不要做 `BlockAfter - Delta`。

---

# 第十一节 F：`EnergyChanged` Cancel

## 11.16 从历史 ViewModel 恢复正式 Energy 显示

Energy active playback 会把：

```text
EnergyAfter
```

临时写进现有正式 `EnergyPanel` 数字控件。

所以 Cancel：

```text
IsValid(ViewModel)
→ ViewModel.Energy
→ 使用正常 HUD 完全相同的文本格式
→ 正式 Energy TextBlock.SetText
```

如果正常 HUD 是：

```text
{Current}/{Max}
```

则 Cancel 也使用：

```text
ViewModel.Energy
ViewModel.MaxEnergy
```

如果正常 HUD 只是单独显示 Current，同样保持现有格式。

不要：

```text
EnergyAfter - Delta
```

不要把 terminal 时的 Energy 强制归零或回满。

---

# 第十一节 G：`StatusChanged` Cancel

## 11.17 所有 Status 生命周期统一用历史 ViewModel 重建

这一点继续沿用第三节锁定方案。

`StatusChanged` Cancel 不区分：

```text
Creation
Update / Increase
Reduction
Removal
```

全部执行：

```text
IsValid(ViewModel)
→ RebuildStatusIcons(ViewModel.Player.Statuses, WB_PlayerStatuses)
→ RebuildStatusIcons(ViewModel.Enemy.Statuses, WB_EnemyStatuses)
→ ActiveStatusPresentationWidget = None
```

这会自动处理：

```text
Creation Cancel
→ 临时新状态消失

Update Cancel
→ AmountAfter 被历史 Amount 恢复

Removal Cancel
→ 被 Collapsed 的历史状态重新创建并显示
```

禁止：

```text
ActiveStatusPresentationWidget.RemoveFromParent
```

作为 StatusChanged 的统一 Cancel 方案。

因为 Update / Removal 时它指向正式历史行。

---

# 第十一节 H：`DeckShuffled` Cancel

## 11.18 恢复 Draw / Discard 历史计数

DeckShuffled active playback 会直接显示：

```text
DrawCountAfter
DiscardCountAfter
```

Cancel 必须恢复：

```text
ViewModel.DrawCount
ViewModel.DiscardCount
```

接回第九节定位到的正式：

```text
DrawPileCountText
DiscardPileCountText
```

并使用正常 HUD 相同格式。

不要自己做逆运算：

```text
DrawAfter - MovedCardCount
DiscardAfter + MovedCardCount
```

历史 ViewModel 是唯一 reconcile 来源。

---

# 第十一节 I：`CardZoneChanged` Cancel

## 11.19 CardZoneChanged 需要按“当前 transient 引用”清理

第八节完成后 CardZoneChanged 至少覆盖：

```text
DrawPile → Hand
Hand → DiscardPile
PlayArea → Destination
```

这些路径的 Cancel 不应该去修改 Gameplay pile 数组，也不应该用 `FromIndex / ToIndex` 逆操作 ViewModel。

只清理 Blueprint 自己创建/隐藏的 transient。

---

## 11.20 DrawPile → Hand：删除 transient drawn card

如果第八节使用了：

```text
ZoneChangedDrawnCardWidget : WBP_BattleCard reference
```

Cancel：

```text
IsValid(ZoneChangedDrawnCardWidget)
→ RemoveFromParent
→ ZoneChangedDrawnCardWidget = None
```

因为正式历史 ViewModel 此时还没有这张新 Hand card。

原来历史 Hand 中的其他 Widget 不动。

---

## 11.21 Hand → DiscardPile：恢复被隐藏的正式 Hand card

如果第八节为了表现 discard 使用了一个引用保存被隐藏的 Hand Widget，例如：

```text
ZoneChangedHiddenHandCardWidget
```

Cancel：

```text
IsValid(ZoneChangedHiddenHandCardWidget)
→ SetVisibility(Visible)
→ ZoneChangedHiddenHandCardWidget = None
```

如果你的第八节实现使用的是别的变量名，沿用实际名称，不必为了本文强行改名。

关键语义只有：

```text
取消 Hand→Discard
→ 历史 Hand 卡必须重新出现
```

---

## 11.22 PlayArea → Destination：清理可能失配的 PlayArea transient

当前 `PlayedCardWidget` 不属于 ViewModel 的正式 HandCards；它是跨 Record 保留的 presentation transient。

正常顺序是：

```text
CardPlayed 完成
→ PlayedCardWidget 继续留在 OV_PlayArea
→ 后续 Damage / Status 等表现
→ CardZoneChanged(PlayArea→Destination)
→ 正常 Finish 时 RemoveFromParent
```

如果整个 playback 被 Cancel / Skip / Collapse，不能留下这张悬空 PlayArea 卡。

所以 `CardZoneChanged` Cancel 最终做：

```text
IsValid(PlayedCardWidget)
→ RemoveFromParent
→ PlayedCardWidget = None
```

这是一种 presentation transient 安全清理，不是修改 Gameplay zone。

在 Controller 的正常单 active Record contract 下，不需要为“另一个 Record 同时替换当前 CardZoneChanged”保留这张卡。

---

## 11.23 CardZoneChanged Cancel 不改 pile 数字

第八节已锁定：

```text
CardZoneChanged payload 没有冻结 pile before/after
```

因此 active window 不自行重算正式 Draw / Discard / Exhaust 数字。

Cancel 同样不要做：

```text
DrawCount + 1
DiscardCount - 1
ExhaustCount - 1
```

如果随后 ViewModel 发生 collapse / skip / reducer 推进，正常 HUD 会按 snapshot 统一刷新 pile count。

---

# 第十一节 J：Terminal Cancel

## 11.24 Victory / Defeat / ResolutionFault 统一恢复历史 terminal surface

第十节应该已经抽出或建立：

```text
RefreshTerminalSurfaceFromViewModel
```

那么三个 Terminal Cancel case：

```text
Victory
Defeat
ResolutionFault
```

都调用同一个：

```text
RefreshTerminalSurfaceFromViewModel
```

它必须根据历史：

```text
ViewModel.Outcome
ViewModel.InteractionState
```

决定：

```text
Overlay_Terminal Visible / Collapsed
Terminal 文案
```

因此：

```text
正常战斗历史状态
→ Cancel terminal transient
→ Overlay_Terminal Collapsed
```

而如果历史本身已经 terminal：

```text
→ 保持对应正式 terminal surface
```

---

## 11.25 `PresentationUnavailable` 仍然不能塞进 ResolutionFault

Cancel / Reconcile 时继续保持：

```text
ResolutionFault
= Gameplay / framework resolution terminal Record

PresentationUnavailable
= Presentation subsystem fail-safe UI state
```

如果 ViewModel 当前：

```text
InteractionState = PresentationUnavailable
```

`RefreshTerminalSurfaceFromViewModel` 必须按当前正常 HUD 的 PresentationUnavailable 规则恢复，不要假装成：

```text
Outcome = ResolutionFaulted
```

---

# 第十一节 K：统一尾部清理

## 11.26 所有 Switch case 最终进入同一个 Local Cleanup

各类型 restore 完成后，统一执行：

```text
HiddenHandCardWidget = None
ZoneChangedDrawnCardWidget = None            // 如果存在
ZoneChangedHiddenHandCardWidget = None       // 如果存在
ActiveStatusPresentationWidget = None
bDamageTargetIsPlayer = false
bBlockTargetIsPlayer = false
ActivePresentationType = None
ActivePresentationToken = default
```

`PlayedCardWidget`：

```text
只有仍应跨正常 Record 保留时才保留
Cancel 路径在 CardPlayed / CardZoneChanged 中已经按上文明确清理
```

不要在统一尾部无条件：

```text
RemoveFromParent(PlayedCardWidget)
```

否则会把类型语义重新混回一起。

---

## 11.27 Cancel 中绝对不能 Notify

整张 Cancel 图中搜索：

```text
Notify Presentation Finished
```

结果必须：

```text
0 个来自 Cancel 执行线的可达调用
```

原因：

```text
Cancel
```

代表当前 Blueprint 不再拥有这条 Record 的正常完成权。

后续是 Skip、Collapse、Timeout 或新 Snapshot，由 Controller 自己决定。

如果 Cancel 也 Notify，可能导致：

```text
旧 Record reducer 被错误推进
下一 Record 提前开始
旧 Token 与新 Token 交叉
```

---

# 第十一节 L：正常 Finish 反向审计

## 11.28 Cancel 修好后，再检查 Finish 没被错误“恢复旧值”

打开：

```text
FinishPresentationRecord
```

逐项确认：

```text
Damage
→ Hide damage text / opacity restore
→ 不恢复 HPBefore / BlockBefore

BlockChanged
→ 不恢复 BlockBefore

EnergyChanged
→ 不恢复 EnergyBefore

StatusChanged Update/Reduction
→ 不恢复 AmountBefore

StatusChanged Removal
→ 不把 Widget 重新 Visible

DeckShuffled
→ 不恢复 DrawCountBefore / DiscardCountBefore

Terminal
→ 不在 Notify 前把 Overlay_Terminal 隐藏
```

正常完成的核心必须始终是：

```text
transient After 保持
→ Notify
→ reducer formal After
→ 正常 HUD 重绘仍然是同一个 After
```

---

## 11.29 每种异步 Record 只能 Notify 一次

检查：

```text
CardPlayed
Damage
BlockChanged
EnergyChanged
CardZoneChanged
DeckShuffled
StatusChanged
Victory
Defeat
ResolutionFault
```

每个正常 Finish 路径最终只能到：

```text
NotifyPresentationRecordFinished
```

一次。

不能：

```text
某分支先 Notify
→ 公共尾部又 Notify
```

也不能：

```text
Timer A Notify
Animation Finished B Notify
```

同时存在。

---

# 第十一节 M：`NotifyPresentationRecordFinished` 审计

## 11.30 保持统一顺序

当前统一完成事件应继续：

```text
NotifyPresentationRecordFinished
→ ClearAndInvalidateTimerByHandle(ActivePresentationTimer)
→ NotifyPresentationFinished(ActivePresentationToken)
→ ActivePresentationType = None
→ ActivePresentationToken = default
```

不要交换成：

```text
ActivePresentationToken = default
→ NotifyPresentationFinished(default)
```

也不要在 Notify 之前清掉 token 数据。

---

## 11.31 不要因为 C++ Notify 是 deferred 就再加 Blueprint Delay

C++ `NotifyPresentationFinished(Token)` 已经通过 CoreTicker 延迟转发，避免 Blueprint 同步 re-entry Controller。

因此 Blueprint 不需要再：

```text
Delay(0)
SetTimerForNextTick
```

来“保证异步”。

现有 0.5s presentation timer + C++ deferred forwarding 已经构成正确边界。

---

# 第十一节 N：Timeout / stale callback 审计

## 11.32 Controller timeout 不需要 Blueprint 自己处理

如果 Blueprint 忘记或未能及时 Notify，Controller timeout 会：

```text
CompleteActiveRecord
→ reducer 推进 ViewModel
→ ViewModel Changed
```

随后基类发现 Widget 仍有 tracked visual，会在非 suppression 情况下调用 Cancel，再让正常 HUD 重绘。

所以 Blueprint 不需要另做：

```text
Timeout Event
```

但本节必须保证：

```text
Cancel 能真正恢复/清理所有 transient
```

否则 timeout 后会留下旧视觉。

---

## 11.33 stale Timer 必须无害

验证：

```text
Cancel
→ ClearAndInvalidateTimer
```

之后旧 `FinishPresentationRecord` 不应再执行。

即使极端情况下 stale completion 已经排进下一 tick，C++ 仍会用完整：

```text
BattleId
ResolutionId
PresentationSequence
LocalPlaybackGeneration
```

做 exact-token gate。

Blueprint 不要通过“只比较 PresentationSequence”自己实现一个弱化版本。

---

# 第十一节 O：Compile 与静态结构验收

## 11.34 Compile 顺序

本节如果只改 `WBP_BattleHUD`：

```text
Compile WBP_BattleHUD
→ 0 Errors
→ Save
```

如果同时改了：

```text
WBP_BattleStatus
WBP_BattleCard
```

先编译子 Widget：

```text
WBP_BattleStatus
WBP_BattleCard
```

最后再：

```text
WBP_BattleHUD
```

避免 GeneratedClass stale reference。

---

## 11.35 静态检查表

进入 PIE 前全部确认：

```text
[ ] Cancel 第一件事 Clear ActivePresentationTimer
[ ] Cancel 使用 Switch ActivePresentationType
[ ] CardPlayed 恢复 Hidden Hand + 删除临时 PlayedCard
[ ] Damage 恢复历史 HP / Block + Hide damage + opacity=1
[ ] BlockChanged 恢复历史 Block
[ ] EnergyChanged 恢复 ViewModel.Energy
[ ] StatusChanged 用 ViewModel.Statuses 重建两边
[ ] DeckShuffled 恢复 ViewModel Draw/Discard counts
[ ] CardZone Draw→Hand 删除 transient drawn card
[ ] CardZone Hand→Discard 恢复历史 Hand widget
[ ] CardZone PlayArea→Destination 清理悬空 PlayedCard transient
[ ] Victory / Defeat / ResolutionFault 恢复历史 terminal surface
[ ] PresentationUnavailable 不当作 ResolutionFault
[ ] Cancel 不调用 NotifyPresentationFinished
[ ] Finish 不恢复任何 Before 值
[ ] 每个正常 Record 只 Notify 一次
[ ] ActivePresentationType / Token 最终清空
[ ] 临时 Widget refs 最终清空
```

---

# 第十一节 P：PIE Cancel / Reconcile 验收

## 11.36 测试 1：Damage active window 中 Skip

构造：

```text
敌人 HP = A
→ 播放 Damage
→ transient HP = B
→ 在 0.5s 完成前触发 SkipPresentation
```

观察：

```text
Damage text 消失
Opacity 恢复 1
旧 transient 不继续闪烁
Controller collapse 后 HUD 与 newest FinalSnapshot 一致
不会卡 Resolving
```

禁止：

```text
旧 HPAfter 残留
旧 Timer 再次修改 UI
旧 Notify 推进错误 Record
```

---

## 11.37 测试 2：Status Update active window 中 Cancel

构造：

```text
历史 Weak A
→ StatusChanged update transient 显示 Weak B
→ 完成前触发 Cancel/Skip
```

取消瞬间检查：

```text
状态列表从当前历史 ViewModel 重建
```

若历史仍是 A：

```text
Weak B 不得继续留着
```

随后如果 Controller collapse 到更新后的 FinalSnapshot B：

```text
正常 ViewModel redraw 才再次显示 B
```

这两次状态变化来源必须清晰，不能由旧 transient timer 驱动。

---

## 11.38 测试 3：Status Removal active window 中 Cancel

构造：

```text
历史 Weak 1
→ Removal transient Collapsed
→ 完成前 Cancel
```

取消后：

```text
历史 ViewModel 仍有 Weak 1
→ RebuildStatusIcons
→ Weak 1 重新出现
```

如果随后 Skip collapse 的最终快照已经没有 Weak：

```text
ViewModel redraw 再正式移除
```

不能永久丢失历史状态行。

---

## 11.39 测试 4：EnergyChanged active window 中 Cancel

构造：

```text
历史 Energy A
→ transient Energy B
→ Cancel
```

取消后先恢复：

```text
ViewModel.Energy = A
```

如果随后 Controller collapse 到 B：

```text
正常 ViewModel 刷新进入 B
```

旧 Energy timer 不得晚到再次修改。

---

## 11.40 测试 5：DeckShuffled active window 中 Cancel

构造：

```text
历史 Draw = 0
Discard = N
→ transient Draw = N
Discard = 0
→ Cancel
```

Cancel reconcile：

```text
Draw = ViewModel.DrawCount
Discard = ViewModel.DiscardCount
```

如果随后 collapse 到 FinalSnapshot，则由正常 HUD 显示最终 pile counts。

---

## 11.41 测试 6：CardPlayed active window 中 Cancel

构造：

```text
Hand 中卡 C
→ CardPlayed transient
→ C 的 Hand widget Hidden
→ PlayArea 出现临时 C
→ Cancel
```

必须：

```text
Hand 中原 C 重新 Visible
PlayArea 临时 C 被移除
PlayedCardWidget = None
HiddenHandCardWidget = None
```

---

## 11.42 测试 7：CardZoneChanged active window 中 Cancel

至少抽查两种：

### DrawPile → Hand

```text
transient 新卡出现在 Hand
→ Cancel
→ transient 新卡删除
→ 历史 Hand 不增加
```

### Hand → Discard

```text
历史 Hand 卡被临时隐藏
→ Cancel
→ 该卡恢复 Visible
```

另外抽查：

```text
PlayArea → Destination
→ Cancel / Skip 后没有悬空 PlayArea card
```

---

## 11.43 测试 8：Terminal active window 中 Cancel

Victory / Defeat 任取一种：

```text
terminal overlay transient 出现
→ 完成前 Cancel
```

如果历史 ViewModel 仍非 terminal：

```text
Overlay_Terminal 必须恢复 Collapsed
```

然后若 Controller collapse 到 terminal FinalSnapshot：

```text
正常 HUD 再显示 terminal
```

这不是闪回 bug；前后两次显示的 authoritative 来源不同。

真正禁止的是旧 Timer 在新状态之后再次覆盖终局 UI。

---

## 11.44 测试 9：PresentationUnavailable 隔离

人为触发 Presentation subsystem unavailable 路径时确认：

```text
当前 transient 被 Cancel / 清理
ViewModel.InteractionState = PresentationUnavailable
```

且没有伪造：

```text
ResolutionFault Record
Outcome = ResolutionFaulted
```

Presentation unavailable 仍是 presentation fail-safe，不是 gameplay terminal record。

---

## 11.45 测试 10：旧 Timer / stale completion

在任一 active Record 中：

```text
开始播放
→ 立刻 Skip / Cancel
→ 等待超过原 0.5s
```

观察：

```text
UI 不再次跳变
Controller 不再次推进旧 Record
新 Record / 新 Snapshot 不被旧 callback 覆盖
```

如果日志可观察 Token，确认旧：

```text
BattleId / ResolutionId / PresentationSequence / Generation
```

不会完成新的 active Record。

---

# 第十一节 Q：本节通过标准

## 11.46 可以进入下一节的条件

全部满足后才结束本节：

```text
✓ 所有 active timer 在 Cancel 第一时间清除
✓ Cancel 按 ActivePresentationType 分流
✓ 所有直接覆盖正式 HUD 数值的 Record 都可恢复历史 ViewModel
✓ 所有临时创建/隐藏的 Widget 都可安全撤销
✓ Status creation/update/reduction/removal 共用历史 ViewModel reconcile
✓ Damage HP/Block 不再留下未来值
✓ BlockChanged 不再留下未来 Block
✓ EnergyChanged 不再留下未来 Energy
✓ DeckShuffled 不再留下未来 pile counts
✓ Terminal 不再留下未来 overlay
✓ Card transient 不残留
✓ Cancel 永远不 Notify
✓ Finish 永远不恢复 Before
✓ stale Timer 不影响新状态
✓ PresentationUnavailable 与 ResolutionFault 分离
✓ Compile 0 Errors
✓ PIE Cancel 抽查通过
✓ 最终没有卡在 Resolving
```

本节完成后记录：

```text
UI-A2E Global Cancel/Reconcile = VALIDATED
```

但整体仍然：

```text
UI-A2E = PARTIAL
```

直到下一节完成整条 End-to-End Scenario Acceptance。

---

## 11.47 下一节锁定目标

下一节进入：

```text
UI-A2E 全链 PIE End-to-End Acceptance
```

会把前面已经分开验证的 Record 串成真实连续战斗流程，至少覆盖：

```text
Scenario A：普通出牌链
Scenario B：状态生命周期 + EndTurn
Scenario C：Discard → Shuffle → Draw
Scenario D：Victory / Defeat terminal
Scenario E：Cancel / Skip / fail-safe
```

下一节不再主要增加功能节点，而是做最终跨 Record 顺序、无闪回、exact-token、Idle/Terminal 收口和 A2E Seal 前验收。

---

# 第十二节：UI-A2E 全链 PIE End-to-End Acceptance

## 12.1 本节目标

这一节不再新增新的 Presentation Record 类型，而是把前面已经分别实现的 Blueprint async playback 串成真实战斗流程，验证：

```text
Gameplay commit
→ Presentation Envelope
→ Record 逐条播放
→ exact-token completion
→ WorkingSnapshot reducer 推进
→ ViewModel 正式刷新
→ 下一条 Record
→ 最终 Idle / Terminal
```

只有本节所有 Scenario 都通过，才允许进入最后的文档收口与 `UI-A2E COMPLETE / SEALED`。

本节重点不是“单个动画看起来正常”，而是验证不同 Record 连续出现时不会发生：

```text
顺序错误
重复表现
Before/After 闪回
历史状态提前推进
stale timer
stale token
旧 transient widget 残留
Resolving 卡死
输入过早解锁
terminal 被后续 HUD 刷新覆盖
```

---

## 12.2 进入本节前的硬性前置条件

开始全链 PIE 前，必须确认前面各节均已经实际接线并通过对应局部验收：

```text
CardPlayed                         VALIDATED
Damage                             VALIDATED
BlockChanged                       VALIDATED
CardZoneChanged PlayArea->Dest     VALIDATED
StatusChanged Creation             VALIDATED
StatusChanged Update/Reduction     VALIDATED
StatusChanged Removal              VALIDATED
EnergyChanged                      VALIDATED
CardZoneChanged Draw/EndTurn       VALIDATED
DeckShuffled                       VALIDATED
Victory                            VALIDATED
Defeat                             VALIDATED
ResolutionFault                    VALIDATED
Cancel/Reconcile audit             PASS
```

如果其中任一项仍是：

```text
NOT WIRED
PARTIAL
未做 PIE
```

不要通过全链 Scenario 来“顺便测试”。先回对应章节单独修好。

---

## 12.3 开始前 Compile / Save

按依赖顺序执行：

```text
WBP_BattleStatus
→ Compile
→ Save

WBP_BattleCard
→ Compile
→ Save

WBP_BattleHUD
→ Compile
→ Save
```

要求：

```text
0 Errors
```

如果 Blueprint Compiler Results 有 Warning，只要与本轮逻辑有关，也先检查清楚；不要在存在未知节点错误的情况下记录正式 End-to-End PASS。

随后关闭可能残留的旧 PIE，再重新开始一个全新的 PIE session。

---

## 12.4 本节统一观察方法

每个 Scenario 都同时观察三个层面。

### A. Record 顺序

用当前项目已有的 Presentation / Battle 日志确认 Record 顺序。

不要仅凭 UI 动画先后肉眼猜测。

至少记录：

```text
ResolutionId
PresentationSequence
Record.Type
必要的 payload 关键字段
```

同一个 Envelope 中 `PresentationSequence` 必须严格递增。

### B. HUD transient visual

每条 Blueprint 接管的 Record 在自己的 active window 内，应只显示该 Record 的冻结 After 值或对应 transient visual。

例如：

```text
Damage
→ HPAfter / BlockAfter

StatusChanged
→ AmountAfter / DescriptionAfter

EnergyChanged
→ EnergyAfter

DeckShuffled
→ DrawCountAfter / DiscardCountAfter
```

### C. Record 完成后的正式 HUD

0.5 秒 async 完成后：

```text
NotifyPresentationFinished(Token)
→ reducer 推进
→ ViewModel Changed
→ 正常 HUD rebuild
```

最终显示必须与刚才 transient After 一致。

禁止出现：

```text
Before
→ transient After
→ Before
→ formal After
```

本节把这种现象统一记为：

```text
After flashback
```

任何 Scenario 出现一次都不能判 PASS。

---

# Scenario A：普通非致死出牌链

## 12.5 目标

先用最简单的普通攻击确认已经验证过的基础链在完整 A2E 下仍然成立。

推荐使用：

```text
Strike / 普通单体攻击牌
```

要求攻击不会击杀 Enemy。

例如：

```text
Enemy HP = 100
Strike Damage = 6
Player Energy = 5
Card Cost = 1
```

---

## 12.6 操作步骤

1. 新开 PIE。
2. 等待 HUD 完全进入可操作状态。
3. 记录打牌前：

```text
Player Energy
Enemy HP
Enemy Block
Hand 中目标卡 RuntimeId
Discard/Exhaust 数量
InteractionState
```

4. 选择 Strike。
5. 选择 Enemy。
6. Confirm。
7. 从点击 Confirm 开始，不再进行第二次输入，等待整个表现队列自然结束。

---

## 12.7 预期 Record 语义

普通攻击至少应看到类似：

```text
CardPlayed
→ Damage
→ 可能的 followup Records
→ CardZoneChanged(PlayArea → DiscardPile / ExhaustPile / RemovedPile)
```

关键能量规则：

```text
CardPlayed
EnergyBefore = 5
EnergyAfter  = 4
CostPaid     = 1
```

同一次费用不能随后再出现一个等价的：

```text
EnergyChanged 5 → 4
```

如果出现，就是卡费重复表现，Scenario A 直接 FAIL。

---

## 12.8 视觉检查

### CardPlayed active window

应看到：

```text
原 Hand Card 暂时 Hidden
PlayArea 出现 presentation-only Card Widget
Energy 直接显示 EnergyAfter
```

### Damage active window

应看到：

```text
Enemy presentation flash / opacity effect
Damage number
Enemy HP = Damage.HPAfter
Enemy Block = Damage.BlockAfter
```

### CardZoneChanged active window

应看到 PlayArea card 按当前既定 destination retirement 逻辑结束，不留下 duplicate Card Widget。

---

## 12.9 Scenario A 完成条件

等待最后一条 Record 完成后，确认：

```text
Enemy HP 最终正确
Player Energy 最终正确
手牌中已无该 RuntimeId
目标 pile 数量正确
OV_PlayArea 无残留卡
Txt_DamagePresentation = Collapsed
两个 Combatant RenderOpacity = 1.0
ActivePresentationType = None
没有 active timer 残留
InteractionState = Idle（若战斗仍可继续）
```

并确认无：

```text
Energy 5→4→5→4
HP Before→After→Before→After
牌先消失又重新回 Hand
同一 CardZoneChanged 播放两次
```

全部满足：

```text
Scenario A = PASS
```

---

# Scenario B：StatusChanged 生命周期完整链

## 12.10 目标

在同一次真实战斗中把一个状态经历：

```text
Creation
→ Increase / Reapply
→ Reduction / TurnEndDecay
→ Removal
```

验证 exact identity 始终稳定。

状态身份必须始终按：

```text
TargetPresentationId
+ StatusId
+ RuntimeSequence
```

不要只观察“屏幕上看起来只有一个 Weak”。日志中也必须确认 RuntimeSequence 没被错误替换。

---

## 12.11 Creation

触发一个当前项目能真实创建的状态，例如：

```text
Weak / Vulnerable / 等价状态
```

确认 Record：

```text
bCreated = true
bRemoved = false
AmountBefore = 0
AmountAfter > 0
RuntimeSequence > 0
```

视觉：

```text
原本无状态
→ transient 创建状态 row
→ async interval 内持续可见
→ Notify
→ formal HUD 仍保留同一状态
```

不得：

```text
出现 → 消失 → 再出现
```

---

## 12.12 Increase / Reapply

再次施加相同 runtime status，使其产生：

```text
bCreated = false
bRemoved = false
Reason = Increased
AmountAfter > AmountBefore
```

确认：

```text
RuntimeSequence 与 Creation 相同
状态 Widget 数量保持 1
原 Widget Amount A → B
```

不得出现第二个同 StatusId icon。

正常 completion 后仍为：

```text
B
```

不得：

```text
A → B → A → B
```

---

## 12.13 Reduction / TurnEndDecay

触发真实：

```text
Reduced
或
TurnEndDecay
```

但本次先让 `AmountAfter > 0`，不要立即 Removal。

确认：

```text
bCreated = false
bRemoved = false
AmountBefore > AmountAfter > 0
same RuntimeSequence
```

视觉必须是同一个 row：

```text
A → B
```

没有 duplicate，也没有 After flashback。

---

## 12.14 Removal

最后让该状态变成：

```text
AmountAfter = 0
bRemoved = true
```

active window：

```text
精确 Widget 临时 Collapsed
```

completion 后：

```text
reducer 正式从 ViewModel.Statuses 移除
HUD rebuild 后仍然不存在
```

不得出现：

```text
消失 → 出现 → 再消失
```

也不能影响其他 Status row。

全部满足：

```text
Scenario B = PASS
```

---

# Scenario C：完整 EndTurn / EnemyTurn / 新回合链

## 12.15 目标

这是 A2E 非终局流程中最重要的一条。

需要覆盖真实存在的：

```text
Hand → DiscardPile
BlockChanged TurnStartClear / 其他真实 Block 变化
Status TurnEndDecay
EnergyChanged EndTurn / TurnStart
Enemy Damage
DeckShuffled（若本次条件满足）
DrawPile → Hand
```

注意：

```text
TurnEnded 本身不是 Presentation Record
```

不要因为点击了 EndTurn 就期待一条 `TurnEnded` 可见 Record。

只有真实发生变化的 committed facts 才应出现 Presentation Record。

---

## 12.16 构造前置状态

为了让一次 EndTurn 覆盖尽可能多的 Record，进入点击 EndTurn 前建议准备：

```text
Hand 中至少有 2 张未使用牌
Player 有非零 Block（如果当前规则会在回合切换清除）
Player 或 Enemy 有会 TurnEndDecay 的 Status
Player Energy > 0
Enemy 有可执行攻击 Intent
DrawPile 尽量接近 0
DiscardPile 有若干张牌
```

若为了触发 shuffle 需要先多走几回合，可以先准备到：

```text
DrawPile = 0
DiscardPile > 0
下一次需要抽牌
```

再执行本 Scenario。

---

## 12.17 点击 EndTurn 后不要追加人工输入

点击：

```text
Btn_EndTurn
```

之后直到下一玩家回合重新 Idle 前：

```text
不要点卡牌
不要点角色
不要重复点 EndTurn
不要 Skip
```

观察整个自动队列自然播放。

---

## 12.18 检查 Hand → DiscardPile

每一张实际被弃掉的 Hand Card 都应有自己的：

```text
CardZoneChanged
FromZone = Hand
ToZone   = DiscardPile
```

要求：

```text
按 RuntimeId 找到正确 Hand Widget
只处理该卡
每个 Record 完成后 reducer 再推进正式 Hand/Discard
多张卡按 Record 顺序逐条完成
```

禁止一次 Blueprint Event 直接：

```text
ClearChildren 全部手牌
```

来代替多条历史 Record。

---

## 12.19 检查 EnergyChanged

若 EndTurn 真实发生：

```text
EnergyBefore > EnergyAfter
```

应看到一条独立 `EnergyChanged`。

新玩家回合如果真实恢复能量：

```text
EnergyBefore < EnergyAfter
```

再看到对应 `EnergyChanged`。

每条都只表现一次。

再次确认：

```text
这些 EnergyChanged 不是 CardPlayed Cost 重复记录
```

---

## 12.20 检查 Enemy Damage

敌方攻击时：

```text
Damage
```

应按冻结：

```text
IncomingDamage
BlockedDamage
HPDamage
HPAfter
BlockAfter
```

显示。

如果玩家有 Block，特别检查：

```text
完全格挡时 HP 不下降
BlockAfter 正确
```

不要出现“先由 BlockChanged 清零，再 Damage 又拿旧 Block”这类历史顺序错乱。

---

## 12.21 检查 DeckShuffled → Draw

若当前 Scenario 触发 shuffle，日志顺序必须是：

```text
DeckShuffled
→ CardZoneChanged(DrawPile → Hand)
```

DeckShuffled active window：

```text
Draw count = DrawCountAfter
Discard count = DiscardCountAfter
```

不能在这里提前创建下一张 Hand card。

只有下一条：

```text
CardZoneChanged DrawPile→Hand
```

才创建对应 presentation Hand card。

若一次抽多张牌，必须逐条按各自 Record 播放。

---

## 12.22 Scenario C 最终状态

整个 EnemyTurn + TurnStart/Draw 链结束后确认：

```text
InteractionState = Idle
Player 可以再次正常选择卡牌
Btn_EndTurn 状态与当前 ViewModel 一致
Hand 与最终 ViewModel.HandCards 一致
Draw / Discard / Exhaust count 一致
Energy 一致
Player/Enemy HP、Block、Status 一致
没有 presentation-only Card Widget 残留
没有 hidden formal Hand Card 残留
没有 active status transient 引用
没有 active timer
```

如果没有触发 DeckShuffled，只能把 Scenario C 的 shuffle 子项记为：

```text
NOT EXERCISED
```

不能因此宣称 DeckShuffled full-chain PASS；必须单独构造触发后再补齐。

全部必要子项通过后：

```text
Scenario C = PASS
```

---

# Scenario D：Lethal Victory 全链

## 12.23 目标

验证 lethal card 在真实 Envelope 中仍保持锁定顺序：

```text
CardPlayed
→ Damage
→ followups
→ CardZoneChanged(PlayArea → Destination)
→ Victory
```

这里最重要的是：

```text
Victory 必须最后出现
```

不能因为 Enemy.HPAfter == 0 就让 Blueprint 自行提前弹出胜利界面。

---

## 12.24 构造 lethal 状态

把 Enemy HP 控制到一张合法攻击牌可击杀的范围。

例如：

```text
Enemy HP = 5
Strike incoming damage >= 5
```

记录出牌前：

```text
Enemy HP
Player Energy
Card RuntimeId
目标 destination
```

然后正常：

```text
Select Card
→ Select Enemy
→ Confirm
```

---

## 12.25 检查 Damage lethal visual

Damage active window 应先表现：

```text
Enemy HPAfter = 0
Enemy.bDead 的正式历史值尚由 reducer 按 Record 推进
```

但这一步不能提前显示 Victory terminal overlay。

如果 Damage 一开始就弹 Victory：

```text
Scenario D = FAIL
```

---

## 12.26 检查 lethal 后 followups

如果该攻击会产生：

```text
StatusChanged
BlockChanged
其他当前合法 followup
```

它们仍按 Envelope 顺序继续播放。

不要因为目标已经死亡而由 Blueprint 自行跳过已经被 Gameplay commit 的 followup Record。

---

## 12.27 检查 PlayArea retirement 在 Victory 前完成

确认：

```text
CardZoneChanged(PlayArea → Destination)
```

先完成。

此时：

```text
PlayedCardWidget 被正确清理
```

然后才进入：

```text
Victory
```

终局 overlay 不应覆盖一个仍悬在 PlayArea 的临时卡牌。

---

## 12.28 检查 Victory active / formal transition

Victory active window：

```text
Overlay_Terminal 显示胜利状态
输入不可继续提交 Gameplay Request
```

exact-token completion 后：

```text
WorkingSnapshot → Victory
ViewModel.BattleState = Victory
ViewModel.Outcome = Victory
正常 HUD rebuild 仍保持 Victory overlay
```

不得：

```text
Victory Visible
→ Notify
→ Overlay Collapsed
→ ViewModel rebuild
→ Victory Visible
```

如果有一次闪回，FAIL。

最终：

```text
InteractionState = Terminal
```

Scenario D 通过：

```text
Scenario D = PASS
```

---

# Scenario E：Defeat / ResolutionFault / PresentationUnavailable 隔离

## 12.29 为什么 Scenario E 同时检查三种结果

本节最后需要证明：

```text
正常 Gameplay Defeat
Framework/GamePlay ResolutionFault
PresentationUnavailable
```

三者不会互相伪装。

其中：

```text
Defeat / ResolutionFault
= Terminal Record

PresentationUnavailable
= Presentation 层 fail-safe
```

---

## 12.30 E1：Defeat

构造玩家低 HP，让真实 Enemy attack 致死。

预期链至少类似：

```text
相关 EndTurn Records
→ Enemy Damage
→ followups
→ Defeat
```

Defeat 必须是该 terminal Envelope 的最后一条 terminal Record。

Damage 先显示：

```text
Player HPAfter = 0
```

随后最后才：

```text
Defeat
```

完成后：

```text
ViewModel.BattleState = Defeat
ViewModel.Outcome = Defeat
InteractionState = Terminal
Overlay_Terminal 保持 Defeat
```

不得在 Damage 阶段提前显示 Defeat。

---

## 12.31 E2：ResolutionFault

使用项目当前已有的可重复故障构造方式触发真实：

```text
ResolutionFault Record
```

不要通过 Blueprint 手工调用 terminal event 伪造。

确认 payload 至少满足：

```text
Reason 非空
ExecutedActionCount >= 0
```

视觉显示必须明确属于：

```text
Resolution Faulted
```

completion 后：

```text
ViewModel.BattleState = ResolutionFaulted
ViewModel.Outcome = ResolutionFaulted
InteractionState = Terminal
```

---

## 12.32 E3：PresentationUnavailable 隔离

这一步只验证边界，不要求为了测试去破坏 Gameplay 数据。

使用当前项目已经存在的 Presentation unavailable / fail-safe 触发方式。

预期：

```text
ViewModel.InteractionState
= PresentationUnavailable
```

并显示 Presentation 层不可用信息。

必须确认：

```text
没有生成 ResolutionFault Record
没有把 Outcome 写成 ResolutionFaulted
没有把 Presentation failure 伪装成 Gameplay failure
```

三者隔离全部成立：

```text
Scenario E = PASS
```

---

# 第十二节 F：跨 Scenario 的统一 Token / Lifecycle 检查

## 12.33 每条异步 Record 都只能完成一次

在 A-E 的日志中检查：

```text
每个 PresentationSequence
→ 最多一个有效 completion
```

如果 stale timer 后来又触发一次 Notify，Controller 虽然有 exact-token 防护，也说明 Blueprint 生命周期没有完全清理，不能直接忽略。

要求：

```text
正常 timer completion 后对应 TimerHandle 被清理
Cancel 后 TimerHandle 被清理
旧 callback 不再改变当前 visual
```

---

## 12.34 Input unlock 时机

在队列仍有未完成 Record 时：

```text
不要允许用户开始下一次 Gameplay Request
```

尤其 Scenario C 里：

```text
EnemyTurn 尚未播放完
Draw 尚未播放完
```

时不能提前恢复正常选牌输入。

只有 Controller 已 catch up，ViewModel 正式刷新 live input bindings 后，才应回到：

```text
Idle
```

或目标选择等正常输入状态。

---

## 12.35 最终 HUD 与 FinalSnapshot 一致

每个非 Cancel Scenario 结束后，用可观察字段逐项比对最终 HUD：

```text
Player HP
Player Block
Player Statuses
Enemy HP
Enemy Block
Enemy Statuses
Energy
HandCards RuntimeId / 顺序
DrawCount
DiscardCount
ExhaustCount
Outcome
BattleState
```

最终 HUD 不允许保留任何只属于上一条 active Record 的 transient After overlay/widget。

---

# 第十二节 G：正式验收记录模板

## 12.36 每个 Scenario 都写入结果

建议在正式 validation log 中记录：

```text
UI-A2E End-to-End Acceptance

Scenario A - ordinary nonlethal card chain
Result: PASS / FAIL
Record order:
...
Visual transition:
...
Final state:
...

Scenario B - status lifecycle
Result: PASS / FAIL
RuntimeSequence:
...
Creation / Increase / Reduction / Removal:
...

Scenario C - EndTurn / EnemyTurn / next Turn
Result: PASS / FAIL
Record order:
...
Shuffle exercised: YES / NO
Final Idle/Input unlock:
...

Scenario D - lethal Victory
Result: PASS / FAIL
Record order:
CardPlayed → Damage → ... → CardZoneChanged → Victory
Terminal flashback: NO / YES

Scenario E - Defeat / ResolutionFault / PresentationUnavailable isolation
Defeat: PASS / FAIL
ResolutionFault: PASS / FAIL
PresentationUnavailable isolation: PASS / FAIL
```

不要只写：

```text
A2E looks good
```

必须保留足够证据让以后能判断究竟测试过哪些分支。

---

## 12.37 本节总 PASS 条件

只有同时满足以下条件，才能把本节记为 PASS：

```text
[ ] Scenario A PASS
[ ] Scenario B PASS
[ ] Scenario C PASS
[ ] Scenario C 中 DeckShuffled 已真实 exercised，或已补跑等价 full-chain shuffle case
[ ] Scenario D PASS
[ ] Scenario E Defeat PASS
[ ] Scenario E ResolutionFault PASS
[ ] Scenario E PresentationUnavailable isolation PASS
[ ] 所有异步 Record exact-token completion 正常
[ ] 无 stale timer 可见副作用
[ ] 无 Before→After→Before→After flashback
[ ] 无重复 Widget / duplicate Record visual
[ ] 无长期 Resolving
[ ] 输入只在 Controller catch-up 后解锁
[ ] 非终局 Scenario 最终回 Idle
[ ] terminal Scenario 最终保持 Terminal
[ ] 最终 HUD 与 FinalSnapshot 一致
```

任一项失败：

```text
UI-A2E End-to-End Acceptance = FAIL
```

先修复对应 Record/lifecycle，再重新跑受影响 Scenario。

---

## 12.38 本节完成后的状态

如果全部通过，可以记录：

```text
UI-A2E Unified Blueprint Playback
= IMPLEMENTED

UI-A2E PIE End-to-End Acceptance
= VALIDATED
```

但此时仍不要直接宣布：

```text
UI-A2E SEALED
UI-A2 SEALED
```

还需要最后一节做：

```text
验证文档收口
Saved Blueprint Snapshot 更新
C++/Blueprint 边界复核
剩余 NOT WIRED 标记清零
最终 Seal checklist
```

下一节：

```text
第十三节：UI-A2E 最终文档收口与 Seal
```
