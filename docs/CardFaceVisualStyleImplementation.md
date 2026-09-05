# Card Face Visual Style 卡牌视觉样式实施计划

日期：**2026-09-05**

状态：

```text
DESIGN REVISED — CARD COLOR / MULTI-CLASS READY
IMPLEMENTATION NOT AUTHORIZED
```

本文件是 Card Face Visual Style（CFV）专项改造的 dedicated authority。

2026-09-05 本轮修订依据：

```text
未来多职业已确认是项目真实需求
且不同职业 / CardColor 将使用不同卡面素材
```

因此，本轮在尚未开始 CFV 实现的低成本窗口内，显式把 `CardColor` 纳入 immutable card semantic metadata，并把卡面素材配置从 Ironclad-only Widget CDO 平铺字段收敛为一个轻量 `UCardFaceStyleSet` Presentation 配置资产。

这不是 Universal Registry，也不是 Character system implementation。本轮只建立真实已确认的扩展轴，并只 author 当前生产需要的 `Red` 风格；Green / Blue / Purple / Colorless / Curse 的生产素材配置留待后续增量内容阶段。

本轮仍然只负责：

```text
CardType / CardRarity / CardColor 语义 metadata
→ Frozen Presentation 传播
→ Native Card Widget 静态视觉样式解析
→ 类型相关卡图窗口 / 裁剪 / 卡面布局
→ CardFaceStyleSet 配置结构
→ 当前 Red 生产卡面素材 authoring
```

不得重新打开已 seal 的 Gameplay / BattleAction / Presentation Timeline / A3 Preview / Card Upgrade ownership 边界。

---

## 1. 目标

在不改变现有 Gameplay 架构的前提下，使卡牌根据：

```text
CardType
CardRarity
CardColor
Upgrade State
```

组合得到正确卡面。

视觉目标包括：

```text
背景
卡图图框（不是覆盖整张卡的外边框）
标题丝带
类型标签底板
费用宝石
阴影
卡图窗口 / 裁剪
中文标题 / 类型 / 描述排版
```

采用参考图的整体构图：卡身、左上费用球、顶部标题丝带、上半部卡图窗口、紧贴卡图下沿的类型标签、下半部描述区。不能退化为普通 VerticalBox 内容列表，也不能把费用重新放回正文流。

最终语义与视觉规则：

```text
CardType
→ VisualCardShape
→ Portrait/Layout geometry

CardColor + VisualCardShape
→ Background

CardColor
→ CostOrb

CardRarity
→ VisualRarityStyle

VisualRarityStyle + VisualCardShape
→ Frame

VisualRarityStyle
→ Banner
→ TypePlate

bUpgraded
→ DisplayName + "+"
→ upgraded title color #7FFF00
```

`CardType`、`CardRarity`、`CardColor`、`bUpgraded` 必须保持正交。

---

## 2. 设计参考与多职业边界

视觉主参考为用户提供的《杀戮尖塔》“打击”卡面；素材尺寸与裁切原点参考仓库：

```text
Content/SlayTheSpireDemo/UI/images/cardui/cardui.atlas
Content/SlayTheSpireDemo/UI/Textures/CardUI/Cropped/...
```

![用户提供的打击卡面参考](references/CardFaceVisualStyle/StrikeReference.png)

该图锁定构图、相对层次、中文阅读层级和当前 Red 风格，不把截图中的鼠标、场景背景或具体数值视为待复刻 Gameplay UI 元素。

参考 Slay the Spire 的卡面分解，本项目采用以下正交轴：

```text
CardType
CardRarity
CardColor
Upgrade State
```

### 2.1 CardColor 不是职业 enum

禁止定义：

```text
Ironclad
Silent
Defect
Watcher
```

作为卡牌视觉颜色 enum。

采用：

```text
Red
Green
Blue
Purple
Colorless
Curse
```

原因：卡面跟卡本身的颜色走，而不是跟当前玩家职业走。

允许未来出现：

```text
Ironclad 持有 Colorless card
Silent 持有 Curse card
跨职业获得 Red card
```

这些卡仍按自己的 `CardColor` 渲染。

### 2.2 多职业是已确认未来需求

因此本轮允许：

```text
ECardColor semantic metadata
UCardFaceStyleSet presentation config asset
Color-aware resolver contract
```

但本轮不实现：

```text
Silent / Defect / Watcher Gameplay/content
角色选择系统
多职业初始牌组
跨职业奖励规则
Green / Blue / Purple 的完整生产视觉 authoring
```

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

静态 metadata 通过 Definition getter 读取，不在 runtime instance 复制第二份 authoritative value。

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

CFV 新增 Rarity 与 CardColor 必须同时进入两条路径。

本阶段仍不向 `FCardReadView` 复制 Rarity / CardColor；formal Hand freeze 继续通过 `Source.Card` / `UCardInstance` 读取 Definition metadata。

---

## 4. 语义 Metadata Contract

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

禁止从 `CardType` 自动推导 `Rarity`，也禁止反向自动改写 `CardType`。

### 4.2 ECardColor

新增：

```cpp
UENUM(BlueprintType)
enum class ECardColor : uint8
{
    Red,
    Green,
    Blue,
    Purple,
    Colorless,
    Curse
};
```

`ECardColor` 表示卡牌自身的视觉颜色族 / card pool color，不表示当前角色实例。

### 4.3 Rarity 迁移默认值

```cpp
ECardRarity Rarity = ECardRarity::Common;
```

含义：

```text
serialized / backward-compatible migration default
```

不表示所有旧卡语义上都是 Common。

CFV-4 必须显式 author 当前生产卡的真实 Rarity。

### 4.4 CardColor 迁移默认值

```cpp
ECardColor CardColor = ECardColor::Red;
```

含义同样只是：

```text
serialized / backward-compatible migration default
```

原因是当前生产 CardData 均属于现有 Ironclad/Red 内容；这样新增字段后，旧 `.uasset`、transient fixture 与历史 Automation 不会因为默认颜色缺失而改变当前视觉。

明确：

```text
Default Red
≠ 所有未来卡牌默认语义上属于 Red
```

CFV-4 必须显式 author 当前生产卡 `CardColor = Red`。

### 4.5 Status / Curse 的显式 authoring

对未来 STS-compatible 内容，推荐生产 authoring：

```text
Status card
CardType  = Status
CardColor = Colorless
Rarity    = Special / 对应内容实际 rarity

Curse card
CardType  = Curse
CardColor = Curse
Rarity    = Curse
```

这些值必须由 CardData 显式 author；底层代码不得从 `CardType` 自动写 `CardColor` 或 `Rarity`。

特别说明：

