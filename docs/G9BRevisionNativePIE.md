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

## Pending gate — USER ACTION REQUIRED

Complete one natural Native time line with a known pending FIFO and EndTurn
marker before mandatory choice. Attempts using MCP Slate refs after window
activation returned true without actually delivering the intended clicks; their
VM readbacks remained Idle. These attempts are not a PASS or a runtime failure.
Screenshot-backed input successfully proved the narrower mandatory surface and
discard/relic gates. The short complete queued-mandatory time line remains
unperformed; automation already proves its identities and clearing behavior.

On the same production map and G9=true:

1. Play PommelStrike, then while its history is playing confirm Warcry against
   the player, confirm another attack against the enemy, and click EndTurn.
2. When Warcry reaches its mandatory put-on-top choice, EndTurn must remain
   unavailable. The later attack and old EndTurn intent must be retired.
3. Finish the choice and wait for playback. The later attack must remain in Hand,
   no old enemy target may fire, and the same player turn must remain active.
4. Record actual HEAD, G9 flag and a short video or before/during/after captures.

Only after this gate passes may a separate tested C++ default-enable commit
start. Original G9-C–F and whole-G9 seal remain outside this amendment. PIE was
stopped and its observer removed; the editor remains open for this check.
