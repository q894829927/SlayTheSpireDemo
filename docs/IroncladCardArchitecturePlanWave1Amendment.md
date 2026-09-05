# Ironclad Card Architecture Plan — Wave 1 Amendment

日期：**2026-09-06**

状态：

```text
CURRENT ORDERING AMENDMENT
SUPERSEDES STALE FOUNDATION-0 / WAVE-1 SCHEDULING ONLY
```

本文件不是新的全卡牌架构总纲。`docs/IroncladCardArchitecturePlan.md` 中的 75-card capability catalog、正交 primitive 原则、typed contract、Continuation 方向、Card-as-composition-root 等仍然有效。

本 Amendment 只修正旧文档中已经被后续实现事实取代或过于粗粒度的 **当前状态、Foundation 0 与 Wave 1 调度**。

发生冲突时，本文件对以下内容具有更新优先级：

```text
IroncladCardArchitecturePlan.md
→ Foundation 0 当前状态
→ Wave 1 推荐实施顺序
→ Card Expansion 当前 next-active ordering

CardTriggerSourceExpansionDesign.md
→ “Phase 8 必须先完成”这一旧 ordering gate
```

它不改变 `CardTriggerSourceExpansionDesign.md` 的架构边界，也不自动授权该 foundation。

---

## 1. Current baseline correction

旧总规划写作时，普通卡 Upgrade Foundation 尚未完成。

现在 authoritative 状态已经变化：

```text
docs/CardUpgradeSTSStyleRefactor.md
→ COMPLETE / VALIDATED / SEALED
```

当前普通卡升级正式模型：

```text
one immutable UCardData
+ one Effects[] composition
+ typed Base / Upgraded authored values
+ one UCardInstance::bUpgraded runtime bit
```

因此旧计划中的：

```text
Foundation 0 — Card Expansion / Upgrade Foundation — NEXT ACTIVE GOAL
```

不再是待实施前置。

### Foundation 0 current interpretation

```text
ordinary single-upgrade foundation
→ COMPLETE / VALIDATED / SEALED

repeatable upgrade / Searing Blow special runtime semantics
→ FUTURE SPECIAL CONSUMER SLICE
→ NOT A BLOCKER FOR NORMAL CARD EXPANSION
```

禁止因为旧规划仍描述 `Optional Repeatable definition policy / typed EffectiveCardView` 等早期方向而重新打开 sealed ordinary-upgrade model。

---

## 2. Current production card baseline

当前 `main` 生产 CardData 为 6 张：

```text
Attack
- Strike
- Pommel Strike
- Twin Strike
- Uppercut

Skill
- Defend

Power
- Inflame
```

这些卡已经用于既有 Gameplay / Upgrade / CFV validation。

Production Card Expansion 从这里继续，不重做这 6 张。

---

## 3. Why the old Wave 1 is split

旧总规划 Wave 1：

```text
Impervious
Pummel
Seeing Red
Shockwave
Burning Pact
Feel No Pain
Dark Embrace
```

该列表是 capability-pressure / recommended-ordering 信号，不是 dependency-closed implementation batch。

这些卡实际依赖异质能力：

```text
Impervious / Seeing Red / Pummel
→ self-exhaust after normal play
→ need exact CardExhausted fact

Shockwave
→ self-exhaust
→ multi-enemy CAP-11

Burning Pact
→ selection
→ targeted exhaust of selected CardInstance
→ typed exhaust result
→ authored Continuation
→ draw

Feel No Pain / Dark Embrace
→ ongoing Power trigger source
→ react to any real CardExhausted event

Sentinel (not old Wave 1 list, but same exhaust domain)
→ exact exhausted CardInstance owns authored Card trigger
→ requires independent Card Trigger Source Expansion
```

因此原 Wave 1 不再作为一次性实现 batch。

---

## 4. Revised Wave 1 ordering

### Wave 1A — Exhaust Fact Surface — NEXT ACTIVE SLICE

Dedicated authority：

```text
docs/CardExpansionWave1AExhaustFactSurface.md
```

范围：

```text
existing self-exhaust commit path
→ exact FCardExhaustedEvent
→ Dispatcher
→ focused Automation
→ one self-exhaust production card (default: Seeing Red)
```

明确不包含 targeted exhaust、selection、reactive Power、Card Trigger Source、multi-enemy。

### Wave 1B — Targeted Exhaust Primitive — FUTURE

目标：

```text
arbitrary specified CardInstance / authored candidate set
→ authoritative Exhaust mutation
→ typed exact CommitResult
→ same CardExhausted event rule
```

此 slice 建立 targeted exhaust primitive，但不把 selection UI/choice 语义塞进 Exhaust primitive。

### Wave 1C — Selection + Targeted Exhaust Composition — FUTURE

典型卡：

```text
Burning Pact
True Grit upgraded selection path
Exhume-related selection/move composition where applicable
```

