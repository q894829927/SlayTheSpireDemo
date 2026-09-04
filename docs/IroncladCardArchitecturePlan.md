# Ironclad Full Card Architecture Plan

日期：**2026-09-04**

状态：**PLANNING REFERENCE / IMPLEMENTATION NOT AUTHORIZED**

本文件把《Slay the Spire 1》Ironclad 全部卡牌拆成长期内容需求与去重后的架构能力。它不是新的实施授权，也不扩大 `docs/Phase8ComboArchitectureDesign.md` 的 Phase 8 范围。

基线口径：Ironclad 有 **72 张常规卡池卡**（20 Common / 36 Uncommon / 16 Rare），另有起始牌 `Strike / Defend / Bash` 3 个不同定义，因此本文覆盖 **75 个不同 Ironclad 卡牌定义**。

本文不锁死每张卡的具体数值；数值属于 content authoring。本文锁定的是能力边界、公共合同、组合方式和长期实现/验证顺序。

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
BattleEvent 当前只覆盖已实现的少量 committed facts
UCardInstance 基本只有 Definition + RuntimeId
没有正式 card-selection pending choice
没有通用 Card cost/destination/playability modifier pipeline
没有 CardExhausted / CardDrawn / HPLost / BlockGained 等后续事件
没有通用 battle RNG / random target / random card pool service
没有正式 typed Result -> authored Continuation 合同
Trigger Runtime Source 当前只有 Status / Relic，没有 Card source
没有正式 Upgrade Foundation runtime/effective-view implementation
```

---

## 2. Locked architecture rule — orthogonal primitives, authored composition

长期原则：

> **能力之间可以通过稳定 typed contract 组合，但 primitive capability 不能知道另一 primitive capability 的内部实现，也不能知道具体 Card / Status / Relic 消费者。**

同时：

> **Card content / authored orchestration 是 composition layer，可以同时知道多个公共能力合同并负责组合；不要为了“零依赖”制造万能 Result Bus、万能 Context、万能 mutable property bag 或中央脚本解释器。**

正确形状：

```text
independent primitive capability
→ typed neutral contract
→ authored card/orchestration
→ typed neutral contract
→ independent primitive capability
```

不是：

```text
Capability A
→ calls Capability B internals
```

也不是：

```text
Everything
→ UniversalResultBus / UniversalContext
→ dynamic string-key interpretation
```

例如 `CardEffect` 可以知道它要构建 `GainBlockAction`；但 `GainBlockAction` 不知道是哪张卡、哪个 Effect、哪个后续消费者调用了它。

### 2.1 Allowed dependency directions

```text
Read-only Query / Predicate
→ fact/value only

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
→ SelectionRequest / SelectionResult
→ does not know later Exhaust / Move / Copy semantics

RNG
→ deterministic index/order/shuffle only
→ does not know card/enemy/zone semantics
```

### 2.2 Forbidden coupling

```text
Selection system knows Exhaust
Exhaust system knows Feel No Pain / Dark Embrace / Sentinel
CardExhausted event knows listeners
Draw system knows Evolve / Battle Trance
Card Rule Pipeline knows Corruption / Clash / Draw restriction
Dynamic Value system knows Body Slam / Perfected Strike
Deck Query knows Damage
RNG knows Sword Boomerang / Infernal Blade / Attack generation
Multi-enemy system knows Cleave / Whirlwind / Damage commits
HP-loss system knows Rupture / Blood for Blood counters
Upgrade system knows Armaments / Searing Blow special cases
Dispatcher traverses Deck zones and interprets Card semantics
```

禁止 identity branch：

```cpp
if (CardId == "Corruption")
if (CardId == "PerfectedStrike")
if (CardId == "FeelNoPain")
if (CardId == "SearingBlow")
if (RelicId == "Sundial" && CardId == "PommelStrike")
if (DisplayName.Contains("Strike"))
```

### 2.3 Card is the composition root

Corruption：

```text
Corruption content
→ Apply Status / Power

Corruption rule source
→ generic Skill Cost Modifier
→ generic Skill Destination Modifier

CardPlaySpec
→ CardRuleModifierPipeline
→ ResolvedCost = 0
→ ResolvedDestination = Exhaust

Zone commit
→ CardExhaustedEvent

