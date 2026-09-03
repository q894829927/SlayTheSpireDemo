# Phase 7F — Relic Counter Metadata Unification 验证

日期：**2026-09-04**

状态：**COMPLETE / VALIDATED / SEALED**

本文记录 Phase 7F 的最终验证证据。实施边界以 `docs/Phase7FCounterMetadataImplementation.md` 为准。Phase 7A–7E 的既有 Gameplay / Presentation 行为合同保持 sealed；7F 只统一 counter threshold metadata 的权威来源。

## 最终实现

```text
URelicCountTrigger : UBattleTrigger
└─ RequiredCount                  // 唯一 authored threshold

UDeckShuffledCountTrigger : URelicCountTrigger

URelicData
├─ bShowCounter                  // Presentation choice，保留
├─ Triggers[]
└─ TryGetCounterMax()            // 从唯一 URelicCountTrigger 派生

RelicPresentationSnapshot
└─ FBattleHUDRelicView.CounterMax = frozen RequiredCount
```

`URelicData::CounterDisplayMax` 已从 C++ / Reflection 中删除。`FBattleHUDRelicView::CounterMax` 继续存在，但只是冻结后的 value DTO，不是第二份 authored configuration。

当前单 Counter runtime 约束继续成立：一个 `URelicData` 最多拥有一个 `URelicCountTrigger`；`bShowCounter=true` 时必须唯一解析出一个正的 `RequiredCount`。

## Build / Focused Automation

用户在 UE 5.8 本地环境完成并确认：

```text
UE project-file regeneration                                   PASS
Development Editor Build                                       PASS

SlayTheSpireDemo.Phase7F.CounterMetadata.SingleSource          PASS
SlayTheSpireDemo.Phase7F.CounterMetadata.InvalidDefinitions    PASS
```

Focused Automation 证明：

```text
UDeckShuffledCountTrigger 使用 generic URelicCountTrigger contract
CounterMax 只从 RequiredCount 派生
修改 RequiredCount 后 frozen CounterMax 同步变化
不存在第二份 authored max 可与 Gameplay 漂移
隐藏 counter 时 frozen CounterMax 归零
无 CountTrigger / 多 CountTrigger / 非正 RequiredCount 的可见 counter 定义 fail-closed
```

## Production Asset Migration

用户在 Unreal Editor 中打开并重新保存生产 `DA_Relic_Sundial`，并确认：

```text
bShowCounter = true
CounterDisplayMax = 不再存在
Triggers[0] = UDeckShuffledCountTrigger
RequiredCount = 3
Effects[0] = UGainEnergyRelicEffect
Amount = 2
```

该保存步骤清理了已从反射类型中删除的旧 `CounterDisplayMax` serialized property，同时保持日晷 Gameplay threshold 和奖励配置不变。

## Regression Gates

资产保存后，用户完成并确认以下回归前缀全部 PASS：

```text
SlayTheSpireDemo.Phase7E                    PASS
SlayTheSpireDemo.Phase7.Sundial             PASS
SlayTheSpireDemo.Phase7.RelicPresentation   PASS
```

没有运行无关的 Phase6R、A2D5、Shipping 或其他历史 aggregate suite；Phase 7F 的 bounded cleanup 不要求这些 Gate。

## Final PIE Smoke

用户完成生产 Sundial PIE smoke 并确认：

```text
日晷正常加载和显示
Counter 真实推进：0 -> 1 -> 2 -> 0
第三次真实洗牌获得 +2 Energy
CounterDisplayMax 不再存在
无 Missing Class / Failed to load / 崩溃
```

这证明生产资产在删除重复 metadata 后仍保持既有 Gameplay 与 Presentation 行为。

## Seal

Phase 7F 完成了单一事实来源收敛：

```text
Before:
RequiredCount       = Gameplay threshold
CounterDisplayMax   = duplicated authored Presentation max

After:
RequiredCount       = sole authored threshold
CounterMax          = frozen value derived from RequiredCount
```

Build、focused Automation、生产资产迁移、7E/Sundial/RelicPresentation 回归以及最终 PIE smoke 均已通过。

**Phase 7F is COMPLETE / VALIDATED / SEALED.**
