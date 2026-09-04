# Ironclad Full Card Architecture Plan

日期：**2026-09-04**

状态：**PLANNING REFERENCE / IMPLEMENTATION NOT AUTHORIZED**

本文件把《Slay the Spire 1》Ironclad 全部卡牌拆成长期内容需求与去重后的架构能力。它不是新的实施授权，也不扩大 `docs/Phase8ComboArchitectureDesign.md` 的 Phase 8 范围。

基线口径：Ironclad 有 **72 张常规卡池卡**（20 Common / 36 Uncommon / 16 Rare），另有起始牌 `Strike / Defend / Bash` 3 个不同定义，因此本文覆盖 **75 个不同 Ironclad 卡牌定义**。

本文不锁死每张卡的具体数值；数值属于 content authoring。本文锁定的是：这张卡需要什么 Gameplay / UI / Presentation 能力，哪些能力可以复用，哪些必须形成新的通用架构。

---

## 1. Current project baseline

当前稳定架构：

```text
UCardData / UCardInstance
→ CardEffect
→ BattleAction
→ BattleActionQueue
→ typed Modifier Pipeline
→ Commit
→ BattleEvent
→ Trigger
→ reaction BattleActions
```

当前 Card Effect 基础能力：

```text
UDamageCardEffect
UGainBlockCardEffect
UDrawCardEffect
UApplyStatusCardEffect
```

已有相关基础：

```text
Damage / Block Modifier Pipeline
Draw-N / real Shuffle continuation
Discard / Exhaust / Removed zones
Status / Curse card enum values
Status runtime + Trigger definitions
Relic runtime + Trigger definitions
GainEnergyAction
CardPlayed committed Presentation
A3 current-state Damage / Block / legality preview
```

当前关键缺口：

```text
BattleManager 仍是 Player + 单 Enemy 模型
BattleEvent 目前只有 TurnEnded / DeckShuffled
UCardInstance 基本只有 Definition + RuntimeId
StatusData 只有 DamageModifiers / BlockModifiers / Triggers
没有正式 card-selection pending choice
没有通用 Card cost/destination/playability modifier pipeline
没有 CardExhausted / CardDrawn / HPLost / BlockGained 等事件
没有通用 battle RNG / random target / random card pool service
没有正式 run-level card upgrade ownership
没有正式 typed Result -> authored Continuation 合同
Trigger Runtime Source 当前只有 Status / Relic，没有 Card source
```

---

## 2. Locked architecture rule — capabilities are orthogonal and composable

后续 Ironclad 扩展必须遵守以下长期规则：

> **能力之间可以通过稳定的数据合同组合，但不能彼此知道具体实现，也不能知道具体卡牌、Status、Relic 或组合消费者。**

同时锁定另一条同等重要的边界：

> **底层原语必须保持中立；卡牌内容 / authored orchestration 作为 composition layer，可以同时知道多个公共能力合同，并负责把它们组合起来。不要为了“零依赖”制造万能 Result Bus、万能 Context 或中央脚本解释器。**

设计目标不是：

```text
Capability A
→ knows Capability B internals
→ knows Capability C internals
```

也不是：

```text
Every capability
→ UniversalResultBus / UniversalContext
→ dynamic key/value interpretation
```

而是：

```text
Query / Spec / SelectionResult / CommitResult / BattleEvent
= shared neutral typed contracts

independent primitive capability
→ neutral typed contract
→ authored card/orchestration layer
→ neutral typed contract
→ independent primitive capability
```

例如 `CardEffect` 可以知道它要组合 `GainBlockAction`；但 `GainBlockAction` 不知道是哪张卡、哪个 Effect、哪个后续消费者调用了它。

### 2.1 Allowed dependency directions

允许的组合只能沿稳定公共合同发生：

```text
Read-only Query
→ returns fact/value only

CardEffect / Rule Source / authored Continuation
→ builds or modifies typed intent/spec/action

BattleAction
→ requests authoritative mutation

Runtime owner
→ commits mutation
→ returns exact typed CommitResult

Commit
→ emits immutable BattleEvent only when a real post-commit consumer exists

Trigger
→ reads Event + source snapshot
→ builds reaction BattleActions

Selection
→ returns SelectionResult
→ does not know what the caller will do with that result

RNG
→ returns deterministic index/order/choice
→ does not know card/enemy/zone semantics
```

### 2.2 Forbidden capability coupling

禁止形成以下关系：

```text
Selection system knows Exhaust
Exhaust system knows Feel No Pain / Dark Embrace
CardExhausted event knows its listeners
Draw system knows Evolve / Battle Trance
Card Rule Pipeline knows Corruption / Clash / Draw restriction
Dynamic Value system knows Body Slam / Perfected Strike
Deck Query knows Damage
RNG knows Sword Boomerang / Infernal Blade / Attack generation
Multi-enemy system knows Cleave / Whirlwind / Damage commits
HP-loss system knows Rupture / Blood for Blood counters
Upgrade system knows Armaments / Searing Blow special cases
```

同样禁止：

```cpp
if (CardId == "Corruption")
if (CardId == "PerfectedStrike")
if (CardId == "FeelNoPain")
if (CardId == "SearingBlow")
if (RelicId == "Sundial" && CardId == "PommelStrike")
if (DisplayName.Contains("Strike"))
```

### 2.3 Card is the composition root, not the capability implementation

具体卡牌 / authored rule object 负责组合独立能力。

例如 Corruption：

```text
Corruption content
→ Apply Status / Power

Corruption rule source
→ contributes generic Skill Cost Modifier
→ contributes generic Skill Destination Modifier

CardPlaySpec
→ CardRuleModifierPipeline
→ ResolvedCost = 0
→ ResolvedDestination = Exhaust

Zone commit
→ CardExhaustedEvent

Feel No Pain Trigger
→ GainBlockAction

Dark Embrace Trigger
→ DrawCardsAction
```

