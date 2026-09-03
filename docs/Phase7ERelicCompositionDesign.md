# Phase 7E —— 遗物反应组合化设计

日期：**2026-09-03**

状态：**设计草案 / 尚未授权实施**

> 本文是 Phase 7E 的候选设计文档，用于在 Phase 7A–7D 已完成、验证并封板之后，讨论遗物 Trigger/Reward 的最小组合化重构。本文当前只定义设计、边界和验收条件；在用户明确批准之前，不授权修改 Gameplay C++，也不重新打开已封板的 7C/7D 行为合同。

---

## 1. 背景与动机

Phase 7A–7D 已经建立并验证：

```text
URelicData      = 不可变遗物定义
URelicInstance  = 战斗内可变运行时状态
URelicContainer = 战斗内权威遗物集合

BattleEventDispatcher
→ 收集 Status / Relic Trigger 候选
→ CanReact 只读判断
→ Priority → RuntimeSequence → LocalTriggerIndex 确定顺序
→ BuildReactions 构造 reaction Actions
→ 原子、有序插入 BattleActionQueue
```

当前日晷链路为：

```text
FDeckShuffledEvent
→ USundialTrigger
   - ShufflesRequired = 3
   - EnergyGain = 2
→ USundialAdvanceAction
→ Counter 0 → 1 → 2 → 0
→ UGainEnergyAction(+2)
```

这套实现是正确且已验证的。`USundialTrigger` 没有检查 `RelicId`，也不直接修改 Gameplay；`USundialAdvanceAction` 在构造时冻结 `RequiredShuffles / EnergyGain`，执行时不回查 `RelicData.Triggers[]`。

因此 7E 的出发点不是“修复 7C 的架构错误”，而是解决第二类真实需求出现后暴露的重复：

```text
事件发生若干次
→ 推进遗物 Counter
→ 达到阈值后 Counter 重置
→ 执行一组奖励 Actions
```

目标是让新的同类遗物通过“可复用 Trigger + 可复用 RelicEffect[]”组合完成，而不是继续新增遗物专属 Trigger/Action 类。

---

## 2. 7E 目标

7E 只解决一个问题：**将“计数触发”和“触发后的奖励”拆成可组合、可复用的定义对象，同时保持现有 BattleAction / BattleEvent / Presentation 契约不变。**

目标结构：

```text
URelicData
└─ Triggers[]
   └─ UDeckShuffledCountTrigger
      ├─ RequiredCount
      └─ Effects[]
         ├─ UGainEnergyRelicEffect
         └─ UGainBlockRelicEffect

BattleEvent
→ Reusable Trigger
→ UAdvanceRelicCounterAction
→ 达到阈值
→ 已冻结的 RewardActions[]
→ BattleActionQueue
```

日晷迁移完成后应成为纯组合配置：

```text
DA_Relic_Sundial

Triggers[0]
= UDeckShuffledCountTrigger

RequiredCount = 3

Effects[0]
= UGainEnergyRelicEffect
Amount = 2
```

迁移成功后不再需要日晷专属：

```text
USundialTrigger
USundialAdvanceAction
```

但这两个类只能在迁移验证完成后删除，不能先删再重建行为。

---

## 3. 明确非目标

7E 不做以下工作：

```text
UGenericEventTrigger / 万能事件枚举 Trigger
Condition DSL / 条件表达式系统
Universal Effect Context
persistent Trigger Registry
任意 key/value Relic runtime state bag
CardPlayed / AttackPlayed / TurnStarted 等新 BattleEvent
UDrawCardsRelicEffect
RelicCounterChanged Presentation Record
RelicTriggered Presentation Record
A3 对遗物反应的预测
7D UI / Tooltip / Frozen DTO 重构
run-level Relic persistence
GAS 迁移
```

现有 `BattleEvent` 只有 `TurnEnded` 和 `DeckShuffled`。7E 第一版继续只使用已经存在并验证的 `FDeckShuffledEvent`，不为了证明组合化而同时扩展 Event 系统。

