# Native card source and pointer selection amendment

Date: 2026-10-06. Implementation base: `de75414`, current branch unchanged.
User-authorized extension of amended G9-B. G9-C–F remain unstarted; default
activation and remaining mandatory-choice acceptance gates remain pending.

## Behavior and ownership

最新用户要求：点击和长按统一使用按钮按下的选牌入口，松开不会再次选牌
或确认出牌。既有指针、目标与右键取消协议沿用；当前证据与中文清单见
[点击／长按选牌](NativeCardPressSelection.md)。

CardPlayed starts at the visible source pose, established during preparation,
before the first Slate paint. Use current arranged Hand geometry (including
hover/pointer render transforms), never the fixed Hand fallback as a production
origin. Copy the center, rendered size and angle into a cosmetic receipt in HUD
coordinates. PlayArea is the committed visual owner after accepted preparation;
the exact historical Hand slot becomes Hidden. Existing Blocking timing,
reducer validation and frozen card faces remain unchanged.

Selecting a Skill or Power follows the mouse; an Attack with TargetType=None
uses the same presentation policy for future untargeted AOE content. This adds
no AOE Gameplay effect or card. Enemy-target Attacks retain their target arrow.
Right click cancels only the ordinary draft and immediately restores the fan
pose. Left click confirms Self through the existing player-target Request,
or None through the existing confirm Request. Enemy-target Skills still require
their legal enemy click. EndTurn and explicit controls retain their own routes.
Mandatory selection never enters this pointer flow.

The formal Hand Widget/slot/Slate tree stays attached. The fan panel reserves
only render transforms and paint layers for pointer-controlled cards; its
ordinary allotted-size arrangement and frozen order remain authoritative.
Hover cannot overwrite those transforms. A transparent HUD backdrop below all
authored controls gives blank-viewport clicks the same preview route without
adding a competing PlayerController left-click listener.

Confirmed FIFO entries optionally carry a UI-only visual origin. Gameplay and
ViewModel Request validation ignore that cosmetic data. A pointer-confirmed
card waits at its frozen center without energy/zone mutation, then transfers
its receipt to the single submitting Request. Battle, ViewModel, Controller,
session, card identity and the ordered input credential scope that receipt;
accepted CardPlayed consumes it into its exact existing playback token. Queue
clear drops all pending receipts; already submitted Gameplay keeps its normal
history. Busy retry preserves the same receipt. Rejected requests discard it.
Viewport changes rebase normalized centers; panel membership/base geometry is
never rewritten by input Tick. Missing production source geometry declines
through the existing presentation fallback rather than inventing a position.

## Execution evidence

The implementation is a local batch on base `de75414`, not a sealed G9 stage.
Prescribed bundled project generation PASS; final UE 5.8 Development Editor
Win64 build PASS (6.93 s). Logs: `Saved/Logs/G9PointerProjectFiles.log` and
`G9PointerFinalBuild.log`. Initial builds exposed local Slot-name shadowing and
a headless FGeometry constructor import; both were repaired without dependencies.
A later build was blocked by active editor Live Coding; closing this run's
editor permitted the prescribed build. Failed builds are not passing evidence.

Focused Automation initially selected 68 cases: HandInteraction, G9-B, G8-B,
FastInput, R8, frozen CardPlayed handoff, CardSelection.Presentation and affected
G0/G4/G5/G6/G7. It completed 57 (56 passed, one failed) before a turn-end GC
fixture crash; no complete report exported (`G9PointerAutomation.log`). The new
pointer fixture needed an explicitly unlocked input state. The turn-end fixture
now removes its raw capture delegate and shuts down its Controller/HUD before
storage/World teardown; its GC/cleanup case passes after this lifecycle repair.

Affected/unfinished 12-case rerun: 11 success, one fail (`G9PointerRepair`).
The real Native no-Tick fixture lacked initial allotted geometry; it now supplies
Slate allocation without calling NativeTick. After removing the remaining
non-fan fixed-origin fallback, the affected 16-case run had 12 success, one
expected warning and three failures (`G9PointerFinal`). All three were old R8
fixtures without retained source Slate/allocation; those fixtures now retain
their Hand Slate tree and provide source geometry. Final R8 is **5 success,
one expected warning, zero failures/notRun** (`G9PointerR8`). The Native no-Tick,
pointer source and FIFO cases passed in the 16-case run.

An additional request test covers Self/None with G9 both enabled and disabled,
ordinary Gameplay cost/effect/consumption and repeated confirmation. Its initial
None fixture incorrectly authored a Self-only Block effect; corrected authored
data passes **1/1** (`G9PointerConfirmFinal`). The source test also checks missing
geometry declines with no PlayArea child, hidden source or energy mutation,
followed by first-paint pose and exact-token completion. Final receipt is below.
Reports live in `Saved/AutomationReports/<name>/index.json`; logs use matching
`Saved/Logs/<name>Automation.log`. Passing scopes are reused only when unaffected.
There is no claim of one uninterrupted aggregate run.

## Native PIE and remaining visual gate

Actual production map: `/Game/SlayTheSpireDemo/Maps/L_Battle_RuinedCitadel`, Native
asset parent Selection HUD, D3D12, ordinary 0.5 s Blocking card timing. C++ G9
default remains false; this preserved user asset/live run uses its existing true
option. User asset SHA256 remained
`F896F7B59A91D4EC86AD73973451E19012B7523EE7D1D1ECEE2B570894E20F81`.
No asset/config/plugin/Legacy changes are part of this batch.

