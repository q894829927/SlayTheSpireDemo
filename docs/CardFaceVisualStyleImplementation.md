# Card Face Visual Style 卡牌视觉样式实施计划

日期：**2026-09-05**

状态：

```text
DESIGN LOCKED
IMPLEMENTATION NOT AUTHORIZED
```

本文件是 Card Face Visual Style（CFV）专项改造的 dedicated authority。

本阶段只负责：

```text
卡牌语义 Rarity
→ Frozen Presentation 传播
→ Native Card Widget 静态视觉样式解析
→ 生产 WBP 卡面素材配置
```

不得重新打开已 seal 的 Gameplay / BattleAction / Presentation Timeline / A3 Preview / Card Upgrade ownership 边界。

---

## 1. 目标

在不改变现有 Gameplay 架构的前提下，使卡牌根据 `CardType`、`CardRarity` 和当前固定 Ironclad/Red 视觉主题显示对应的：

```text
背景
边框
标题丝带
类型标签底板
费用宝石
阴影
```

并保持升级表现与稀有度/类型视觉完全正交。

最终视觉规则：

```text
CardType
→ Background

CardType + semantic Rarity
→ rarity-specific Frame

semantic Rarity
→ Visual Rarity Style
→ Banner
→ TypePlate

fixed Ironclad theme
→ red background family
→ card_red_orb
→ card shadow

bUpgraded
→ DisplayName + "+"
→ upgraded title color #7FFF00
```

---

## 2. 设计参考与正交维度

参考《Slay the Spire》公开 Mod/API 生态中对卡牌元数据和卡面资源的组织方式，本项目保持以下维度正交：

```text
CardType
CardRarity
CardColor / Theme
Upgrade State
```

当前项目只有 Ironclad 风格，因此本阶段：

```text
不新增 CardColor
不新增 CharacterTheme
不建立多角色 Visual Registry
```

本轮固定使用 Red / Ironclad Theme；未来出现第二个真实角色 consumer 后，再单独设计 CardColor/Theme。

---

## 3. 当前项目基线

当前 `ECardType`：

```text
Attack
Skill
Power
Status
Curse
```

当前 `UCardData` 已有：

```text
CardId
DisplayName
Description
CardArt
CardType
TargetType
BaseCost
UpgradedCost
DefaultDestination
Effects[]
```

当前 `UCardInstance` 保持 definition/runtime split：

```text
Definition
RuntimeId
bUpgraded
```

静态元数据通过 Definition getter 读取，不在 runtime instance 复制第二份 authoritative value。

当前 Frozen Presentation 存在两条正式路径：

```text
Current / Formal Hand
FCardReadView
→ ABattleManager::TryFreezePresentationStateSnapshot
→ FBattleHUDCardView

Committed / Historical Card
UCardInstance
→ FPresentationCardSnapshot
→ PresentationCardView::MakePresentationOnlyCardView
→ FBattleHUDCardView
```

CFV 新增 Rarity 必须同时进入两条路径。

---

## 4. 语义 Rarity Contract

### 4.1 ECardRarity

新增：

```cpp
UENUM(BlueprintType)
enum class ECardRarity : uint8
{
    Basic,
    Common,
    Uncommon,
    Rare,
    Special,
    Curse
};
```

`ECardRarity` 是长期语义 metadata，不是 UI skin enum。

示例：

```text
Strike
CardType = Attack
Rarity   = Basic

Pommel Strike
CardType = Attack
Rarity   = Common

Uppercut
CardType = Attack
Rarity   = Uncommon
```

禁止从 `CardType` 自动推导 `Rarity`，也禁止从 `Rarity` 自动改写 `CardType`。

例如：

```text
CardType = Curse
Rarity   = Curse
```

应由资产显式 author，而不是代码隐式强制。

### 4.2 迁移默认值

新增字段默认：

```cpp
ECardRarity Rarity = ECardRarity::Common;
```

该默认值是：

```text
serialized / backward-compatible migration default
```

不是：

```text
所有旧卡语义上都属于 Common
```

这样现有 `.uasset` 和未显式设置 Rarity 的 transient Automation fixture 在新增字段后继续稳定加载。