```text
ECardType::Curse
= “这张卡的内容类型是 Curse”

ECardColor::Curse
= “这张卡使用 Curse 颜色视觉族”
```

二者只是同名语义值，仍然完全正交。

禁止：

```text
if CardType == Curse
    force CardColor = Curse

if CardColor == Curse
    force CardType = Curse
```

理论上底层数据模型允许其他组合；具体内容是否合法由内容设计与生产 authoring 决定，而不是 enum setter side effect 决定。

---

## 5. UCardData / UCardInstance 边界

`UCardData` 增加：

```cpp
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card|Identity")
ECardRarity Rarity = ECardRarity::Common;

UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card|Identity")
ECardColor CardColor = ECardColor::Red;
```

`UCardInstance` 只增加 getter：

```cpp
ECardRarity GetRarity() const;
ECardColor GetCardColor() const;
```

语义：

```text
valid Definition
→ Definition->Rarity
→ Definition->CardColor

invalid Definition
→ Common / Red defensive getter fallback
```

禁止在 `UCardInstance` 增加第二份 mutable/serialized：

```text
Rarity
CardColor
```

---

## 6. CardData Editor Validation

`ECardRarity` 与 `ECardColor` 都属于 authored semantic metadata，必须进入 `UCardData::IsDataValid()`。

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
→ defensive fallback / hide only
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

合法 CardColor：

```text
Red
Green
Blue
Purple
Colorless
Curse
```

非法 enum payload 必须在 CardData Editor validation 阶段报错。

Resolver fallback 不能替代 authoring validation。

---

## 7. Frozen Presentation Metadata 数据链

新增：

```cpp
FPresentationCardSnapshot::Rarity = ECardRarity::Common;
FPresentationCardSnapshot::CardColor = ECardColor::Red;

FBattleHUDCardView::Rarity = ECardRarity::Common;
FBattleHUDCardView::CardColor = ECardColor::Red;
```

完整链路：

```text
UCardData::Rarity / CardColor
        ↓
UCardInstance::GetRarity() / GetCardColor()
        │
        ├─ Current / Formal Hand
        │      ↓
        │ ABattleManager::TryFreezePresentationStateSnapshot
        │      ↓
        │ FBattleHUDCardView::Rarity / CardColor
        │
        └─ Historical / Committed Presentation
               ↓
        PresentationCardSnapshot::TryBuild
               ↓
        FPresentationCardSnapshot::Rarity / CardColor
               ↓
        PresentationCardView::MakePresentationOnlyCardView
               ↓
        FBattleHUDCardView::Rarity / CardColor
```

历史卡面必须只消费 frozen metadata，不允许根据当前玩家职业、当前 BattleManager 或 CardId 重新推断 Color。

---

## 8. Snapshot Validation 与 Continuity Contract

### 8.1 enum domain validation

以下函数必须同时增加 Rarity 与 CardColor enum domain 检查：

```text
IsNativeCardSnapshotValid
IsDiagnosticCardSnapshotValid
```

非法值必须 fail closed。

### 8.2 exact card-face continuity

以下 comparison 必须增加：

```cpp
View.bUpgraded == Snapshot.bUpgraded
View.Rarity == Snapshot.Rarity
View.CardColor == Snapshot.CardColor
```

涉及：

```text
DoesNativeCardViewMatchSnapshot
DoesDiagnosticCardViewMatchSnapshot
```

原因：

```text
bUpgraded
→ 决定卡牌名称 "+" 与升级标题颜色

Rarity
→ 决定 Frame / Banner / TypePlate

CardColor
→ 决定 Background / CostOrb
```

### 8.3 RichDescription 明确排除

generic Hand identity / continuity comparison **不得加入 `RichDescription`**。

合同保持：

```text
RichDescription
→ mapper 必须完整传播
→ generic Hand identity comparison 故意不比较
```

因为 CardPlayed target-specific committed RichDescription 可以合法不同于 source-side Hand baseline。

---

## 9. Semantic Rarity → Visual Rarity Style

项目继续只有三套可见 rarity style：

```text
CommonVisual
UncommonVisual
RareVisual
```

映射：

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

Rarity style 全职业共享，不复制进 Red / Green / Blue / Purple color style。

禁止在 `UCardData` 增加：

```text
VisualRarity
BannerTexture
FrameTexture
BackgroundTexture
StyleSet reference
```

---

## 10. CardType → VisualCardShape

正常内容路径不再使用 `FallbackFrame` 作为 Status / Curse 的主规则。

新增轻量 Presentation-only shape 概念：

```text
AttackShape
SkillShape
PowerShape
```

语义映射：

```text
ECardType::Attack
→ AttackShape

ECardType::Skill
→ SkillShape

ECardType::Power
→ PowerShape

ECardType::Status
→ SkillShape

ECardType::Curse
→ SkillShape
```

该映射只表示：

```text
Frame family
Portrait geometry family
TypePlate geometry family
Background type slot
```

不能把它理解为把 Status / Curse 语义改成 Skill。

例如：

```text
CardType  = Curse
CardColor = Curse
Rarity    = Curse

CardType
→ SkillShape

CardColor
→ Curse ColorStyle

Rarity
→ CommonVisual
```

三者分别解析。

非法 `ECardType` 直接调用 resolver 时，可以 defensive fallback 到 `SkillShape`，但正式 frozen payload validation 仍必须拒绝非法 CardType。

---

## 11. CardColor → Color Visual Style

Color 只选择职业/颜色相关资源，不改变 canonical layout。

每个 ColorStyle 负责：

```text
AttackBackground
SkillBackground
PowerBackground
CostOrb
```

概念结构：

```cpp
USTRUCT(BlueprintType)
struct FCardColorVisualStyle
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FCardFaceTextureRegion AttackBackground;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FCardFaceTextureRegion SkillBackground;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FCardFaceTextureRegion PowerBackground;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FCardFaceTextureRegion CostOrb;
};
```

未来组合示例：

```text
Red + Attack
→ bg_attack_red
→ card_red_orb

Green + Skill
→ bg_skill_green
→ card_green_orb

Blue + Power
→ bg_power_blue
→ card_blue_orb
```

Color 不拥有：

```text
Frame
Banner
TypePlate
AttackLayout / SkillLayout / PowerLayout
```

因此禁止：

```text
RedAttackLayout
GreenAttackLayout
BlueAttackLayout
...
```

不同 Color 的同一 Shape 必须共享同一 canonical geometry；每张 texture region 自带的 Position/Size 只用于恢复 atlas trim placement，不代表语义 layout 不同。

---

## 12. Trimmed Texture Primitive

Cropped Texture 已失去原 AtlasRegion 的 `orig / offset` runtime metadata，因此 Presentation 配置需要保存 texture 与 canonical trim placement 的配对。