independent listeners such as Feel No Pain / Dark Embrace
→ consume neutral Event
```

Corruption 不需要 `CardPlayedEvent` 来实现核心 Cost/Destination 行为；因此不把 CAP-14 当作 Corruption 的必需能力。

Perfected Strike：

```text
Card Trait metadata
→ DeckQuery.CountCardsWithTrait(Strike)
→ numeric fact
→ Dynamic Numeric Source
→ DamageSpec
→ normal Damage pipeline
```

Burning Pact：

```text
CardSelectionRequest
→ SelectionResult
→ authored orchestration
→ ExhaustCardAction
→ typed Exhaust CommitResult
→ authored Continuation
→ DrawCardsAction
```

Selection 不知道 Exhaust；ExhaustAction 不知道后续 Draw。

### 2.4 Capability implementation checklist

```text
[ ] 不查询具体 CardId / DisplayName / StatusId / RelicId 来决定通用行为
[ ] 可单独测试输入、输出、失败语义和 deterministic ordering
[ ] 对外暴露 typed Query / Predicate / Spec / Result / Event
[ ] 不通过 Widget / Presentation 反向驱动 Gameplay mutation
[ ] 不为当前唯一消费者造万能接口
[ ] 第二个真实需求出现后再继续抽象
[ ] integration 逻辑留在 authored composition，不下沉 primitive
[ ] 不继续向 FCardPlayContext 添加 subsystem service
[ ] 不继续向 FTriggerContext 添加 subsystem service
[ ] 不用 string-key Result Bus / arbitrary mutable property bag
```

### 2.5 Waves are implementation order, not dependency graph

后文 Wave 只表示推荐实现/验证顺序。

```text
Card content/orchestration
├─ Capability A
├─ Capability B
└─ Capability C
```

而不是：

```text
Capability A
→ Capability B internals
→ Capability C internals
```

---

## 2.6 Locked resolution-local authored Continuation contract

需要真实前序 Result 才能决定后续 Action 的典型卡：

```text
Burning Pact
Second Wind
Fiend Fire
Feed
Reaper
```

Continuation 的目的只有一个：

> **把“谁根据前序 typed Result 构建下游 Action”的职责从 primitive Action 移到 authored card/continuation object。**

锁定：

```text
Continuation 必须：
- typed
- resolution-local
- authored
- immutable / stateless definition object
- non-persistent registry
- 只读 Result / Query / Predicate
- 只负责构建 dependent Action batch

Continuation 不是：
- BattleEvent
- Dispatcher listener
- persistent Trigger Registry
- Universal Result Bus
- arbitrary key/value Context
```

禁止 authored definition object 保存：

```text
LastResult
PendingCard
CurrentTarget
任何 resolution-specific mutable state
```

动态状态只能存在于：

```text
executing Action
typed CommitResult
local invocation data
```

### Sealed reaction-before-continuation ordering

此处不发明新的 Queue 模型，直接泛化已 sealed 的 Draw/Shuffle precedent：

```text
DrawCardsAction
→ first queues [Draw..., Shuffle, RemainingDraw] at Queue front

ShuffleDeckAction
→ commits Shuffle
→ dispatches DeckShuffled

Dispatcher
→ inserts reactions at Queue front

final:
Shuffle
→ reactions
→ RemainingDraw
→ previously pending work
```

未来同一次 commit 同时有 Continuation 和 BattleEvent 时，使用同一机制：

```text
Action Execute
→ authoritative commit
→ obtain typed CommitResult
→ Continuation builds dependent batch
→ validate batch
→ AddBatchToFrontPreserveOrder(ContinuationBatch)
→ dispatch committed BattleEvent
→ Dispatcher reactions insert ahead of ContinuationBatch
→ Finish
```

最终执行顺序：

```text
same-commit Trigger reactions
→ authored Continuation Actions
→ previously pending Actions
```

失败路径：

```text
Continuation build/insert failure
→ RequestResolutionFault
→ Finish
→ return immediately
→ do NOT dispatch afterward from that failed path
```

不能依赖 `RequestResolutionFault` 后继续调用 Dispatcher 并期待它自动拒绝；Queue fault 只在 safe point 正式进入。

Continuation 不参与 Trigger 的 source ordering。

---

## 2.7 Shared Predicate / Query outlet and timing

Dropkick、Spot Weakness 等条件卡共享 read-only Query/Predicate 出口，不新造“Conditional Card System”。

```text
Gameplay Query
→ typed fact / predicate
→ authored Effect / Continuation
→ conditionally builds normal Actions
```

时序沿用既有不变量：

```text
mutable-state-dependent value / predicate
→ resolve at Action Execute-time

