# Card Upgrade Foundation Design

日期：**2026-09-04**

状态：**DESIGN REFINED / IMPLEMENTATION NOT AUTHORIZED**

本文件定义战士卡牌扩展阶段需要的通用 Upgrade Foundation。它不属于 Phase 8；Phase 8 只做 transient authored Draw-2 card + Sundial 的 Automation 架构验证，并复用现有 Pommel Strike Draw 2 的 Production PIE evidence。

---

## 1. Goal

升级系统与正式卡牌内容一起开发，而不是为了 Phase 8 单独造测试升级卡，也不是等全部卡牌完成后再整体迁移。

核心设计原则：

```text
普通升级是默认能力：最多升级一次。
多次升级不是所有卡牌的默认状态维度，而是可选的独立能力。
每个 CardInstance 在任一时刻只有一个 authoritative upgrade-state shape。
Gameplay upgrade state 与 Presentation formatting 分离。
```

因此完整模型应是：

```text
Default Card Upgrade Policy
→ single-use
→ runtime state shape = Single

Optional Repeatable Upgrade Capability
→ immutable authored policy on CardData
→ runtime state shape = Repeatable

Upgrade State View
+ typed Effective Card / Effect View
→ frozen read-only boundary for gameplay/preview/presentation consumers
```

灼热打击是 Repeatable Upgrade Capability 的首个真实消费者，但通用系统不得识别具体 CardId。

---

## 2. Locked decoupling rule

Upgrade 是独立能力域。

```text
Upgrade System 不知道：
Damage
Block
Draw
Exhaust
Status
Corruption
Perfected Strike
Searing Blow
任何具体 CardId
```

反向同样成立：

```text
Damage / Block / Draw / Exhaust 等系统
不得直接判断具体升级卡牌
不得根据 CardId 决定升级结果
```

所有 Gameplay / Preview / Presentation 消费者只读取统一的 typed effective card/effect view 或 frozen state view。

Repeatable Upgrade Capability 只负责 immutable policy：

```text
this definition is repeatable
repeated CanUpgrade policy
state transition policy N -> N + 1
```

它不得保存 mutable `UpgradeCount`，不得直接知道 Damage、Draw、Block 等具体消费者，也不得负责 UI 文本拼接。

---

## 3. One authoritative runtime upgrade-state shape

绝大多数卡牌只存在两个状态：

```text
Base
Upgraded
```

普通卡的 runtime authority 可以直接表达为：

```text
SingleUpgradeState
→ bool bUpgraded
```

规则：

```text
bUpgraded = false
→ CanUpgrade = true

执行一次 Upgrade
→ bUpgraded = true

bUpgraded = true
→ CanUpgrade = false
```

拥有 `RepeatableUpgradeCapability` 的定义不再同时维护一个独立 authoritative `bUpgraded`：

```text
RepeatableUpgradeState
→ int32 RepeatCount >= 0

RepeatCount = 0
→ base

RepeatCount = N
→ upgraded N times

N -> N + 1
→ next upgrade
```

因此 runtime state 必须是**按定义选择的单一 shape**，概念上等价于 tagged variant：

```text
CardUpgradeRuntimeState
├─ Single     { bUpgraded }
└─ Repeatable { RepeatCount }
```

锁定不变量：

```text
No RepeatableUpgradeCapability
→ authoritative state = Single only

Has RepeatableUpgradeCapability
→ authoritative state = Repeatable only
```

非法组合：

```text
bUpgraded = true + RepeatCount = 0
bUpgraded = false + RepeatCount = 5
```

必须在结构上不可表示，而不是靠运行时同步两个 mutable fields。

`FUpgradeStateView.bIsUpgraded` 是派生只读事实：

```text
Single     → bUpgraded
Repeatable → RepeatCount > 0
```

---

## 4. Definition policy vs runtime mutable state

概念结构：

```text
UCardData
├─ normal authored upgrade data
└─ optional RepeatableUpgradeCapability
   └─ immutable policy only

UCardInstance / dedicated card runtime state
└─ exactly one runtime upgrade-state shape
```

推荐 Gameplay boundary：

```text
CanUpgrade(CardInstance)
ApplyUpgrade(CardInstance)
GetUpgradeStateView(CardInstance)
```

其中：

```text
No RepeatableUpgradeCapability
→ generic single-upgrade state transition

Has RepeatableUpgradeCapability
→ generic repeatable state transition
```

不在 Repeatable capability 中提供：

```text
mutable UpgradeCount storage
BuildUpgradeDisplaySuffix
if DamageEffect / DrawEffect / BlockEffect
ResolveUpgradeContribution(...) 这种会迫使它理解具体 Effect 的宽泛接口
```

禁止：

```cpp
if (CardId == "SearingBlow")
{
    ++UpgradeCount;
}
```

正确方向：

