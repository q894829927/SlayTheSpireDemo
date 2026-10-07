# G9-B revision execution

2026-10-07：用户确认修订后的 G9-B 全部人工验收完成，当前门槛已关闭；
独立默认启用批次见 [G9-B 收口](G9BClosure.md)。下文各批待验状态为历史。

Authority: [confirmed plan](QueuedCardPlayAndRelicTimingAmendment.md).
Branch: `codex/g9-buffered-input-detached-cards`; base `fe80565`.
Contract amendment commit: `fc0de47`. Original G9-C–F are outside this task.

## Batch 2 — relic scheduling

Source under test: `fc0de47` plus the relic scheduling batch. Bundled .NET 10
project generation PASS (`Saved/Logs/G9RelicTailProjectFiles.log`); prescribed
UE 5.8 Development Editor build PASS, 247.32 seconds
(`Saved/Logs/G9RelicTailBuild.log`). Focused Automation used:
`SlayTheSpireDemo.Phase6A+SlayTheSpireDemo.Phase6C+SlayTheSpireDemo.Phase7`.
Actual report: **54 cases: 28 Success, 26 SuccessWithWarnings, 0 Fail, 0 NotRun**
(`Saved/AutomationReports/G9RelicTail/index.json`, log
`Saved/Logs/G9RelicTailAutomation.log`). Warnings include intentional rejection
and fail-soft scenarios; the report records their individual events.

The real draw-two test runs with and without committed recording. Status sees
the shuffled pile before retry; Relic sees the completed draw. The second draw
and PlayArea destination precede both relic rewards; threshold energy makes the
next card request legal. Multiple same-command shuffle events retain event
order and reward exactly once; combined insertion rejects malformed/cross-end
duplicates without modifying either end. Existing stale membership, frozen
reward, threshold insertion fault and deterministic ordering tests pass. Mixed
source execution assertions now require Status front / Relic back; synchronous
eligibility trace retains its original deterministic sorting.

This batch changes actual Gameplay reward timing and recorded ordering. It does
not add Presentation waits to Gameplay. Relic count actions execute at the tail;
their frozen compound rewards remain consecutive. Future player plays remain
outside the Gameplay queue (FIFO batch follows).

Manual production-map relic timing: **USER ACTION REQUIRED**, pending the final
focused PIE pass. No visual PASS claimed. G9 stays OPT-IN / NOT SEALED.

An existing editor session was stopped via MCP. Its unsaved Native HUD asset
was subsequently saved and the editor closed externally; that user asset change
is preserved and excluded from these commits. No Content/Config change is part
of this implementation or its validation claims.

## Batch 3 — confirmed play FIFO

Source under test: `6071c56` plus this FIFO batch. The HUD now owns a frozen
target draft, up to 32 unique confirmed plays, and a same-turn EndTurn marker.
Enemy/Self targets and None confirmation can be completed during Blocking
playback. No resources or live binding refresh occur at enqueue time. One
request is popped before publication and submitted at each fully caught-up
boundary. Invalid items report feedback and skip; terminal/identity changes
clear all. Explicit Skip, recovery, replacement, disablement and mandatory
Selection clear pending inputs. Ordinary DirectBaseline refresh does not.

EndTurn preserves the existing express same-turn legal-boundary behavior when
there is no preceding submitted queued play. A submitted queued play adds a
history-completion obligation, so its EndTurn marker waits for that card's
Gameplay, relics and Blocking history. The temporary draft is retired only
after EndTurn permission is accepted. No NativeTick drain or FastInput Skip is
used while G9 is enabled; disabled behavior is unchanged.

Bundled project generation PASS (`Saved/Logs/G9QueueProjectFiles.log`). Initial
Development Editor build PASS, 321.88 s (`G9QueueBuild.log`); final affected repair
build PASS, 237.72 s (`Saved/Logs/G9QueueBoundaryBuild.log`). Initial Automation
scope: G9 + G8B + FastInput + HandInteraction + R8 + G6 + G4 + G5 + G0 +
UIA3.CardPlayedRichHandoff, **60 cases: 58 Success, one expected warning, one
failure** (`Saved/AutomationReports/G9ConfirmedQueue/index.json`). The failure
exposed the distinction between ordinary direct refresh and destructive
recovery. An intermediate 30-case rerun exposed the express-EndTurn boundary
and an incorrect sole-enemy-death test expectation. Both are corrected.

