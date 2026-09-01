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
Phase 6UI-A2N R3 review 4/4 PASS
Phase 6UI-A2N R4 focused PASS
Phase 6UI-A2N R5 focused 4/4 PASS
Phase 6UI-A2N R6 focused 5/5 PASS
Phase 6UI-A2N R7 focused 5/5 PASS
Phase 6UI-A2N R8 focused 6/6 PASS
Phase 6UI-A2N R9 focused 5/5 PASS
```

## Current UI-A2E Goal-Run Evidence — 2026-08-31

### StatusChanged visible Blueprint/PIE acceptance

On `main@8af9487`, the saved `WBP_BattleHUD` at SHA-256 `574FF058...` was exercised
in a capturable floating PIE session through the real Gameplay status and EndTurn
paths. `Strength#1` committed `2` as creation and `3` as a same-identity update while
remaining a single visible widget. Enemy `Weak#3` committed `2`, reduced to `1` on
the first real EndTurn using the same widget, then reduced `1 -> 0` on the second
EndTurn and disappeared without reappearing after completion. No duplicate or
A→B→A flashback was observed, later Records completed, and the controller returned
to the ready/Idle state. StatusChanged creation, update/reduction, and removal are
therefore fully validated on that saved asset.

### Batch 2 visible Blueprint/PIE acceptance — 2026-08-31

On the saved `WBP_BattleHUD` at SHA-256 `7BF7488D...`, a real one-cost card changed
Energy from `5/5` to `4/5` exactly once, with no duplicate same-cost
`EnergyChanged`. A real EndTurn produced four ordered Hand discard records and five
ordered Draw operations. The real shuffle producer then committed
`MovedCardCount=5`, Draw `0 -> 5`, Discard `5 -> 0` exactly once, after which Draw
continued to a final Draw count of `4`. The queue emptied, the controller returned
to `ReadStateReady / State=2`, and input remained usable.

After the one Batch 2 architecture review, five P1 Blueprint wiring findings were
fixed and the HUD was recompiled/saved: Energy text format, Energy Cancel restore,
CardZone card/ToZone validation, two DeckShuffled count comparisons, and PlayArea
transient-reference cleanup. The corrected graph passed the same focused real PIE
regression.

The batch-level `SlayTheSpireDemo.Phase6UIA2C` Automation run completed 8 tests with
5 succeeded, 3 succeededWithWarnings, 0 failed, and 0 notRun. The warnings were the
expected rollback/fail-soft cases. This was not a final-head A2D5, Phase6R, or
Shipping-exclusion run.

### Batch 3 terminal Blueprint/PIE acceptance — 2026-08-31

On saved `WBP_BattleHUD` SHA-256 `24BA3F8B...`, real gameplay produced Victory
(enemy `29/100 -> 0/100`) and Defeat (player `2/80 -> 0/80`); the formal terminal
surface showed `胜利` and `战斗失败` only after preceding committed Records.

Two isolated real `UEDPIE` scenarios then used existing authoritative testing
producers through a temporary Editor-only harness. The EndTurn structural failure
produced seven ordered Records with exactly one final ResolutionFault, entered the
faulted State/Outcome, and showed the formal Overlay with `战斗结算异常`. A forced
presentation freeze failure instead published no ResolutionFault Envelope, left
Gameplay in PlayerTurn with `Outcome=None`, and kept the terminal Overlay collapsed.
The harness constructed no Record/Payload, was removed afterward, and the standard
Editor build succeeded with no C++ diff.

The one Batch 3 architecture review found no P0/P1/P2 issue. The one focused
`SlayTheSpireDemo.Phase6UIA2C` run completed 8 tests with 5 succeeded,
3 succeededWithWarnings, 0 failed, and 0 notRun. It is not final-head evidence.

### Batch 4 Cancel/Reconcile and full PIE acceptance — 2026-08-31

On saved `WBP_BattleHUD` SHA-256 `990125C9...`, Cancel now clears the active timer,
restores the type-specific historical ViewModel surface, and enters one single-
direction local cleanup tail. The tail clears card/status transient references,
Damage/Block target flags, active type, and active token, and never calls normal
completion Notify. One independent architecture review initially blocked four P1
wiring errors; all were corrected, recompiled/saved/reloaded, and the directed final
review passed with no remaining P0/P1.

