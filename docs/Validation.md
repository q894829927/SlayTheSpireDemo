# Validation

This document records trusted historical validation evidence and the rules for making new validation claims.

## Validation Rules

After C++ changes:

- verify includes and module dependencies;
- build `SlayTheSpireDemoEditor` when the build environment is available and the user has authorized it;
- run the smallest focused Automation suite relevant to the changed contract;
- run the aggregate regression gate when required and available;
- report failures instead of masking them;
- require user-side compile/Automation if the current environment cannot run UE.

Never infer Blueprint/UMG or PIE correctness from C++ Automation. Never claim build, PIE, packaged-game, Shipping exclusion or regression success unless that exact validation was run.

Exact suite totals are historical evidence, not permanent acceptance constants. Do not arithmetically combine owner suites run with different configured prefixes.

## Trusted Automation and Build Evidence

```text
Phase 5         13/13 PASS
Phase 6A        23/23 PASS
Phase 6B        12/12 PASS
Phase 6C         5/5 PASS
Phase 6UI-A0    20/20 PASS
Phase 6UI-A1    11/11 PASS
Phase 6UI-A3     8/8 PASS

Historical combined owner run 92/92 PASS

Phase 6UI-A2A    8/8 PASS
Phase 6UI-A2B    8/8 PASS
Phase 6UI-A2C    8/8 PASS
Phase 6UI-A2D1   3/3 PASS
Phase 6UI-A2D2   4/4 PASS
Phase 6UI-A2D3   4/4 PASS
Phase 6UI-A2D4   6/6 PASS
Phase 6UI-A2D5 focused 6/6 PASS
Phase6R expanded aggregate 100/100 PASS
Shipping exclusion PASS
```

## Trusted Manual Evidence

- Normal UI player → enemy → player turn loop passed in PIE.
- Self-target Defend → highlighted Player selection passed in PIE.
- Packaged Defend dynamic `{Block}` description passed.

These manual results predate unified UI-A2 committed-record playback and therefore do **not** close UI-A2E.

## Current Acceptance Boundary

UI-A2A/A2B/A2C/A2D C++ committed-presentation contracts are sealed by focused and aggregate Automation evidence. UI-A2E still requires unified Blueprint/UMG routing and actual PIE acceptance.

Required A2E scenarios include:

- ordinary card Damage;
- card plus Status creation/update/reduction/removal;
- complete EndTurn macro Envelope, including Block/Energy/zone/shuffle behavior as applicable;
- Victory and Defeat;
- genuine ResolutionFault distinct from PresentationUnavailable;
- input remains locked during playback and unlocks only after catch-up to the newest matching revision.

Use `docs/UIA2ERemainingSteps.zh-CN.md` for exact execution and evidence steps.

## User-Action Boundary

When UE Editor work is required but unavailable to the agent, label it `USER ACTION REQUIRED` and provide exact asset/menu paths, graph/function names, nodes, pins, property values, compile/save order, expected results and requested logs/screenshots.
