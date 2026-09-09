# Selection Presentation G1 Execution Record

Date: **2026-09-09**

Status:

```text
IMPLEMENTATION COMMIT PREPARED /
BUILD NOT RUN / AUTOMATION NOT RUN / NO PASS CLAIM
```

## Scope

G1 only: exact Selection-boundary outcome correlation, writer-scoped optional
PresentationGroup metadata, canonical selected-identity manifests and direct
continuation Action-local group context.

Explicitly not included: G2 Controller group discovery, Group playback, G4 generic
transition migration, G5 SelectionArea production ownership or G8 early input.

## Implemented contract

Recorded mode stores an immutable receipt in the continuation Resolution:

```text
(BattleId, SelectionBoundaryRevision)
-> exact continuation ResolutionId
```

Direct/no-history mode registers the accepted boundary and finalizes its receipt
only when the ActionQueue returns to stable idle after continuation Gameplay. The
resulting StateRevision is strictly newer than the Selection boundary. No code
guesses `ResolutionId + 1`, the newest Resolution or `BoundaryRevision + 1` before
that exact outcome exists.

Group metadata is optional and cannot affect Gameplay success. The active Resolution
builder owns GroupId allocation. A sealed declaration contains both expected count
and the canonical selected RuntimeIds. Direct continuation Actions receive group
context explicitly; writer inheritance alone never propagates it to trigger/reaction
Actions.

Current direct Hand destination Actions can stamp the exact matching group tag for:

```text
Hand -> ExhaustPile
Hand -> DrawPile
Hand -> DiscardPile
```

## Added focused source tests

```text
SlayTheSpireDemo.SelectionPresentation.G1.RecorderMetadata
SlayTheSpireDemo.SelectionPresentation.G1.ManifestValidation
```

These tests have not yet been executed. Do not mark G1 validated until an actual
Editor build and focused Automation run are recorded.