Real PIE Scenario A used Strike (`Energy 5/5 -> 4/5`, Enemy `100/100 -> 94/100`),
Scenario B used Uppercut and two real EndTurn requests (Weak/Vulnerable `2 -> 1 -> 0`
with no duplicate), and Scenario C exercised the full discard/draw/shuffle EndTurn
macro before returning to PlayerTurn. The accepted Victory/Defeat and isolated
ResolutionFault/PresentationUnavailable runs supply Scenario D/E evidence.

A temporary Editor-only PIE Automation harness used the formal
`ViewModel->RequestEndTurn()` request, waited until the Controller owned a real active
token, then called public `WidgetInstance->SkipPresentation()`. It verified input
locked/Resolving before Skip, no waiting or backlog after reconcile, cleared Blueprint
transient/type/token fields, rejection of the stale token beyond the timer window,
a subsequent real request completing normally, and final Idle/input unlocked. It
constructed no Record/Payload, was deleted afterward, and the standard Editor build
succeeded with no Source diff. At this batch boundary the final-head gates had not
yet run; their later seal evidence follows.

### Final-head UI-A2E / UI-A2 seal gates — 2026-08-31

```text
Implementation commit 81cbfb6af09a52f96ececff597491c5bfcc3665f
WBP_BattleHUD SHA-256 990125C951D52D5F23194D9EB7C079C2F3C514C78A285DF0DDE273B6B1C0F94A

Phase6UIA2D5: exactly 6/6 successful, 0 failed, 0 notRun
Report: Saved/AutomationReports/FinalA2D5/index.json

Phase6R prefixes: 13+23+12+5+8+8+8+3+4+4+6+6 = 100/100 successful
0 failed, 0 notRun, all Editor exits 0
Reports: Saved/AutomationReports/FinalSeal_Phase5 through FinalSeal_Phase6UIA2D5

Clean-worktree Win64 Shipping build: exit 0
Forbidden SlayTheSpireDemoTests / Phase6ATest artifact hits: 0
Runtime UPhase6ATest hits: 0
```

An earlier nonexistent single prefix `SlayTheSpireDemo.Phase6R` matched zero tests
and is not aggregate evidence. The valid aggregate is the formal twelve-prefix
workflow above. Likewise, a first Shipping scan in the main workspace was rejected
because it saw pre-existing Editor-test artifacts; the valid Shipping gate used a
clean detached worktree at the same implementation commit, matching the workflow's
clean-checkout boundary. That worktree was removed after validation.

With the saved Blueprint/PIE evidence plus these final-head gates, UI-A2E and UI-A2
are **COMPLETE / VALIDATED / SEALED**.

### Phase 6UI-A2N R2 Native HUD shell — 2026-08-31

R2 implementation commit `d15287ec068f699390a4f64cfab824dcbe53980b`
adds only the Native HUD/Card/Status shells, their Designer-backed duplicate assets,
and the non-production `L_BattleTest_Native` map. Production remains on
`L_BattleTest` and `WBP_BattleHUD_C`.

```text
UE 5.8 project-file regeneration: PASS
SlayTheSpireDemoEditor Win64 Development build: PASS
Native HUD/Card/Status Blueprint compile + save: PASS

WBP_BattleHUD_Native: parent UBattleHUDWidget, 75 Designer Widgets,
  one empty EventGraph, 23 required bindings, 6 optional bindings
WBP_BattleCard_Native: parent UBattleCardWidget, 20 Designer Widgets,
  one empty EventGraph
WBP_BattleStatus_Native: parent UBattleStatusWidget, 4 Designer Widgets,
  one empty EventGraph

Native PIE map: /Game/SlayTheSpireDemo/Maps/L_BattleTest_Native
runtime WidgetClass: WBP_BattleHUD_Native_C
runtime WidgetInstance: WBP_BattleHUD_Native_C_0
ViewModel / PresentationController assembly: created through the existing Presenter
Native binding / ensure / Blueprint / UMG errors: 0

Focused SlayTheSpireDemo.Phase6UIA2A:
8 total, 3 succeeded, 5 succeededWithWarnings, 0 failed, 0 notRun
Report: Saved/AutomationReports/R2FocusedPhase6UIA2A/index.json
```

