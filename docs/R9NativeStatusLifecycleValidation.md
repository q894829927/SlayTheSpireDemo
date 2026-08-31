# Phase 6UI-A2N — R9 Native Status Lifecycle

Status:

```text
R8 COMPLETE / VALIDATED
R9 SOURCE IMPLEMENTED / AUTOMATED VALIDATION PENDING
R10 NOT STARTED
```

Implementation branch: `main` (explicitly authorized; no R9 working branch)
Starting main HEAD: `b05a7d1281e921eed2bbb4bf5238842fa16421f7`
Implementation date: **2026-09-01**

R9 migrates only formal Native Status-row ownership and committed
`StatusChanged` presentation. It does not modify Gameplay, Controller, reducer,
Presentation Record/Envelope contracts, Legacy WBP assets, production WidgetClass,
Terminal/PresentationUnavailable behavior, UI-A3, or R10+ behavior.

## Implemented boundary

`UBattleStatusWidget` now owns only one supplied frozen `FBattleHUDStatusView` and
its Designer-backed amount/icon rendering. It exposes native `StatusId` and
`RuntimeSequence` getters and never queries Gameplay or owns merge/decay/remove
rules.

The Native class intentionally uses native-only storage names instead of the
Legacy duplicate's retained `StatusView`, `CurrentStatusView` and `MID_StatusIcon`
member names. This follows the same migration-residue rule used by Native Card and
does not require editing the frozen Legacy asset.

Formal Player/Enemy Status rows are rebuilt from the historical ViewModel through:

```text
ViewModel.Player.Statuses -> WB_PlayerStatuses
ViewModel.Enemy.Statuses  -> WB_EnemyStatuses
```

Each formal row is a `StatusWidgetClass` instance using the frozen DTO. The UI does
not create a second status model.

## Exact identity

R9 preserves the sealed identity:

```text
TargetPresentationId
+ StatusId
+ RuntimeSequence
```

Lookup is deliberately two-stage:

```text
TargetPresentationId
-> exact Player or Enemy formal Status container
-> exact StatusId + RuntimeSequence Widget inside that container
```

`TargetPresentationId` was not added to `FBattleHUDStatusView`. StatusId-only,
array-index and DisplayName lookup are not used.

## StatusChanged playback

The handler validates Record/Token metadata, exact target/source identity, positive
RuntimeSequence, frozen before/after values, created/removed flags, frozen
description boundaries, and the same reason contract used by the authoritative
Status producer.

Supported transitions are:

```text
0 -> N       create
A -> B       increase
A -> B > 0   reduction
A -> 0       removal
```

Create requires the exact identity to be absent from both the historical ViewModel
and the target formal container. It creates one frozen Native Status Widget only for
this Record.

Update/reduction/removal require exactly one historical Status and exactly one
formal Widget with the same `StatusId + RuntimeSequence`, and require the formal
Widget's complete frozen Before view to match the historical ViewModel/payload.
Update/reduction reuses that exact Widget. Removal reuses and collapses only that
exact Widget; it does not remove an arbitrary same-StatusId row.

Every invalid target, identity, flags/reason or historical Before mismatch returns
`false` before committed local ownership/visible mutation so Controller immediate
fallback remains available.

## Finish / Cancel / destruction

R9 reuses the sealed R5 exact-token playback kernel.

```text
stale / duplicate Finish -> no-op
wrong-token Cancel -> no-op
exact Finish -> keep frozen committed After, clear R9 local state, exact Notify once
exact Cancel -> rebuild BOTH Player and Enemy formal rows from historical ViewModel,
                clear local R9 state, never Notify
NativeDestruct -> local transient/reference cleanup only; no historical restore,
                  no normal completion Notify
```

Cancel never reverse-calculates `B -> A`.

The R8 exact-Cancel tail that retires a retained cross-Record PlayedCard remains in
place, so a Status Cancel/Skip cannot reintroduce the R8 PlayArea leak.

## Focused Automation authored

Prefix:

```text
SlayTheSpireDemo.Phase6UIA2N.R9
```

Five focused Editor-only tests are authored:

```text
StatusWidget.DTOAndIdentity
Lifecycle.CreateIncreaseReuse
Lifecycle.ReductionRemovalAndCancel
Identity.NewSequenceAndInvalidFallback
Token.StaleAndDestructCleanup
```

Coverage includes frozen Status DTO/identity, create, increase, exact Widget reuse,
2 -> 1 reduction, 1 -> 0 removal, Player+Enemy historical Cancel rebuild,
wrong-token Cancel, same StatusId with a later new RuntimeSequence, wrong target,
wrong RuntimeSequence, invalid flags/reason zero-side-effect fallback, stale Finish
and destruction-local cleanup.

These tests are source only at this point. No PASS claim is made until UE5.8 runs
them.

## AUTOMATED GATES — PENDING

Run only the R9 closed-scope gates:

```text
1. SlayTheSpireDemoEditor Win64 Development build
2. Compile WBP_BattleStatus_Native
   - required because R9 adds reflected Native Status API / BindWidget ownership
3. SlayTheSpireDemo.Phase6UIA2N.R9 focused Automation
   - expected discovery: 5 tests
```

Do not automatically run R3-R8, A2D5, Phase6R, Shipping, Scenario A-E, full
Blueprint compilation or an architecture reviewer.

If a Gate fails, fix only the failed contract and rerun only the Gate(s) invalidated
by that fix.

## MANUAL PIE GATE — PENDING

After all automated Gates pass, perform one focused visual pass in:

```text
/Game/SlayTheSpireDemo/Maps/L_BattleTest_Native
```

Use existing real Status producers and verify:

1. Status creation displays one correct row/icon/amount.
2. Same exact identity update/reduction reuses one row; no duplicate icon.
3. Observe a reduction such as `2 -> 1`.
4. At `1 -> 0`, the exact Status disappears.
5. Row/icon and existing Status tooltip appearance remain coherent.
6. No `A -> B -> A` flashback, duplicate Status, abnormal HUD or permanent input lock.

Do not replace this visual Gate with repeated screenshots.

## Current acceptance state

```text
R9 SOURCE IMPLEMENTED
AUTOMATED VALIDATION PENDING
MANUAL PIE PENDING
R10 NOT STARTED
```

R9 must not be marked `COMPLETE / VALIDATED` until the required Build,
`WBP_BattleStatus_Native` compile, focused R9 Automation and user-confirmed minimal
PIE all pass. Do not start R10 automatically.
