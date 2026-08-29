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
→ Break StatusChanged Presentation Payload
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