新增轻量 primitive：

```cpp
USTRUCT(BlueprintType)
struct FCardFaceLayerPlacement
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FVector2D Position = FVector2D::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FVector2D Size = FVector2D::ZeroVector;
};

USTRUCT(BlueprintType)
struct FCardFaceTextureRegion
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TObjectPtr<UTexture2D> Texture = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FCardFaceLayerPlacement Placement;
};
```

原则：

```text
一个被 atlas trim 过的视觉资源
= Texture + canonical placement
```

禁止把：

```text
Texture 放在一个 struct
Placement 放在另一张不相关表
运行时再按 TextureSize 猜位置
```

Resolver 不读取纹理像素、不解析 atlas、不从 PNG metadata 动态恢复位置。

---

## 13. Rarity Visual Style

使用全职业共享：

```cpp
USTRUCT(BlueprintType)
struct FCardRarityVisualStyle
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FCardFaceTextureRegion Banner;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FCardFaceTextureRegion AttackFrame;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FCardFaceTextureRegion SkillFrame;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FCardFaceTextureRegion PowerFrame;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TObjectPtr<UTexture2D> TypeLeft = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TObjectPtr<UTexture2D> TypeCenter = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TObjectPtr<UTexture2D> TypeRight = nullptr;
};
```

TypePlate 三段使用固定 canonical widget size，本轮不把原始 PNG BrushSize 当 layout authority。

---

## 14. Type Layout 与固定 WBP Geometry

### 14.1 固定 canonical geometry 归 WBP Designer

以下固定几何继续由 `WBP_BattleCard_Native` Designer 持有：

```text
RootSize = 150 × 210
Name 基准区域
Cost text 基准区域
Description 基准区域
Shadow ZOrder / card-face ZOrder
Button hit area
共同交互 geometry
```

不把整张卡的所有布局数值搬进 StyleSet。

### 14.2 只有真正随 Shape 改变的几何进入 Type Layout

```cpp
USTRUCT(BlueprintType)
struct FCardFaceTypeLayout
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FCardFaceLayerPlacement PortraitRect;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FCardFaceLayerPlacement FrameAnchor;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FCardFaceLayerPlacement TypePlateRect;
};
```

StyleSet 只保存：

```text
AttackLayout
SkillLayout
PowerLayout
```

`Status / Curse` 通过 `SkillShape` 使用 `SkillLayout`。

如果后续视觉证明 DescriptionRect 必须按 Shape 有真实差异，再基于实际 evidence 增加；当前不提前做 Red/Green/职业专属 DescriptionLayout。

---

## 15. UCardFaceStyleSet — 单一 Presentation 配置资产

由于未来多职业已确认，本轮允许引入一个很窄的 Presentation DataAsset：

```cpp
UCLASS(BlueprintType)
class UCardFaceStyleSet : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FCardFaceStyleConfig Config;
};
```

其中：

```cpp
USTRUCT(BlueprintType)
struct FCardFaceStyleConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TMap<ECardColor, FCardColorVisualStyle> ColorStyles;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FCardRarityVisualStyle CommonStyle;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FCardRarityVisualStyle UncommonStyle;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FCardRarityVisualStyle RareStyle;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FCardFaceTypeLayout AttackLayout;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FCardFaceTypeLayout SkillLayout;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FCardFaceTypeLayout PowerLayout;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FCardFaceTextureRegion CardShadow;
};
```

生产 `WBP_BattleCard_Native` 只持有：

```cpp
UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Battle HUD|Card|Style")
TObjectPtr<UCardFaceStyleSet> CardFaceStyleSet;
```

### 15.1 DataAsset 定性

`UCardFaceStyleSet` 是：

```text
authored presentation configuration asset
```

不是：

```text
Registry
Service
Global singleton
Gameplay definition
runtime discovery system
Universal Skin System
```

禁止：

```text
CardData → StyleSet reference
CardInstance → StyleSet reference
CardColor → hard-coded LoadObject path
GetMutableDefault / singleton 查 StyleSet
全局 Visual Registry
```

正确 ownership：

```text
WBP_BattleCard_Native
→ EditDefaultsOnly UCardFaceStyleSet reference

UBattleCardWidget
→ 只读取该显式配置

pure resolver
→ 只消费 FCardFaceStyleConfig 参数
```

### 15.2 本轮 populate 范围

本轮只要求：

```text
Red ColorStyle
→ production complete
→ strict mapping required
```

以下 Color key 允许结构存在但未配置：

```text
Green
Blue
Purple
Colorless
Curse
```

本轮这些不属于 CFV-4 non-null requirement。

若未来某个颜色进入生产内容：

```text
author 对应 ColorStyle
→ 增加该 color 的 focused strict mapping gate
→ 不修改 DTO / Widget / Resolver architecture
```

对于有效但未配置的 ColorStyle：

```text
color-specific Background / CostOrb
→ null / Hidden

shared Frame / Banner / TypePlate / text / CardArt
→ 仍可正常显示

Gameplay / Input
→ unaffected
```

禁止静默把 Green / Blue / Curse 渲染成 Red 来掩盖缺配置。

---

## 16. Pure Visual Resolver

视觉计算必须与 Widget lifecycle 解耦。

推荐：

```cpp
FResolvedCardFaceStyle ResolveCardFaceStyle(
    ECardColor CardColor,
    ECardType CardType,
    ECardRarity Rarity,
    const FCardFaceStyleConfig& Config);
```

resolver 内部逻辑分三步：

```text
ResolveVisualCardShape(CardType)
ResolveVisualRarity(Rarity)
ResolveColorStyle(CardColor)
```

概念输出：

```text
FResolvedCardFaceStyle
├─ VisualShape
├─ BackgroundRegion
├─ FrameRegion
├─ BannerRegion
├─ TypeLeft
├─ TypeCenter
├─ TypeRight
├─ CostOrbRegion
├─ ShadowRegion
└─ TypeLayout
```

Resolver：

```text
不创建 Widget
不访问 Gameplay
不查询 BattleManager
不 LoadObject
不读取纹理像素
不根据 TextureSize 决定 canonical layout
```

Automation 可直接构造 transient `FCardFaceStyleConfig` 和 transient `UTexture2D` pointer identity probe，不依赖真实 WBP / NativeConstruct / asset load。

非法 enum 直接调用 resolver 时只做 defensive fallback；正式 authored/frozen validation 仍负责 fail closed。

---

## 17. Core Surface 与 Decorative Surface

### 17.1 Core fail-closed surface

继续保持：

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

### 17.2 Decorative presentation surface

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

新增 decorative `UImage` 使用：

```cpp
UPROPERTY(meta = (BindWidgetOptional))
```

