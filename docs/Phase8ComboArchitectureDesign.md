# Phase 8 — Combo Architecture Validation

日期：**2026-09-04**

状态：**DESIGN REFINED / DEFERRED / NOT A BLOCKER FOR CARD EXPANSION**

Phase 7A–7F 已完成、验证并 sealed。Phase 8 的设计继续保留，但根据 2026-09-04 的最新顺序决定，**Phase 8 暂缓，不再作为 Card Expansion / Upgrade Foundation 的前置 Gate**。

Phase 8 以后恢复时，仍用于验证 Card、Draw、Shuffle、BattleEvent、Relic Trigger、ActionQueue 和 Presentation 架构能否在真实跨系统组合中自然协作。

当前生产/调试内容中，Pommel Strike（剑柄打击）已经配置为 `Draw 2`，并已通过人工 PIE 观察到目标组合链。这份 Production PIE evidence 保留；Phase 8 Automation 仍不依赖生产 Pommel Strike 当前 authored 数值。

Phase 8 不创建持久测试用 `Pommel Strike+` 资产，也不实现 Upgrade System。

---

## 1. Deferred goal

恢复 Phase 8 时的目标仍然是：

```text
使用 authored transient Draw-2 card definition + Sundial
验证：
Card → Draw → Shuffle → Event → Relic Reaction → Energy
完整跨系统链路能够自然成立，且不存在 card/relic 组合特判。
```

Automation 目标链：

```text
Transient UCardData
├─ generic Damage Effect
└─ UDrawCardEffect(DrawCount = 2)

real Card play
→ Damage
→ UDrawCardsAction(2)
→ DrawPile 不足
→ real Shuffle commit
→ DeckShuffled Presentation Record
→ FDeckShuffledEvent
→ UDeckShuffledCountTrigger
→ UAdvanceRelicCounterAction
→ Counter 0 → 1 → 2 → 0
→ prepared UGainEnergyAction(+2)
→ remaining bulk-draw continuation
```

---

## 2. Locked architecture rule

```text
Card / Effect 不知道 Sundial
Draw / Shuffle 不知道 Sundial
Sundial 不知道测试卡定义或 Pommel Strike
Presentation 不制造 Gameplay 结果
A3 不预测未来 Shuffle / Relic reaction
```

允许依赖方向：

```text
Card definition
→ generic Effects
→ generic Actions
→ authoritative Commit
→ immutable BattleEvent
→ generic Trigger
→ reaction BattleActions
```

禁止：

```cpp
if (CardId == "PommelStrike" && RelicId == "Sundial")
if (CardId == "PommelStrike") { ForceShuffle(); }
if (RelicId == "Sundial") { /* draw-specific branch */ }
```

Automation 允许在 test module 中构造 transient `UCardData` / Effect subobjects；但必须走真实 `PlayCardAction → CardEffect → BattleAction` 路径，不能手工制造 Shuffle/Event/Counter 结果。

---

## 3. Existing Production PIE evidence

已观察：

```text
Pommel Strike Draw 2
→ real Shuffle
→ FDeckShuffledEvent
→ Sundial Counter advance
→ third counted Shuffle resets Counter
→ +2 Energy
→ remaining Draw continuation
```

这份证据继续保留，但只是 Production PIE evidence；不定义以后 Phase 8 Automation 的测试输入。

---

## 4. Deferred Phase 8 slices

### 8A — Combo Integration Validation

Automation 是验证手段，不是阶段本身的定义。

恢复时建议 focused tests：

```text
SlayTheSpireDemo.Phase8.Combo.Draw2Sundial
SlayTheSpireDemo.Phase8.Combo.OrderingAndContinuation
```

必须验证：

```text
Draw 2 来自真实 UDrawCardEffect
真实 UDrawCardsAction(2)
真实 Shuffle commit
真实 FDeckShuffledEvent
Sundial Counter 只由 committed Shuffle 推进
第三次 counted Shuffle 精确 +2 Energy
Relic reaction 先于 remaining Draw continuation
Queue 正常 drain
无 ResolutionFault
```

### 8B — Production Evidence

继续复用已有 Pommel Strike Draw-2 PIE evidence；除非后续生产 Gameplay 变化使证据失效，否则不重复人工 Gate。

### 8C — Validation / Seal

恢复后建立：

```text
docs/Phase8Validation.md
```

最终：

```text
Phase 8 Combo Architecture Validation:
COMPLETE / VALIDATED / SEALED
```

---

## 5. Sealed ordering contract reused by future Phase 8

Phase 8 不发明新 Queue 顺序。

```text
DrawCardsAction
→ first queues [immediate draws, Shuffle, RemainingDraw] at Queue front

ShuffleDeckAction
→ commits Shuffle
→ dispatches DeckShuffled

Dispatcher
→ inserts reactions at Queue front
```

最终：

```text
Shuffle commit
→ DeckShuffled reactions
→ RemainingDraw continuation
→ previously pending work
```

---

## 6. Relationship to Card Expansion / Upgrade Foundation

最新顺序已经改变：

```text
Phase 8 = deferred integration gate
Card Expansion / Upgrade Foundation = current next active goal
```

因此不再要求：

```text
Phase 8 seal
→ 才能开始 Upgrade Foundation
```

现在允许在 Phase 8 未实施的情况下，单独授权并开始：

```text
Card Expansion / Upgrade Foundation
→ first formal upgraded-card batch
→ later bounded Ironclad card/capability slices
```

Upgrade authority：

```text
docs/CardUpgradeFoundationDesign.md
```

Phase 8 的设计和 evidence 保留，以后作为集成验证使用。

---

## 7. Deferred validation policy

以后恢复 Phase 8 时仍遵守：

```text
Build once
→ smallest focused Phase8 Automation suite
→ only directly invalidated regression gates
→ reuse sticky PIE evidence when valid
→ record evidence
→ STOP
```

---

## 8. Current next action

```text
DO NOT IMPLEMENT PHASE 8 NOW.

Phase 8 is DEFERRED and is not a blocker for Card Expansion.
The next active bounded goal is Card Expansion / Upgrade Foundation.
Card implementation still requires separate explicit authorization.
```
