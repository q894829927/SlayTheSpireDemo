# Card Upgrade Foundation Implementation

日期：**2026-09-04**

状态：**HISTORICAL IMPLEMENTATION / SUPERSEDED / DO NOT CONTINUE**

当前 implementation authority：

```text
docs/CardUpgradeSTSStyleRefactor.md
```

---

## Historical implementation preserved

本文件此前对应的 source shape：

```text
UCardData
├─ shared metadata
├─ Base Description / Cost / Destination / Effects
├─ bHasUpgrade
└─ Upgrade : FCardUpgradeConfig
   ├─ Description
   ├─ Cost
   ├─ Destination
   └─ Effects[]

UCardInstance
└─ bool bUpgraded
```

并已经建立：

```text
UUpgradeCardAction
UCardInstance::bUpgraded
frozen Presentation bUpgraded
Native upgraded-name gold styling
```

这些实现事实保留；但 `FCardUpgradeConfig / bHasUpgrade / second Effects[]` 不再是下一步需要重新验证和 seal 的目标。

---

## Contracts retained into the new refactor

继续保留：

```text
UCardInstance::bUpgraded = single ordinary runtime upgrade truth
UUpgradeCardAction = in-combat mutation boundary
DisplayName text remains shared/unchanged
Presentation freezes bUpgraded
Native card name renders gold when upgraded
CardData/CardEffects remain immutable shared definitions
```

---

## Contracts superseded

停止继续维护：

```text
Base/Upgrade full configuration switching
bHasUpgrade presence flag
FCardUpgradeConfig
second Upgrade Effects[]
GetEffects() selecting Base vs Upgrade objects
BattleTextResolver Base/Upgrade double configuration validation
old CardUpgradeFoundationTests assertions that Effects are distinct objects
```

下一轮实现必须按：

```text
one Effects[]
per-effect typed Base / Upgraded fields
bool-only GetEffectiveXXX(bool bIsUpgraded)
explicit authored Upgraded* values
```

执行。

---

## Validation note

此前针对 slim two-config model 排队等待的：

```text
Editor Build
SlayTheSpireDemo.CardUpgrade
PresentationCardViewMapper
Phase6UIA2N.R4
```

不再作为“恢复旧 Foundation seal”的待办。该 shape 已被新设计取代，继续为它花费 Gate 预算没有意义。

新的最终 Gate 只以 `docs/CardUpgradeSTSStyleRefactor.md` 为准：

```text
Build
→ CardUpgrade
→ UIA3.DynamicText
→ UIA3.ImmediatePreview
→ Phase6C
→ one focused PIE visual pass
```

---

## Stop state

```text
[x] preserve historical implementation record
[x] retain runtime bUpgraded / UpgradeCardAction / gold Presentation contracts
[x] retire two-config implementation as active authority
[ ] implement docs/CardUpgradeSTSStyleRefactor.md
```
