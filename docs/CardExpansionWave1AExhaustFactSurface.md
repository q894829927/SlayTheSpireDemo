# Card Expansion — Wave 1A Exhaust Fact Surface

日期：**2026-09-06**

状态：

```text
DESIGN LOCKED
NEXT ACTIVE SLICE
IMPLEMENTATION NOT STARTED
```

本文件是 Production Card Expansion 的第一个 dedicated authority。它只建立 **self-exhaust committed fact surface**，不把原 `IroncladCardArchitecturePlan.md` 中异质的 Wave 1 卡牌一次性实现。

上位规划：

```text
docs/IroncladCardArchitecturePlan.md
docs/IroncladCardArchitecturePlanWave1Amendment.md
```

已 sealed 前置：

```text
Phase 6UI-A / A3                     COMPLETE / VALIDATED / SEALED
Phase 7A–7F                          COMPLETE / VALIDATED / SEALED
Card Upgrade STS-Style Refactor      COMPLETE / VALIDATED / SEALED
Card Face Visual Style               COMPLETE / USER-ACCEPTED / SEALED
```

Phase 8 继续保持：

```text
DESIGN REFINED / DEFERRED / NOT A BLOCKER FOR CARD EXPANSION
```

---

## 1. Goal

当前项目已经支持：

```text
played card
→ PlayArea
→ FinishCardPlay
→ TryMovePlayAreaCardToDestinationCommit(...)
→ ExhaustPile
```

即 **打出后自我耗尽（self-exhaust）** 的 authoritative zone mutation 已存在。

当前缺口是：成功的 Exhaust commit 尚未产生可供 Gameplay 消费的 exact committed fact。

Wave 1A 目标：

```text
self-exhaust authoritative commit
→ exact FCardExhaustedEvent
→ BattleEventDispatcher
→ focused Automation
→ one real self-exhaust production card
```

核心不变量：

> **先 commit，后 dispatch；没有真实 Exhaust commit 就没有 CardExhausted event。**

---

## 2. Exhaust capability split — locked

后续开发必须区分两个不同能力。

### 2.1 Self-exhaust after play — already supported mutation

```text
UCardData::DefaultDestination = Exhaust
→ normal card play
→ PlayArea
→ FinishCardPlay
→ PlayArea → ExhaustPile commit
```

Wave 1A 只在这条既有 mutation path 上建立 committed event surface。

### 2.2 Targeted exhaust — NOT part of Wave 1A

典型消费者：

```text
Burning Pact
Fiend Fire
Second Wind
True Grit
```

它们需要“耗尽某个指定 CardInstance / 多个候选 CardInstance”的独立 authored effect/action contract。

Wave 1A 禁止为了这些未来卡牌提前实现：

```text
UExhaustCardAction for arbitrary target
selection
bulk exhaust
generic targeted-exhaust effect
```

Targeted exhaust 作为后续独立 slice 设计和验证。

`True Grit` 是混合消费者：基础行为与升级后 selection 需求不能借 Wave 1B 提前合并。Wave 1B 只建立 targeted-exhaust primitive；需要玩家选择的行为仍属于 Wave 1C。

---

## 3. Event producer boundary — locked

### 3.1 Do not dispatch from DeckRuntime

`UDeckRuntime` 继续只拥有 zone truth 和 mutation commit。

禁止：

```text
DeckRuntime
→ knows BattleEventDispatcher
→ dispatches CardExhausted itself
```

原因：这会把 Deck mutation owner 与 Event orchestration 耦合。

### 3.2 Wave 1A producer shape

沿用当前 card-play cleanup owner：

```text
FinishCardPlay / current card-play composition boundary
→ request PlayArea → configured destination commit
→ receive exact typed zone-move commit result
→ if commit succeeded AND ToZone == ExhaustPile
   → build immutable FCardExhaustedEvent from held Card + committed result
   → dispatch
→ otherwise
   → no CardExhausted event
```

锁定顺序：

```text
zone mutation
→ commit result says Exhaust succeeded
→ existing committed Presentation record when available
→ construct immutable event from held Card + the same commit result
→ dispatch event
```

