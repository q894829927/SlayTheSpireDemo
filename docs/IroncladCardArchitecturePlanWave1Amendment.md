# Ironclad Card Architecture Plan — Wave 1 Amendment

日期：**2026-09-07**

状态：

```text
CURRENT ORDERING AMENDMENT
POST-WAVE-1C MERGE SYNC
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

当前 `main` 已包含旧基线卡牌以及后续 Card Expansion 内容。

旧基线为：

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

后续已进入 `main` 的 Wave 内容包括：

```text
Wave 1A
- Seeing Red

Wave 1C merge / PR #16 repository content
- Burning Pact asset
```

Wave 1C 的通用行为 seal 仍以 C++ / Automation / PIE 合同为 authority；二进制 CardData 资产本身不取代 generic architecture authority。

Production Card Expansion 从当前 `main` 继续，不重做这些已存在内容。

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

### Wave 1A — Exhaust Fact Surface — COMPLETE / VALIDATED / SEALED

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
→ one self-exhaust production card (Seeing Red)
```

明确不包含 targeted exhaust、selection、reactive Power、Card Trigger Source、multi-enemy。

### Wave 1B — Targeted Exhaust Primitive — COMPLETE / VALIDATED / SEALED

已建立：

```text
exact UCardInstance currently in Hand
→ authoritative Exhaust mutation
→ typed exact CommitResult
→ same CardExhausted event rule
```

此 slice 建立 targeted exhaust primitive，但不把 selection UI/choice 语义塞进 Exhaust primitive。

### Wave 1C — Selection + Targeted Exhaust Composition — COMPLETE / VALIDATED / SEALED / MERGED TO MAIN

Dedicated authorities：

```text
docs/CardExpansionWave1CSelectionPrimitive.md
docs/CardExpansionWave1CSelectionExecution.md
docs/CardExpansionWave1CSelectExhaustExecution.md
```

Merge：

```text
PR #16
main merge commit: a9f26ee4bcc8f12a03ba10d5121eb0ff6ef8d523
```

Wave 1C-A 已封板 generic Selection primitive；Wave 1C-B 已用 Burning Pact 作为首个真实 consumer 完成 playable closure。

已封板方向：

```text
SelectionRequest
→ SelectionResult
→ authored orchestration
→ targeted Exhaust Action
→ typed Result
→ dependent Actions in normal BattleActionQueue order
```

以及 committed Presentation closure：

```text
CardPlayed
→ Hand → ExhaustPile
→ DrawPile → Hand ...
→ PlayArea → destination
```

### Wave 1C-C — Second Consumer Expansion — NEXT ACTIVE / NOT STARTED

目标不是重新设计 Selection，而是证明 Wave 1C 的 primitive 可被第二个真实消费者复用，并只在真实需求出现时补最窄的新 primitive。

当前优先 consumer：

```text
True Grit
Base     → Block + random valid Hand-card Exhaust
Upgraded → Block + manual valid Hand-card Exhaust
```

它复用：

```text
CAP-04 sealed manual Selection
CAP-06 sealed targeted Exhaust
existing Block
```

并首次形成真实需求：

```text
CAP-12 deterministic battle RNG
→ ChooseIndex(Count)
→ domain-neutral random choice
```

Random primitive 不得知道 True Grit、Hand、Exhaust 或 CardId。Wave 1C-C implementation 尚未开始；在写代码前应先新增 dedicated Wave 1C-C design/execution authority。

Exhume 暂不作为 1C-C 首选，因为它还会同时要求 ExhaustPile selection + non-Hand zone move surface（CAP-05），扩大 slice。

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

该 foundation 仍需单独授权和 sealed，不属于 Wave 1A–1C。

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

当前 mutation 已存在并已在 Wave 1A 形成 exact `CardExhausted` fact surface。

### Targeted exhaust

```text
a card mechanic selects/specifies another exact CardInstance
→ explicit Exhaust mutation/action
```

当前 generic Hand targeted-exhaust capability 已由 Wave 1B 实现并 seal，Wave 1C 已证明它可以通过 generic Selection composition 被玩家选择驱动。

不要再用“Exhaust 已实现”笼统描述 self-exhaust 与 targeted exhaust 两条不同路径。

---

## 6. CardExhausted producer rule — locked for future slices

无论 producer 是：

```text
self-exhaust card-play cleanup
targeted ExhaustCardAction
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

Wave 1A / 1B 已证明 self-exhaust 与 targeted exhaust 两个真实 producer 可遵守同一 committed-fact 规则。未来第三种 producer 出现时，再按实际重复决定是否继续抽共享 helper，不提前造 universal zone-event framework。

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

Wave 1A 使用 Seeing Red 作为 production validation card，以控制 slice 大小。

Wave 1A seal 后，其余只依赖已 sealed primitive 的 self-exhaust cards 可以作为后续 content-only batch author，而不需要再次重新设计 CardExhausted event surface。

---

## 9. Current implementation ordering

当前推荐顺序：

```text
Card Upgrade STS-Style Refactor
→ COMPLETE / VALIDATED / SEALED

Card Face Visual Style
→ COMPLETE / USER-ACCEPTED / SEALED

Wave 1A — Exhaust Fact Surface
→ COMPLETE / VALIDATED / SEALED

Wave 1B — Targeted Exhaust Primitive
→ COMPLETE / VALIDATED / SEALED

Wave 1C-A — Selection Primitive
→ COMPLETE / VALIDATED / SEALED

Wave 1C-B — Burning Pact First Consumer Closure
→ COMPLETE / VALIDATED / SEALED
→ MERGED TO MAIN (PR #16)

Wave 1C-C — Second Consumer Expansion
→ NEXT ACTIVE / NOT STARTED
→ preferred first consumer: True Grit
→ dedicated authority must be authored before implementation

Wave 1D — Reactive Exhaust Powers
→ FUTURE / NOT AUTHORIZED BY THIS ORDERING SYNC

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
if (CardId == "TrueGrit")
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
重开 Wave 1A / 1B / 1C-A / 1C-B sealed contracts without a concrete regression
把 Widget 变成 Gameplay authority
把 FCardPlayContext / FTriggerContext 扩成 service locator
建立 UniversalResultBus / mutable property bag
```

---

## 11. Stop / next authority

当前状态：

```text
main
→ contains Wave 1A / 1B / 1C sealed implementation

Wave 1C-C
→ NEXT ACTIVE
→ dedicated design/execution authority does not exist yet
```

下一步不是继续修改 Wave 1C sealed 文档，也不是直接实现 Wave 1D。

下一步应先 author 一份 dedicated Wave 1C-C authority，明确：

```text
True Grit Base / Upgraded composition
eligible Hand candidate rule
played-card exclusion rule
empty-candidate behavior
deterministic RNG ownership/API boundary
random vs manual authored mode
Presentation expectations
focused Automation + PIE seal gates
```

该 dedicated authority 审定后，再进入 Wave 1C-C 实现。