缺失 decorative control / texture：

```text
visual degradation / fallback / hide
→ 不得改变 Gameplay/Input authority
```

生产 `WBP_BattleCard_Native` 仍必须完整提供本轮需要的 decorative controls；缺控件是 CFV asset acceptance failure，但不是 card input fail-closed condition。

---

## 18. Visual Fallback / Clear-Restore Contract

正常内容映射：

```text
Attack → AttackShape
Skill  → SkillShape
Power  → PowerShape
Status → SkillShape
Curse  → SkillShape
```

`FallbackFrame` 不再是 Status/Curse 正常路径。

缺资源规则：

```text
missing selected Background
→ Img_CardBackground Hidden

missing selected Frame
→ Img_CardFrame Hidden

missing Banner
→ Img_CardBanner Hidden

missing TypePlate piece
→ corresponding piece Hidden

missing CostOrb / Shadow
→ corresponding image Hidden

missing CardArt
→ clear old Brush + Img_CardArt Hidden
```

任何 style texture 缺失都不得阻止 card input 或 Gameplay request。

每次刷新必须覆盖全部 resolved 槽：

```text
有效纹理
→ Set Brush ResourceObject
→ apply canonical placement
→ HitTestInvisible

空纹理
→ clear old Brush / ResourceObject
→ Hidden
```

禁止仅在 non-null 时更新导致复用 Widget 保留上一张卡的：

```text
Background
Frame
Banner
TypePlate
CostOrb
CardArt
placement
```

禁止 `MatchSize` 把 512/1024 原始素材像素尺寸写回 150×210 卡面 layout。

---

## 19. WBP_BattleCard_Native 目标层级与 Geometry Ownership

### 19.1 建议层级

保留 `SB_Card → Btn_Card` 根尺寸、点击入口和现有动画锚点。

建议收敛为：

```text
SB_Card
└─ Btn_Card
   └─ OV_Card
      └─ CN_CardFace
         ├─ Img_CardShadow
         ├─ Img_CardBackground
         ├─ Img_CardArt
         ├─ Img_CardFrame
         ├─ Img_CardBanner
         │
         ├─ SB_CardName
         │  └─ Txt_CardName
         │
         ├─ HB_CardTypePlate
         │  ├─ Img_TypeLeft
         │  ├─ Img_TypeCenter
         │  └─ Img_TypeRight
         ├─ Txt_CardType
         │
         ├─ Img_CostOrb
         ├─ Txt_Cost
         └─ Txt_CardDescription
```

把需要 absolute trim placement 的 decorative image 直接放在 `CN_CardFace` 下，避免把 texture placement 与 `UOverlaySlot` 能力混淆。

核心 Text / Art Widget 可以移动，但保留现有绑定名与对象 identity，尽量不删除后重新创建同名控件。

### 19.2 根尺寸

```text
150 × 210
```

这是 layout/geometry 尺寸。

hover / selected / playback 继续沿用现有 render transform；费用球和 Banner 可以视觉超出 body，但不得扩大 Hand slot DesiredSize 或改变播放动画锚点。

父级不对整棵卡面做 `ClipToBounds`；卡图轮廓单独处理。

### 19.3 Atlas trim authoring

本轮生产资源统一使用 CFV-4 指定的 512 Cropped 版本。

离线 authoring 换算继续使用：

```text
Source canvas = 512 × 512
Canonical body origin O = (106, 46)
Canonical body size   = (300, 420)
Uniform scale S       = 0.5

TrimTopLeft = (offset.x, orig.height - offset.y - size.height)
LocalPosition = (TrimTopLeft - O) * S
LocalSize     = size * S
```

`offset` 与 atlas `xy` 不得混用。

换算结果写入相应 `FCardFaceTextureRegion.Placement`；运行时不解析 atlas。

### 19.4 Type Layout 初始值

以下仅作为 CFV-2 authoring 起点，不提前宣称最终视觉 PASS：

| Shape / 层 | 初始局部位置 / 尺寸 | 约束 |
|---|---|---|
| Attack Frame | 约 `(9, 31)` / `131 × 92.5` | 保留下沿斜角 |
| Skill Frame | 约 `(9, 30.5)` / `131.5 × 91.5` | Status/Curse 也复用此 Shape geometry |
| Power Frame | 约 `(7.5, 3)` / `134.5 × 119` | 保留椭圆 |
| Banner | 约 `(-6, 5.5)` / `162 × 38.5` | rarity-specific trim compensation |
| Name | 初始 Rect `(22, 7, 106, 25)` | Designer 固定基准区 |
| Cost Orb | 约 `(-9.5, -8.5)` / `36 × 35.5` | Color-specific texture region |
| TypePlate | 初始中心约 `(75, 115)`，高 `11.5` | 由 Type Layout 微调 |
| Description | 初始 Rect `(17, 133, 116, 57)` | Designer 固定基准区，若真实 evidence 需要再按 Shape 扩展 |

Color 不改变以上 canonical geometry；不同 color texture 的 placement 只补偿其自身 trim 差异。

### 19.5 卡图轮廓与 Alpha

本轮采用：

```text
frozen CardArt
+ VisualCardShape-specific PortraitRect
+ 对应 Frame 覆盖接缝
```

规则：

- `Img_CardArt` 始终使用 frozen `FBattleHUDCardView.CardArt`；不得用 CardId / DisplayName / 当前职业查图。
- Attack / Skill / Power 使用各自 PortraitRect；Status / Curse 使用 SkillShape PortraitRect。
- 透明 Frame 不能真正裁掉窗口外非透明像素。
- 如果当前代表生产卡 `T_strike / T_defend / T_inflame` 的 UI portrait Alpha / crop 与目标图框不吻合，本 CFV 明确授权仅对这些 UI CardArt texture 的 Alpha / 裁切导入做必要修正。
- 该授权只涉及 UI texture presentation，不得修改 CardData Gameplay metadata、Runtime identity、Effect 或 frozen semantics。
- 本轮不建立 per-color / universal mask material；若大量后续素材证明需要统一 mask，再基于真实 consumer 单独设计。

### 19.6 TypePlate 与交互

```text
HB_CardTypePlate
├─ Img_TypeLeft   → 固定 7 × 11.5 / Auto
├─ Img_TypeCenter → Fill / 固定高 11.5
└─ Img_TypeRight  → 固定 7.5 × 11.5 / Auto
```

Decorative leaf 使用 `HitTestInvisible`；祖先容器不得截断 `Btn_Card` 命中。

Button Normal/Hovered/Pressed 的默认矩形 brush / ContentPadding 不得覆盖、挤动或重新着色 rarity / upgrade visual semantics。

---

## 20. UMG 迁移约束与 Refresh 分层

