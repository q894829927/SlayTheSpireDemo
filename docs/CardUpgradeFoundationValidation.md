# Card Upgrade Foundation Validation

日期：**2026-09-04**

状态：**COMPLETE / VALIDATED / SEALED**

## Scope

本记录只覆盖简化后的普通卡升级 Foundation：

```text
UCardData Base configuration
+ optional UpgradedVariant
+ UCardInstance.bUpgraded
+ effective getters
+ UUpgradeCardAction
+ variant-sensitive Gameplay / Preview / Presentation migration
```

不覆盖 Repeatable Upgrade、Searing Blow、Armaments、Run Deck、campfire/reward/shop、save/load、Phase 8 或生产卡资产批量迁移。

## Validation source

当前 ChatGPT/GitHub 工具环境不能执行用户本地 UE5.8 Editor。以下结果由用户在规定命令执行后于 2026-09-04 明确报告通过。

## Gates

```text
1. SlayTheSpireDemoEditor Win64 Development Build
   PASS

2. SlayTheSpireDemo.CardUpgrade focused Automation
   PASS

3. SlayTheSpireDemo.UIA3.ImmediatePreview directly-invalidated focused regression
   PASS
```

未记录具体测试总数，因此本记录不推断或补写 count。

## What this validates

结合 source review 与上述 Gates，可关闭当前 implementation authority 规定的 validation boundary：

```text
Base / UpgradedVariant source shape compiles
UCardInstance effective boundary compiles and focused contracts pass
ordinary upgrade commits once and second attempt stays fail-soft
Base/Plus stable identity contracts pass
Base/Plus effective authored values reach focused text/snapshot consumers
A3 ImmediatePreview existing focused contract remains passing after migration to Card->GetEffects()
```

## What this does not claim

本次没有据此宣称：

```text
manual PIE PASS
full Phase6R aggregate PASS on this head
Shipping exclusion PASS on this head
packaged-game PASS
production card .uasset migration PASS
repeatable upgrade behavior
```

这些均不属于本 slice 的 seal Gate。

## Seal decision

当前 implementation document 规定的唯一失效回归为 A3 ImmediatePreview；该 gate 与 CardUpgrade focused suite、Editor Build 均已通过。

因此：

```text
Card Expansion / Upgrade Foundation
= COMPLETE / VALIDATED / SEALED
```

Passing Gates remain sticky。后续 production card authoring 不应重跑本 Foundation Gate，除非修改了这里的共享合同或出现 concrete regression。
