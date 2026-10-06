# Checkpoint — Native pointer/source extension of amended G9-B

## 当前继续点：持牌悬停与结束回合修订

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
