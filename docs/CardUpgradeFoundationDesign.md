# Card Upgrade Foundation Design

日期：**2026-09-04**

状态：**SUPERSEDED / HISTORICAL DESIGN**

当前 ordinary-card upgrade authority：

```text
docs/CardUpgradeSTSStyleRefactor.md
```

本文件记录此前的中间设计：

```text
UCardData
├─ stable shared fields
├─ Base Description / Cost / Destination / Effects
├─ bHasUpgrade
└─ Upgrade : FCardUpgradeConfig
   ├─ Description
   ├─ Cost
   ├─ Destination
   └─ Effects[]
```

该方案经过进一步代码核验后已被明确取代，不得继续作为实现目标。

---

## Historical decisions that remain valid

以下结论仍有效，并已迁入新的 authority：

```text
one CardId / one runtime identity
UCardData and CardEffect are immutable shared definitions
UCardInstance::bUpgraded is the single ordinary-card mutable upgrade truth
UUpgradeCardAction is the in-combat upgrade mutation boundary
DisplayName/CardArt/CardType/TargetType are not duplicated for ordinary upgrade
Presentation freezes bUpgraded and styles upgraded name gold
DisplayName text itself does not gain '+'
```

---

## Decisions explicitly superseded

以下旧设计不得继续实现或扩展：

```text
bHasUpgrade as authored presence flag
FCardUpgradeConfig as second authored configuration
second Upgrade.Description
second Upgrade.Cost container
second Upgrade.DefaultDestination
second Upgrade.Effects[]
base/upgraded Effect object replacement
"upgrade exists because second config exists" validation
```

新设计改为：

```text
one UCardData
one Effects[]
per-field typed Base / Upgraded authored values
one UCardInstance::bUpgraded
```

具体字段、时序、迁移、资产 parity、测试和验收合同只以：

```text
docs/CardUpgradeSTSStyleRefactor.md
```

为准。

---

## Historical note

此前关于 `UCardVariantData`、slim `FCardUpgradeConfig`、金色名称 Presentation 的实现/验证记录继续作为历史证据保存，但不能证明新 STS-style refactor 已实现或已验证。
