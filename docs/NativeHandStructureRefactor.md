# Native Hand structure and layout refactor

Authority: the user-approved G9-B architecture correction, 2026-10-05. G9 input
remains opt-in/default off; C–F cannot begin before the full B acceptance gate.
Gameplay authority, historical reducers, Selection protocol and Blocking timing
are unchanged. Scope: Native C++, tests and documentation only.

Final refactor status: **IMPLEMENTED / BUILD PASS / AFFECTED AUTOMATION PASS /
FOCUSED NATIVE PIE PASS**. This accepts the requested structure/layout correction,
not the remaining full G9-B enabled-input phase or G9 seal.

## Failure and responsibility

G9-B changed Canvas slot callbacks from immediate layout to marking layout dirty.
The production Reconciled override bypassed the base structural refresh; playback
already suppressed the interaction Tick that could consume that dirtiness. New
Canvas slots consequently kept default geometry. The initial repair restored
immediate reconciliation, but virtual whole-surface overrides and several Hand
attachment writers still allowed the invariant to be bypassed.

The HUD owns frozen membership, exact `(BattleId, RuntimeId)` Widget identity and
attachment lifetime. The fan panel owns base geometry for its allotted size.
Hover owns only transforms and explicit paint layers. Presentation owns visual
tokens; none of these surfaces owns Gameplay truth.

## Commit sequence

1. Baseline saved in `5f7f4fc`: G9-B OPT-IN / AUTOMATED PASS / PARTIAL PIE / NOT
   SEALED. The valid repair evidence is retained in SelectionPresentationG9BExecution.md.
2. Structure saved in `4237526`: unified final notification dispatcher, nonvirtual Hand request entry, private
   formal registry and prepare-before-commit reconciliation. Restricted hooks
   handle binding, ownership and Selection after structural consistency. Nested
   publications merge into a sequential outer drain. One rooted incoming draw
   attachment carries the exact playback token, surface generation, frozen card
   identity and index; completion and official history are both required to adopt.
3. This layout batch replaces the Canvas implementation with a dedicated UPanelWidget/slot/private
   Slate SPanel. Arrangement uses current allotted geometry and frozen ranks;
   draw layers sort deterministically. Remove layout dirtiness/Tick repair. Freeze
   only a moving Hand card's base geometry under its exact playback token.

Preparation failure preserves the last complete display and uses
PresentationUnavailable. Playback preparation failure declines through the
existing path. No Presentation failure requests Gameplay ResolutionFault.

## Evidence and remaining gates

Batch 2 and 3 validation is recorded here after actual runs, including the tested
source base, flag configuration, exact Automation scope and Native PIE evidence.
Reports under Saved are local execution artifacts, not source files.

Required visual map: `/Game/SlayTheSpireDemo/Maps/L_Battle_RuinedCitadel` with the
production Native HUD. Observe ordinary play, incoming draw/cancel, Selection
handoff, viewport change during playback and subsequent hover. Unperformed visual
gates are USER ACTION REQUIRED. This refactor does not waive the remaining full
G9-B visual gates in SelectionPresentationG9BExecution.md.

Batch 2 source: baseline `5f7f4fc` plus the structural refactor commit.
Bundled project generation PASS (`Saved/Logs/HandStructureProjectFiles.log`);
Development Editor build PASS (`Saved/Logs/HandStructureFinalBuild.log`). Initial
focused execution completed 16 tests, then aborted in the old R8 draw fixture:
its ViewModel had BattleId 0 and recreated Hand directly. The fixture now uses the
exact Battle and production reconciliation entry. Affected/unfinished recovery:
37 succeeded / 1 expected-warning / 0 failed / 0 notRun, 38 tests
(`Saved/AutomationReports/HandStructureRecovery/index.json`). Union with unchanged
initial cases: 50 distinct passes: HandStructure 2, HandInteraction 3, Native
FastInput 2, R8 6, G0 9, G4 2, G5 5, G6 4, G8-B 9, G9-B 7, frozen rich
CardPlayed handoff 1. G7 has no separate prefix; its retained G0/G4/G5/G6 protocols
are covered. This is combined distinct coverage, not an uninterrupted 50-test run.
Batch 2 does not claim new PIE acceptance. The Slate batch follows separately.