CFV-4 再显式 author 生产卡的真实 Rarity。

---

## 5. UCardData / UCardInstance 边界

`UCardData` 增加：

```cpp
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card|Identity")
ECardRarity Rarity = ECardRarity::Common;
```

`UCardInstance` 只增加：

```cpp
ECardRarity GetRarity() const;
```

语义：

```text
valid Definition
→ Definition->Rarity

invalid Definition
→ Common defensive fallback
```

禁止在 `UCardInstance` 再增加一份 mutable/serialized `Rarity`。

---

## 6. CardData Editor Validation

`ECardRarity` 作为 authored semantic metadata，必须进入 `UCardData::IsDataValid()`。

形成三层防御：

```text
Layer 1
UCardData::IsDataValid
→ authoring-time enum validation

Layer 2
FPresentationCardSnapshot validation
→ frozen payload validation

Layer 3
Visual Resolver
→ defensive fallback only
```

合法 Rarity：

```text
Basic
Common
Uncommon
Rare
Special
Curse
```

非法 enum payload 必须在 CardData Editor validation 阶段报错。

Resolver fallback 不能替代 authoring validation。

---

## 7. Frozen Presentation Rarity 数据链

新增：

```cpp
FPresentationCardSnapshot::Rarity = ECardRarity::Common;
FBattleHUDCardView::Rarity = ECardRarity::Common;
```

完整链路：

```text
UCardData::Rarity
        ↓
UCardInstance::GetRarity()
        │
        ├─ Current / Formal Hand
        │      ↓
        │ ABattleManager::TryFreezePresentationStateSnapshot
        │      ↓
        │ FBattleHUDCardView::Rarity
        │
        └─ Historical / Committed Presentation
               ↓
        PresentationCardSnapshot::TryBuild
               ↓
        FPresentationCardSnapshot::Rarity
               ↓
        PresentationCardView::MakePresentationOnlyCardView
               ↓
        FBattleHUDCardView::Rarity
```

本阶段不向 `FCardReadView` 增加 Rarity；formal Hand freeze 继续通过 `Source.Card` / `UCardInstance` 读取静态 metadata。

---

## 8. Snapshot Validation 与 Continuity Contract

### 8.1 enum domain validation

以下函数必须增加 Rarity enum domain 检查：

```text
IsNativeCardSnapshotValid
IsDiagnosticCardSnapshotValid
```

非法 Rarity 必须 fail closed。

### 8.2 exact card-face continuity

以下 comparison 必须增加：

```cpp
View.bUpgraded == Snapshot.bUpgraded
View.Rarity == Snapshot.Rarity
```

涉及：

```text
DoesNativeCardViewMatchSnapshot
DoesDiagnosticCardViewMatchSnapshot
```

原因：

```text
bUpgraded
→ 已是正式 frozen presentation fact
→ 决定卡牌名称 "+" 与标题颜色

Rarity
→ 新增正式 frozen presentation fact
→ 决定卡面静态视觉
```

### 8.3 RichDescription 明确排除

generic Hand identity / continuity comparison **不得加入 `RichDescription`**。

理由：

```text
CardPlayed target-specific committed RichDescription
可能合法地不同于 source-side Hand baseline
```

因此合同锁定：

```text
RichDescription
→ mapper 必须完整传播
→ generic Hand identity comparison 故意不比较
```

禁止实现者因“exact continuity”而自行把 `RichDescription` 纳入 equality。

---

## 9. Semantic Rarity → Visual Rarity Style

项目当前只使用三套可见 rarity style：

```text
CommonVisual
UncommonVisual
RareVisual
```

语义 Rarity 映射：

```text
Basic
Common
Special
Curse
→ CommonVisual

Uncommon
→ UncommonVisual

Rare
→ RareVisual
```

禁止在 `UCardData` 增加：

```text
VisualRarity
BannerTexture
FrameTexture
BackgroundTexture
```

视觉折叠只属于 Presentation Resolver。

---

## 10. 卡牌类型视觉规则

### 10.1 Background