第一版只实现 `UGainEnergyRelicEffect` 与 `UGainBlockRelicEffect`。这两个 Effect 已足以验证 Trigger 复用、Counter/Threshold 复用、multi-effect 顺序、Presentation participant identity 与 nested Writer propagation；`UDrawCardsRelicEffect` 留待真实需求出现后再设计。

---

## 4. 保持封板的前置合同

7E 必须继续遵守：

```text
BattleAction / BattleActionQueue 是 Gameplay mutation authority
BattleEvent 是 commit 后的 immutable fact
Trigger 只负责只读 eligibility + Action builder
Trigger 不直接修改 Counter / Energy / Block
reaction order = Priority → RuntimeSequence → LocalTriggerIndex
Dispatcher 最终 reaction batch 原子、有序插入
Action 不 pump Queue
Gameplay 与 Presentation 是独立时间线
A2 Historical playback / FinalSnapshot 时序不变
7D Relic Read / Frozen / Native UI 零语义修改
```

特别是日晷现有可观察行为必须完全保持：

```text
真实洗牌：Counter 0 → 1 → 2 → 0
第三次真实洗牌：+2 Energy
setup shuffle 不计数
A2 播放 EnergyChanged(+2) 时仍显示历史 Counter=2
Envelope.FinalSnapshot 后显示 Counter=0
UI 永远只显示 0 / 1 / 2，不显示 /3
```

---

## 5. `URelicEffect`：独立于 `UCardEffect`

不复用 `UCardEffect`。

原因是 `UCardEffect` 已经承担卡牌专属语义：

```text
FCardPlayContext
Card Source / Target
EffectIndex
Description Preview
BuildImmediatePreviewOperations
A3 Preview
```

强行复用会制造伪造 Card Context，并把遗物系统耦合到卡牌 Preview。

7E 新增：

```cpp
UCLASS(Abstract, EditInlineNew, DefaultToInstanced)
class SLAYTHESPIREDEMO_API URelicEffect : public UObject
{
    GENERATED_BODY()

public:
    virtual bool BuildActions(
        const FRelicEffectContext& Context,
        TArray<UBattleAction*>& OutActions
    ) const PURE_VIRTUAL(URelicEffect::BuildActions, return false;);
};
```

`bool` 用于区分：

```text
false = 构建失败，当前 reaction 必须 fail-closed
true  = Effect 配置和构建过程有效
```

接口允许单个 Effect 在特殊情况下合法产出 0 个 Action，但 7E 的 `UDeckShuffledCountTrigger` 要求最终聚合后的 `PreparedRewardActions` 非空；最终为空则不创建 CounterAction。

---

## 6. `FRelicEffectContext` 最小边界

第一版不建立万能上下文，只提供当前真实 Effect 所需的信息：

```cpp
struct FRelicEffectContext
{
    URelicInstance* Relic = nullptr;
    ABattleManager* Battle = nullptr;

    ACombatant* Owner = nullptr;
    FName OwnerPresentationId = NAME_None;

    UObject* ActionOuter = nullptr;
};
```

### 6.1 为什么只有 `Owner / OwnerPresentationId`

Phase 7 当前遗物由 `Battle->PlayerRelicContainer` 持有，因此 7E 明确规定：

```text
Relic Gameplay Owner = Battle->Player
```

当前 `FTriggerRuntimeSource::FromRelic()` 不设置 `CombatantOwner`，所以 `FTriggerContext::GetOwner()` 对 Relic 仍为 null。7E 不修改这个已封板的 Trigger-source 语义，也不让每个 Effect 自己猜 Owner。

`UDeckShuffledCountTrigger::BuildReactions()` 创建 Effect Context 时统一解析：

```cpp
URelicInstance* Relic = Context.GetRelicSource();
ABattleManager* Battle = Context.GetBattle();
ACombatant* Owner = IsValid(Battle) ? Battle->Player.Get() : nullptr;

FName OwnerPresentationId = NAME_None;
const bool bOwnerPresentationIdResolved =
    IsValid(Battle)
    && IsValid(Owner)
    && Battle->TryResolveCombatantPresentationId(
        Owner,
        OwnerPresentationId);
```

