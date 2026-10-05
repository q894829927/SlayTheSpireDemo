# Selection Presentation G9-B — Buffered Player Input and Stable Hand Hover

2026-10-05 user-approved scope revision: [confirmed FIFO / relic tails / turn-end
discard group](QueuedCardPlayAndRelicTimingAmendment.md). Delivered selection-only
behavior and old manual gates below are historical; amended implementation and
affected acceptance follow [revision execution](G9BRevisionExecution.md) and
[final focused Native PIE](G9BRevisionNativePIE.md): implementation/build/affected
Automation PASS, partial PIE, queued-mandatory clearing USER ACTION REQUIRED.
User EndTurn/mandatory observations and their
evidence limitations are recorded in the amendment. Default off / NOT SEALED.

2026-10-05 architecture correction: implementation baseline saved in `5f7f4fc`;
subsequent structure/layout work is tracked in
[Native Hand refactor](NativeHandStructureRefactor.md). This replaces the former
uncommitted delivery state; historical run identities and remaining full B visual
gates are retained. Structure ownership was committed in `4237526`; the subsequent
Slate implementation has build/focused Automation and refactor-specific Native
PIE acceptance, including same-HUD draw Skip and the deferred-completion window.
The original full G9-B enabled-input gates remain pending. G9 input remains
default off / NOT SEALED.

Date: **2026-10-05**

Status: **IMPLEMENTED / AUTOMATED GATES PASS / PARTIAL PIE / USER ACTION REQUIRED / OPT-IN; G9 NOT SEALED**

Design authority: [G9 design](SelectionPresentationG9Design.md). Delivery branch: `codex/g9-buffered-input-detached-cards`, based on `cafe7bf`.

## Delivered behavior

- The existing HUD-owned input arbiter is the only G9 pending-intent storage. Production consumption uses explicit Ready/resolution/display notifications and a coalesced, generation-fenced one-shot callback; NativeTick never polls Gameplay readiness.
- An accepted EndTurn retires the previous card intent and pending Native FastInput retry before normal transient-selection cancellation. The ViewModel forwards the captured exact Gameplay player-turn token independently of displayed revision and calls the existing Gameplay Request. It preserves frozen display chronology and does not Skip active playback.
- Card input prefers the normal current surface, then the exact already-sealed G9 played-card lag window, then existing G8 FastInput. Buffered selection is taken before normal selection; confirmation/target input still requires a fresh action.
- Mandatory-selection, battle/turn/session/binding changes and runtime disable retire old G9 intent. A queued callback cannot migrate across HUD binding generations. Original ViewModel EndTurn semantics, including the G8 target-choice rejection, remain the disabled fallback.
- Native refresh consumes dirty flags through one final dispatcher. The production Reconciled/Selection HUD uses private prepare/commit Hand reconciliation: surviving fan cards retain Widgets, dedicated Hand slots and live Slate trees in frozen order. The private Slate panel arranges current allotted-size geometry on its first pass, including during Blocking playback; Hidden source slots remain. Exact-token geometry protection isolates moving Hand cards. Hover updates transforms/layers without structural layout writes and ignores hidden/disabled/protected sources.
- `bEnableG9BufferedPlayerInput` is the Native startup option and remains **false pending the visual gate**. `SetBufferedPlayerInputEnabled` controls the existing HUD instance at runtime. The generic/test Base Widget remains disabled by default.

## Automated gates

- Bundled UE 5.8 project-file generation, then Development Editor Win64 build.
- `SlayTheSpireDemo.SelectionPresentation.G9B`: production replay without Skip; EndTurn/card/FastInput arbitration; busy and DirectBaseline EndTurn; transient replacement with disabled fallback; mandatory/stale/disable fences; Native dirty updates and surviving Hand identity; hover-only structural isolation.
- Affected `G9A`, `G8B`, Native FastInput, HandInteraction, R8 and G6 tests. The G9-A fixture is shared rather than duplicated.
- `git diff --check` and ownership/request-boundary inspection.

Actual execution (HEAD `cafe7bf6c0d484433cf7737b9b299fb3cb69271b` plus the uncommitted G9-B worktree):

