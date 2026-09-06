# Card Expansion — Wave 1C-A Selection Primitive Execution Record

Date: **2026-09-07**

Status:

```text
IMPLEMENTATION UPDATED / REVALIDATION REQUIRED
```

Branch:

```text
Wave-1C
```

Authority:

```text
docs/CardExpansionWave1CSelectionPrimitive.md
```

The original Wave 1C-A implementation passed its five focused Automation cases on 2026-09-06. The 2026-09-07 closure changed the request contract and resolver/action cancellation behavior, so that earlier `5/5 PASS` is historical evidence only and must be rerun for the current branch head.

---

## Current primitive source surface

Selection DTOs (`Source/SlayTheSpireDemo/Selection/SelectionTypes.h`):

```text
FSelectionCandidate    RuntimeObject / RuntimeSequence / SelectionKey
ESelectionStatus       Pending / Resolved / Cancelled / Invalid
ESelectionCancelPolicy Allowed / Forbidden
FSelectionResult       Status / SelectedObjects / Reason
FSelectionRequest      SelectionSource / MinCount / MaxCount / CancelPolicy / Candidates
```

Authored Continuation contract:

```text
UAuthoredContinuation
BuildNextActions(Result, Queue, OutActions) -> bool
```

Gameplay resolver:

```text
USelectionResolver
Initialize(QueueAccess)
BeginSelection(Request, Continuation, PendingAction)
TryResolveSelection(Result, OutActions)
SubmitResult(Result)
SubmitCancel()
CanCancelPendingSelection()
CancelSelection()
HasPendingSelection() / GetPendingRequest()
```

Queue-lifecycle owner:

```text
USelectionRequestAction
Execute -> begins selection and intentionally holds without Finish
ResolvePendingSelection -> validate/build continuation/front-insert -> Finish
CancelPendingSelection -> Finish only when Gameplay cancellation policy permits it
```

---

## Current cancellation contract

Generic primitive requests default to:

```text
CancelPolicy = Allowed
```

Therefore the existing generic `CancelIsLegalPath` behavior remains part of Wave 1C-A:

```text
allowed cancel
→ clear pending
→ no mutation
→ no ResolutionFault
→ waiting Action finishes
→ queue resumes
```

Wave 1C now additionally supports consumer-authored mandatory requests:

```text
CancelPolicy = Forbidden
→ cancel request rejected
→ pending selection retained
→ waiting Action remains current/unfinished
→ queue remains held
```

The mandatory path is exercised by the Wave 1C-B SelectExhaust consumer tests rather than changing the meaning of the existing generic allowed-cancel test.

---

## Focused Automation inventory

Prefix:

```text
SlayTheSpireDemo.CardExpansion.Wave1C.Selection
```

Existing cases (5):

```text
ResolveBuildsContinuation
InvalidSelectionRejected
CancelIsLegalPath
MalformedRequestRejected
CountBoundsEnforced
```

Historical pre-closure result:

```text
5/5 PASS (2026-09-06, previous branch head)
```

Current branch requirement:

```text
[ ] rerun all 5 primitive cases after CancelPolicy/resolver/action changes
```

Required preserved behavior:

```text
request creation / BeginSelection sets pending
Action holds while awaiting selection; queue stays busy
Resolver exposes the pending request
valid resolved selection -> continuation batch built and executed
foreign object rejection does not mutate Gameplay
allowed cancel clears pending without fault
selected count outside [min,max] rejected
malformed request rejected
second concurrent request rejected
```

---

## Current seal gate

```text
[ ] Editor Development Build PASS on current Wave-1C head
[ ] focused Wave 1C-A Selection Automation PASS (5/5) on current head
[ ] combined Wave 1C focused suite PASS (expected 11 total cases)
[ ] Native HUD PIE closure gate from Wave 1C-B
[ ] Final user seal confirmation
```

Do not restore `COMPLETE / VALIDATED / READY FOR SEAL` until the current branch head passes these gates.