必须保留现有 core binding 名称：

```text
Btn_Card
Txt_CardName
Txt_Cost
Txt_CardDescription
Txt_CardType
Img_CardArt
```

CFV-2 优先移动已有 Widget，而不是删除后新建同名 Widget。

旧：

```text
BG_Card
VB_CardContent
Img_CostBase（若仍存在）
```

新路径验证前不得立即删除。

推荐：

```text
建立新视觉层
→ 接入
→ Collapse 旧 fallback
→ 验证
→ 最后删除旧层
```

最终生产树只能有一套可见 CardArt / Name / Cost / Description。

### 20.1 RefreshFromCardView 分层

```text
RefreshFromCardView()
├─ ResolveCardFaceStyle(
│      CurrentCardView.CardColor,
│      CurrentCardView.CardType,
│      CurrentCardView.Rarity,
│      StyleSet.Config)
│
├─ RefreshCoreCardContent()
│  ├─ Name
│  ├─ Cost
│  ├─ Description
│  ├─ Type
│  └─ RefreshCardArtwork(resolved TypeLayout + frozen CardArt)
│
└─ RefreshVisualStyle()
   ├─ Background
   ├─ Frame
   ├─ Banner
   ├─ TypePlate
   ├─ Orb
   └─ Shadow
```

旧 transient Card Probe / A3 tests 继续只需要注入 core surface。

不得要求旧 DTO / A3 测试为了 CFV 创建 decorative fake UImage。

`SetCardView()` 在 Construct 前后都必须最终得到相同状态；同一 Widget 连续接收不同 DTO 时不得残留旧样式。

`ApplyImmediatePreview / ClearImmediatePreview` 仍只更新描述，不得修改：

```text
CardColor
Rarity
Frame
Banner
TypePlate
Background
CardArt
root geometry
formal Hand Widget identity
```

---

## 21. Upgrade 与 CFV 完全正交

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
A3 Preview Gameplay semantics
```

最终组合：

```text
CardColor + VisualCardShape
→ Background

CardColor
→ CostOrb

VisualRarity + VisualCardShape
→ Frame

VisualRarity
→ Banner / TypePlate

VisualCardShape
→ Portrait geometry

bUpgraded
→ "+" / #7FFF00

CardArt
→ Artwork

Description
→ RichText
```

---

## 22. 字体策略

中文标题、类型和正文使用同一简体中文字体族；费用数字保留参考图的大号数字风格。

CFV-4 author：

| 控件 / 样式 | UE Font 包路径 | 初始字号 / 排版 |
|---|---|---|
| `Txt_CardName` | `/Game/SlayTheSpireDemo/UI/font/zhs/SourceHanSerifSC-Bold_Font` | 14，居中，深色 Outline 1；升级仍 `#7FFF00` |
| `Txt_CardType` | `/Game/SlayTheSpireDemo/UI/font/zhs/SourceHanSerifSC-Bold_Font` | 8，居中 |
| `Txt_Cost` | `/Game/SlayTheSpireDemo/UI/font/Kreon-Bold_Font` | 23，居中，深色 Outline 1 |
| Description Default / numeric styles | `/Game/SlayTheSpireDemo/UI/font/zhs/SourceHanSerifSC-Medium_Font` | 11，自动换行 |

`Txt_CardDescription` 是 `URichTextBlock`，必须同步确认：

```text
DT_BattleCardTextStyles
Default
PreviewIncrease
PreviewDecrease
其他现有 semantic rows
```

所有描述行保持统一 Font / Size / Outline / baseline，并保留既有 per-value semantic color/tag。

Name 单行必须容纳当前最长生产名称及 `+`；Description 必须容纳当前最长、多段生产描述。

本阶段不修改 `BattleTextResolver` 语义解析。

---

## 23. 当前生产 CardData Metadata 迁移

CFV-4 显式 author 当前六张生产卡：

| Card | CardType | Rarity | CardColor |
|---|---|---|---|
| Strike | Attack | Basic | Red |
| Defend | Skill | Basic | Red |
| Pommel Strike | Attack | Common | Red |
| Twin Strike | Attack | Common | Red |
| Uppercut | Attack | Uncommon | Red |
| Inflame | Power | Uncommon | Red |

完整包路径：

| 生产 CardData 包路径 | CardType | Rarity | CardColor |
|---|---|---|---|
| `/Game/SlayTheSpireDemo/Data/Cards/Ironclad/Attacks/DA_Card_Strike` | Attack | Basic | Red |
| `/Game/SlayTheSpireDemo/Data/Cards/Ironclad/Skills/DA_Card_Defend` | Skill | Basic | Red |
| `/Game/SlayTheSpireDemo/Data/Cards/Ironclad/Attacks/DA_Card_PommelStrike` | Attack | Common | Red |
| `/Game/SlayTheSpireDemo/Data/Cards/Ironclad/Attacks/DA_Card_TwinStrike` | Attack | Common | Red |
| `/Game/SlayTheSpireDemo/Data/Cards/Ironclad/Attacks/DA_Card_Uppercut` | Attack | Uncommon | Red |
| `/Game/SlayTheSpireDemo/Data/Cards/Ironclad/powers/DA_Card_Inflame` | Power | Uncommon | Red |

`Common / Red` 默认值只负责 migration safety，不替代显式生产 authoring。

---

# 24. 实施阶段

## CFV-1 — Card Metadata Contract

目标：

```text
建立 semantic Rarity + CardColor
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
ECardColor domain
UCardData::IsDataValid
UCardInstance::GetRarity
UCardInstance::GetCardColor
Snapshot.Rarity / CardColor
HUDView.Rarity / CardColor
formal Hand freeze
historical mapper
bUpgraded continuity
Rarity continuity
CardColor continuity
invalid enum fail-closed
RichDescription comparison exclusion
```

### CFV-1 AUTOMATED GATES

```text
1. SlayTheSpireDemoEditor Win64 Development Build once
2. SlayTheSpireDemo.CFV.CardMetadataContract once
3. SlayTheSpireDemo.Phase6UIA2C.Record.CardZoneChanged once
```

如果新的 CFV contract test 没有直接覆盖 Native exact-identity comparison，则额外跑一个最小直接受影响的 R8 identity test；否则不增加历史 suite。

禁止扩展到：

```text
Phase6R
A2D5
Shipping
full R8
broad Scenario replay
```

### CFV-1 Editor Asset Gate

`FPresentationCardSnapshot / FBattleHUDCardView` 是 BlueprintType；Build 后：

```text
Compile + Save WBP_BattleCard_Native
```

只有 UE 明确指出其他直接依赖 Blueprint 需要处理时，才额外 Compile / Save 对应资产。

