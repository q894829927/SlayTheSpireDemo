# Phase 7E —— 遗物反应组合化验证

日期：**2026-09-04**

状态：**COMPLETE / VALIDATED / SEALED**

本文记录 Phase 7E 的最终验证证据。行为与架构合同以 `docs/Phase7ERelicCompositionDesign.md` 为准，实施状态以 `docs/Phase7EImplementation.md` 为准。

## 最终实现

```text
URelicEffect + FRelicEffectContext
UGainEnergyRelicEffect
UGainBlockRelicEffect
UDeckShuffledCountTrigger
UAdvanceRelicCounterAction
URelicData generic Trigger/DataValidation traversal
URelicInstance generic counter-action friend boundary
```

生产 `DA_Relic_Sundial` 已在 Unreal Editor 中迁移并保存为：

```text
Triggers[0] = UDeckShuffledCountTrigger
RequiredCount = 3
Effects[0] = UGainEnergyRelicEffect
Amount = 2
```

7D Presentation metadata 保持原合同；本阶段没有重构 Relic Frozen DTO / Native UI 语义。

旧的日晷专属实现已在迁移和回归验证之后删除：

```text
USundialTrigger
USundialAdvanceAction
URelicInstance -> friend class USundialAdvanceAction
```

## Automated Gates

用户在 UE 5.8 环境完成并确认：

```text
Development Editor Build                                      PASS

SlayTheSpireDemo.Phase7E.Composite.FailClosedAndLiveMembership PASS
SlayTheSpireDemo.Phase7E.Composite.SequenceOrderAndPresentation PASS
SlayTheSpireDemo.Phase7E.Effects.BuildContracts                PASS
SlayTheSpireDemo.Phase7E.Counter.ThresholdEnqueueFailure        PASS
```

`ThresholdEnqueueFailure` 首次运行时，Gameplay/Queue 状态合同已按预期进入 ResolutionFault，但测试没有注册该负向路径产生的预期 Error 日志，Automation 因此将预期诊断判为失败。测试随后只补充 `AddExpectedErrorPlain()`，没有修改生产逻辑；该 focused Gate 重跑后 PASS。此前三个未被该测试修复影响的 focused Gate 按 repository validation policy 保持 sticky PASS。

生产资产迁移后，以下既有回归前缀均由用户确认 PASS：

```text
SlayTheSpireDemo.Phase7.Sundial            PASS
SlayTheSpireDemo.Phase7.EnergyGain         PASS
SlayTheSpireDemo.Phase7.RelicPresentation  PASS
```

在旧 `USundialTrigger / USundialAdvanceAction` 删除并移除旧 friend 后：

```text
UE project-file regeneration                    PASS
Development Editor Build                        PASS
SlayTheSpireDemo.Phase7.Sundial final regression PASS
```

没有运行无关的 Phase6R、A2D5、Shipping 或其他历史 aggregate suite；当前 Phase 7E acceptance 不要求这些 Gate。

## Manual PIE Gate

用户已在生产测试地图中确认迁移后的日晷实际有效，并在旧类删除后的最终 PIE smoke 再次确认：

```text
日晷资产可正常加载和显示
真实洗牌推进 Counter：0 -> 1 -> 2 -> 0
第三次真实洗牌获得 +2 Energy
无 Missing Class / Failed to load / 崩溃
```

这同时证明保存后的 `DA_Relic_Sundial.uasset` 不再依赖已删除的旧 Sundial UObject 类。

## Seal

Phase 7E 的目标已经完成：

```text
专属 Sundial Trigger/Action
-> 可复用 DeckShuffled Count Trigger
-> 可组合 RelicEffect[]
-> generic Counter Action
```

Build、focused Automation、迁移回归、生产资产 PIE 与旧类删除后的最终 smoke 均已通过。

**Phase 7E is COMPLETE / VALIDATED / SEALED.**
