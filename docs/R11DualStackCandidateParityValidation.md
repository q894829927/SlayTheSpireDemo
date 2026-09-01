# Phase 6UI-A2N — R11 Dual-Stack Candidate Parity

Status:

```text
R0-R10 COMPLETE / VALIDATED
R11 VALIDATION IN PROGRESS
R12 NOT STARTED
```

Branch: `main`
R11 starting HEAD: `08d6fc003701e485ef37414d2ac79ba8a436d3cb`
Start date: **2026-09-01**

## Purpose

R11 is a validation-only candidate-parity phase. It does not migrate another Record
type and it does not cut production over to the Native HUD.

Production remains:

```text
Map:        /Game/SlayTheSpireDemo/Maps/L_BattleTest
WidgetClass: WBP_BattleHUD_C
```

Native candidate remains isolated to:

```text
Map:        /Game/SlayTheSpireDemo/Maps/L_BattleTest_Native
WidgetClass: WBP_BattleHUD_Native_C
```

No Runtime source, Gameplay, Controller, reducer, Record/Envelope schema, Legacy WBP,
production map/configuration, or UI-A3 change is required by R11 unless a concrete
parity failure proves a defect that must be fixed.

## Formal parity contract

Run the Legacy and Native candidate configurations with the same real Gameplay/UI
Request producers. Both configurations cover:

```text
Scenario A
Scenario B
Scenario C
Scenario D
Scenario E
active Skip
active Cancel
stale callback
Input Unlock
```

Compare only observable contracts:

```text
Record acceptance/rejection
frozen display facts
visible Record order
Cancel historical result
Hand / Energy / HP / Block / Status / pile display
FinalSnapshot surface
terminal timing
input-unlock timing
final Idle/Terminal state
```

Do not require identical private helpers, timer implementation, transient
construction, or internal call order. Final PIE parity must use real producers;
synthetic Records cannot close the R11 parity Gate.

## Scenario matrix

The sealed Legacy UI-A2E definitions are reused unchanged.

```text
Scenario A — normal card/damage
  Strike
  Energy 5/5 -> 4/5
  Enemy HP 100/100 -> 94/100
  input returns after catch-up

Scenario B — card/status lifecycle
  Uppercut
  Enemy HP 100 -> 87
  Weak/Vulnerable 2 -> 1 -> 0 through two real EndTurn requests
  no duplicate status row or A->B->A flashback

Scenario C — full EndTurn macro
  Hand discard sequence
  -> TurnEnded
  -> enemy Damage
  -> draw sequence
  -> shuffle when required
  -> continued draw
  final PlayerTurn / Energy 5/5 / queue caught up

Scenario D — terminal outcomes
  real Victory
  real Defeat
  lethal HP result is visible before terminal overlay

Scenario E — failure separation
  real framework ResolutionFault -> 战斗结算异常
  real PresentationUnavailable -> feedback/error surface only
  PresentationUnavailable must not render the ResolutionFault terminal overlay
```

The Legacy and Native runs do not need identical RuntimeIds between separate PIE
sessions. Each run must preserve its own exact frozen identities and observable
result/order contract.

## AUTOMATED GATES — PENDING

R11 is a Level-3 parity phase, so these broader gates are intentional here.
Run once on the R11 candidate head:

```text
1. SlayTheSpireDemoEditor Win64 Development build

2. Legacy/final-history regression:
   SlayTheSpireDemo.Phase6UIA2D5
   expected discovery: exactly 6 top-level tests
   required: 6/6 PASS, 0 failed, 0 notRun

3. Native focused-handler regression:
   SlayTheSpireDemo.Phase6UIA2N
   required: every discovered R3-R10 Native test PASS, 0 failed, 0 notRun

4. Native Designer assets compile/save:
   WBP_BattleHUD_Native
   WBP_BattleCard_Native
   WBP_BattleStatus_Native
   required: all three compile/save with no errors
```

Do not run Phase6R, Shipping exclusion, production cutover, or R12 acceptance in R11.
Those belong to later cutover/seal boundaries unless a concrete failure explicitly
invalidates a prerequisite.

Passing R3-R10 historical gates remain sticky; the R11 Native prefix is run once here
because the formal candidate Gate explicitly requires the complete focused Native
handler set on the candidate head.

## MANUAL PIE GATES — PENDING

Run the same scenario matrix first on Legacy and then on Native candidate:

```text
Legacy:
  /Game/SlayTheSpireDemo/Maps/L_BattleTest

Native:
  /Game/SlayTheSpireDemo/Maps/L_BattleTest_Native
```

For each matching scenario compare player-visible ordering and final surface/state,
not implementation details.

In addition to Scenario A-E, exercise the existing real-producer validation path for:

```text
active Skip
active Cancel
stale callback rejection
Input Unlock after catch-up
```

The Skip/Cancel/stale checks must operate on a real active playback produced through
formal Gameplay/UI requests. Do not construct a synthetic Presentation Record merely
to satisfy R11.

## Candidate acceptance

R11 may be marked `COMPLETE / VALIDATED` only when all of the following are confirmed:

```text
Legacy regression PASS
Native Scenario A-E PASS
Legacy-vs-Native observable parity PASS
active Skip/Cancel PASS
stale callback PASS
input-unlock PASS
focused Native handler tests PASS
Editor build PASS
Blueprint compile/save PASS for all Native WBP assets
```

R11 completion does not authorize production cutover by itself. R12-A remains a
separate next phase and must not start automatically.