不得全项目 Blueprint resave。

### CFV-1 MANUAL PIE

```text
none required
```

---

## CFV-2 — Card Face Shell

目标：

```text
完成 canonical UMG card-face shell
+ Attack / Skill / Power Shape geometry authoring
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

要求：

```text
固定公共 geometry 归 WBP Designer
Color 不产生独立 layout
Type-dependent geometry 只分 Attack/Skill/Power
Status/Curse 复用 SkillShape
优先移动已有 core widgets
保留 existing BindWidget names
new decorative widgets 使用 Img_ 前缀
旧层先 Collapse 后删除
```

### CFV-2 GATES

```text
WBP_BattleCard_Native Compile PASS
WBP_BattleCard_Native Save
core BindWidget contract intact
decorative controls / ZOrder / hit-test contract correct
root 150 × 210
保存最终 Attack/Skill/Power PortraitRect / FrameAnchor / TypePlateRect
保存各已使用 512 texture region trim placements
```

无独立 PIE；最终视觉集中在 CFV-5。

---

## CFV-3 — StyleSet + Pure Resolver

目标：

```text
CardColor + CardType + CardRarity
→ deterministic FResolvedCardFaceStyle
```

实现：

```text
FCardFaceLayerPlacement
FCardFaceTextureRegion
FCardColorVisualStyle
FCardRarityVisualStyle
FCardFaceTypeLayout
FCardFaceStyleConfig
UCardFaceStyleSet
FResolvedCardFaceStyle
pure resolver
RefreshVisualStyle()
RefreshCardArtwork()
new decorative BindWidgetOptional references
independent CFV decorative probe
```

### Resolver 自动测试

分别验证三个正交轴：

```text
CardColor selection
Red / Green / Blue / Purple / Colorless / Curse
→ transient sentinel ColorStyle 选择正确

CardType → Shape
Attack → AttackShape
Skill  → SkillShape
Power  → PowerShape
Status → SkillShape
Curse  → SkillShape

Rarity → VisualRarity
Basic/Common/Special/Curse → CommonVisual
Uncommon → UncommonVisual
Rare → RareVisual
```

再做少量组合验证：

```text
Red + Attack + Basic
Green + Skill + Rare
Blue + Power + Uncommon
CurseColor + CurseType + CurseRarity
```

证明三个轴独立组合，不需要穷举 6×5×6 全矩阵。

### WidgetStyle 自动测试

同一 Widget：

```text
Red Rare Attack
→ transient Green Basic Skill
→ valid-but-unconfigured Blue
→ restore Red Power
```

必须验证：

```text
Brush.ResourceObject 与当前 resolved config 一致
Hidden / HitTestInvisible 正确切换
旧 Brush / placement / CardArt 不残留
missing optional decorative controls 不阻断 core content
MatchSize 不改变 150×210 root
SetCardView Construct 前后结果一致
bUpgraded 不改变 Color/Rarity/Shape/Layout
Preview Apply/Clear 不改变静态 style
```

### CFV-3 AUTOMATED GATES

```text
Editor Build once
SlayTheSpireDemo.CFV.VisualResolver once
SlayTheSpireDemo.CFV.WidgetStyle once
Compile + Save + Reopen WBP_BattleCard_Native once after reflected config/bindings change
```

无独立 PIE。

---

## CFV-4 — Production StyleSet / Asset Authoring

目标：

```text
创建并配置生产 UCardFaceStyleSet
只完整 author Red ColorStyle
严格配置共享 Rarity styles / Type layouts / Shadow
显式 author 六张生产 CardData Rarity + CardColor
```

建议生产 StyleSet 资产：

```text
/Game/SlayTheSpireDemo/UI/Styles/DA_CardFaceStyleSet
```

`WBP_BattleCard_Native` CDO 的 `CardFaceStyleSet` 指向该资产。

### CFV-4 Red ColorStyle 唯一生产资源表

| 配置槽 | 完整 UE 包路径 | PNG 尺寸 |
|---|---|---|
| `ColorStyles[Red].AttackBackground` | `/Game/SlayTheSpireDemo/UI/Textures/CardUI/Cropped/cardui4/512/bg_attack_red` | 302 × 419 |
| `ColorStyles[Red].SkillBackground` | `/Game/SlayTheSpireDemo/UI/Textures/CardUI/Cropped/cardui4/512/bg_skill_red` | 299 × 419 |
| `ColorStyles[Red].PowerBackground` | `/Game/SlayTheSpireDemo/UI/Textures/CardUI/Cropped/cardui4/512/bg_power_red` | 299 × 419 |
| `ColorStyles[Red].CostOrb` | `/Game/SlayTheSpireDemo/UI/Textures/CardUI/Cropped/cardui/512/card_red_orb` | 72 × 71 |
| `CardShadow` | `/Game/SlayTheSpireDemo/UI/Textures/CardUI/Cropped/cardui4/512/card_shadow` | 300 × 420 |

### CFV-4 Shared Rarity Style 资源表

| 配置槽 | 完整 UE 包路径 | PNG 尺寸 |
|---|---|---|
| `CommonStyle.AttackFrame` | `/Game/SlayTheSpireDemo/UI/Textures/CardUI/Cropped/cardui2/512/frame_attack_common` | 262 × 185 |
| `UncommonStyle.AttackFrame` | `/Game/SlayTheSpireDemo/UI/Textures/CardUI/Cropped/cardui/512/frame_attack_uncommon` | 263 × 185 |
| `RareStyle.AttackFrame` | `/Game/SlayTheSpireDemo/UI/Textures/CardUI/Cropped/cardui2/512/frame_attack_rare` | 262 × 185 |
| `CommonStyle.SkillFrame` | `/Game/SlayTheSpireDemo/UI/Textures/CardUI/Cropped/cardui/512/frame_skill_common` | 263 × 183 |
| `UncommonStyle.SkillFrame` | `/Game/SlayTheSpireDemo/UI/Textures/CardUI/Cropped/cardui2/512/frame_skill_uncommon` | 263 × 183 |
| `RareStyle.SkillFrame` | `/Game/SlayTheSpireDemo/UI/Textures/CardUI/Cropped/cardui2/512/frame_skill_rare` | 263 × 183 |
| `CommonStyle.PowerFrame` | `/Game/SlayTheSpireDemo/UI/Textures/CardUI/Cropped/cardui/512/frame_power_common` | 269 × 238 |
| `UncommonStyle.PowerFrame` | `/Game/SlayTheSpireDemo/UI/Textures/CardUI/Cropped/cardui/512/frame_power_uncommon` | 269 × 238 |
| `RareStyle.PowerFrame` | `/Game/SlayTheSpireDemo/UI/Textures/CardUI/Cropped/cardui/512/frame_power_rare` | 269 × 238 |
| `CommonStyle.Banner` | `/Game/SlayTheSpireDemo/UI/Textures/CardUI/Cropped/cardui/512/banner_common` | 324 × 77 |
| `UncommonStyle.Banner` | `/Game/SlayTheSpireDemo/UI/Textures/CardUI/Cropped/cardui2/512/banner_uncommon` | 324 × 77 |
| `RareStyle.Banner` | `/Game/SlayTheSpireDemo/UI/Textures/CardUI/Cropped/cardui/512/banner_rare` | 324 × 77 |
| `CommonStyle.TypeLeft` | `/Game/SlayTheSpireDemo/UI/Textures/CardUI/Cropped/cardui/512/common_left` | 14 × 23 |
| `CommonStyle.TypeCenter` | `/Game/SlayTheSpireDemo/UI/Textures/CardUI/Cropped/cardui/512/common_center` | 32 × 23 |
| `CommonStyle.TypeRight` | `/Game/SlayTheSpireDemo/UI/Textures/CardUI/Cropped/cardui/512/common_right` | 15 × 23 |
| `UncommonStyle.TypeLeft` | `/Game/SlayTheSpireDemo/UI/Textures/CardUI/Cropped/cardui/512/uncommon_left` | 14 × 23 |
| `UncommonStyle.TypeCenter` | `/Game/SlayTheSpireDemo/UI/Textures/CardUI/Cropped/cardui/512/uncommon_center` | 32 × 23 |
| `UncommonStyle.TypeRight` | `/Game/SlayTheSpireDemo/UI/Textures/CardUI/Cropped/cardui/512/uncommon_right` | 15 × 23 |
| `RareStyle.TypeLeft` | `/Game/SlayTheSpireDemo/UI/Textures/CardUI/Cropped/cardui/512/rare_left` | 14 × 23 |
| `RareStyle.TypeCenter` | `/Game/SlayTheSpireDemo/UI/Textures/CardUI/Cropped/cardui/512/rare_center` | 32 × 23 |
| `RareStyle.TypeRight` | `/Game/SlayTheSpireDemo/UI/Textures/CardUI/Cropped/cardui/512/rare_right` | 15 × 23 |

Green / Blue / Purple / Colorless / Curse 本轮允许没有生产 ColorStyle，不进入 strict non-null Gate。

### CFV-4 Strict Mapping Gate

新增：

```text
SlayTheSpireDemo.CFV.AssetAuthoring
```

必须严格验证：

```text
DA_CardFaceStyleSet exists
WBP_BattleCard_Native.CardFaceStyleSet == DA_CardFaceStyleSet

