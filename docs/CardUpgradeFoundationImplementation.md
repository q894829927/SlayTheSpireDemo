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

普通升级配置已经收窄，不使用 `UCardVariantData` 完整复制整张卡。

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

## 2. Stable fields and upgraded name presentation

普通升级不重复填写：

```text
DisplayName
CardArt
CardType
TargetType
```

`DisplayName` 文本在升级前后完全相同：

```text
Base     → Strike
Upgraded → Strike
```

不再自动拼接 `+`。

升级状态通过 Presentation 表达：

```text
Base name     → Designer/default color
Upgraded name → gold
```

`bUpgraded` 会冻结进入：

```text
FPresentationCardSnapshot
FBattleHUDCardView
```

Native `UBattleCardWidget` 只读取冻结 DTO；当 `bUpgraded=true` 时把 `Txt_CardName` 渲染为金色，不反向查询或修改 Gameplay。

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
GetDisplayName()       // shared authored text, no suffix
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

Gameplay effective-value consumer boundary保持：

```text
PlayCardAction
BattleTextResolver
BattleManagerUIA3Preview
PresentationCardSnapshotBuilder
BattleManager current-state Presentation freeze
```

本次 name-style review fix 额外扩展了 Presentation state propagation：

```text
UCardInstance::IsUpgraded()
→ FPresentationCardSnapshot.bUpgraded
→ PresentationCardView
→ FBattleHUDCardView.bUpgraded
→ UBattleCardWidget
→ gold Txt_CardName
```

当前 Hand freeze 也直接冻结 `Card->IsUpgraded()` 到 `FBattleHUDCardView.bUpgraded`，因此 current-state 与 historical Card snapshot 使用同一个 presentation fact。

---

## 6. Focused Automation

Upgrade focused suite：

```text
SlayTheSpireDemo.CardUpgrade.SingleConfig
SlayTheSpireDemo.CardUpgrade.EffectiveConsumers
```

当前验证：

```text
stable DisplayName/CardType/TargetType are not duplicated
upgraded DisplayName text remains identical
Base/Upgrade Description/Cost/Destination/Effects switch correctly
Base/Upgrade share CardId / RuntimeId
FPresentationCardSnapshot freezes bUpgraded
second upgrade remains fail-soft
```

Presentation mapper focused test也已扩展为验证：

```text
FPresentationCardSnapshot.bUpgraded
→ FBattleHUDCardView.bUpgraded
```

Native R4 card widget 是本次修改的直接 regression owner。

---

## 7. Validation state

更早版本用户已经通过：

```text
Editor Build
SlayTheSpireDemo.CardUpgrade
SlayTheSpireDemo.UIA3.ImmediatePreview
```

这些结果保留为历史/sticky evidence，但当前 head 后续又修改了 upgrade authoring shape 和 upgraded-name Presentation DTO/widget。

当前直接失效 Gate：

```text
Editor Build once
→ SlayTheSpireDemo.CardUpgrade once
→ SlayTheSpireDemo.Phase6UIA2D4.PresentationCardViewMapper once
→ SlayTheSpireDemo.Phase6UIA2N.R4 once
```

A3 production source没有在本轮 gold-name fix 中修改，因此既有：

```text
SlayTheSpireDemo.UIA3.ImmediatePreview: PASS
```

保持 sticky，不要求再次运行。

不默认扩大到 Phase6R / A2D5 / Shipping aggregate gates。

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
[x] keep upgraded DisplayName text unchanged
[x] freeze bUpgraded into current/historical presentation DTOs
[x] render upgraded Native card name gold
[x] update focused Automation source
[ ] Editor Build PASS on current head
[ ] SlayTheSpireDemo.CardUpgrade PASS
[ ] SlayTheSpireDemo.Phase6UIA2D4.PresentationCardViewMapper PASS
[ ] SlayTheSpireDemo.Phase6UIA2N.R4 PASS
[ ] restore validation seal
```

当前不继续 production card authoring，直到这些直接失效 Gate 通过。
