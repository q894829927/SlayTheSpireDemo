# G9-B revision execution

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
