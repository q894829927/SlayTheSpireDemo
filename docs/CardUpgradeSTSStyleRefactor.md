# Card Upgrade STS-Style Refactor

日期：**2026-09-04**

状态：**R1 IMPLEMENTED / BUILD PENDING / R2 SIX-ASSET PARITY PENDING**

本文件是下一轮普通卡牌升级重构的 dedicated authority。它在实现层面取代当前 `FCardUpgradeConfig + bHasUpgrade + second Effects[]` 方案，但保留既有 `UCardInstance::bUpgraded`、`UUpgradeCardAction`、A3/Presentation 冻结链与金色升级名称表现。

当前 R1 source head：

```text
16ce7863c6ae4910ce4c0627dadb0e626641115f
```

---

## 1. Goal

普通卡牌升级收敛为：

```text
one immutable UCardData
+ one immutable Effects[] composition
+ typed Base / Upgraded authored values on the field that actually changes
+ one runtime UCardInstance::bUpgraded state bit
```

禁止继续维护第二套完整 Card configuration。

目标：

```text
CardData / CardEffect = immutable authored definition
CardInstance.bUpgraded = single mutable upgrade truth
Effect typed resolver = one effective authored value source
Gameplay / Dynamic Text / A3 = consume the same effective value
Presentation = freeze bUpgraded and style upgraded name gold
```

---

## 2. Target data shape

### 2.1 UCardData

重构后普通卡定义：

```text
UCardData
├─ CardId
├─ DisplayName
├─ CardArt
├─ CardType
├─ TargetType
├─ Description
├─ BaseCost
├─ UpgradedCost
├─ DefaultDestination
└─ Effects[]
```

删除普通升级 authoring 中的：

```text
FCardUpgradeConfig
bHasUpgrade
Upgrade.Description
Upgrade.Cost
Upgrade.DefaultDestination
Upgrade.Effects[]
```

`Description` 默认只保留一份格式文本；动态数值由 Effect 的有效值填充。

`DefaultDestination` 本轮保持共享。只有未来出现真实升级需求改变 Destination 时，才为该规则增加最小 typed Base/Upgraded 字段；本轮不预建。

### 2.2 UCardInstance

唯一普通升级运行时状态继续是：

```text
UCardInstance
├─ Definition
├─ RuntimeId
└─ bool bUpgraded
```

不增加第二个 authoritative upgrade flag、UpgradeLevel 或通用 Upgrade context。

---

## 3. Per-effect typed upgrade values

哪个 Effect 的哪个字段会因普通升级变化，就由该 Effect 自己 author Base / Upgraded 值。

第一轮覆盖当前生产已有四类 Effect：

```text
UDamageCardEffect
├─ BaseAmount
├─ UpgradedAmount
├─ HitCount
└─ UpgradedHitCount

UGainBlockCardEffect
├─ BaseAmount
└─ UpgradedAmount

UDrawCardEffect
├─ DrawCount
└─ UpgradedDrawCount

UApplyStatusCardEffect
├─ Amount
└─ UpgradedAmount
```

只有已有或首批正式卡需要升级变化的 typed 字段进入本轮。不得为了未来未知卡牌创建万能 Upgrade Delta / Expression / Context。

### 3.1 Effective helper contract

Effect helper 接收 **`bool bIsUpgraded`**，不接收 `UCardInstance*`：

```cpp
int32 GetEffectiveAmount(bool bIsUpgraded) const;
int32 GetEffectiveHitCount(bool bIsUpgraded) const;
```

理由：

```text
Effect resolver 只依赖一个 immutable-at-build-time fact
→ Effect header 不依赖 CardInstance
→ 单测不需要构造伪 CardInstance
→ 不把运行时对象依赖扩散进 definition API
```

调用边界负责从当前 Context 中读取状态：

```text
Context.Card
→ freeze Card->IsUpgraded()
→ bool bIsUpgraded
→ Effect.GetEffectiveXXX(bIsUpgraded)
```

---

## 4. Explicit authored-value rule

`Upgraded*` 不允许存在任何隐式回退语义。

明确禁止：

```text
UpgradedAmount == 0  → use BaseAmount
UpgradedCost == 0    → use BaseCost
任何 sentinel / magic default → fallback Base
```

