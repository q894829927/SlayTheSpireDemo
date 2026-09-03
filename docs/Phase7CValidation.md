# Phase 7C — Sundial + GainEnergyAction Validation

Date: **2026-09-03**

Status: **COMPLETE / VALIDATED / SEALED**

Validated implementation HEAD before this evidence-only documentation update:

```text
225e5a1ad9c20c6cf5abcf24aec18cc4d68f62a9
```

## Scope

Phase 7C establishes the first concrete Relic gameplay vertical slice and its reusable positive-Energy primitive:

```text
BattleEnergyMutation::TryGain
UGainEnergyAction
EnergyPresentationRecord::AppendCommittedEnergyChanged
USundialTrigger
USundialAdvanceAction
URelicInstance::Counter
Bulk Draw-N semantics required by real shuffle behavior
```

Sundial remains a first-class Relic and is not modeled as a Status. The mechanic contains no Pommel Strike, card-identity, DrawAction-identity, or Relic/card-combination special case.

## Accepted gameplay contract

### GainEnergy

```text
Amount > 0 required
invalid Battle / zero / negative / overflow -> fail soft
EnergyAfter = EnergyBefore + Amount
no MaxEnergy clamp in the Phase 7C contract
committed change -> exact EnergyChanged(Before, After, Delta)
```

`UGainEnergyAction` owns the intended amount and commits only through `BattleEnergyMutation::TryGain`.

The sealed design-conformance cleanup is also complete: both the GainEnergy action path and BattleManager turn energy path now delegate committed `EnergyChanged` record construction to the same narrow helper:

```text
EnergyPresentationRecord::AppendCommittedEnergyChanged(
    const FEnergyCommitResult&,
    const FPresentationRecordWriter&)
```

The helper owns only existing A2 committed-record semantics. Gameplay mutation remains outside it.

### Sundial

```text
committed authoritative FDeckShuffledEvent
-> read-only Sundial trigger eligibility/build
-> USundialAdvanceAction with frozen RequiredShuffles / EnergyGain
-> authoritative Relic counter mutation
-> every third shuffle resets counter
-> dependent UGainEnergyAction(+2)
```

The trigger does not mutate the counter or Energy, and `USundialAdvanceAction` does not rediscover authored Trigger configuration at Execute time.

### Bulk Draw contract

`Draw N` is one bulk request rather than N unrelated independent draw attempts:

```text
UDrawCardEffect(DrawCount=N)
-> UDrawCardsAction(N)
   -> UDrawCardAction x available-now
   -> UShuffleDeckAction when the bulk request still owes draws
   -> UDrawCardsAction(Remaining)
```

`UDrawCardAction` is one atomic `DrawPile -> Hand` mutation only.

A fresh bulk request with `Draw=0 / Discard=0` ends without a shuffle. A shuffle already planned by an earlier bulk step may execute after the last available card has been consumed and therefore commit with `MovedCardCount=0`. This preserves source-like Draw-N planning and allows the two-upgraded-Pommel-Strike/Sundial loop to emerge from generic Draw/Shuffle/Event/Relic rules.

## Accepted validation evidence

User-reported focused validation on 2026-09-03:

```text
SlayTheSpireDemo.Phase6C                         PASS
SlayTheSpireDemo.Phase7.Sundial                 PASS
SlayTheSpireDemo.Phase7.EnergyGain              2/2 PASS
SlayTheSpireDemo.Phase6UIA2C.Record.EndTurnEnergy 1/1 PASS
```

The `Phase7.EnergyGain` prefix contains the two focused primitive tests:

```text
MutationContracts
ActionAndPresentation
```

Together they cover positive gain, exceeding MaxEnergy, zero/negative rejection, overflow rejection, invalid-Battle fail-soft behavior, authoritative Energy mutation, and exact committed `EnergyChanged` Before/After/Delta.

The `EndTurnEnergy` regression proves the BattleManager path still produces the established committed Energy presentation semantics after routing through the shared helper.

The previously accepted Sundial focused gate remains sticky because the final helper cleanup did not modify Relic trigger logic, counter mutation, queue ordering, Draw/Shuffle semantics, or Energy gameplay mutation.

Manual PIE is **not required for Phase 7C acceptance** by the sealed Phase 7 plan because Relic HUD/read/frozen presentation arrives in 7D.

## Related post-seal UI correction

During Phase 7C validation, an independent committed card-face continuity defect was found and closed without changing the Phase 7C gameplay contract. Its evidence is recorded separately in:

```text
docs/PostSealCardFaceContinuityValidation.md
```

That correction is already **COMPLETE / VALIDATED** and is not part of the 7C acceptance budget.

## Acceptance conclusion

The Phase 7C implementation satisfies the sealed design requirements and focused validation budget.

Therefore:

```text
Phase 7A Relic Runtime:                    COMPLETE / VALIDATED / SEALED
Phase 7B Status + Relic Trigger Sources:   COMPLETE / VALIDATED / SEALED
Phase 7C Sundial + GainEnergyAction:       COMPLETE / VALIDATED / SEALED
Phase 7D Relic Read/Frozen/Native UI:      NEXT / NOT STARTED
```

Phase 7 overall remains **IN PROGRESS** until 7D passes and the final Phase 7 seal is recorded.

No additional Phase6R, A2D5, Shipping, Legacy-parity, Phase6C, Sundial, card-face-continuity, or unrelated UI rerun is required to close 7C.