```text
Card Definition
→ optional immutable RepeatableUpgradeCapability

CardInstance runtime state
→ Single OR Repeatable authority

Upgrade action/boundary
→ generic transition
```

未来若出现其他可多次升级卡，也只需要赋予同类 capability / policy，而不是修改默认升级系统。

---

## 5. Typed effective card/effect boundary

升级能力不能迫使每个 Gameplay / UI 系统自己解释 runtime upgrade state。

但统一 boundary **不能**演变成一个包含所有未来能力字段的万能 `FEffectiveCardFacts`：

```text
禁止：
Universal EffectiveCardFacts
├─ Damage
├─ Block
├─ Draw
├─ Weak
├─ Vulnerable
├─ ... every future mechanic
```

正确方向是 typed / polymorphic effective view：

```text
EffectiveCardView
├─ card-level typed facts
│  ├─ Cost
│  ├─ Target
│  ├─ Destination
│  └─ other true card-level rules
└─ EffectiveEffects[]
   └─ each effect exposes its own typed effective authored parameters
```

Upgrade Core 只解析：

```text
Base Card Definition
+ exactly-one runtime upgrade state
+ authored upgrade data
+ permitted combat card state
→ typed EffectiveCardView / EffectiveEffects[]
```

Upgrade Core 不需要知道 Damage/Draw/Block 的具体含义；对应 Effect 自己知道自己的 typed authored/effective parameters。

以下路径必须读取**同一个 effective view**：

```text
PlayCardAction / actual gameplay execution
Card cost / playability
Card Effects
BattleTextResolver
A3 Immediate Preview
committed card-face freeze
PresentationCardSnapshot builder
```

不得出现：

```text
Gameplay execution → upgraded Effect values
A3 preview        → Definition->Effects base values
```

也不得散落：

```cpp
if (Card->bUpgraded) { ... }
if (Card->RepeatCount > 0) { ... }
```

---

## 6. First-slice authoring scope

升级发生什么变化属于 Card content，不属于 Upgrade runtime 的卡名规则。

长期升级可能改变：

```text
Damage amount
Block amount
Draw count
Status amount
Cost
Destination / keyword
Effect count / effect configuration
```

但为了避免首个 Upgrade Foundation 同时解决“状态 + Effect 结构替换 + Preview/Playback 重写”，**首个 implementation slice 锁定为：**

```text
Effect type/order/count remain unchanged.
Only typed authored parameters of existing effects/card-level facts may change.
```

例如：

```text
Damage 6 -> 9
Block 5 -> 8
Draw 1 -> 2
Vulnerable 2 -> 3
Cost 2 -> 1
```

首 slice 不做：

```text
add/remove/reorder Effect
replace one Effect class with another
upgrade changes entire CardEffect graph shape
```

等出现真实卡牌结构变化需求后，再单独扩展 `EffectiveEffects[]` authoring contract；不提前做万能表达式/万能 delta interpreter。

Repeatable 卡牌的 `RepeatCount = N` 只是 runtime state。`N` 如何映射到该卡的 typed effective numeric/content facts，由该卡自己的 authored data/function 表达，并通过同一个 effective view 输出；Repeatable policy 本身不识别具体 Effect 类型。

不得：

```cpp
if (CardId == "PommelStrike") DrawCount = 2;
if (CardId == "Bash") Vulnerable += 1;
if (CardId == "SearingBlow") BaseDamage = ...;
```

---

## 7. Locked presentation rule — state view, not Gameplay text building

Gameplay Upgrade System 不拼接名称后缀。

它只暴露冻结、只读的升级状态：

```text
FUpgradeStateView
├─ StateKind = Single / Repeatable
├─ bIsUpgraded        // derived read-only fact
└─ RepeatCount        // meaningful only when StateKind = Repeatable
```

Presentation formatter 根据 frozen state view 生成显示：

```text
普通单次升级：
Strike
→ Strike+

Bash
→ Bash+

重复升级：
灼热打击
→ 灼热打击+1
→ 灼热打击+2
→ 灼热打击+3
→ ...
```

规则：

```text
Single:
bIsUpgraded = false → ""
bIsUpgraded = true  → "+"

Repeatable:
RepeatCount = 0 → ""
RepeatCount = N → "+N"
```

Native HUD、A2、A3、卡牌详情等消费同一 frozen DTO / resolved DisplayName，不直接读取 Gameplay mutable state。

禁止：

```text
RepeatableUpgradeCapability.BuildUpgradeDisplaySuffix(...)
Widget 直接读取 CardInstance upgrade mutable state
UI if (CardId == "SearingBlow")
```

---

## 8. Runtime mutation authority

战斗内升级仍然由 Gameplay ActionQueue 掌权。

```text
Armaments
→ Select Card(s)
→ Query CanUpgrade
→ UpgradeCardAction
→ Upgrade boundary commits exactly-one runtime state transition
```

普通卡：

```text
Single(false) → Single(true)
Single(true)  → reject further upgrade
```

