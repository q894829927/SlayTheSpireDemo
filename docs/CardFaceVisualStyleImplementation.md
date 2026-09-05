# Card Face Visual Style 卡牌视觉样式实施计划

日期：**2026-09-05**

状态：

```text
DESIGN REVISED — STS CARD-FACE REFERENCE
IMPLEMENTATION NOT AUTHORIZED
```

本文件是 Card Face Visual Style（CFV）专项改造的 dedicated authority。

2026-09-05 修订：采用用户提供的《杀戮尖塔》「打击」卡面作为视觉验收参考，补齐卡面几何、卡图裁剪、optional binding、Widget 应用测试、唯一素材映射及可执行 PIE 场景。本轮授权是修改设计文档；C++、UMG、材质、CardData 的实施尚未授权。文中新增类型、测试入口和资产均是待实施要求，不代表已存在或已验证。

本阶段只负责：

```text
卡牌语义 Rarity
→ Frozen Presentation 传播
→ Native Card Widget 静态视觉样式解析
→ 类型相关卡图窗口 / 裁剪 / 卡面布局
→ 生产 WBP 卡面素材配置
```

不得重新打开已 seal 的 Gameplay / BattleAction / Presentation Timeline / A3 Preview / Card Upgrade ownership 边界。

---

## 1. 目标

在不改变现有 Gameplay 架构的前提下，使卡牌根据 `CardType`、`CardRarity` 和当前固定 Ironclad/Red 视觉主题显示对应的：

```text
背景
卡图图框（不是覆盖整张卡的外边框）
标题丝带
类型标签底板
费用宝石
阴影
```

并保持升级表现与稀有度/类型视觉完全正交。

采用参考图的整体构图：红色卡身；左上角叠压卡身与标题的费用球；顶部横向标题丝带；上半部卡图窗口；紧贴卡图下沿的类型标签；下半部深色描述区。不能把这些组件排成普通垂直列表，也不能把费用放回内容区。

最终视觉规则：

```text
CardType
→ Background + Portrait window / clipping + Frame placement

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

视觉主参考为本次用户提供的图片；素材尺寸、裁切原点参考仓库的 `Content/SlayTheSpireDemo/UI/images/cardui/cardui.atlas` 与对应 Cropped PNG。无需从第三方 API 推断当前素材的布局。

![用户提供的打击卡面参考](references/CardFaceVisualStyle/StrikeReference.png)

该图锁定构图、相对层次、中文阅读层级和 Ironclad/Red 外观；截图里的数字、卡名、鼠标指针和场景背景不作为 Gameplay 配置或待复刻 UI 元素。截图不能直接给出所有类型的精确坐标；Attack / Skill / Power 分别使用其真实图框轮廓，具体布局按 §15 author。

本项目保持以下维度正交：

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

### 11.1 单一配置所有者

采用一个 lightweight `FCardFaceStyleConfig` 聚合上述字段，保存在 `UBattleCardWidget` 的 `EditDefaultsOnly` 属性中，由生产 WBP CDO author。不要同时保留一份平铺字段与一份 struct 副本。

另包含本次已有明确 consumer 的卡面配置：

```text
AttackLayout / SkillLayout / PowerLayout / FallbackLayout
    → PortraitRect、FramePlacement、TypePlateRect、DescriptionRect
    → Portrait fitting / source-alpha contract
CommonLayout
    → RootSize、Background placement、Banner placement、Cost placement、Shadow placement