### Batch 3: Slate layout and geometry protection

Tested source base: `4237526` plus this batch's source diff. G9 startup remains
false; no asset/configuration or module dependency was changed. The runtime class
name and existing layout parameters remain. Arrangement and painting now belong
to the panel's private Slate implementation. The HUD commits frozen ranks only;
NativeTick handles hover, target arrow and visual animation. Moving attached
cards hold exact-token base geometry; completion/cancel releases it. Losing the
ViewModel clears the temporary draw, retires surface generation and unbinds
formal requests while preserving the last complete display.

Bundled project generation PASS (`Saved/Logs/HandSlateProjectFiles.log`);
Development Editor build PASS (`Saved/Logs/HandSlateFinalBuild.log`). Earlier
compile/link failures were corrected without a dependency/build-setting change;
tests inspect the actual panel arrangement through its read-only geometry API.

Initial focused execution (`Saved/AutomationReports/HandSlate/index.json`):
54 success / 1 expected R8-warning / 1 failed / 0 notRun, 56 cases. The only
failure was the new production `G9B.NativeHandBlockingWithoutTick` fixture. It
first discarded the live Slate root at the end of RunTest, then its recovery
observed before the normal deferred completion callback. The fixture now retains
the root across latent frames and drives the existing timer/CoreTicker boundary;
it never invokes NativeTick, manually applies history or uses Skip to finish.
Targeted final recovery (`Saved/AutomationReports/HandSlateBlockingFinal/index.json`,
`Saved/Logs/HandSlateBlockingFinal.log`): 1 success / 0 failed / 0 notRun.
At that source revision, 55 unaffected initial passes plus the targeted recovery
provided 56 distinct successful cases, not one uninterrupted 56/56 execution.
The later Skip lifecycle finding below required additional affected validation.

Actual disjoint scope: HandStructure 2, HandInteraction 4, Native FastInput 2,
R8 6, G0 9, G4 2, G5 5, G6 4, G8-B 9, G8-D DamageNumber integration 4,
G9-B 8 and frozen rich CardPlayed handoff 1. G7 has no independent prefix;
retained G0/G4/G5/G6 ownership/completion protocols are included.

Production tests establish formal structure after actual Blocking CardPlayed
completion without NativeTick, surviving Widget/slot/live-Slate identity, first
arrangement at 500/1200 widths, frozen order and deterministic paint layers,
Hidden slots, geometry token release/stale isolation and the attachment/GC/
preparation-failure protocols. These state assertions do not substitute for PIE.

### Actual Native MCP PIE

Source base `4237526` plus the same runtime diff; production map
`/Game/SlayTheSpireDemo/Maps/L_Battle_RuinedCitadel`, Native HUD, D3D12,
`bEnableG9BufferedPlayerInput=false`, HandCardSize 150x210, HandMoveUpPixels 0.
Configuration: `Saved/HandSlateHUDConfig.json`. Editor logs:
`Saved/Logs/HandArchitectureEditor.log` and
`Saved/Logs/HandSlateFinalPIEEditor.log`. No temporary asset/default edits were
made; Content/Config remained unchanged. All images were captured with
CaptureEditorImage; the formerly failing Slate screenshot path was not used.

- Pommel Strike RuntimeId 6: normal card/target clicks, F11 during playback;
  coherent full-size remaining fan, separate incoming Burning Pact/Defend,
  displayed Hand 4 -> 5 -> 6 while still Resolving, energy 4 and enemy HP 141.
  Evidence: `Saved/HandSlatePommelSubmitResize.json`,
  `Saved/HandSlatePommelResize-Frames.json`, images `HandSlatePommelResize-0/1/2.png`.
