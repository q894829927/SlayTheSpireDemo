# Card Upgrade Foundation Implementation

日期：**2026-09-04**

状态：**REVIEW FIX IMPLEMENTED / REVALIDATION PENDING**

本文件是 Card Expansion / Upgrade Foundation 的 implementation authority。

---

## 1. Current simplified model

普通卡继续使用：

```text
UCardInstance
└─ bool bUpgraded
```

但升级配置已经进一步收窄，不再使用 `UCardVariantData` 完整复制整张卡。

当前 source shape：

```text
UCardData
├─ stable shared fields
│  ├─ CardId
│  ├─ DisplayName
│  ├─ CardArt
│  ├─ CardType
│  └─ TargetType
│
├─ Base authored fields
│  ├─ Description
│  ├─ BaseCost
│  ├─ DefaultDestination
│  └─ Effects[]
│
├─ bHasUpgrade
└─ Upgrade : FCardUpgradeConfig
   ├─ Description
   ├─ Cost
   ├─ DefaultDestination
   └─ Effects[]
```

Editor 不再创建/选择 `Card Variant Data` UObject。

---

## 2. Why stable fields are not duplicated

普通升级不需要重新填写：

```text
DisplayName
CardArt
CardType
TargetType
```

`DisplayName` 的可见 `+` 从 runtime upgrade state 派生：

```text
Strike → Strike+
```

不是第二个 authored 名字。

---

## 3. Runtime boundary

`UCardInstance` 当前行为：

```text
stable fields
→ always read UCardData top-level

Description / Cost / Destination / Effects
→ bUpgraded ? Upgrade : Base
```

有效 getter：

```text
GetDisplayName()       // upgraded derives +
GetCardArt()           // shared
GetCardType()          // shared
GetTargetType()        // shared
GetDescriptionFormat() // Base/Upgrade
GetCurrentCost()       // Base/Upgrade
ResolveDestination()   // Base/Upgrade
GetEffects()           // Base/Upgrade
```

---

## 4. Mutation

`UUpgradeCardAction` 合同不变：

```text
valid Card + bHasUpgrade + !bUpgraded
→ false -> true

already upgraded / bHasUpgrade=false
→ fail-soft reject
→ Finish
→ no ResolutionFault
```

---

## 5. Production consumers

此前迁移的 consumer boundary 保持不变：

```text
PlayCardAction
BattleTextResolver
BattleManagerUIA3Preview
PresentationCardSnapshotBuilder
BattleManager current-state Presentation freeze
```

review fix 只缩窄 upgrade authoring shape，没有新建第二套 Gameplay path。

---

## 6. Focused Automation

继续使用：

```text
SlayTheSpireDemo.CardUpgrade.SingleConfig
SlayTheSpireDemo.CardUpgrade.EffectiveConsumers
```

测试已更新为验证：

```text
stable DisplayName/CardType/TargetType are not duplicated
upgraded display name derives '+' automatically
Base/Upgrade Description/Cost/Destination/Effects switch correctly
Base/Upgrade share CardId / RuntimeId
second upgrade remains fail-soft
```

---

## 7. Validation state

上一轮用户已经通过：

```text
Editor Build
SlayTheSpireDemo.CardUpgrade
SlayTheSpireDemo.UIA3.ImmediatePreview
```

但该证据对应旧 `UCardVariantData` source shape。

本次 review fix 修改了：

```text
CardData.h
CardInstance.h/.cpp
BattleTextResolver.cpp
CardUpgradeFoundationTests.cpp
```

因此旧 seal 作为历史证据保留，但当前 source 需要重新：

```text
Editor Build once
→ SlayTheSpireDemo.CardUpgrade once
```

本次没有修改 A3 production code，因此上一轮 A3 ImmediatePreview PASS 仍 sticky，不要求再次运行。

---

## 8. Non-goals

仍不做：

```text
RepeatableUpgradeCapability
Searing Blow
Armaments
Run Deck
campfire/reward/shop
save/load
CardUpgradedEvent
Phase 8
```

---

## 9. Stop state

```text
[x] remove UCardVariantData ordinary authoring object
[x] add slim FCardUpgradeConfig
[x] keep name/art/type/target shared
[x] derive '+' from runtime upgrade state
[x] update validation and focused Automation source
[ ] Editor Build PASS on review-fix head
[ ] SlayTheSpireDemo.CardUpgrade PASS on review-fix head
[ ] restore validation seal
```

当前不继续 production card authoring，直到这两个直接失效 Gate 重新通过。
