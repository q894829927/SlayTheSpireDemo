# Ironclad Full Card Architecture Plan

日期：**2026-09-04**

状态：**PLANNING REFERENCE / IMPLEMENTATION NOT AUTHORIZED**

本文件把《Slay the Spire 1》Ironclad 全部卡牌拆成长期内容需求与去重后的架构能力。它不是新的实施授权，也不扩大 `docs/Phase8ComboArchitectureDesign.md` 的 Phase 8 范围。

基线口径：Ironclad 有 **72 张常规卡池卡**（20 Common / 36 Uncommon / 16 Rare），另有起始牌 `Strike / Defend / Bash` 3 个不同定义，因此本文覆盖 **75 个不同 Ironclad 卡牌定义**。

本文不锁死每张卡的具体数值；数值属于 content authoring。本文锁定的是：这张卡需要什么 Gameplay / UI / Presentation 能力，哪些能力可以复用，哪些必须形成新的通用架构。

---

## 1. Current project baseline

当前稳定架构已经有：

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

## 2. Architecture capability catalog

后续所有 Ironclad 卡牌都映射到以下能力编号。实现时应按真实消费者组合抽象，禁止为具体卡名加分支。

### CAP-00 — Existing primitive composition

无需新核心架构，只组合现有 Damage / Block / Draw / ApplyStatus / multi-hit / Energy / Destination。

### CAP-01 — Upgrade state and upgraded definition resolution

支持真正的卡牌升级，而不只是单独制作 `Plus` DataAsset。至少要解决：

```text
run-level UpgradeLevel ownership
battle materialization
combat-only temporary upgrade（Armaments）
unbounded upgrade count（Searing Blow）
所有 card text / cost / effects / A2 / A3 从同一 effective definition/value source 读取
```

Phase 8 的 Pommel Strike+ **不授权 CAP-01**；它仍只是 content variant。

### CAP-02 — Card traits + zone/deck/hand queries

通用查询，不允许通过 DisplayName / CardId 列表硬编码。

```text
CardTraits / tags
count cards by trait / type / zone
query all active combat cards
query Hand composition
```

Perfected Strike 应依赖 `Strike` trait，而不是名字包含字符串。

### CAP-03 — Dynamic numeric sources / formulas

Effect 的 base value 可以来自当前 Gameplay state，而不是永远是 authored constant。

```text
Current Block
Deck trait count
cards exhausted by this operation
per-card combat state
special modifier multiplier
```

必须仍然进入正确 typed pipeline；例如 Body Slam 计算出的 base damage 之后仍吃 Strength / Weak / Vulnerable。

### CAP-04 — Pending card selection / player choice

支持 ActionQueue 中需要玩家选择一张牌的暂停边界：

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

选择不能由 Widget 直接修改 DeckRuntime。

### CAP-05 — Generic card-zone mutation / creation / copy

补齐可组合的 authoritative Actions：

```text
create runtime card from definition
copy runtime card identity/config into new runtime instance
move Hand ↔ Draw/Discard/Exhaust
put card on DrawPile top
add card to Hand / Discard / DrawPile
bulk zone operations
```

DeckRuntime 继续拥有 zone truth。

### CAP-06 — Exhaust lifecycle + exact CardExhausted fact

Exhaust 不只是 `DefaultDestination=Exhaust`。需要统一：

```text
explicit exhaust from Hand
played card exhaust destination
automatic Ethereal exhaust
bulk exhaust
exact CardRuntimeId / CardId / source context
post-commit CardExhausted BattleEvent
```

这是 Ironclad 最重要的新增能力簇之一。

### CAP-07 — Temporal card keywords / turn-scoped rules

包括：

```text
Ethereal
Innate
this-turn temporary statuses
turn-scoped trigger charges
end/start-turn expiry
```

不能靠卡牌自己在 Tick 中维护。

### CAP-08 — HP loss / healing / MaxHP / combat counters

需要区分：

```text
Damage != Lose HP
LoseHP bypasses Block
Heal
MaxHP gain
HP-loss source attribution（from card / enemy / other）
combat-scoped HP-loss count
```

### CAP-09 — Card play rule pipeline

把目前散落的 `BaseCost / TargetType / DefaultDestination / QueryPlayability` 收敛为可扩展 read-only rule resolution：

```text
playability predicates
resolved cost
resolved destination
Draw restriction
X-cost
per-turn cost override
free-play policy
```

