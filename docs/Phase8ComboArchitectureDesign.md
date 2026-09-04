# Phase 8 — Combo Architecture Validation

日期：**2026-09-04**

状态：**DESIGN REFINED / REVIEW PENDING / IMPLEMENTATION NOT AUTHORIZED**

Phase 7A–7F 已完成、验证并 sealed。Phase 8 不重新打开这些阶段的 Gameplay / Presentation 合同，而是验证当前 Card、Draw、Shuffle、BattleEvent、Relic Trigger、ActionQueue 和 Presentation 架构能否在真实跨系统组合中自然协作。

当前生产/调试内容中，Pommel Strike（剑柄打击）已经配置为 `Draw 2`，并已通过人工 PIE 观察到目标组合链。这份 Production PIE evidence 保留；但 **Phase 8 Automation 不再依赖生产 Pommel Strike 当前恰好为 Draw 2 的内容状态**，避免后续正式 Upgrade Foundation 把 Pommel Strike 恢复为 Base Draw 1 / Upgraded Draw 2 时破坏架构回归测试。

Phase 8 不创建持久测试用 `Pommel Strike+` 资产，也不实现 Upgrade System。

---

## 1. Goal

Phase 8 的唯一主目标：

```text
使用 authored transient Draw-2 card definition + Sundial
验证：
Card → Draw → Shuffle → Event → Relic Reaction → Energy
完整跨系统链路能够自然成立，且不存在 card/relic 组合特判。
```

Automation 目标链路：

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

Phase 8 成功的核心证据是：上述组合由独立能力通过稳定合同自然形成，而不是新增专属框架或绑定某张生产卡的 authored 数值。

---

## 2. Locked architecture rule

Phase 8 必须继续遵守：

```text
Card / Effect 不知道 Sundial
Draw / Shuffle 不知道 Sundial
Sundial 不知道测试卡定义或 Pommel Strike
Presentation 不制造 Gameplay 结果
A3 不预测未来 Shuffle / Relic reaction
```

允许的依赖方向只有：

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

用户已在真实 PIE 中把 Pommel Strike 配置为 Draw 2，并确认可以观察到：

```text
Damage
→ Draw 2
→ DrawPile 不足
→ real Shuffle
→ FDeckShuffledEvent
→ Sundial Counter advance
→ third counted Shuffle resets Counter
→ +2 Energy
→ remaining Draw continuation
```

这份人工证据继续作为 Phase 8 的 Production PIE evidence 使用。它证明当前生产 Gameplay/UI 可以观察到同一组合链，但 **它不再定义 Phase 8 Automation 的输入内容**。

因此：

```text
Production PIE evidence
→ existing Draw-2 Pommel Strike

Automation architecture evidence
→ transient authored Draw-2 card definition
```

若 8A 没有修改生产 Gameplay 路径，不重复人工 Gate。

---

## 4. Phase 8 slices

### 8A — Automated Combo Integration

新增 focused Automation，从 transient authored Card definition 的真实 Card play / CardEffect 开始，而不是手工 dispatch `FDeckShuffledEvent`，也不依赖生产 Pommel Strike 的当前 Base/Upgrade content。

Dedicated prefix：

```text
SlayTheSpireDemo.Phase8
```

建议最少测试：

```text
SlayTheSpireDemo.Phase8.Combo.Draw2Sundial
SlayTheSpireDemo.Phase8.Combo.OrderingAndContinuation
```

测试定义要求：

```text
Transient UCardData
→ ordinary authored target/cost/destination facts
→ generic Damage Effect
→ generic UDrawCardEffect(DrawCount = 2)
```

它不是特殊 Gameplay type，也不进入生产资产目录。

必须验证：

```text
Draw 2 来自真实 UDrawCardEffect
真实 UDrawCardsAction(2) 发起 bulk draw
真实 Shuffle commit
真实 FDeckShuffledEvent
Sundial Counter 只由已提交 Shuffle 推进
第三次 counted Shuffle 精确 +2 Energy
Relic reaction 先于 remaining Draw continuation
Queue 最终正常 drain
无 ResolutionFault
```

主组合测试不得直接以：

```cpp
Dispatcher->Dispatch(FBattleEvent::MakeDeckShuffled(...))
```

作为起点。

也不得通过修改生产 Pommel Strike 资产来准备测试输入。

