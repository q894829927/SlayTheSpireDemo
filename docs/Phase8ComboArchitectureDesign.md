# Phase 8 — Combo Architecture Validation

日期：**2026-09-04**

状态：**DESIGN REVISED / REVIEW PENDING / IMPLEMENTATION NOT AUTHORIZED**

Phase 7A–7F 已完成、验证并 sealed。Phase 8 不重新打开这些阶段的 Gameplay / Presentation 合同，而是验证当前 Card、Draw、Shuffle、BattleEvent、Relic Trigger、ActionQueue 和 Presentation 架构能否在真实跨系统组合中自然协作。

当前生产/调试内容中，Pommel Strike（剑柄打击）已经配置为 `Draw 2`，并已通过人工 PIE 观察到目标组合链。因此 Phase 8 不再创建专门的 `Pommel Strike+` 测试资产，也不再把 Upgrade System 作为本阶段内容。

---

## 1. Goal

Phase 8 的唯一主目标：

```text
使用现有 Draw 2 Pommel Strike + Sundial
验证：
Card → Draw → Shuffle → Event → Relic Reaction → Energy
完整跨系统链路能够自然成立，且不存在 card/relic 组合特判。
```

目标链路：

```text
Pommel Strike
→ Damage
→ UDrawCardEffect(DrawCount = 2)
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

Phase 8 成功的核心证据是：上述组合由独立能力通过稳定合同自然形成，而不是新增专属框架。

---

## 2. Locked architecture rule

Phase 8 必须继续遵守：

```text
Card / Effect 不知道 Sundial
Draw / Shuffle 不知道 Sundial
Sundial 不知道 Pommel Strike
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

---

## 3. Existing evidence

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

这份人工证据可作为 Phase 8 的 Production PIE evidence 使用，不要求为了 Phase 8 重复制作 `Pommel Strike+` 资产或重复同一人工场景，除非后续代码修改使该 Gate 失效。

---

## 4. Phase 8 slices

### 8A — Automated Combo Integration

新增 focused Automation，从真实 Card play / CardEffect 开始，而不是手工 dispatch `FDeckShuffledEvent`。

建议 dedicated prefix：

```text
SlayTheSpireDemo.Phase8
```

建议最少测试：

```text
SlayTheSpireDemo.Phase8.Combo.PommelSundial
SlayTheSpireDemo.Phase8.Combo.OrderingAndContinuation
```

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

升级系统改为战士卡牌扩展的基础能力，与正式卡牌开发一起推进，而不是为了 Phase 8 制造一个测试用 Plus 资产，也不是等所有卡牌完成后再补。

后续卡牌开发原则：

```text
Card Expansion Foundation
├─ generic Upgrade state / resolution
├─ first real upgraded card definitions
└─ focused validation
```

Upgrade 必须保持正交：

```text
Upgrade System 不知道 Damage / Draw / Exhaust / Corruption 等具体能力
Damage / Draw / Exhaust 等能力也不直接判断 UpgradeLevel
所有 Gameplay / UI / A2 / A3 只能通过统一的 effective card definition/value boundary 读取升级后的事实
```

普通卡先支持基础/升级形态；模型必须从设计上允许以后扩展到：

```text
Armaments     → combat temporary upgrade
Searing Blow  → level N / repeatable persistent upgrade
Run Deck      → persistent UpgradeLevel ownership
```

具体设计 authority 单独放在卡牌开发文档，不扩大 Phase 8。

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
[ ] existing Pommel Strike Draw 2 is used; no test-only Pommel Strike+ asset required
[ ] automated combo starts from real Card / Effect execution
[ ] real Shuffle commits emit the existing DeckShuffled fact
[ ] Sundial counts only those committed facts
[ ] third counted Shuffle grants exact +2 Energy
[ ] reaction ordering precedes remaining bulk-draw continuation
[ ] Queue drains without ResolutionFault
[ ] no card/relic combination special case exists
[ ] existing Production PIE evidence remains valid
[ ] final evidence recorded in Phase8Validation.md
```

---

## 8. Next exact action

```text
REVIEW THIS REVISED DESIGN.

Phase 8 implementation is still NOT AUTHORIZED.
Do not build Upgrade System inside Phase 8.
After Phase 8 seal, start a separately bounded Card Expansion / Upgrade Foundation design and implementation.
```