predicate depends on previous Action result
→ resolve inside authored Continuation

immutable/static predicate
→ may be frozen earlier
```

因此 Dropkick 不应在 `PlayCardAction` 预构建全部 Effect 时冻结一个可能过期的 Vulnerable 状态；第一个条件牌应示范 Execute-time predicate resolution，而不是新机制。

---

## 2.8 Known sealed coupling guardrails — do not expand

当前 sealed architecture 已有历史耦合热点：

```text
UCardEffect compile-time includes A3 Preview DTO
FCardPlayContext contains Battle / Deck / Dispatcher / Presentation writer/ids
FTriggerContext contains RuntimeSource / ActionOuter / Battle / Presentation writer
PlayCardAction 同时承担 Gameplay commit + committed card-face freeze + Presentation snapshot
```

这些不在本规划中重开旧 Phase；但新能力不得放大：

```text
不继续向 FCardPlayContext 添加 subsystem service
不继续向 FTriggerContext 添加 subsystem service
不把 FTriggerContext::GetBattle() 当任意 subsystem locator
新 Trigger 优先消费 Event + source snapshot + typed Query
不让新 primitive Action 反向依赖 UI / A3
不把更多 Presentation authoring 塞进 Gameplay primitive
```

---

## 3. Architecture capability catalog

### CAP-00 — Existing primitive composition

现有 Damage / Block / Draw / ApplyStatus / multi-hit / Energy / Destination 的组合。

### CAP-01 — Upgrade Foundation

完整 authority：`docs/CardUpgradeFoundationDesign.md`。

锁定：

```text
normal definition
→ exactly-one Single runtime state
→ bool bUpgraded semantics

repeatable definition
→ immutable RepeatableUpgradeCapability policy on CardData
→ exactly-one Repeatable runtime state on CardInstance/dedicated runtime state
→ RepeatCount
```

Single/Repeatable 两种 authoritative mutable state **互斥**；不允许同时维护 `bUpgraded` 和 `RepeatCount` 再同步。

Upgrade Core 不知道 Damage/Draw/Block；Presentation 只消费 frozen `FUpgradeStateView`。

Effective boundary 是 typed card/effect view，不是万能 flattened `EffectiveCardFacts` bag：

```text
EffectiveCardView
├─ card-level typed facts
└─ EffectiveEffects[]
   └─ typed effective parameters per Effect