必须满足：

```text
Corruption 不知道 Feel No Pain
Corruption 不知道 Dark Embrace
Feel No Pain 不知道 Corruption
Dark Embrace 不知道 Corruption
Exhaust 不知道任何上述具体能力
```

例如 Perfected Strike：

```text
Card Trait metadata
→ DeckQuery.CountCardsWithTrait(Strike)
→ numeric fact
→ Dynamic Numeric Source
→ Damage Spec
→ existing Damage Modifier Pipeline
→ Damage commit
```

DeckQuery 只回答查询；Dynamic Numeric Source 只消费事实并计算数值；Damage Pipeline 不知道数值来自 DeckQuery。

例如 Burning Pact：

```text
CardSelectionRequest
→ SelectionResult
→ authored continuation/orchestration
→ ExhaustCardAction
→ typed Exhaust CommitResult
→ authored continuation/orchestration
→ DrawCardsAction
```

Selection 系统不应该出现 `SelectCardForExhaust`、`SelectCardForHeadbutt` 等具体能力 API；ExhaustAction 也不应该知道 Burning Pact 后面要 Draw。

### 2.4 Capability implementation requirements

每个新增能力域必须满足：

```text
[ ] 不查询具体 CardId / DisplayName / StatusId / RelicId 来决定通用行为
[ ] 可以单独测试自己的输入、输出、失败语义和 deterministic ordering
[ ] 对外暴露 typed Query / Spec / Result / Event，而不是另一能力的内部对象
[ ] 不通过 Widget / Presentation 反向驱动 Gameplay mutation
[ ] 不为当前唯一消费者设计专用万能接口
[ ] 出现第二个真实消费者后再决定是否继续抽象
[ ] 跨能力 integration test 验证组合，但不把 integration 逻辑下沉进任一能力
[ ] 不把新的 subsystem 指针继续塞进 FCardPlayContext 形成更大的 Service Bag
[ ] 不用任意字符串 key/value Result Bus 代替 typed contract
```

### 2.5 Planning waves are not architecture dependencies

本文后面的 `Wave` 只表示：

```text
推荐实现顺序
推荐验证顺序
风险控制顺序
```

它们**不表示能力 A 在语义上依赖能力 B**。

例如 `CardExhaustedEvent` 由 Exhaust commit 产生、Trigger 消费，这是通过稳定 Event contract 组合；不能理解成“Trigger 系统依赖 Exhaust 系统内部实现”。

如果某张卡同时使用多个能力：

```text
Card content/orchestration
├─ Capability A
├─ Capability B
└─ Capability C
```

而不是：

```text
Capability A
→ directly calls Capability B internals
→ directly calls Capability C internals
```

### 2.6 Locked resolution-local authored continuation contract

Ironclad 中存在一批必须先得到前序 Action 的真实结果，再决定后续 Action 的卡：

```text
Burning Pact
Fiend Fire
Feed
Reaper
以及其他 Result-dependent card composition
```

因此在实现这些卡之前，必须先建立一个**最小、typed、resolution-local authored Continuation** 合同。

它相对现有“依赖 Action 在 Execute 时读取可变状态”模式新增的核心能力只有一件事：

> **把“谁根据前序 Result 构建下游 Action”的职责从底层原语 Action 移到 authored card/continuation object。**

锁定边界：

```text
Continuation 必须：
- resolution-local
- authored（由 Card Definition / authored rule object 持有或引用）
- 非持久 runtime registry
- 读取一个明确 typed Result / Query / Predicate
- 只负责决定/构建后续 Action

Continuation 不是：
- BattleEvent
- Dispatcher listener
- persistent Trigger Registry
- Universal Result Bus
- arbitrary key/value Context
```

时序锁定：

```text
Action Execute
→ authoritative commit
→ obtain typed CommitResult
→ synchronously invoke authored Continuation with that Result
→ Continuation builds dependent Action batch
→ validate batch
→ enqueue dependent batch BEFORE current Action Finish()
→ do not pump / recursively execute Queue
→ current Action Finish()
```

Continuation 不参与 `Priority → RuntimeSequence → LocalTriggerIndex` 的 Trigger 排序；它属于当前 resolution 内部的直接依赖关系。

如果现有某个 dependent Action 可以安全地在 Execute 时解析动态值，可以继续复用该模式；只有当“原语 Action 被迫知道具体后续语义”时，才需要 authored Continuation。

### 2.7 Shared read-only predicate/query outlet

Dropkick、Spot Weakness 等条件卡需要共享同一个 read-only predicate/query 出口，不另造一套“Conditional Card System”。

允许：

```text
Gameplay Query
→ typed fact / predicate result
→ authored CardEffect / Continuation
→ conditionally builds normal Actions
```

例如：

```text
TargetStatusQuery(Vulnerable)
→ bool
→ Dropkick authored composition
→ if true: GainEnergyAction + DrawCardsAction

EnemyIntentQuery
→ typed intent fact
→ Spot Weakness authored composition
→ if Attack: GainStrengthAction
```

Predicate/query 只回答事实；它不执行后续 Gameplay。

### 2.8 Known legacy coupling guardrails — do not expand

当前 sealed architecture 已存在三处历史耦合热点：

```text
UCardEffect compile-time includes A3 Preview DTO
FCardPlayContext contains Battle / Deck / Dispatcher / Presentation writer/ids
PlayCardAction 同时承担 Gameplay commit + committed card-face freeze + Presentation snapshot
```