- Warcry RuntimeId 11: normal Self submission, draw, mandatory Selection;
  select Defend into SelectionArea, deselect to restore the Hidden Hand slot,
  reselect/confirm, transition retires and the five-card frozen Hand remains.
  Evidence: `Saved/HandSlateWarcrySubmit.json`, `Saved/HandSlateWarcry-0/1/2.png`,
  `Saved/HandSlateSelectionOwned-0.png`, `Saved/HandSlateSelectionDeselected-0.png`,
  `Saved/HandSlateSelectionConfirmed-Frames.json` and corresponding images/states.
- Fresh verification session: normal Pommel Strike, two draws complete; surviving
  Pommel Strike hover raises/scales normally with six formal cards, Idle revision
  5. Evidence: `Saved/HandSlateFinalAfterDrawHover.json` (Hover true),
  `Saved/HandSlateFinalAfterDrawHover-0.png` and matching state JSON. The older
  `HandSlatePostSelectionHover` attempt used a stale Slate ref and is not counted
  as a successful hover check.
- Another normal Pommel Strike, then StopPIE during Resolving (revision 5,
  energy 3, enemy HP 132); restart returns to a clean five-card Idle baseline and
  fresh hover succeeds, with no old moving/Selection visuals visible.
  Evidence: `Saved/HandSlateFinalCancelSubmit.json`,
  `Saved/HandSlateFinalCancelStop.json/.png`, `Saved/HandSlateFinalRestart-0.png`,
  `Saved/HandSlateFinalRestartHover.json`, `Saved/HandSlateFinalRestartHover-0.png`.
  This proves playback stop/restart, not the exact mid-draw/Skip visual boundary.

### Skip completion-receipt boundary

The targeted MCP Skip check found an actual additional defect, rather than an
unperformed gate. `Saved/HandSlateSkipInterrupt.json/.png` captured Resolving
revision 4 with four formal cards and a separate incoming Burning Pact. A normal
Strike click invoked existing FastInput/Skip: revision 5, six formal cards and
ChoosingTarget, but `Saved/HandSlateSkipRecovered-0.png` still showed the retained
Pommel Strike in PlayArea. Its animation had finished before deferred forwarding;
the Native cancellation guard no longer considered that visual active.

The Base tracked playback unit now owns the completion receipt throughout that
window. Retiring an exact cancelled unit notifies the final Native lifetime hook
before derived cancellation dispatch. It cancels the matching temporary Hand
attachment and cross-record retained PlayArea visual whether the animation is
active or already finished. Normal forwarding retires the receipt without
destroying an incoming draw that is about to be adopted. No duplicate pending
token owner, timing change, reducer replay or Gameplay fault path is introduced.
SingleRecord and Group tracking use the same retirement boundary.

The existing R8 Skip regression now runs both active-animation and
finished-before-forwarding scenarios, including wrong-token isolation and the
queued old callback after Skip.

Final runtime project generation/build PASS:
`Saved/Logs/HandSlateRetirementProjectFiles.log`,
`Saved/Logs/HandSlateRetirementBuild.log` (81 actions). Focused affected run
`Saved/AutomationReports/HandSlateRetirement/index.json`: 52 success / 1 expected
R8 warning / 2 failed / 0 notRun, 55 cases. Both failures were newly included old
R5 fixtures: Token B/C had sequence 2/3 but reused a sequence-1 Record. The exact
Record/Token guard already existed at source parent `4237526`; it was preserved.
Fixtures now create matching immutable Records. Required generation/build PASS
(`Saved/Logs/HandSlateR5ProjectFiles.log`, `Saved/Logs/HandSlateR5Build.log`);
only those two cases were rerun: 2 success / 0 failed / 0 notRun
(`Saved/AutomationReports/HandSlateR5Recovery/index.json`).

Latest affected coverage is therefore the 53 valid initial cases plus the two
repaired R5 cases, not an uninterrupted 55/55 execution. Scope: R5 4, R8 6,
HandStructure 2, Native FastInput 2, G0 9, G4 2, G5 5, G6 4, G8-B 9,
G8-D 4 and G9-B 8. Earlier HandInteraction 4 and frozen rich CardPlayed handoff
1 evidence is unaffected by this receipt-retirement change and reused; no
overlapping suite totals are added together.