```

`FCardFaceLayerPlacement` 只描述局部位置和尺寸；每个 rarity frame、banner 的 trim placement 与对应纹理成对 author，因为不同 rarity 的 Cropped 外接矩形可能略有不同。Rarity 切换只补偿素材裁切差异，不改变该类型的卡图窗口、文字区域或根尺寸。§15 定义坐标换算。

布局属于 UI 配置，不得写入 CardData、CardInstance 或 frozen DTO。本轮优先使用现有带 Alpha 的 portrait 纹理，不建立新的动态材质 / mask 系统。生产卡图若不符合 §15.3 轮廓要求，应先修正该 UI 纹理的 Alpha，再继续视觉验收。

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

同时返回 resolved layer placements、该类型的 PortraitRect / DescriptionRect 等轻量布局值。Resolver 只选择配置和计算布局，不访问纹理像素、不根据加载后的 TextureSize 决定布局、不创建 Widget、不查询 Gameplay。

`RefreshFromCardView()` 用 `CurrentCardView.CardType / Rarity` 解析一次结果，再分别交给核心内容/卡图刷新和装饰刷新。`UBattleCardWidget::RefreshVisualStyle()` 只负责应用纹理、placement 和可见性；`RefreshCardArtwork()` 负责 frozen `CardArt` 的 Brush 和该类型的 PortraitRect。普通文本和 A3 RichText 仍走既有核心内容路径。

Automation 可使用 transient `UTexture2D` pointer identity probe 测试，不依赖 WBP / NativeConstruct。

有效 enum 的映射按 §9–10；独立调用 Resolver 遇到非法 Rarity 时使用 CommonVisual，非法 CardType 使用 FallbackLayout / Background / Frame。这只是视觉防御，不代替 §6/8 的 authored/frozen payload validation。

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

所有新增 decorative `UImage` 明确使用 `UPROPERTY(meta = (BindWidgetOptional))`，并逐项检查有效性。用于定位的新增容器也不得进入 `bNativeBindingsValid` 核心有效性判定。Core 继续使用现有 `BindWidget`。

背景、图框、丝带等不得参与 Gameplay playability 判定。生产资产缺少必需装饰时，CFV 资产验收失败；运行时仍保留卡牌操作能力。这两个结果分别记录。

---

## 14. Visual Fallback Contract

视觉资源缺失与 UMG 核心绑定缺失必须区分。

规则：

```text
missing Background texture
→ FallbackBackground

missing FallbackBackground
→ Img_CardBackground Hidden

missing rarity Frame
→ FallbackFrame

missing FallbackFrame
→ Img_CardFrame Hidden

missing Banner
→ Img_CardBanner Hidden

missing TypePlate piece
→ corresponding decorative piece Hidden（保留其显式布局尺寸）

missing CostOrb / Shadow
→ corresponding decorative image hidden
```

任何 style texture 缺失都不得阻止 card input 或 Gameplay request。

每次刷新都覆盖全部 resolved 槽：有效纹理 → 设置新 Brush、恢复 `HitTestInvisible`；空纹理 → 清空旧 Brush/ResourceObject 后设为 `Hidden`。禁止仅在非空时更新导致复用 Widget 保留上一张卡的纹理，也禁止使用 MatchSize 把 512/1024 素材原始像素尺寸写回布局。

卡图为空 → 清空旧 Brush，隐藏 `Img_CardArt`；卡图恢复 → 应用当前 frozen CardArt 并恢复可见。卡图 Alpha 不符合图框轮廓时不得关闭输入，但生产视觉 Gate 必须报告越界；不允许靠保留上一张卡的卡图或背景遮挡来掩盖错误。

---

## 15. WBP_BattleCard_Native 目标层级

### 15.1 目标层级与绘制顺序

保留 `SB_Card → Btn_Card` 的根尺寸、点击入口和动画锚点；内部结构如下，Canvas 子项用显式 ZOrder 保证绘制顺序：

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

`OV_Cost` 绘制在标题丝带和卡图之上；文字绘制在各自底板之上。`Img_CardFrame` 只包围卡图区域；红色卡身外沿属于 Background。类型标签紧贴卡图下沿，不占一行独立正文空间。

### 15.2 坐标、尺寸与 trim placement

根卡片固定布局尺寸：

```text
150 × 210
```

`150 × 210` 是 layout/geometry 尺寸；hover/选中/播放只沿用现有 render transform。费用球、丝带可超出根矩形绘制，不能扩大根 DesiredSize、手牌槽间距或动画锚点。父级卡面不设整树 `ClipToBounds`，避免切掉左上球和丝带；卡图裁剪单独处理。

本轮统一使用 §21 CFV-4 映射的 **512** Cropped 素材，保存它们的 trim placement。离线 authoring 换算规则：

```text
Source canvas = 512 × 512（仅适用于本轮选定且已检查 orig 的条目）
Canonical body origin O = (106, 46)   // 原画布左上坐标系
Canonical body size   = (300, 420)
Uniform scale S       = 0.5

