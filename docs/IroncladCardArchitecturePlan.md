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
```

---

## 2. Locked architecture rule — capabilities are orthogonal and composable

后续 Ironclad 扩展必须遵守以下长期规则：

> **能力之间可以通过稳定的数据合同组合，但不能彼此知道具体实现，也不能知道具体卡牌、Status、Relic 或组合消费者。**

设计目标不是：

```text
Capability A
→ knows Capability B
→ knows Capability C
```

而是：

```text
Query / Spec / SelectionResult / CommitResult / BattleEvent
= shared neutral contracts

independent capability producer
→ neutral contract
→ independent capability consumer
```

### 2.1 Allowed dependency directions

允许的组合只能沿现有架构方向发生：

```text
Read-only Query
→ returns fact/value only

CardEffect / Rule Source
→ builds or modifies typed intent/spec

BattleAction
→ requests authoritative mutation

Runtime owner
→ commits mutation
→ returns exact CommitResult

Commit
→ emits immutable BattleEvent when such a fact has real consumers

Trigger
→ reads Event + source snapshot
→ builds reaction BattleActions

Selection
→ returns SelectionResult
→ does not know what the caller will do with that result

RNG
→ returns deterministic choice
→ does not know card semantics
```

### 2.2 Forbidden capability coupling

禁止形成以下关系：

```text
Selection system knows Exhaust
Exhaust system knows Feel No Pain / Dark Embrace
CardExhausted event knows its listeners
Draw system knows Evolve / Battle Trance
Card Rule Pipeline knows Corruption / Clash
Dynamic Value system knows Body Slam / Perfected Strike
Deck Query knows Damage
RNG knows Sword Boomerang / Infernal Blade
Multi-enemy system knows Cleave / Whirlwind
HP-loss system knows Rupture / Blood for Blood
Upgrade system knows Armaments / Searing Blow special cases
```

同样禁止：

```cpp
if (CardId == "Corruption")
if (CardId == "PerfectedStrike")
if (CardId == "FeelNoPain")
if (RelicId == "Sundial" && CardId == "PommelStrike")
if (DisplayName.Contains("Strike"))
```

### 2.3 Card is the composition root, not the capability implementation

具体卡牌只负责组合独立能力。

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
→ Dynamic Numeric Source
→ Damage Spec
→ existing Damage Modifier Pipeline
→ Damage commit
```

DeckQuery 只回答查询；Dynamic Numeric Source 只计算数值；Damage Pipeline 不知道数值来自 DeckQuery。

例如 Burning Pact：

```text
CardSelectionRequest
→ SelectionResult
→ ExhaustCardAction
→ CardExhaustedEvent
→ DrawCardsAction
```

Selection 系统不应该出现 `SelectCardForExhaust`、`SelectCardForHeadbutt` 等具体能力 API。

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
→ directly calls Capability B
→ directly calls Capability C
```

---

## 3. Architecture capability catalog

后续所有 Ironclad 卡牌都映射到以下能力编号。能力编号表示正交能力域，不表示彼此之间的直接依赖关系。

### CAP-00 — Existing primitive composition

无需新核心架构，只组合现有 Damage / Block / Draw / ApplyStatus / multi-hit / Energy / Destination。

### CAP-01 — Upgrade state and upgraded definition resolution

支持真正的卡牌升级，而不只是单独制作 `Plus` DataAsset：

```text
run-level UpgradeLevel ownership
battle materialization
combat-only temporary upgrade（Armaments）
unbounded upgrade count（Searing Blow）
所有 card text / cost / effects / A2 / A3 从同一 effective definition/value source 读取
```

Phase 8 的 Pommel Strike+ **不授权 CAP-01**；它仍只是 content variant。

### CAP-02 — Card traits + zone/deck/hand queries

```text
CardTraits / tags
count cards by trait / type / zone
query all active combat cards
query Hand composition
```

Perfected Strike 使用 `Strike` trait，不使用名字字符串匹配。

### CAP-03 — Dynamic numeric sources / formulas

Effect base value 可以读取 Gameplay state：

```text
Current Block
Deck trait count
cards exhausted by this operation
per-card combat state
special modifier multiplier
```

计算结果仍必须进入对应 typed pipeline。

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
copy runtime card config into new runtime instance
move Hand ↔ Draw/Discard/Exhaust
put card on DrawPile top
add card to Hand / Discard / DrawPile
bulk zone operations
```