这些事实不在本规划中重新打开 Phase 6；但新 Ironclad 能力不得继续放大这些耦合：

```text
不继续向 FCardPlayContext 添加新 subsystem service
不让新底层 Action 反向依赖 UI / A3
不因为新卡牌把更多 Presentation authoring 塞进 Gameplay primitive
```

---

## 3. Architecture capability catalog

后续所有 Ironclad 卡牌都映射到以下能力编号。能力编号表示正交能力域，不表示彼此之间的直接依赖关系。

### CAP-00 — Existing primitive composition

无需新核心架构，只组合现有 Damage / Block / Draw / ApplyStatus / multi-hit / Energy / Destination。

### CAP-01 — Default single upgrade + optional repeatable upgrade capability

卡牌升级由默认单次升级能力和可选重复升级能力组成；不能为了极少数可重复升级卡，让所有卡牌共享整数等级模型。

默认能力：

```text
Default Card Upgrade
→ battle/runtime state uses bool bUpgraded
→ false = base
→ true = upgraded
→ normal card may upgrade exactly once
```

可选扩展能力只负责 Gameplay state/policy：

```text
Optional RepeatableUpgradeCapability
→ only explicitly authored cards receive it
→ owns repeatable UpgradeCount = 0, 1, 2, ...
→ generic repeated CanUpgrade / ApplyUpgrade
```

它**不负责**拼接 UI 文本，也不直接解释 Damage / Draw / Block 等 Effect。

统一 Gameplay 解析边界：

```text
Base Card Definition
+ default bUpgraded
+ optional repeatable state
→ EffectiveCardFacts
```

统一 Presentation 边界：

```text
Upgrade gameplay state
→ FUpgradeStateView / frozen effective DTO
→ presentation formatter
→ CardName+ 或 CardName+N
```

所有 card text / cost / effects / A2 / A3 / UI 读取统一的 effective/frozen boundary，不直接散落解释 `bUpgraded` 或 `UpgradeCount`。

Armaments 只通过 generic Upgrade query/action 操作卡牌；Searing Blow 只是被内容赋予 RepeatableUpgradeCapability 的消费者。Upgrade 系统不得识别任何具体 CardId。

禁止：

```text
全局 UpgradeLevel 作为所有卡牌的默认状态模型
if (CardId == "SearingBlow") 决定能否再次升级
RepeatableUpgradeCapability 内 if DamageEffect / DrawEffect / BlockEffect
Gameplay Upgrade capability 自己 BuildDisplaySuffix
Damage / Draw / UI 等能力直接读取 bUpgraded 或 UpgradeCount
```

Phase 8 不授权 CAP-01；CAP-01 在后续 Card Expansion / Upgrade Foundation 中独立实施。

### CAP-02 — Card traits + zone/deck/hand queries

```text
CardTraits / tags
count cards by trait / type / zone
query all active combat cards
query Hand composition
```

Perfected Strike 使用 `Strike` trait，不使用名字字符串匹配。

### CAP-03 — Dynamic numeric sources / formulas through neutral facts

Effect base value 可以来自动态 Gameplay facts，但 Dynamic Numeric 能力不能直接依赖产生这些 facts 的其他 capability 内部实现。

允许输入：

```text
authored constant
read-only Query result
operation-local typed CommitResult / Result
explicit runtime state snapshot
```

组合方向：

```text
independent capability
→ neutral Query / Result
→ authored card/orchestration
→ Dynamic Numeric Source
→ numeric value
→ typed Gameplay Spec
```

例如：

```text
CombatantQuery
→ CurrentBlock = N
→ Dynamic Numeric Source
→ GainBlockSpec(N)

DeckQuery
→ StrikeTraitCount = N
→ Dynamic Numeric Source
→ DamageSpec

Bulk card mutation
→ typed Result.SucceededCount = N
→ authored Continuation freezes/uses N
→ Dynamic Numeric Source
→ Damage / Block intent
```

Entrench 应表达为：

```text
GainBlockAction Execute-time resolution
→ read CurrentBlock = N through Query
→ resolve Amount = N
→ normal Block pipeline
→ commit GainBlock(N)
```

也就是“当前 20 Block → 再 Gain 20 Block”，而不是 CAP-17 提供一个专用 Block transform。金额必须在该 Action 的 Execute-time resolution boundary 解析，不能在更早的卡牌提交时冻结成过期值。

因此 Dynamic Numeric 只知道“当前输入事实/数值是什么”，不知道它来自 Block、Exhaust、DeckQuery 或其他具体能力；产生结果的能力也不知道数值最终会被 Damage、Block 或别的消费者使用。

禁止：

```text
Dynamic Numeric Source 直接访问另一 capability 的内部对象
查询“上一次 Exhaust 操作”之类的隐式历史状态
根据 CardId 决定去哪个系统取值
Exhaust / DeckQuery / Block 系统直接调用 Damage numeric logic
```

### CAP-04 — Pending card selection / player choice

```text
Hand selection
Discard selection
Exhaust selection
legal candidate query
cancel policy
Presentation/UI request
resume exact queued resolution
Automation deterministic chooser
```

Selection 只返回结果，不决定后续 Exhaust / Move / Copy 等行为。

### CAP-05 — Generic card-zone mutation / creation / copy

```text
create runtime card from definition
move Hand ↔ Draw/Discard/Exhaust
put card on DrawPile top
add card to Hand / Discard / DrawPile
bulk zone operations
```

DeckRuntime 继续拥有 zone truth。

复制规则采用**条件触发抽象**：当前 `UCardInstance` 只有 Definition + RuntimeId 时，不提前建立复杂 Clone framework；当 Upgrade / temporary cost / per-card combat state 与真正 Copy consumer 同批出现时，再引入 typed clone snapshot：