```text
Attack
→ bg_attack_red

Skill
→ bg_skill_red

Power
→ bg_power_red

Status / Curse
→ FallbackBackground
```

### 10.2 Frame

```text
Attack
→ resolved rarity style.AttackFrame

Skill
→ resolved rarity style.SkillFrame

Power
→ resolved rarity style.PowerFrame

Status / Curse
→ FallbackFrame
```

`FallbackFrame` 是 Presentation 配置槽，不是 Gameplay 概念。

当前允许：

```text
FallbackFrame == nullptr
→ Img_CardFrame Hidden
→ Gameplay/Input unaffected
```

后续加入 Status / Curse 通用边框素材后：

```text
只配置 WBP Class Defaults
→ 不修改 resolver 规则
```

### 10.3 Banner / TypePlate

Banner 与 TypePlate 继续按 resolved visual rarity style：

```text
CommonVisual
→ banner_common
→ common_left / common_center / common_right

UncommonVisual
→ banner_uncommon
→ uncommon_left / uncommon_center / uncommon_right

RareVisual
→ banner_rare
→ rare_left / rare_center / rare_right
```

Status / Curse 即使使用 fallback background/frame，也仍允许按 Rarity 显示对应 Banner / TypePlate。

---

## 11. Visual Style 数据结构

本阶段不建立 DataAsset / Registry。

使用轻量 Presentation struct：

```cpp
USTRUCT(BlueprintType)
struct FCardRarityVisualStyle
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TObjectPtr<UTexture2D> Banner = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TObjectPtr<UTexture2D> AttackFrame = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TObjectPtr<UTexture2D> SkillFrame = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TObjectPtr<UTexture2D> PowerFrame = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TObjectPtr<UTexture2D> TypeLeft = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TObjectPtr<UTexture2D> TypeCenter = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TObjectPtr<UTexture2D> TypeRight = nullptr;
};
```

由 `UBattleCardWidget` / `WBP_BattleCard_Native` CDO 保存：

```text
CommonStyle
UncommonStyle
RareStyle

AttackBackground
SkillBackground
PowerBackground
FallbackBackground
FallbackFrame
CostOrb
CardShadow
```

外层 CDO style/config 属性使用 `EditDefaultsOnly`。

---

## 12. Resolver 必须与 Widget 生命周期解耦

视觉计算不能依赖：

```text
NativeOnInitialized
NativeConstruct
真实 WBP 加载
真实 Texture asset 加载
```

推荐纯 Presentation resolver：

```cpp
FResolvedCardFaceStyle ResolveCardFaceStyle(
    ECardType CardType,
    ECardRarity Rarity,
    const FCardFaceStyleConfig& Config);
```

概念输出：

```text
FResolvedCardFaceStyle
├─ Background
├─ Frame
├─ Banner
├─ TypeLeft
├─ TypeCenter
├─ TypeRight
├─ CostOrb
└─ Shadow
```

`UBattleCardWidget::RefreshVisualStyle()` 只负责把 resolver 结果应用到 `UImage`。

Automation 可使用 transient `UTexture2D` pointer identity probe 测试，不依赖 WBP / NativeConstruct。

禁止：

```text
CardId branch
DisplayName branch
LoadObject("/Game/...")
字符串拼 asset path
Universal Visual Registry
```

---

## 13. Core Surface 与 Decorative Surface

### 13.1 Core fail-closed surface

继续保持现有核心绑定：

```text
Btn_Card
Txt_CardName
Txt_Cost
Txt_CardDescription
Txt_CardType
Img_CardArt
```

缺失核心控件：

```text
invalid Native card surface
→ input disabled
```

### 13.2 Decorative presentation surface

新增：

```text
Img_CardShadow
Img_CardBackground
Img_CardFrame
Img_CardBanner
Img_TypeLeft
Img_TypeCenter
Img_TypeRight
Img_CostOrb
```

这些控件在生产 `WBP_BattleCard_Native` 中应完整配置，但：

```text
缺失 decorative control
→ visual degradation/fallback
→ 不得改变 Gameplay/Input authority

缺失 style texture
→ visual fallback/hide
→ gameplay/input unaffected
```

禁止：

