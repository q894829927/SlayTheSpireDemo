# Card Upgrade Foundation Design

日期：**2026-09-04**

状态：**DESIGN DRAFT / IMPLEMENTATION NOT AUTHORIZED**

本文件定义战士卡牌扩展阶段需要的通用 Upgrade Foundation。它不属于 Phase 8；Phase 8 仍只做现有 Pommel Strike Draw 2 + Sundial 的组合验证。

---

## 1. Goal

升级系统与正式卡牌内容一起开发，而不是为了 Phase 8 单独造测试升级卡，也不是等全部卡牌完成后再整体迁移。

核心设计原则：

```text
普通升级是默认能力：最多升级一次。
多次升级不是所有卡牌的默认状态维度，而是可选的独立能力。
Gameplay upgrade state 与 Presentation formatting 分离。
```

因此完整模型应是：

```text
Default Card Upgrade
→ bool bUpgraded

Optional Repeatable Upgrade Capability
→ only cards explicitly granted this capability can upgrade repeatedly

Upgrade State View / Effective Card Facts
→ frozen read-only boundary for gameplay/presentation consumers
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

所有 Gameplay / Presentation 消费者只读取统一的 effective card / frozen state boundary。

Repeatable Upgrade Capability 只扩展：

```text
能否再次升级
当前重复升级次数
如何提交重复升级状态
```

它不得直接知道 Damage、Draw、Block 等具体消费者，也不得负责 UI 文本拼接。

---

## 3. Default upgrade model

绝大多数卡牌只存在两个状态：

```text
Base
Upgraded
```

因此默认 runtime 状态允许直接使用：

```cpp
bool bUpgraded = false;
```

通用默认规则：

```text
bUpgraded = false
→ CanUpgrade = true

执行一次 Upgrade
→ bUpgraded = true

bUpgraded = true
→ CanUpgrade = false
```

这正好表达绝大多数 Slay the Spire 卡牌的真实升级规则，不需要为了极少数特殊卡牌让所有 UCardInstance 默认携带一个通用整数等级语义。

普通卡牌的升级内容仍由数据定义：

```text
Base Card Facts
+ Upgraded Card Facts / typed authored upgrade data
→ Effective Card Facts
```

具体采用完整 upgraded variant 还是 typed authored delta，在实施前结合当前 CardData / Effect 序列化结构决定；无论哪种形式，都不得形成 CardId 分支。

---

## 4. Optional Repeatable Upgrade Capability

多次升级能力从默认 Upgrade 中摘出，作为**可选、正交能力**赋给需要它的卡牌。

概念结构：

```text
UCardData
├─ normal upgrade definition
└─ optional RepeatableUpgradeCapability
```

运行时：

```text
普通卡
→ only bUpgraded

拥有 RepeatableUpgradeCapability 的卡
→ repeatable runtime upgrade state
→ UpgradeCount = 0, 1, 2, 3, ...
```

推荐的 Gameplay 接口语义只保留：

```text
CanUpgrade(CardInstance)
ApplyUpgrade(CardInstance)
GetUpgradeStateView(CardInstance)
```

其中：

```text
No RepeatableUpgradeCapability
→ generic single-upgrade path
→ bool bUpgraded

Has RepeatableUpgradeCapability
→ capability handles repeated CanUpgrade / ApplyUpgrade / UpgradeCount state
```

不在 Repeatable capability 中提供：

```text
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
→ optional RepeatableUpgradeCapability
→ generic Upgrade query/action boundary
```

未来若出现其他可多次升级卡，也只需要赋予同一能力或新的同类 capability，而不是修改默认升级系统。

---

## 5. Effective card boundary

升级能力不能迫使每个 Gameplay / UI 系统自己解释 `bUpgraded` 或 repeatable state。

统一 Gameplay resolution 入口负责：

```text
Base Card Definition
+ default bUpgraded state
+ optional repeatable state
+ permitted combat card state
+ authored upgrade data
→ EffectiveCardFacts
```

以下 Gameplay/preview 读取同一个 effective source：

```text
PlayCardAction
Card cost / playability
Card Effects
BattleTextResolver
A3 Immediate Preview
PresentationCardSnapshot builder
```

不得出现散落逻辑：

```cpp
if (Card->bUpgraded) { ... }
if (Card->RepeatUpgradeCount > 0) { ... }
```

不得让 Repeatable Upgrade Capability 自己识别 Damage / Draw / Block 等 Effect；具体升级内容由 authored card/effect data 参与 effective-card resolution。

---

## 6. Content authoring principle

升级发生什么变化属于 Card content，不属于 Upgrade runtime 的卡名规则。

升级可以改变：

```text
Damage amount
Block amount
Draw count
Status amount
Cost
Destination / keyword
Effect count / effect configuration
```

默认 Single upgrade 可以提供 Base / Upgraded 两套 authored facts，或等价的 typed authored upgrade data。

Repeatable 卡牌的 `UpgradeCount = N` 只是 Gameplay state。`N` 如何映射到该卡的 effective numeric/content facts，必须由该卡自己的 authored resolution data/function 表达，并通过统一 `EffectiveCardFacts` 输出；Repeatable Upgrade Capability 本身不识别具体 Effect 类型。

不得：

```cpp
if (CardId == "PommelStrike") DrawCount = 2;
if (CardId == "Bash") Vulnerable += 1;
if (CardId == "SearingBlow") BaseDamage = ...;
```

---

## 7. Locked presentation rule — state view, not Gameplay text building

Gameplay Upgrade System 不拼接名称后缀。

它只暴露冻结、只读的升级状态，例如：

```text
FUpgradeStateView
├─ bIsUpgraded
├─ bIsRepeatable
└─ RepeatCount
```

Presentation formatter 再根据这个 state view 生成显示：

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
single:
bIsUpgraded = false → ""
bIsUpgraded = true  → "+"

repeatable:
RepeatCount = 0 → ""
RepeatCount = N → "+N"
```

