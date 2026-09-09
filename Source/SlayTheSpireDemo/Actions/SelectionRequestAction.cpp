#include "SelectionRequestAction.h"

#include "BattleActionQueue.h"
#include "../Selection/AuthoredContinuation.h"
#include "../Selection/SelectionResolver.h"

void USelectionRequestAction::Initialize(
	USelectionResolver* InResolver,
	const FSelectionRequest& InRequest,
	UAuthoredContinuation* InContinuation
)
{
	Resolver = InResolver;
	Request = InRequest;
	Continuation = InContinuation;
	OwningQueue = nullptr;
	bAwaitingSelection = false;
}

void USelectionRequestAction::Execute(UBattleActionQueue* Queue)
{
	bAwaitingSelection = false;
	OwningQueue = nullptr;

	if (!IsValid(Queue) || !IsValid(Resolver.Get()) || !IsValid(Continuation.Get()))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Action] SelectionRequestAction skipped: invalid Queue, Resolver or Continuation."));
		if (IsValid(Queue))
		{
			Queue->RequestResolutionFault(TEXT("SelectionRequestAction could not begin: a required runtime dependency was invalid."));
		}
		Finish();
		return;
	}

	if (!Resolver->BeginSelection(Request, Continuation.Get(), this))
	{
		// A rejected BeginSelection is an internal authoring/queue invariant
		// failure. Do not silently release the current Action and continue later
		// authored Effects past a mandatory choice.
		Queue->RequestResolutionFault(FString::Printf(
			TEXT("SelectionRequestAction could not begin selection for %s."),
			*Request.SelectionSource.ToString()
		));
		Resolver->ClearPendingSelectionForFault();
		Finish();
		return;
	}

	// Hold without Finish. The Queue keeps this Action as CurrentAction and
	// releases the pump frame; ResolvePendingSelection / CancelPendingSelection
	// (or a Fault path) eventually returns control.
	OwningQueue = Queue;
	bAwaitingSelection = true;
}

void USelectionRequestAction::ResolvePendingSelection(const FSelectionResult& Result)
{
	ResolvePendingSelectionWithDisposition(Result);
}

ESelectionResolveDisposition USelectionRequestAction::ResolvePendingSelectionWithDisposition(const FSelectionResult& Result)
{
	if (!bAwaitingSelection)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Action] SelectionRequestAction resolve ignored: not awaiting selection."));
		return ESelectionResolveDisposition::NoPendingSelection;
	}

	UBattleActionQueue* Queue = GetOwningQueue();
	if (!IsValid(Queue) || !IsValid(Resolver.Get()))
	{
		UE_LOG(LogTemp, Error, TEXT("[Action] SelectionRequestAction resolve failed: Queue or Resolver lost."));
		if (IsValid(Queue))
		{
			Queue->RequestResolutionFault(TEXT("SelectionRequestAction lost its Queue or Resolver while awaiting input."));
		}
		bAwaitingSelection = false;
		Finish();
		return ESelectionResolveDisposition::InternalFailure;
	}

	// A mandatory request must remain the current Action when Presentation tries
	// to cancel it. This guard also protects direct Action-side callers instead
	// of relying only on USelectionResolver::SubmitCancel().
	if (Result.Status == ESelectionStatus::Cancelled
		&& !Resolver->CanCancelPendingSelection())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Action] SelectionRequestAction cancel ignored: active request is mandatory."));
		return ESelectionResolveDisposition::ForbiddenCancellation;
	}

	TArray<UBattleAction*> ContinuationBatch;
	const ESelectionResolveDisposition Disposition = Resolver->ResolveSelection(Result, ContinuationBatch);
	if (Disposition != ESelectionResolveDisposition::Resolved)
	{
		// Invalid submissions and forbidden cancellation are rejected while the
		// resolver keeps the valid request and this Action pending. Legal
		// cancellation clears the request and releases the Action. Internal
		// failures have already requested a Queue fault and clean resolver state.
		if (Disposition == ESelectionResolveDisposition::InvalidSubmission
			|| Disposition == ESelectionResolveDisposition::ForbiddenCancellation)
		{
			return Disposition;
		}
		if (Disposition == ESelectionResolveDisposition::InternalFailure
			|| Disposition == ESelectionResolveDisposition::NoPendingSelection)
		{
			Queue->RequestResolutionFault(FString::Printf(
				TEXT("SelectionRequestAction could not resolve %s (disposition %d)."),
				*Request.SelectionSource.ToString(),
				static_cast<int32>(Disposition)
			));
		}
		bAwaitingSelection = false;
		Finish();
		return Disposition;
	}

	// Continuation Actions are created after the original PlayCardAction has
	// already built and stamped its follow-up batch. They therefore must inherit
	// the still-active resolution writer here, at the generic selection boundary,
	// or their committed Presentation facts would be silently lost.
	for (UBattleAction* ContinuationAction : ContinuationBatch)
	{
		if (!IsValid(ContinuationAction)
			|| ContinuationAction->IsFinished()
			|| ContinuationAction->GetOuter() != Queue)
		{
			Queue->RequestResolutionFault(FString::Printf(
				TEXT("SelectionRequestAction received an invalid continuation Action for %s."),
				*Request.SelectionSource.ToString()
			));
			bAwaitingSelection = false;
			Finish();
			return ESelectionResolveDisposition::InternalFailure;
		}
		ContinuationAction->SetPresentationRecordWriter(GetPresentationRecordWriter());
	}

	if (ContinuationBatch.Num() > 0 && !Queue->AddBatchToFrontPreserveOrder(ContinuationBatch))
	{
		Queue->RequestResolutionFault(FString::Printf(
			TEXT("SelectionRequestAction could not insert resolved continuation batch (%d actions) for %s."),
			ContinuationBatch.Num(),
			*Request.SelectionSource.ToString()
		));
		bAwaitingSelection = false;
		Finish();
		return ESelectionResolveDisposition::InternalFailure;
	}

	bAwaitingSelection = false;
	Finish();
	return ESelectionResolveDisposition::Resolved;
}

void USelectionRequestAction::CancelPendingSelection()
{
	if (!bAwaitingSelection)
	{
		return;
	}

	if (!IsValid(Resolver.Get()) || !Resolver->CancelSelection())
	{
		if (!IsValid(Resolver.Get()))
		{
			if (UBattleActionQueue* Queue = GetOwningQueue())
			{
				Queue->RequestResolutionFault(TEXT("SelectionRequestAction lost its Resolver while cancelling."));
			}
			bAwaitingSelection = false;
			Finish();
			return;
		}
		UE_LOG(LogTemp, Warning, TEXT("[Action] SelectionRequestAction cancel ignored: cancellation is not permitted."));
		return;
	}

	bAwaitingSelection = false;
	Finish();
}

void USelectionRequestAction::AbandonPendingSelectionForFault()
{
	UBattleActionQueue* Queue = OwningQueue.Get();
	bAwaitingSelection = false;
	OwningQueue = nullptr;
	if (IsValid(Queue) && Queue->IsCurrentAction(this))
	{
		Queue->RequestResolutionFault(TEXT("SelectionRequestAction abandoned an inconsistent pending request."));
		Finish();
	}
}

UBattleActionQueue* USelectionRequestAction::GetOwningQueue() const
{
	return OwningQueue.Get();
}
