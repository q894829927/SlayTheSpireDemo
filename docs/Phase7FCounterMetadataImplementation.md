# Phase 7F — Relic Counter Metadata Unification

日期：**2026-09-04**

状态：**COMPLETE / VALIDATED / SEALED**

用户已明确授权并完成 Phase 7F。Phase 7A–7E 保持 sealed；本阶段只消除 Relic counter threshold 的重复 authored metadata，不重新打开既有 Gameplay / Presentation 行为合同。

## 目标

Counter threshold 只有一个权威 authored 来源：

```text
URelicCountTrigger::RequiredCount
```

`URelicData::CounterDisplayMax` 已删除。`FBattleHUDRelicView::CounterMax` 继续保留，但它是 freeze 后的值 DTO，不是第二份配置。

## 最终实现

```text
URelicCountTrigger : UBattleTrigger
└─ RequiredCount

UDeckShuffledCountTrigger : URelicCountTrigger

URelicData
├─ bShowCounter                  // 保留：Presentation choice
├─ Triggers[]
└─ TryGetCounterMax()            // 从唯一 URelicCountTrigger 派生

RelicPresentationSnapshot
└─ CounterMax = authoritative RequiredCount at freeze time
```

`URelicData::CounterDisplayMax` 已从反射属性和 C++ 类型中删除。

## 单 Counter runtime 约束

当前 `URelicInstance` 只有一个：

```text
int32 Counter
```

因此一个 Relic definition 最多只能拥有一个 `URelicCountTrigger`。DataValidation 规则为：

```text
0 个 CountTrigger + bShowCounter=false  -> valid
1 个 CountTrigger                       -> valid（RequiredCount 仍需 > 0）
>1 个 CountTrigger                      -> invalid
bShowCounter=true                       -> 必须唯一解析出一个正 RequiredCount 的 CountTrigger
```

## 保持不变

```text
bShowCounter 继续由 Presentation authored choice 决定
FBattleHUDRelicView 继续包含 bShowCounter / Counter / CounterMax
Widget 仍只消费 frozen HUD DTO
Widget 不查询 Trigger / RelicData
不按 RelicId / Sundial / DeckShuffled 做 Presentation 特判
不修改 WBP
不修改 Counter mutation authority
不修改 Phase 7E reaction composition / queue semantics
```

历史 7D/7E 文档中关于 `CounterDisplayMax` 的文字继续作为当时 sealed 状态的历史记录；Phase 7F supersede 该特定 metadata contract，不回写历史证据。

## 测试迁移

新增：

```text
Source/SlayTheSpireDemoTests/Private/Phase7FCounterMetadataTests.cpp

SlayTheSpireDemo.Phase7F.CounterMetadata.SingleSource
SlayTheSpireDemo.Phase7F.CounterMetadata.InvalidDefinitions
```

既有测试已迁移：

```text
Phase7DRelicPresentationTests.cpp
- 不再写 CounterDisplayMax
- counter presentation fixture 使用 count trigger threshold
- FreezeContract 验证 CounterMax 从 Gameplay threshold 派生

Phase7ERelicCompositionTests.cpp
- 删除 CounterDisplayMax fixture 配置
```

## 生产资产

生产 `DA_Relic_Sundial` 已在 Unreal Editor 中重新保存，并确认：

```text
bShowCounter = true
CounterDisplayMax = 不再存在
Triggers[0] = UDeckShuffledCountTrigger
RequiredCount = 3
Effects[0] = UGainEnergyRelicEffect
Amount = 2
```

## Validation

用户已完成并确认：

```text
UE project-file regeneration                                   PASS
Development Editor Build                                       PASS
SlayTheSpireDemo.Phase7F.CounterMetadata.SingleSource          PASS
SlayTheSpireDemo.Phase7F.CounterMetadata.InvalidDefinitions    PASS
SlayTheSpireDemo.Phase7E                                       PASS
SlayTheSpireDemo.Phase7.Sundial                                PASS
SlayTheSpireDemo.Phase7.RelicPresentation                      PASS
Production Sundial PIE smoke                                   PASS
```

PIE 中日晷继续按 `0 -> 1 -> 2 -> 0` 推进，并在第三次真实洗牌获得 `+2 Energy`；无 Missing Class / Failed to load / 崩溃。

详细最终证据见：

```text
docs/Phase7FValidation.md
```

## Seal

Phase 7F 已完成且无剩余实施 Gate。

```text
RequiredCount = sole authored counter threshold
CounterMax    = frozen Presentation value derived from RequiredCount
```

**Phase 7F is COMPLETE / VALIDATED / SEALED.**