The independent R2 architecture review found no P0/P1 blocker. It recorded one
non-blocking migration residue: duplicated assets still contain unexecuted Legacy
member variables even though their business graphs are empty. Those variables must
be taken over or removed in the applicable later ownership phase; R2 does not expand
into the R4/R5/R9 behavior or the R14 cleanup boundary.

### Phase 6UI-A2N R3-A Static HUD and long-lived delegates — 2026-08-31

R3-A implementation commit is based on `e0ac820245e8ea93128507f058316e32c5aaf427`
and changes only `UBattleHUDWidget` static refresh and long-lived input ownership.
Production remains on `L_BattleTest` / `WBP_BattleHUD_C`; the Native checks use only
`L_BattleTest_Native` / `WBP_BattleHUD_Native_C`.

```text
SlayTheSpireDemoEditor Win64 Development build: PASS (Result: Succeeded)
CompileAllBlueprints: PASS (0 errors, 0 warnings, 0 failed blueprints)
Native PIE: PASS
  initial Player 80/80, Enemy 100/100, Energy 5/5
  TestAttack -> Enemy 94/100, Energy 4/5
  EndTurn -> real turn-ending commit, next ReadStateReady, Player 74/80, Energy 5/5
  Draw / Discard / Exhaust count surfaces remained ViewModel-consistent
  target handler -> frozen "Choose a legal target." feedback
  Confirm and Cancel handlers each invoked once on Native instance
  enemy inspection surfaced frozen name and cleared cleanly
NativeOnBattleHUDViewModelChanged: Native refresh only; no Legacy BP refresh
delegates: one NativeConstruct AddUniqueDynamic boundary, matching NativeDestruct removal
Legacy HUD/Card/Status hashes: unchanged from sealed baseline
```

Runtime log evidence is in `Saved/Logs/SlayTheSpireDemo.log`; local PIE captures are
under `Saved/Screenshots/WindowsEditor/`. R3-A deliberately did not rerun A2D5,
Phase6R or Shipping. R3-A is **COMPLETE / VALIDATED**; R4 remains NOT STARTED.

After the final PIE, the Editor returned to formal `L_BattleTest`; its Presenter and
the `BP_BattleHUDPresenter` default still resolve to `WBP_BattleHUD_C`. The three
Legacy WBP hashes remain the sealed R0 values.

### Phase 6UI-A2N R3-A review fixes — focused validation — 2026-08-31

The R3 review found two parity gaps: combatant inspect did not rebuild the optional
status tooltip from frozen `CombatantView.Statuses`, and Block `0` left the Designer
shield badge visible. The review-fix branch corrected only those Native static HUD
surfaces and added permanent Editor-only probes; it did not enter R4 Hand/Card,
R7 Damage playback, or R9 formal Status-row lifecycle.

The user rebuilt the saved review-fix branch on UE 5.8 and ran the focused prefix:

```text
SlayTheSpireDemo.Phase6UIA2N.R3.BlockBadge                 PASS
SlayTheSpireDemo.Phase6UIA2N.R3.StatusTooltip              PASS
SlayTheSpireDemo.Phase6UIA2N.R3.Terminal                   PASS
SlayTheSpireDemo.Phase6UIA2N.R3.PresentationUnavailable    PASS

4/4 PASS
```

The focused evidence proves that zero/positive/zero Block toggles the complete
Designer badge, inspect receives the frozen Status DTO and clears cleanly, all four
Terminal outcomes render from ViewModel state, and PresentationUnavailable remains
input-locked and visually distinct from ResolutionFaulted. The Editor build for this
saved review-fix head also passed.
This closes the R3 review findings. It does not replace the earlier Native WBP/PIE
acceptance; together they keep R3-A **COMPLETE / VALIDATED**.

### Phase 6UI-A2N R4 Native Card / Hand — 2026-08-31