TrimTopLeft = (offset.x, orig.height - offset.y - size.height)
LocalPosition = (TrimTopLeft - O) * S
LocalSize     = size * S
```

`offset` 是原画布中的裁切偏移，`xy` 是图集内坐标，不能混用。原画布 512 高不能直接当作卡身高；512/1024 条目也不能混用，例如 1024 orb 的 `orig` 与 512 orb 不同。坐标在配置中保存；运行时不解析 atlas，也不读取 PNG 来重建布局。

初始配置依据如下（这些是实施起点，尚未通过视觉 Gate；个别 0.5 px 差异以该资源的 atlas 原值为准）：

| 层 | 根卡片局部位置 / 尺寸 | 约束 |
|---|---|---|
| Background / Shadow | 由各自 trim 换算，主体约 `150 × 210` | 保持红色卡身比例；阴影只影响绘制 |
| Attack Frame | 约 `(9, 31)` / `131 × 92.5` | 保留下沿斜角；按 rarity 补偿 trim 差 |
| Skill Frame | 约 `(9, 30.5)` / `131.5 × 91.5` | 使用 Skill 图框自身轮廓 |
| Power Frame | 约 `(7.5, 3)` / `134.5 × 119` | 保留椭圆；上部被标题丝带叠压 |
| Banner | 约 `(-6, 5.5)` / `162 × 38.5` | 横跨卡身；不是正文背景 |
| Name | 初始 Rect `(22, 7, 106, 25)` | 居中，避开费用球；不随文字增长撑宽卡牌 |
| Cost Orb | 约 `(-9.5, -8.5)` / `36 × 35.5` | 左上角压住卡身与 Banner |
| Cost text | 居中于 Orb 的可见内圈 | 大号费用数字，无独立底色 |
| TypePlate | 初始中心约 `(75, 115)`，高 `11.5` | 按各类型图框下沿作局部微调 |
| Description | 初始 Rect `(17, 133, 116, 57)` | 位于下半部深色区，短文案垂直居中；多行自动换行 |

CFV-2 必须用三种生产 CardArt 完成各 `PortraitRect` / 内缘对齐和必要的局部微调，将最终 Rect 与 placement 记录为实施布局表；CFV-3 将这些值收敛到同一 CDO 配置，CFV-4 验证真实素材。记录与上述初值的差异。根尺寸、类型窗口语义和费用/标题的叠压关系不变；不是用一个 Frame Rect 拉伸全部类型。

### 15.3 卡图轮廓与 Alpha

本轮选择 **复用带 Alpha 的原卡图 + 类型专属 PortraitRect + 对应图框覆盖内缘**。已有 Strike / Defend / Inflame 的原始 portrait PNG 为 `500 × 380`，实际存在透明角和边缘；但仅存在 Alpha 通道不等于轮廓已与每种图框严格吻合。

- `Img_CardArt` 始终使用 frozen `FBattleHUDCardView.CardArt`，不得用 CardId/DisplayName 重新查图。
- 按类型 author PortraitRect，保留图中人物/主体，卡图的有效轮廓与图框内缘贴合；不能把整张卡片尺寸传给 CardArt 或 frame，也不能把所有类型的 portrait 强行套入同一窗口。
- 透明 frame 只能覆盖接缝，**不能裁掉窗口外的非透明像素**。CFV-2/4 必须同时检查透明区与可见内缘；若生产 UI portrait 仍越界，修正该 portrait 的 Alpha/裁切导入，不更改 Gameplay metadata 或 frozen identity。
- 本轮不增加通用 mask material / shader 系统。若现有源图无法通过局部 Alpha authoring 达到目标，应记录具体素材问题后修订该局部方案，不能宣称矩形 ClipToBounds 已实现椭圆裁剪。
- Status/Curse 使用 FallbackLayout（Skill 布局）与黑色背景；FallbackFrame 为空时只隐藏图框，卡图和文字仍可显示。该 fallback 不承诺未提供素材的完整原作 Status/Curse 外观。

### 15.4 类型标签与交互

类型标签：

```text
HB_CardTypePlate
├─ Img_TypeLeft   → 固定 7 × 11.5，Auto slot
├─ Img_TypeCenter → Fill，固定高 11.5，只横向伸展
└─ Img_TypeRight  → 固定 7.5 × 11.5，Auto slot
```

三段宽度由标签文字测量加固定 padding 得到；不让原始 Texture BrushSize 自动决定 UI 尺寸。图框自带的底部底座与 TypePlate 对齐，避免双重标签错位。标签文字使用既有中文类型文本。

装饰叶子设 `HitTestInvisible`；`OV_Card` / `CN_CardFace` 等祖先用 `SelfHitTestInvisible`，不能使 `Btn_Card` 连同子树失去命中。Button Normal/Hovered/Pressed 的默认矩形皮肤与 ContentPadding 不得盖住或挤动卡面；按现有输入协议保留反馈，状态反馈不能改写稀有度样式或升级标题颜色。历史播放卡继续由 HUD 设置整卡 `HitTestInvisible`。

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

最终生产树只能保留一套可见 CardArt/Name/Cost/Description；旧 `Img_CostBase` 如仍存在，应随费用球迁移清除或 Collapse，不与 `Img_CostOrb` 叠出两套底图。旧层删除以新层 Gate 为依据，不额外安排一次相同 PIE。CFV-2 只记录布局并保留可读中间态，CFV-3/4 才接管为 CDO 样式；不能让空配置先隐藏全部生产卡面后停工。

---

## 17. RefreshFromCardView 分层

为了保护已有 transient Card Probe 和 A3 测试，核心内容刷新与视觉刷新必须解耦。

推荐：

```text
RefreshFromCardView()
├─ ResolveCardFaceStyle(CurrentCardView.CardType, CurrentCardView.Rarity, Config)
├─ RefreshCoreCardContent()
│  ├─ Name
│  ├─ Cost
│  ├─ Description
│  ├─ Type
│  └─ RefreshCardArtwork(resolved PortraitRect + frozen CardArt)
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