#### Sealed ordering contract reused by Phase 8

Phase 8 不发明新的 Queue 顺序。现有 sealed Draw/Shuffle 合同已经形成：

```text
DrawCardsAction
→ first queues [immediate draws, Shuffle, RemainingDraw] continuation batch at Queue front

ShuffleDeckAction
→ commits Shuffle
→ dispatches DeckShuffled

Dispatcher
→ inserts reaction batch at Queue front
```

因此最终顺序自然为：

```text
Shuffle commit
→ DeckShuffled reactions
→ RemainingDraw continuation
→ previously pending work
```

Phase 8 Automation 只验证这个既有合同，没有新 Queue API。

### 8B — Record Production PIE Evidence

直接记录已经完成的人工 PIE 事实：

```text
Pommel Strike Draw 2 正常
DrawPile / DiscardPile 真实变化
Counter 0 → 1 → 2 → 0
第三次 counted Shuffle +2 Energy
remaining Draw continuation 正常
后续 Gameplay 可继续操作
无 Missing Class / Failed to load / crash / ResolutionFault
```

若 8A 实施没有修改生产 Gameplay 路径，不重复人工 Gate。

### 8C — Validation / Seal

建立：

```text
docs/Phase8Validation.md
```

全部 Gate 通过后：

```text
Phase 8 Combo Architecture Validation:
COMPLETE / VALIDATED / SEALED
```

然后 STOP，正式选择下一项卡牌开发能力。

---

## 5. Upgrade System relationship

**Upgrade System 仍然要做，但不属于 Phase 8。**

升级系统改为战士卡牌扩展的基础能力，与正式卡牌内容一起推进，而不是为了 Phase 8 制造一个测试用 Plus 资产，也不是等全部卡牌完成后再补。

后续卡牌开发原则：

```text
Card Expansion Foundation
├─ default single-upgrade state / resolution
├─ optional repeatable-upgrade capability
├─ first real upgraded card definitions
└─ focused validation
```

Upgrade 必须保持正交：

```text
普通卡默认使用 single-upgrade state
可重复升级不是所有卡的全局等级，而是 optional RepeatableUpgradeCapability
Upgrade System 不知道 Damage / Draw / Exhaust / Corruption 等具体能力
Damage / Draw / Exhaust 等能力也不直接判断 upgrade runtime state
Gameplay / preview / frozen presentation 从统一 typed effective card/effect view 读取升级后的事实
Presentation 通过冻结 UpgradeStateView 格式化 "+" 或 "+N"
```

模型必须允许以后扩展到：

```text
Armaments     → combat temporary upgrade through generic Upgrade action
Searing Blow  → optional repeatable count state
Run Deck      → future persistence materializes the correct single/repeatable state
```

具体设计 authority：

```text
docs/CardUpgradeFoundationDesign.md
```

Phase 8 不实现、也不验证上述 Upgrade Foundation。

---

## 6. Validation policy

继续遵守：

```text
Build once
→ smallest focused Phase8 Automation suite
→ only directly invalidated regression gates
→ reuse sticky PIE evidence when still valid
→ record evidence
→ STOP
```

多个 Automation prefix 一次性全部给用户，不逐个发送。

Phase 8 不默认运行 Phase6R / A2D5 / Shipping 等大型 aggregate gate；只有实际修改范围明确使其失效时才运行。

---

## 7. Acceptance definition

```text
[ ] Automation uses a transient authored Draw-2 card definition, not production Pommel content
[ ] no persistent test-only Pommel Strike+ asset is required
[ ] automated combo starts from real Card / Effect execution
[ ] real Shuffle commits emit the existing DeckShuffled fact
[ ] Sundial counts only those committed facts
[ ] third counted Shuffle grants exact +2 Energy
[ ] reaction ordering precedes remaining bulk-draw continuation through the existing sealed Queue pattern
[ ] Queue drains without ResolutionFault
[ ] no card/relic combination special case exists
[ ] existing Production PIE evidence remains valid as separate evidence
[ ] final evidence recorded in Phase8Validation.md
```

---

## 8. Next exact action

```text
REVIEW THIS REFINED DESIGN.

Phase 8 implementation is still NOT AUTHORIZED.
Do not build Upgrade System inside Phase 8.
After Phase 8 seal, start the separately bounded Card Expansion / Upgrade Foundation goal.
```
