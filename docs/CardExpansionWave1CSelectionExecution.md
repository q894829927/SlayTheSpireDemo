# Card Expansion — Wave 1C-A Selection Primitive Execution Record

Date: **2026-09-06**

Status:

```text
COMPLETE / VALIDATED / READY FOR SEAL
```

Branch:

```text
main
```

Authority:

```text
docs/CardExpansionWave1CSelectionPrimitive.md
```

This record covers only the Wave 1C-A primitive slice (SelectionRequest / SelectionResolver / SelectionResult / authored Continuation / queue resume contract). Wave 1C-B (Burning Pact as first consumer) is outside this record.

---

## Implemented source surface

Selection DTOs (`Source/SlayTheSpireDemo/Selection/SelectionTypes.h`):

```text
FSelectionCandidate    RuntimeObject / RuntimeSequence / SelectionKey
ESelectionStatus       Pending / Resolved / Cancelled / Invalid
FSelectionResult       Status / SelectedObjects / Reason
FSelectionRequest      SelectionSource / MinCount / MaxCount / Candidates
```

Authored Continuation contract (`Source/SlayTheSpireDemo/Selection/AuthoredContinuation.h`):

```text
UAuthoredContinuation  abstract, immutable/stateless definition object
BuildNextActions(Result, Queue, OutActions) -> bool
```

Narrow resolver (`Source/SlayTheSpireDemo/Selection/SelectionResolver.h/.cpp`):

```text
USelectionResolver     Gameplay-owned owner of the single active pending selection
Initialize(QueueAccess)
BeginSelection(Request, Continuation)
TryResolveSelection(Result, OutActions)
CancelSelection()
HasPendingSelection() / GetPendingRequest()
```

Queue-lifecycle owner (`Source/SlayTheSpireDemo/Actions/SelectionRequestAction.h/.cpp`):

```text
USelectionRequestAction  BattleAction; Execute begins selection and holds without Finish
ResolvePendingSelection(Result)  validate -> front-insert continuation batch -> Finish
CancelPendingSelection()         clear pending -> Finish
IsAwaitingSelection()
```

Determinism / ownership points frozen by this slice:

```text
- Selection state belongs to Gameplay (resolver), never Presentation.
- The Action owns the queue lifecycle; it never pumps the queue itself.
- Execute deliberately does NOT call Finish when it begins a selection, so the
  existing asynchronous Action model keeps it as CurrentAction and releases the
  pump frame. Resolve/Cancel then Finish and the existing HandleActionFinished
  resume path advances the queue. No frame-time dependency is introduced.
- Cancel is a legal path: no mutation, no ResolutionFault, pending cleared.
- Invalid is a contract violation: no mutation, controlled failure info, pending
  cleared, no fault.
- Continuation is typed / authored / local / immutable / stateless; it returns a
  batch, never drives the queue.
```

---

## Focused Automation validation

Prefix:

```text
SlayTheSpireDemo.CardExpansion.Wave1C.Selection
```

Cases (5):

```text
ResolveBuildsContinuation
InvalidSelectionRejected
CancelIsLegalPath
MalformedRequestRejected
CountBoundsEnforced
```

Result:

```text
5/5 PASS (EXIT CODE 0)
```

Coverage:

```text
request creation / BeginSelection sets pending
Action holds (does not Finish) while awaiting selection; queue stays busy
Resolver exposes the pending request
valid resolved selection -> continuation batch built and executed
foreign object not in candidate set -> rejected, no dependent work, no fault
cancel -> clears pending, no mutation, no fault
selected count outside [min,max] -> rejected
malformed request (empty candidates / inverted bounds / max>candidate count) rejected
second concurrent request rejected while one is pending
invalid/cancel never requests ResolutionFault
```

---

## Seal gate

```text
[X] Editor Development Build PASS
[X] focused Wave 1C-A Selection Automation PASS (5/5)
[X] source review of resolver / continuation / action ownership PASS
[ ] Final user seal confirmation
```

No Wave 1C-B (Burning Pact) implementation is included in this slice.