- Project-file generation **PASS**, exit 0: `Saved/Logs/G9BProjectFiles.log`.
- Development Editor Win64 build **PASS**, exit 0: `Saved/Logs/G9BBuild.log` (147 actions), followed by the final amended-source build `Saved/Logs/G9BFinalBuild.log` (27 actions).
- Initial focused run: 37 tests, **35 succeeded / 1 succeededWithWarnings / 1 failed / 0 notRun**. Report: `Saved/AutomationReports/G9B/index.json`; log: `Saved/Logs/G9BAutomation.log`. All seven G9-B tests passed. The R8 invalid-identity negative test passed with four expected rejection warnings.
- The only failure was `G6.ParallelVisualSerialReducer`: its interleaved Damage fixture omitted `IncomingDamage`, `HPDamage`, `BlockedDamage` and `DamageKind` required by the current reducer. The fixture was completed without relaxing production validation. Repair build **PASS**, exit 0, four actions: `Saved/Logs/G9BRegressionRepairBuild.log`.
- The invalidated G6 test alone was rerun: **1 succeeded / 0 warnings / 0 failed / 0 notRun**. Report: `Saved/AutomationReports/G9B_G6Repair/index.json`; log: `Saved/Logs/G9BG6RepairAutomation.log`. Other passing evidence remains valid under the validation execution policy. Across these two runs, all 37 distinct scoped tests now have passing evidence; this is not a claim that the original run was green.
- `git diff --check` **PASS**. No Content/Config changes or retained-Legacy references were introduced.

The disjoint initial scope was G9-A 6, G9-B 7, G8-B 9, Native FastInput 2, HandInteraction 3, R8 6 and G6 4. Process exit 0 did not reflect the initial failed test; results were read from the JSON report.

## Production Hand regression repair — 2026-10-05

User-reported symptom: after playing a card, the remaining card faces shrink and overlap in the bottom-left corner. This invalidated the previous Hand-layout evidence and was repaired before any dependent G9 stage.

The direct regression was changing `OnSlotAdded/OnSlotRemoved` from immediate `LayoutCards()` to marking dirty, without closing the production consumer path. The production `UBattleHUDReconciledWidget::RefreshHand` override still cleared/re-added all children and bypassed the base G9-B fan reconciliation. New Canvas slots therefore retained default placement/size during Blocking playback. Both the old and new NativeTick paths suspend structural layout during playback; previously the slot callbacks guaranteed valid layout. UObject reuse alone had not proved live slot/Slate continuity.

The override now delegates structural reconciliation to the shared base path, then adopts completed draws and applies explicit non-Hand ownership. The fan path removes only departing cards, preserves survivors' slots/Slate trees, restores frozen ordering and lays out immediately on membership changes. Hand/terminal/availability dirty notifications also refresh the production input affordance cache. No per-frame structural rebuild was restored. Battle scoping remains in the shared base; the redundant subclass battle-id field was removed.

Actual HEAD/configuration: `cafe7bf6c0d484433cf7737b9b299fb3cb69271b` plus the uncommitted worktree on `codex/g9-buffered-input-detached-cards`. UE 5.8 Development Editor Win64, production Native asset/map, default D3D12/SM6, `bEnableG9BufferedPlayerInput=false`, Hand card size `(150,210)` in HUD coordinates.