这与现有 `ShuffleDeckAction` 的 sealed precedent 保持一致：Gameplay commit 是 authority；Presentation record 可以先记录同一 committed fact；BattleEvent 随后 dispatch。

绝不允许：

```text
dispatch intent
→ then attempt mutation
```

也不允许：

```text
DefaultDestination == Exhaust
→ assume success
→ dispatch without checking commit result
```

Dispatch 沿用现有 `ShuffleDeckAction` contract，显式传入当前 resolution 的 Presentation writer：

```cpp
Dispatcher->Dispatch(
    Event,
    Queue,
    Combatants,
    nullptr,
    &PresentationWriter
);
```

Wave 1A 没有 listener，但这样保证未来 Feel No Pain / Dark Embrace 等 reaction action 能自然继承当前 resolution 的 Presentation writer。

Dispatch 失败策略 — locked：

```text
successful Exhaust commit
→ Dispatch(CardExhausted) returns false
→ RequestResolutionFault(...)
→ Finish()
→ return
→ DO NOT rollback committed Exhaust
```

两类错误必须区分，不得混用同一处理：

```text
Dispatcher wiring invalid
→ fail/fault BEFORE commit

Dispatch itself fails after commit
→ fault resolution
→ committed Exhaust remains authoritative
→ never rollback
```

这与 `ShuffleDeckAction` 的 sealed 语义一致（ShuffleDeckAction.cpp:103-114）：Gameplay commit 成功即为 authority；dispatch 失败只请求 ResolutionFault，绝不回滚已 committed 的事实。

### 3.3 Dispatcher propagation — explicit battle-scoped dependency

Wave 1A 锁定 `BattleEventDispatcher` 的获取方式：**由已有 card-play composition boundary 显式解析并传入 cleanup action**，不得在 `UFinishCardPlayAction` 内通过 world / actor 搜索重新发现。

当前 `UPlayCardAction` 已有：

```text
ResolvedEventDispatcher
RawEventCombatants
```

因此实现形状应为：

```text
UPlayCardAction
→ resolves battle-scoped Dispatcher + authoritative combatant context
→ constructs UFinishCardPlayAction
→ passes Dispatcher + combatant context explicitly

UFinishCardPlayAction
→ stores only resolution-scoped references needed for dispatch
→ commits destination
→ dispatches only from committed result
```

推荐增加窄 overload，而不是让 `FinishCardPlayAction` 变成 locator：

```cpp
Initialize(
    UDeckRuntime* Deck,
    UCardInstance* Card,
    ACombatant* PresentationCardSource,
    UBattleEventDispatcher* EventDispatcher,
    const TArray<ACombatant*>& EventCombatants
);
```

既有 overload 可保留供旧测试/兼容路径使用，但 Wave 1A 的真实 self-exhaust producer 必须获得显式 battle-scoped event wiring。

禁止：

```text
GetAllActorsOfClass / world search
name-based lookup
CardId-based dispatcher routing
DeckRuntime owning Dispatcher
adding another subsystem locator to FCardPlayContext
```

若将要执行 Exhaust destination，而所需 event wiring 无效，应在 mutation 前 fail/fault；不得先成功 Exhaust commit，再因为本应已知的 Dispatcher wiring 缺失而把 committed fact 静默丢掉。

### 3.4 Future targeted-exhaust reuse

未来 targeted-exhaust action 应复用同一规则：

```text
UExhaustCardAction (future)
→ authoritative DeckRuntime commit
→ exact commit result
→ build same FCardExhaustedEvent
→ dispatch
```

Wave 1A 不要求现在抽象一个万能 Event Bus 或 universal mutation hook。若 self-exhaust 与 targeted-exhaust 出现第二个真实 producer 后需要去重，再基于两个真实 consumer 抽出最窄 helper。

---

## 4. FCardExhaustedEvent payload

Wave 1A event 必须描述 **已经发生的 exact committed fact**，而不是未来规则意图。

最低 payload：

```text
Card / already-held UCardInstance* typed reference
CardRuntimeId
CardId / immutable definition identity when already available from committed subject
FromZone = CommitResult.FromZone
ToZone   = CommitResult.ToZone
```