```text
missing banner/frame texture
→ card cannot click
```

---

## 14. Visual Fallback Contract

视觉资源缺失与 UMG 核心绑定缺失必须区分。

规则：

```text
missing Background texture
→ FallbackBackground

missing rarity Frame
→ FallbackFrame

missing FallbackFrame
→ Img_CardFrame Hidden

missing Banner
→ Img_CardBanner Hidden

missing TypePlate piece
→ corresponding decorative piece hidden/fallback

missing CostOrb / Shadow
→ corresponding decorative image hidden
```

任何 style texture 缺失都不得阻止 card input 或 Gameplay request。

---

## 15. WBP_BattleCard_Native 目标层级

建议结构：

```text
SB_Card
└─ Btn_Card
   └─ OV_Card
      ├─ Img_CardShadow
      │
      └─ CN_CardFace
         ├─ Img_CardBackground
         ├─ Img_CardArt
         ├─ Img_CardFrame
         │
         ├─ OV_CardName
         │  ├─ Img_CardBanner
         │  └─ SB_CardName
         │     └─ Txt_CardName
         │
         ├─ OV_CardType
         │  ├─ HB_CardTypePlate
         │  │  ├─ Img_TypeLeft
         │  │  ├─ Img_TypeCenter
         │  │  └─ Img_TypeRight
         │  └─ Txt_CardType
         │
         ├─ OV_Cost
         │  ├─ Img_CostOrb
         │  └─ Txt_Cost
         │
         └─ Txt_CardDescription
```

卡面当前约为：

```text
150 × 210
```

卡面主体建议使用 `CanvasPanel` 精确定位。

类型标签：

```text
HB_CardTypePlate
├─ Img_TypeLeft   → Auto
├─ Img_TypeCenter → Fill
└─ Img_TypeRight  → Auto
```

---

## 16. UMG 迁移约束

现有 Native binding 名称必须保留：

```text
Btn_Card
Txt_CardName
Txt_Cost
Txt_CardDescription
Txt_CardType
Img_CardArt
```

CFV-2 重排层级时：

```text
优先移动已有 Widget
而不是删除后重新创建同名 Widget
```

目的：减少 BindWidget、Blueprint GUID、序列化层面的无意义变化。

旧：

```text
BG_Card
VB_CardContent
```

新路径验证前不得立即删除。

推荐：

```text
建立新视觉层
→ 接入并验证
→ Collapse 旧 fallback
→ 再验证
→ 最后删除旧层
```

---

## 17. RefreshFromCardView 分层

为了保护已有 transient Card Probe 和 A3 测试，核心内容刷新与视觉刷新必须解耦。

推荐：

```text
RefreshFromCardView()
├─ RefreshCoreCardContent()
│  ├─ Name
│  ├─ Cost
│  ├─ Description
│  ├─ Type
│  └─ Art
│
└─ RefreshVisualStyle()
   ├─ Background
   ├─ Frame
   ├─ Banner
   ├─ TypePlate
   ├─ Orb
   └─ Shadow
```

旧测试 probe 继续只需要注入现有核心 surface。

不得要求旧 DTO/A3 测试为了 CFV 创建 Shadow/Banner/TypePlate 等假的 `UImage`。

生产 `WBP_BattleCard_Native` 则应完整提供 decorative surface。

---

## 18. Upgrade 与 CFV 完全正交

已 seal 的升级表现保持：

```text
bUpgraded == false
→ authored DisplayName
→ Designer/default title color

bUpgraded == true
→ DisplayName + "+"
→ #7FFF00
```

CFV 不修改：

```text
UCardInstance::bUpgraded semantics
Upgrade action
typed Base/Upgraded values
Dynamic Text
A3 Preview gameplay semantics
```

最终组合：

```text
CardType
→ Background

CardType + Rarity
→ Frame

Rarity
→ Banner / TypePlate

bUpgraded
→ "+" / #7FFF00

CardArt
→ Artwork

Cost
→ Cost text

Description
→ RichText
```

---

## 19. 字体策略

统一以下文字的中文字体：

```text
Txt_CardName
Txt_CardType
Txt_Cost
Txt_CardDescription
```

