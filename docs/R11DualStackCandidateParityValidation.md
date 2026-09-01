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
Map:         /Game/SlayTheSpireDemo/Maps/L_BattleTest
WidgetClass: WBP_BattleHUD_C
```

Native candidate remains isolated to:

```text
Map:         /Game/SlayTheSpireDemo/Maps/L_BattleTest_Native
WidgetClass: WBP_BattleHUD_Native_C
```

No Runtime source, Gameplay, Controller, reducer, Record/Envelope schema, Legacy WBP,
production map/configuration, or UI-A3 change is required by R11 unless a concrete
parity failure proves a defect that must be fixed.

## Production-freeze preflight — PASS

A repository compare from the sealed Legacy implementation
`81cbfb6af09a52f96ececff597491c5bfcc3665f` through the R10 completion head
`08d6fc003701e485ef37414d2ac79ba8a436d3cb` shows the expected Native additions and
shared-base/test/document changes only. The compare does not list the production
`L_BattleTest.umap`, `WBP_BattleHUD.uasset`, `WBP_BattleCard.uasset`,
`WBP_BattleStatus.uasset`, or production config files as changed.

This is a static repository preflight only. It does not replace local asset hash,
Blueprint compile/save, Automation, or PIE evidence.

## Formal parity contract

Run the Legacy and Native candidate configurations with the same real Gameplay/UI
Request producers. Compare observable contracts only:

```text
Scenario A-E
active Skip
active Cancel
stale callback rejection
Input Unlock after catch-up

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

## Scenario A-E manual dual-stack parity — PASS

The user completed the complete Scenario A-E matrix on both Legacy and Native PIE on
2026-09-01 and confirmed all five scenarios successful.

```text
Scenario A: PASS
  normal Strike/card/damage flow matched between Legacy and Native

Scenario B: PASS
  Uppercut/status lifecycle matched between Legacy and Native
  no duplicate status row or visible lifecycle flashback was reported

Scenario C: PASS
  full EndTurn discard/enemy-turn/draw/shuffle/catch-up flow matched

Scenario D: PASS
  Victory and Defeat terminal ordering matched
  lethal HP reached the visible terminal condition before the terminal surface

Scenario E: PASS
  ResolutionFault -> 战斗结算异常
  PresentationUnavailable ->
    Could not freeze the exact player-facing Presentation snapshot.
  PresentationUnavailable did not render 战斗结算异常
```

Therefore:

```text
Legacy Scenario A-E: PASS
Native Scenario A-E: PASS
Legacy-vs-Native observable Scenario A-E parity: PASS
```

This evidence is manual PIE evidence confirmed by the user; it is not inferred from
Automation.

## Temporary R11 PIE validation harness

R11 keeps all temporary execution code inside the Editor-only test module:

```text
Source/SlayTheSpireDemoTests/Private/Phase6UIA2NR11PIECommands.cpp
Source/SlayTheSpireDemoTests/SlayTheSpireDemoTests.Build.cs
```

It registers:

```text
A2N.R11.ForceResolutionFault
A2N.R11.ForcePresentationUnavailable
A2N.R11.TestSkip
A2N.R11.TestCancelStale
```

`A2N.R11.ForceResolutionFault` finds the active PIE `ABattleManager`, verifies a real
EndTurn request is legal, arms the existing
`SetForceInvalidEnemyTurnBatchForTesting(true)` seam, then calls the real
`RequestEndPlayerTurn()` producer. It constructs no synthetic Record/Payload.

`A2N.R11.ForcePresentationUnavailable` begins an isolated system Presentation
resolution, enables the existing forced snapshot-freeze failure seam, seals the
resolution, then clears the force flag. It enters the existing
PresentationUnavailable path without manufacturing a ResolutionFault.

`A2N.R11.TestSkip` issues a real `Widget->EndTurn()` request, waits until the
Controller owns a valid active Token, calls the formal `Widget->SkipPresentation()`
path, and asserts synchronous playback/backlog collapse, exact FinalSnapshot HUD
state, Controller watermark catch-up, unlocked Idle input, and acceptance of a
second real Widget EndTurn request.

