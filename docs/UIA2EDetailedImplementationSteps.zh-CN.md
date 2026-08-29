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
