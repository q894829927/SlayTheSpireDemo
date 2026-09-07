# Card Expansion — Wave 1C-A Selection Primitive Execution Record

Date: **2026-09-07**

Status:

```text
COMPLETE / VALIDATED / SEALED
```

Branch:

```text
Wave-1C
```

Authority:

```text
docs/CardExpansionWave1CSelectionPrimitive.md
```

The original Wave 1C-A implementation passed five focused Automation cases on 2026-09-06. The 2026-09-07 closure then changed the request contract and resolver/action cancellation behavior, requiring revalidation. That revalidation has now been completed on the current Wave-1C branch by the user: Editor Development Build PASS, the combined Wave 1C focused Automation suite PASS, and the Native HUD PIE closure gates PASS.

---

## Sealed primitive source surface

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

Selection-generated continuation Actions inherit the active committed-presentation writer at the common `USelectionRequestAction` continuation boundary. This keeps post-selection Gameplay mutation and Presentation records in the same resolution.

---

## Sealed cancellation contract

Generic primitive requests default to:

```text
CancelPolicy = Allowed
```

Therefore the generic `CancelIsLegalPath` behavior remains part of Wave 1C-A:

```text
allowed cancel
→ clear pending
→ no mutation
→ no ResolutionFault
→ waiting Action finishes
→ queue resumes
```

Wave 1C additionally supports consumer-authored mandatory requests:

```text
CancelPolicy = Forbidden
→ cancel request rejected
→ pending selection retained
→ waiting Action remains current/unfinished
→ queue remains held
```

The mandatory path is exercised by the Wave 1C-B SelectExhaust consumer tests rather than changing the meaning of the generic allowed-cancel test.

---

## Focused Automation inventory

Primitive prefix:

```text
SlayTheSpireDemo.CardExpansion.Wave1C.Selection
```

Primitive cases (5):

```text
ResolveBuildsContinuation
InvalidSelectionRejected
CancelIsLegalPath
MalformedRequestRejected
CountBoundsEnforced
```

Current validation result:

```text
5/5 primitive cases PASS as part of the current Wave 1C focused suite
```

Preserved behavior:

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
mandatory cancel cannot release a Forbidden request
```

---

## Final seal evidence

User-confirmed local validation on **2026-09-07**:

```text
[x] SlayTheSpireDemoEditor Win64 Development Build PASS
[x] focused Wave 1C-A Selection Automation PASS (5/5)
[x] combined SlayTheSpireDemo.CardExpansion.Wave1C Automation PASS (13/13)
[x] Native HUD PIE closure gate PASS
[x] final user validation confirmation received
```

The earlier pre-closure `5/5 PASS` remains historical evidence only; the seal above is based on the revalidated current branch behavior.

Wave 1C-A is complete, validated and sealed.