```text
Card runtime owner
→ BuildCloneSpec(CopyPolicy)
→ FCardCloneSpec
→ CardCreation materializes new runtime instance
```

`FCardCloneSpec` 只在第二个真实 mutable-state copy 需求出现时落地。CardCreation 不得逐字段认识 `bUpgraded / RepeatCount / TempCost / RampageState` 等具体能力。

### CAP-06 — Exhaust lifecycle + exact CardExhausted fact

```text
explicit exhaust from Hand
played card exhaust destination
automatic Ethereal exhaust
bulk exhaust
exact CardRuntimeId / CardId / source context
post-commit CardExhausted BattleEvent
```

`CardExhausted` 是 Ironclad 阶段需要显式新增的 committed Gameplay fact，不沿用 Phase 7E 的旧 non-goal；它必须随 Exhaust commit 一起独立设计和验证。

Exhaust 只负责 mutation + exact CommitResult / committed fact，不知道任何 Exhaust reactive Power，也不知道 Sentinel 的后续奖励。

### CAP-07 — Temporal card keywords / turn-scoped rules

```text
Ethereal
Innate
this-turn temporary statuses
turn-scoped trigger charges
end/start-turn expiry
```

不能靠具体卡 Tick 维护。

Battle Trance 的“本回合不能继续 Draw”虽然是 turn-scoped lifetime，但**Draw legality 本身属于 Draw request rule surface，不属于 CardPlayRule pipeline**。

### CAP-08 — HP loss / healing / MaxHP + exact HPLost fact

```text
Damage != Lose HP
LoseHP bypasses Block
Heal
MaxHP gain
HP-loss source attribution
post-commit HPLost fact
```

HP 系统不维护 Blood for Blood、Rupture 等消费者的“本场累计次数”。

计数状态必须有明确消费者宿主：

```text
card-specific persistent-in-combat counter
→ UCardInstance dedicated state / explicit card runtime state（例如 Blood for Blood）

status/power-owned counter
→ corresponding Status/Power runtime object（如果真实机制需要）
```

宿主通过 `HPLostEvent` 自己更新；HP mutation 只提交事实，不知道谁在计数。

### CAP-09 — Card play rule pipeline

把 `BaseCost / TargetType / DefaultDestination / QueryPlayability` 解析为 typed read-only card-play rules：

```text
playability predicates
resolved cost
resolved destination
X-cost
per-turn cost override
free-play policy
```

**Draw restriction 已从 CAP-09 删除。**

Draw 相关规则必须区分两类：

```text
Draw legality
→ CanDraw? / reject request

Draw amount modification
→ modifies requested/resolved draw count
```

Draw amount modifier 应复用现有 typed modifier pipeline 的同构确定性模型：

```text
Phase
→ Priority
→ RuntimeSequence
→ LocalModifierIndex
```

但仍是 Draw 自己的 typed rule/modifier surface，不把 Draw 语义塞回 CardPlayRule。

Pipeline 不知道 Corruption、Clash、Whirlwind 等名字；这些只是 modifier sources/consumers。

### CAP-10 — Autonomous / repeated card play

```text
play top card for free
repeat next Attack
preserve source/runtime identity
avoid duplicate cost spend
explicit destination policy
```

必须重新进入正常 Card play contract，而不是直接调用具体 Effect。

### CAP-11 — Multi-enemy combat model and target sets

```text
ordered enemy collection
AllEnemies target set
RandomEnemy candidate target set
single enemy target remains compatible
terminal state only after all enemies dead
Presentation participant identity for N enemies
```

Multi-enemy 能力只提供 ordered combatant collection / typed target set，不负责 per-target Damage/Status commit。

正确组合：

```text
TargetQuery / TargetPolicy
→ FTargetSet

DamageAction
→ consumes FTargetSet
→ owns Damage commits

ApplyStatusAction
→ consumes FTargetSet
→ owns Status commits
```

### CAP-12 — Deterministic battle RNG service

RNG 只提供领域无关的确定性随机原语：

```text
ChooseIndex(Count)
ChooseOne(Candidates)
Shuffle(Order)
seeded Automation reproducibility
```

消费者自己提供 ordered candidates。

例如：

```text
Enemy query → ordered candidates → RNG ChooseIndex
Hand query  → ordered candidates → RNG ChooseIndex
CardCatalog → Definition candidates → RNG ChooseIndex
```

RNG 不知道 Enemy、Hand、Attack、CardCatalog、draw-pile insertion 等消费者语义，也不负责“随机生成 Attack”。

### CAP-13 — Status amount transforms

```text
lose Strength
multiply current Strength
set / clamp / temporary delta
exact current Status instance mutation
```

### CAP-14 — Gameplay event surface expansion + generic Trigger source growth

按真实 committed fact 增加事件：

```text
TurnStarted
CardPlayed（Gameplay event，区别于 Presentation record）
CardDrawn
CardExhausted
HPLost
BlockGained
CombatantAttacked / DamageResolved
```

Event 只描述事实，不查询或调用 Listener。

#### CardExhausted + Card trigger source decision

Sentinel 需要同时具备：

```text
CardExhausted committed event
+ CardInstance 作为 Trigger Runtime Source
```

锁定：

```text
Sentinel trigger definition authored on CardData
runtime source identity = exhausted UCardInstance identity
no per-instance Trigger UObject is required
Exhaust system does not special-case Sentinel
```

现有 Phase 7 Trigger source 只有 Status / Relic；Ironclad 阶段允许新增 `Card` source kind，但不得破坏 Phase 7 已 sealed 的 Status/Relic 相对排序。