调整：

```text
Font Family
字号
Letter Spacing
Outline Size
Outline Color
Horizontal Alignment
Vertical Alignment
```

`Txt_CardDescription` 是 `URichTextBlock`，因此需同步确认：

```text
Default RichText Style
PreviewIncrease
PreviewDecrease
现有其他 semantic value styles
```

使用相同字体族和合理字号/基线。

本阶段不修改 `BattleTextResolver` 的语义解析。

---

## 20. 生产 CardData Rarity 迁移

CFV-4 显式 author 当前生产卡的语义 Rarity。

当前目标：

| Card | Rarity |
|---|---|
| Strike | Basic |
| Defend | Basic |
| Pommel Strike | Common |
| Twin Strike | Common |
| Uppercut | Uncommon |
| Inflame | Uncommon |

默认 `Common` 只负责迁移安全，不替代本表的正式 authoring。

---

# 21. 实施阶段

## CFV-1 — Rarity Contract

目标：

```text
建立 semantic Rarity
+ authoring validation
+ current/frozen/historical propagation
+ continuity enforcement
```

修改范围：

```text
Cards/CardTypes.h
Cards/CardData.h/.cpp
Cards/CardInstance.h/.cpp
Presentation/PresentationTypes.h
Presentation/PresentationCardSnapshotBuilder.cpp
Presentation/PresentationCardView.cpp
UI/BattleHUDTypes.h
Battle/BattleManagerPresentation.cpp
UI/BattleHUDWidget.cpp
UI/BattleHUDWidgetBase.cpp
focused Automation tests
```

必须覆盖：

```text
ECardRarity domain
UCardData::IsDataValid
UCardInstance::GetRarity
Snapshot.Rarity
HUDView.Rarity
formal Hand freeze
historical mapper
bUpgraded continuity comparison
Rarity continuity comparison
invalid Rarity fail-closed
RichDescription explicit comparison exclusion
```

### CFV-1 Automated Gates

```text
1. SlayTheSpireDemoEditor Win64 Development Build once
2. SlayTheSpireDemo.CFV.RarityContract once
3. SlayTheSpireDemo.Phase6UIA2C.Record.CardZoneChanged once
```

如果 `SlayTheSpireDemo.CFV.RarityContract` 没有直接覆盖 Native exact-identity comparison，则额外跑一个最小直接受影响的 R8 identity test；否则不增加历史 suite。

禁止为了信心扩展到：

```text
Phase6R
A2D5
Shipping
full R8
broad Scenario replay
```

### CFV-1 Editor Asset Gate

`FPresentationCardSnapshot` / `FBattleHUDCardView` 是 BlueprintType；Build 后：

```text
Compile + Save WBP_BattleCard_Native
```

只有 UE 明确指出其他直接依赖 Blueprint 需要重编译/保存时，才处理对应资产。

不得全项目 Blueprint resave。

### CFV-1 Manual PIE Gate

```text
none required
```

CFV-1 尚未改变玩家实际视觉。

---

## CFV-2 — Card Face Shell

目标：

```text
完成新的 UMG 卡面层级
```

工作：

```text
Shadow
Background
Art
Frame
Banner
Name
TypePlate
CostOrb
Description
```

只调整 WBP hierarchy，不实现 Style Resolver。

要求：

```text
优先移动已有 core widgets
保留现有 BindWidget 名称
新增 decorative widgets 使用 Img_ 前缀
旧 BG_Card/VB_CardContent 暂不立即删除
```

### CFV-2 Gate

```text
WBP_BattleCard_Native Compile PASS
WBP_BattleCard_Native Save
core BindWidget contract intact
```

不运行 broad Gameplay regression。

---

## CFV-3 — Pure Visual Resolver

目标：

```text
CardType + Rarity + fixed Ironclad config
→ deterministic FResolvedCardFaceStyle
```

实现：

```text
FCardRarityVisualStyle
FCardFaceStyleConfig（如实现需要，保持 lightweight）
FResolvedCardFaceStyle
pure resolver
RefreshVisualStyle()
new decorative BindWidget references
```

必须测试：

