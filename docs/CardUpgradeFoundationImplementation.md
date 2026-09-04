# Card Upgrade Foundation Implementation

日期：**2026-09-04**

状态：**COMPLETE / VALIDATED / SEALED**

本文件是 Card Expansion / Upgrade Foundation 的 implementation authority。用户已明确要求：**简化阶段并开始执行**；随后在本地 UE5.8 环境完成规定 Build 与 focused Automation，并报告全部通过。

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

现有 `FCardReadView` 已通过 `GetCurrentCost / GetTargetType` 获得 effective cost/target；description 继续通过已迁移的 `BattleTextResolver` 解析。

---

## 6. Focused Automation

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

A3 implementation迁移到 `Card->GetEffects()`，因此本 slice 还要求重跑直接失效的 A3 ImmediatePreview focused gate。

---

## 7. Validation evidence

2026-09-04，用户在本地 UE5.8 环境报告以下规定 Gate 全部通过：

```text
SlayTheSpireDemoEditor Win64 Development Build: PASS
SlayTheSpireDemo.CardUpgrade: PASS
SlayTheSpireDemo.UIA3.ImmediatePreview: PASS
```

这些结果属于用户本地执行的 validation evidence；本工具环境没有独立重跑 UE。

本 slice 没有运行、也不需要据此宣称：

```text
full Phase6R aggregate
Shipping exclusion
manual PIE
packaged-game validation
```

详细 seal 记录见：

```text
docs/CardUpgradeFoundationValidation.md
```

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

## 9. Final seal state

```text
[x] Base + UpgradedVariant runtime implemented
[x] UCardInstance effective getters implemented
[x] UpgradeCardAction implemented
[x] variant-sensitive production consumers migrated
[x] focused Automation source added
[x] docs/checkpoint updated
[x] Editor Build PASS
[x] CardUpgrade focused Automation PASS
[x] A3 directly-invalidated regression PASS
[x] Validation record / seal
```

**Card Expansion / Upgrade Foundation is now COMPLETE / VALIDATED / SEALED.**

下一 bounded task 是 production card Base/Plus authoring；本 Foundation 不再继续扩 scope，除非后续真实卡牌证明当前合同不足。
