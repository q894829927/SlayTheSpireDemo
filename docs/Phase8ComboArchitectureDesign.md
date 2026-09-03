# Phase 8 — Combo Architecture Validation

日期：**2026-09-04**

状态：**DESIGN DRAFTED / REVIEW PENDING / IMPLEMENTATION NOT AUTHORIZED**

Phase 7A–7F 已完成、验证并 sealed。Phase 8 不重新打开这些阶段的 Gameplay / Presentation 合同，而是验证此前建立的 Card、Draw、Shuffle、BattleEvent、Relic Trigger、ActionQueue 和 Presentation 架构能否在真实跨系统组合中自然协作。

本文件是 Phase 8 的设计草案。除非用户明确授权实施，否则不得据此开始修改源码或生产资产。

---

## 1. Goal

Phase 8 的唯一主目标：

```text
通过两张数据驱动的 Pommel Strike+ 与 Sundial，
验证 Card → Draw → Shuffle → Event → Relic Reaction → Energy
完整跨系统链路，不存在 card/relic 组合特判。
```

目标链路：

```text
Pommel Strike+
→ Card Effect composition
→ UDrawCardEffect(DrawCount = 2)
→ UDrawCardsAction(2)
→ real Draw / Shuffle continuation
→ UShuffleDeckAction commit
→ DeckShuffled Presentation Record
→ FDeckShuffledEvent
→ UDeckShuffledCountTrigger
→ UAdvanceRelicCounterAction
→ every third shuffle resets counter
→ prepared UGainEnergyAction(+2)
→ remaining bulk-draw continuation
```

Phase 8 成功的最重要证据不是“新增了多少框架”，而是上述组合尽可能在**不改底层 sealed architecture**的前提下直接成立。

---

## 2. Current architecture review

当前 Card 模型已经具备完成 Phase 8 所需的核心结构：

```text
UCardData
- immutable definition
- CardId / DisplayName / Description
- BaseCost / TargetType / Destination
- Effects[]

UCardInstance
- Definition
- RuntimeId
```

`UDeckRuntime::InitializeFromDefinitions()` 直接从 `UCardData[]` 创建 battle-scoped `UCardInstance`。运行时具体卡牌副本由 `UCardInstance` + `RuntimeId` 区分，Card Definition 本身保持 immutable。

当前 Gameplay / Presentation / Preview 都通过同一个 Definition boundary 读取卡牌事实：

```text
UCardInstance::GetDefinition()
→ PlayCardAction consumes Effects[]
→ BattleTextResolver consumes Effects[]
→ A3 Immediate Preview consumes Effects[]
→ PresentationCardSnapshot consumes immutable card-facing facts
```

因此 Phase 8 没有必要为了一个组合验证引入新的 runtime upgrade branch。

当前 Draw 已具备 Phase 8 所需的 bulk semantics：

```text
UDrawCardEffect(DrawCount=N)
→ UDrawCardsAction(N)
   → UDrawCardAction(s)
   → UShuffleDeckAction
   → UDrawCardsAction(Remaining)
```

Shuffle commit 后的顺序已经 sealed：

```text
Shuffle commit
→ DeckShuffled Presentation Record
→ FDeckShuffledEvent
→ Trigger reactions inserted at Queue front
→ reactions execute
→ remaining bulk draw continues
```

Phase 8 必须复用这一行为，不得重新设计 Draw / Shuffle ownership。

---

## 3. Locked design decision — Pommel Strike+ is a content variant

Phase 8 **不实现通用 Card Upgrade runtime**。

Pommel Strike+ 首先作为一个新的 immutable `UCardData` content variant：

```text
DA_Card_PommelStrike
CardId = PommelStrike
DisplayName = Pommel Strike
Effects
├─ DamageCardEffect(...)
└─ DrawCardEffect(DrawCount = 1)

DA_Card_PommelStrikePlus
CardId = PommelStrike
DisplayName = Pommel Strike+
Effects
├─ DamageCardEffect(upgraded authored value)
└─ DrawCardEffect(DrawCount = 2)
```

Phase 8 的架构验收只要求：

```text
Pommel Strike+ 的 DrawCardEffect.DrawCount = 2
```

Damage 的具体升级数值属于 content authoring，不作为 Combo Architecture 成败条件。若生产内容已有明确升级数值，则按生产定义填写；不要为了 Phase 8 额外设计升级数值框架。

`CardId` 保持 `PommelStrike`，表示同一逻辑 card archetype；asset identity / DisplayName 区分普通版与 Plus，battle 内的具体副本继续由 `RuntimeId` 区分。

任何 Gameplay / UI 代码都不得因为 Plus 变体增加：

```cpp
if (CardId == "PommelStrike")
if (DisplayName == "Pommel Strike+")
```

行为必须仅来自 Definition / Effect composition。

---

## 4. Why no generic upgrade runtime in Phase 8

以下内容明确不进入 Phase 8：

