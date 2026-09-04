# Card Upgrade Foundation Implementation

日期：**2026-09-04**

状态：**SOURCE IMPLEMENTED / VALIDATION PENDING**

本文件是当前 Card Expansion / Upgrade Foundation 的 implementation authority。用户已明确要求：**简化阶段并开始执行**。

---

## 1. Simplified model

普通卡不建立通用升级表达式、typed delta 系统或多阶段参数解释器。

```text
UCardData
├─ Base configuration        // 现有字段，保持现有资产兼容
└─ optional UpgradedVariant  // 第二套完整 authored configuration

UCardInstance
└─ bool bUpgraded
```

```text
bUpgraded=false
→ Base

bUpgraded=true
→ UpgradedVariant
```

`CardId` 只存在于 `UCardData`；升级前后仍是同一个 CardInstance / RuntimeId。

---

## 2. Implemented data model

`UCardData` 顶层现有字段继续作为 Base：

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

新增 inline `UCardVariantData`：

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

以及：

```text
UCardData.UpgradedVariant
```

没有 `UpgradedVariant` 的卡不能执行普通升级。

---

## 3. Implemented runtime boundary

`UCardInstance` 已新增：

```text
bUpgraded
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

只有 `UCardInstance` 负责 Base/Plus 选择。

Gameplay / Preview / Presentation 不应自行根据 `bUpgraded` 选择 definition 字段。

---

## 4. Implemented mutation

新增：

```text
UUpgradeCardAction
```

合同：

```text
valid Card + UpgradedVariant + !bUpgraded
→ commit false -> true

already upgraded / no UpgradedVariant
→ reject fail-soft
→ Finish
→ no ResolutionFault
```

本 slice 不创建 `CardUpgradedEvent`。

---

## 5. Migrated production consumers

以下 variant-sensitive 路径已切到 `UCardInstance` effective getters：

```text
PlayCardAction
→ target + Effects + committed card-face resolution

BattleTextResolver
→ Description format + target + Effects
→ validates both Base and Upgraded authored configurations

BattleManagerUIA3Preview
→ effective Effects

PresentationCardSnapshotBuilder
→ effective display/cost/type/target/art/text

BattleManager current-state Presentation freeze
→ effective display/type/art
```

现有 `FCardReadView` 已经通过 `GetCurrentCost / GetTargetType` 获得 effective cost/target；description 继续通过已迁移的 `BattleTextResolver` 解析。

---

## 6. Focused Automation source

新增：

```text
SlayTheSpireDemo.CardUpgrade.SingleVariant
SlayTheSpireDemo.CardUpgrade.EffectiveConsumers
```

覆盖：

```text
Base getters
CanUpgrade
UpgradeCardAction one-time commit
second action fail-soft rejection
stable CardId / RuntimeId
Base/Plus authored Effects are distinct
Base Draw 1 -> Plus Draw 2 effective Effects
Base/Plus dynamic description
Base/Plus committed card snapshot
```

A3 implementation本身已经迁移到 `Card->GetEffects()`；由于修改了 sealed A3 生产路径，CardUpgrade focused suite PASS 后应只重跑直接失效的 A3 ImmediatePreview focused gate。

---

## 7. Validation policy

当前工具环境不能执行用户本地 UE5.8 Editor Build / Automation，因此目前不能宣称 PASS。

Gate：

```text
Editor Build once
→ SlayTheSpireDemo.CardUpgrade once
→ if PASS, directly-invalidated A3 ImmediatePreview focused suite once
→ record evidence
→ seal
```

不默认跑 Phase6R / A2D5 / Shipping aggregate gates；只有实际失败证明共享合同被破坏时再扩大。

---

## 8. Explicit non-goals

```text
RepeatableUpgradeCapability
Searing Blow
Armaments
Run Deck persistence
campfire / reward / shop
save/load
CardUpgradedEvent
universal EffectiveCardFacts
upgrade delta/expression language
production .uasset migration
Phase 8
```

重复升级后续单独设计，不污染普通卡的 `bool + two configs` 模型。

---

## 9. Current stop state

```text
[x] Base + UpgradedVariant runtime implemented
[x] UCardInstance effective getters implemented
[x] UpgradeCardAction implemented
[x] variant-sensitive production consumers migrated
[x] focused Automation source added
[x] docs/checkpoint updated
[ ] Editor Build PASS
[ ] CardUpgrade focused Automation PASS
[ ] A3 directly-invalidated regression PASS
[ ] Validation record / seal
```

当前应 STOP 等待用户运行验证命令，不继续做生产卡资产或下一能力。