正确方向：

```text
SelectionRequest
→ SelectionResult
→ authored orchestration
→ targeted Exhaust / Move Action
→ typed Result
→ authored Continuation when dependent follow-up exists
```

### Wave 1D — Reactive Exhaust Powers — FUTURE

典型卡：

```text
Feel No Pain
Dark Embrace
```

正确方向：

```text
CardExhausted committed event
→ ongoing Power runtime trigger source
→ reaction BattleAction
```

它们对“任意真实 CardExhausted”作出反应，不应被错误绑定到 CardInstance trigger-source provider。

### Independent foundation — Card Trigger Source Expansion — FUTURE

典型 consumer：

```text
Sentinel
```

它解决：

```text
this exact CardInstance
→ as generic runtime trigger source
→ reacts when the exact instance is the committed event subject
```

该 foundation 仍需单独授权和 sealed，不属于 Wave 1A。

---

## 5. Exhaust capability terminology — locked

后续文档与代码评审统一使用：

### Self-exhaust

```text
played card
→ DefaultDestination = Exhaust
→ FinishCardPlay
→ PlayArea → ExhaustPile
```

当前 mutation 已存在。

### Targeted exhaust

```text
a card mechanic selects/specifies another exact CardInstance
→ explicit Exhaust mutation/action
```

当前尚未作为 generic authored capability 实现。

不要再用“Exhaust 已实现”笼统描述两者。

---

## 6. CardExhausted producer rule — locked for future slices

无论 producer 是：

```text
self-exhaust card-play cleanup
future targeted ExhaustCardAction
future bulk exhaust action
future Ethereal cleanup
```

都必须遵守：

```text
authoritative mutation
→ exact commit succeeds
→ immutable CardExhausted fact
→ dispatch
```

禁止：

```text
intent → dispatch → mutation
DefaultDestination check → assume commit success
DeckRuntime directly owns Dispatcher
CardId-specific event emission
```

是否以后抽出共享 helper，等第二个真实 producer 出现后按实际重复代码决定，不提前造 universal zone-event framework。

---

## 7. Shockwave placement correction

Shockwave 需要：

```text
all enemies Weak + Vulnerable
+ self Exhaust
```

因此它的 CardExhausted 部分可复用 Wave 1A surface，但完整生产卡实现必须等 multi-enemy CAP-11。

状态：

```text
Shockwave full card
→ DEFERRED TO MULTI-ENEMY CAPABILITY WAVE
```

不要为了维持旧 Wave 1 列表而提前扩展 BattleManager 多敌模型。

---

## 8. Pummel / Impervious / Seeing Red interpretation

这三张只需要 self-exhaust + 已有/窄 content primitive：

```text
Impervious
→ Block + self Exhaust

Pummel
→ multi-hit Damage + self Exhaust

Seeing Red
→ Gain Energy + self Exhaust
```

Wave 1A 默认只选 **Seeing Red** 作为 production validation card，以控制 slice 大小。

Wave 1A seal 后，其余只依赖已 sealed primitive 的 self-exhaust cards 可以作为后续 content-only batch author，而不需要再次重新设计 CardExhausted event surface。

---

## 9. Current implementation ordering

当前推荐顺序：

```text
Card Upgrade STS-Style Refactor
→ COMPLETE / VALIDATED / SEALED

Card Face Visual Style
→ COMPLETE / USER-ACCEPTED / SEALED

Production Card Expansion
→ STARTS WITH Wave 1A

Wave 1A — Exhaust Fact Surface
→ NEXT ACTIVE SLICE

Wave 1B / 1C / 1D
→ NOT AUTHORIZED BY THIS AMENDMENT

Card Trigger Source Expansion
→ FUTURE INDEPENDENT FOUNDATION

Phase 8 Combo Architecture Validation
→ DEFERRED / NOT A BLOCKER
```

---

## 10. Guardrails retained from the original plan

仍然禁止：

```cpp
if (CardId == "SeeingRed")
if (CardId == "BurningPact")
if (CardId == "FeelNoPain")
if (CardId == "Sentinel")
```

仍然坚持：

```text
primitive capability
→ typed neutral contract
→ authored card/orchestration
→ normal BattleAction
→ authoritative commit
→ exact BattleEvent when a real consumer/fact surface exists
```

不得为了 Card Expansion：

```text
重开 sealed CFV
重开 ordinary Upgrade architecture
把 Widget 变成 Gameplay authority
把 FCardPlayContext / FTriggerContext 扩成 service locator
建立 UniversalResultBus / mutable property bag
```

---

## 11. Stop / next authority

当前唯一 next-active dedicated authority：

```text
docs/CardExpansionWave1AExhaustFactSurface.md
```

先完成、验证、seal Wave 1A，再决定 Wave 1B/1C/1D 的具体授权和顺序。