如果解析失败，`OwnerPresentationId` 保持 `NAME_None`。`bOwnerPresentationIdResolved` 只用于构建阶段判断；只有真正依赖 participant identity 的 Effect 才因此构建失败。`UGainEnergyRelicEffect` 不依赖它，`UGainBlockRelicEffect` 依赖它。

### 6.2 第一版不提供通用 Target

7E 当前没有一个统一的 Relic Target 语义，因此不提前加入：

```text
Target
SourcePresentationId
TargetPresentationId
FBattleEvent*
```

未来只有出现真实需求，例如“根据事件中的受击者/攻击者产生效果”时，才扩充 Context。

---

## 7. Presentation participant identity 是 correctness 契约

`UGainBlockAction` 在 Gameplay Block 已 commit 后，如果存在 `FPresentationRecordWriter` 但 `SourcePresentationId / TargetPresentationId` 不可信，会调用：

```text
Writer.InvalidateCurrentResolution()
```

这不会回滚已经 commit 的 Gameplay Block，也不是 `BattleActionQueue::RequestResolutionFault()`；但会使当前 committed Presentation resolution 失效。

因此 `UGainBlockRelicEffect` 必须在 Build 阶段设置 participant IDs。

当前 7E 的 self-block 语义：

```text
Source = Owner
Target = Owner

SourcePresentationId = OwnerPresentationId
TargetPresentationId = OwnerPresentationId
```

如果 `Owner` 或 `OwnerPresentationId` 无效：

```text
UGainBlockRelicEffect::BuildActions() = false
→ 整个 reaction 放弃
→ Counter 不前进
```

不允许在 Effect 内重新从 HUD / ViewModel / mutable Presentation state 查询 identity。

---

## 8. `UDeckShuffledCountTrigger`

第一版可复用 Trigger：

```cpp
UCLASS(EditInlineNew, DefaultToInstanced)
class SLAYTHESPIREDEMO_API UDeckShuffledCountTrigger : public UBattleTrigger
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="1"))
    int32 RequiredCount = 1;

    UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly)
    TArray<TObjectPtr<URelicEffect>> Effects;
};
```

### 8.1 `CanReact`

只读判断：

```text
Event.TryGet<FDeckShuffledEvent>() 成功
Deck 有效
Relic runtime source 有效
Battle 有效
Relic.GetBattle() == Battle
Event Deck 是当前 authoritative DeckRuntime
RequiredCount > 0
Effects 配置有效
```

不检查：

```text
RelicId == Sundial
Card identity
DrawAction identity
RetryDraw identity
```

7E **不在 `CanReact()` 或 `BuildReactions()` 增加 PlayerRelicContainer membership 检查**。这是有意保持现有 sealed Sundial eligibility/build 语义的最小迁移。

当前正常路径中，Dispatcher 已经同步地从当前 `PlayerRelicContainer` 枚举 Relic candidate，并在同一调用栈内依次完成 `CanReact → sort → BuildReactions → final batch insertion`，中间没有 yield 点。因此在 `BuildReactions()` 再检查 membership 只是冗余防御，不能覆盖 `Build → Execute` 的运行时窗口。

真正覆盖该窗口的权威 membership 重验证只保留在：

```text
UAdvanceRelicCounterAction::Execute
→ 确认 Relic 当前仍属于 Battle.PlayerRelicContainer
```

### 8.2 `BuildReactions`

职责：

```text
验证 Relic / Battle / Player ownership
建立 FRelicEffectContext
↓
按 Effects[] 声明顺序逐个 Build
↓
任一 Effect Build 失败
→ 整个 reaction 放弃
→ 不创建 CounterAction
→ Counter 不前进
↓
所有 Effect 成功
→ 得到 PreparedRewardActions[]
→ 必须至少包含一个 Action
↓
创建 UAdvanceRelicCounterAction
↓
调用 Initialize(Relic, RequiredCount, PreparedRewardActions)
↓
Initialize 返回 false
→ 整个 reaction fail-closed
→ 不加入 OutActions
↓
Initialize 返回 true
→ 冻结 Relic / RequiredCount / PreparedRewardActions[]
→ OutActions.Add(CounterAction)
```