MCP started actual PIE; Windows control selected Warcry. VM readback proves its
draft (`ChoosingTarget`, RuntimeId 11, Energy 5, unlocked). Native fan/draft and
later PlayArea cards were visible. Other continuous inputs changed the battle
and PIE subsequently stopped, so those observations do not prove a controlled
pointer/queue motion timeline. Zero CardPlayedReject entries occurred in that
run. Evidence: `Saved/Logs/G9PointerPIE.log`, `G9PointerStartPIE.json`,
`G9PointerBindings.json`, `G9PointerSkillDraft.json` and tool screenshots in this
conversation. The final non-fan contract tightening does not replace missing
visual proof. Editor is closed after the required final build.

## 中文人工验收清单

2026-10-06 用户反馈：1、3、4、6、7 没有问题；2、5 的原标准通过。
记录为 **用户人工反馈通过（USER_REPORTED_PASS）**，实际提交／配置／视口
未另行提供。下面保留原清单供追溯；新增“持牌期间其他牌不突出”以及
“攻击瞄准期间也抑制悬停”和“结束回合保留此前确认牌、只屏蔽后续追加”的当前验收见
[持牌悬停与结束回合修订](NativeInputHoverAndEndTurnRevision.md)。
原项目 7 的提前结束回合时间线按最新队尾规则恢复适用；复验仍以最新清单为准。

**历史操作清单，原标准已获用户反馈通过。** 实现提交为 `792f6e2`；本次清单
中文化仅修改文档，不改变运行行为，也不将待办标记为通过。

打开生产地图 `/Game/SlayTheSpireDemo/Maps/L_Battle_RuinedCitadel`，进入 PIE。
使用 Native HUD；排队项目需要实际启用 `Enable G9 Buffered Player Input`。
C++ 默认仍为关闭，不要仅凭按钮或悬停现象推断该开关已启用。记录实际
运行提交、开关状态和视口尺寸。每项保留完整操作录像；静态截图不能证明
出牌起点和连续轨迹。

| 项目 | 操作步骤 | 通过条件 |
|---|---|---|
| 1. 攻击牌实际起点 | 悬停并选中 Strike，再左键点击敌人出牌。 | 保留目标箭头；牌从刚才可见的手牌位置连续移向出牌区，没有从固定位置闪现、重复牌或幽灵牌。 |
| 2. 技能／能力牌跟随 | 分别选中 Warcry 和 Inflame，将鼠标移动到战场内两个不同位置。 | 选中牌跟随鼠标；其余手牌保留正常扇形顺序，没有一起移动或叠到左下角。 |
| 3. 右键归位 | 在项目 2 中，移动后按右键。 | 同一张牌立即回到原来的扇形槽位；未扣能量、未执行效果，也未进入弃牌／消耗堆；后续仍能悬停和选中。 |
| 4. 从鼠标位置出牌 | 再次选中上述牌，移动到不同位置，在战场空白处左键确认。Warcry 后续的强制选牌按正常流程完成。 | 牌从确认时的位置连续进入出牌区；只执行一次效果和一次对应扣费。需要敌人目标的技能仍必须点击合法敌人。 |
| 5. 排队起点与顺序 | 打出 A（例如 PommelStrike），在其播放期间确认 B（例如 Inflame）；可再确认 C（例如 Warcry），然后移开鼠标。 | 当前动画不中断；确认时不提前扣费或执行效果；B 保留确认位置，轮到它时自动从该位置出牌；C 按确认顺序执行。Warcry 产生强制选牌时进入正常选择流程。 |
| 6. 视口变化与后续交互 | 分别在跟随鼠标、排队等待和出牌运动期间调整窗口／视口大小，再操作剩余手牌。 | 本次演出没有新增的起点跳变、错位、重复或残留；剩余手牌顺序正确，悬停、选中及右键取消仍正常。历史扇形裁切调优不在本次验收范围。 |
| 7. 强制选择清空旧输入 | 按下述时间线，先确认后续牌和结束回合，再让 Warcry 进入强制选择。 | 普通鼠标跟随／空白处左键不能绕过强制选择；结束回合不可点击；未执行的后续牌和旧结束回合意图被清空，完成选择后不恢复。 |

项目 7 是前一轮仍未完成的完整时间线，具体操作为：

1. 打出 PommelStrike，在其历史播放期间选中 Warcry，左键确认；随后
   将另一张攻击牌按原目标点击流程确认入队，再点击结束回合。
2. 等到 Warcry 的强制置顶选择出现。此时结束回合保持不可点击，不能通过
   普通出牌输入继续执行那张排队攻击牌。
3. 完成选择时，选择另一张牌，**不要选择刚才排队的攻击牌**，以免混淆
   “被选择效果移出手牌”和“排队命令执行”的结果。
4. 等待演出完成。那张攻击牌仍在手牌中，没有自动攻击旧目标；旧结束回合
   不自动执行，仍处于同一个玩家回合。原有证据见 `G9BRevisionNativePIE.md`。

反馈格式：`项目编号：通过／失败；操作用牌；实际开关；视口尺寸；现象；录像`。
失败时说明卡牌在何处出现、是否重复扣费／出牌，以及能否继续操作。
未来 AOE 卡牌尚未实装，其无目标跟随策略已有自动化，本次不要求验证不存在
的内容。以上待办通过前不启用 G9 默认，不进入 C–F，也不宣称 G9 封板。

Final source/decline receipt: **1/1 PASS**, zero failures/notRun
(`G9PointerSourceFinal`). Across the initial scope, repaired affected runs and
the added request test, **69 distinct tests have valid passing evidence**.
This is not a single 69/69 run. `Saved/G9PointerPassingCases.txt` lists the paths.
Status: **IMPLEMENTED / BUILD PASS / AFFECTED AUTOMATION PASS / PARTIAL PIE /
USER ACTION REQUIRED / NOT SEALED**.
