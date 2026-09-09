# Selection Presentation G4 Execution Record

Date: **2026-09-09**

Current delivery authority: `docs/SelectionPresentationG5Execution.md`. After
the failed visual retest below, the user explicitly authorized G5 and coherent
G4+G5 delivery. The historical G4-only gate does not prohibit that migration;
it remains failed/unsealed rather than being retroactively declared passed.

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

### Follow-up: sequential selected Exhaust drift — 2026-09-09

User reproduced drift when the second selected card begins Exhaust after Confirm.
G4 moved Hand->Exhaust from the earlier in-place formal-widget fade to a moving
copy on PlayArea. The Selection adapter still removed non-Draw confirmed centers
before calling the new generic resolver. The resolver consequently fell back to
the formal Hand geometry, which can change after the preceding card is consumed.
The previous in-place fix therefore did not survive the new routing boundary.

The repair routes all three migrated Hand destinations through the generic engine
before compatibility cleanup; acceptance retires only the consumed RuntimeId's
center. This preserves staged G4 behavior without activating G5 ownership.
`G4.SequentialConfirmedSourceContinuity` adds three sequential children for each
destination, distinct frozen centers, changed historical Hand positions and
assertions that later centers survive earlier completion. The expected focused
discovery count was eight after this first repair.

The user reported continued drift. The resumed standard build was already
up-to-date, so old binaries cannot be assumed to explain that report. The first
repair passed eight tests but missed waiting visuals: `UpdateSelectionCardPositions`
returned immediately after Confirm while later cards remained attached to Hand.
Removing preceding entries changed their slot origins without changing their
stored relative render translations. Preserving only the eventual transition
source could therefore leave drift while waiting and a jump on acceptance.

G4 now compensates waiting implicit/explicit Hand visuals against the existing
confirmed absolute center on position updates. Explicit non-Hand owners are
excluded. Accepted/retired centers stop participating. This is temporary staged
compatibility, not G5 activation or a new ownership contract.
`G4.ConfirmedWaitingPositionAcrossHandRelayout` checks absolute-center stability
under successive slot-origin changes, DPI scale and repeated updates, plus release.
It does not simulate Slate paint timing or claim visual acceptance.

Standard project generation and Development Editor build PASS; focused suite
**9/9 PASS** (one existing ProductionConfirmRouting fixture warning), exit 0.
Exact evidence is recorded in `docs/Validation.md`.
**MANUAL PIE — USER ACTION REQUIRED:** in Native
`/Game/SlayTheSpireDemo/Maps/L_BattleTest`, repeat the reported selection-Exhaust
scenario with at least two selected cards; Confirm and observe each successive
fade. Each must start at its confirmed displayed position, with no second-card
drift/jump, duplicate or stuck input. Report the visual result after the new build.

Latest user PIE result: **FAIL**. The second card briefly visits the first card's
position before returning to its own. G4 is not visually validated. The precise
one-frame cause is not yet proven by the coordinate-only tests. Same-tick group
fading is a G6 feature, not current G4 behavior. The Group design explicitly allows
a coherent G4+G5 migration if the intermediate production state is unsafe; that
would require updating this G4-only execution scope before implementation rather
than declaring this failed visual gate passed.

The focused `SlayTheSpireDemo.CardSelection.Presentation` prefix now contains
the following **9** tests:

```text
SlayTheSpireDemo.CardSelection.Presentation.Input.PendingBoundaryBlocksOrdinaryCardPlay
SlayTheSpireDemo.CardSelection.Presentation.Input.ProductionConfirmRouting
SlayTheSpireDemo.CardSelection.Presentation.HandToDraw.GenericTransferFinishAndCancel
SlayTheSpireDemo.CardSelection.Presentation.HandToExhaust.GenericFadeAtSource
SlayTheSpireDemo.CardSelection.Presentation.HandToDiscard.GenericMovementAndDestinationValidation
SlayTheSpireDemo.CardSelection.Presentation.G4.SelectionAreaExactVisualTransfer
SlayTheSpireDemo.CardSelection.Presentation.G4.SelectionAreaPrepareRollback
SlayTheSpireDemo.CardSelection.Presentation.G4.SequentialConfirmedSourceContinuity
SlayTheSpireDemo.CardSelection.Presentation.G4.ConfirmedWaitingPositionAcrossHandRelayout
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
   expected current discovery: 9
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