Red.AttackBackground == bg_attack_red
Red.SkillBackground  == bg_skill_red
Red.PowerBackground  == bg_power_red
Red.CostOrb           == card_red_orb

Common/Uncommon/Rare × Attack/Skill/Power Frame
→ 与上表逐项严格一致

Common/Uncommon/Rare Banner
→ 与上表逐项严格一致

Common/Uncommon/Rare TypeLeft/Center/Right
→ 与上表逐项严格一致

CardShadow == card_shadow

Attack/Skill/Power TypeLayout
→ 与 CFV-2 最终 authoring 表一致

所有 FCardFaceTextureRegion
→ Texture 与 trim Placement 成对正确
```

不得只检查 short name / non-null。

该 Gate 还验证：

```text
六张生产 CardData
→ CardId / CardType / Rarity / CardColor
→ UCardData::IsDataValid PASS

生产 WBP
→ core/decorative 控件类型正确
→ 无第二套可见旧卡面

RichText/font authoring
→ 与 §22 contract 一致
```

### CFV-4 CardArt Alpha 授权边界

如果 Strike / Defend / Inflame 的 UI CardArt Alpha/crop 与对应 Shape 图框不吻合：

```text
允许修改 UI texture Alpha / crop import
```

但不得修改：

```text
CardId
CardType
Rarity
CardColor
Effects
Gameplay values
Runtime identity
```

修改后记录具体 texture 与原因。

### CFV-4 Build Budget

若 CFV-3 build 后仅做 asset authoring：

```text
不重复 Editor Build
```

只有实际新增/修改 C++ 测试代码时补一次必要 Build。

无独立 PIE。

---

## CFV-5 — Visual Acceptance

前置：

```text
CFV-1 CardMetadataContract PASS
CFV-3 VisualResolver PASS
CFV-3 WidgetStyle PASS
CFV-4 AssetAuthoring PASS
```

复用 sticky evidence，不为了最终编号重跑。

### A. 正式 Validation Preview Asset

保留：

```text
/Game/SlayTheSpireDemo/UI/Validation/WBP_CFVCardFacePreview
```

它是 committed validation asset：

```text
只消费 detached FBattleHUDCardView + production CardFaceStyleSet
不接 Gameplay
不接 HUD
不接 Queue
不绑定选牌事件
不得成为生产引用链依赖
```

用途：在没有真实生产 Rare / 非 Red 卡时，重复验证 frozen DTO 的静态组合能力。

本轮至少配置一个：

```text
CardType = Attack
CardColor = Red
Rarity = Rare
bUpgraded = false
DisplayName = 打击
Cost = 1
CardArt = T_strike
```

验证 Rare frame / banner / typeplate / layout。

未来新增 Green / Blue / Purple style 后继续复用同一 Preview Asset，而不是新增每职业 WBP。

### B. 一次 focused production-map PIE

地图：

```text
/Game/SlayTheSpireDemo/Maps/L_BattleTest
```

在未保存的关卡实例临时设置：

```text
Strike
Defend
Inflame
OpeningHandDrawCount = 3
DeckDebugSeed = 1337
```

观察：

```text
Strike  = Red + Attack + Basic
Defend  = Red + Skill + Basic
Inflame = Red + Power + Uncommon
```

确认：

```text
Red Background 正确
red orb 正确
Attack / Skill / Power frame 正确
rarity banner/typeplate 正确
卡图 Alpha / frame 内缘正确
标题 / 类型 / 描述无裁切
shadow / cost overlap 正确
旧层无重复显示
输入区域仍正确
```

然后用现有 `TestUpgradeFirstHandCard` 升级同一 runtime card：

```text
名称出现 +
标题 #7FFF00
升级数值正确
CardColor / Rarity / Frame / Banner / TypePlate / layout 不变
```

再通过合法操作打出 Inflame、Strike，确认 A3 Preview 与：

```text
Hand → PlayArea → Discard
```

历史播放卡保持 frozen：

```text
CardColor
Rarity
CardType/Shape
CardArt
name/size
```

无跳变、无旧卡面残留。

结束 PIE 后恢复临时地图设置且不保存。

如果无法自动执行视觉步骤，标记：

```text
USER ACTION REQUIRED
```

自动测试不能替代未执行的交互/动画观察。

---

## 25. Validation Policy

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

失败：

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

除非失败直接 implicate 对应 shared contract。

---

## 26. Scope / Non-Goals

### 26.1 本轮明确允许

因为未来多职业 + 不同卡面素材已经确认，本轮明确允许：

```text
ECardColor semantic metadata
Rarity + CardColor frozen propagation
CardColor-aware pure resolver
UCardFaceStyleSet presentation DataAsset structure
Red production ColorStyle authoring
shared Rarity styles
Attack/Skill/Power Type layouts
committed CFV validation preview asset
必要的代表生产 CardArt Alpha/crop UI 修正
```

### 26.2 本轮仍不做

```text
Card Expansion
新 CardEffect
新 BattleAction
新 Modifier Pipeline
Phase 8 implementation