```

首个 Upgrade slice：

```text
Effect type/order/count unchanged
only typed authored parameters change
```

Phase 8 seal 后先建立 generic Upgrade Foundation；Armaments/Searing Blow 只是以后对应 card batch 的特殊消费者。

### CAP-02 — Card traits + zone/deck/hand queries

```text
CardTraits / tags
count cards by trait / type / zone
query active combat cards
query Hand composition
```

Perfected Strike 使用 `Strike` trait，不做名字匹配。

### CAP-03 — Dynamic numeric sources / formulas

允许输入：

```text
authored constant
read-only Query result
operation-local typed CommitResult
explicit runtime state snapshot
```

```text
independent producer
→ typed Query / Result
→ authored composition
→ Dynamic Numeric Source
→ numeric value
→ typed Gameplay Spec
```

Entrench：

```text
GainBlockAction Execute-time
→ Query CurrentBlock = N
→ Amount = N
→ normal Block pipeline
→ GainBlock(N)
```

不建立 CAP-17 `Block transform`。

### CAP-04 — Pending card selection / player choice

Gameplay core：

```text
candidate query
cancel policy
pending selection state
immutable SelectionRequest DTO
resume exact queued resolution
Automation deterministic chooser
SelectionResult
```

Presentation boundary：

```text
Gameplay pending state
→ SelectionRequest DTO
→ Presentation Adapter / UI
→ user command
→ SelectionResult
→ Gameplay resumes
```

Selection Core 不知道 Widget，也不决定后续 Exhaust / Move / Copy。

### CAP-05 — Generic card-zone mutation / creation / copy

```text
create runtime card from definition
move Hand ↔ Draw/Discard/Exhaust
put card on DrawPile top
add card to Hand / Discard / DrawPile
bulk zone operations
```

DeckRuntime 继续拥有 zone truth。

Clone abstraction 条件触发：只有 mutable card state 与真实 Copy consumer 同批出现时才引入 `FCardCloneSpec`。

```text
Card runtime owner
→ BuildCloneSpec(CopyPolicy)
→ FCardCloneSpec
→ CardCreation materializes new runtime instance
```

RuntimeId 不属于可复制状态：

```text
Every newly materialized UCardInstance
→ fresh battle-unique RuntimeId
→ same authoritative NextRuntimeId allocator
```

包括 initial/create/copy/clone。绝不复制来源 `RuntimeId`。

### CAP-06 — Exhaust lifecycle + exact CardExhausted fact

```text
explicit exhaust from Hand
played-card Exhaust destination
automatic Ethereal exhaust
bulk exhaust
exact CardRuntimeId / CardId / source context
post-commit CardExhausted BattleEvent
```

Exhaust 只负责 mutation + CommitResult / committed fact，不知道任何 reactive Power 或 Sentinel。

### CAP-07 — Temporal card keywords / turn-scoped rules

```text
Ethereal
Innate
this-turn temporary statuses/rules
turn-scoped trigger charges
end/start-turn expiry
```

Keyword metadata 是 authored fact；对应 lifecycle executor 通过正常 Zone/Action/Rule 合同执行，不让具体卡 Tick，也不让 CAP-07 自己越权操作 Deck internals。

Battle Trance 的持续时间属于 turn-scoped state；Draw legality 属于独立 Draw rule surface。

### CAP-08 — HP loss / healing / MaxHP + exact HPLost fact

```text
Damage != LoseHP
LoseHP bypasses Block
Heal
MaxHP gain
source attribution
post-commit HPLost fact
```

HP primitive 不维护消费者计数。

```text
Blood for Blood-style count
→ explicit card runtime state

Status/Power-owned count
→ corresponding runtime owner
```

宿主通过 Event 更新自身状态。

### CAP-09 — Card play rule pipeline

```text
playability predicates
resolved cost
resolved destination
X-cost
per-turn cost override
free-play policy
```

不包含 Draw restriction。

Draw 独立规则：

```text
Draw legality
→ CanDraw / reject

Draw amount modifier
→ modifies requested/resolved count
→ deterministic Phase / Priority / RuntimeSequence / LocalModifierIndex ordering
```

### CAP-10 — Autonomous / repeated card play

```text
play top card for free
repeat next Attack
preserve source/runtime identity
avoid duplicate cost spend
explicit destination policy
```

重新进入 normal Card play contract，不直接执行具体 Effect。

### CAP-11 — Multi-enemy combat model and target sets

只负责 Gameplay target model：

```text
ordered enemy collection
AllEnemies target set
RandomEnemy candidate target set
single enemy compatibility
terminal state only after all enemies dead
stable combatant identity
```

```text
TargetQuery / TargetPolicy
→ FTargetSet

DamageAction
→ consumes FTargetSet
→ Damage commits

ApplyStatusAction
→ consumes FTargetSet
→ Status commits
```

**CAP-11 不负责 Presentation participant identity mapping。** Presentation adapter 从 stable combatant identity 构建 frozen participant/presentation mapping。

### CAP-12 — Deterministic battle RNG

领域无关原语：

```text
ChooseIndex(Count)
ChooseOne(ordered candidates)
Shuffle(Order)
seeded Automation reproducibility
```

RNG 不知道 Enemy / Hand / Attack / CardCatalog / zone insertion。

### CAP-13 — Status amount transforms

```text
lose Strength
multiply current Strength
set / clamp / temporary delta
exact Status-instance mutation
```

### CAP-14 — Gameplay event surface expansion

按真实 committed fact 增加：

```text
TurnStarted
CardPlayed（仅真实独立消费者需要时）
CardDrawn
CardExhausted
HPLost
BlockGained
CombatantAttacked / DamageResolved
```

Event 只描述事实，不调用 listener。

Card Trigger Source Expansion 不塞进 Sentinel；独立 authority：

```text
docs/CardTriggerSourceExpansionDesign.md
```

单一比较键：

```text
Priority
→ SourceTier
   Status / Relic = 0
   Card           = 1
→ SequenceKey
   Status / Relic = RuntimeSequence
   Card           = RuntimeId