Delivery generation/build PASS (`Saved/Logs/HandSlateDeliveryProjectFiles.log`,
`Saved/Logs/HandSlateDeliveryBuild.log`). After aligning the dedicated-slot test
description/include, only `G9B.StableHandAndHover` and
`G9B.NativeHandBlockingWithoutTick` were checked again: 2/2 PASS
(`Saved/AutomationReports/HandSlateDelivery/index.json`). They replace the same
two cases' earlier evidence; they do not increase the reported scope.

Repaired production MCP verification, same Native map/HUD, G9 false, D3D12,
150x210 cards (`Saved/HandSlateSkipFinalHUDConfig.json`):

- Repeat the previously failing normal Pommel Strike RuntimeId 6 and Strike
  click during the uncommitted draw/completion window. Before: revision 4,
  Resolving, four formal cards plus incoming Burning Pact and retained PlayArea
  card. After existing FastInput/Skip: revision 5, ChoosingTarget RuntimeId 4,
  six formal cards, energy 4 and enemy HP 141; PlayArea is visibly empty.
  Evidence: `Saved/HandSlateSkipFinalSubmit.json`,
  `Saved/HandSlateSkipFinalInterrupt.json/.png`,
  `Saved/HandSlateSkipFinalRecovered-0.png` and matching state/Slate JSON.
- Cancel ordinary selection, hover/select the newly drawn Defend: raised card,
  Idle -> ChoosingTarget, RuntimeId 1, energy remains 4; a new Self target click
  is still required. Evidence: `Saved/HandSlateSkipFinalNewCardHover.json`,
  `Saved/HandSlateSkipFinalNewCardHover-0.png`,
  `Saved/HandSlateSkipFinalNewCardSelect-0.png` and corresponding state JSON.
- In the same HUD, play remaining Pommel Strike RuntimeId 2 and click Strike
  earlier during incoming draw movement. Before: revision 5, Resolving, five
  formal cards with translucent moving Twin Strike; after Skip: revision 6,
  ChoosingTarget RuntimeId 4, seven formal cards, energy 3 / enemy HP 132,
  no temporary draw or retained PlayArea card visible. Evidence:
  `Saved/HandSlateSkipActiveSubmit.json`, `Saved/HandSlateSkipActiveInterrupt.json/.png`,
  `Saved/HandSlateSkipActiveRecovered-0.png` and corresponding state/Slate JSON.
- Cancel ordinary selection, hover/select newly drawn Twin Strike RuntimeId 7:
  normal raised visual and ChoosingTarget while energy stays 3, with no frozen
  movement lease or lingering prior callback visible. Evidence:
  `Saved/HandSlateSkipActiveNewCardHover.json`,
  `Saved/HandSlateSkipActiveNewCardHover-0.png`,
  `Saved/HandSlateSkipActiveNewCardSelect-0.png` and corresponding state JSON.

The numeric/exact-identity assertions are also covered by Automation; images and
actual normal input establish the visual behavior. Editor log:
`Saved/Logs/HandSlateSkipFinalPIEEditor.log`. PIE ended and the editor closed
normally; Content/Config/Build.cs remained unchanged. These focused checks
complete the cancellation/Skip visual gate previously left open by stop/restart.

### Remaining acceptance

Implementation, build, affected automated protocols and the requested focused
Native visual checks are complete. The structural entry points are converged,
layout does not rely on Tick, and attachment/cancellation paths use the shared
commit and tracked-retirement boundaries. The three local implementation batches
include their relevant tests/docs; generated evidence remains local under Saved.

**USER ACTION REQUIRED** only for the remaining original G9-B enabled-input
timing/EndTurn gates in SelectionPresentationG9BExecution.md. This correction does not
enable G9 by default, open C–F, or declare G9 COMPLETE / VALIDATED / SEALED.
