# Native Hand structure and layout refactor

Authority: the user-approved G9-B architecture correction, 2026-10-05. G9 input
remains opt-in/default off; C–F cannot begin before the full B acceptance gate.
Gameplay authority, historical reducers, Selection protocol and Blocking timing
are unchanged. Scope: Native C++, tests and documentation only.

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
2. Unified final notification dispatcher, nonvirtual Hand request entry, private
   formal registry and prepare-before-commit reconciliation. Restricted hooks
   handle binding, ownership and Selection after structural consistency. Nested
   publications merge into a sequential outer drain. One rooted incoming draw
   attachment carries the exact playback token, surface generation, frozen card
   identity and index; completion and official history are both required to adopt.
3. Replace the Canvas implementation with a dedicated UPanelWidget/slot/private
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