`FromZone / ToZone` 必须从 authoritative `FCardZoneMutationResult` 派生，不能在 Event constructor 中把 `PlayArea` 写死成通用 Exhaust 事实。

Wave 1A 当前 self-exhaust producer 的预期真实结果仍然是：

```text
FromZone == PlayArea
ToZone   == ExhaustPile
```

但这是 **当前 producer 的 committed result invariant**，不是 `FCardExhaustedEvent` payload schema 的硬编码来源。这样未来 targeted-exhaust 从 Hand / Discard 等合法来源产生同一事件时，无需改变 Event 类型。

Event 必须直接携带能够唯一指向该战斗中具体 CardInstance 的 typed reference；不要只靠 `CardId`。

不得加入尚无真实 consumer 的字段：

```text
WasSelected
WasEthereal
WasPlayed
ExhaustReason string
Listener list
Power-specific data
Sentinel-specific data
```

Event 构造来源 — locked：

```text
FCardExhaustedEvent
← already-held UCardInstance* Card
+ exact FCardZoneMutationResult

Card pointer
→ from the producer's already-held Card reference

CardRuntimeId / CardId
FromZone / ToZone
FromIndex / ToIndex if included
→ copied from CommitResult
```

`FCardZoneMutationResult` 本身不携带 `UCardInstance*`；对象引用只来自 producer 已持有的 Card 引用，其余字段从 commit result 复制。构造前必须校验一致：

```text
Card->GetRuntimeId() == CommitResult.CardRuntimeId
Card->GetCardId()    == CommitResult.CardId
```

禁止在 dispatch 时重新遍历 Deck 查找卡牌。

---

## 5. Dispatcher contract

`EBattleEventType` 增加：

```text
CardExhausted
```

并增加：

```text
FCardExhaustedEvent
FBattleEvent::MakeCardExhausted(...)
FBattleEvent::TryGet<FCardExhaustedEvent>()
```

`BattleEventDispatcher` 继续消费 generic immutable `FBattleEvent`。

Wave 1A 不要求新增任何具体 listener。

因此允许：

```text
CardExhausted event dispatched
→ zero reactions
→ resolution continues normally
```

这不是无效工作：Wave 1A 的交付物就是稳定的 **post-commit fact contract**。后续 targeted-exhaust、Feel No Pain / Dark Embrace、Sentinel 等消费者应依赖已经 sealed 的事实面，而不是各自在自身实现中重新解释 zone mutation。

---

## 6. Reactive Power boundary — explicitly deferred

以下不属于 Wave 1A：

```text
Feel No Pain
Dark Embrace
```

它们的正确形状是：

```text
any real CardExhausted committed event
→ ongoing Power / Status-like runtime trigger source
→ builds normal reaction BattleAction
```

这类“持续 Power 对任意卡耗尽事件作出反应”不应被误等同于 `CardTriggerSourceExpansion`。

`CardTriggerSourceExpansion` 主要服务于类似 Sentinel 的：

```text
this exact CardInstance
→ when this exact instance becomes the event subject
→ its own authored CardData trigger reacts
```

两者保持独立。

---

## 7. Card Trigger Source Expansion boundary — not Wave 1A

`docs/CardTriggerSourceExpansionDesign.md` 继续作为独立未来 foundation。

当前 ordering 已更新：Phase 8 不再是 Card Expansion 的前置 blocker；但这不代表 Card Trigger Source 自动获得实现授权。

Wave 1A 明确禁止：

```text
CardData Trigger authoring
CardInstance trigger-source provider
Dispatcher Deck traversal
Sentinel implementation
Card source ordering changes
```

只有真实进入 Sentinel / Card-owned trigger consumer slice 时再单独授权。

---

## 8. Authored Continuation boundary — not Wave 1A

当前 `UDrawCardsAction` 内部已有专用 continuation precedent：

```text
Draw
→ Shuffle
→ RemainingDraw
```

以及通用 Queue API：

```text
AddBatchToFrontPreserveOrder
```

但项目尚无 generic typed authored Continuation contract。

Wave 1A 不建立它。