DeckRuntime 继续拥有 zone truth。

### CAP-06 — Exhaust lifecycle + exact CardExhausted fact

```text
explicit exhaust from Hand
played card exhaust destination
automatic Ethereal exhaust
bulk exhaust
exact CardRuntimeId / CardId / source context
post-commit CardExhausted BattleEvent
```

Exhaust 只负责 mutation + committed fact，不知道任何 Exhaust reactive Power。

### CAP-07 — Temporal card keywords / turn-scoped rules

```text
Ethereal
Innate
this-turn temporary statuses
turn-scoped trigger charges
end/start-turn expiry
```

不能靠具体卡 Tick 维护。

### CAP-08 — HP loss / healing / MaxHP / combat counters

```text
Damage != Lose HP
LoseHP bypasses Block
Heal
MaxHP gain
HP-loss source attribution
combat-scoped HP-loss count
```

### CAP-09 — Card play rule pipeline

把 `BaseCost / TargetType / DefaultDestination / QueryPlayability` 解析为 typed read-only card-play rules：

```text
playability predicates
resolved cost
resolved destination
Draw restriction
X-cost
per-turn cost override
free-play policy
```

Pipeline 不知道 Corruption、Clash、Whirlwind 等名字；这些只是 modifier consumers/sources。

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
RandomEnemy target policy
single enemy target remains compatible
per-target Damage/Status commits
terminal state only after all enemies dead
Presentation participant identity for N enemies
```

### CAP-12 — Deterministic battle RNG service

```text
random enemy
random Hand card
random Attack generation
random draw-pile insertion semantics
seeded Automation reproducibility
```

RNG 只做随机选择，不理解消费者语义。

### CAP-13 — Status amount transforms

```text
lose Strength
multiply current Strength
set / clamp / temporary delta
exact current Status instance mutation
```

### CAP-14 — Gameplay event surface expansion

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

### CAP-15 — Per-card runtime combat state

为 Rampage 等具体副本提供有限、显式 mutable combat state 或 dedicated state component。禁止 arbitrary key/value bag。

### CAP-16 — Status / Curse card gameplay

```text
Wound / Dazed / Burn definitions
unplayable or special-playability rules
Status/Curse draw detection
Ethereal Status
end-turn Burn behavior
created Status card runtime identity
```

### CAP-17 — Block lifecycle/rule modifiers

```text
retain Block across turn start（Barricade）
current Block transform
```

### CAP-18 — Damage outcome / Fatal / aggregation

```text
actual HP loss after Block
whether target died from this hit
aggregate damage across multiple targets
Fatal follow-up
```

### CAP-19 — Enemy intent query

Gameplay-facing、read-only、authoritative enemy intent query。不得读取 HUD 或 Presentation intent。

### CAP-20 — Card catalog / generation pool

```text
query eligible card definitions
rarity/type/color filters
battle RNG pick
create temporary runtime instance
```

Card Catalog 只返回候选 Definition，不负责 RNG，不负责创建 runtime card。

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
| Battle Trance | 大量 Draw + 本回合禁止继续 Draw | CAP-07, CAP-09 |
| Blood for Blood | 本场每次失去 HP 后费用下降 | CAP-08, CAP-09, CAP-14 |
| Bloodletting | Lose HP → Gain Energy | CAP-08 |
| Burning Pact | 选择手牌 Exhaust → Draw | CAP-04, CAP-06 |
| Carnage | Ethereal + 高伤害 | CAP-06, CAP-07 |
| Combust | 回合结束 Lose HP + 全体伤害 | CAP-07, CAP-08, CAP-11, CAP-14 |
| Dark Embrace | 每次 CardExhausted → Draw | CAP-06, CAP-14 |
| Disarm | 降低敌人 Strength，自身 Exhaust | CAP-06, CAP-13 |
| Dropkick | 攻击；若目标 Vulnerable 则 Gain Energy + Draw | CAP-09 |
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
| Second Wind | Exhaust 手牌中所有非 Attack；按数量 Gain Block | CAP-02, CAP-03, CAP-06 |
| Seeing Red | Gain Energy，自身 Exhaust | CAP-06 |
| Sentinel | Block；若此具体副本被 Exhaust 则 Gain Energy | CAP-06, CAP-14 |
| Sever Soul | Exhaust 所有非 Attack 手牌 + Damage | CAP-02, CAP-06 |
| Shockwave | 全体 Weak + Vulnerable，自身 Exhaust | CAP-06, CAP-11 |
| Spot Weakness | 若目标 Intent 为 Attack，则 Gain Strength | CAP-19 |
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
| Feed | Damage；若该 hit Fatal，则永久提高 MaxHP；自身 Exhaust | CAP-06, CAP-08, CAP-18 |
| Fiend Fire | Exhaust Hand；按成功 Exhaust 数量造成重复/比例伤害 | CAP-02, CAP-03, CAP-06 |
| Immolate | 全体高伤害 + 创建 Burn 到 Discard | CAP-05, CAP-11, CAP-16 |
| Impervious | 高 Block，自身 Exhaust | CAP-06 |
| Juggernaut | 每次 Gain Block → 对随机敌人伤害 | CAP-11, CAP-12, CAP-14 |
| Limit Break | 当前 Strength 翻倍；基础版 Exhaust | CAP-06, CAP-13 |
| Offering | Lose HP + Gain Energy + Draw，自身 Exhaust | CAP-06, CAP-08 |
| Reaper | 全体伤害；按实际未被 Block 的 HP damage 总量 Heal；自身 Exhaust | CAP-06, CAP-08, CAP-11, CAP-18 |

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
CAP-09 Card play rule pipeline              ≈  8 cards
CAP-04 Player card selection                ≈  7 cards
CAP-03 Dynamic values/formulas              ≈  7 cards
CAP-12 Battle RNG                           ≈  6 cards
CAP-16 Status/Curse card gameplay           ≈  6 cards
CAP-02 Traits / deck-hand query             ≈  5 cards
CAP-13 Status amount transforms             ≈  3 cards
CAP-01 True upgrade ownership               = Armaments + Searing Blow as direct consumers
CAP-10 Autonomous/repeat play               = Havoc + Double Tap
CAP-18 Fatal/damage aggregation              = Feed + Reaper
CAP-15 Per-card combat state                 = Rampage
CAP-17 Block retention                       = Barricade
CAP-19 Enemy intent query                    = Spot Weakness
CAP-20 Random card generation pool           = Infernal Blade
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
Pommel Strike / Pommel Strike+
Shrug It Off
Twin Strike
Inflame
Uppercut
Bludgeon
```

