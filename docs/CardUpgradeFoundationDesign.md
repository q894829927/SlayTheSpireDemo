# Card Upgrade Foundation Design

日期：**2026-09-04**

状态：**DESIGN REFINED / REVIEW FIX IMPLEMENTED / REVALIDATION PENDING**

当前 implementation authority：

```text
docs/CardUpgradeFoundationImplementation.md
```

用户进一步收敛普通卡升级模型：**不会因普通升级而变化的字段不应重复配置。**

---

## 1. Ordinary card shape

一张卡仍只有一个 `UCardData`、一个 `CardId` 和一个运行时身份。

稳定字段只配置一次：

```text
UCardData
├─ CardId
├─ DisplayName
├─ CardArt
├─ CardType
├─ TargetType
├─ Base Description / Cost / Destination / Effects
├─ bHasUpgrade
└─ Upgrade : FCardUpgradeConfig
```

`FCardUpgradeConfig` 只包含普通升级真正可能变化的内容：

```text
Description
Cost
DefaultDestination
Effects[]
```

明确不重复：

```text
CardId
DisplayName
CardArt
CardType
TargetType
```

因此 Editor 中不再出现 `Card Variant Data` UObject，也不会要求为升级卡重复填写名字、图标、类型和目标类型。

---

## 2. Runtime state

普通卡仍只需要：

```text
bool bUpgraded
```

```text
bUpgraded = false
→ Description / Cost / Destination / Effects 读取 Base

bUpgraded = true
→ Description / Cost / Destination / Effects 读取 Upgrade
```

稳定字段始终读取 `UCardData` 顶层。

```text
CanUpgrade
= !bUpgraded && bHasUpgrade
```

第一次升级：`false -> true`。

第二次升级：generic fail-soft reject。

---

## 3. Display name

卡牌名称本体不重复 author。

```text
DisplayName = "Strike"
```

升级后的 `+` 是 runtime/presentation-derived indicator：

```text
Base     → Strike
Upgraded → Strike+
```

不需要再填写一个 `DisplayName = Strike+`。

`CardArt` 同样始终复用同一资源。

---

## 4. Effective boundary

`UCardInstance` 继续作为唯一 effective boundary。

稳定 getter：

```text
GetDisplayName()   // upgraded 时自动派生 +
GetCardArt()
GetCardType()
GetTargetType()
```

升级敏感 getter：

```text
GetDescriptionFormat()
GetCurrentCost()
ResolveDestination()
GetEffects()
```

Gameplay / A3 / Presentation 不允许自行读取 `bUpgraded` 后选择字段。

---

## 5. Why Destination remains in Upgrade

名字、图标、CardType、TargetType 对普通 Ironclad 升级没有必要重复；但 `DefaultDestination` 必须保留，因为真实升级内容可能改变 Exhaust/Discard 行为。

同理，Description / Cost / Effects 是普通升级的核心差异面。

---

## 6. Mutation authority

```text
UUpgradeCardAction
→ UCardInstance::CommitUpgrade()
→ false -> true
→ Finish
```

Widget 不直接修改 `bUpgraded`。

当前仍不创建 `CardUpgradedEvent`。

---

## 7. Repeatable upgrade

Searing Blow 不进入本 ordinary-card model。

```text
普通卡
→ bool + Base fields + slim Upgrade config

可重复升级卡
→ future optional capability
```

不得为 Searing Blow 把全部普通卡改成全局 `UpgradeLevel`。

---

## 8. Acceptance

```text
[ ] no Card Variant Data UObject in ordinary upgrade authoring
[ ] Upgrade editor section contains only Description / Cost / Destination / Effects
[ ] DisplayName is authored once and upgraded '+' is derived
[ ] CardArt is authored once
[ ] CardType is authored once
[ ] TargetType is authored once
[ ] Base/Upgrade share CardId and RuntimeId
[ ] upgrade commits exactly once
[ ] actual Gameplay / A3 / Presentation consume the effective Description/Cost/Destination/Effects
[ ] no CardId-specific or Effect-type-specific upgrade branch
```

本次 review fix 修改了已验证 source contract，因此上一轮 seal evidence 变为历史证据；需要重新 Build + focused `SlayTheSpireDemo.CardUpgrade` 后才能恢复 seal。