规则：

```text
升级后数值不变
→ Upgraded* 显式 author 成与 Base 相同

升级后数值变化
→ Upgraded* 显式 author 成升级后的真实值
```

DataValidation 对 Base 与 Upgraded 字段应用各自相同的合法性规则。例如：

```text
Damage BaseAmount / UpgradedAmount >= 0
Block BaseAmount / UpgradedAmount >= 0
Draw DrawCount / UpgradedDrawCount >= 0
Status Amount / UpgradedAmount >= 1
HitCount / UpgradedHitCount >= 1
BaseCost / UpgradedCost >= 0
```

不能因为 `Upgraded*` 当前未被某测试读取，就允许无效默认值进入资产。

---

## 5. Build-time freeze invariant

当前有效值在以下 build/read boundary 读取：

```text
CardEffect::BuildActions
BattleTextResolver preview build
A3 immediate-preview build
```

本轮允许在这些 boundary 冻结：

```cpp
const bool bIsUpgraded = Context.Card && Context.Card->IsUpgraded();
```

其合法性依赖以下明确不变量：

> **被 play 的这一个 CardInstance 的 `bUpgraded` 在该 card-play resolution 内不会发生变化。**

当前生产机制满足该条件：没有“同一张正在结算的牌在自己的 resolution 中途升级自身”的能力。

因此：

```text
Build-time bUpgraded freeze
→ BuildActions / Dynamic Text / A3 读取一致 typed authored value
→ normal Actions 可继续携带冻结后的 intent/value
```

如果未来出现 resolution 内自我升级或其他会使该 CardInstance 的 upgrade state 在前置 Action 执行后发生变化的机制，则：

```text
Build-time freeze 不再合法
→ 对受影响字段把 effective-value resolution 移到 Action Execute-time
```

该升级时序规则与既有 predicate timing contract 一致：mutable Gameplay fact 若可能在前置 Action 后变化，就必须在 Execute boundary 解析。

---

## 6. Consumer migration

### 6.1 CardInstance

重构后：

```text
GetEffects()
→ always Definition->Effects

GetDescriptionFormat()
→ always Definition->Description

GetCurrentCost()
→ bUpgraded ? Definition->UpgradedCost : Definition->BaseCost

ResolveDestination()
→ Definition->DefaultDestination
```

`CanUpgrade()` 不再读取 `bHasUpgrade`。

本轮 ordinary character-card policy：

```text
valid Definition && !bUpgraded
```

当 Status / Curse 或其他真正不可升级的 production definition 进入需要时，再引入具有真实语义的不可升级 policy；不得用 `Upgraded*=0` 或缺失 Upgrade config 推断。

### 6.2 Effects

Gameplay / Dynamic Text / A3 均必须使用同一 Effect typed resolver。

例如 Damage：

```text
BuildActions
BuildPreviewArguments
BuildImmediatePreviewOperations
→ same GetEffectiveAmount(bIsUpgraded)
→ same GetEffectiveHitCount(bIsUpgraded)
```

禁止其中任一路径继续直接读取 raw Base 字段。

### 6.3 BattleTextResolver

当前 `BattleTextResolver::ValidateCardDefinition` 中按：

```text
Base Description + Base Effects
bHasUpgrade
→ Upgrade Description + Upgrade Effects
```

进行双配置验证的分支必须删除。

迁移后：

```text
one Description
one Effects[]
→ effect-level ValidatePreviewConfiguration validates both Base and Upgraded authored fields
```

`UCardData::IsDataValid` 继续委托 BattleTextResolver，但不得残留 `bHasUpgrade` / `Upgrade.Effects` 验证路径。

---

## 7. Mutation and Presentation stay unchanged

### 7.1 In-combat mutation authority

继续：

```text
UUpgradeCardAction
→ UCardInstance::CommitUpgrade()
→ false -> true
→ Finish
```

Widget、Presentation、CardData 不直接修改 Gameplay upgrade state。

### 7.2 Presentation

当前已建立的冻结链保持：

```text
UCardInstance::bUpgraded
→ FPresentationCardSnapshot.bUpgraded
→ FBattleHUDCardView.bUpgraded
→ UBattleCardWidget
→ upgraded name uses gold color
```