拥有 RepeatableUpgradeCapability 的卡：

```text
Repeatable(N) → Repeatable(N + 1)
```

Widget 不直接修改 upgrade runtime state。

如果没有真实 Trigger / Presentation consumer，不提前创建 CardUpgradedEvent。

---

## 9. Ownership layering

首个实现只需要 battle-scoped ownership，但边界不得妨碍未来 Run ownership：

```text
future persistent card entry
→ stores exactly one persistent upgrade-state shape selected by definition policy

battle materialization
→ UCardInstance receives the corresponding single/repeatable runtime state

combat temporary mutation
→ UpgradeCardAction
```

首个 Upgrade slice 不实现 Run Deck / campfire / save-load，只保留清晰 materialization boundary。

---

## 10. First implementation consumers

Upgrade Foundation 和第一批正式卡牌一起验证。

至少覆盖：

```text
普通 Damage 升级卡
→ Single(false) -> Single(true)
→ 第二次升级被拒绝
→ actual gameplay uses upgraded typed value
→ A3/committed card-face use the same effective value
→ Presentation state view 显示 CardName+

普通 Block 升级卡
→ 同一 Single path

Effect 参数升级卡
→ 例如 Draw 1 → Draw 2
→ 证明 Draw system 本身不知道 upgrade state
```

之后再用：

```text
Armaments
→ 验证 battle-time UpgradeCardAction

Searing Blow
→ definition selects Repeatable state shape
→ RepeatCount 0 → 1 → 2 → ...
→ authored typed effective value resolution
→ Presentation state view 显示 +1 / +2 / ...
```

关键验收是：

```text
普通卡保持最简单的 single bool semantics
特殊重复升级使用独立 repeatable state shape
同一实例不存在两个 authoritative upgrade mutable states
Repeatable capability 是 immutable policy，不持有 mutable count
升级 Gameplay state 不负责 Presentation 文本
具体升级数值仍属于 authored card/effect content
Gameplay / A3 / A2 frozen card-face 消费同一个 typed effective view
```

---

## 11. Relationship to Ironclad capability plan

`docs/IroncladCardArchitecturePlan.md` 中 CAP-01 继续表示 Upgrade 能力域：

```text
CAP-01A Default Single Upgrade
→ exactly-one Single runtime state

CAP-01B Optional Repeatable Upgrade Capability
→ immutable definition policy
→ selects Repeatable runtime state shape
→ only explicit consumers such as Searing Blow

Presentation
→ consumes FUpgradeStateView / frozen effective DTO
→ does not live inside CAP-01 Gameplay capability
```

Upgrade Foundation 在 Phase 8 seal 后作为 **Card Expansion Foundation** 提前实施；Ironclad 后续 Wave 中的 Armaments / Searing Blow 只是特殊 runtime-upgrade consumers，不代表 Upgrade Foundation 要等到最后才建立。

两者与 Damage / Exhaust / Selection / Dynamic Value 等能力域保持正交。

---

## 12. Non-goals for first Upgrade slice

首个 Upgrade Foundation 不自动包含：

```text
campfire UI
reward UI
map/run progression
save/load
shop
card acquisition
full Run Deck ownership
all 75 upgraded card assets
Effect graph structural mutation
universal upgrade expression language
```

---

## 13. Acceptance principles

```text
[ ] each CardInstance has exactly one authoritative upgrade-state shape
[ ] normal cards use Single state and can upgrade exactly once
[ ] repeatable definitions use Repeatable state and do not maintain an independent authoritative bUpgraded
[ ] impossible mixed states are structurally unrepresentable, not merely rejected at runtime
[ ] RepeatableUpgradeCapability is immutable policy/authored data, not mutable count storage
[ ] repeated upgrading is an optional capability assigned by card definition
[ ] no CardId-specific branch exists for Searing Blow
[ ] RepeatableUpgradeCapability does not know Damage/Draw/Block Effect types
[ ] Gameplay Upgrade code does not build UI suffix text
[ ] presentation consumes frozen FUpgradeStateView / effective DTO
[ ] normal upgraded title uses only "+"
[ ] repeatable title uses "+1", "+2", ...
[ ] effective-card boundary is typed and not a flattened Universal EffectiveCardFacts bag
[ ] actual gameplay, A3 preview and committed card-face consume the same effective card/effect values
[ ] first slice keeps Effect type/order/count unchanged and upgrades typed parameters only
[ ] Upgrade remains orthogonal to Damage/Draw/Exhaust/etc.
[ ] runtime mutation remains Action-authoritative
[ ] future Run persistence can materialize exactly one selected state shape
```

---

## 14. Next exact action

```text
Do not implement this document during Phase 8.

After Phase 8 is COMPLETE / VALIDATED / SEALED,
select Card Expansion / Upgrade Foundation as the next bounded implementation goal,
review exact current CardData / CardInstance / Effect contracts,
and authorize implementation explicitly.
```
