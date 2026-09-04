# Card Upgrade Foundation Implementation

日期：**2026-09-04**

状态：**ACTIVE / IMPLEMENTATION AUTHORIZED**

本文件是当前 Card Expansion / Upgrade Foundation 的实施 authority。用户已明确要求：**简化阶段并开始执行**。

---

## 1. Simplified scope

普通卡牌不建立通用升级表达式、typed delta 系统或多阶段参数解释器。

每张普通卡直接维护两套配置：

```text
UCardData
├─ Base configuration        // 现有字段，保持现有资产兼容
└─ optional UpgradedVariant  // 第二套完整配置
```

运行时只需要：

```text
UCardInstance.bUpgraded
false → Base configuration
true  → UpgradedVariant
```

所有消费者只读取 `UCardInstance` 的 effective getters，不自行判断 `bUpgraded`。

---

## 2. Base / upgraded configuration

为兼容现有 `.uasset`，当前 `UCardData` 顶层字段继续作为 Base configuration：

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

新增 `UCardVariantData` 作为可内联编辑的 upgraded configuration：

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

`CardId` 仍只存在于 `UCardData`，升级前后是同一张卡、同一个 CardId、同一个 RuntimeId。

没有 `UpgradedVariant` 的卡不能升级。

---

## 3. Runtime boundary

`UCardInstance` 新增：

```text
bool bUpgraded = false

IsUpgraded()
CanUpgrade()
GetDisplayName()
GetDescriptionFormat()
GetCardArt()
GetCardType()
GetCurrentCost()
GetTargetType()
ResolveDestination()
GetEffects()
```

这些 getter 是唯一 effective-card boundary：

```text
bUpgraded=false
→ read Base configuration

bUpgraded=true
→ read UpgradedVariant
```

禁止 Gameplay/A3/Presentation 自己写：

```cpp
if (Card->IsUpgraded()) { ... }
```

来选择字段。

---

## 4. Upgrade mutation

新增普通 `UUpgradeCardAction`：

```text
UpgradeCardAction
→ Execute
→ validate CardInstance
→ commit false -> true
→ Finish
```

规则：

```text
CanUpgrade = !bUpgraded && UpgradedVariant exists
first upgrade  = commit
second upgrade = generic reject / fail-soft / Finish
```

本 slice 不创建 `CardUpgradedEvent`，不做 UI 按钮，不做 Run Deck/campfire/save-load。

---

## 5. Effective consumers to migrate now

本 slice 只修改当前直接读取 Card definition variant-sensitive 字段的生产路径：

```text
PlayCardAction
BattleTextResolver
BattleManagerUIA3Preview
PresentationCardSnapshotBuilder
BattleManager current-state Presentation freeze
```

目标：

```text
actual Gameplay
A3 preview
current Hand read/freeze
committed CardPlayed snapshot
```

全部读取同一 `UCardInstance` effective configuration。

---

## 6. Validation

新增最小 focused Automation：

```text
SlayTheSpireDemo.CardUpgrade.SingleVariant
```

至少验证：

```text
Base state reads Base config
CanUpgrade true only when UpgradedVariant exists
UpgradeCardAction commits exactly once
second upgrade rejected without ResolutionFault
upgraded cost/target/destination/display/description/effects come from UpgradedVariant
base and upgraded Effects are distinct authored objects
```

再增加一个 integrated effective-value test：

```text
SlayTheSpireDemo.CardUpgrade.EffectiveConsumers
```

验证至少一张 transient card：

```text
Base  -> Damage value A
Upgrade
Plus  -> Damage value B
```

并确认 text/snapshot/preview 使用 upgraded config，而不是 Base `Definition->Effects`。

验证策略：

```text
Build once
→ SlayTheSpireDemo.CardUpgrade focused suite once
→ only directly invalidated regressions if needed
→ record evidence
→ STOP
```

当前工具环境不能执行本地 UE Build/Automation；代码提交后不得宣称 PASS，必须给用户精确命令执行。

---

## 7. Explicit non-goals

本 slice 不做：

```text
RepeatableUpgradeCapability
Searing Blow
Armaments
Run Deck persistence
campfire/reward/shop UI
CardUpgradedEvent
universal EffectiveCardFacts
upgrade delta/expression language
production .uasset migration beyond user/editor follow-up
```

重复升级后续单独设计，不污染普通卡的简单 bool + two-config 模型。

---

## 8. Stop condition

当以下完成即 STOP：

```text
[ ] simplified Base + UpgradedVariant runtime implemented
[ ] UCardInstance effective getters implemented
[ ] UpgradeCardAction implemented
[ ] variant-sensitive production consumers migrated
[ ] focused Automation source added
[ ] docs/checkpoint updated to VALIDATION PENDING
[ ] exact Build + focused Automation commands provided to user
```
