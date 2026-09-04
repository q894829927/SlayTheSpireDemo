# Card Upgrade Foundation Validation

日期：**2026-09-04**

状态：**HISTORICAL PASS / SUPERSEDED BY REVIEW FIX / REVALIDATION PENDING**

## Historical validated scope

更早的 Upgrade Foundation 版本曾由用户在本地 UE5.8 环境验证通过：

```text
SlayTheSpireDemoEditor Win64 Development Build: PASS
SlayTheSpireDemo.CardUpgrade: PASS
SlayTheSpireDemo.UIA3.ImmediatePreview: PASS
```

这些证据是真实历史证据并继续保留。

## Current review-fix source

用户随后进一步收敛 ordinary-card authoring 与升级名称表现。

当前数据模型：

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

当前升级名称合同：

```text
DisplayName text is stable
Base     → default Designer name color
Upgraded → gold name color
```

不自动拼接 `+`，也不 author 第二个名字。

Presentation state path：

```text
UCardInstance::bUpgraded
→ FPresentationCardSnapshot.bUpgraded
→ FBattleHUDCardView.bUpgraded
→ UBattleCardWidget
→ gold Txt_CardName
```

Current Hand freeze 同样冻结 `bUpgraded` 到 `FBattleHUDCardView`。

## Why the previous seal is not current-head evidence

在历史 PASS 后，当前 head 修改了：

```text
CardData / CardInstance ordinary upgrade shape
BattleText definition validation
CardUpgrade focused tests
FBattleHUDCardView
FPresentationCardSnapshot
PresentationCardSnapshotBuilder
PresentationCardView
BattleManager current-state Presentation freeze
UBattleCardWidget name styling
```

因此之前的 COMPLETE / VALIDATED / SEALED 状态不能直接覆盖当前 head。

## Required revalidation

当前只重跑直接失效 Gate：

```text
1. SlayTheSpireDemoEditor Win64 Development Build
2. SlayTheSpireDemo.CardUpgrade
3. SlayTheSpireDemo.Phase6UIA2D4.PresentationCardViewMapper
4. SlayTheSpireDemo.Phase6UIA2N.R4
```

此前：

```text
SlayTheSpireDemo.UIA3.ImmediatePreview: PASS
```

保持 sticky，因为 gold-name review fix 没有修改 A3 production source。

当前也不要求 Phase6R / A2D5 / Shipping aggregate gate。

## Current claim boundary

当前只能声明：

```text
review-fix source implemented
static contract synchronized
revalidation pending
```

只有四项直接失效 Gate 全部 PASS 后，才恢复：

```text
Card Expansion / Upgrade Foundation
COMPLETE / VALIDATED / SEALED
```