```text
Attack × CommonVisual
Attack × UncommonVisual
Attack × RareVisual

Skill × CommonVisual
Skill × UncommonVisual
Skill × RareVisual

Power × CommonVisual
Power × UncommonVisual
Power × RareVisual
```

同时验证 semantic → visual rarity mapping：

```text
Basic   → CommonVisual
Common  → CommonVisual
Special → CommonVisual
Curse   → CommonVisual
Uncommon→ UncommonVisual
Rare    → RareVisual
```

并验证：

```text
Status / Curse
→ FallbackBackground
→ FallbackFrame
```

缺失 FallbackFrame 时：

```text
Frame = nullptr
→ Widget hides Img_CardFrame
```

### CFV-3 Gates

```text
Editor Build once
SlayTheSpireDemo.CFV.VisualResolver once
```

Manual PIE：

```text
none required
```

---

## CFV-4 — Production Asset Authoring

目标：

```text
把真实 Cropped 素材严格配置进 WBP_BattleCard_Native CDO
并 author 当前生产 CardData Rarity
```

配置目标包括：

```text
bg_attack_red
bg_skill_red
bg_power_red

frame_attack_common
frame_attack_uncommon
frame_attack_rare

frame_skill_common
frame_skill_uncommon
frame_skill_rare

frame_power_common
frame_power_uncommon
frame_power_rare

banner_common
banner_uncommon
banner_rare

common_left / common_center / common_right
uncommon_left / uncommon_center / uncommon_right
rare_left / rare_center / rare_right

card_red_orb
card_shadow
FallbackBackground
FallbackFrame（后续素材未加入前允许为空）
```

### CFV-4 Strict Mapping Gate

采用严格 asset mapping 验证，而不是只检查 non-null。

必须验证类似：

```text
Common.AttackFrame   == frame_attack_common
Common.SkillFrame    == frame_skill_common
Common.PowerFrame    == frame_power_common

Uncommon.AttackFrame == frame_attack_uncommon
Uncommon.SkillFrame  == frame_skill_uncommon
Uncommon.PowerFrame  == frame_power_uncommon

Rare.AttackFrame     == frame_attack_rare
Rare.SkillFrame      == frame_skill_rare
Rare.PowerFrame      == frame_power_rare

Common.Banner        == banner_common
Uncommon.Banner      == banner_uncommon
Rare.Banner          == banner_rare

Common.TypeLeft/Center/Right
→ common_left/common_center/common_right

Uncommon.TypeLeft/Center/Right
→ uncommon_left/uncommon_center/uncommon_right

Rare.TypeLeft/Center/Right
→ rare_left/rare_center/rare_right

CostOrb == card_red_orb
Shadow  == card_shadow
AttackBackground == bg_attack_red
SkillBackground  == bg_skill_red
PowerBackground  == bg_power_red
```

该 Gate 同时验证：

```text
不能把 rare texture 配到 uncommon slot
不能把 Attack frame 配到 Skill/Power slot
不能漏配正式 9 组合中的生产 slot
```

`FallbackFrame` 在对应素材正式加入前允许为空，因此当前不作为 non-null failure。

未来主动更换生产素材时，应同步更新 strict mapping test。

---

## CFV-5 — Visual Acceptance

Automation 已证明：

```text
resolver algorithm
production Class Defaults mapping
```

因此 PIE 不重复手测 9 种组合。

只做代表性视觉验收：

```text
Attack + Basic/CommonVisual
Skill + Uncommon
Power + Rare
```

检查：

```text
背景正确
边框正确
标题丝带正确
类型标签正确
费用宝石正确
阴影正常
卡图无遮挡
名称/类型/描述无裁切
RichText 可读
```

再选择其中一张升级：

```text
same runtime card
→ Frame/Banner/TypePlate 不因升级改变
→ 名称出现 "+"
→ 标题 #7FFF00
→ upgraded numeric text 仍正确
→ Gameplay 不受影响
```

### CFV-5 Gate

```text
one focused manual PIE pass
→ record evidence
→ STOP
```

---

## 22. Validation Policy

遵循 `docs/ValidationExecutionPolicy.md`：