`DisplayName` 文本本体不变。

本重构不增加 `+` 后缀。

---

## 8. Initial/spawn upgraded state

需要一个 definition 外部的创建规格，使测试/起始牌可以生成升级实例：

```text
FCardSpawnSpec / StartingCardEntry
├─ CardData
└─ bUpgraded
```

语义：

```text
CardData = definition only
spawn entry = this created copy starts upgraded or not
CardInstance = mutable runtime state
```

它不得把“当前是否升级”写回 `UCardData`。

战斗中任何升级仍走 `UUpgradeCardAction`。

### Phase 8 relation

该 spawn spec 允许未来 PIE 合法配置：

```text
two upgraded Pommel Strike instances
```

但 Phase 8 Automation 的既定结论不变：

```text
Phase 8 Automation
→ transient UCardData / transient Effects
→ real play/draw/shuffle/Sundial path
→ does not lock production Pommel numeric values
```

Phase 8 继续 deferred，不因本重构恢复为 Card Expansion blocker。

---

## 9. Asset migration contract

当前受影响 production card assets 必须覆盖全部六张：

```text
DA_Card_Strike
DA_Card_Defend
DA_Card_PommelStrike
DA_Card_TwinStrike
DA_Card_Uppercut
DA_Card_Inflame
```

### 9.1 Parity-before-removal

在旧字段仍存在时，先完成新字段 authoring parity：

```text
old Base fields
old Upgrade.Cost / Upgrade.Effects values
        ↓ compare
new Base / Upgraded typed fields
```

必须逐资产确认：

```text
new UpgradedCost == old Upgrade.Cost where applicable
new Effect.Upgraded* == corresponding old Upgrade.Effects value
new Effect order/type still represents the same current production semantics
```

没有 parity evidence 前，不删除旧 serialized fields。

### 9.2 USER ACTION REQUIRED — .uasset resave

GitHub text edits不能安全代写 Unreal binary `.uasset`。

删除旧 `FCardUpgradeConfig` / `bHasUpgrade` 等 serialized fields 后，用户必须在 UE Editor 中：

```text
open each of the six DA_Card_* assets
→ verify new Base/Upgraded fields
→ Save
```

该步骤沿用 Phase 7F 删除旧 serialized property 后的手工 resave precedent。

资产 resave 是 migration Gate，不得用 C++ Automation 假装替代。

### 9.3 UCardVariantData compatibility shim

`UCardVariantData` 当前仅是此前短暂实现窗口留下的 load-compatibility shim，不是 ordinary authoring contract。

本 initiative 中：

```text
keep while old saved assets may still reference the reflected class
→ resave/verify all affected assets
→ only then remove shim if no serialized reference remains
```

不得为了追求一次文本清理而先删 shim 导致旧资产无法加载。

---

## 10. Existing test migration

不新建第二套平行 Upgrade Foundation 测试。

直接迁移：

```text
Source/SlayTheSpireDemoTests/Private/CardUpgradeFoundationTests.cpp
```

删除旧模型断言，例如：

```text
"Base and upgraded Effects are distinct authored objects"
```

替换为新的关键证据：

```text
same CardId
same RuntimeId
same Effects object identity before/after upgrade
bUpgraded false -> true exactly once
base typed values before upgrade
upgraded typed values after upgrade
same DisplayName text
frozen bUpgraded changes false -> true
Gameplay / Dynamic Text / snapshot consume upgraded values
```

---

## 11. One-initiative implementation sequence

本工作只定义一个 initiative：

```text
Card Upgrade STS-Style Refactor
```

内部按迁移依赖连续收敛：

### R1 — IMPLEMENTED / BUILD PENDING

已完成 source edit：

```text
add UpgradedCost
add per-effect Upgraded* fields
add GetEffectiveXXX(bool bIsUpgraded)
add Base+Upgraded DataValidation
keep old FCardUpgradeConfig/bHasUpgrade temporarily for migration parity only
```

R1 没有切换 Gameplay/DynamicText/A3 runtime authority；旧资产仍可按原模型加载，待 Build PASS 后进入 R2 parity。