→ LocalTriggerIndex
```

Source discovery 必须通过 typed provider：

```text
Card runtime owner / provider
→ eligible FTriggerRuntimeSource snapshots
→ Battle composition layer
→ Dispatcher
```

Dispatcher 不遍历 Deck zones 解释 Card semantics。

该能力是独立 future slice，必须带：

```text
focused Card-source Automation
+ Phase7 Status/Relic ordering regression
+ Phase6 trigger-order regression
```

### CAP-15 — Per-card runtime combat state

Rampage、Blood for Blood 等具体副本使用有限、显式 card runtime state / dedicated state component。禁止 arbitrary key/value bag。

### CAP-16 — Status / Curse card definition and runtime rules

CAP-16 只定义 Card-facing authored facts：

```text
Status / Curse Card Definition
CardType
traits / tags
authored playability metadata / rule source
zone/default destination metadata
created-card runtime identity
static keyword metadata
```

真正 playability resolution 仍进入唯一 CAP-09 rule engine；CAP-16 不拥有第二套 playability engine。

跨系统行为：

```text
CardDrawnEvent
→ Evolve / Fire Breathing Trigger

TurnEndedEvent
→ Burn-authored Trigger

Ethereal
→ CAP-07 lifecycle
```

CAP-16 不拥有 Draw / Turn dispatch。

### CAP-17 — Block lifecycle / retention

```text
turn-boundary clear policy
retain Block across turn start (Barricade)
```

不提供 generic current-Block transform。

### CAP-18 — Damage outcome facts

Damage commit 输出：

```text
actual HP loss after Block
whether target died from this hit
per-target committed outcome
```

不负责：

```text
Fatal follow-up
Heal follow-up
MaxHP follow-up
cross-target aggregation policy
```

```text
FDamageCommitResult(s)
→ typed aggregation if needed
→ authored Continuation
→ normal downstream Actions
```

### CAP-19 — Enemy intent query

Gameplay-facing、read-only、authoritative intent Query。不得读取 HUD/Presentation intent；通过共享 Predicate/Query outlet 被 Spot Weakness 消费。

### CAP-20 — Card catalog / generation pool

只负责：

```text
query eligible card definitions
rarity/type/color filters
→ ordered candidate Definitions[]
```

不负责 RNG / runtime creation / zone insertion。

```text
CardCatalog
→ candidates
→ CAP-12 RNG chooses index
→ CAP-05 CardCreation
→ Zone Action
```

---

## 4. Full Ironclad card inventory → capability mapping

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
| Battle Trance | 大量 Draw + 本回合禁止继续 Draw | CAP-07 + Draw rule surface |
| Blood for Blood | 本场每次失去 HP 后费用下降 | CAP-08, CAP-09, CAP-14, CAP-15 |
| Bloodletting | Lose HP → Gain Energy | CAP-08 |
| Burning Pact | 选择手牌 Exhaust → Draw | CAP-04, CAP-06 + authored Continuation |
| Carnage | Ethereal + 高伤害 | CAP-06, CAP-07 |
| Combust | 回合结束 Lose HP + 全体伤害 | CAP-07, CAP-08, CAP-11, CAP-14 |
| Dark Embrace | 每次 CardExhausted → Draw | CAP-06, CAP-14 |
| Disarm | 降低敌人 Strength，自身 Exhaust | CAP-06, CAP-13 |
| Dropkick | 攻击；若目标 Vulnerable 则 Gain Energy + Draw | shared Predicate/Query + CAP-00 |
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
| Sentinel | Block；若此具体副本被 Exhaust 则 Gain Energy | CAP-06, CAP-14 + Card Trigger Source Expansion |
| Sever Soul | Exhaust 所有非 Attack 手牌 + Damage | CAP-02, CAP-06 |
| Shockwave | 全体 Weak + Vulnerable，自身 Exhaust | CAP-06, CAP-11 |
| Spot Weakness | 若目标 Intent 为 Attack，则 Gain Strength | CAP-19 + shared Predicate/Query |
| Uppercut | Damage + Weak + Vulnerable | CAP-00 |
| Whirlwind | X-cost；对所有敌人重复 X 次伤害 | CAP-09, CAP-11 |

### Rare — 16

| Card | Mechanic family | Required capabilities |
|---|---|---|
| Barricade | 回合开始不清除 Block | CAP-17 |
| Berserk | 先获得 Vulnerable；之后每回合开始 Gain Energy | CAP-14 |
| Bludgeon | 高额单体伤害 | CAP-00 |
| Brutality | 每回合开始 Lose HP + Draw；升级版 Innate | CAP-07, CAP-08, CAP-14 |
| Corruption | Skill 费用变 0；打出 Skill 后 Exhaust | CAP-06, CAP-09 |
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

```text
Basic       3
Common     20
Uncommon   36
Rare       16
----------------
Distinct   75
```

---

## 5. Capability pressure

粗粒度 planning 信号：

```text
CAP-06 Exhaust lifecycle/event            ≈ 24 cards
CAP-14 Expanded committed events/triggers ≈ 17 cards
CAP-05 Zone mutation/create/copy           ≈ 11 cards
CAP-11 Multi-enemy targeting               ≈ 11 cards
CAP-07 Turn-scoped keywords/rules          ≈ 10 cards
CAP-08 HP loss/heal/maxHP                   ≈  9 cards
CAP-04 Selection                            ≈  7 cards
CAP-03 Dynamic values/formulas              ≈  7 cards
CAP-09 Card play rules                      ≈  6–7 cards
CAP-12 RNG                                  ≈  6 cards
CAP-16 Status/Curse card authoring          ≈  6 cards
CAP-02 Traits/zone queries                  ≈  5 cards
CAP-13 Status amount transforms             ≈  3 cards
CAP-01 Upgrade special consumers            = Armaments + Searing Blow
CAP-10 Autonomous/repeat play               = Havoc + Double Tap
CAP-18 Damage outcome                       = Feed + Reaper
CAP-15 Per-card combat state                = Rampage + Blood for Blood
CAP-17 Block retention                      = Barricade
CAP-19 Enemy intent                         = Spot Weakness
CAP-20 Card catalog                         = Infernal Blade
Draw rule surface                            = Battle Trance direct consumer
Authored Result->Continuation                = Burning Pact / Second Wind / Fiend Fire / Feed / Reaper ...
Card Trigger Source Expansion                = Sentinel first direct validation consumer; future card-owned triggers may reuse
```

这些只是优先级信号，不是 capability dependency graph。

---

## 6. Recommended implementation / validation order

### Foundation 0 — Phase 8 first

Phase 8 使用 transient authored Draw-2 test definition + Sundial 做架构验证；Production PIE 继续保留当前 Draw-2 Pommel evidence。

Phase 8 Automation 不依赖未来 Pommel Base/Upgrade 数值。

### Foundation 1 — Card Expansion / Upgrade Foundation

**Phase 8 seal 后立即进行。**

```text
Default Single upgrade state
Optional Repeatable definition policy
exactly-one runtime upgrade-state shape
typed EffectiveCardView / EffectiveEffects[]
first normal upgraded cards
```

这一步不是 Wave 10；generic Upgrade Foundation 在正式大批卡牌开发前建立。

### Wave 1 — Exhaust fact surface

```text
Impervious
Pummel
Seeing Red
Shockwave
Burning Pact
Feel No Pain
Dark Embrace
```

需要：Exhaust commit + CardExhausted Event + authored Continuation boundary。

### Independent foundation slice — Card Trigger Source Expansion

在 Sentinel/Card-trigger consumers 之前单独实施：

```text
source-provider boundary
Card source kind
Priority -> SourceTier -> SequenceKey -> LocalTriggerIndex
fresh RuntimeId invariant
Phase7 + Phase6 ordering regression gates
```

Authority: `docs/CardTriggerSourceExpansionDesign.md`。

### Wave 2 — Selection + generic zone operations

```text
Burning Pact
Headbutt
Warcry
Dual Wield
Exhume
True Grit
```

### Wave 3 — Status/Curse cards + card creation

```text
Wild Strike
Reckless Charge
Power Through
Immolate
Evolve
Fire Breathing
```

### Wave 4 — Dynamic values / traits / result-dependent numeric composition

```text
Body Slam
Perfected Strike
Heavy Blade
Entrench
Second Wind
Fiend Fire
```

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

### Wave 8 — Card rule modifier pipeline consumers

```text
Clash
Blood for Blood
Whirlwind
Corruption
Infernal Blade
```

Corruption = Cost/Destination modifier source，不是 Event special case。

### Wave 9 — Autonomous/repeated play

```text
Havoc
Double Tap
```

### Wave 10 — Special runtime mutation / Upgrade consumers

此 Wave **不建立 generic Upgrade Foundation**；它只实现需要额外 runtime semantics 的特殊消费者：

```text
Rampage      → per-card combat state
Armaments    → battle-time generic Upgrade action consumer
Searing Blow → repeatable-upgrade policy/state consumer
```

Clone spec 只在 mutable states 与真实 Copy consumer 同时形成需求时落地。

---

## 7. Durable review checklist for every new card

```text
1. 需要读取哪些 Fact / Query / Predicate？
2. mutable predicate/value 是否在 Execute-time 解析？
3. 创建哪些 typed intent / Spec？
4. 哪个 BattleAction 承担 mutation？
5. 哪个 runtime owner authoritative commit？
6. CommitResult 冻结哪些事实？
7. 后续是 resolution-local Continuation 还是 post-commit BattleEvent？为什么？
8. Continuation 是否 typed/authored/local/immutable/stateless？
9. Continuation batch 是否先 front-insert，再 Dispatch，使 reactions 自然在其前？
10. failure path 是否 RequestResolutionFault + Finish + return，不再 Dispatch？
11. Event 是否真的有独立 post-commit consumer？
12. Trigger 是否只读 Event/source snapshot + typed Query 并 Build Action？
13. Selection 是否只返回 SelectionResult？
14. RNG 是否只做 deterministic index/order/shuffle？
15. 是否出现具体 Card/Status/Relic identity branch？
16. primitive A 是否直接调用 primitive B internals？
17. 是否向 FCardPlayContext / FTriggerContext 塞新 subsystem？
18. 是否创建 Universal Result Bus / Universal Effective Facts / arbitrary property bag？
19. 如果创建/复制 Card，是否从统一 NextRuntimeId allocator 分配 fresh RuntimeId？
20. Gameplay/A3/A2 frozen card-face 是否读取同一个 effective card/effect view？
```

15/16/17/18 任一为“是”，默认视为架构走偏。

---

## 8. Key architectural decisions to preserve

```text
Card name / CardId 不决定组合行为
CardData immutable definition 与 runtime state 分离
ActionQueue 是 Gameplay mutation authority
DeckRuntime 是 zone truth
Modifier 在 commit 前改变 typed spec
mutable-state-dependent value/predicate 在 Execute-time 解析
BattleEvent 只描述已 commit 的事实
Trigger 只读 eligibility + Action build
Continuation 只处理当前 resolution 内 typed Result-dependent downstream build
same-commit ordering = reactions -> Continuation -> previously pending
Primitive capability 保持中立；authored composition 负责组合
Selection core 不知道 Widget
RNG 不知道领域语义
UI 不直接操作 Gameplay state
A2 只播放 committed facts
A3 只做当前状态、确定性、read-only preview
Upgrade runtime 每个 CardInstance 只有一个 authoritative state shape
Effective card boundary 是 typed card/effect view，不是万能 bag
```

---

## 9. Phase 8 relationship

Phase 8 当前 authority：`docs/Phase8ComboArchitectureDesign.md`。

Automation：

```text
transient authored Draw-2 card definition + Sundial
→ real Card / Effect / Action / Shuffle / Event / Reaction chain
```

Production evidence：

```text
existing Draw-2 Pommel Strike + Sundial PIE observation
```

两者分离，避免未来正式 Pommel Base/Upgrade authoring 破坏 Phase 8 architecture regression。

---

## 10. Planning status

```text
Inventory coverage:           75 / 75 distinct Ironclad card definitions
Architecture capabilities:    CAP-00 .. CAP-20 mapped + explicit cross-cutting contracts
Coupling rule:                ORTHOGONAL PRIMITIVES / AUTHORED COMPOSITION / CONTRACT-ONLY INTEGRATION
Continuation rule:            TYPED / LOCAL / AUTHORED / IMMUTABLE / NON-EVENT
Continuation ordering:        REACTIONS -> CONTINUATION -> PREVIOUSLY PENDING
Card Trigger source:          INDEPENDENT DESIGN SLICE / PROVIDER-BASED DISCOVERY
Card Trigger ordering:        PRIORITY -> SOURCE TIER -> SEQUENCE KEY -> LOCAL INDEX
Upgrade Foundation:           AFTER PHASE8 SEAL / BEFORE BROAD CARD WAVES
Implementation authorization: NONE
```
