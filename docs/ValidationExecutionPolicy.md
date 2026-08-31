# Validation Execution Policy

Date: **2026-08-31**

This document defines how validation work is executed in this repository. It is a durable execution policy, not a phase-status document and not a replacement for `docs/Validation.md`, which records trusted evidence that was actually run.

The goal is to make validation proportional to the changed contract, prefer deterministic Automation over repeated visual inspection, keep manual PIE focused on genuinely visual behavior, and prevent validation work from expanding after the required Gate has already been proven.

## 1. Core rule

Validation exists to reach the acceptance Gate required by the current phase.

It does not exist to repeatedly increase confidence after that Gate is already satisfied.

```text
required Gate
-> sufficient evidence
-> record result
-> STOP
```

Do not turn validation into:

```text
PASS
-> inspect PASS again
-> take another screenshot
-> inspect screenshot
-> rerun PIE
-> reread logs
-> create another fixture
-> rerun historical suites
```

One sufficient evidence item per Gate is enough unless the applicable design/acceptance document explicitly requires more.

## 2. Validation ownership

### 2.1 Automated Gate

Automated validation is the default for deterministic program state and contracts. The agent/Codex should execute these itself when the environment supports them.

Prefer Automation or direct deterministic assertions for:

```text
C++ compilation
UHT/reflection contract
Blueprint automated compile when available
RuntimeId / CardId equality
Hand Widget count
Delegate callback count
exact request payload
Presentation Token ownership
stale-token rejection
Finish / Cancel callback count
Timer cleared / active
transient reference null/non-null
InteractionState
Outcome
bInputLocked
bCanEndTurn
HP / Block / Energy values
Draw / Discard / Exhaust counts
Status RuntimeSequence / identity
Widget create/update/remove state
Visibility enum
RenderOpacity numeric state
frozen DTO consumption
invalid payload false fallback
historical restore after Cancel
destruction cleanup
```

Do not use screenshots to prove a deterministic state that can be asserted directly.

### 2.2 Manual PIE Gate

Manual PIE is reserved for behavior whose acceptance depends on what a player actually sees or feels and cannot be fully established from deterministic state alone.

Typical manual checks are:

```text
animation appears once
animation timing feels coherent
visual movement path
layout / overlap / clipping
hover presentation
mouse interaction
highlight placement
card hide / restore appearance
transient card movement
visible flicker / A->B->A flashback
duplicate visual instances
text placement and readability
Legacy vs Native visual parity
final production-map smoke after cutover
```

When manual UE work is required, the agent must stop attempting to replace it with repeated screenshots and instead provide the user with the exact minimal actions and expected observations.

Manual evidence is not required for deterministic contracts already closed by Automation unless the current phase explicitly requires a visual parity Gate.

## 3. Default validation budget

For an ordinary implementation phase, the default budget is:

```text
1. Build once.
2. Run the smallest focused Automation suite once.
3. Run one focused manual PIE pass only when the phase has a genuine visual/manual Gate.
4. Record the result.
5. Stop.
```

Do not automatically add:

```text
full regression
Phase6R
A2D5
Shipping
all-Blueprint recompilation
Scenario A-E replay
architecture reviewer
extra screenshots
additional synthetic fixtures
```

Those are run only when the applicable phase/acceptance document explicitly requires them or a concrete failure makes them necessary.

## 4. Passing Gates are sticky

A Gate that already passed must not be rerun unless one of these is true:

```text
code affecting that Gate changed after the PASS;
the test/fixture proving that Gate changed;
a new failure directly invalidates that evidence;
the applicable acceptance document requires a final-head rerun;
the user explicitly requests a rerun.
```

Do not rerun a completed phase merely because a later phase is being implemented.

Completed phases are sealed execution history for normal forward work. Do not re-audit their detailed evidence unless a current failure directly implicates that contract.

## 5. Failure policy

If a Gate fails:

```text
identify the failed Gate
-> investigate only that failure
-> make the smallest relevant fix
-> rebuild if the changed code requires it
-> rerun only the Gate(s) invalidated by that fix
-> continue from the next unfinished Gate
```

