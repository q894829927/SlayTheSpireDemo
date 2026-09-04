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
Searing Blow
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

### 3.1 UpgradeLevel

升级状态使用：

```text
UpgradeLevel : int32 >= 0
```

不能只设计：

```text
bool bUpgraded
```

因为完整战士卡池同时存在两类升级行为：

```text
绝大多数卡牌：
Level 0 → Level 1
之后不可继续升级

Searing Blow / 灼热打击：
Level 0 → 1 → 2 → 3 → ...
允许重复升级
```

因此 `UpgradeLevel` 表示当前升级层级，但“还能不能继续升级”不能由 `UpgradeLevel == 0` 这种全局规则决定。

### 3.2 Upgrade limit policy

升级次数限制属于 immutable Card Definition 的 authored rule，而不是具体卡名分支。

推荐最小语义：

```text
ECardUpgradePolicy
├─ None
├─ Single
└─ Repeatable
```

语义：

```text
None
→ 永远不可升级

Single
→ 只允许 Level 0 → Level 1
→ Level >= 1 后 CanUpgrade = false

Repeatable
→ Level N → Level N+1
→ 不由通用系统设置固定上限
```

普通 Ironclad 卡默认使用：

```text
UpgradePolicy = Single
```

灼热打击使用：

```text
UpgradePolicy = Repeatable
```

禁止：

```cpp
if (CardId == "SearingBlow")
{
    // allow another upgrade
}
```

正确方向：

```text
Card Definition
→ UpgradePolicy
→ generic CanUpgrade query
```

如果未来出现有限多次升级的真实卡牌需求，再扩展 policy / MaxUpgradeLevel；当前不要提前增加没有消费者的复杂度。

### 3.3 Ownership layering

具体 ownership 分层：

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

`CanUpgrade` 也应由统一 Upgrade boundary 提供，而不是由 UI 自己推断。

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
upgrade-limit policy
resolution
authoritative mutation
presentation-readable effective facts
```

具体哪一个字段如何变化由卡牌内容定义。

不得：

```cpp
if (CardId == "PommelStrike") DrawCount = 2;
if (CardId == "Bash") Vulnerable += 1;
if (CardId == "SearingBlow") BaseDamage = ...;
```

对于 Repeatable 卡牌，Level N 对数值的影响也必须由可数据化/可解析的升级内容规则表达，而不是由 Upgrade System 识别卡名。

---

## 6. Locked presentation rule — upgrade suffix

升级层级的 Gameplay state 与名称显示规则分离。

卡牌标题显示遵循：

```text
普通 Single-upgrade 卡：
Level 0 → Strike
Level 1 → Strike+

不显示：
Strike+1
```

对于 Repeatable upgrade 卡：

```text
灼热打击 Level 0 → 灼热打击
灼热打击 Level 1 → 灼热打击+1
灼热打击 Level 2 → 灼热打击+2
灼热打击 Level 3 → 灼热打击+3
...
```

因此 Presentation 需要一个通用 upgrade suffix policy，而不是 UI 判断具体 CardId。

推荐 authored semantic：

```text
ECardUpgradeDisplayPolicy
├─ None
├─ Plus
└─ PlusLevel
```

规则：

```text
None
→ 不添加升级后缀

Plus
→ UpgradeLevel > 0 时追加 "+"

PlusLevel
→ UpgradeLevel > 0 时追加 "+{UpgradeLevel}"
```

普通可升级卡：

```text
UpgradePolicy        = Single
UpgradeDisplayPolicy = Plus
```

灼热打击：

```text
UpgradePolicy        = Repeatable
UpgradeDisplayPolicy = PlusLevel
```

最终 UI 不应自己拼接：

```cpp
if (CardId == "SearingBlow")
    Name += FString::Printf(TEXT("+%d"), UpgradeLevel);
else if (UpgradeLevel > 0)
    Name += TEXT("+");
```

而应统一读取：

```text
EffectiveCardFacts.DisplayName
或
EffectiveCardFacts.UpgradeDisplaySuffix
```

从而 Native HUD、A2、A3、卡牌详情等不会出现不同的升级名称格式。

---

## 7. Runtime mutation

战斗中的升级必须仍由 Gameplay ActionQueue 掌权：

```text
Armaments
→ Select Card(s)
→ Query CanUpgrade
→ UpgradeCardAction
→ authoritative UCardInstance state commit
→ optional committed CardUpgraded fact if a real consumer requires it
```

Widget 不能直接修改 UpgradeLevel。

`UpgradeCardAction` 必须服从 Definition 的 UpgradePolicy：

```text
Single Level 1
→ reject further upgrade

Repeatable Level N
→ allow N + 1
```

如果没有真实 Trigger / Presentation consumer，不要为了形式提前创建 `CardUpgradedEvent`。

---

## 8. First implementation consumers

Upgrade Foundation 应和第一批正式卡牌一起验证。

推荐至少覆盖：

```text
一个普通数值升级卡
→ 验证 Level 0 / 1 effective values
→ 验证第二次升级被拒绝
→ 名称从 CardName → CardName+

一个 Effect 参数升级卡
→ 例如 Draw 1 → Draw 2

Armaments
→ 验证 battle temporary upgrade
→ 已升级普通卡不能再次升级

Searing Blow（后续）
→ 验证 Level N repeatable model
→ 名称显示 CardName+1 / +2 / +3 ...
```

不要求第一批就实现 Searing Blow，但模型必须在第一次落地时明确支持：

```text
UpgradePolicy = Repeatable
UpgradeDisplayPolicy = PlusLevel
```

避免以后为了灼热打击推翻 Single-only 模型。

---

## 9. Relationship to Ironclad capability plan

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

## 10. Non-goals for first Upgrade slice

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

## 11. Acceptance principles

```text
[ ] UpgradeLevel is int32 >= 0, not bool-only
[ ] normal upgrade policy allows exactly Level 0 → 1
[ ] second upgrade of a normal Single card is rejected
[ ] repeatable policy supports Level N → N+1 without CardId special case
[ ] normal upgraded card title uses only "+"
[ ] repeatable upgraded card title uses "+1", "+2", ...
[ ] upgrade display policy is data-driven and shared by all UI paths
[ ] no CardId-specific upgrade branch
[ ] Upgrade is orthogonal to Damage/Draw/Exhaust/etc.
[ ] all Gameplay/UI read one effective card boundary
[ ] runtime upgrade mutation is Action-authoritative
[ ] normal Level 0/1 card upgrade is data-driven
[ ] design remains extensible to Armaments temporary upgrade
[ ] design directly supports Searing Blow Level N
[ ] no unrelated Run/UI systems are pulled into the first slice
```

---

## 12. Next exact action

```text
Do not implement this document during Phase 8.

After Phase 8 is COMPLETE / VALIDATED / SEALED,
select Card Expansion / Upgrade Foundation as the next bounded implementation goal,
review the exact current CardData / CardInstance / Effect contracts,
and authorize implementation explicitly.
```