```text
Build once
→ smallest focused Automation once
→ directly affected Level-2 regression only when shared boundary changed
→ manual PIE only for genuinely visual Gate
→ record evidence
→ STOP
```

Passing Gate sticky。

失败时：

```text
identify failed Gate
→ smallest fix
→ rerun only invalidated Gate(s)
```

不得自动扩展：

```text
full regression
Phase6R
A2D5
Shipping
all Blueprint recompilation
broad Scenario A-E
architecture re-review
```

除非当前失败直接 implicate 对应 shared contract。

---

## 23. 明确非目标

CFV 不做：

```text
Card Expansion
新 CardEffect
新 BattleAction
新 Modifier Pipeline
Phase 8 implementation

CardColor / Character Theme system
Silent / Defect / Watcher

Visual Style DataAsset
Visual Registry
Universal Skin System

CardId / DisplayName style branch
string path LoadObject

奖励池
商店
随机稀有度生成
卡牌收藏

升级模型修改
UpgradeLevel
Repeatable Upgrade

Gameplay ↔ Widget reverse query
FCardReadView rarity duplication
```

---

## 24. 长期扩展边界

未来只有在出现第二个真实角色视觉 consumer 后，才引入：

```text
CardColor / Theme
```

届时可自然扩展为：

```text
Theme
├─ AttackBackground
├─ SkillBackground
├─ PowerBackground
├─ CostOrb
└─ RarityStyles
```

当前 CFV 固定 Ironclad/Red，不提前建立多角色 framework。

---

## 25. 最终架构

```text
                     UCardData
                  ┌─────┴─────┐
                  │           │
              CardType      Rarity
                  │           │
                  │           ↓
                  │    Semantic Rarity
                  │           ↓
                  │    Visual Rarity Style
                  │           │
                  ↓           ↓
             Background      Banner
                  │          TypePlate
                  └─────┬─────┘
                        ↓
                       Frame

CardArt ───────────────→ Artwork
Cost ─────────────────→ Cost text / Orb
Description ──────────→ RichText
bUpgraded ────────────→ "+" / #7FFF00
```

Gameplay authority 保持不变：

```text
CardData / CardInstance
→ CardEffect
→ BattleAction
→ BattleActionQueue
→ Modifier Pipeline
→ Commit
→ BattleEvent
```

Presentation 只消费冻结事实：

```text
Gameplay
→ frozen DTO
→ ViewModel
→ UBattleCardWidget
→ pure Visual Resolver
→ UMG decorative surfaces
```

---

## 26. 当前 Stop State

```text
[x] Card Upgrade STS-style Refactor SEALED
[x] CFV requirements identified
[x] STS-inspired semantic rarity model reviewed
[x] Basic/Common/Uncommon/Rare/Special/Curse contract locked
[x] Common migration default locked
[x] CardData authoring validation requirement locked
[x] current Hand + historical Presentation propagation path locked
[x] bUpgraded + Rarity continuity comparison locked
[x] RichDescription generic identity exclusion locked
[x] Status/Curse fallback background/frame contract locked
[x] FallbackFrame may remain null until later asset authoring
[x] pure resolver / Widget lifecycle separation locked
[x] core vs decorative surface failure policy locked
[x] old transient probe protection locked
[x] UMG target hierarchy locked
[x] Img_TypeLeft/Center/Right naming locked
[x] move-existing-widget migration rule locked
[x] CFV-1 narrow Level-2 validation strategy locked
[x] CFV-4 strict production asset mapping Gate locked
[x] validation strategy locked

[ ] CFV-1 implementation authorized
[ ] CFV-1 Rarity Contract implemented/validated
[ ] CFV-2 Card Face Shell implemented/validated
[ ] CFV-3 Visual Resolver implemented/validated
[ ] CFV-4 Production Asset Authoring implemented/validated
[ ] CFV-5 Visual Acceptance
[ ] CFV sealed
```

当前必须停止在：

```text
CARD FACE VISUAL STYLE
DESIGN LOCKED
IMPLEMENTATION NOT AUTHORIZED
```

未经用户明确授权，不开始 CFV-1，也不自动进入后续阶段。