- Extended `G9B.StableHandAndHover` loads the production Native HUD, builds its live Slate tree, plays through normal Gameplay and checks the surviving Widget's exact slot/Slate identity, configured size and bottom-center anchors/alignment. It exposed the missed production override: first repair run **21 succeeded / 1 succeededWithWarnings / 1 failed / 0 notRun**, 23 tests (`Saved/AutomationReports/G9BHandRepair/index.json`, `Saved/Logs/G9BHandRepairAutomation.log`). The sole failure was the extended Hand test; the initial base-only fix did not repair production.
- After fixing the override, bundled project generation **PASS**, exit 0 (`Saved/Logs/G9BHandRepairFinalProjectFiles.log`), and Development Editor build **PASS**, exit 0, 15 actions (`Saved/Logs/G9BHandRepairFinalBuild.log`).
- Final affected-scope Automation: **38 succeeded / 1 succeededWithWarnings / 0 failed / 0 notRun**, 39 tests. Prefixes: `SelectionPresentation.G9B` (7), `HandInteraction` (3), `Phase6UIA2N.R8` (6), `SelectionPresentation.G6` (4), `UIA3.CardPlayedRichHandoff` (1), `Phase6UIA2N.FastInput` (2), `CardSelection.Presentation.G4` (2), `.G5` (5), and `SelectionPresentation.G0` (9), all under `SlayTheSpireDemo`. Report: `Saved/AutomationReports/G9BHandRepairFinal/index.json`; log: `Saved/Logs/G9BHandRepairFinalAutomation.log`. The warning is the expected R8 invalid-identity rejection. These totals describe this run only.
- MCP PIE repair check **PASS** for the reported defect: normal click selected Twin Strike RuntimeId 7, then enemy-target click entered Resolving. While the ViewModel still showed revision 17 / Resolving, its Hand had four cards, energy 4 and enemy HP 104. Captured frames show the remaining four full-size cards in the bottom fan while the played card is still in PlayArea. After revision 18 / Idle, the fan remains coherent and Uppercut hover raises/scales normally. Evidence: `Saved/G9BHandRepairTwinSubmit.json`, `Saved/G9BHandRepairTwin-Frames.json`, `Saved/G9BHandRepairTwin-0.png` (Blocking), `Saved/G9BHandRepairTwin-2.png` (Idle), `Saved/G9BHandRepairHover-1.png` (hover), `Saved/G9BHandRepairHUDConfiguration.json`, `Saved/Logs/G9BHandRepairEditor.log`. Images use `CaptureEditorImage`, not the previously failing offscreen Slate screenshot tool. Earlier captures named `Inflame` followed an intervening battle/turn change and are not claimed as a controlled Inflame test.
- `git diff --check` **PASS**; Content/Config status remains empty. Editor PID 54080 is left running in Native PIE for user continuation; no asset/default override was saved.

This repairs the reported layout defect. It does **not** waive the remaining enabled-G9 visual acceptance below: B remains OPT-IN / PARTIAL PIE / USER ACTION REQUIRED, C-F have not started, and G9 is NOT SEALED.

## Manual PIE gates

Production map: `/Game/SlayTheSpireDemo/Maps/L_Battle_RuinedCitadel`; production Native HUD only.

1. Enable G9 on the running HUD. Play A; hover/click a remaining visible Hand card during an exact already-sealed CardPlayed/destination window. A continues, B raises/scales and selects once after catch-up; fresh Confirm/Target is required.
2. During ordinary CardPlayed, Damage, Draw and Shuffle Presentation, EndTurn accepts without Skip; transient ReadyToConfirm/ChoosingTarget clears only after acceptance. Mandatory selection rejects EndTurn. Disable G9 and verify target-choice fallback.
3. DirectBaseline/busy and player-turn ABA are also deterministic Automation gates; do not infer visual acceptance from them. Where a required manual scenario cannot be reached with the available editor tools, record the exact remaining scenario as USER ACTION REQUIRED.

The B -> C gate remains closed until required automated and visual evidence is recorded. G9-C/D1/D2/E/F have not started.

### MCP observations so far

The existing UE MCP service was restarted with the built editor; no plugin/config changes were made. Native PIE loaded the production map without a map-check error. G9 was temporarily enabled in the in-memory Native HUD Blueprint default and compiled for this inspection; no asset was saved.

