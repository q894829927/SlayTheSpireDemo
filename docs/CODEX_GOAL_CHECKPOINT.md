# Checkpoint — Native pointer/source extension of amended G9-B

## 当前继续点：第二回合连续出牌修复

基线 `6b0ab9c`，继续当前分支。本检查点随独立本地提交
`fix(g9-b): retire completed end-turn input fences` 保存；用
`git log -1 --format='%h %s'` 核实精确最新 HEAD。

完成：旧结束凭据在同 Battle 的不同 PlayerTurnSerial 达到精确正常 Ready
显示边界时退役一次，后续出牌忙碌不能恢复旧阻断。保留相同回合／旧演出
窗口防重、当前队尾规则和强制选择清空，没有 Tick 消费或 Gameplay 改动。

新回归在旧实现上单项复现失败；修复后规定工程生成／Editor 构建通过，
54.56 秒。一次聚焦 54 项为 53 成功、1 预期 R8 警告、0 失败／未运行。
覆盖有历史和无历史模式的第二、第三回合连续 A／B／C 与队尾结束一次。
准确范围、日志和第二／第三回合中文清单在 `G9NextTurnInputFenceFix.md`。

生产 Native 第二回合连续多次出牌及第三回合继续两张攻击实际通过，VM
均恢复 Idle／解锁且反馈为空。实例 G9=true，普通连续点击不代替动画期间
B／C 完整排队录像；该轨迹及队列后结束仍待人工复验。PIE 停止、观察器
移除、编辑器关闭。配置、数值、截图记录与限制均在专用文档。

下一步：按 `G9NextTurnInputFenceFix.md` 中文 A／B／C 清单取得播放期间
完整排队与队尾录像；此前其他未完成的时间线按最新输入修订清单继续。
C++ 默认仍 false，不进入 C–F、不启用默认、不封板。用户 Native 资产
SHA256 保持原状并排除；不 push。以下均为此前执行历史，不是当前 HEAD
或下一步。

## 当前继续点：攻击瞄准与结束回合队尾澄清

本批起点 HEAD `925aff2`，继续分支 `codex/g9-buffered-input-detached-cards`。
本检查点随独立本地提交
`fix(g9-b): preserve pre-end-turn plays and suppress aiming hover` 保存，精确
最新提交用 `git log -1 --format='%h %s'` 核实。

已完成：所有普通选中牌（包括 Enemy 攻击瞄准）都抑制其他牌突出，保留
静止命中切换和选中攻击自身抬升／箭头；结束回合保留此前确认 FIFO 和
已取出但忙碌的早期精确重试，禁止屏障后追加，只取消未确认草稿；此前
命令完成后结束一次。提交回合凭据继续防发布重入／旧历史重复点击；
强制选择清空未执行 FIFO、草稿和旧结束意图。上一批取消确认牌约定失效。

规定工程生成／Development Editor 构建通过：初次 6.23 秒，最终 5.85 秒。
聚焦 53 项为 52 成功、1 预期 R8 警告、0 失败／未运行；最终保留命中切换
后，仅受影响 HandInteraction／G9B.StableHandAndHover 8/8 通过。53 个不同
测试均有有效通过证据，没有最终版本一次 53/53 运行的声明。

生产 Native PIE 打击瞄准、移到邻牌仍不突出、右键取消恢复普通悬停通过。
实例读回 G9=true，C++ 默认 false。完整队尾轨迹的实际操作未受控，仍待
人工验收。PIE 已停止并读回 false，观察器移除，编辑器保持打开。

下一步：使用 `docs/NativeInputHoverAndEndTurnRevision.md` 文末最新中文
B／C／D 清单复验队列先完成再结束、重复点击完整阶段矩阵、战吼强制选择
清空后续确认牌与旧结束意图。A 的打击窄场景已通过。旧取消队列清单不再
执行；不进入 C–F，不启用默认或封板。完整执行范围、日志、配置及限制均
在专用文档。用户 Native 资产 SHA256 保持原状，仍为唯一排除的外部改动；
没有资产／配置／Legacy／插件／依赖修改，没有 push。

以下为上一批的历史继续点，不再代表当前行为或待办：