Trigger 自身不修改 Counter，也不直接执行 reward。

---

## 9. Effect 构建失败采用 fail-closed

7E 明确选择：

```text
任一 Effect 构建失败
→ 整个 Trigger reaction 放弃
→ 已构造但尚未入队的局部 RewardActions 不使用
→ 后续 Effect 不再构建
→ Counter 不前进
→ 不产生部分奖励
```

例如定义：

```text
每 3 次洗牌：
1. GainBlock(6)
2. GainEnergy(1)
```

如果 `GainBlock` 构建失败，不能退化成：

```text
每 3 次洗牌：只 GainEnergy(1)
```

因为那会静默改变 authored Relic 语义。

推荐构建方式：

```cpp
TArray<UBattleAction*> PreparedRewards;

for (const URelicEffect* Effect : Effects)
{
    TArray<UBattleAction*> EffectActions;

    if (!IsValid(Effect)
        || !Effect->BuildActions(EffectContext, EffectActions))
    {
        return; // fail-closed
    }

    PreparedRewards.Append(EffectActions);
}

if (PreparedRewards.Num() == 0)
{
    return;
}
```

---

## 10. Reward Action 在 BuildReactions 阶段 eager build

7E 刻意选择：

```text
BuildReactions 时立即创建 RewardActions
```

而不是：

```text
CounterAction::Execute()
→ 回查 RelicData
→ 找 Trigger
→ 遍历 Effects
→ 临时 Build RewardActions
```

这样可以保证：

```text
BattleEvent dispatch
→ 本次完整 reaction intent 已冻结
→ 后续 Execute 不重新解释 DataAsset
→ 不受 definition 再查询 / Trigger 再定位影响
```

代价是非阈值事件也会构造 RewardActions：

```text
第 1 次洗牌：构造 RewardActions，但不入队
第 2 次洗牌：构造 RewardActions，但不入队
第 3 次洗牌：新的 RewardActions 真正入队
```

这是 7E 有意接受的成本。当前规模下 UObject 创建开销可忽略，优先保证冻结意图和确定性。

---

## 11. Reward Action Outer 是硬契约

所有 `URelicEffect::BuildActions()` 必须：

```cpp
NewObject<ConcreteAction>(Context.ActionOuter)
```

并且 `Context.ActionOuter` 必须严格等于目标 `UBattleActionQueue`。

原因：`BattleActionQueue::ValidateBatchForInsertion()` 明确要求：

```text
Action->GetOuter() == this Queue
```

否则 batch insertion 失败。

### 11.1 提前发现错误

Dispatcher 的 local batch 只包含 `UAdvanceRelicCounterAction`，看不到其内部的 `RewardActions[]`。如果某个 Effect 使用错误 Outer，只依赖 Queue 最终检查，会把错误延迟到 Counter 达到阈值时才暴露。

因此 `UAdvanceRelicCounterAction::Initialize()` 必须返回 `bool`，并在初始化阶段对 prepared rewards 做一次静态验证：

```cpp
bool Initialize(
    URelicInstance* InRelic,
    int32 InRequiredCount,
    const TArray<UBattleAction*>& InRewardActions);
```

静态验证至少覆盖：

```text
Relic 有效
RequiredCount > 0
PreparedRewardActions 非空
每个 RewardAction：
- IsValid
- 未 Finished
- GetOuter() == CounterAction.GetOuter()
- batch 内没有重复 Action 指针
```

如果任一项失败：

```text
Initialize(...) = false
→ 不保存部分初始化状态
→ UDeckShuffledCountTrigger 不把 CounterAction 加入 OutActions
→ 当前 reaction fail-closed
→ Counter 不前进
```

这里不检查 Queue 动态状态：

```text
当前是否 pending
当前 CurrentAction
Queue fault 状态
QueueEmpty broadcast 状态
```