Native HUD、A2、A3、卡牌详情等消费同一冻结 DTO / resolved DisplayName，不直接读取 Gameplay mutable state。

禁止：

```text
RepeatableUpgradeCapability.BuildUpgradeDisplaySuffix(...)
Widget 直接读取 CardInstance.bUpgraded / UpgradeCount
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
→ Upgrade boundary commits state
```

默认普通卡：

```text
false → true
true → reject further upgrade
```

拥有 RepeatableUpgradeCapability 的卡：

```text
N → N + 1
```

Widget 不直接写 `bUpgraded`，也不直接修改 capability runtime state。

如果没有真实 Trigger / Presentation consumer，不提前创建 CardUpgradedEvent。

---

## 9. Ownership layering

首个实现只需要 battle-scoped ownership，但边界不得妨碍未来 Run ownership：

```text
future persistent card entry
→ stores default upgraded state
   or repeatable capability state

battle materialization
→ UCardInstance receives corresponding battle state

combat temporary mutation
→ UpgradeCardAction
```

首个 Upgrade slice 不实现 Run Deck / campfire / save-load，只保留清晰的 materialization boundary。

---

## 10. First implementation consumers

Upgrade Foundation 和第一批正式卡牌一起验证。

至少覆盖：

```text
普通 Damage 升级卡
→ Base → Upgraded
→ 第二次升级被拒绝
→ EffectiveCardFacts 数值正确
→ Presentation state view 显示 CardName+

普通 Block 升级卡
→ 同一 default bool path

Effect 参数升级卡
→ 例如 Draw 1 → Draw 2
→ 证明 Draw 系统本身不知道 bUpgraded
```

之后再用：

```text
Armaments
→ 验证 battle-time UpgradeCardAction

Searing Blow
→ 验证 optional RepeatableUpgradeCapability
→ UpgradeCount 0 → 1 → 2 → ...
→ authored effective value resolution
→ Presentation state view 显示 +1 / +2 / ...
```

关键验收是：

```text
普通卡保持最简单的 bool 升级模型
特殊重复升级能力独立存在
升级 Gameplay state 不负责 Presentation 文本
具体升级数值仍属于 authored card/effect content
所有消费者通过统一 effective/frozen boundary 读取结果
```

---

## 11. Relationship to Ironclad capability plan

`docs/IroncladCardArchitecturePlan.md` 中 CAP-01 继续表示 Upgrade 能力域：

```text
CAP-01A Default Single Upgrade
→ generic bool bUpgraded

CAP-01B Optional Repeatable Upgrade Capability
→ only explicit consumers such as Searing Blow
→ owns repeat count/policy only

Presentation
→ consumes FUpgradeStateView / frozen effective DTO
→ does not live inside CAP-01 Gameplay capability
```

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
```

---

## 13. Acceptance principles

```text
[ ] default card upgrade state can be represented by bool bUpgraded
[ ] normal cards can upgrade exactly once
[ ] second default upgrade is rejected generically
[ ] repeated upgrading is not part of every card's default model
[ ] repeatable upgrading is an optional capability assigned by card definition
[ ] no CardId-specific branch exists for Searing Blow
[ ] RepeatableUpgradeCapability does not know Damage/Draw/Block Effect types
[ ] Gameplay Upgrade code does not build UI suffix text
[ ] presentation consumes frozen FUpgradeStateView / effective DTO
[ ] normal upgraded title uses only "+"
[ ] repeatable title uses "+1", "+2", ...
[ ] all Gameplay consumers use one effective-card boundary
[ ] Upgrade remains orthogonal to Damage/Draw/Exhaust/etc.
[ ] runtime mutation remains Action-authoritative
[ ] future Run persistence can materialize both default and repeatable state
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