- D3D12 PIE: a Pommel Strike (RuntimeId 6) was played, then Warcry (RuntimeId 11) hovered/clicked during its Blocking playback. The captured image `Saved/G9BDuringReplay.png` shows the played card and raised/scaled Warcry concurrently. The immediate ViewModel observation stayed `Resolving`, selected ID -1, revision 4 and energy 5. After catch-up it was `ChoosingTarget`, selected ID 11, revision 5, energy 4 and discard count 1. Warcry had not executed and required a fresh target action. Registered-tool batch and readbacks: `Saved/G9BRapidReplay.json`, `Saved/G9BAfterReplayState.json`.
- A subsequent Slate screenshot returned empty image data, then the editor crashed in D3D12RHI/SlateRHIRenderer (`Saved/Logs/G9BEditor.log`, crash `Saved/Crashes/UECC-Windows-53D8B0C44BBE2380589BB7BDB24FB4E2_0000`). The stack identifies the rendering path, not a proven root cause. A temporary `-d3d11` launch allowed further observation; project RHI/build settings are unchanged.
- D3D11 PIE: EndTurn was physically clicked immediately after Pommel Strike started CardPlayed. Immediate readback retained historical revision 4 / Resolving. The following player turn was playable at revision 10, 5 energy, player HP 65 and enemy HP 141. Evidence: `Saved/G9BEndTurnCardPlayed.json`, `Saved/G9BEndTurnCardPlayedFinal.json`, `Saved/G9BEndTurnCardPlayedFinal.png`. Exact once-only request and no-Skip token assertions are Automation evidence; the initial same-frame application capture alone does not prove the whole animation timeline.
- D3D11 PIE: selecting a target card entered ChoosingTarget (RuntimeId 3 / revision 10), with EndTurn enabled. Clicking EndTurn cleared selection and entered Resolving. Evidence: `Saved/G9BTransientBefore.json`, `Saved/G9BTransientSlate.json`, `Saved/G9BTransientAfter.json`.
- D3D11 PIE: Warcry opened mandatory selection at revision 16; EndTurn was disabled and its physical click left that choice unchanged. Evidence: `Saved/G9BMandatorySlate.json`, `Saved/G9BMandatoryBefore.json`, `Saved/G9BMandatoryAfterEndClick.json`. After selecting a candidate, mouse/Enter submission was inconclusive; focused SpaceBar submission completed it. The following Slate snapshot showed EndTurn enabled while the immediately preceding ViewModel readback remained Resolving at revision 16 (`Saved/G9BMandatoryRetry2Slate.json`). This observes removal of the choice lock without claiming the entire trailing-animation timeline passed.
- D3D11 PIE, G9 disabled in a fresh session: target-card selection remained ChoosingTarget / RuntimeId 6 / revision 4; EndTurn was disabled, and a click preserved all three. Evidence: `Saved/G9BFallbackChoosingSlate.json`, `Saved/G9BFallbackBeforeEnd.json`, `Saved/G9BFallbackAfterEnd.json`.

PIE was stopped; the temporary Blueprint default was restored to false. The editor's Save Content dialog contained only `WBP_BattleHUD_Native`; Don't Save was selected with computer-use after the modal blocked MCP dispatch. The editor exited, and `git status --short -- Content Config` was empty. No visual-test configuration was persisted.

### Remaining gate — USER ACTION REQUIRED

G9-B is **not accepted**. The locked design's §16.1–16.6 still requires complete visual acceptance, and the B -> C dependency remains closed. The available MCP toolset can click Slate and inspect reflected properties, but cannot invoke the Widget runtime setter or introduce/release an ordinary queue hold / force a turn-ABA scenario. No production debug mutation path or content change was added just to bypass those gates.

Use `/Game/SlayTheSpireDemo/Maps/L_Battle_RuinedCitadel` with `WBP_BattleHUD_Native`. Temporarily enable `bEnableG9BufferedPlayerInput` in class defaults, compile and start PIE; do not save that temporary override. Record HEAD `cafe7bf` plus this G9-B worktree, RHI, flag state, actual scenarios and result. A short video or direct user observation of the animation timeline is sufficient alongside the existing numeric/identity Automation evidence; test totals alone are not visual evidence.

1. Observe A continuously while hovering/clicking B in the exact sealed window: A completes naturally, B selects once after catch-up and needs fresh Confirm/Target; no flashback/ghost/layout jump. The recorded MCP overlap is partial evidence, not a continuous timeline.
2. Click EndTurn during Damage, Draw and Shuffle, plus the required ordinary same-turn busy and DirectBaseline busy boundaries. Verify the current animation is not skipped and the same turn's request executes once. Existing busy/DirectBaseline Automation is passing; these artificial busy windows are not exposed by the production map's registered MCP tools.
3. Finish the no-target ReadyToConfirm EndTurn replacement, mandatory-choice trailing-animation timeline, and runtime disable check through `SetBufferedPlayerInputEnabled(false)`. The production starting deck inspected here uses targeted cards; ReadyToConfirm is proved in Automation but was not reached in this PIE. Startup-disabled ChoosingTarget fallback was observed above.
4. Perform the design's player-turn ABA observation: an accepted old-turn intent must not end the next player turn. The deterministic ABA test is passing; no manual turn-forcing path was exposed by MCP.

After those results pass, enable the Native startup default, verify the affected default behavior, update B acceptance and begin G9-C. Do not implement/activate C, D1, D2, E or seal F before this dependency is satisfied.
