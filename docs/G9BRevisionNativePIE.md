# Amended G9-B Native PIE — 2026-10-06

Source HEAD: `72a564a` (`codex/g9-buffered-input-detached-cards`). UE 5.8
Development Editor, D3D12, production
`/Game/SlayTheSpireDemo/Maps/L_Battle_RuinedCitadel`. Native Blueprint parent
read back as `/Script/SlayTheSpireDemo.BattleHUDSelectionWidget`; live Widget is
`WBP_BattleHUD_Native_C_0`. No Legacy runtime target, asset edits, saves, clock
changes or animation-duration changes were used. Editor log:
`Saved/Logs/G9RevisionPIE.log`.

The externally saved user asset already has `bEnableG9BufferedPlayerInput=true`;
both its CDO and live HUD read back true. This is the tested opt-in configuration,
not proof of default cutover: the C++ startup default remains false. The user
asset is excluded from implementation commits. Readback receipts:
`Saved/G9RevisionStartupConfig.json`, `G9RevisionRuntimeRefs.json`,
`G9RevisionNativeTree.json`, `G9RevisionHUDCDO.json`. These partial visual results
do not yet satisfy the default-enable gate.

## Performed gates

- **Confirmed automatic FIFO — PASS.** Start with Strike #4, PommelStrike #6 and
  #2. During Blocking history, hover/click each later card and click the frozen
  enemy target. The script's first VM read is Resolving/input-locked; after both
  confirmations it remains Resolving. Requests subsequently occur once in the
  order #4, #6, #2, with completed earlier history between them. Final VM: Idle,
  input unlocked, Energy 2, enemy HP 126, three discards. No further confirmation
  or Skip was used. `Saved/G9RevisionQueuePIE.json`,
  `G9RevisionQueueFinalVM.json`, `G9RevisionQueueDuring.png`.
- **Queued EndTurn tail — PASS.** Fresh PIE, same three confirmed cards, then
  EndTurn while Resolving. Request log: #4 at 18:28:18.507 UTC, #6 at
  18:28:19.844, #2 at 18:28:22.512; EndTurn at 18:28:25.177. EndTurn freezes six
  remaining Hand cards only after these plays. Captured VM edges show Energy
  3 -> 2 -> 0, Hand 6 -> 0, then next-turn draws. Final Idle/unlocked readback
  has Energy 5 and revision 12. `Saved/G9RevisionEndTailPIE.json`,
  `G9RevisionEndTailFinalVM.json`. Numerical exactly-once identity is also proved
  by Automation; this observation proves the real Native interaction sequence.
- **Simultaneous turn-end discard — PASS for the amended behavior.** Real
  screenshot-backed Windows input ended a fresh five-card Hand. Six consecutive
  computer-use captures show all five cards leave their individual fan positions
  together, become upright moving clones, follow separate converging paths and
  fade at the discard pile within the existing 0.5 s interval. These captures
  are displayed in the task's tool results. Later Hand draws are clean. This
  confirms simultaneous motion, not a new acceptance of historical fan/clipping
  tuning: the editor's small game viewport crops the lower idle-card region.
- **Mandatory isolation surface — PASS, narrower than the full clearing gate.**
  BurningPact enters its real mandatory exhaust choice. Native EndTurn is dimmed
  and unavailable; VM `bCanEndTurn=false`, Resolving, Energy 4. Selecting Defend
  hands the same visual to SelectionArea, then confirmation continues normally.
  `Saved/G9RevisionForcedChoiceVM.json` and computer-use captures. No visual
  claim of a pre-existing FIFO/EndTurn marker being cleared is made here.
- **Draw-two/shuffle/relic tail — PASS.** At DrawCount 1 / DiscardCount 5,
  BurningPact exhausts Defend. It draws TrueGrit, shuffles, draws Strike, then
  completes PlayArea -> DiscardPile. The live VM capture reaches Hand 5 with
  Block still 0; Abacus grants Block 6 afterward, with Sundial counter 1 shown
  at catch-up. The ActionQueue log independently records second draw and card
  destination before both counter actions and GainBlock. Evidence:
  `Saved/G9RevisionRelicCapture.json`, `G9RevisionRelicTailCapture.json`,
  `G9RevisionRelicFrame2.png`, `G9RevisionRelicFrame6.png`,
  `G9RevisionRelicFrame9.png`, `G9RevisionRelicTail0.png`. Sundial's energy-threshold
  exactly-once/order guarantees remain Automation evidence; no third-shuffle
  energy reward is claimed from this one-shuffle visual run.

## 待人工验收：强制选择清空队列（USER ACTION REQUIRED）

最新澄清：结束回合再次保留此前已确认牌，只阻止之后追加，因此下面的
Warcry 时间线恢复适用。当前验收以专用文档最新清单为准；下面关于取消
队列的说明仅为上一批历史记录。

更新（2026-10-06）：用户反馈上一清单项目 7 没有问题，记录为用户人工
通过反馈，未补写缺失的实际运行配置。下列时间线是旧“结束回合保留确认
队列”行为的历史清单，已被最新取消规则取代。当前操作请使用
[最新中文验收清单](NativeInputHoverAndEndTurnRevision.md)，不要按旧时间线
期待已被结束回合取消的 Warcry 自动执行。

仍需完成一条完整的 Native 时间线：在强制选择出现前，确实存在已确认的
出牌队列和结束回合标记。历史 MCP 点击尝试返回成功，但 ViewModel 仍为 Idle，
因此既不能计为通过，也不能据此判断运行时失败。较窄的强制选择隔离、同时
弃牌和遗物表现已有证据；完整清空时间线仍待人工确认。

在生产地图 `/Game/SlayTheSpireDemo/Maps/L_Battle_RuinedCitadel` 开启 G9 后：

1. 打出 PommelStrike。在其播放期间确认 Warcry，再将另一张攻击牌按敌人
   目标点击流程确认入队，随后点击结束回合。`792f6e2` 起 Warcry 支持选中后
   在空白处左键确认；原玩家目标点击入口仍有效。
2. Warcry 的强制置顶选择出现时，结束回合保持不可点击；后续攻击牌及旧
   结束回合意图应被清空，普通出牌输入不能绕过当前选择。
3. 选择另一张牌完成强制选择，不要选择刚才排队的攻击牌。等待演出结束，
   确认那张攻击牌仍在手牌，没有自动攻击旧目标，也没有自动结束当前玩家回合。
4. 记录实际提交、G9 开关、视口尺寸及完整操作录像，反馈通过／失败和具体现象。

当前完整中文清单见 [Native 出牌起点与鼠标跟随验收](NativePointerCardPresentation.md)。
此门槛通过后才可开始独立的默认启用提交。G9-C–F 和整体封板仍不在本轮范围内。
历史验收记录中的编辑器状态仅描述当时环境；当前状态以检查点为准。
