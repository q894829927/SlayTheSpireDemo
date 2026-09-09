# Selection Presentation G1 Execution Record

Date: **2026-09-09**

Status:

```text
COMPLETE / VALIDATED / SEALED
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

## Focused Automation

Distinct G1 tests:

```text
SlayTheSpireDemo.SelectionPresentation.G1.RecorderMetadata
SlayTheSpireDemo.SelectionPresentation.G1.ManifestValidation
```

User-confirmed local validation on **2026-09-09**:

```text
[x] SlayTheSpireDemoEditor Win64 Development Build PASS
[x] SlayTheSpireDemo.SelectionPresentation.G1 prefix run PASS
[x] RecorderMetadata individual run PASS
[x] ManifestValidation individual run PASS
```

The prefix run plus the two individual runs are three Automation invocations over
two distinct G1 test cases. This evidence validates the G1 source contract above;
it does not validate G2+ Controller Group semantics, visible Group playback,
SelectionArea production ownership or G8 early input.

## Seal

G1 is **COMPLETE / VALIDATED / SEALED**. Do not rerun its passing gates unless a
later edit invalidates this evidence or a concrete regression directly implicates
the sealed G1 contract.

Next active slice:

```text
G2 — Controller semantic Group discovery / reducer dry-run / interference
```

G2 must remain semantic-only: visible Group playback stays disabled, Controller
must use sealed immutable facts only, and production SelectionArea ownership
remains deferred.