两类刷新分别处理所需控件，decorative 的空引用/空纹理不得造成核心内容 early return。`SetCardView()` 无论在 Construct 前后发生均可在控件就绪后得到相同结果；同一 Widget 连续接收不同 DTO 时不得残留旧样式。`ApplyImmediatePreview` / `ClearImmediatePreview` 仍只更新描述，不能重建正式手牌、触发结构 `OnChanged` 或改变静态布局。历史卡只消费 frozen view。

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

中文标题、类型和正文使用同一简体中文字体族，费用数字保留参考图的大号数字风格。CFV-4 负责实际字体 authoring：

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

具体落点（字号为 `150 × 210` 卡面的初值，CFV-2/4 视觉排版可微调并记录最终值）：

| 控件 / 样式 | UE Font 包路径 | 初始字号 / 排版 |
|---|---|---|
| `Txt_CardName` | `/Game/SlayTheSpireDemo/UI/font/zhs/SourceHanSerifSC-Bold_Font` | 14，居中，深色 Outline 1；基态浅色，升级仍为 `#7FFF00` |
| `Txt_CardType` | `/Game/SlayTheSpireDemo/UI/font/zhs/SourceHanSerifSC-Bold_Font` | 8，居中，深灰文字，无粗描边 |
| `Txt_Cost` | `/Game/SlayTheSpireDemo/UI/font/Kreon-Bold_Font` | 23，居中，浅色数字、深色 Outline 1 |
| Description Default / numeric styles | `/Game/SlayTheSpireDemo/UI/font/zhs/SourceHanSerifSC-Medium_Font` | 11，浅米色正文，左对齐、在描述区域垂直居中、自动换行 |

富文本实际修改 `/Game/SlayTheSpireDemo/Data/DT_BattleCardTextStyles` 中的 Default、PreviewIncrease、PreviewDecrease 及已有 semantic rows，并检查 WBP 是否设置了覆盖字体的 DefaultTextStyle。只改 WBP 的外观属性不能代替修改被使用的表行。所有描述表行的 Font/Size/Outline/基线一致；保留现有逐数值增减颜色和 tag 名，不给整句话着色。

Name 单行必须容纳当前最长生产名称及 `+`；Description 必须容纳当前最长、多段生产描述。不要固定旧 `WrapTextAt = 100` 导致新描述区域仍按旧宽度换行，也不要为容纳文案自动改变卡牌外形。正文中 `造成6点伤害。` 的内容仍由 Gameplay 文本解析提供，不在 WBP 写死参考图文案。

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

CFV-4 必须按以下完整包路径加载实际生产资产，断言 CardId 对应、CardType 与 Rarity。不要用 transient fixture 的值代替资产 Gate：