Do not restart the complete acceptance sequence after every small fix.

Example:

```text
Build PASS
Automation PASS
Manual PIE Damage visual FAIL

fix Damage visual code
-> Build (because C++ changed)
-> rerun only the Damage-focused automated/manual evidence invalidated by the fix
```

Do not automatically rerun unrelated R3/R4 Gates.

## 6. No validation-of-validation loops

After the required result is established, do not:

- inspect the same screenshot multiple times;
- take multiple screenshots of the same visual state without a concrete ambiguity;
- repeatedly reopen PIE for the same passing scenario;
- reread logs after the required success result is already known;
- create a second synthetic fixture to prove a contract already covered by the focused test;
- run an independent reviewer only to reconfirm a passing test;
- re-read broad historical documents to decide whether an already documented predecessor phase really passed;
- run unrelated regression suites “for confidence”.

If evidence is ambiguous, identify the exact ambiguity and collect one targeted additional evidence item. Do not expand the whole test surface.

## 7. Screenshot policy

Screenshots are supplemental manual evidence, not the default validation mechanism.

Use a screenshot only when:

```text
the acceptance criterion is genuinely visual;
a deterministic assertion cannot prove it;
and the image materially resolves the Gate.
```

When screenshots are required:

```text
one screenshot per distinct visual state is normally sufficient;
do not repeatedly inspect the same screenshot;
do not generate more after the Gate is clearly confirmed.
```

For transient animation where a still image cannot prove the behavior, prefer one short manual PIE observation over a screenshot loop.

## 8. Validation levels

### Level 1 — ordinary phase Gate

Use for narrow implementation phases:

```text
Editor Build
focused Automation
minimal manual PIE if genuinely visual
```

### Level 2 — shared infrastructure regression

Use only when shared contracts such as these are changed:

```text
BattleHUDWidgetBase
BattlePresentationController
shared ViewModel protocol/semantics
Presentation Record/Envelope protocol
other documented cross-phase ownership boundaries
```

Run only the directly affected historical regression suites required by that shared contract.

### Level 3 — parity, cutover and seal

Reserve broad/expensive validation for explicit parity/cutover/seal phases, for example:

```text
Dual-stack parity
Scenario A-E
aggregate Phase6R
A2D5 final-head gate
Shipping exclusion
production cutover PIE
final seal review
```

Do not pay Level-3 validation cost during every Level-1 migration step.

## 9. Agent and subagent policy during validation

The main agent owns the validation plan and acceptance claim.

Unless explicitly required:

```text
do not spawn a test_runner for a trivial focused suite;
do not spawn an architecture_reviewer after every phase;
do not spawn repo_explorer to rediscover already documented contracts;
do not use multiple agents to independently prove the same Gate.
```

A dedicated `test_runner` or reviewer is appropriate only for a meaningful bounded validation scope or concrete high-risk issue.

Subagent completion is not an acceptance claim; actual required evidence must still be recorded once.

## 10. Required phase output format

Every implementation phase should separate its acceptance into two sections.

```text
AUTOMATED GATES
- exact tests/builds the agent must execute

MANUAL PIE GATES
- only the visual/player-facing checks the user must execute
```

If there is no meaningful manual Gate, write:

```text
MANUAL PIE GATES
- none required for this phase
```

Do not silently convert a manual Gate into repeated agent screenshot inspection.

After Automated Gates pass, the agent should provide the shortest executable manual checklist and wait for the user rather than continuing to search for more evidence.

## 11. Phase 6UI-A2N validation split

The dedicated A2N migration plan still defines the exact behavior and acceptance requirements. This section defines the default ownership of those checks so implementation prompts do not repeatedly reinvent the validation strategy.