后续 Burning Pact / Fiend Fire / Feed / Reaper 等需要真实前序 typed result 的卡牌，再按长期规划建立：

```text
primitive commit
→ typed CommitResult
→ authored stateless Continuation
→ dependent Action batch
```

---

## 9. Production validation card

Wave 1A 只选择一张依赖现有 primitive + self-exhaust 的真实生产卡。

默认验证卡：

```text
Seeing Red
```

原因：

```text
Gain Energy
+ self Exhaust
```

`UGainEnergyAction` 已存在，但当前 `Cards/Effects/` 中没有 `UGainEnergyCardEffect`。因此 Wave 1A 存在一个已确认的最窄 content-adapter 缺口。

### 9.1 GainEnergy CardEffect adapter — required and narrow

新增：

```text
UGainEnergyCardEffect
→ only resolves authored effective amount
→ only builds existing UGainEnergyAction
```

它必须沿用当前 ordinary-upgrade typed field 模式：

```text
DescriptionArgumentName = Energy
BaseAmount
UpgradedAmount
GetEffectiveAmount(bool bIsUpgraded)
```

并实现与现有 typed Effect 一致的最窄接口：

```text
BuildActions
GetPreviewArgumentNames
BuildPreviewArguments
ValidatePreviewConfiguration
```

`BuildImmediatePreviewOperations` 明确**不要求**。卡面基础动态数字由 `BuildPreviewArguments` 提供：`FBattleTextResolver::ResolveCardRichDescriptionForImmediatePreview` 先执行 `BuildCardDescriptionArguments` 构造基础卡面数字，`FImmediatePreviewOperation` 只用于覆盖随当前目标/战斗状态变化的数值（BattleTextResolver.cpp:356-385）。Seeing Red 的 “Gain 2 Energy” 由基础数字即可表达，当前没有即时 override 需求；现有 `UDrawCardEffect` 同样没有 override 该接口。不得为了接口完整制造没有真实语义的 fake preview operation。

`UGainEnergyAction::Initialize(Battle, Amount)` 不获得升级语义；升级只在 CardEffect adapter 处通过 `Context.Card->IsUpgraded()` 解析为 effective authored amount，然后把普通整数 Amount 交给现有 Action。

禁止：

```text
CardId == SeeingRed 特判
CardData 直接 mutate Energy
PlayCardAction 直接 mutate Energy
Widget / Presentation 驱动 Energy mutation
第二套 upgrade representation
```

### 9.2 Seeing Red Wave 1A boundary

具体卡牌数值按 STS 原卡 author，使用项目现有 typed Base/Upgraded 字段，不新增第二套 upgrade representation：

```text
BaseCost       = 1
UpgradedCost   = 0

BaseAmount     = 2
UpgradedAmount = 2
```

升级只改变费用 1 → 0，Energy 仍为 2。这正好体现已 sealed 的“没有 magic fallback，不变的字段也显式 author”原则：Energy 显式 `2/2`，费用显式 `1/0`。

卡牌应：

```text
CardType           = Skill
CardColor          = Red
DefaultDestination = Exhaust
Effects
└─ GainEnergyCardEffect
```

Seeing Red 只作为 production validation consumer；不得以它的 CardId 驱动任何 generic Exhaust/Event 行为。

备选 `Impervious` 不再作为默认 Wave 1A validation card，除非 Seeing Red asset authoring 出现与本 slice 无关的不可用阻塞；即便切换验证卡也不得扩大 Gameplay scope。

---

## 10. Focused Automation

新增 focused suite，建议命名：

```text
SlayTheSpireDemo.CardExpansion.Wave1A.ExhaustFact
```

最低覆盖：

```text
[ ] successful self-exhaust commit emits exactly one CardExhausted
[ ] event refers to the exact exhausted runtime card
[ ] event CardRuntimeId / CardId match the commit result
[ ] event FromZone / ToZone equal the exact commit result values
[ ] current self-exhaust path proves FromZone == PlayArea and ToZone == ExhaustPile
[ ] event is emitted only after commit success
[ ] event dispatch observes the already-committed Exhaust zone state
[ ] non-Exhaust destination emits zero CardExhausted events
[ ] failed / rejected move emits zero CardExhausted events
[ ] one self-exhaust play cannot double-dispatch
[ ] existing Discard / Removed destination behavior unchanged
[ ] event may have zero listeners without fault
```