### R2 — Asset parity authoring

USER ACTION REQUIRED：六个生产资产把旧 Upgrade 数值复制/核对到新 typed fields。

完成 parity evidence 后才允许进入 R3。

### R3 — Switch runtime and tests to the new authority

```text
CardInstance getters stop selecting Upgrade config
Effects[] becomes single immutable composition
BattleTextResolver removes Base/Upgrade double-config validation
four current Effect implementations consume bool-based effective helpers
CardUpgradeFoundationTests.cpp fully migrated
starting/spawn upgraded state added at the narrow existing creation boundary
```

### R4 — Remove old ordinary upgrade model and resave assets

```text
remove FCardUpgradeConfig
remove bHasUpgrade
remove Upgrade.* ordinary authoring
USER ACTION REQUIRED: reopen/resave all six assets
remove UCardVariantData shim only after load/resave evidence proves it is safe
```

R1-R4 属于同一个重构 initiative，不形成新的长期 Phase hierarchy。

---

## 12. Validation budget

最终 head 的 Gate 顺序锁定为：

```text
1. SlayTheSpireDemoEditor Win64 Development Build
2. SlayTheSpireDemo.CardUpgrade
3. SlayTheSpireDemo.UIA3.DynamicText
4. SlayTheSpireDemo.UIA3.ImmediatePreview
5. SlayTheSpireDemo.Phase6C
6. one focused PIE visual pass
```

Phase6C 必须加入，因为本重构直接修改当前 Effect `BuildActions` 的 authored-value读取路径。

不运行全量 Phase 6 / Phase 7。

Passing Gate 仍遵循 sticky 规则；若某个后续 fix 未影响某个已经 PASS 的 contract，则不重复跑该 Gate。

### PIE manual Gate

只验证真正 player-facing 的部分：

```text
one ordinary card shown before upgrade → default name color
same runtime card after upgrade → name text unchanged, name becomes gold
upgraded numeric text matches configured upgraded value
```

PIE 不承担 Gameplay 数值正确性的主要证明；Gameplay/Dynamic Text/A3 由 Automation 证明。

---

## 13. Reference acceptance case

以 Pommel Strike 作为最终 integration evidence：

```text
same UCardInstance
Base:
→ Damage 9
→ Draw 1
→ normal name color

UpgradeCardAction
→ bUpgraded false -> true

Upgraded:
→ Damage 10
→ Draw 2
→ same RuntimeId
→ same CardId
→ same Effects object identities
→ same DisplayName text
→ gold name color
```

同时必须成立：

```text
Gameplay effective values = 10 / 2
Dynamic Text effective values = 10 / 2
A3 effective values = 10 / 2
committed/frozen presentation bUpgraded = true
```

数值只是当前 parity reference；Phase 8 Automation 不因该生产数值被锁死。

---

## 14. Explicit non-goals

本重构明确不做：

```text
repeatable upgrade / UpgradeCount / Searing Blow
UpgradedDescriptionOverride
effect count changes
effect type changes
effect structural replacement
universal Upgrade Delta / Upgrade Expression / Upgrade Context
CardId-specific upgrade branches
card-name '+' suffix
Phase 8 implementation
Armaments content implementation
save/load/run-deck persistence
campfire/reward/shop upgrade UX
```

若未来需要 `+` 后缀，只允许 Presentation 根据冻结 `bUpgraded` 格式化；不得把 `+` 写入 Gameplay authored name。

---

## 15. Stop / acceptance state

当前：

```text
[x] architecture direction reviewed against current source
[x] bool-only Effect resolver dependency locked
[x] build-time freeze invariant locked
[x] BattleTextResolver migration included
[x] explicit Upgraded* semantics locked; no magic fallback
[x] CardUpgradeFoundationTests migration required
[x] Phase6C regression Gate included
[x] all six production assets included
[x] parity-before-removal + USER ACTION resave locked
[x] Phase8 relation preserved
[x] explicit non-goals locked
[x] R1 source implementation
[ ] R1 editor Build PASS
[ ] six-asset parity authoring
[ ] R3 authority switch
[ ] R4 old-model removal + asset resave
[ ] final validation Gates
[ ] seal
```
