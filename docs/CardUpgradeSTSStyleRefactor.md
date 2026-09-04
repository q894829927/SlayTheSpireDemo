# Card Upgrade STS-Style Refactor

日期：**2026-09-05**

状态：**COMPLETE / VALIDATED / SEALED**

本文件是普通卡升级重构的 dedicated authority。ordinary-card upgrade 已从 `FCardUpgradeConfig + bHasUpgrade + second Effects[]` 完整收敛到单一 `UCardData`、单一 `Effects[]`、typed Base/Upgraded authored values 与唯一 runtime `UCardInstance::bUpgraded`。

---

## 1. Locked final model

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

禁止重新引入第二个 authoritative upgrade flag、普通卡 UpgradeLevel、Universal Upgrade Delta/Context、CardId 分支或第二套 Effects composition。

---

## 2. Typed per-effect upgrade values

当前生产覆盖：

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

调用边界读取 `Card->IsUpgraded()` 后只传 bool；Effect 不依赖 `UCardInstance*`。

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

普通升级不更换 Description、Destination 或 Effects object composition。

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

## 6. Presentation contract

Gameplay/DTO 的 `DisplayName` 永远保持 authored base name；升级标题的 `+` 和颜色只属于 Widget presentation：

```text
UCardInstance::bUpgraded
→ FPresentationCardSnapshot.bUpgraded
→ FBattleHUDCardView.bUpgraded
→ UBattleCardWidget
   ├─ false: DisplayName + Designer/default title color
   └─ true:  DisplayName + "+" + upgraded title color
```

升级标题颜色：

```text
sRGB #7FFF00
```

因此：

```text
剑柄打击  → base
剑柄打击+ → upgraded
```

禁止在 `UCardData::DisplayName` 中 author 第二份升级名称；`+` 只能由 Presentation 根据冻结 `bUpgraded` 格式化。

---

## 7. Asset migration and legacy cleanup

迁移范围：

```text
DA_Card_Strike
DA_Card_Defend
DA_Card_PommelStrike
DA_Card_TwinStrike
DA_Card_Uppercut
DA_Card_Inflame
```

迁移已完成：

```text
R2
→ legacy fields still present
→ new UpgradedCost / Effect.Upgraded* authored to parity
→ six assets saved/committed

R4
→ removed FCardUpgradeConfig
→ removed bHasUpgrade
→ removed UCardData::Upgrade
→ Build PASS
→ six assets loaded successfully
→ six assets post-removal resaved

Final compatibility cleanup
→ removed UCardVariantData shim
→ final Build PASS
→ full Editor restart
→ all six assets loaded successfully without the shim
```

`UCardVariantData` 已从生产源码删除，不再是任何 active compatibility dependency。

---

## 8. Validation evidence

### R3 logic gates

全部 PASS：

```text
SlayTheSpireDemoEditor Win64 Development Build
SlayTheSpireDemo.CardUpgrade
SlayTheSpireDemo.Phase6UIA3.DynamicText
SlayTheSpireDemo.UIA3.ImmediatePreview
SlayTheSpireDemo.Phase6C
```

### R4 / Presentation gates

全部 PASS：

```text
SlayTheSpireDemoEditor Win64 Development Build
SlayTheSpireDemo.Phase6UIA2N.R4
focused PIE
```

focused PIE confirmed：

```text
normal card
→ authored name
→ Designer/default title color

same runtime card upgraded
→ authored name + "+"
→ bright yellow-green title (#7FFF00)
→ upgraded numeric text
→ actual Gameplay uses the same upgraded values
```

### Final shim-removal gates

全部 PASS：

```text
UCardVariantData removed from source
→ SlayTheSpireDemoEditor Win64 Development Build PASS
→ full Editor restart
→ all six production DA_Card_* assets open normally
```

No further Upgrade-focused regression run is required unless a future change directly invalidates this sealed surface.

---

## 9. Reference acceptance case

Pommel Strike sealed behavior：

```text
same UCardInstance
Base:     Damage 9 / Draw 1
Upgrade:  Damage 10 / Draw 2
same RuntimeId
same CardId
same Effects object identities
Gameplay DisplayName remains the same authored text
Widget title becomes DisplayName+
Widget upgraded title color = #7FFF00
```

并且：

```text
Gameplay = Dynamic Text = A3 = configured upgraded typed values
```

---

## 10. Explicit non-goals

以下仍不属于本次 sealed ordinary-upgrade foundation：

```text
repeatable upgrade / UpgradeCount / Searing Blow
UpgradedDescriptionOverride
effect count/type structural replacement
universal Upgrade Delta / Upgrade Context
second authored upgraded DisplayName
Armaments content implementation
Phase 8 implementation
save/load/run-deck persistence
campfire/reward/shop upgrade UX
```

若未来真实卡牌需求触发这些能力，应作为独立 capability slice 设计，不重开本次 ordinary-card upgrade 模型。

---

## 11. Final sealed checklist

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
[x] R4 Build PASS
[x] six-asset post-removal load/resave complete
[x] upgraded Widget title '+' implementation
[x] upgraded title #7FFF00 implementation
[x] upgraded Widget Build PASS
[x] SlayTheSpireDemo.Phase6UIA2N.R4 PASS
[x] final focused PIE PASS
[x] UCardVariantData compatibility shim removed
[x] post-shim-removal Build PASS
[x] six-asset post-shim-removal load PASS
[x] COMPLETE / VALIDATED / SEALED
```

---

## 12. Stop state

```text
CARD UPGRADE STS-STYLE REFACTOR
COMPLETE / VALIDATED / SEALED

No new Phase / Card Expansion implementation is authorized by this document.
STOP.
```