目标：不新增核心架构，证明 current composition 稳定。Phase 8 的 `Pommel Strike+ + Sundial` 仍属于这一层。

### Wave 1 — Exhaust fact surface

验证消费者：

```text
Impervious
Pummel
Seeing Red
Shockwave
Burning Pact
Feel No Pain
Dark Embrace
```

需要分别设计并独立测试：

```text
Exhaust mutation
CardExhausted CommitResult / BattleEvent
CardExhausted Trigger consumer
```

Feel No Pain + Dark Embrace 用来验证同一个 neutral Event 可以被多个独立 Trigger 消费；它们不得形成彼此依赖。

### Wave 2 — Selection + generic zone operations

验证消费者：

```text
Burning Pact
Headbutt
Warcry
Dual Wield
Exhume
True Grit
```

Selection 与 Zone Mutation 是两个独立能力域。Selection 只产生结果，具体卡牌 orchestration 决定把结果交给 Exhaust / Move / Copy 中的哪一个操作。

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

### Wave 4 — Dynamic values / traits

```text
Body Slam
Perfected Strike
Heavy Blade
Entrench
Second Wind
Fiend Fire
```

Body Slam 与 Perfected Strike 只是两个 Dynamic Numeric Source 消费者。是否抽象 `CardNumericSource` 必须由第二个真实消费者证明，而不是让 Dynamic Value 系统直接依赖 DeckQuery 或 Block internals。