这些继续由真正 enqueue 时的 `BattleActionQueue` 权威校验。

---

## 12. `UAdvanceRelicCounterAction`

替代 `USundialAdvanceAction` 的可复用 Counter/Threshold Action。

初始化接口：

```cpp
bool Initialize(
    URelicInstance* InRelic,
    int32 InRequiredCount,
    const TArray<UBattleAction*>& InRewardActions);
```

只有 `Initialize()` 返回 `true` 的 CounterAction 才能进入 Trigger 的 `OutActions`。

概念状态：

```cpp
UPROPERTY(Transient)
TObjectPtr<URelicInstance> Relic;

int32 RequiredCount = 0;

UPROPERTY(Transient)
TArray<TObjectPtr<UBattleAction>> RewardActions;
```

`URelicInstance::SetCounterFromAction()` 继续保持非 public，只把 friend 从：

```text
USundialAdvanceAction
```

迁移为：

```text
UAdvanceRelicCounterAction
```

Trigger / Effect / UI 均不能直接写 Counter。

### 12.1 Execute —— 未达到阈值

```text
验证 Queue / Relic / Battle / PlayerRelicContainer
验证 Relic 仍属于当前 Container
验证 RequiredCount > 0
↓
CounterBefore = Relic.Counter
↓
CounterBefore < RequiredCount - 1
→ Counter = CounterBefore + 1
→ Finish
```

### 12.2 Execute —— 达到阈值

必须先确认 dependent work 能成功入队，再 reset Counter：

```text
CounterBefore >= RequiredCount - 1
↓
把 CounterAction 自身的 PresentationRecordWriter
传播给全部 RewardActions
↓
Queue.AddBatchToFrontPreserveOrder(RewardActions)
↓
如果插入失败：
    RequestResolutionFault
    Counter 保持原值
    Finish

如果插入成功：
    Counter = 0
    Finish
```

禁止：

```text
先 Counter = 0
→ 再尝试 enqueue
```

否则 dependent Action 插入失败时会留下错误的已重置 Counter。

---

## 13. Reward 顺序与原子插入

多个 Effect 必须保持声明顺序。

例如：

```text
Effects[0] = GainBlock(6)
Effects[1] = GainEnergy(1)
```

构建：

```text
PreparedRewardActions
= [GainBlockAction, GainEnergyAction]
```

执行必须使用：

```cpp
Queue->AddBatchToFrontPreserveOrder(PreparedRewardActions);
```

最终执行顺序：

```text
GainBlockAction
→ GainEnergyAction
```

禁止循环逐个 `AddToFront()`，否则声明顺序可能反转。

---

## 14. Presentation Writer 传播职责

Dispatcher 只给它直接收到的 local reaction Actions 设置 Writer：

```text
Dispatcher
→ UAdvanceRelicCounterAction.SetPresentationRecordWriter(...)
```

它不会自动递归到：

```text
CounterAction.RewardActions[]
```

因此 7E 正式规定：

```text
URelicEffect
负责：
- 构造 RewardAction
- 冻结 Gameplay 参数
- 配置必要的 participant identity

UAdvanceRelicCounterAction
负责：
- 继承 Dispatcher Writer
- 在真正 enqueue 前把自己的 Writer
  传播给每个 RewardAction
```

概念执行：

```cpp
for (UBattleAction* RewardAction : RewardActions)
{
    RewardAction->SetPresentationRecordWriter(
        GetPresentationRecordWriter());
}

Queue->AddBatchToFrontPreserveOrder(RewardActions);
```

Effect 不负责 Writer propagation，避免未来某个 Effect 忘记设置 Writer 后静默丢失 committed Presentation。

---

## 15. 第一批 `URelicEffect`

### 15.1 `UGainEnergyRelicEffect`

配置：

```cpp
int32 Amount = 1;
```

第一版要求：

```text
Amount > 0
Battle 有效
ActionOuter 有效
```

Build：

```text
NewObject<UGainEnergyAction>(ActionOuter)
→ Initialize(Battle, Amount)
→ OutActions.Add(Action)
```

