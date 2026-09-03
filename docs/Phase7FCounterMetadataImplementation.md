# Phase 7F — Relic Counter Metadata Unification

日期：**2026-09-04**

状态：**IMPLEMENTATION AUTHORIZED / SOURCE IMPLEMENTED / VALIDATION PENDING**

用户已明确授权按本文件范围实施 Phase 7F。Phase 7A–7E 保持 sealed；本阶段只消除 Relic counter threshold 的重复 authored metadata，不重新打开既有 Gameplay / Presentation 行为合同。

## 目标

Counter threshold 只有一个权威 authored 来源：

```text
URelicCountTrigger::RequiredCount
```

删除 `URelicData::CounterDisplayMax`。`FBattleHUDRelicView::CounterMax` 继续保留，但它是 freeze 后的值 DTO，不是第二份配置。

## 实现边界

已实现：

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

这避免在现有单 Counter runtime 上伪造多个独立计数 mechanic。

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

历史 7D/7E 文档中关于 `CounterDisplayMax` 的文字是当时 sealed 状态的历史记录；Phase 7F 从本阶段开始 supersede 该特定 metadata contract，不回写历史证据。

## 测试迁移

新增：

```text
Source/SlayTheSpireDemoTests/Private/Phase7FCounterMetadataTests.cpp

SlayTheSpireDemo.Phase7F.CounterMetadata.SingleSource
SlayTheSpireDemo.Phase7F.CounterMetadata.InvalidDefinitions
```

测试覆盖：

```text
UDeckShuffledCountTrigger 继承 generic URelicCountTrigger
TryGetCounterMax 只读取 RequiredCount
修改 RequiredCount 后 frozen CounterMax 同步变化，不存在第二份 authored max
bShowCounter=false 时 frozen CounterMax 归零
无 CountTrigger / 多 CountTrigger / 非正 RequiredCount 的可见 Counter freeze fail-closed
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

## 生产资产迁移

`DA_Relic_Sundial` 当前已具有：

```text
bShowCounter = true
Triggers[0] = UDeckShuffledCountTrigger
RequiredCount = 3
Effects[0] = UGainEnergyRelicEffect
Amount = 2
```

由于 `CounterDisplayMax` 已从 C++ 反射类型删除，源码 Build 通过后需在 Unreal Editor 打开并重新保存 `DA_Relic_Sundial`，确认 Details 中不再存在 `CounterDisplayMax`，并让保存后的 `.uasset` 清理旧 serialized property。

Connected GitHub 不直接编辑该二进制 `.uasset`。

## Next exact gate

```text
USER ACTION REQUIRED

1. git pull
2. regenerate UE project files（新增 reflected URelicCountTrigger）
3. Development Editor Build once
4. STOP and report the Build result
```

Build PASS 后再依次执行：

```text
SlayTheSpireDemo.Phase7F
→ 打开/保存 DA_Relic_Sundial，确认无 CounterDisplayMax 且 RequiredCount=3
→ SlayTheSpireDemo.Phase7E
→ SlayTheSpireDemo.Phase7.Sundial
→ SlayTheSpireDemo.Phase7.RelicPresentation
→ Sundial PIE smoke
```

遵循项目 validation policy：已经 PASS 的 Gate 不重复运行，除非后续修改使其失效。
