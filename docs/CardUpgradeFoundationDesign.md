# Card Upgrade Foundation Design

日期：**2026-09-04**

状态：**DESIGN DRAFT / IMPLEMENTATION NOT AUTHORIZED**

本文件定义战士卡牌扩展阶段需要的通用 Upgrade Foundation。它不属于 Phase 8；Phase 8 仍只做现有 Pommel Strike Draw 2 + Sundial 的组合验证。

---

## 1. Goal

升级系统与正式卡牌内容一起开发，而不是：

```text
为了 Phase 8 单独造 Pommel Strike+
或
等全部卡牌做完后再补升级系统
```

目标是建立一个正交、可复用的升级能力，使普通卡、临时升级和未来 Run 持久化都能共享同一套边界。

---

## 2. Locked decoupling rule

Upgrade 是独立能力域。

```text
Upgrade System 不知道：
Damage
Block
Draw
Exhaust
Status
Corruption
Perfected Strike
任何具体 CardId
```

反向同样成立：

```text
Damage / Block / Draw / Exhaust 等系统
不得直接判断 UpgradeLevel
不得根据 CardId 决定升级结果
```

所有消费者只读取一个统一的 effective card boundary。

---

## 3. Required model

设计必须从一开始允许：

```text
normal card:       Level 0 / Level 1
Armaments:         combat-only temporary upgrade
Searing Blow:      Level N / repeatable upgrade
future Run Deck:   persistent UpgradeLevel ownership
```

因此禁止只设计：

```text
bool bUpgraded
```

升级状态至少应抽象成：

```text
UpgradeLevel : int32 >= 0
```

但具体 ownership 要分层：

```text
Persistent ownership
→ future RunCardEntry / deck entry

Battle materialized state
→ UCardInstance receives effective UpgradeLevel

Combat temporary mutation
→ authoritative UpgradeCardAction or equivalent card-state mutation
```

Phase 8 不建立 RunCardEntry；卡牌扩展阶段只实现当前真实消费者需要的最小 ownership，同时保留未来持久化入口。

---

## 4. Effective card boundary

现有系统大量通过 `UCardInstance::GetDefinition()` 读取卡牌事实。升级能力不能迫使每个系统各自解释 UpgradeLevel。

后续必须只有一个统一入口负责把：

```text
Base Card Definition
+ UpgradeLevel
+ permitted combat card state
```

解析为当前有效卡牌事实。

所有以下系统都必须消费同一个 resolved/effective source：

```text
PlayCardAction
Card cost / playability
Card Effects
BattleTextResolver
A3 Immediate Preview
PresentationCardSnapshot
Card UI
```

不得出现：

```cpp
if (Card->GetUpgradeLevel() > 0) // scattered across gameplay systems
```

---

## 5. Content authoring principle

卡牌升级变化属于内容数据，不属于 Upgrade System 的卡名分支。

一张牌可以升级：

```text
Damage amount
Block amount
Draw count
Status amount
Cost
Destination / keyword
Effect count / effect configuration
```

Upgrade Foundation 只提供：

```text
state
resolution
authoritative mutation
presentation-readable effective facts
```

具体哪一个字段如何变化由卡牌内容定义。

不得：

```cpp
if (CardId == "PommelStrike") DrawCount = 2;
if (CardId == "Bash") Vulnerable += 1;
```

---

## 6. Runtime mutation

战斗中的升级必须仍由 Gameplay ActionQueue 掌权：

```text
Armaments
→ Select Card(s)
→ UpgradeCardAction
→ authoritative UCardInstance state commit
→ optional committed CardUpgraded fact if a real consumer requires it
```

Widget 不能直接修改 UpgradeLevel。

如果没有真实 Trigger / Presentation consumer，不要为了形式提前创建 `CardUpgradedEvent`。

---

## 7. First implementation consumers

Upgrade Foundation 应和第一批正式卡牌一起验证。

推荐至少覆盖：

```text
一个普通数值升级卡
→ 验证 Level 0 / 1 effective values

一个 Effect 参数升级卡
→ 例如 Draw 1 → Draw 2

Armaments
→ 验证 battle temporary upgrade

Searing Blow（后续）
→ 验证 Level N 模型没有被 bool-upgraded 限死
```

不要求第一批就实现 Searing Blow，但模型不能让 Level N 未来需要推倒重来。

---

## 8. Relationship to Ironclad capability plan

`docs/IroncladCardArchitecturePlan.md` 中 CAP-01 仍是 Upgrade 能力域，但其实施优先级调整为：

```text
Phase 8 seal
→ Card Expansion Foundation
   ├─ Upgrade Foundation
   ├─ first normal card content
   └─ focused Upgrade validation
→ other orthogonal capability areas independently
```

这不表示 Upgrade 与 Exhaust / Dynamic Value / Selection 等能力存在依赖关系。

它只是实施优先级：之后所有正式卡牌都应从一开始具备正确的升级表达能力，避免先制作大量基础版内容再整体迁移。

---

## 9. Non-goals for first Upgrade slice

首个 Upgrade Foundation 不自动包含：

```text
campfire UI
reward UI
map/run progression
save/load
shop
card acquisition
full Run Deck ownership
all 75 upgraded card assets
```

这些需要各自单独授权。

---

## 10. Acceptance principles

```text
[ ] UpgradeLevel is not a bool-only dead end
[ ] no CardId-specific upgrade branch
[ ] Upgrade is orthogonal to Damage/Draw/Exhaust/etc.
[ ] all Gameplay/UI read one effective card boundary
[ ] runtime upgrade mutation is Action-authoritative
[ ] normal Level 0/1 card upgrade is data-driven
[ ] design remains extensible to Armaments temporary upgrade
[ ] design remains extensible to Searing Blow Level N
[ ] no unrelated Run/UI systems are pulled into the first slice
```

---

## 11. Next exact action

```text
Do not implement this document during Phase 8.

After Phase 8 is COMPLETE / VALIDATED / SEALED,
select Card Expansion / Upgrade Foundation as the next bounded implementation goal,
review the exact current CardData / CardInstance / Effect contracts,
and authorize implementation explicitly.
```