Final affected rerun G9 + G8B + FastInput: **30 Success / zero Fail or NotRun**
(`Saved/AutomationReports/G9QueueBoundary/index.json`,
`Saved/Logs/G9QueueBoundaryAutomation.log`). This includes a new removed-card /
dead-target test. Together with unaffected first-run results, 61 distinct cases
have valid passing evidence; this is not a claimed single 61-case execution.
Tests prove B/C confirmation and automatic order, exact frozen targets, one
submission under publication reentry, duplicate rejection, skip feedback,
EndTurn after confirmed plays, mandatory isolation, disable/Skip/replacement,
turn ABA, removed cards, terminal target death and disabled G8-B fallback.

Production-map FIFO/EndTurn/mandatory visual timelines remain **USER ACTION
REQUIRED**, pending the combined final PIE pass. No visual PASS, default
activation or G9 seal is claimed. TurnEndDiscard implementation follows below.

## Batch 4 — simultaneous turn-end discard, 2026-10-06

Source under test: `b105fb1` plus this batch. Turn-end freezes the complete Hand
RuntimeId order, declares explicit `TurnEndDiscard` metadata and stamps only its
discard Actions. The shared Controller reducer preflight and Native N-child
engine now support formal Hand sources, without Selection generations. All
clones and geometry prepare before activation. They start together from their
individual fan positions; reducer commits remain chronological. Exact active /
visually-consumed Group records suppress future Hand members through refresh.
Viewport changes rebase each path from its frozen source to the current pile.
Incomplete/interfered/unavailable groups decline to serial playback. Single-card
Blocking completion retains its existing Collapsed behavior; grouped historical
slots remain Hidden. Disable, Skip, timeout, new Battle, Widget replacement and
destruction retire every child. Explicit input clear also advances the binding
generation, preventing a popped Busy result from restoring an old queue.

Final prescribed generation PASS (`Saved/Logs/G9TurnEndG2ProjectFiles.log`),
Development Editor build PASS, 4.47 s (`Saved/Logs/G9TurnEndG2Build.log`), after
the runtime repair build PASS, 39.61 s (`G9TurnEndGroupRepairBuild.log`). An early
build was aborted after a concurrent header-comment edit invalidated generated
macro line numbers; the stable rebuild then exposed test-module geometry linker
imports. Test-only geometry injection now stays in the existing runtime SlateCore
boundary, with no dependency change. Neither failed build is passing evidence.

Initial Automation selected 94 cases: SelectionPresentation +
CardSelection.Presentation + Phase6UIA2N.R8 + Phase6UIA2N.FastInput +
HandInteraction + UIA3.CardPlayedRichHandoff. It completed 83 (79 Success,
4 Fail) before a fixture GC crash; no complete report exported
(`Saved/Logs/G9TurnEndGroupAutomation.log`). Three failures exposed the singleton
visibility difference; one exposed incomplete historical Damage fixture fields.
The native fixture now roots only its external dependencies, not moving children.

Affected/unfinished rerun: CardSelection.Presentation + SelectionPresentation.G2
+ SelectionPresentation.G9B + Phase6UIA2N.R8 + UIA3.CardPlayedRichHandoff,
**43 cases: 41 Success, one expected R8 warning, one Fail, zero NotRun**
(`Saved/AutomationReports/G9TurnEndGroupRepair/index.json`). The remaining old G2
fixture also needed valid MaxHP; final G2 + TurnEndDiscard.Semantics rerun is
**7/7 Success, zero Fail/NotRun** (`Saved/AutomationReports/G9TurnEndG2/index.json`).
All 94 distinct selected cases now have valid passing evidence across these
runs; this is not a single 94/94 run. `G9TurnEndGroupPassingCases.txt` lists paths.

New tests prove complete/missing/duplicate/wrong-order/wrong-destination and
interfered groups, pure preflight, actual turn-end producer metadata, three
independent Native Hand clones, formal Widget identity, no early reducer or fake
Selection owner, future-member refresh suppression, GC, viewport rebasing,
finish/Skip/timeout/disable/replacement/Battle restart/destruction, serial geometry
decline and stale callbacks. Headless cached geometry is test input, not PIE proof.

Production simultaneous-discard and all amended B visual timelines remain
**USER ACTION REQUIRED**, pending the next MCP pass. G9 default remains false;
this implementation batch does not enable Native defaults or seal G9.

## Final focused Native PIE receipt

At committed HEAD `72a564a`, actual Native/D3D12 production PIE passed confirmed
automatic FIFO, queued EndTurn ordering, simultaneous five-card discard and
draw-two across shuffle followed by Abacus reward. Mandatory EndTurn isolation
was observed; the stronger queued-mandatory clearing time line remains USER
ACTION REQUIRED. Actual asset/live G9 readback was true from the preserved user
asset; C++ default remains false. Configuration, artifact paths, failed tool
attempts and the one remaining manual time line:
[Native PIE receipt](G9BRevisionNativePIE.md). No default activation or seal.