Corruption、Blood for Blood、Battle Trance、Clash、Whirlwind 都依赖这一层。

### CAP-10 — Autonomous / repeated card play

支持 Gameplay Action 触发另一张牌的正常 play pipeline，而不是直接调用 Effect：

```text
play top card for free
repeat next Attack
preserve source/runtime identity
avoid recursive special-case / duplicate cost spend
explicit destination policy
```

### CAP-11 — Multi-enemy combat model and target sets

当前项目只有单 `Enemy`。完整 Ironclad 需要：

```text
ordered enemy collection
AllEnemies target set
RandomEnemy target policy
single enemy target remains compatible
per-target Damage/Status commits
terminal state only after all enemies dead
Presentation participant identity for N enemies
```

这是较大的架构扩展，不能以循环当前单 Enemy 指针伪装。

### CAP-12 — Deterministic battle RNG service

Deck shuffle 已有自己的 deterministic RNG，但随机目标/随机卡牌/随机弃牌需要 battle-level deterministic random choice contract：

```text
random enemy
random Hand card
random Attack generation
random draw-pile insertion/shuffle semantics
seeded Automation reproducibility
```

### CAP-13 — Status amount transforms

除“Apply +N”外，还需要：

```text
lose Strength
multiply current Strength
set / clamp / temporary delta
exact current Status instance mutation
```

Limit Break / Disarm / Flex 不能通过伪造特殊 Status 名称绕过。

### CAP-14 — Gameplay event surface expansion

按 committed fact 增加真正有消费者的事件，候选包括：

```text
TurnStarted
CardPlayed（Gameplay event，区别于 Presentation record）
CardDrawn
CardExhausted
HPLost
BlockGained
CombatantAttacked / DamageResolved
```

仍遵守：Event = post-commit immutable fact，Trigger = read-only Action builder。

### CAP-15 — Per-card runtime combat state

`UCardInstance` 需要有限、显式的 mutable combat state 或 dedicated state component，用于 Rampage 这类“这个具体副本本场战斗永久变化”的牌。

不要提前做 arbitrary key/value bag。

### CAP-16 — Status / Curse card gameplay

虽然 enum 已有 Status / Curse，但还需要实际规则：

```text
Wound / Dazed / Burn definitions
unplayable or special-playability rules
Status/Curse draw detection
Ethereal Status（如 Dazed）
end-turn Burn behavior if needed
created Status card runtime identity
```

### CAP-17 — Block lifecycle/rule modifiers

支持 Block 本身的生命周期规则，而不只是 GainBlock amount：

```text
retain Block across turn start（Barricade）
explicit current Block transform（Entrench 可复用 CAP-03）
```

### CAP-18 — Damage outcome / Fatal / aggregation

Action 需要能可靠得到 committed outcome：

```text
actual HP loss after Block
whether target died from this hit
aggregate damage across multiple targets
Fatal follow-up
```

Feed / Reaper 依赖，不能通过 play 后重新猜测 HP 差值。

### CAP-19 — Enemy intent query

Gameplay-facing、read-only、authoritative enemy intent query，用于 Spot Weakness。不能读取 HUD 文本或 Presentation intent。

### CAP-20 — Card catalog / generation pool

随机生成 Attack 等能力需要明确的 content registry / pool query：

```text
query eligible card definitions
rarity/type/color filters
battle RNG pick
create temporary runtime instance
```

Infernal Blade 是首个真实消费者。

---

## 3. Full Ironclad card inventory → capability mapping

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

