# Card Upgrade STS-Style Refactor

日期：**2026-09-04**

状态：**R1 BUILD PASS / R2 SIX-ASSET PARITY COMMITTED / R3 SOURCE SWITCH IMPLEMENTED / VALIDATION PENDING**

本文件是普通卡升级重构的 dedicated authority。它取代 `FCardUpgradeConfig + bHasUpgrade + second Effects[]` 作为最终 ordinary-card upgrade 设计，但在 R4 完成前暂时保留这些旧 serialized fields 作为迁移安全壳。

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

不接 `UCardInstance*`。Effect header 不依赖 runtime CardInstance；调用边界读取 `Card->IsUpgraded()` 后只传 bool。

`Upgraded*` 没有任何 magic fallback：升级不改某值时也必须显式 author 成与 Base 相同。Base/Upgraded 各自应用相同 DataValidation 规则。

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

## 4. Runtime authority after R3

R3 已切换为：

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

第二次普通升级继续 fail-soft，不触发 ResolutionFault。

创建时可显式初始化：

```cpp
UCardInstance::Initialize(Definition, RuntimeId, bStartUpgraded)
```

该参数只初始化 runtime instance，不写回共享 `UCardData`。战斗过程中仍只能通过 `UUpgradeCardAction` 升级。

---

## 5. Consumer convergence

R3 当前 source 已要求：

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

`BattleTextResolver::ValidateCardDefinition` 只验证：

```text
one Description
+ one Effects[]
```

Effect-level validation同时验证 Base 和 Upgraded typed authored fields。

---

## 6. Presentation remains unchanged

```text
UCardInstance::bUpgraded
→ FPresentationCardSnapshot.bUpgraded
→ FBattleHUDCardView.bUpgraded
→ UBattleCardWidget
→ upgraded card name gold
```

`DisplayName` 文本本身不变，不自动拼 `+`。如果未来需要 `+`，只能由 Presentation 根据冻结的 `bUpgraded` 格式化，不进入 Gameplay authored name。

---

## 7. Asset migration

六个生产资产全部纳入迁移：

```text
DA_Card_Strike
DA_Card_Defend
DA_Card_PommelStrike
DA_Card_TwinStrike
DA_Card_Uppercut
DA_Card_Inflame
```

R2 已由用户完成：在旧字段仍存在时，把旧 Upgrade 数值 parity 到新 `UpgradedCost / Effect.Upgraded*`，保存并提交六个资产。

因此 R3 可以切 runtime authority，但在 R3 验证通过前仍不删除旧 serialized fields。

---

## 8. R4 legacy removal

R3 focused gates 全部 PASS 后进入 R4：

```text
remove FCardUpgradeConfig
remove bHasUpgrade
remove Upgrade.* ordinary authoring
```

随后 USER ACTION REQUIRED：

```text
open all six DA_Card_* assets
→ verify typed Base/Upgraded fields
→ Save
```

`UCardVariantData` 继续作为 load-compatibility shim，直到六资产在旧字段删除后能够正常加载/重存；只有有证据证明没有 serialized reference 后才删除。

---

## 9. Existing test migration

不新建平行 Upgrade Foundation 测试。直接迁移：

```text
Source/SlayTheSpireDemoTests/Private/CardUpgradeFoundationTests.cpp
```

R3 测试应证明：

```text
same CardId / RuntimeId
same Effects object identity across upgrade
bUpgraded false -> true once
base typed values before upgrade
upgraded typed values after upgrade
same DisplayName text
creation-time upgraded instance is legal
Dynamic Text uses upgraded typed value
Presentation snapshot freezes bUpgraded
```

---

## 10. Validation budget

R3 当前 Gate：

```text
1. SlayTheSpireDemoEditor Win64 Development Build
2. SlayTheSpireDemo.CardUpgrade
3. SlayTheSpireDemo.UIA3.DynamicText
4. SlayTheSpireDemo.UIA3.ImmediatePreview
5. SlayTheSpireDemo.Phase6C
```

Phase6C 必须跑，因为 R3 直接修改了 Effect `BuildActions` 的 authored-value读取路径。

不跑全量 Phase 6 / Phase 7。Passing Gate sticky；失败后只修复并重跑被该修复直接失效的 Gate。

R4 删除 serialized fields 后至少重新 Build；若 R4 仅删除已不再被 runtime 读取的 legacy fields，则已经 PASS 的逻辑 Gate保持 sticky，除非编译/资产加载问题说明相关 contract 被重新失效。

最终 manual PIE 只验证真正视觉部分：

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
[ ] R3 Build PASS
[ ] SlayTheSpireDemo.CardUpgrade PASS
[ ] SlayTheSpireDemo.UIA3.DynamicText PASS
[ ] SlayTheSpireDemo.UIA3.ImmediatePreview PASS
[ ] SlayTheSpireDemo.Phase6C PASS
[ ] R4 legacy field removal
[ ] six-asset post-removal resave
[ ] final focused PIE
[ ] seal
```