GainEnergy adapter 覆盖：

```text
[ ] base card builds GainEnergyAction with BaseAmount
[ ] upgraded card builds GainEnergyAction with UpgradedAmount
[ ] preview argument uses the same effective authored amount
[ ] adapter contains no CardId-specific behavior
```

测试优先使用 transient test CardData 验证 fact surface，不把生产 Seeing Red 资产作为 C++ contract test 的唯一 fixture。

可复用现有：

```text
UBattleEventDispatcher::OnEventDispatchedForTesting
```

在 event observation hook 中检查 Deck/commit-derived identity，可直接证明“commit before dispatch”。若当前 Presentation writer 可用，也应沿用现有 Shuffle ordering precedent，确认对应 committed `CardZoneChanged` record 已先于 Event dispatch 存在。

如果实现修改了 existing event dispatch ordering 或 shared dispatcher code，再补跑已有相关 ordering regression；不要无原因重跑整个历史测试矩阵。

---

## 11. Production PIE acceptance

使用 Seeing Red：

```text
play Seeing Red
→ energy effect resolves normally
→ card enters / uses existing PlayArea lifecycle
→ FinishCardPlay commits Exhaust destination
→ card appears in ExhaustPile / exhaust count updates through existing presentation path
→ no duplicate card-zone transition
→ no resolution fault
```

Gameplay focused evidence同时确认：

```text
exactly one CardExhausted committed event
```

Wave 1A 不要求 UI 显示“CardExhausted event”本身；现有 Exhaust presentation 继续消费现有 card-zone transition contract。

---

## 12. Non-goals

本 slice 明确不实现：

```text
targeted exhaust
bulk exhaust
pending selection
Burning Pact full card
Fiend Fire
Second Wind
True Grit selection behavior
Shockwave / multi-enemy
Feel No Pain
Dark Embrace
Sentinel
Card Trigger Source Expansion
generic authored Continuation
Ethereal
draw-on-exhaust
block-on-exhaust
universal zone-event bus
DeckRuntime → Dispatcher dependency
CardId / DisplayName special cases
CFV redesign
Upgrade redesign
Phase 8 implementation
```

---

## 13. Acceptance / seal checklist

```text
[ ] producer boundary remains outside DeckRuntime
[ ] Dispatcher + combatant context are passed explicitly from battle/card-play composition
[ ] no world/actor search is introduced for Dispatcher discovery
[ ] commit always precedes dispatch
[ ] CardExhausted exists as a typed immutable BattleEvent payload
[ ] exact runtime card identity is preserved
[ ] FromZone / ToZone are derived from exact commit result
[ ] only successful Exhaust commits emit the event
[ ] no double-dispatch
[ ] no CardId branch
[ ] no new targeted-exhaust capability
[ ] no selection capability
[ ] no reactive Power implementation
[ ] no Card Trigger Source implementation
[ ] UGainEnergyCardEffect only adapts typed authored values to existing UGainEnergyAction
[ ] focused Automation PASS
[ ] required Build PASS if C++ changed
[ ] Seeing Red production asset authored
[ ] focused PIE PASS
[ ] Wave 1A evidence recorded before seal
```

---

## 14. Stop state

Wave 1A 完成并 seal 后停止。

后续候选顺序：

```text
Wave 1B — Targeted Exhaust Primitive
→ exact arbitrary-card / bulk exhaust mutation + commit result

Wave 1C — Selection + Targeted Exhaust Composition
→ Burning Pact / related consumers

Wave 1D — Reactive Exhaust Powers
→ Feel No Pain / Dark Embrace

Independent future foundation — Card Trigger Source Expansion
→ before Sentinel / exact CardInstance-owned trigger consumers
```

Shockwave 留到 multi-enemy capability；不强行留在 Wave 1A。

本文件不自动授权 Wave 1B/1C/1D 或 Card Trigger Source Expansion。
