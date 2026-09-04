# Card Upgrade Foundation Design

日期：**2026-09-04**

状态：**DESIGN SIMPLIFIED / IMPLEMENTED / VALIDATED / SEALED**

实施与验证 authority：

```text
docs/CardUpgradeFoundationImplementation.md
docs/CardUpgradeFoundationValidation.md
```

用户已明确收敛普通卡升级方案：**一张卡只维护升级前、升级后两套 authored configuration；普通升级不建立通用参数 delta / expression framework。**

Phase 8 已 deferred，不是当前 Card Expansion 的前置 Gate。

---

## 1. Normal card model

```text
UCardData
├─ CardId
├─ Base configuration
└─ optional UpgradedVariant
```

Base configuration 继续使用当前 `UCardData` 已有字段，以保持现有 `.uasset` 兼容：

```text
DisplayName
Description
CardArt
CardType
TargetType
BaseCost
DefaultDestination
Effects[]
```

`UpgradedVariant` 是第二套完整配置：

```text
DisplayName
Description
CardArt
CardType
TargetType
Cost
DefaultDestination
Effects[]
```

Base / Plus 永远共享：

```text
CardId
UCardInstance
RuntimeId
```

不为 Plus 创建第二个 card identity。

---

## 2. Runtime upgrade state

普通 `UCardInstance` 只维护：

```text
bool bUpgraded
```

规则：

```text
false → Base
true  → UpgradedVariant

CanUpgrade
= !bUpgraded && UpgradedVariant exists
```

第一次升级 `false -> true`；第二次升级 generic fail-soft reject。

普通卡不使用 `UpgradeLevel`、参数 patch 列表、表达式语言或 Damage/Block/Draw-specific upgrade interpreter。

---

## 3. Effective boundary

所有 variant-sensitive 消费者只通过 `UCardInstance` effective getters 读取当前配置：

```text
GetDisplayName()
GetDescriptionFormat()
GetCardArt()
GetCardType()
GetCurrentCost()
GetTargetType()
ResolveDestination()
GetEffects()
```

生产路径包括：

```text
PlayCardAction
BattleTextResolver
A3 Immediate Preview
current-state Hand freeze
committed CardPlayed snapshot
```

消费者不得自行根据 `bUpgraded` 选择 `Definition` 字段。

---

## 4. Mutation authority

战斗内普通升级遵循 Action authority：

```text
UpgradeCardAction
→ validates UCardInstance
→ commits bUpgraded false -> true
→ Finish
```

Widget 不直接修改 `bUpgraded`。

当前没有独立消费者，因此本 Foundation 不创建 `CardUpgradedEvent`。

---

## 5. Authoring examples

```text
Strike
Base: Damage 6
Plus: Damage 9
```

```text
Defend
Base: Block 5
Plus: Block 8
```

```text
Pommel Strike
Base: Damage + Draw 1
Plus: upgraded Damage + Draw 2
```

两套配置可以拥有不同 authored Effect 参数；普通 Upgrade Runtime 不解释这些差异。

---

## 6. Repeatable upgrades

Searing Blow 等重复升级不属于 ordinary-card Foundation。

```text
普通卡
→ bool + Base/UpgradedVariant

可重复升级卡
→ future optional orthogonal capability
```

不能为了重复升级卡把所有普通卡改成整数 `UpgradeLevel`。

---

## 7. Sealed scope

已实现并验证：

```text
UCardVariantData / UCardData.UpgradedVariant
UCardInstance.bUpgraded + effective getters
UpgradeCardAction
variant-sensitive production consumer migration
focused Automation
```

明确不包含：

```text
Searing Blow
Armaments
Run Deck
campfire
reward/shop
save/load
CardUpgradedEvent
universal upgrade expression/delta system
Phase 8
```

---

## 8. Acceptance

```text
[x] existing cards without UpgradedVariant continue using current Base fields unchanged
[x] a card with UpgradedVariant can upgrade exactly once
[x] Base and Plus keep the same CardId / RuntimeId
[x] second upgrade is rejected fail-soft
[x] variant-sensitive Gameplay path uses UCardInstance effective boundary
[x] A3 uses effective Effects boundary
[x] card text uses effective Description/Effects
[x] current-state and committed snapshots use effective card fields
[x] no Damage/Block/Draw-specific upgrade branch exists
[x] no CardId-specific upgrade branch exists
[x] Editor Build PASS reported by user
[x] CardUpgrade focused Automation PASS reported by user
[x] directly-invalidated A3 ImmediatePreview regression PASS reported by user
```

**This design is sealed.** Production card Base/Plus authoring may now consume it without reopening Foundation design.
