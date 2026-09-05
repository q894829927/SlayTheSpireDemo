# Card Trigger Source Expansion Design

日期：**2026-09-04**

状态：**DESIGN DRAFT / IMPLEMENTATION NOT AUTHORIZED**

本文件定义 Ironclad 卡牌扩展阶段需要的 generic Card Trigger Runtime Source 扩展。它是一个独立基础 slice，不能折进 Sentinel 卡牌实现，也不重新打开 Phase 7A–7F 已 sealed 的 Status/Relic 行为。

---

## 1. Goal

当前 sealed Dispatcher 只发现 Status / Relic trigger source。未来 Sentinel、Burn、Blood for Blood 等机制需要 CardInstance 也能作为 generic Trigger Runtime Source，但不能让 Exhaust、Turn、HP 或 Dispatcher 对具体 CardId 做特判。

目标：

```text
CardData
→ authors generic Trigger definitions

CardInstance
→ supplies runtime identity / card runtime state

Card runtime owner / source provider
→ exposes eligible Card trigger-source snapshots

Battle composition layer
→ combines source snapshots

Dispatcher
→ consumes generic trigger sources
→ deterministic ordering
→ BuildReactions
```

Dispatcher 不直接学习 Deck zone internals，也不查询具体卡牌语义。

---

## 2. Locked source-discovery boundary

Card Trigger Source 不能简单理解为“Dispatcher 遍历所有 Deck zones”。

必须存在明确的 source-provider boundary，回答：

```text
which CardInstances are active candidates for this Event?
what runtime identity does each source expose?
what definition-level Triggers belong to that source?
what stable ordering key belongs to the source?
```

允许的候选策略由 authored/general source rules决定，例如：

```text
Event-subject-only
→ Sentinel reacts only when that exact CardInstance is exhausted

Zone-active
→ Burn in the relevant active zone can react to TurnEnded

Combat-wide card runtime state
→ Blood for Blood-style runtime owner can consume HPLost when its mechanics require it
```

这些策略必须通过 typed source/provider contract 表达；Dispatcher 不得出现：

```cpp
if (CardId == "Sentinel")
if (CardId == "Burn")
if (CardId == "BloodForBlood")
```

也不得自己遍历 Hand/Draw/Discard/Exhaust 并解释每个 zone 的卡牌语义。

---

## 3. Trigger definition / runtime identity ownership

锁定：

```text
CardData
→ immutable Trigger definitions

CardInstance
→ runtime card identity
→ mutable card-owned state when a real mechanic requires it

no per-instance Trigger UObject required
```

`CardData` 仍是 immutable definition；不得把 runtime counters / last event / pending target 写入 definition subobject。

---

## 4. Deterministic ordering key

现有 Phase 7 Status/Relic 排序必须保持原结果。Card source 使用一个显式、单一比较键：

```text
Priority
→ SourceTier
   Status / Relic = 0
   Card           = 1
→ SequenceKey
   Status / Relic = RuntimeSequence
   Card           = RuntimeId
→ LocalTriggerIndex
```

意义：

```text
旧 Status/Relic 组合永远落在 SourceTier=0
→ 原 Phase7 RuntimeSequence 相对顺序保持不变

Card source 落在 SourceTier=1
→ equal Priority 时排在既有 non-card source 之后

Card vs Card
→ stable RuntimeId order
```

当前不让 deck setup 消耗 battle RuntimeSequence，也不为了 Card source 推翻 Phase 7 的 creation-order contract。

如果未来出现真实需求要求 Card 与 Status/Relic 按统一 creation timeline 交叉排序，再单独设计新的 neutral sequence authority；本 slice 不提前做。

---

## 5. RuntimeId invariant

Card trigger ordering依赖稳定 `RuntimeId`，因此锁定：

```text
Every newly materialized UCardInstance
→ obtains a fresh battle-unique RuntimeId
→ from the same authoritative NextRuntimeId allocator
```

包括：

```text
initial deck materialization
created cards
copied cards
cloned cards
```

禁止：

```text
copy source RuntimeId into clone/copy
reuse a removed card's RuntimeId during the same battle
let individual card abilities allocate their own RuntimeId
```

`FCardCloneSpec` 若以后出现，只描述可复制的 card state；`RuntimeId` 永远由 Card runtime owner/materialization boundary重新分配。

---

## 6. Event relationship

Card source expansion 本身不凭空制造事件。每个 Event 仍必须来自真实 authoritative commit。

Sentinel 的首个验证合同：

```text
Exhaust commit
→ exact CardExhausted BattleEvent
→ source provider exposes the exact exhausted CardInstance as an eligible source
→ CardData-authored Trigger reads Event + source snapshot
→ builds GainEnergyAction
```

Exhaust system不知道 Sentinel；Card trigger source provider也不执行 GainEnergy。

---

## 7. Legacy context guardrail

`FTriggerContext` 当前包含 runtime source、ActionOuter、Battle 和 Presentation writer。此 sealed 历史接口不在本 slice 中重构，但禁止继续把它扩成 Service Locator。

新 Card Trigger 实现优先消费：

```text
BattleEvent
FTriggerRuntimeSource snapshot
typed read-only Query
ActionOuter / writer only when existing Action-building contract requires them
```

禁止继续向 `FTriggerContext` 添加新的 subsystem service；不得把 `GetBattle()` 当成任意 Deck/Relic/Status/Card subsystem locator。

---

## 8. Implementation slice and regression gates

本能力必须作为独立基础 slice 实施，不折进 Sentinel content batch。

最小实现范围：

```text
Card source kind / generic source snapshot
source-provider boundary
single deterministic comparison key
fresh RuntimeId materialization invariant
focused Card source tests
```

必须保护 sealed ordering：

```text
new Card-source focused Automation
+ existing Phase7 Status/Relic trigger-source ordering regression
+ existing Phase6 trigger ordering regression
```

只有该 slice sealed 后，Sentinel 等具体卡牌才能依赖它。

---

## 9. Non-goals

本 slice 不实现：

```text
Sentinel full card content
Burn full card content
Blood for Blood full card content
new universal Trigger Registry
Deck-wide persistent listener registry
new UI
new Presentation ownership
```

---

## 10. Acceptance

```text
[ ] CardData can author generic Trigger definitions without CardId branching
[ ] CardInstance supplies runtime identity without per-instance Trigger UObject
[ ] source discovery is provided by a typed provider boundary, not Dispatcher Deck traversal
[ ] comparator is Priority -> SourceTier -> SequenceKey -> LocalTriggerIndex
[ ] Status/Relic Phase7 relative ordering is unchanged
[ ] Card vs Card ordering uses stable RuntimeId
[ ] all newly materialized cards receive fresh battle-unique RuntimeId from the same allocator
[ ] clone/copy never copies RuntimeId
[ ] FTriggerContext is not expanded into a larger Service Locator
[ ] focused Card-source + Phase7/Phase6 ordering regressions pass before the slice is sealed
```

---

## 11. Authorization

```text
DESIGN ONLY.
IMPLEMENTATION NOT AUTHORIZED.

The former Phase-8 prerequisite has been superseded by:
docs/IroncladCardArchitecturePlanWave1Amendment.md

Card Trigger Source Expansion remains independently unauthorized.
Implement only when an explicitly authorized Card-owned trigger consumer
requires it.
```