不复制 Energy mutation；继续复用 7C sealed 的：

```text
BattleEnergyMutation::TryGain
UGainEnergyAction
```

### 15.2 `UGainBlockRelicEffect`

配置：

```cpp
int32 Amount = 1;
```

第一版只支持“玩家遗物给玩家自己获得 Block”：

```text
Source = Context.Owner
Target = Context.Owner
BaseAmount = Amount

SourcePresentationId = Context.OwnerPresentationId
TargetPresentationId = Context.OwnerPresentationId
```

Build 前必须验证：

```text
Owner 有效
OwnerPresentationId != NAME_None
ActionOuter 有效
Amount > 0
```

然后：

```text
NewObject<UGainBlockAction>(ActionOuter)
→ Initialize(Owner, Owner, Amount)
→ SetPresentationParticipantIds(
      OwnerPresentationId,
      OwnerPresentationId)
→ OutActions.Add(Action)
```

继续复用现有 Block Pipeline 和 `UGainBlockAction`，不建立遗物专属 Block mutation。

---

## 16. DataValidation

7E 应为新 definition objects 增加最小内容校验：

### `UDeckShuffledCountTrigger`

```text
RequiredCount > 0
Effects.Num() > 0
每个 Effect 有效
```

### `UGainEnergyRelicEffect`

```text
Amount > 0
```

### `UGainBlockRelicEffect`

```text
Amount > 0
```

不让 `URelicData::IsDataValid()` 通过 `Cast<UDeckShuffledCountTrigger>` 理解具体 mechanic。

`CounterDisplayMax` 与 `RequiredCount` 仍是两个明确用途不同的 authored 值：

```text
RequiredCount       = Gameplay threshold
CounterDisplayMax   = Presentation metadata
```

7E 不为了消除重复数字重新打开 7D Frozen/UI contract。内容作者负责保持一致；未来只有出现实际维护问题时再设计更通用的 cross-definition validation。

---

## 17. 第二个组合化验证案例

仅加入 Abacus 风格：

```text
DeckShuffled
→ GainBlock(6)
```

只能证明第二个 Relic 可以复用 `FDeckShuffledEvent` 和 source-neutral Dispatcher，不足以证明 Counter/Reward composition 成功。

7E 使用一个测试定义（不要求制作正式生产资产）：

```text
TestCompositeRelic

每洗牌 3 次：
→ 获得 6 Block
→ 获得 1 Energy
```

配置必须只有：

```text
UDeckShuffledCountTrigger
RequiredCount = 3

Effects[0]
= UGainBlockRelicEffect(6)

Effects[1]
= UGainEnergyRelicEffect(1)
```

严格禁止新增：

```text
UTestCompositeRelicTrigger
UTestCompositeRelicAction
```

该案例必须证明：

```text
Counter 0 → 1 → 2 → 0
阈值前没有 RewardAction 执行
阈值时 GainBlock(6) 先执行
随后 GainEnergy(1)
A2 BlockChanged participant IDs 完整可信
A2 EnergyChanged payload 正确
没有 Presentation resolution invalidation
不需要遗物专属 C++ 类
```

---

## 18. Sundial 迁移策略

不能直接删除旧实现。

建议按以下顺序迁移：

```text
1. 新增 URelicEffect / FRelicEffectContext
2. 新增 GainEnergyRelicEffect / GainBlockRelicEffect
3. 新增 UAdvanceRelicCounterAction
4. 新增 UDeckShuffledCountTrigger
5. 新增 focused 7E tests
6. 使用测试定义验证 multi-effect composition
7. 迁移 Phase7.Sundial 测试 fixture / 测试代码 / 测试定义：
   - 不再实例化 USundialTrigger / USundialAdvanceAction
   - 改用 UDeckShuffledCountTrigger + UGainEnergyRelicEffect
   - 保留原有可观察行为断言
8. 运行“迁移后的 Phase7.Sundial 行为回归”：
   - 这里的“回归”指原 observable behavior 全部保持
   - 不是继续运行仍依赖旧 Sundial C++ 类型的测试路径
   - 同时运行 Phase7.EnergyGain / Phase7.RelicPresentation 必要回归
9. 生产 DA_Relic_Sundial 改用新 Trigger + GainEnergyRelicEffect
10. 删除 USundialTrigger / USundialAdvanceAction，以及测试中的旧类引用
11. 再跑一次最小必要回归
```