R4 moved only formal Hand/Card display and request ownership into the Native stack.
The user completed Editor Build, `WBP_BattleCard_Native` and `WBP_BattleHUD_Native`
compile, focused R4 Automation, and Native PIE interaction acceptance. Exact
RuntimeId selection, target/cancel behavior, accepted single submission, no duplicate
card callback, and the R3 zero-Block regression all passed. Production remained on
Legacy and committed Record playback remained immediate-fallback.

R4 is **COMPLETE / VALIDATED**. Detailed evidence is in
`docs/R4NativeCardHandValidation.md`.

### Phase 6UI-A2N R5 Native Playback Kernel — 2026-08-31

R5 added only the Native HUD local playback ownership kernel: exact active Token and
Record type, local finish timer, exact-token Finish/Cancel, failed-Begin rollback,
and destruction cleanup. It did not migrate any real Record visual or copy Controller
queue/reducer/WorkingSnapshot/generation/timeout authority into the HUD.

The first corrected-build cycle exposed and fixed one UE5.8 delegate-binding mismatch:
the finish timer now uses a weak lambda that captures the exact Token by value. The
user then completed the final R5 gates:

```text
SlayTheSpireDemoEditor Win64 Development build: PASS
WBP_BattleHUD_Native targeted compile: PASS
SlayTheSpireDemo.Phase6UIA2N.R5: 4/4 PASS
L_BattleTest_Native minimal PIE smoke: PASS
```

The focused suite validates unsupported/failed Begin zero side effects, exact and
wrong-token Cancel, Cancel without normal completion Notify, duplicate/stale Finish,
old/new Token isolation, local timer ownership, and NativeDestruct cleanup. The PIE
smoke confirmed normal Native HUD/Hand startup, card-selection Cancel, EndTurn,
continued interaction, and no crash, permanent lock, duplicate Hand or blank HUD.

R5 is **COMPLETE / VALIDATED**. Detailed evidence is in
`docs/R5NativePlaybackKernelValidation.md`.

### Phase 6UI-A2N R6 Energy / Block / Shuffle — 2026-08-31

R6 migrated only `EnergyChanged`, `BlockChanged` and `DeckShuffled` into the Native
HUD. The handlers consume frozen Record payload plus the required frozen historical
Before state, reuse the R5 exact-token kernel, render frozen After on Begin/Finish,
and restore frozen Before on exact Cancel without normal completion Notify.

Validation evidence:

```text
SlayTheSpireDemoEditor Win64 Development build: PASS
WBP_BattleHUD_Native targeted compile: NOT REQUIRED
  (no runtime reflected binding/API contract changed)
SlayTheSpireDemo.Phase6UIA2N.R6: 5/5 PASS
L_BattleTest_Native minimal R6 PIE: PASS
```

Focused coverage includes Player/Enemy Block and zero-badge behavior, Energy and
Shuffle Before/After/Cancel, invalid payload/target/token zero-side-effect Begin,
stale/duplicate Finish, wrong/exact Cancel, exact completion ownership cleanup and
NativeDestruct cleanup. The manual PIE confirmed Energy, Block and real Shuffle final
values with no visible flashback, duplicate display, permanent Input Lock or abnormal
HUD state.

R6 is **COMPLETE / VALIDATED**. Detailed evidence is in
`docs/R6NativeEnergyBlockShuffleValidation.md`.

### Phase 6UI-A2N R7 Native Damage — 2026-08-31

R7 migrated only the committed `Damage` Record into the Native HUD. The handler
resolves the exact frozen `TargetPresentationId`, validates historical HP/Block Before
against the frozen ViewModel, consumes the Record's `IncomingDamage`, `HPBefore /
HPAfter` and `BlockBefore / BlockAfter` directly, and never derives committed HP or
Block outcomes from IncomingDamage.

Validation evidence:

```text
SlayTheSpireDemoEditor Win64 Development build: PASS
WBP_BattleHUD_Native targeted compile: NOT REQUIRED
  (no runtime reflected binding/API contract changed)