`A2N.R11.TestCancelStale` issues the same real request, captures Token A, enters the
formal `ExpireActivePlaybackForTesting() -> HandleActiveTimeout()` path, waits for
distinct Token B, and deliberately calls `Widget->NotifyPresentationFinished(A)`.
It asserts that B remains the exact active owner, then waits for natural catch-up
and proves the same FinalSnapshot, queue and post-catch-up input contract.

The same harness has an optional command-line bootstrap used only for isolated,
unattended in-process PIE validation. `R11TemporalMap` requests the chosen Legacy or
Native test map after the Editor module is loaded; `R11TemporalTest=Skip` or
`CancelStale` runs the corresponding formal Widget path. The added `UnrealEd`
dependency exists only to start that in-process PIE session from this Editor-only
module; no Runtime module depends on it.

The harness changes no Runtime module, reflected Gameplay API, Blueprint asset,
production map, or production WidgetClass. It is temporary R11 validation
infrastructure and must be deleted after R11 parity closes and before R12 production
cutover validation begins.

## Temporal protocol validation — PASS

On 2026-09-01, the two temporal tests were executed once on each formal map as
unattended in-process PIE. All four runs passed:

```text
Legacy / L_BattleTest / WBP_BattleHUD_C
  active Skip + FinalSnapshot catch-up + Input Unlock: PASS
  active timeout Cancel + stale Token A + natural catch-up + Input Unlock: PASS

Native / L_BattleTest_Native / WBP_BattleHUD_Native_C
  active Skip + FinalSnapshot catch-up + Input Unlock: PASS
  active timeout Cancel + stale Token A + natural catch-up + Input Unlock: PASS
```

Both stacks captured a real Token A at `Resolution=2, Sequence=1, Generation=3`.
Both Cancel/stale runs advanced to a distinct Token B, delivered the abandoned A
callback after B became active, and observed B remain active. All runs ended with:

```text
Controller waiting=false
Controller backlog=0
completion watermark caught up
ViewModel exactly equals latest frozen FinalSnapshot
InteractionState=Idle
bInputLocked=false
bCanEndTurn=true
real post-catch-up Widget EndTurn accepted
```

This is deterministic state/protocol evidence from real PIE producers. Because the
runs used an unattended no-rendering configuration, they do **not** claim the
genuinely visual checks for visible flashback, duplicate Hand/Status widgets, or the
appearance of the interrupted animation. Those observations remain a minimal user
PIE action and are not replaced by log assertions.

## AUTOMATED GATES — PENDING UNLESS SEPARATELY CONFIRMED

Run once on the R11 candidate head:

```text
1. SlayTheSpireDemoEditor Win64 Development build: PASS

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

## Remaining manual R11 catch-up gate

Scenario A-E and the deterministic temporal protocol are complete. The remaining
manual parity scope is only the visible part of the two temporal flows:

```text
Legacy active Skip: no A -> B -> A flashback or duplicate visual
Legacy active Cancel/stale: no abandoned visual returns or duplicate visual
Native active Skip: no A -> B -> A flashback or duplicate visual
Native active Cancel/stale: no abandoned visual returns or duplicate visual
```

These checks must operate on real active playback produced through formal Gameplay/UI
requests. Do not construct a synthetic Presentation Record merely to satisfy R11.

The commands are now stable manual entry points; run each once while observing PIE:

```text
A2N.R11.TestSkip
A2N.R11.TestCancelStale
```

The harness log must also end in `PASS`; the user observation closes only the visual
portion that deterministic assertions cannot own.

## Candidate acceptance

R11 may be marked `COMPLETE / VALIDATED` only when all of the following are confirmed:

```text
Legacy regression PASS
Native Scenario A-E PASS                         [PASS]
Legacy-vs-Native observable Scenario A-E parity [PASS]
active Skip/Cancel deterministic protocol PASS   [PASS — Legacy + Native]
stale callback rejection PASS                    [PASS — Legacy + Native]
input-unlock after catch-up PASS                  [PASS — Legacy + Native]
temporal visual no-flashback/no-duplicate parity [PENDING USER PIE]
focused Native handler tests PASS                [PENDING unless confirmed]
Editor build PASS                                [PASS]
Blueprint compile/save PASS for all Native WBP   [PENDING unless confirmed]
```

R11 completion does not authorize production cutover by itself. R12-A remains a
separate next phase and must not start automatically.