### Wave 5 — Turn-scoped powers/events

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

Turn lifecycle 只产生 neutral turn facts / expiry boundary；具体 Status/Power 通过 Trigger 或 modifier contract 消费。

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

明确 `LoseHP != Damage`。HP mutation、HPLost Event、Fatal outcome 分别保持独立职责。

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

Multi-enemy target query 与 RNG 是独立能力。RandomEnemy = ordered candidate query + deterministic RNG choice，而不是 RNG 直接访问 BattleManager enemy internals。

### Wave 8 — Card rule modifier pipeline

```text
Clash
Battle Trance
Blood for Blood
Whirlwind
Corruption
Infernal Blade
```

建立 typed CardPlaySpec / Modifier contract。Corruption 只是 Cost/Destination modifier source，不得成为 Pipeline 内部特判。

### Wave 9 — Autonomous/repeated play

```text
Havoc
Double Tap
```

两张牌共同验证 generic autonomous/repeated card play contract；该能力只重新提交 normal play intent，不知道具体后续 Effects。

### Wave 10 — Runtime mutation / true upgrades

```text
Rampage      → per-card combat state
Armaments    → temporary combat upgrade
Searing Blow → unbounded persistent upgrade count
```

Per-card state 与 Upgrade ownership 仍是两个独立能力域。它们可以同时存在于有效卡牌解析中，但不得合并成 arbitrary mutable property bag。

---

## 7. Durable review checklist for every new card

每增加一张新卡，先问：

```text
1. 这张卡需要读取哪些 Fact / Query？
2. 它创建哪些 typed intent / Spec？
3. 哪个 BattleAction 承担 mutation？
4. 哪个 runtime owner 承担 authoritative commit？
5. CommitResult 需要冻结哪些事实？
6. 是否真的存在 post-commit Event 消费者？
7. Trigger 是否只读 Event 并 Build Action？
8. 是否需要 Selection？Selection 是否只返回结果？
9. 是否需要 RNG？RNG 是否只做 deterministic choice？
10. 是否出现任意具体 Card/Status/Relic identity branch？
11. 是否让一个能力直接调用了另一个能力的内部实现？
12. integration 是否可以只通过公共 typed contract 完成？
```

如果第 10 或第 11 项为“是”，默认视为架构走偏，需要重新设计。

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
能力域之间只通过 neutral typed contracts 组合
UI 不直接操作 Gameplay state
A2 只播放 committed facts
A3 只做当前状态、确定性、read-only preview
```

正确方向：

```text
Traits
Queries
Typed Specs
SelectionResult
CommitResult
Modifier Pipelines
BattleEvents
Triggers
Zone Actions
Deterministic RNG

→ orthogonal capability composition
→ individual card behavior emerges from content/orchestration
```

---

## 9. Phase 8 relationship

本规划不会把 Phase 8 扩成“实现所有 Ironclad 卡”。

Phase 8 仍保持：

```text
Pommel Strike+ + Sundial
→ Card → Draw → Shuffle → Event → Relic Reaction → Energy
→ Combo Architecture Validation
```

这个组合本身就是能力解耦的样板：Pommel Strike+ 不知道 Sundial，Draw 不知道 Relic，Shuffle 只提交事实，Sundial 只消费 `DeckShuffled` fact。

Phase 8 通过后，再从本文选择一个 bounded capability goal。后续选择 Wave 只决定实施顺序，不构成能力之间的语义依赖授权。

---

## 10. Planning status

```text
Inventory coverage:          75 / 75 distinct Ironclad card definitions
Architecture capabilities:   CAP-00 .. CAP-20 mapped
Coupling rule:               ORTHOGONAL / CONTRACT-ONLY COMPOSITION
Implementation authorization: NONE
Phase 8 scope:               unchanged
```