SlayTheSpireDemo.Phase6UIA2N.R7: 5/5 PASS
L_BattleTest_Native minimal R7 Damage PIE: PASS
```

Focused coverage includes Player and Enemy targets, ordinary Damage, full Block
absorption with unchanged HP, lethal overkill, exact Cancel historical restore,
wrong-token Cancel, stale/duplicate Finish, next-Record isolation, invalid target /
payload / Before / Token zero-side-effect Begin, and NativeDestruct cleanup. The
manual PIE confirmed one correctly targeted Damage number/feedback, correct final
HP/Block, no duplicate/flashback, and no permanent Input Lock.

R7 is **COMPLETE / VALIDATED**. Detailed evidence is in
`docs/R7NativeDamageValidation.md`.

### Phase 6UI-A2N R8 Native Card Lifecycle — 2026-09-01

R8 migrated the committed `CardPlayed` and `CardZoneChanged` facts together while
keeping them independent exact-token playback units. All card visuals consume only
the frozen card snapshot, Record indices/counts and frozen ViewModel Before state.
Presentation cards are noninteractive, `HitTestInvisible`, and never become formal
Gameplay-playable Hand cards.

The initial implementation correctly serialized one-card-at-a-time DrawPile-to-Hand
presentation, but the first manual pass found that the non-Draw paths still used
immediate visibility changes. The correction added the required Slay-the-Spire-like
movement/retirement cues:

```text
Hand -> PlayArea:                  move / scale / fade into centered PlayArea
Hand -> DiscardPile:               move / fade toward DiscardPile
PlayArea -> DiscardPile:           move / fade toward DiscardPile
PlayArea -> Exhaust/RemovedPile:   scale / fade out at PlayArea
DrawPile -> Hand:                  exactly one Record/card moves to exact ToIndex
```

A later narrow review found one P1 cleanup gap: after exact `CardPlayed` Finish, the
cross-Record `NativePlayedCardWidget` intentionally survives for a later PlayArea
destination. If a subsequent Record was abandoned through `SkipPresentation` /
fail-safe exact Cancel, that retained PlayedCard could survive Controller collapse.
The exact native Cancel boundary now retires any retained PlayedCard after the
current Record type-specific Cancel and before local ownership is cleared. Wrong or
stale Token cancellation still returns before this cleanup.

Validation evidence after the P1 fix:

```text
SlayTheSpireDemoEditor Win64 Development build: PASS
WBP_BattleHUD_Native / WBP_BattleCard_Native targeted compile: NOT REQUIRED
  (no production reflected binding/API contract changed)
