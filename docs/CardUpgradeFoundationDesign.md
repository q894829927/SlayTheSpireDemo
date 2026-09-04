# Card Upgrade Foundation Design

日期：**2026-09-04**

状态：**DESIGN SIMPLIFIED / IMPLEMENTATION ACTIVE**

当前实施 authority：

```text
docs/CardUpgradeFoundationImplementation.md
```

用户已明确收敛普通卡升级方案：**一张卡只维护升级前、升级后两套 authored configuration；普通升级不建立通用参数 delta / expression framework。**

Phase 8 已 deferred，不是当前 Card Expansion 的前置 Gate。

---

## 1. Normal card model

普通卡只有同一个卡牌定义与同一个运行时身份：

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

不创建：

```text
DA_Strike
DA_StrikePlus
```

两个独立 card identity。Base / Plus 永远共享：

```text
CardId
UCardInstance
RuntimeId
```

---

## 2. Runtime upgrade state

普通 `UCardInstance` 只需要：

```text
bool bUpgraded
```

规则：

```text
bUpgraded=false
→ Base configuration

bUpgraded=true
→ UpgradedVariant
```

```text
CanUpgrade
= !bUpgraded && UpgradedVariant exists
```

第一次升级：

```text
false -> true
```

第二次升级：

```text
reject generically / fail-soft
```

不需要 UpgradeLevel，不需要参数 patch 列表，不需要按 Damage / Draw / Block 分别实现升级解释。

---

## 3. Effective boundary

所有消费者只能通过 `UCardInstance` effective getters 读取当前配置：

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

因此：

```text
PlayCardAction
BattleTextResolver
A3 Immediate Preview
current-state Hand freeze
committed CardPlayed snapshot
```

都不自行判断 `bUpgraded`，也不直接读取 variant-sensitive `Definition->Effects / TargetType / DisplayName / ...`。

这不是 Universal EffectiveCardFacts；只是 `UCardInstance` 对 Base / Plus 两套完整配置做一个统一选择。

---

## 4. Mutation authority

战斗内升级继续遵循项目 Action authority：

```text
UpgradeCardAction
→ validates UCardInstance
→ commits bUpgraded false -> true
→ Finish
```

Widget 不直接修改 `bUpgraded`。

当前没有真实独立消费者，因此不创建 `CardUpgradedEvent`。

---

## 5. Authoring examples

```text
Strike
Base:
→ Damage 6

Plus:
→ Damage 9
```

```text
Defend
Base:
→ Block 5

Plus:
→ Block 8
```

```text
Pommel Strike
Base:
→ Damage / Draw 1

Plus:
→ upgraded Damage / Draw 2
```

两套配置允许未来 Effect 数量或类型不同；普通 Upgrade Runtime 不需要知道两套配置具体差异是什么。

---

## 6. Repeatable upgrades

Searing Blow 等可重复升级不进入当前 ordinary-card slice。

锁定原则仍是：

```text
普通卡
→ bool + Base/UpgradedVariant

可重复升级卡
→ future optional orthogonal capability
```

不能为了 Searing Blow 把所有普通卡重新改成整数 UpgradeLevel。

Repeatable capability 的具体 API 等真实实施 Searing Blow 前再单独定稿。

---

## 7. Current implementation scope

当前只做：

```text
UCardVariantData / UCardData.UpgradedVariant
UCardInstance.bUpgraded + effective getters
UpgradeCardAction
variant-sensitive production consumers migration
focused Automation
```

不做：

```text
Searing Blow
Armaments
Run Deck
campfire
reward/shop
save/load
CardUpgradedEvent
universal upgrade expression/delta system
```

---

## 8. Acceptance

```text
[ ] existing cards without UpgradedVariant continue using current Base fields unchanged
[ ] a card with UpgradedVariant can upgrade exactly once
[ ] Base and Plus keep the same CardId / RuntimeId
[ ] second upgrade is rejected fail-soft
[ ] actual Gameplay reads Plus Effects after upgrade
[ ] A3 reads Plus Effects after upgrade
[ ] card text reads Plus Description/Effects after upgrade
[ ] current-state and committed snapshots freeze Plus display/cost/type/target/art/text
[ ] no Damage/Block/Draw-specific upgrade branch exists
[ ] no CardId-specific upgrade branch exists
```

当前 implementation 仍需本地 Build + focused Automation 验证后才能 seal。
