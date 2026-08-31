# Phase 6UI-A2N — R6 Energy / Block / Shuffle

Status:

```text
R6 COMPLETE / VALIDATED
R7 NOT STARTED
```

Branch: `main`
Starting HEAD: `778073be41ffa0c003cdab5fde9ca1d1ac996cb8`
Source implementation commit: `1250cb411afe640802d7b70239a51228a94ed369`
Validation date: **2026-08-31**

R6 migrates only these committed-presentation Records into the Native HUD:

```text
EnergyChanged
BlockChanged
DeckShuffled
```

Damage, CardPlayed, CardZoneChanged, StatusChanged, terminal Record visuals,
PresentationUnavailable redesign, Controller/Reducer/Record/Envelope changes,
Gameplay changes, production cutover, Legacy WBP edits and UI-A3 remain outside
this phase. R7 and later are not started.

## Implemented boundary

All three handlers reuse the sealed R5 kernel:

```text
validate Record metadata, Token and frozen payload
-> validate required frozen historical Before state and target
-> prepare exact Designer-backed surface
-> CommitNativePresentationOwnership
-> copy frozen Before / After into local visual context
-> apply frozen After
-> StartNativePresentationFinishTimer
-> exact-token Finish or exact-token Cancel
```

Every validation failure occurs before local ownership or visible mutation and
returns `false`, preserving the Controller immediate-fallback path with zero local
visual side effects.

### EnergyChanged

The Native HUD validates non-negative Before/After values, a real transition,
`Delta == EnergyAfter - EnergyBefore`, exact Record/Token metadata, and a frozen
ViewModel Energy matching `EnergyBefore`. Begin and Finish render the Record's
`EnergyAfter`; Cancel renders the Record's `EnergyBefore`. `MaxEnergy` is copied
once from the frozen ViewModel because it is not part of the Energy payload.

No Gameplay Energy query or Before-plus-Delta recomputation is used.

### BlockChanged

The handler resolves `TargetPresentationId` against the frozen Player/Enemy views,
requires the target's frozen historical Block to equal `BlockBefore`, validates the
delta and reason-specific transition, and updates only that target's formal Block
surface.

Both Begin/Finish and Cancel reuse the complete badge rule:

```text
Block > 0 -> complete badge SelfHitTestInvisible
Block == 0 -> complete badge Collapsed
```

Begin/Finish render the Record's `BlockAfter`; Cancel restores the Record's
`BlockBefore`. Player and Enemy targets are both covered.

### DeckShuffled

The handler consumes only the frozen shuffle payload and frozen ViewModel Before
counts. It requires the sealed shuffle transition:

```text
MovedCardCount > 0
DrawBefore = 0
DiscardBefore = MovedCardCount
DrawAfter = MovedCardCount
DiscardAfter = 0
Draw + Discard total is conserved
```

Begin/Finish render frozen Draw/Discard After counts; Cancel restores frozen Before
counts. It does not inspect `UDeckRuntime`, live zones or per-card contents and does
not fabricate card-zone Records.

## Exact-token and cleanup semantics

The R5 timer continues to capture `FPresentationPlaybackToken` by value. R6 adds no
second queue, generation, reducer or snapshot owner.

```text
stale / duplicate Finish -> no-op
wrong-token Cancel -> no-op
exact Finish -> retain After, clear timer/context/ownership, exact Notify
exact Cancel -> restore Before, clear timer/context/ownership, never Notify
NativeDestruct -> local timer/context/ownership cleanup only
```

## Automated Gates — PASS

### Editor Build

```text
SlayTheSpireDemoEditor Win64 Development: PASS
Result: Succeeded
```

The runtime Native HUD reflected binding/API contract did not change. The new
reflected probe exists only in the Editor-only test module, so a targeted
`WBP_BattleHUD_Native` compile was not required by the R6 Gate.

### Focused Automation

Prefix:

```text
SlayTheSpireDemo.Phase6UIA2N.R6
```

Result:

```text
Block:           PASS
DestructCleanup: PASS
Energy:          PASS
InvalidBegin:    PASS
Shuffle:         PASS

5 succeeded
0 failed
0 notRun
```

Evidence:

```text
Saved/AutomationReports/R6FocusedPhase6UIA2N/index.json
```

Coverage includes Energy Before/After/Cancel, Player and Enemy Block, zero-badge
collapse, Block Cancel, Draw/Discard shuffle Before/After/Cancel, invalid payload,
invalid target, inconsistent Record/Token metadata, zero-side-effect false Begin,
exact/stale Finish, wrong/exact Cancel, exact Notify ownership cleanup, Cancel
without normal completion, timer cleanup and NativeDestruct local cleanup.

No R3/R4/R5, A2D5, Phase6R, Shipping or aggregate regression suite was run.

## Manual PIE Gate — PASS

The user confirmed completion of the required minimal PIE pass on **2026-08-31** in:

```text
/Game/SlayTheSpireDemo/Maps/L_BattleTest_Native
```

Accepted observations:

1. Play one card that costs Energy; confirm the final Energy value is correct.
2. Play one card that grants Block; confirm the correct combatant Block value and
   complete badge visibility.
3. Use EndTurn until a real discard-to-draw shuffle occurs; confirm final Draw and
   Discard counts are correct.
4. Confirm there is no visible flashback, duplicate display, permanent Input Lock,
   or abnormal HUD state.

The user confirmation closes the required Energy, Block, Shuffle and HUD-stability
visual checks. No screenshots or additional Automation were substituted for this
manual Gate.

## Current acceptance state

```text
R0-R6 COMPLETE / VALIDATED
R7 NOT STARTED
```

R6 is sealed. Do not start R7 automatically.
