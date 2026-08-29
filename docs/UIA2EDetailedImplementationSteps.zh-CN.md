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
