# Card Upgrade Foundation Validation

日期：**2026-09-04**

状态：**HISTORICAL PASS / SUPERSEDED BY REVIEW FIX / REVALIDATION PENDING**

## Historical validated scope

上一轮简化 Foundation 曾以以下 source shape 通过用户本地验证：

```text
UCardData Base configuration
+ optional UCardVariantData UpgradedVariant
+ UCardInstance.bUpgraded
+ effective getters
+ UUpgradeCardAction
```

用户当时明确报告：

```text
SlayTheSpireDemoEditor Win64 Development Build: PASS
SlayTheSpireDemo.CardUpgrade: PASS
SlayTheSpireDemo.UIA3.ImmediatePreview: PASS
```

该证据是真实历史证据，不删除。

## Review fix that invalidated the old seal

随后用户指出升级配置不应重复名字、图标等稳定字段。

当前 source 已改为：

```text
stable shared UCardData fields
├─ CardId
├─ DisplayName
├─ CardArt
├─ CardType
└─ TargetType

Base
├─ Description
├─ BaseCost
├─ DefaultDestination
└─ Effects[]

Upgrade : FCardUpgradeConfig
├─ Description
├─ Cost
├─ DefaultDestination
└─ Effects[]
```

同时移除了 ordinary-card `UCardVariantData` authoring object，并让 `GetDisplayName()` 在升级后从同一个 authored name 派生 `+`。

因此上一轮 COMPLETE / VALIDATED / SEALED 状态不能直接覆盖当前 head。

## Required revalidation

本次 review fix 直接失效的 Gate 只有：

```text
1. SlayTheSpireDemoEditor Win64 Development Build
2. SlayTheSpireDemo.CardUpgrade focused Automation
```

A3 production source没有在本 review fix 中再次修改，因此上一轮：

```text
SlayTheSpireDemo.UIA3.ImmediatePreview: PASS
```

保持 sticky，不要求重跑。

## Current claim boundary

当前只能声明：

```text
Review-fix source implemented
Static source shape updated
Revalidation pending
```

不能重新声明 Foundation sealed，直到上述两个 Gate PASS。
