# Card Upgrade Foundation Validation

日期：**2026-09-04**

状态：**HISTORICAL EVIDENCE / SUPERSEDED TARGET / NO CURRENT SEAL**

当前 ordinary-card upgrade authority：

```text
docs/CardUpgradeSTSStyleRefactor.md
```

---

## Historical validated evidence

更早的 Upgrade Foundation 版本曾由用户在本地 UE5.8 环境验证通过：

```text
SlayTheSpireDemoEditor Win64 Development Build: PASS
SlayTheSpireDemo.CardUpgrade: PASS
SlayTheSpireDemo.UIA3.ImmediatePreview: PASS
```

这些结果是真实历史证据，继续保留，不 retroactively 删除。

之后又实现过中间 review-fix：

```text
slim FCardUpgradeConfig
shared DisplayName/CardArt/CardType/TargetType
frozen bUpgraded Presentation state
gold upgraded card name
```

该中间 shape 尚未重新完成最终 Gate，就被新的 STS-style refactor 设计取代。

---

## Why no old revalidation is required now

当前最终目标已经不再是：

```text
Base config
+ bHasUpgrade
+ FCardUpgradeConfig
+ second Effects[]
```

而是：

```text
one immutable CardData
one immutable Effects[]
per-field typed Base / Upgraded values
one UCardInstance::bUpgraded
```

因此此前排队等待的旧 intermediate-head Gate 不再用于恢复旧 Foundation seal。

不能声明新 STS-style refactor 已验证，也不应为了一个已经被 supersede 的 source shape 额外消耗 Build/Automation 预算。

---

## Next validation authority

新 refactor 的最终验证只按：

```text
docs/CardUpgradeSTSStyleRefactor.md
```

执行：

```text
Editor Build
→ SlayTheSpireDemo.CardUpgrade
→ SlayTheSpireDemo.UIA3.DynamicText
→ SlayTheSpireDemo.UIA3.ImmediatePreview
→ SlayTheSpireDemo.Phase6C
→ one focused PIE visual pass
```

并包含六个生产 `.uasset` 的 parity-before-removal 与 USER ACTION resave Gate。

在这些步骤完成前，当前只能声明：

```text
STS-style refactor design approved
implementation not started
historical Upgrade Foundation evidence retained
no current refactor seal
```