SlayTheSpireDemo.Phase6UIA2N.R8: 6/6 PASS
L_BattleTest_Native corrected minimal R8 Card lifecycle PIE: PASS / sticky
```

Focused coverage includes exact RuntimeId/CardId/HandIndex identity, duplicate
RuntimeId/wrong CardId rejection, no duplicate Energy visual, supported/unsupported
zone pairs, exact/stale Token behavior, Finish/Cancel historical cleanup,
noninteractive presentation cards, strict per-Record consecutive draws,
transform/opacity progress for every migrated lifecycle path, NativeDestruct cleanup,
and the new `Zone.SkipClearsRetainedPlayedCard` regression proving Skip clears both
the active Draw transient and the previously retained PlayedCard.

The corrected manual PIE confirmed Hand-to-PlayArea-to-Discard movement, Exhaust
disappearance at PlayArea, end-turn/manual discard movement, strictly serial draws,
correct final Hand/HUD state, and no flashback, duplicate card, transient leak,
abnormal HUD, or permanent Input Lock. It remained valid after the P1 fix because
normal visual paths did not change.

R8 is **COMPLETE / VALIDATED**. Detailed evidence is in
`docs/R8NativeCardLifecycleValidation.md`.

### Phase 6UI-A2N R9 Native Status Lifecycle — 2026-09-01

R9 migrated formal Native Status-row ownership and committed `StatusChanged`
presentation only. `UBattleStatusWidget` owns a frozen `FBattleHUDStatusView` and
Designer-backed amount/icon rendering. HUD lookup uses the sealed identity:

```text
TargetPresentationId + StatusId + RuntimeSequence
```

Create requires the exact identity to be absent. Increase/reduction/removal require
one exact historical ViewModel status and one exact formal Widget; update/reduction
reuse the same Widget and removal collapses only that exact identity. Exact Cancel
rebuilds both Player and Enemy formal Status rows from the historical ViewModel and
never reverse-computes `B -> A`.

Validation evidence:

```text
SlayTheSpireDemoEditor Win64 Development build: PASS
WBP_BattleStatus_Native targeted compile: PASS
SlayTheSpireDemo.Phase6UIA2N.R9: 5/5 PASS
L_BattleTest_Native minimal R9 Status lifecycle PIE: PASS
```

Focused coverage includes frozen DTO/identity, create, increase, exact Widget reuse,
`2 -> 1` reduction, `1 -> 0` removal, same `StatusId` with new `RuntimeSequence`,
invalid target/identity/flags/reason zero-side-effect fallback, Player+Enemy historical
Cancel rebuild, wrong-token Cancel, stale/duplicate Finish, next-Record isolation and
NativeDestruct cleanup. The manual PIE accepted the real Status lifecycle with one
correct row/icon/amount, exact-identity reuse, disappearance at zero, coherent
row/icon/tooltip presentation, no `A -> B -> A` flashback, no duplicate Status, no
abnormal HUD and no permanent Input Lock.

R9 is **COMPLETE / VALIDATED**. R10 remains **NOT STARTED**. Detailed evidence is in
`docs/R9NativeStatusLifecycleValidation.md`.

On local branch `codex/A2E-continue`, with the saved StatusChanged update/reduction and Cancel-restoration HUD asset plus uncommitted documentation changes, the focused `SlayTheSpireDemo.Phase6UIA2D5` suite was rediscovered as exactly six tests and actually run:

```text
Terminal.Defeat             PASS
Terminal.ResolutionFault   PASS
Terminal.Victory           PASS
CardStatusIntegration      PASS
StatusLifecycle            PASS
TurnCycleOrdering          PASS

6 passed / 0 failed / 0 skipped
total duration 0.108884 s
```

This historical run was current-working-tree focused regression evidence only. It is
not `final-head`; the current Status and Scenario A-E Blueprint/PIE evidence is
recorded above, and the valid final-head A2D5 run is recorded in the seal section.

## Trusted Manual Evidence

- Normal UI player → enemy → player turn loop passed in PIE.
- Self-target Defend → highlighted Player selection passed in PIE.
- Packaged Defend dynamic `{Block}` description passed.

These manual results predate unified UI-A2 committed-record playback and therefore do **not** close UI-A2E.

## Current Acceptance Boundary

UI-A2A/A2B/A2C/A2D C++ committed-presentation contracts are sealed by focused and
aggregate Automation evidence. UI-A2E unified Blueprint/UMG routing and actual PIE
Scenario A-E/Cancel acceptance are now validated on HUD hash `990125C9...`.
UI-A2E and UI-A2 are **COMPLETE / VALIDATED / SEALED** on implementation commit
`81cbfb6` after A2D5 exactly 6, Phase6R 100/100, and clean-worktree Shipping
exclusion all passed.

A2N migration status is now:

```text
R0-R9 COMPLETE / VALIDATED
R10+ NOT STARTED
```

Validated A2E scenarios include:

- ordinary card Damage;
- card plus Status creation/update/reduction/removal;
- complete EndTurn macro Envelope, including Block/Energy/zone/shuffle behavior as applicable;
- Victory and Defeat;
- genuine ResolutionFault distinct from PresentationUnavailable;
- input remains locked during playback and unlocks only after catch-up to the newest matching revision.

Use `docs/UIA2ERemainingSteps.zh-CN.md` for historical UI-A2E execution evidence and
`docs/ValidationExecutionPolicy.md` for current A2N validation budgeting and manual/
automated Gate ownership.

## User-Action Boundary

When UE Editor work is required but unavailable to the agent, label it `USER ACTION REQUIRED` and provide exact asset/menu paths, graph/function names, nodes, pins, property values, compile/save order, expected results and requested logs/screenshots.