Silent / Defect / Watcher Gameplay/content implementation
character selection system
character-owned visual lookup
Green / Blue / Purple full production authoring
Colorless / Curse production card content implementation

Visual Registry
Universal Skin System
runtime style discovery
CardColor → LoadObject path
CardData → StyleSet reference

CardId / DisplayName style branch

奖励池
商店
随机稀有度生成
卡牌收藏

升级模型修改
UpgradeLevel
Repeatable Upgrade

Gameplay ↔ Widget reverse query
FCardReadView Rarity/CardColor duplication
```

---

## 27. 长期扩展边界

未来增加 Silent / Defect / Watcher 时，正常路径应是：

```text
新增生产 CardData
→ 显式 author CardColor = Green / Blue / Purple

补对应 UCardFaceStyleSet.ColorStyles[color]
→ AttackBackground
→ SkillBackground
→ PowerBackground
→ CostOrb

增加该 Color 的 focused strict asset mapping
```

以下内容**不应**因为新增职业而重新设计：

```text
FPresentationCardSnapshot
FBattleHUDCardView
UBattleCardWidget ownership
WBP hierarchy
RarityStyle
TypeLayout
pure resolver signature
Upgrade presentation contract
```

如果未来某个职业真的需要不同 canonical geometry，必须由实际素材 evidence 单独提出，而不是提前引入 `RedAttackLayout / GreenAttackLayout`。

---

## 28. 最终架构

```text
                         UCardData
              ┌────────────┼────────────┐
              │            │            │
          CardType      CardRarity   CardColor
              │            │            │
              │            │            │
              ↓            ↓            ↓
       VisualCardShape  VisualRarity   ColorStyle
              │            │            │
              │            │            ├─ Background family
              │            │            └─ CostOrb
              │            │
              │            ├─ Banner
              │            └─ TypePlate
              │
              ├─ Portrait/Layout geometry
              │
              └──────┬───────────────┐
                     │               │
               + VisualRarity   + ColorStyle
                     │               │
                     ↓               ↓
                   Frame         Background

CardArt ─────────────────────────────→ alpha-aware Artwork
bUpgraded ───────────────────────────→ "+" / #7FFF00
Description ─────────────────────────→ RichText
```

配置 ownership：

```text
WBP_BattleCard_Native
→ canonical fixed geometry
→ EditDefaultsOnly CardFaceStyleSet reference

UCardFaceStyleSet
→ ColorStyles
→ shared RarityStyles
→ shared TypeLayouts
→ trimmed TextureRegion placements

pure resolver
→ consumes frozen DTO metadata + StyleSet.Config
```

Gameplay authority 保持：

```text
CardData / CardInstance
→ CardEffect
→ BattleAction
→ BattleActionQueue
→ Modifier Pipeline
→ Commit
→ BattleEvent
```

Presentation 仍只消费冻结事实。

---

## 29. 当前 Stop State

```text
[x] Card Upgrade STS-style Refactor SEALED
[x] CFV requirements identified
[x] STS-inspired semantic rarity model reviewed
[x] future multi-class / different card-face assets confirmed as concrete requirement
[x] ECardColor semantic axis decided
[x] Red/Green/Blue/Purple/Colorless/Curse enum contract decided
[x] CardColor != character identity locked
[x] Common Rarity migration default locked
[x] Red CardColor migration default locked
[x] Status/Curse explicit CardColor authoring rule locked
[x] ECardType::Curse vs ECardColor::Curse orthogonality locked
[x] CardData authoring validation requirement locked
[x] current Hand + historical Presentation Rarity/CardColor path defined
[x] bUpgraded + Rarity + CardColor continuity comparison defined
[x] RichDescription generic identity exclusion retained
[x] CardType → VisualCardShape mapping defined
[x] Status/Curse → SkillShape normal visual path defined
[x] FallbackFrame removed from normal Status/Curse path
[x] ColorStyle owns Background family + CostOrb only
[x] RarityStyle remains shared across colors/classes
[x] Color does not own layout
[x] trimmed TextureRegion primitive defined
[x] fixed WBP geometry vs TypeLayout ownership defined
[x] UCardFaceStyleSet explicitly justified by confirmed multi-class need
[x] UCardFaceStyleSet classified as config asset, not Registry
[x] Red-only production authoring scope locked
[x] Green/Blue/Purple/Colorless/Curse may remain unconfigured this slice
[x] valid-but-unconfigured ColorStyle runtime hide behavior defined
[x] pure resolver / Widget lifecycle separation retained
[x] core vs decorative failure policy retained
[x] old transient probe protection retained
[x] CardArt Alpha/crop UI-only repair boundary authorized
[x] formal /UI/Validation/WBP_CFVCardFacePreview retained
[x] CFV-1 narrow Level-2 validation strategy updated for Rarity + CardColor
[x] CFV-4 strict Red + shared-style mapping Gate defined
[x] one focused production-map PIE path retained
[x] implementation evidence still not claimed

[ ] user final review of this revised authority
[ ] CFV design status promoted to DESIGN LOCKED
[ ] CFV-1 implementation authorized
[ ] CFV-1 Card Metadata Contract implemented/validated
[ ] CFV-2 Card Face Shell implemented/validated
[ ] CFV-3 StyleSet + Visual Resolver implemented/validated
[ ] CFV-4 Production Asset Authoring implemented/validated
[ ] CFV-5 Visual Acceptance
[ ] CFV sealed
```

当前必须停止在：

```text
CARD FACE VISUAL STYLE
DESIGN REVISED — CARD COLOR / MULTI-CLASS READY
IMPLEMENTATION NOT AUTHORIZED
```

本次只修订 authority 文档。

未经用户明确授权，不开始 CFV-1，也不自动进入后续阶段。