| 生产 CardData 包路径 | CardType | Rarity |
|---|---|---|
| `/Game/SlayTheSpireDemo/Data/Cards/Ironclad/Attacks/DA_Card_Strike` | Attack | Basic |
| `/Game/SlayTheSpireDemo/Data/Cards/Ironclad/Skills/DA_Card_Defend` | Skill | Basic |
| `/Game/SlayTheSpireDemo/Data/Cards/Ironclad/Attacks/DA_Card_PommelStrike` | Attack | Common |
| `/Game/SlayTheSpireDemo/Data/Cards/Ironclad/Attacks/DA_Card_TwinStrike` | Attack | Common |
| `/Game/SlayTheSpireDemo/Data/Cards/Ironclad/Attacks/DA_Card_Uppercut` | Attack | Uncommon |
| `/Game/SlayTheSpireDemo/Data/Cards/Ironclad/powers/DA_Card_Inflame` | Power | Uncommon |

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

### CFV-1 AUTOMATED GATES

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

### CFV-1 MANUAL PIE GATES

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

调整 WBP hierarchy、图层 slot、portrait Alpha 对齐与文字排版初值，不实现 Style Resolver。用现有真实素材作 Designer 配置以保留可读中间态；最终纹理 authority 在 CFV-3/4 收敛为 CDO。

要求：

```text
优先移动已有 core widgets
保留现有 BindWidget 名称
新增 decorative widgets 使用 Img_ 前缀
旧 BG_Card/VB_CardContent 暂不立即删除
```

### CFV-2 AUTOMATED / EDITOR ASSET GATES

```text
WBP_BattleCard_Native Compile PASS
WBP_BattleCard_Native Save
core BindWidget contract intact
逐项核对目标容器和 decorative 控件的名称、类、父子关系及 ZOrder
根尺寸 150 × 210；旧层状态明确；同一核心控件不重复创建
保存三种类型的最终 PortraitRect / trim placements / 文本区域布局表
```

不运行 broad Gameplay regression。

### CFV-2 MANUAL PIE GATES

无独立 PIE Gate；布局在 CFV-5 集中作一次视觉验收。这里的 Designer authoring/Compile 不等于视觉验收通过。

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
RefreshCardArtwork() / 类型布局应用
new decorative BindWidgetOptional references
独立 CFV decorative test probe
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

新增最小 Widget 应用断言，纳入 `SlayTheSpireDemo.CFV.WidgetStyle`：

```text
同一 Widget: Rare Attack → Basic Skill → 无资源 → 恢复 Power
→ 各 Image Brush.ResourceObject 与当前配置严格一致
→ Hidden / HitTestInvisible 正确切换，旧 Brush 清空
→ 不残留前一张卡的 Frame / CardArt / Banner / TypePlate / placement

有效纹理不能通过 MatchSize 改变 150 × 210 根布局
缺少全部 optional 控件仍能刷新核心内容，并保留既有选牌请求边界
构造前/后 SetCardView 顺序不影响最终核心内容与样式
升级前后 Rarity / Frame / Banner / TypePlate / 布局不变，标题按 frozen bUpgraded 更新
Preview Apply/Clear 不更改静态样式、根尺寸或正式 Hand Widget identity
```

测试使用新 CFV probe，不给旧 A3/DTO probe 增加装饰控件前提。纹理指针断言不代替 Alpha 轮廓与最终排版的手动观察。

### CFV-3 AUTOMATED GATES

```text
Editor Build once
SlayTheSpireDemo.CFV.VisualResolver once
SlayTheSpireDemo.CFV.WidgetStyle once
Compile + Save + Reopen WBP_BattleCard_Native once after reflected config/bindings change
```

### CFV-3 MANUAL PIE GATES

```text
none required
```

上述三个 CFV Automation 前缀（含 CFV-1 RarityContract）均为待新增测试；实施时把实际测试文件、用例数和命令写入证据，不提前标为 PASS。

---

## CFV-4 — Production Asset Authoring

目标：

```text
把真实 Cropped 素材严格配置进 WBP_BattleCard_Native CDO
并 author 当前生产 CardData Rarity
```

### CFV-4 唯一生产资源表

以下是实际存在的 Cropped 512 PNG / `.uasset` 包路径。表内 PNG 尺寸只供 authoring 对照，不能用作运行时 DesiredSize。ObjectPath 在包路径后追加同名对象（例如 `.../card_red_orb.card_red_orb`）。