```text
UCardInstance::bUpgraded
UCardInstance::UpgradeLevel
UpgradeCardAction
BaseEffects + UpgradeEffects
UpgradeDelta
Effect-level generic upgrade mutation
RunCardEntry
campfire / reward upgrade flow
save/load upgrade persistence
```

原因：当前项目尚没有正式 Run Deck ownership、reward/campfire acquisition 或 save/load ownership。现在把升级状态塞进 battle-scoped `UCardInstance` 会提前决定一个尚未设计的跨战斗所有权边界，并迫使已经 sealed 的 PlayCard / A2 / A3 / text resolution 路径理解 Upgrade state。

未来真正引入 run-level Card ownership 时，再根据第二个真实升级需求设计类似：

```text
RunCardEntry
├─ BaseCardDefinition
└─ UpgradeLevel
```

并决定 battle materialization 方式。Phase 8 不提前实现该框架。

---

## 5. Existing contracts that remain sealed

Phase 8 必须复用以下已有合同：

```text
BattleAction / BattleActionQueue = Gameplay mutation authority
Card Effect = data-driven Action builder
UCardData = immutable definition
UCardInstance = runtime identity
DeckRuntime = card-zone truth
UDrawCardsAction = one bulk Draw-N request
UDrawCardAction = one atomic DrawPile -> Hand commit
UShuffleDeckAction = atomic gameplay shuffle commit
BattleEvent = committed immutable fact
Trigger = read-only eligibility + Action builder
Relic reaction order = existing deterministic dispatcher order
Sundial = generic DeckShuffled count trigger + GainEnergy effect
A2 = committed Presentation playback
A3 = pre-commit read-only immediate preview; does not predict Draw/Shuffle/Relic reactions
```

除非 focused Phase 8 test 证明其中存在真实缺口，否则不得“顺手重构”这些系统。

---

## 6. Explicit forbidden special cases

Phase 8 必须通过静态检查与 test intent 明确禁止以下模式：

```cpp
if (CardId == "PommelStrike" && RelicId == "Sundial")
if (CardId == "PommelStrike") { ForceShuffle(); }
if (RelicId == "Sundial") { /* draw-specific branch */ }
if (Action is DrawCardsAction && source card is PommelStrikePlus) { ... }
```

同样禁止：

```text
Draw code querying RelicContainer
Sundial querying current played Card identity
Relic Trigger querying DrawCardEffect
Pommel Strike+ directly granting Energy because Sundial exists
UI manufacturing gameplay shuffle/counter state
A3 predicting Sundial trigger results
```

正确依赖方向只能是：

```text
Card definition
→ generic Effects
→ generic Actions
→ Deck commits
→ committed Event
→ generic Relic Trigger
→ reaction Actions
```

---

## 7. Phase 8 slices

### 8A — Upgraded Pommel Content Variant

目标：只创建 Phase 8 所需的 Plus content，不引入 Upgrade runtime。

预期工作：

```text
Create / configure DA_Card_PommelStrikePlus
- CardId = PommelStrike
- DisplayName = Pommel Strike+
- normal existing Card rules copied/aligned from Pommel Strike
- DamageCardEffect uses intended upgraded authored value
- DrawCardEffect.DrawCount = 2
- Description Format continues to use dynamic semantic arguments
```

需要验证：

```text
card face resolves Draw = 2 from Effect data
PlayCardAction consumes the normal Effects[] path
Draw effect builds one UDrawCardsAction with bulk intent N=2
no Plus-specific C++ class
no Upgrade branch in UCardInstance
```

8A 可以包含 focused C++ Automation fixture；生产 `.uasset` 仍由 Unreal Editor author/save。

### 8B — Automated Combo Integration

目标：通过 Automation 从真实 Card Effects 发起整链，不手工伪造 shuffle event。

建议 dedicated prefix：

```text
SlayTheSpireDemo.Phase8
```

最少建议测试：

```text
SlayTheSpireDemo.Phase8.PommelPlus.ContentContract
SlayTheSpireDemo.Phase8.Combo.TwoPommelPlusSundial
SlayTheSpireDemo.Phase8.Combo.OrderingAndContinuation
```

测试合同：

#### ContentContract

```text
Plus Definition uses normal UCardData / UCardEffect model
DrawCardEffect.DrawCount == 2
resolved dynamic card text exposes Draw 2
normal PlayCard/Effect path creates bulk DrawCardsAction(2)
```

#### TwoPommelPlusSundial

从真实 Card play / Draw Effect 开始，验证：

```text
real Draw requests cause real gameplay Shuffle commits
committed Shuffle emits FDeckShuffledEvent
Sundial Counter advances only from those events
Counter progression reaches threshold through generic interaction
third counted shuffle resets Counter to 0
exact +2 Energy is granted through GainEnergyAction
Queue reaches a non-faulted drained state
```

测试不得直接以：

```cpp
Dispatcher->Dispatch(FBattleEvent::MakeDeckShuffled(...))
```

作为主组合路径的起点；那已经由 Phase 7 tests 覆盖，无法证明 Card → Draw → Shuffle integration。