排序键决策锁定为：

```text
Priority
→ source ordering
   - Status vs Relic: preserve existing Phase7 RuntimeSequence ordering exactly
   - Card vs Card: stable Card RuntimeId ordering
   - Card vs existing Status/Relic at equal Priority: Card sources sort after existing non-card sources
→ LocalTriggerIndex
```

这样：

```text
deck setup 不消耗 battle RuntimeSequence
Status/Relic 既有相对顺序不变
Card trigger 获得稳定 deterministic ordering
```

如果以后出现必须让 Card 与 Status/Relic 交叉按统一 creation timeline 排序的真实需求，再单独设计新的 neutral source-order key；当前不为未来假设推翻 Phase 7 ordering。

### CAP-15 — Per-card runtime combat state

为 Rampage、Blood for Blood 等具体副本提供有限、显式 mutable combat state 或 dedicated state component。禁止 arbitrary key/value bag。

### CAP-16 — Status / Curse card definition and runtime rules

CAP-16 只定义 Status / Curse 卡牌本身的通用 card-facing 规则与 runtime identity，不拥有 Draw 或 Turn 的调度逻辑。

负责：

```text
Wound / Dazed / Burn 等 Card Definition
CardType = Status / Curse
traits / tags
playability rules
zone / default destination metadata
created card runtime identity
与卡本身相关的静态 keyword metadata
```

例如：

```text
Wound
→ Status
→ Unplayable

Dazed
→ Status
→ Unplayable
→ Ethereal metadata
```

跨系统行为通过中立事实组合：

```text
Draw commit
→ CardDrawnEvent
→ Evolve / Fire Breathing Trigger reads Event.Card facts
→ builds reaction Action

TurnEndedEvent
→ Burn-authored generic Trigger
→ builds its reaction Action

Ethereal resolution
→ CAP-07 temporal keyword/lifecycle contract
```

因此 Status/Curse card runtime 不知道 Evolve、Fire Breathing、Draw subsystem 或 Turn dispatcher；Draw/Turn 系统也不知道任何具体 Status/Curse card。

### CAP-17 — Block lifecycle / retention rule

CAP-17 只负责 Block 生命周期规则：

```text
turn boundary clear policy
retain Block across turn start（Barricade）
```

不提供 generic `current Block transform`。Entrench 等当前 Block 数值效果走 CAP-03 Query + Execute-time numeric resolution + normal GainBlock pipeline。

### CAP-18 — Damage outcome facts

Damage commit 输出 exact typed outcome：

```text
actual HP loss after Block
whether target died from this hit
per-target committed outcome
```

CAP-18 不负责：

```text
Fatal follow-up
Heal follow-up
MaxHP follow-up
cross-target aggregation policy
```

这些由 authored Continuation / generic typed aggregator 组合：

```text
DamageAction(s)
→ FDamageCommitResult(s)
→ typed aggregation if needed
→ authored Continuation
→ HealAction / GainMaxHPAction / other normal Action
```

Feed、Reaper 是消费者，不进入 Damage primitive。

### CAP-19 — Enemy intent query

Gameplay-facing、read-only、authoritative enemy intent query。不得读取 HUD 或 Presentation intent。

它通过 §2.7 的 shared predicate/query outlet 被 Spot Weakness 等 authored composition 消费。

### CAP-20 — Card catalog / generation pool

CAP-20 **只负责候选 Definition 查询**：

```text
query eligible card definitions
rarity/type/color filters
→ ordered candidate Definitions[]
```

它明确不负责：

```text
RNG pick
runtime instance creation
zone insertion
```

正确组合：

```text
CardCatalog
→ candidate Definitions[]
→ CAP-12 RNG chooses index
→ CAP-05 CardCreation materializes chosen Definition
→ Zone Action inserts runtime card
```

---

## 4. Full Ironclad card inventory → capability mapping

说明：下面只概括机制，不锁数值。`CAP-00` 表示核心 Gameplay 基本可由现有 primitive composition 完成；生产资产、文本、图标和 focused test 仍然要做。

### Basic — 3

| Card | Mechanic family | Required capabilities |
|---|---|---|
| Strike | 单体攻击 | CAP-00 |
| Defend | 自身格挡 | CAP-00 |
| Bash | 攻击 + Vulnerable | CAP-00 |

### Common — 20

| Card | Mechanic family | Required capabilities |
|---|---|---|
| Anger | 攻击后复制自身到弃牌堆 | CAP-05 |
| Armaments | 格挡 + 选择/批量临时升级手牌 | CAP-01, CAP-04 |
| Body Slam | 伤害基值 = 当前 Block | CAP-03 |
| Clash | 手牌全部为 Attack 才可打出 | CAP-02, CAP-09 |
| Cleave | 对所有敌人造成伤害 | CAP-11 |
| Clothesline | 攻击 + Weak | CAP-00 |
| Flex | 临时 Strength，本回合结束回退 | CAP-07, CAP-13 |
| Havoc | 顶牌免费自动打出并 Exhaust | CAP-05, CAP-09, CAP-10 |
| Headbutt | 攻击 + 从弃牌堆选择一张放抽牌堆顶 | CAP-04, CAP-05 |
| Heavy Blade | Strength 对本攻击有额外倍率 | CAP-03 |
| Iron Wave | 格挡 + 伤害 | CAP-00 |
| Perfected Strike | 根据 Strike trait 卡数量动态加伤害 | CAP-02, CAP-03 |
| Pommel Strike | 伤害 + Draw | CAP-00 |
| Shrug It Off | Block + Draw | CAP-00 |
| Sword Boomerang | 随机敌人多段伤害 | CAP-11, CAP-12 |
| Thunderclap | 全体伤害 + 全体 Vulnerable | CAP-11 |
| True Grit | Block + Exhaust 手牌；基础版随机、升级版选择 | CAP-04, CAP-06, CAP-12 |
| Twin Strike | 单体多段伤害 | CAP-00 |
| Warcry | Draw 后选择手牌置顶，自身 Exhaust | CAP-04, CAP-05, CAP-06 |
| Wild Strike | 攻击 + 向抽牌堆加入 Wound | CAP-05, CAP-12, CAP-16 |

