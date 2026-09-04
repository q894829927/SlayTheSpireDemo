# Card Upgrade STS-Style Refactor

日期：**2026-09-04**

状态：**R3 COMPLETE / VALIDATED / R4 LEGACY SOURCE REMOVAL IMPLEMENTED / BUILD + SIX-ASSET RESAVE PENDING**

本文件是普通卡升级重构的 dedicated authority。最终 ordinary-card upgrade 模型已经从 `FCardUpgradeConfig + bHasUpgrade + second Effects[]` 收敛到单一 `UCardData`、单一 `Effects[]`、typed Base/Upgraded authored values 与唯一 runtime `UCardInstance::bUpgraded`。

---

## 1. Locked target

```text
one immutable UCardData
+ one immutable Effects[] composition
+ typed Base / Upgraded authored values on the field that actually changes
+ one runtime UCardInstance::bUpgraded state bit
```

最终结构：

```text
UCardData
├─ CardId / DisplayName / CardArt / CardType / TargetType
├─ Description
├─ BaseCost / UpgradedCost
├─ DefaultDestination
└─ Effects[]

UCardInstance
├─ Definition
├─ RuntimeId
└─ bool bUpgraded
```

禁止引入第二个 authoritative upgrade flag、普通卡 UpgradeLevel、Universal Upgrade Delta/Context、CardId 分支或第二套 Effects composition。

---

## 2. Typed per-effect upgrade values

本轮覆盖当前生产已有四类 Effect：

```text
UDamageCardEffect
├─ BaseAmount / UpgradedAmount
└─ HitCount / UpgradedHitCount

UGainBlockCardEffect
└─ BaseAmount / UpgradedAmount

UDrawCardEffect
└─ DrawCount / UpgradedDrawCount

UApplyStatusCardEffect
└─ Amount / UpgradedAmount
```

Effect resolver API 只接：

```cpp
GetEffectiveXXX(bool bIsUpgraded)
```

不接 `UCardInstance*`。调用边界读取 `Card->IsUpgraded()` 后只传 bool。

`Upgraded*` 没有 magic fallback：升级不改某值时也必须显式 author 成与 Base 相同；Base/Upgraded 各自应用相同 DataValidation 规则。

---

## 3. Build-time freeze invariant

当前 Gameplay / Dynamic Text / A3 可在 build/read boundary 冻结：

```cpp
const bool bIsUpgraded = Context.Card && Context.Card->IsUpgraded();
```

合法性不变量：

> 被 play 的同一个 CardInstance 在该 card-play resolution 内不会中途改变自己的 `bUpgraded`。

若未来出现 resolution 内自我升级，则受影响 effective-value resolution 必须移动到 Action Execute-time，与既有 predicate timing contract 一致。

---

## 4. Runtime authority

```text
CanUpgrade()
→ valid Definition && !bUpgraded

GetDescriptionFormat()
→ Definition->Description

GetCurrentCost()
→ bUpgraded ? Definition->UpgradedCost : Definition->BaseCost

ResolveDestination()
→ Definition->DefaultDestination

GetEffects()
→ always Definition->Effects
```

普通升级不再更换 Description、Destination 或 Effects object composition。

战斗中 mutation authority 保持：

```text
UUpgradeCardAction
→ UCardInstance::CommitUpgrade()
→ false -> true once
```

创建时可显式初始化：

```cpp
UCardInstance::Initialize(Definition, RuntimeId, bStartUpgraded)
```

该参数只初始化 runtime instance，不写回共享 `UCardData`。

---

## 5. Consumer convergence

```text
Damage BuildActions / BuildPreviewArguments / BuildImmediatePreviewOperations
→ same GetEffectiveAmount/GetEffectiveHitCount

Block BuildActions / BuildPreviewArguments / BuildImmediatePreviewOperations
→ same GetEffectiveAmount

Draw BuildActions / BuildPreviewArguments
→ same GetEffectiveDrawCount

ApplyStatus BuildActions / BuildPreviewArguments
→ same GetEffectiveAmount
```

Gameplay、Dynamic Text、A3 不允许继续直接读取 raw Base 字段作为升级后的 authority。

`BattleTextResolver::ValidateCardDefinition` 只验证 one Description + one Effects[]；Effect-level validation 同时验证 Base 与 Upgraded typed authored fields。

---

## 6. Presentation remains unchanged

```text
UCardInstance::bUpgraded
→ FPresentationCardSnapshot.bUpgraded
→ FBattleHUDCardView.bUpgraded
→ UBattleCardWidget
→ upgraded card name gold
```