在步骤 7 完成之前，旧 Sundial 测试/生产路径仍可用于对照；步骤 7 之后，`Phase7.Sundial` 这个测试组名可以保留，但其 fixture 和实现必须已经迁移到新组合路径。

---

## 19. 7D 不应发生任何语义修改

7E 迁移后仍然使用同一个：

```text
URelicInstance::Counter
```

因此以下全部保持不变：

```text
FRelicReadView
FBattleReadSnapshot.Relics
RelicPresentationSnapshot::TryFreeze
FBattleHUDRelicView
FPresentationStateSnapshot.Player.Relics
UBattleHUDViewModel.Player.Relics
UBattleRelicStripWidget
UBattleRelicWidget
UBattleRelicTooltipWidget
```

7E 不新增：

```text
RelicCounterChanged Record
RelicTriggered Record
```

历史播放继续遵守 7D sealed contract：

```text
A2 Envelope active
→ Relic counter 维持上一份 completed historical snapshot

Envelope 完成
→ Apply FinalSnapshot
→ Counter 显示精确 committed current state
```

如果实现过程中需要修改 7D Frozen/UI 才能让组合化工作，应视为设计越界并停止实现，而不是顺手扩展 7D。

---

## 20. 错误处理合同

### 20.1 Effect build-time failure

```text
任一 Effect BuildActions 返回 false
→ 整个 reaction 放弃
→ Counter 不前进
→ 不执行部分 reward
→ 不触发 Queue ResolutionFault
→ 记录明确错误日志
```

这是 definition / reaction intent 无法可靠构造，不是已进入 Queue 后的 resolution integrity failure。

### 20.2 Prepared reward 静态验证失败

```text
Invalid Action
Finished Action
错误 Outer
重复 Action 指针
非法 RequiredCount
空 RewardActions
```

都应在 `UAdvanceRelicCounterAction::Initialize(...)` 阶段拒绝：

```text
Initialize(...) = false
→ Trigger 不把 CounterAction 加入 OutActions
→ 当前 reaction fail-closed
→ Counter 不前进
→ 不触发 Queue ResolutionFault
```

不把这些 definition / frozen-intent 错误拖到 threshold 执行时。

### 20.3 threshold 时 Queue 插入失败

```text
AddBatchToFrontPreserveOrder 失败
→ RequestResolutionFault
→ Counter 不 reset
→ Finish
```

这是 resolution integrity failure。

### 20.4 RewardAction 自身执行时 fail-soft

如果 reward batch 已经成功插入，Counter 已 reset；后续某个 RewardAction 按自己的既有合同合法 fail-soft 时，不回滚 Counter，也不回滚前序 RewardAction。

---

## 21. 自动化验收范围

7E 至少新增 focused tests，覆盖：