### Uncommon — 36

| Card | Mechanic family | Required capabilities |
|---|---|---|
| Battle Trance | 大量 Draw + 本回合禁止继续 Draw | CAP-07 + Draw rule surface（不属于 CAP-09） |
| Blood for Blood | 本场每次失去 HP 后费用下降 | CAP-08, CAP-09, CAP-14, CAP-15 |
| Bloodletting | Lose HP → Gain Energy | CAP-08 |
| Burning Pact | 选择手牌 Exhaust → Draw | CAP-04, CAP-06 + authored Continuation |
| Carnage | Ethereal + 高伤害 | CAP-06, CAP-07 |
| Combust | 回合结束 Lose HP + 全体伤害 | CAP-07, CAP-08, CAP-11, CAP-14 |
| Dark Embrace | 每次 CardExhausted → Draw | CAP-06, CAP-14 |
| Disarm | 降低敌人 Strength，自身 Exhaust | CAP-06, CAP-13 |
| Dropkick | 攻击；若目标 Vulnerable 则 Gain Energy + Draw | shared predicate/query outlet + CAP-00 |
| Dual Wield | 选择 Attack/Power，创建副本进 Hand | CAP-04, CAP-05 |
| Entrench | 当前 Block 翻倍 | CAP-03 |
| Evolve | Draw Status 时额外 Draw | CAP-14, CAP-16 |
| Feel No Pain | 每次 CardExhausted → Gain Block | CAP-06, CAP-14 |
| Fire Breathing | Draw Status/Curse → 全体伤害 | CAP-11, CAP-14, CAP-16 |
| Flame Barrier | Block + 本回合受攻击时反伤 | CAP-07, CAP-14 |
| Ghostly Armor | Ethereal + Block | CAP-06, CAP-07 |
| Hemokinesis | Lose HP + 单体伤害 | CAP-08 |
| Infernal Blade | 随机生成 Attack 到 Hand，本回合 0 费，自身 Exhaust | CAP-05, CAP-06, CAP-07, CAP-09, CAP-12, CAP-20 |
| Inflame | Gain Strength Power | CAP-00 |
| Intimidate | 全体 Weak，自身 Exhaust | CAP-06, CAP-11 |
| Metallicize | 每回合结束 Gain Block | CAP-14 |
| Power Through | 创建 Wound 到 Hand + Block | CAP-05, CAP-16 |
| Pummel | 多段攻击，自身 Exhaust | CAP-06 |
| Rage | 本回合每打出 Attack → Gain Block | CAP-07, CAP-14 |
| Rampage | 每次此具体副本打出后，本场伤害永久增加 | CAP-03, CAP-15 |
| Reckless Charge | 攻击 + 向抽牌堆加入 Dazed | CAP-05, CAP-12, CAP-16 |
| Rupture | 因 Card 导致 Lose HP 时 Gain Strength | CAP-08, CAP-14 |
| Searing Blow | 可无限升级 | CAP-01 |
| Second Wind | Exhaust 手牌中所有非 Attack；按数量 Gain Block | CAP-02, CAP-03, CAP-06 + authored Continuation |
| Seeing Red | Gain Energy，自身 Exhaust | CAP-06 |
| Sentinel | Block；若此具体副本被 Exhaust 则 Gain Energy | CAP-06, CAP-14（CardExhausted + Card trigger source） |
| Sever Soul | Exhaust 所有非 Attack 手牌 + Damage | CAP-02, CAP-06 |
| Shockwave | 全体 Weak + Vulnerable，自身 Exhaust | CAP-06, CAP-11 |
| Spot Weakness | 若目标 Intent 为 Attack，则 Gain Strength | CAP-19 + shared predicate/query outlet |
| Uppercut | Damage + Weak + Vulnerable | CAP-00 |
| Whirlwind | X-cost；对所有敌人重复 X 次伤害 | CAP-09, CAP-11 |

### Rare — 16

| Card | Mechanic family | Required capabilities |
|---|---|---|
| Barricade | 回合开始不清除 Block | CAP-17 |
| Berserk | 先获得 Vulnerable；之后每回合开始 Gain Energy | CAP-14 |
| Bludgeon | 高额单体伤害 | CAP-00 |
| Brutality | 每回合开始 Lose HP + Draw；升级版 Innate | CAP-07, CAP-08, CAP-14 |
| Corruption | Skill 费用变 0；打出 Skill 后 Exhaust | CAP-06, CAP-09, CAP-14 |
| Demon Form | 每回合开始 Gain Strength | CAP-14 |
| Double Tap | 本回合下一张/两张 Attack 自动再执行一次 | CAP-07, CAP-10, CAP-14 |
| Exhume | 从 Exhaust pile 选择一张回 Hand，自身 Exhaust | CAP-04, CAP-05, CAP-06 |
| Feed | Damage；若该 hit Fatal，则永久提高 MaxHP；自身 Exhaust | CAP-06, CAP-08, CAP-18 + authored Continuation |
| Fiend Fire | Exhaust Hand；按成功 Exhaust 数量造成重复/比例伤害 | CAP-02, CAP-03, CAP-06 + authored Continuation |
| Immolate | 全体高伤害 + 创建 Burn 到 Discard | CAP-05, CAP-11, CAP-16 |
| Impervious | 高 Block，自身 Exhaust | CAP-06 |
| Juggernaut | 每次 Gain Block → 对随机敌人伤害 | CAP-11, CAP-12, CAP-14 |
| Limit Break | 当前 Strength 翻倍；基础版 Exhaust | CAP-06, CAP-13 |
| Offering | Lose HP + Gain Energy + Draw，自身 Exhaust | CAP-06, CAP-08 |
| Reaper | 全体伤害；按实际未被 Block 的 HP damage 总量 Heal；自身 Exhaust | CAP-06, CAP-08, CAP-11, CAP-18 + authored Continuation |