| Phase | Automated Gates — agent/Codex | Manual PIE Gates — user |
|---|---|---|
| R5 Playback Kernel | Begin accepted/false paths; active Token ownership; exact-token Finish; duplicate/stale Finish no-op; Cancel does not Notify; destruction cleanup; unsupported Record false; Editor Build | Minimal Native PIE smoke only if required by the phase: battle opens, no crash, no permanent input lock. No record-specific animation judging yet. |
| R6 Energy / Block / Shuffle | Frozen Before/After validation; invalid payload false; exact-token Finish; Cancel historical restore; Energy/Block/pile numeric state; timer/transient cleanup | One focused observation of Energy/Block/pile surfaces if the phase plan requires visible parity. Do not replay full battles. |
| R7 Damage | Target identity; frozen IncomingDamage/HP/Block fields; invalid target false; exact-token Finish; stale callback; Cancel restore; destruction cleanup | One Damage scenario: damage number appears once, correct target reacts, number clears, HP/Block visual stays coherent, no duplicate/flicker. |
| R8 Card lifecycle | CardPlayed/CardZoneChanged identity/index validation; formal vs presentation-only input boundary; transient ownership; hide/restore state; destination cleanup; Cancel restore; duplicate/stale callback behavior | One or a few focused card paths: Hand -> PlayArea -> Discard/Exhaust as applicable; verify visible hide/move/retire/restore and no duplicate card/flashback. |
| R9 Status | Exact identity `TargetPresentationId + StatusId + RuntimeSequence`; create/update/remove; exact Widget reuse; invalid lookup false; Cancel frozen-VM rebuild | Focused visible status lifecycle: create, same-identity amount update/reduction, removal, tooltip/row appearance, no duplicate icon. |
| R10 Terminal / PresentationUnavailable | Victory/Defeat/ResolutionFault validation and ordering; historical Outcome surface; PresentationUnavailable remains separate; input lock | One focused visual check per genuinely distinct terminal surface required by the plan; do not use repeated screenshots to prove enum/state values. |
| R11 Dual-stack parity | Deterministic Legacy/Native state/result comparison; focused regression suites; record ordering/final VM parity | Concentrate broad Scenario A-E and Legacy-vs-Native visual parity here rather than repeating it in R5-R10. |
| R12 Cutover | Production WidgetClass/config; build; required focused/aggregate automation; no unintended Legacy runtime path | Production `L_BattleTest` PIE smoke proving the real configuration uses Native and remains playable. |
| R13 Stabilization | Objective regression/fuzz/boundary tests required by the stabilization plan | Manual PIE only for concrete issues found by automated stabilization; no broad exploratory replay by default. |
| R14 Cleanup | Build; reference scan; compile; required Legacy-reference absence checks | Minimal open/run smoke after destructive cleanup when authorized. R14-B Legacy removal remains separately authorized. |

This table does not authorize starting a future phase and does not weaken any explicit Gate in `docs/Phase6UIA2NNativeHUDRefactor.md`. If that plan requires a specific additional Gate, run it once and classify it as automated or manual using the rules above.

## 12. A2N prompt contract

For R5 and later, implementation prompts should use a closed validation scope similar to:

```text
Validation is CLOSED-SCOPE.

AUTOMATED GATES
1. Editor Build once.
2. Run the named phase-focused Automation prefix once.
3. Run only other deterministic Gate(s) explicitly required by this phase.

MANUAL PIE GATES
1. Provide the user only the minimal genuinely visual scenario(s).
2. Do not attempt to replace these with repeated screenshots.

A passing Gate must not be repeated unless later edits invalidate it.
If one Gate fails, fix and rerun only the invalidated Gate(s).

Do not run Phase6R, A2D5, Shipping, broad Scenario A-E, architecture review,
or unrelated historical regression unless this phase explicitly requires it.

When all required Gates pass:
update evidence/checkpoint
commit
STOP

Do not automatically begin the next phase.
```

## 13. Relationship to evidence documents

`docs/ValidationExecutionPolicy.md` answers:

```text
How should validation be executed?
Who runs which Gate?
When should validation stop?
```

`docs/Validation.md` answers:

```text
What validation was actually run and trusted?
```

`docs/CODEX_GOAL_CHECKPOINT.md` answers:

```text
What has the current Goal already completed?
What is the next exact action?
```

Do not copy large historical evidence blocks into this policy document. Do not put transient current-phase status into root `AGENTS.md`.