`DisplayName` 文本本身不变，不自动拼 `+`。

---

## 7. Asset migration

六个生产资产：

```text
DA_Card_Strike
DA_Card_Defend
DA_Card_PommelStrike
DA_Card_TwinStrike
DA_Card_Uppercut
DA_Card_Inflame
```

R2 已完成：旧字段仍存在时，新 `UpgradedCost / Effect.Upgraded*` 与旧 Upgrade 数值完成 parity，六资产已保存并提交。

R4 source removal 现已完成：

```text
removed FCardUpgradeConfig
removed bHasUpgrade
removed UCardData::Upgrade
```

`UCardVariantData` 暂时继续作为 load-compatibility shim，不属于 ordinary authoring surface。

---

## 8. R4 user action

删除旧 reflected fields 后，必须在成功 Build 后执行一次资产重存：

```text
open all six DA_Card_* assets
→ verify BaseCost / UpgradedCost
→ verify each Effect Base / Upgraded typed values
→ confirm no old Upgrade / Has Upgrade authoring surface remains
→ Save all six assets
→ commit the six post-removal .uasset changes
```

只有六资产在旧字段删除后能够正常加载并重存，才有资格评估是否删除 `UCardVariantData` compatibility shim。本轮不提前删除 shim。

---

## 9. R3 validation evidence

全部 PASS：

```text
SlayTheSpireDemoEditor Win64 Development Build
SlayTheSpireDemo.CardUpgrade
SlayTheSpireDemo.Phase6UIA3.DynamicText
SlayTheSpireDemo.UIA3.ImmediatePreview
SlayTheSpireDemo.Phase6C
```

Dynamic Text 最终使用真实前缀 `SlayTheSpireDemo.Phase6UIA3.DynamicText` 并通过。此前错误前缀 `SlayTheSpireDemo.UIA3.DynamicText` 的 `No automation tests matched` 不计为逻辑失败。

---

## 10. R4 validation budget

R4 当前直接失效 Gate：

```text
1. SlayTheSpireDemoEditor Win64 Development Build
2. six production card assets post-removal load/resave
```

R4 仅删除已不再被 Runtime 读取的 legacy serialized fields，因此 R3 的 CardUpgrade / DynamicText / ImmediatePreview / Phase6C 逻辑 Gate保持 sticky；除非 Build 或资产加载暴露出新的共享问题，否则不重复执行。

资产重存后最终进行一次 focused PIE：

```text
normal card → default name color
same runtime card upgraded → same name text + gold name
upgraded numeric text matches configured upgraded value
```

---

## 11. Reference acceptance case

Pommel Strike 最终应满足：

```text
same UCardInstance
Base:     Damage 9 / Draw 1
Upgrade:  Damage 10 / Draw 2
same RuntimeId
same CardId
same Effects object identities
same DisplayName text
gold upgraded name
```

并且：

```text
Gameplay = Dynamic Text = A3 = configured upgraded typed values
```

Phase 8 仍 deferred；其 Automation 继续使用 transient definitions，不锁生产 Pommel 数值。

---

## 12. Explicit non-goals

```text
repeatable upgrade / UpgradeCount / Searing Blow
UpgradedDescriptionOverride
effect count/type structural replacement
universal Upgrade Delta / Upgrade Context
card-name '+' suffix
Armaments content implementation
Phase 8 implementation
save/load/run-deck persistence
campfire/reward/shop upgrade UX
```

---

## 13. Current stop state

```text
[x] architecture direction locked
[x] bool-only Effect resolver contract
[x] no magic fallback
[x] build-time freeze invariant
[x] R1 typed fields + validation implemented
[x] R1 Build PASS
[x] six-asset R2 parity authored/saved/committed
[x] R3 CardInstance runtime authority switched
[x] R3 Effect consumers switched
[x] R3 BattleTextResolver single-config validation switched
[x] CardUpgradeFoundationTests migrated in place
[x] R3 Build PASS
[x] SlayTheSpireDemo.CardUpgrade PASS
[x] SlayTheSpireDemo.Phase6UIA3.DynamicText PASS
[x] SlayTheSpireDemo.UIA3.ImmediatePreview PASS
[x] SlayTheSpireDemo.Phase6C PASS
[x] R4 FCardUpgradeConfig / bHasUpgrade / Upgrade source removal
[ ] R4 Build PASS
[ ] six-asset post-removal load/resave/commit
[ ] assess UCardVariantData shim removal
[ ] final focused PIE
[ ] seal
```