#### OrderingAndContinuation

验证 sealed ordering 在组合中仍成立：

```text
Shuffle commit
→ DeckShuffled event
→ Sundial reaction
→ remaining bulk draw continuation
```

特别要确认 reward reaction 不会被 remaining Draw continuation 越过。

如果测试触发合法的 pre-planned zero-card Shuffle，它仍按照 Phase 6C sealed semantics 计作真实 committed Shuffle；不得为 Phase 8 添加 card-specific 排除逻辑。

### 8C — Production PIE Acceptance

目标：使用 Native production HUD 完成一次玩家可理解的真实组合验证。

Production acceptance setup 应包含：

```text
2 × Pommel Strike+
1 × Sundial
```

可以加入达到确定 pile-state 所必需的普通 filler cards，但 filler 只能服务于确定性 playable setup，不得承载组合 special-case logic。

PIE 验收至少观察：

```text
Pommel Strike+ 卡面动态显示 Draw 2
真实出牌执行 Damage / Draw
DrawPile / DiscardPile 变化可理解
真实 Shuffle 发生
Sundial Counter 随真实 Shuffle 推进
Counter eventually 0 -> 1 -> 2 -> 0
第三次 counted Shuffle 后 +2 Energy
remaining Draw continuation 正常完成
后续 Gameplay 仍可继续操作
无 ResolutionFault / Missing Class / Failed to load / crash
```

A3 不要求预告未来 Shuffle 或 Sundial +2 Energy；这是继续保持 sealed A3 boundary 的正确表现。

### 8D — Validation / Seal

8A–8C 全部通过后，建立：

```text
docs/Phase8Validation.md
```

并把 Phase 8 标记为：

```text
COMPLETE / VALIDATED / SEALED
```

如果完整 combo 在不修改底层系统的情况下直接通过，这是 Phase 8 最优结果。

---

## 8. Validation policy

继续遵守项目当前 validation policy：

```text
Build once
→ smallest new focused Phase8 Automation suite
→ only directly invalidated existing regression prefixes
→ one final production PIE acceptance
→ record evidence
→ STOP
```

PASS Gate 是 sticky 的。只有后续修改使某个 Gate 失效时才重跑该 Gate。

当存在多个需要执行的 Automation prefix 时，应一次性把全部命令提供给用户，而不是逐个等待反馈。

Phase 8 预期直接相关的历史 regression 候选为：

```text
Phase6C Draw / Shuffle focused tests
Phase7.Sundial
Phase7E reaction composition（仅当 reaction path 被修改）
A3 DynamicText / CardFace focused tests（仅当 card text / preview path 被修改）
```

不要默认运行 Phase6R、A2D5、Shipping 等大型 aggregate gate，除非实际修改范围明确使其失效。

---

## 9. Expected implementation impact

理想 Phase 8 的 C++ 改动应非常小：

```text
new focused Phase8 Automation test source
possibly small test-only fixtures/helpers
no new generic Upgrade runtime
no new card-specific Action class
no Draw / Shuffle semantic rewrite
no Sundial special-case
no A2 / A3 architecture rewrite
```

生产内容主要变化：

```text
new DA_Card_PommelStrikePlus.uasset
production/debug battle setup adjusted for final PIE acceptance as needed
```

如果实施过程中发现必须修改核心系统，先 STOP，把问题写入设计 amendment，再决定是否扩大 Phase 8 scope。

---

## 10. Acceptance definition

Phase 8 只有在以下条件全部成立后才可 seal：

```text
[ ] Pommel Strike+ is a normal immutable UCardData content variant
[ ] Draw 2 comes from normal UDrawCardEffect data
[ ] no generic Upgrade runtime was introduced
[ ] no card/relic combination special case exists
[ ] automated combo starts from real card/effect execution
[ ] real Shuffle commits emit the existing DeckShuffled fact
[ ] Sundial counts those facts through the existing Relic trigger path
[ ] third counted shuffle grants exact +2 Energy
[ ] reaction ordering precedes remaining bulk-draw continuation
[ ] queue drains without ResolutionFault
[ ] dynamic card face correctly exposes Draw 2
[ ] production Native HUD PIE is understandable and playable
[ ] directly invalidated regressions PASS
[ ] final evidence recorded in Phase8Validation.md
```

---

## 11. Non-goals after Phase 8

Phase 8 完成后仍不自动授权：

```text
Run Deck / map progression
reward screens
campfire upgrade choices
shop
relic/card acquisition
save/load
rarity pools
universal card upgrade framework
RelicTriggered / RelicCounterChanged dedicated Presentation Records
advanced A3 prediction
presentation polish
```

下一阶段必须重新选择并明确授权。

---

## 12. Next exact action

```text
REVIEW THIS DESIGN.

Do not implement Phase 8 yet.
Do not modify UCardInstance / UCardData / Draw / Shuffle / Relic runtime yet.
Do not create production Pommel Strike+ asset until implementation is explicitly authorized.
```
