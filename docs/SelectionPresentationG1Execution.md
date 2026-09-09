# Selection Presentation G1 Execution Record

Date: **2026-09-09**

Status:

```text
IMPLEMENTED IN SOURCE /
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

The existing `FPresentationRecordWriter` now carries the exact interactive
Selection boundary. On accepted Confirm, `USelectionRequestAction` records the
receipt through that still-current writer before continuation Actions are queued.
No code guesses `ResolutionId + 1` or searches for a destination Record.

Direct/no-history mode uses the same writer-shaped boundary capability with no
active recorder. Accepted Confirm registers the exact boundary in `ABattleManager`.
The deferred `ReadStateReady` edge resolves it only to a strictly newer
`StateRevision`; if Gameplay did not otherwise advance the revision, G1 creates a
single read/presentation revision and refreshes the frozen baseline before
publication. The resulting receipt remains queryable after resolver clearing.

Group metadata is optional and cannot affect Gameplay success. The active Resolution
builder owns GroupId allocation. A sealed declaration contains both expected count
and the canonical selected RuntimeIds. Direct continuation Actions receive group
context explicitly; writer inheritance alone never propagates it to trigger/reaction
Actions.

Current direct Hand destination Actions stamp the exact matching group tag for:

```text
Hand -> ExhaustPile
Hand -> DrawPile
Hand -> DiscardPile
```

Malformed/undeclared optional record tags degrade to ordinary serial history rather
than invalidating an otherwise trustworthy Presentation record.

## Added focused source tests

```text
SlayTheSpireDemo.SelectionPresentation.G1.RecorderMetadata
SlayTheSpireDemo.SelectionPresentation.G1.ManifestValidation
```

These tests have not yet been executed. Do not mark G1 validated until an actual
Editor build and focused Automation run are recorded.
