# Phase 7E —— 遗物反应组合化实施

日期：**2026-09-03**

状态：**DESIGN APPROVED / IMPLEMENTATION AUTHORIZED / IN PROGRESS**

用户已明确授权开始 Phase 7E。本文记录实施状态；详细设计、边界与验收合同继续以 `docs/Phase7ERelicCompositionDesign.md` 为准。该设计文档末尾原有的 `DESIGN DRAFT / IMPLEMENTATION NOT AUTHORIZED` 是授权前状态，现由本次用户明确授权和本文状态覆盖，不改变其中已经完成最终 review 的设计内容。

## 当前实施范围

```text
URelicEffect + FRelicEffectContext
UGainEnergyRelicEffect
UGainBlockRelicEffect
UDeckShuffledCountTrigger
UAdvanceRelicCounterAction
focused Phase7E Automation
Sundial 测试/生产配置迁移
迁移验证完成后删除 USundialTrigger / USundialAdvanceAction
```

继续保持：

```text
7A / 7B / 7C / 7D sealed contracts
Trigger 只读 eligibility + Action builder
Counter mutation 仅由 BattleAction 执行
RewardActions 在 BuildReactions 阶段 eager build 并冻结
multi-effect 使用 AddBatchToFrontPreserveOrder
nested RewardActions 由 CounterAction 传播 PresentationRecordWriter
7D Read/Frozen/Native UI 零语义修改
```

## 当前执行状态

已开始新增 7E 通用 C++ primitives 和 focused tests。生产 `DA_Relic_Sundial` 是二进制 `.uasset`，需要在 Unreal Editor 中迁移到 `UDeckShuffledCountTrigger + UGainEnergyRelicEffect` 后才能安全删除旧 Sundial 专属 C++ 类。

在本轮 C++ 变更完成前，不声明 Build / Automation / PIE 通过。
