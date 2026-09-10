# 扇形手牌与攻击指向交互

日期：2026-09-10。状态：**IMPLEMENTED / AUTOMATED GATES PASS / PIE PENDING / UNSEALED**。

本次用户授权：参照四张《杀戮尖塔》截图，实现扇形手牌、悬停抬起放大、单体攻击
灰/红曲线指向箭头，并允许从本地游戏目录导入需要的素材。本工作独立于尚未实现的 G8。

## 行为

- 普通手牌以冻结顺序居中成扇形，外侧牌稍低并倾斜；重叠宽度随可用宽度和数量收缩。
- 鼠标悬停卡牌抬起、转正、平滑放大到 1.35 倍并提高绘制层级；离开恢复扇形。
  待选目标的卡牌保持抬起，直到取消或提交进入 committed playback。
- 点击一张 `Attack + Enemy` 卡进入现有 ChoosingTarget 流程后出现分段曲线箭头。
  未指向合法敌人时灰色，指向合法敌人时红色；点击敌人仍走既有正式请求接口。
  自身目标、无目标/群体攻击、非攻击技能和消耗选择均不显示此攻击箭头。
- 普通选牌、选敌人或待确认状态下按鼠标右键会取消当前 UI 选择。Native HUD 在
  `PreviewMouseButtonDown` 先拦截右键，因此卡牌/敌人子控件不会吞掉取消；多选消耗
  复用已有的事务取消路径，并严格遵守 Gameplay 的 `ESelectionCancelPolicy`，强制选择
  不会被右键绕过。
- 取消、输入锁定、提交成功、终局或 ViewModel 失效时撤销箭头；不改变输入解锁时机，
  不增加 Gameplay 队列，不启用 G8。

## 实现与连续性合同

`UBattleHandFanPanel` 是 UMG Canvas 容器，在 Native HUD 初始化时替换原空水平 Hand
宿主。`HB_Hand` 的 C++ 控件类型拓宽为 `UPanelWidget`；原 Native 资产绑定名保留，
头部以外的 Blueprint 资产不改。Canvas children 仍是正式 Hand，而非另一套可见复制品。
历史 child count/index、RuntimeId 顺序、同对象 reconcile 和 Hidden 非 Hand 所有权
槽位全部保留。G5 SelectionArea 和 G6 transition 继续持有它们自己的精确对象。

扇形只负责正式 Hand 的 slot、角度、抬起和 scale。播放期间停止覆盖其 source/arrival
transform。消耗选择的 dim opacity 保持由现有选择逻辑负责，避免 scale 双写。
重叠牌的 hover 使用稳定的原位置横向区域，并让点击与该候选一致；不以放大后覆盖
邻牌的范围作为唯一 hover 来源。

箭头是独立 Native `UBattleTargetingArrowWidget`，由 Canvas + 私有 Images 组成，
整层 HitTestInvisible，不抢敌人和按钮的点击。以选中卡牌的实际 geometry 为起点，
指针为终点，沿三次曲线按弧长采样分段。颜色只使用当前已公开合法目标映射，不遍历
CardEffects 推导合法性，不按 CardId 分支。

右键取消有两层输入入口：`UBattleHUDWidgetBase::NativeOnPreviewMouseButtonDown`
覆盖 Slate 命中路径，`ABattleHUDPresenter` 推入 PlayerController 的全局输入组件覆盖
视口空白区域。两层最终都调用 Widget 的同一虚拟取消边界，不复制 ViewModel 状态，
也不会绕过 Gameplay 的取消策略；Presenter 输入绑定设置为不阻塞其他未相关输入。

## 素材来源

用户指定本地目录：
`E:/SteamLibrary/steamapps/common/SlayTheSpire/desktop-1.0/images/ui/combat/`。

| 原文件 | 导入 UE 路径 |
|---|---|
| `reticleArrow.png` | `/Game/SlayTheSpireDemo/UI/Textures/Targeting/T_reticleArrow` |
| `reticleBlock.png` | `/Game/SlayTheSpireDemo/UI/Textures/Targeting/T_reticleBlock` |

使用原灰度/透明纹理，以 UI tint 表示灰/红；没有重新生成或修改原图片。
使用 UE AssetImportTask 导入，UI texture group；运行时为明确资源引用，未读取外部
Steam 文件。导入记录：`Saved/Logs/HandFanImport.log`。Python 插件仅用于该命令行会话，
未修改 `.uproject` 或插件配置。右键预览事件只增加 Native 模块私有 `SlateCore` 链接，
不把 Slate 类型暴露到 Gameplay 公共边界；Legacy 资源未使用。