## 历史继续点：持牌悬停与结束回合修订（925aff2）

本批起点 HEAD `fee0790`，源实现 `792f6e2`。本检查点随独立本地提交
`fix(g9-b): cancel queued plays and fence repeated end turn` 保存，精确最新
提交用 `git log -1 --format='%h %s'` 核实。

已完成：持牌期间禁止其他手牌悬停突出；结束回合验权后取消未执行 FIFO；
取出结束意图前保存精确提交凭据，防止旧显示窗口接受未来回合命令；
Pending 清理保持凭据，当前拒绝回执可释放自己的凭据。没有 Tick 消费或
时间冷却。原清单 1／3／4／6／7、2／5 原标准已获用户反馈通过；新增规则
取代旧“结束回合保留确认牌”约定。

本轮工程生成／Editor 构建通过：初次 75.99 秒，最终夹具构建 6.51 秒。
聚焦 77 项：75 成功、1 预期警告、1 失败；旧 Hand→Draw 起点布局夹具
修复后仅该项 1/1 通过，77 个不同测试均有有效通过证据。没有一次
77/77 运行的声明。Native 开场、G9=true 读回及快速连续三次点击只结束
一次的窄场景通过。PIE 停止、观察器移除、编辑器保持打开。

下一步：按 `docs/NativeInputHoverAndEndTurnRevision.md` 的中文 A／B／C／D
清单获取新增视觉反馈。旧第 7 项“提前结束回合仍执行 Warcry”步骤已被
取消队列规则取代，不能继续使用。默认仍关闭；不进入 C–F，不封板。
完整证据与失败／修复记录见该专用文档。原 Native 资产修改及 SHA256 均
保持原状，不纳入提交；没有 push。

下文为之前源实现批次的历史继续点，不代表本轮仍需重跑的范围。

Branch: `codex/g9-buffered-input-detached-cards`. Implementation HEAD: `792f6e2`
(`feat(native-ui): play cards from visible source and follow pointer drafts`),
based on `de75414`. The subsequent documentation-only batch translates the
pending checklists into Chinese and records the future language rule in root
AGENTS.md. Resolve the latest documentation receipt with `git log -1 --format='%h %s'`.
Authority: `docs/NativePointerCardPresentation.md`, alongside the existing
confirmed FIFO / relic-tail / simultaneous-discard amendment.

Completed in this batch: synchronous proven CardPlayed first pose, no fixed-origin
fallback; Skill/Power/untargeted-Attack pointer render transforms, immediate right
cancel, Self/None left confirmation through normal Requests; optional queued
cosmetic origins and per-submission receipt with identity/session checks. Formal
Hand identity, slot/base geometry, Gameplay, history and Blocking ownership stay
in their existing owners. No queued input is consumed by NativeTick.

Prescribed UE 5.8 project generation and final Development Editor build PASS,
6.93 s. 69 distinct tests have valid passing evidence across initial/affected
repair runs. Final source/decline 1/1, Self/None normal+G9 Request 1/1, affected
R8 5 success plus one expected warning. Complete failures, repairs, exact scope
and report/log names: `docs/NativePointerCardPresentation.md`.

MCP actually started production Native PIE. Draft readback and fan/PlayArea were
observed, but additional inputs and PIE stopping prevented controlled motion
proof. USER ACTION REQUIRED: the four recording checks in the dedicated document
(Hand-origin Attack; Skill/Power follow, right cancel and left play; FIFO source
and viewport continuity; mandatory isolation). Editor is closed after build.

Earlier commits remain valid: `6071c56` relic tails, `b105fb1` confirmed FIFO,
`72a564a` simultaneous discard, `de75414` partial Native PIE receipt. Its full
queued-mandatory clearing gate is still pending in `G9BRevisionNativePIE.md`.
Obtain pending visual receipts before the independent default-enable batch.
G9 C++ default remains false. Original C–F remain unstarted; no G9 seal.

Preserve/exclude the externally saved Native HUD Content modification. Its
SHA256 still matches the pre-batch user asset. No assets, Config, Legacy,
plugins, dependencies or generated/local files belong in this commit. No push.