| 配置槽 | 完整 UE 包路径 | PNG 尺寸 |
|---|---|---|
| `AttackBackground` | `/Game/SlayTheSpireDemo/UI/Textures/CardUI/Cropped/cardui4/512/bg_attack_red` | 302 × 419 |
| `SkillBackground` | `/Game/SlayTheSpireDemo/UI/Textures/CardUI/Cropped/cardui4/512/bg_skill_red` | 299 × 419 |
| `PowerBackground` | `/Game/SlayTheSpireDemo/UI/Textures/CardUI/Cropped/cardui4/512/bg_power_red` | 299 × 419 |
| `FallbackBackground` | `/Game/SlayTheSpireDemo/UI/Textures/CardUI/Cropped/cardui4/512/bg_skill_black` | 299 × 419 |
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
| `CostOrb` | `/Game/SlayTheSpireDemo/UI/Textures/CardUI/Cropped/cardui/512/card_red_orb` | 72 × 71 |
| `CardShadow` | `/Game/SlayTheSpireDemo/UI/Textures/CardUI/Cropped/cardui4/512/card_shadow` | 300 × 420 |
| `FallbackFrame` | `nullptr` | 当前唯一允许空的生产 frame 槽 |

本轮不混用 1024 同名资源。纹理导入使用 UI 适用的压缩/过滤和保留 Alpha 的设置；透明边缘不得出现黑边或白边。全部配置纹理应与保存的 trim placement 同步校验。

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

### CFV-4 AUTOMATED / EDITOR ASSET GATES

新增 `SlayTheSpireDemo.CFV.AssetAuthoring`，一次验证：

- 生产类 `/Game/SlayTheSpireDemo/UI/Widgets/WBP_BattleCard_Native` 的 CDO 配置与上表完整路径逐项严格相等；同时核对 trim placements / 三类型布局值，不仅比较 short name 或 non-null。
- §20 六张真实 CardData 的 CardId/CardType/Rarity，以及 `UCardData::IsDataValid()` 结果。
- 生产 WBP 必须具备全部 core/decorative 控件及预期类型；核心绑定不能依赖新的 optional 控件，树中没有第二套可见旧卡面。
- §19 WBP Font、RichText style table 绑定及各表行的 Font/Size/Outline 一致性，保留现有语义颜色与 tag。
- CardData、纹理、字体、RichText 表先 Save；最后 WBP Compile → Save → Reopen。若 CFV-3 之后仅资产变化，不重复 Editor Build；新增此 Gate 的 C++ 测试必须包含在 CFV-3 build 批次，或因实际新增代码只补一次必要 Build。

测试中的精确资产加载属于 Editor-only 资产验收，不允许把测试路径加载逻辑搬入 runtime Resolver。

### CFV-4 MANUAL PIE GATES

无独立 PIE；三类卡图 Alpha 与排版在 CFV-5 一次观察。AssetAuthoring PASS 不等于视觉 Gate PASS。

---

## CFV-5 — Visual Acceptance

### CFV-5 AUTOMATED GATES

前置：CFV-1 RarityContract、CFV-3 VisualResolver/WidgetStyle、CFV-4 AssetAuthoring 均实际 PASS。复用有效证据；CFV-5 不为了最终编号重跑这些 Gate，也不重复手测九种类型×稀有度组合。

### CFV-5 MANUAL VISUAL / PIE GATES

#### A. Rare 只读 Designer 样本

现有生产卡没有 Rare，也没有 Uncommon Skill。使用真实生产卡验证类型，另用一个 detached frozen DTO 验证 Rare 静态外观，禁止为凑组合修改生产 CardData Rarity。

CFV-4 准备一个本地验收用 `UUserWidget` Blueprint：`/Game/SlayTheSpireDemo/UI/Validation/WBP_CFVCardFacePreview`。它不进入生产引用链，默认不提交、不打包；不引入新的插件或 Gameplay 调试 API。最小内容：

```text
Designer:
  PreviewCard = 一个 WBP_BattleCard_Native 子控件

Variable:
  PreviewCardView : FBattleHUDCardView（供该预览资产 Defaults 编辑）

PreConstruct:
  IsDesignTime → Branch True
      → PreviewCard.SetCardView(PreviewCardView)
  False → 无操作
```

不绑定选牌事件，不接入 HUD/Gameplay/Queue。预览值只属于这个隔离资产；生产 `WBP_BattleCard_Native` 不添加第二份 Rarity authority。

操作：打开该资产，配置 `CardType=Attack`、`Rarity=Rare`、`bUpgraded=false`、`RuntimeId=INDEX_NONE`、`bGameplayPlayable=false`、`DisplayName=打击`、`Cost=1`、`Description=造成6点伤害。`、`RichDescription` 为空、`CardArt=/Game/SlayTheSpireDemo/UI/Textures/red/attack/T_strike`，Compile/Save 后刷新 Designer。在 `150 × 210` 逻辑尺寸观察 Rare 图框、丝带和标签；确认装配与正文阅读区域正确。这里只验视觉，预览文案不进入生产卡牌文本。

