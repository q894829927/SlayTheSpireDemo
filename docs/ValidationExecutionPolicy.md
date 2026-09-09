# Validation Execution Policy

Updated: **2026-09-09**

This is the repository's durable execution policy. Dedicated design/acceptance documents define required Gates; `docs/Validation.md` records evidence actually obtained. Historical phase instructions do not create new work outside the current authorized task.

## Scope and completion

Validate the changed contract with sufficient evidence, record the result, then continue remaining authorized work. Do not expand validation merely to reconfirm a PASS. Complete the requested scope; do not automatically begin an unrequested phase. An explicit review or manual acceptance dependency still gates dependent work.

Choose validation by impact:

- Documentation-only edits: review the diff, references and consistency. No UE build, Automation or PIE unless the edit changes an executable artifact or the user explicitly requests them.
- C++ changes: use the root UE 5.8 project-generation/build workflow, then the smallest focused Automation suite covering the affected contract. Add/update tests when existing coverage does not prove the changed behavior; avoid tests that only mirror implementation.
- Asset/Blueprint changes: use verified UE-supported editing and the relevant compile/load checks. C++ Automation alone does not establish Blueprint or visual correctness.
- Shared infrastructure changes: include directly affected regression coverage. Broad historical suites, Shipping, all-Blueprint recompilation and parity/seal checks require a concrete affected contract or an explicit acceptance requirement.

The ordinary implementation budget remains one build, one focused Automation run and one focused manual PIE pass only if there is a visual Gate. This is an initial scope, not a cap that excuses failures or missing required evidence.

## Evidence reuse and failures

A passing Gate remains valid unless affected code or its proving test/fixture changes, a new failure invalidates it, an applicable final-head requirement applies, or the user requests a rerun. Completed phases are sealed history; reopen their evidence only when a current failure implicates their contract.

On failure, investigate and fix the affected contract, rebuild if required, and rerun only invalidated Gates. Resume at the next unfinished Gate instead of restarting the whole acceptance sequence. If evidence is ambiguous, resolve the specific ambiguity with targeted evidence.

Do not repeat passing PIE, screenshots, log reads, synthetic fixtures, historical-suite runs or independent reviews solely for more confidence. A reviewer or subagent report is not a substitute for actual required evidence. Validation does not require fixed agent roles or duplicate independent proof; delegation follows the session's authorization and is useful only for a distinct bounded task.

## Automated and manual ownership

Automate deterministic state and protocols: identities, ordering, counts, request payloads, tokens/callbacks, lifecycle cleanup, frozen DTOs, numeric values and fallback state. Use manual PIE for player-visible behavior such as animation, layout, clipping, hover/mouse interaction and flicker. Screenshots supplement a visual Gate when they resolve it; they do not replace state assertions or prove transient animation by themselves.

Each phase acceptance separates **AUTOMATED GATES** and **MANUAL PIE GATES**, naming the focused checks and stating when no manual Gate is required. Preserve explicit visual Gates. When manual UE work cannot be performed with available tools, mark it `USER ACTION REQUIRED` and provide the minimal asset/map, actions, expected observations and evidence needed. Await that evidence for dependent acceptance while continuing independent authorized work.

Record exact build/test scope, result and evidence paths once in the applicable validation document. Do not infer Blueprint, PIE, packaged-game or Shipping success from a different check. Do not combine totals from overlapping or differently configured suite prefixes. A checkpoint stores resumable execution state, not new validation policy or authority to commit.

## Historical A2N validation split

The following table is retained as migration reference, not a current task list or authorization to rerun sealed phases. `docs/Phase6UIA2NNativeHUDRefactor.md` holds the original phase Gates. For forward work, Native-only and retained-Legacy restrictions in `docs/LegacyUIPreservationPolicy.md` take precedence over historical parity/recovery procedures; R14-B remains separately unauthorized.

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