## 4. Capability pressure / how many cards need each area

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
CAP-01 True upgrade ownership               =  Armaments + Searing Blow as direct consumers
CAP-10 Autonomous/repeat play               =  Havoc + Double Tap
CAP-18 Fatal/damage aggregation              =  Feed + Reaper
CAP-15 Per-card combat state                 =  Rampage
CAP-17 Block retention                       =  Barricade
CAP-19 Enemy intent query                    =  Spot Weakness
CAP-20 Random card generation pool           =  Infernal Blade
```

最明显的结论：**Exhaust + expanded committed events 是 Ironclad 最大的下一代架构中心。** Corruption 不是一个孤立特例；它依赖的 Exhaust/CardPlayed rule surface 会同时服务大量卡牌。

---

## 5. Recommended architecture dependency order

不要按 rarity 或卡牌名字逐张实现。应按能力簇推进，每个能力至少用两个真实消费者验证后再继续抽象。

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

### Wave 1 — Exhaust foundation

优先消费者：

```text
Impervious
Pummel
Seeing Red
Shockwave
Burning Pact
Feel No Pain
Dark Embrace
```

应一次设计：

```text
ExhaustCardAction / bulk exhaust
CardExhausted committed event
exact CardRuntimeId payload
played-card Exhaust destination
Trigger reactions from CardExhausted
```

Feel No Pain + Dark Embrace 必须一起作为通用性证明。

### Wave 2 — Selection + generic zone operations

优先消费者：

```text
Burning Pact
Headbutt
Warcry
Dual Wield
Exhume
True Grit
```

建立正式 pending choice，而不是 UI callback 直接修改 Gameplay。

### Wave 3 — Status/Curse cards + card creation

优先消费者：

```text
Wild Strike
Reckless Charge
Power Through
Immolate
Evolve
Fire Breathing
```

这一步同时验证 CardDrawn event、Status/Curse runtime identity 和 created card Presentation。

### Wave 4 — Dynamic values / traits

建议顺序：

```text
Body Slam
Perfected Strike
Heavy Blade
Entrench
Second Wind
Fiend Fire
```

先 Body Slam + Perfected Strike 形成第二个真实 dynamic-value consumer，再决定是否抽 `CardNumericSource` 层。

### Wave 5 — Turn-scoped powers/events

消费者：

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

补 `TurnStarted` 及 this-turn lifecycle。不要用 Actor Tick 实现。

### Wave 6 — HP-loss and outcome semantics

消费者：

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

明确 `LoseHP != Damage`，并给 committed outcome 提供 Fatal / actual HP loss。

### Wave 7 — Multi-enemy + deterministic RNG

消费者：

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

这是从单 Enemy demo 走向完整 combat encounter 的大边界，应独立设计，不夹在某张卡的小提交里。

### Wave 8 — Card rule modifier pipeline

消费者：

```text
Clash
Battle Trance
Blood for Blood
Whirlwind
Corruption
Infernal Blade
```

建立 typed card-play rule specs；Corruption 用正式 Cost/Destination rule modifier，而不是 `HasCorruption` 特判。

### Wave 9 — Autonomous/repeated play

```text
Havoc
Double Tap
```

两张牌一起证明“自动打牌”不是 Havoc 专属 Action。

### Wave 10 — Runtime mutation / true upgrades

```text
Rampage      → per-card combat state
Armaments    → temporary combat upgrade
Searing Blow → unbounded persistent upgrade count
```

这一层应等 Run Deck ownership 明确后再做。Searing Blow 是检验 upgrade model 是否真正正确的终极消费者。

---

## 6. Key architectural decisions to preserve

未来实现全部 75 张时，仍必须保持：

```text
Card name / CardId 不决定组合行为
CardData immutable definition 与 runtime state 分离
ActionQueue 是 Gameplay mutation authority
DeckRuntime 是 zone truth
Modifier 在 commit 前改变 typed spec
BattleEvent 只描述已 commit 的事实
Trigger 只读 eligibility + Action build
UI 不直接操作 Gameplay state
A2 只播放 committed facts
A3 只做当前状态、确定性、read-only preview
```

禁止形成：

```cpp
if (CardId == "Corruption")
if (CardId == "PerfectedStrike")
if (CardId == "FeelNoPain")
if (DisplayName.Contains("Strike"))
```

正确方向应是：

```text
Traits / typed specs / modifiers / events / triggers / zone actions
→ content composition
→ individual card emerges from data
```

---

## 7. Phase 8 relationship

本规划不会把 Phase 8 扩成“实现所有 Ironclad 卡”。

Phase 8 仍保持：

```text
Pommel Strike+ + Sundial
→ Card → Draw → Shuffle → Event → Relic Reaction → Energy
→ Combo Architecture Validation
```

Phase 8 通过后，再从本文选择下一个**bounded capability wave**。建议优先进入 Exhaust foundation，而不是直接跳 Corruption；因为 Corruption 同时依赖 CardExhausted、CardPlayed、Cost rule、Destination rule，直接做会一次打开太多边界。

---

## 8. Planning status

```text
Inventory coverage:          75 / 75 distinct Ironclad card definitions
Architecture capabilities:   CAP-00 .. CAP-20 mapped
Implementation authorization: NONE
Phase 8 scope:               unchanged
```

下一步如果继续设计，应针对单个 Wave 创建 dedicated design doc；不要把本文件当成一次性实现清单。
