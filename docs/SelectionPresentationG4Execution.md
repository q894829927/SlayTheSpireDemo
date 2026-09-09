# Selection Presentation G4 Execution Record

Date: **2026-09-09**

Status:

```text
IMPLEMENTED / VALIDATION PENDING
```

Baseline:

```text
main@dabb1c078d89cae19bbded980cba691ee253e660
G3 COMPLETE / VALIDATED / SEALED
```

## Scope

G4 only: generic per-child card-transition engine plus a source resolver capable
of consuming either the formal Hand source or a durable SelectionArea source.

Explicitly not included:

```text
G5 production SelectionArea ownership activation
G6 visible/parallel Group playback
G6 automatic Transition -> ConsumedPendingReducer child lifecycle
G7 compatibility-field / old adapter deletion
G8 cross-Resolution early input / pipelining
Gameplay selection, queue, reducer or Presentation recording changes
production .uasset/.umap edits
```

G0-G3 remain sealed unless a concrete G4 regression implicates their contract.

## Implemented contract

### Generic SingleRecord child

`UBattleHUDCardTransitionWidget` is inserted between the reconciled Native HUD and
`UBattleHUDSelectionWidget`. Production Selection HUD SingleRecord playback for:

```text
Hand -> DrawPile
Hand -> DiscardPile
Hand -> ExhaustPile
```

now uses one `FNativeCardTransitionInstance` child instead of destination-specific
Selection animation ownership or the old global Hand-zone moving-card state.

The child freezes only immutable Presentation facts and local visual state:

```text
RuntimeId
FromZone / ToZone
source owner
exact moving visual
historical formal Hand visual
source/destination geometry
scale / opacity / elapsed state
Selection generation only when an explicit SelectionArea owner exists
```

SingleRecord creates exactly one child. G4 does not start N-child Group playback.

### Source resolver

The source resolver supports:

```text
implicit/explicit Hand
    -> validate exact formal Hand RuntimeId/index/snapshot
    -> create one presentation-only moving copy
    -> retain the formal structural Hand child Hidden during playback

SelectionArea(Confirmed)
    -> require exact ownership entry and generation
    -> require exactly one matching visible SelectionArea card object
    -> reparent that exact object to the transition surface
    -> atomically advance SelectionArea -> Transition only after local visual
       preparation and timer ownership have succeeded
```

`Transition` and `ConsumedPendingReducer` sources are rejected so a committed
child cannot be replayed.

Production Selection still uses the G0 compatibility Hand/`ConfirmedCardCenters`
path in G4. The SelectionArea source is executable and test-covered but remains
dormant until G5 performs the production ownership switch.

### Transactional SelectionArea preparation

Before reparenting an exact SelectionArea object, G4 freezes its local render
transform, opacity, visibility and enabled state. A failure after preparation but
before ownership acceptance restores the same object to the persistent Host with
those values intact. No clone is substituted and ownership stays SelectionArea.

The source absolute center is captured before any hide/reparent and converted into
the stable PlayArea coordinate space. The temporary confirmed-position handoff may
override that center during staged Hand-source compatibility. This prevents G4
from reconstructing continuity from CardId or destination-specific Selection code.

### Destination resolver

Destination behavior is driven only by committed `CardZoneChanged` facts:

```text
DrawPile   -> exact ToIndex == historical DrawCount, move/fade to Draw surface
Discard    -> exact ToIndex == historical DiscardCount, move/fade to Discard surface
Exhaust    -> exact ToIndex == historical ExhaustCount, fade at source position
```

There is no CardId, Effect or SelectionSource animation branch.

### Existing adapters retained

G4 deliberately leaves these existing R8 paths on their current adapters:

```text
CardPlayed
DrawPile -> Hand
PlayArea -> Discard / Exhaust / Removed
```

The old Selection Hand->Draw helper fields/functions remain as a dormant
compatibility shell with an explicit G7 deletion target. Its Begin entry delegates
to the generic engine. G4 does not combine behavior migration with broad legacy
field deletion.

## Focused Automation source

The focused `SlayTheSpireDemo.CardSelection.Presentation` prefix now contains the
existing two input-routing tests plus five presentation tests, for an expected
current discovery count of **7**:

```text
SlayTheSpireDemo.CardSelection.Presentation.Input.PendingBoundaryBlocksOrdinaryCardPlay
SlayTheSpireDemo.CardSelection.Presentation.Input.ProductionConfirmRouting
SlayTheSpireDemo.CardSelection.Presentation.HandToDraw.GenericTransferFinishAndCancel
SlayTheSpireDemo.CardSelection.Presentation.HandToExhaust.GenericFadeAtSource
SlayTheSpireDemo.CardSelection.Presentation.HandToDiscard.GenericMovementAndDestinationValidation
SlayTheSpireDemo.CardSelection.Presentation.G4.SelectionAreaExactVisualTransfer
SlayTheSpireDemo.CardSelection.Presentation.G4.SelectionAreaPrepareRollback
```

The tests cover immutable identity, exact destination index, one-child ownership,
exact/stale token behavior, cancel/finish cleanup, exact SelectionArea object
transfer, failed-prepare rollback and no replay from Transition/Consumed ownership.
They do not claim visual movement/layout acceptance.

## Validation required

Per `docs/ValidationExecutionPolicy.md`, G4 changes C++ production presentation
behavior and has a visual Gate.

### AUTOMATED GATES

```text
1. SlayTheSpireDemoEditor Win64 Development build
2. Automation RunTest SlayTheSpireDemo.CardSelection.Presentation
   expected current discovery: 7
```

No G0/G1/G2/G3 rerun is required absent a concrete implicated failure. The Wave1CC1
Gameplay/reducer code and its proving tests are unchanged; the focused G4 suite
constructs the same immutable Hand->DrawPile Record shape at the changed HUD
boundary. Warcry itself is covered by the manual visual Gate below.

### MANUAL PIE GATE — USER ACTION REQUIRED

Use the production Native HUD in `L_BattleTest` and exercise Warcry's current
SingleRecord selection flow:

```text
play Warcry
-> newly drawn card remains a legal candidate
-> select one Hand card and Confirm
-> selected card visibly departs from its confirmed displayed position toward DrawPile
-> no snap/flash back to the ordinary Hand position
-> no duplicate/ghost card
-> input/presentation continues normally after the transfer
-> the played Warcry card later follows the ordinary existing PlayArea cleanup path
```

Also observe one available Hand->Exhaust or Hand->Discard committed transition if
that scenario is already exposed by the current battle setup: movement/fade should
start from the visible source without duplicate/flash. Do not add assets or debug
Gameplay solely to manufacture this optional second observation.

G4 must not be marked COMPLETE / VALIDATED / SEALED until the build, focused
Automation and required Warcry PIE visual evidence are actually reported.

## Next stage after seal

```text
G5 — production SelectionArea ownership
     + Pending/Confirmed exact visible object in persistent Host
     + transactional Confirm/outcome correlation
     + generic SingleRecord SelectionArea -> Transition consumption
```