总计：

```text
Basic       3
Common     20
Uncommon   36
Rare       16
----------------
Distinct   75
```

---

## 5. Capability pressure / how many cards need each area

下面是粗粒度 planning 计数；一张卡可计入多个能力，因此总和不会等于 75。

```text
CAP-06 Exhaust lifecycle/event            ≈ 24 cards
CAP-14 Expanded committed events/triggers ≈ 17 cards
CAP-05 Zone mutation/create/copy           ≈ 11 cards
CAP-11 Multi-enemy targeting               ≈ 11 cards
CAP-07 Turn-scoped keywords/rules          ≈ 10 cards
CAP-08 HP loss/heal/maxHP                   ≈  9 cards
CAP-09 Card play rule pipeline              ≈  7 cards
CAP-04 Player card selection                ≈  7 cards
CAP-03 Dynamic values/formulas              ≈  7 cards
CAP-12 Battle RNG                           ≈  6 cards
CAP-16 Status/Curse card gameplay           ≈  6 cards
CAP-02 Traits / deck-hand query             ≈  5 cards
CAP-13 Status amount transforms             ≈  3 cards
CAP-01 Upgrade                              = Armaments + Searing Blow as direct special consumers
CAP-10 Autonomous/repeat play               = Havoc + Double Tap
CAP-18 Damage outcome                       = Feed + Reaper
CAP-15 Per-card combat state                = Rampage + Blood for Blood direct state consumers
CAP-17 Block retention                      = Barricade
CAP-19 Enemy intent query                    = Spot Weakness
CAP-20 Card generation catalog               = Infernal Blade
Draw request rule surface                    = Battle Trance direct consumer
Authored Result->Continuation                 = Burning Pact / Second Wind / Fiend Fire / Feed / Reaper etc.
```

最明显的容量压力仍然来自 Exhaust 与 expanded committed events，但这只是**优先级信号**，不是让两个能力域互相耦合的理由。

---

## 6. Recommended implementation / validation order — not dependency order

不要按 rarity 或卡牌名字逐张实现。下面 Wave 仅用于控制实现风险和选择集成验证消费者；**不是能力间依赖图**。

### Wave 0 — Current composition / Phase 8

```text
Strike / Defend / Bash
Clothesline
Iron Wave
Pommel Strike（当前生产/调试配置 Draw 2）
Shrug It Off
Twin Strike
Inflame
Uppercut
Bludgeon
```

目标：不新增核心架构，证明 current composition 稳定。Phase 8 使用现有 Draw 2 Pommel Strike + Sundial，不创建测试用 Pommel Strike+。

### Wave 1 — Exhaust fact surface + Card trigger-source prerequisite design

验证消费者：

```text
Impervious
Pummel
Seeing Red
Shockwave
Burning Pact
Feel No Pain
Dark Embrace
Sentinel（source-order design only when implemented）
```

在实现消费者前先定稿：

```text
Exhaust mutation
CardExhausted CommitResult / BattleEvent
resolution-local authored Continuation boundary
Card Trigger Runtime Source
Card-vs-Status/Relic deterministic ordering rule
```

Feel No Pain + Dark Embrace 验证同一个 neutral Event 可被多个独立 Trigger 消费；Sentinel 验证 CardData-authored Trigger + CardInstance runtime identity，而不是 Exhaust 特判。

### Wave 2 — Selection + generic zone operations

```text
Burning Pact
Headbutt
Warcry
Dual Wield
Exhume
True Grit
```

Selection 与 Zone Mutation 是两个独立能力域。Selection 只产生结果，authored orchestration / continuation 决定把结果交给 Exhaust / Move / Copy 中的哪一个操作。

### Wave 3 — Status/Curse cards + card creation

```text
Wild Strike
Reckless Charge
Power Through
Immolate
Evolve
Fire Breathing
```

分别验证 Card Creation、CardDrawn Event、Status/Curse rule。三者通过 neutral runtime identity / event contract 组合，不共享消费者特判。

### Wave 4 — Dynamic values / traits / result-dependent numeric composition

```text
Body Slam
Perfected Strike
Heavy Blade
Entrench
Second Wind
Fiend Fire
```

Body Slam / Perfected Strike / Entrench 证明 Query → numeric fact → typed spec；Second Wind / Fiend Fire 证明 typed Result → authored Continuation → Dynamic Numeric/Action，不允许原语互相认识。

### Wave 5 — Turn-scoped powers/events + Draw rule surface

```text
Metallicize
Demon Form
Berserk
Brutality
Rage
Flame Barrier
Flex
Battle Trance
```

Turn lifecycle 只产生 neutral turn facts / expiry boundary；Battle Trance 的持续时间由 turn-scoped state 管理，但 Draw legality / Draw amount modifier 属于 Draw 自己的 typed rule surface。

### Wave 6 — HP-loss and outcome semantics

```text
Bloodletting
Hemokinesis
Offering
Rupture
Blood for Blood
Feed
Reaper
Combust
```