```text
A. DeckShuffledCountTrigger
- 非 DeckShuffled 不响应
- 非 authoritative Deck 不响应
- setup shuffle 不参与
- RequiredCount 非法配置拒绝
- CanReact / BuildReactions 不新增 PlayerRelicContainer membership 分支

B. Counter
- 0 → 1 → 2 → 0
- 非阈值不执行 reward
- threshold enqueue 成功后才 reset
- enqueue 失败时 counter 保持原值并 fault
- Execute 对当前 PlayerRelicContainer membership 做权威重验证

C. Effect composition
- GainEnergyRelicEffect 产生正确 GainEnergyAction
- GainBlockRelicEffect 产生正确 self-block Action
- multi-effect 保持声明顺序
- 任一 Effect build 失败时 whole reaction fail-closed
- final prepared reward batch 为空时 reaction 放弃

D. Outer / frozen intent
- RewardAction Outer 必须是目标 Queue
- 错误 Outer 在 Initialize 阶段返回 false
- Initialize false 时 CounterAction 不进入 OutActions
- CounterAction Execute 不遍历 RelicData.Triggers[]
- CounterAction Execute 不重新调用 RelicEffect::BuildActions

E. Presentation
- GainBlock reward participant IDs 正确
- BlockChanged record 不使 resolution invalid
- nested RewardActions 收到 CounterAction 传播的 Writer
- GainEnergy record 继续使用 sealed 7C 语义

F. 第二个组合遗物
- 每 3 次洗牌 → Block 6 → Energy 1
- 不新增遗物专属 Trigger / Action 类

G. Sundial migration regression
- Phase7.Sundial 测试 fixture / 测试代码已迁移到新 Trigger + Effect
- 原 Phase7.Sundial observable behavior 全部保持
- Phase7.EnergyGain 保持
- Phase7.RelicPresentation 3/3 保持
- 7D FinalSnapshot 2 → 0 时序保持
```

---

## 22. 验证预算

遵守项目既有验证预算：

```text
设计批准后再实现
↓
Development Editor Build 一次
↓
最小 focused Phase7E Automation 一次
↓
必要的 sealed regression 一次
↓
只有存在无法由 Automation 覆盖的玩家可见变化时才 PIE
```

7E 按设计不改变 UI，因此原则上不需要重新做 7D 全量视觉 PIE。只有实现意外触及 7D 表现时才说明设计发生越界。

---

## 23. 7E 完成判据

只有同时满足以下条件，7E 才能标记为 COMPLETE / VALIDATED / SEALED：

```text
1. URelicEffect 与 CardEffect 保持独立
2. UDeckShuffledCountTrigger 不包含 Energy / Block / Sundial 特判
3. UAdvanceRelicCounterAction 不回查 RelicData.Triggers[]
4. RewardActions 在 BuildReactions 阶段冻结
5. RewardActions 保持 Effects[] 声明顺序
6. 任一 Effect 构建失败时 whole reaction fail-closed
7. UAdvanceRelicCounterAction::Initialize 返回 bool，并在 build/init 阶段静态验证 RewardAction Outer / 重复 / finished / 空 batch / RequiredCount
8. Initialize 失败时 CounterAction 不进入 Trigger OutActions
9. nested RewardActions 正确继承 PresentationRecordWriter
10. GainBlock participant identities 完整可信
11. TestCompositeRelic 无任何遗物专属 C++
12. Sundial 测试 fixture / 测试代码和生产资产完成纯组合迁移
13. 原 7C / 7D observable behavior 无回归
14. 删除 USundialTrigger / USundialAdvanceAction 后所有 gate 仍 PASS
15. 7E 第一版未引入 UDrawCardsRelicEffect 或其他非必要 Scope 扩张
```

最终希望得到：

```text
RelicData
→ reusable Trigger
→ reusable RelicEffect[]
→ frozen BattleActions
→ authoritative Queue
```

而不是：

```text
每新增一个遗物
→ 新增一个 XXXRelicTrigger
→ 新增一个 XXXRelicAction
```

---

## 24. 当前决策状态

本文记录的设计已经明确解决：

```text
Trigger / Effect 职责边界
Effect Context 最小字段
Player Relic Owner 来源
Presentation participant identity
Reward Action Outer 合同
Initialize 失败回传与 fail-closed 落点
Eager-build 冻结边界
Writer propagation
multi-effect 顺序
fail-closed 粒度
Counter reset / Queue fault 时序
第二个真实组合验证案例
Sundial 测试代码 / fixture / 生产资产迁移顺序
CanReact / BuildReactions 与 Execute membership 权威重验证边界
7D 零语义修改边界
7E 第一版明确不实现 DrawCardsRelicEffect
```

但当前状态仍为：

```text
DESIGN DRAFT
IMPLEMENTATION NOT AUTHORIZED
```

下一步应先对本文做一次最终 design review。只有用户明确批准后，才进入 7E 实现。