#### B. 一次生产地图 focused PIE

资产/地图：

```text
/Game/SlayTheSpireDemo/UI/Widgets/WBP_BattleCard_Native
/Game/SlayTheSpireDemo/Maps/L_BattleTest
```

1. 记录地图中 BattleManager 的原 `DebugStartingDeck`、`OpeningHandDrawCount`、`DeckDebugSeed`；仅在未保存的关卡实例中临时将起始牌组设为 §20 的 Strike / Defend / Inflame 各一张，OpeningHandDrawCount=3，Seed=1337。保留 Native HUD 默认和已有 Gameplay 规则；不修改 CardData。开局必能观察三种实际类型，不依赖随机抽到测试卡。
2. 启动一次 PIE，在通常游戏窗口尺寸观察 **Strike=Attack+Basic、Defend=Skill+Basic、Inflame=Power+Uncommon**。对照参考图检查费用球/丝带叠压、红色卡身、卡图与图框内缘、类型标签落点、下半部正文和阴影；三种类型无卡图越界、拉伸、接缝漏底、文字裁切或旧层重复显示。
3. 玩家回合、队列空闲时，在 PIE 的 BattleManager Details 中调用现有 **Test Upgrade First Hand Card**（`ABattleManager::TestUpgradeFirstHandCard`，必要时先 Eject）。记录该首张卡身份。观察同一 runtime card 出现 `+` / `#7FFF00` 和升级数值，而 Frame/Banner/TypePlate/根尺寸不变；不直接写 `bUpgraded`。
4. 返回游戏控制，按现有合法选牌/选目标操作先打出 Inflame，再选择 Strike 并指向敌人。观察 Strength 支持的 A3 数值高亮、中文字体基线与换行；提交 Strike，观察 **Hand → PlayArea → Discard** 连贯移动。手牌与历史播放卡的图框、卡图、名称、尺寸保持一致，无起点跳动、A→B→A 闪回或双重卡面。之后用 Defend 的 Self 目标操作确认卡面重排后的点击/反馈仍正常。
5. 结束 PIE，将临时关卡属性恢复原值，确认没有把验收牌组保存进生产地图。记录最小视觉结果与升级卡身份；失败只给具体卡/步骤/观察，一张对应静态截图或一段针对移动问题的短观察即可，不重复截图取证。

最终检查包含当前最长生产名称（含 `+`）/多段描述的 Designer 排版，复用 CFV-2/4 authoring 观察，不为此追加完整战斗。Rare Designer 样本只关闭静态视觉 Gate；真实交互和历史播放仍由本次 PIE 关闭。

若不能用可用 UE 工具执行上述 Designer/PIE，明确标记 `USER ACTION REQUIRED` 并交付上述最小步骤；自动测试或静态截图不得替代未执行的出牌动画/交互观察。

```text
one Rare Designer observation + one focused production-map PIE pass
→ record actual evidence
→ restore temporary validation configuration
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

CardType ──────────────→ PortraitRect / type layout
CardArt ───────────────→ alpha-aware Artwork
Cost ─────────────────→ Cost text
fixed Ironclad theme ─→ CostOrb / Shadow
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
[x] STS reference image and target layering specified
[x] 512 atlas trim placement / type layout / portrait Alpha contract specified
[x] UMG target hierarchy specified
[x] Img_TypeLeft/Center/Right naming locked
[x] move-existing-widget migration rule locked
[x] CFV-1 narrow Level-2 validation strategy locked
[x] CFV-4 strict production asset mapping Gate locked
[x] exact 512 package paths / production CardData rarity assertions specified
[x] BindWidgetOptional / clear-hide-restore Widget application tests specified
[x] Chinese font / RichText style asset ownership specified
[x] actual production card samples / isolated Rare Designer sample specified
[x] one focused A3 → CardPlayed → Discard PIE path specified
[x] validation strategy revised; no execution evidence claimed

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
DESIGN REVISED — STS CARD-FACE REFERENCE
IMPLEMENTATION NOT AUTHORIZED
```

未经用户明确授权，不开始 CFV-1，也不自动进入后续阶段。
