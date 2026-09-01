# Phase 6UI-A2N — R9 Native Status Lifecycle

Status:

```text
R8 COMPLETE / VALIDATED
R9 COMPLETE / VALIDATED
R10 NOT STARTED
```

Implementation branch: `main` (explicitly authorized; no R9 working branch)
Starting main HEAD: `b05a7d1281e921eed2bbb4bf5238842fa16421f7`
Implementation date: **2026-09-01**
Validation confirmed by user: **2026-09-01**

R9 migrates only formal Native Status-row ownership and committed
`StatusChanged` presentation. It does not modify Gameplay, Controller, reducer,
Presentation Record/Envelope contracts, Legacy WBP assets, production WidgetClass,
Terminal/PresentationUnavailable behavior, UI-A3, or R10+ behavior.

## Implemented boundary

`UBattleStatusWidget` owns only one supplied frozen `FBattleHUDStatusView` and
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

## Focused Automation

Prefix:

```text
SlayTheSpireDemo.Phase6UIA2N.R9
```

Five focused Editor-only tests:

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

## AUTOMATED GATES — PASS

The user ran the closed-scope R9 validation on UE5.8 and confirmed all required
automated gates passed:

```text
1. SlayTheSpireDemoEditor Win64 Development build: PASS
2. WBP_BattleStatus_Native compile: PASS
3. SlayTheSpireDemo.Phase6UIA2N.R9 focused Automation: 5/5 PASS
   0 failed / 0 notRun
```

No R3-R8, A2D5, Phase6R, Shipping, Scenario A-E, full Blueprint suite or
architecture reviewer was required for this closed-scope phase.

## MANUAL PIE GATE — PASS

The user confirmed the required focused visual pass in:

```text
/Game/SlayTheSpireDemo/Maps/L_BattleTest_Native
```

The accepted R9 manual gate covers the real Status lifecycle through existing
producers:

```text
Status creation -> one correct row/icon/amount
same exact identity update/reduction -> same Widget reused, no duplicate icon
reduction such as 2 -> 1 -> correct amount
1 -> 0 -> exact Status disappears
row/icon/tooltip appearance remains coherent
no A -> B -> A flashback
no duplicate Status
no abnormal HUD
no permanent Input Lock
```

## Final acceptance state

```text
R0-R9 COMPLETE / VALIDATED
R10 NOT STARTED
```

R9 is **COMPLETE / VALIDATED**. Do not start R10 automatically.