## 验证与验收

自动化计划：生产 Native 资产加载并实际安装扇形宿主；隐藏槽位/对象顺序；箭头适用
类型；受影响的 Native card transition、Preview、G0 Hand identity、G5/G6 选择回归。
结果按实际执行补充，不以源代码构造测试代替 Slate 视觉验收。

实际验证（本地未提交工作树，基线 `268e3006b8f04e924af6d09a020c45460b35c103`）：

- 最终标准 UE 5.8 工程生成和 Development Editor Win64 build PASS，exit 0：
  `Saved/Logs/HandInteractionVerifiedProjectFiles.log`、`HandInteractionVerifiedBuild.log`。
- 初始 `HandInteractionAutomation.log` 已完成 12 个 CardSelection.Presentation 测试和
  FanSlots（共 13 个成功），随后新测试重复初始化 World 导致中止，无完整 JSON。
  修复测试初始化后，`HandInteractionCompletionAutomation.log` 暴露无 LocalPlayer 时
  Native arrow 的 OnInitialized 不执行，测试的强制转换中止；已改为 Initialize 构建
  私有控件，并把测试改为受控空值断言。两次中止均没有被报告成完整 suite PASS。
- `Saved/AutomationReports/HandInteractionClosure/index.json`：18 项，17 成功（含一个
  既有非法 CardPlayed 诊断 warning 测试）、1 失败。失败为旧 R8 负例把当前已支持的
  Hand→Exhaust 当作不支持路径；经 HEAD 源码核实后改为实际不支持的 Exhaust→Hand。
  G6 四项、G0 HandIdentity/FormalSlotOwnership、Preview 三项及其余 R8 通过证据保留。
- 最终受影响范围：`SlayTheSpireDemo.HandInteraction`、
  `SlayTheSpireDemo.CardSelection.Presentation.G5`、
  `SlayTheSpireDemo.CardSelection.Presentation.Input`、
  `SlayTheSpireDemo.Phase6UIA2N.R8.Zone.HandToDiscardFinishCancelAndInvalid`。
  **11/11 PASS，0 warnings / failed / notRun，process exit 0**：
  `Saved/AutomationReports/HandInteractionVerified/index.json`、
  `Saved/Logs/HandInteractionVerifiedAutomation.log`。G5/Input 夹具现在实际使用 FanHand。
- 右键取消追加验证：标准工程生成和 Development Editor Win64 build PASS，exit 0，
  `Saved/Logs/RightClickGlobalFinalProjectFiles.log`、`Saved/Logs/RightClickGlobalFinalBuild.log`；
  `SlayTheSpireDemo.CardSelection.Presentation.Input.RightMouseButtonCancel` **1/1 PASS**，
  0 warnings/failed/notRun，process exit 0，`Saved/AutomationReports/RightMouseButtonCancel/index.json`。
  用例覆盖普通 Attack 选敌取消，以及强制消耗选择不绕过禁止取消策略。
- 右键变更后的 G5 + Input 受影响回归 **8/8 PASS**，0 warnings/failed/notRun，process
  exit 0，`Saved/AutomationReports/RightClickCancelSelectionRegression/index.json`。
- Presenter 全局输入组件安装回归与右键专项合计 **2 项成功**；该 Presenter 夹具保留
  两条既有 headless warning（空测试牌组、无游戏视口），没有失败或错误，
  `Saved/AutomationReports/RightClickGlobal/index.json`。
- `git diff --check` PASS。重叠范围不累计；headless 结果不证明实际悬停画面或 PIE 验收。

**USER ACTION REQUIRED（PIE）**：Native `L_BattleTest`。

1. 观察少量/多量手牌居中成扇形；依次悬停两端和重叠处，牌抬起、转正并放大，
   文字可读；移动至邻牌不持续抖动，点击与高亮牌相同。
2. 选单体攻击，指向空白为灰箭头，指向合法敌人为红箭头；点击敌人正常结算。
   右键取消后箭头消失；再次进入选敌时左键敌人正常结算；自身/无目标/非攻击牌不出现攻击箭头。
3. 选多张牌消耗、Warcry 放回牌堆、连续抽牌，验证没有跳回第一张、重复、隐藏槽位
   闪现或残留箭头；允许取消的选择可右键撤销，强制选择右键不应结束请求；G6 同时消耗保持正常。

本变更不包含拖拽出牌重写、敌人场景布局迁移或整套战斗界面重制。
