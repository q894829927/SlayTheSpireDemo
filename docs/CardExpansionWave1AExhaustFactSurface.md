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
   → build immutable FCardExhaustedEvent from committed result
   → dispatch
→ otherwise
   → no CardExhausted event
```

锁定顺序：

```text
zone mutation
→ commit result says Exhaust succeeded
→ construct immutable event
→ dispatch event
```

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

### 3.3 Future targeted-exhaust reuse

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
CardInstance / exact runtime card identity
CardRuntimeId
CardId / immutable definition identity when already available from committed subject
FromZone = PlayArea
ToZone   = ExhaustPile
```

实现时优先直接携带能够唯一指向该战斗中具体 CardInstance 的 typed reference / snapshot；不要只靠 `CardId`。

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

如果现有 zone commit result 已提供足够的 exact runtime identity，则 event 从该 commit result 构建；不要在 dispatch 时重新遍历 Deck 查找卡牌。

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

事件存在本身即是本 slice 的 Gameplay fact surface。

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

默认推荐：

```text
Seeing Red
```

原因：

```text
Gain Energy
+ self Exhaust
```

`GainEnergyAction` 已存在，因此它不会额外引入 Block、selection、multi-enemy、targeted-exhaust、new trigger source 等新能力。

备选：

```text
Impervious
```

但 Wave 1A 默认只 author 一张验证卡，不为了覆盖更多内容扩大 slice。

### Seeing Red Wave 1A boundary

仅验证当前项目已支持的普通 single-upgrade model。

具体卡牌 base/upgraded 数值属于 content authoring；实现时应按项目现有 typed Base/Upgraded 字段 author，不新增第二套 upgrade representation。

卡牌应：

```text
CardType           = Skill
CardColor          = Red
DefaultDestination = Exhaust
Effects            = existing Gain Energy composition
```

如果当前项目没有可 authored 的 GainEnergy `UCardEffect`，不得为了赶 Seeing Red 在 CardId 分支里直接改 Energy。此时应把“最窄 GainEnergy CardEffect adapter”作为 Wave 1A 的必要 content adapter 单独实现，并保持它只负责构建既有 `GainEnergyAction`；不要把 Energy mutation 写进 CardData/Widget/PlayCardAction 特判。

---

## 10. Focused Automation

新增 focused suite，建议命名：

```text
SlayTheSpireDemo.CardExpansion.Wave1A.ExhaustFact
```

最低覆盖：

```text
[ ] successful PlayArea → ExhaustPile commit emits exactly one CardExhausted
[ ] event refers to the exact exhausted runtime card
[ ] event is emitted only after commit success
[ ] non-Exhaust destination emits zero CardExhausted events
[ ] failed / rejected move emits zero CardExhausted events
[ ] one self-exhaust play cannot double-dispatch
[ ] existing Discard / Removed destination behavior unchanged
[ ] event may have zero listeners without fault
```

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
[ ] commit always precedes dispatch
[ ] CardExhausted exists as a typed immutable BattleEvent payload
[ ] exact runtime card identity is preserved
[ ] only successful Exhaust commits emit the event
[ ] no double-dispatch
[ ] no CardId branch
[ ] no new targeted-exhaust capability
[ ] no selection capability
[ ] no reactive Power implementation
[ ] no Card Trigger Source implementation
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