明确 `LoseHP != Damage`。HP mutation、HPLost Event、consumer-owned counters、Damage outcome、authored Fatal/Heal continuation 分别保持独立职责。

### Wave 7 — Multi-enemy + deterministic RNG

```text
Cleave
Thunderclap
Intimidate
Shockwave
Whirlwind
Immolate
Reaper
Sword Boomerang
Juggernaut
```

Multi-enemy target query 与 RNG 是独立能力。RandomEnemy = ordered candidate query + deterministic RNG choice；RNG 不访问 BattleManager enemy internals，不知道 Enemy 语义。

### Wave 8 — Card rule modifier pipeline

```text
Clash
Blood for Blood
Whirlwind
Corruption
Infernal Blade
```

建立 typed CardPlaySpec / Modifier contract。Corruption 只是 Cost/Destination modifier source，不得成为 Pipeline 内部特判。Battle Trance 已移出本 Wave，因为 Draw restriction 不属于 CardPlayRule。

### Wave 9 — Autonomous/repeated play

```text
Havoc
Double Tap
```

两张牌共同验证 generic autonomous/repeated card play contract；该能力只重新提交 normal play intent，不知道具体后续 Effects。

### Wave 10 — Runtime mutation / upgrades

```text
Rampage      → per-card combat state
Armaments    → temporary combat upgrade through generic Upgrade action
Searing Blow → optional repeatable-upgrade capability state
```

Per-card state 与 Upgrade ownership 仍是独立能力域。默认普通卡只使用 bool `bUpgraded`；Searing Blow 的 `UpgradeCount` 只存在于 repeatable capability state，不成为所有卡牌的全局 UpgradeLevel。

Clone spec 仅在这些 mutable states 与真正 Copy consumer 同时形成需求时落地。

---

## 7. Durable review checklist for every new card

每增加一张新卡，先问：

```text
1. 这张卡需要读取哪些 Fact / Query / Predicate？
2. 它创建哪些 typed intent / Spec？
3. 哪个 BattleAction 承担 mutation？
4. 哪个 runtime owner 承担 authoritative commit？
5. CommitResult 需要冻结哪些事实？
6. 后续依赖的是 resolution-local Continuation 还是 post-commit BattleEvent？为什么？
7. 如果是 Continuation，是否 authored / typed / local，并在当前 Action Finish 前只构建+入队、不 pump queue？
8. 如果是 Event，是否真的存在独立 post-commit consumer？
9. Trigger 是否只读 Event + source snapshot 并 Build Action？
10. 是否需要 Selection？Selection 是否只返回结果？
11. 是否需要 RNG？RNG 是否只做 deterministic index/order choice？
12. 是否出现任意具体 Card/Status/Relic identity branch？
13. 是否让一个 primitive capability 直接调用了另一个 primitive capability 的内部实现？
14. integration 是否可以由 authored composition 只通过公共 typed contract 完成？
15. 是否正在把新的 subsystem service 塞进 FCardPlayContext？
16. 是否正在创建万能 Result Bus / string-key Context / arbitrary mutable property bag？
```

如果第 12、13、15、16 项为“是”，默认视为架构走偏，需要重新设计。

---

## 8. Key architectural decisions to preserve

未来实现全部 75 张时，必须保持：

```text
Card name / CardId 不决定组合行为
CardData immutable definition 与 runtime state 分离
ActionQueue 是 Gameplay mutation authority
DeckRuntime 是 zone truth
Modifier 在 commit 前改变 typed spec
BattleEvent 只描述已 commit 的事实
Trigger 只读 eligibility + Action build
Continuation 只处理当前 resolution 内 typed Result-dependent downstream build
Primitive capability 保持中立；authored composition layer 负责组合多个公共能力
能力域之间只通过 neutral typed contracts 组合
UI 不直接操作 Gameplay state
A2 只播放 committed facts
A3 只做当前状态、确定性、read-only preview
```

正确方向：

```text
Traits
Queries / Predicates
Typed Specs
SelectionResult
CommitResult
Resolution-local authored Continuation
Modifier Pipelines
BattleEvents
Triggers
Target Sets
Zone Actions
Deterministic RNG

→ orthogonal primitive capabilities
→ authored composition
→ individual card behavior emerges without consumer special cases
```

不要把上述合同合并成一个 universal context/result bus。

---

## 9. Phase 8 relationship

本规划不会把 Phase 8 扩成“实现所有 Ironclad 卡”。

Phase 8 当前保持：

```text
existing Draw 2 Pommel Strike + Sundial
→ Card → Draw → Shuffle → Event → Relic Reaction → Energy
→ Combo Architecture Validation
```

这个组合本身就是能力解耦的样板：Pommel Strike 不知道 Sundial，Draw 不知道 Relic，Shuffle 只提交事实，Sundial 只消费 `DeckShuffled` fact。

Phase 8 通过后，再从本文选择一个 bounded capability goal。后续选择 Wave 只决定实施顺序，不构成能力之间的语义依赖授权。

---

## 10. Planning status

```text
Inventory coverage:           75 / 75 distinct Ironclad card definitions
Architecture capabilities:    CAP-00 .. CAP-20 mapped + explicit cross-cutting contracts
Coupling rule:                ORTHOGONAL PRIMITIVES / AUTHORED COMPOSITION / CONTRACT-ONLY INTEGRATION
Continuation rule:            TYPED / RESOLUTION-LOCAL / AUTHORED / NON-EVENT
Card Trigger ordering:        PRESERVE PHASE7 STATUS/RELIC ORDER; CARD AFTER NON-CARD AT SAME PRIORITY
Implementation authorization: NONE
Phase 8 scope:                unchanged
```
