#include "SelectionResolver.h"

#include "AuthoredContinuation.h"
#include "../Actions/BattleAction.h"
#include "../Actions/BattleActionQueue.h"
#include "../Actions/SelectionRequestAction.h"

void USelectionResolver::Initialize(FSelectionResolverQueueAccess InQueueAccess)
{
	QueueAccess = InQueueAccess;
}

bool USelectionResolver::HasPendingSelection() const
{
	return bHasPendingSelection;
}

const FSelectionRequest* USelectionResolver::GetPendingRequest() const
{
	return bHasPendingSelection ? &PendingRequest : nullptr;
}

bool USelectionResolver::CanCancelPendingSelection() const
{
	return bHasPendingSelection
		&& PendingRequest.CancelPolicy == ESelectionCancelPolicy::Allowed;
}

bool USelectionResolver::BeginSelection(
	const FSelectionRequest& Request,
	const UAuthoredContinuation* Continuation,
	USelectionRequestAction* InPendingAction
)
{
	if (bHasPendingSelection)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Selection] BeginSelection rejected: a selection is already pending."));
		return false;
	}

	FString Reason;
	if (!ValidateRequest(Request, Reason))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Selection] BeginSelection rejected: %s"), *Reason);
		return false;
	}

	if (!IsValid(Continuation))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Selection] BeginSelection rejected: missing authored Continuation."));
		return false;
	}

	if (!IsValid(InPendingAction))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Selection] BeginSelection rejected: missing awaiting Action."));
		return false;
	}

	PendingRequest = Request;
	PendingContinuation = Continuation;
	PendingAction = InPendingAction;
	bHasPendingSelection = true;
	return true;
}

bool USelectionResolver::TryResolveSelection(
	const FSelectionResult& Result,
	TArray<UBattleAction*>& OutActions
)
{
	OutActions.Reset();

	if (!bHasPendingSelection)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Selection] TryResolveSelection rejected: no pending selection."));
		return false;
	}

	// Cancelled is a legal primitive path only when the authored request permits
	// it. A mandatory choice remains pending so Presentation cannot skip a
	// required Gameplay cost and allow later queued effects to continue.
	if (Result.Status == ESelectionStatus::Cancelled)
	{
		if (!CanCancelPendingSelection())
		{
			UE_LOG(LogTemp, Warning, TEXT("[Selection] Cancellation rejected: active request is mandatory."));
			return false;
		}
		ClearPendingSelectionInternal();
		return false;
	}

	if (Result.Status != ESelectionStatus::Resolved)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[Selection] TryResolveSelection rejected: invalid status %d."),
			static_cast<int32>(Result.Status)
		);
		ClearPendingSelectionInternal();
		return false;
	}

	FString Reason;
	if (!ValidateResult(Result, Reason))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Selection] TryResolveSelection rejected invalid result: %s"), *Reason);
		ClearPendingSelectionInternal();
		return false;
	}

	UBattleActionQueue* Queue = QueueAccess.IsBound() ? QueueAccess.Execute(this) : nullptr;
	if (!IsValid(Queue))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Selection] TryResolveSelection rejected: no authoritative queue available."));
		ClearPendingSelectionInternal();
		return false;
	}

	const UAuthoredContinuation* Continuation = PendingContinuation.Get();
	if (!IsValid(Continuation))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Selection] TryResolveSelection rejected: continuation lost."));
		ClearPendingSelectionInternal();
		return false;
	}

	if (!Continuation->BuildNextActions(Result, Queue, OutActions))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Selection] Continuation declined to build dependent actions."));
		ClearPendingSelectionInternal();
		return false;
	}

	ClearPendingSelectionInternal();
	return true;
}

bool USelectionResolver::SubmitResult(const FSelectionResult& Result)
{
	if (!bHasPendingSelection || !IsValid(PendingAction.Get()))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Selection] SubmitResult rejected: no awaiting selection Action."));
		return false;
	}

	if (Result.Status == ESelectionStatus::Cancelled && !CanCancelPendingSelection())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Selection] SubmitResult rejected: mandatory selection cannot be cancelled."));
		return false;
	}

	USelectionRequestAction* Action = PendingAction.Get();
	Action->ResolvePendingSelection(Result);
	return true;
}

bool USelectionResolver::SubmitCancel()
{
	if (!bHasPendingSelection || !IsValid(PendingAction.Get()))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Selection] SubmitCancel rejected: no awaiting selection Action."));
		return false;
	}

	if (!CanCancelPendingSelection())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Selection] SubmitCancel rejected: active request is mandatory."));
		return false;
	}

	USelectionRequestAction* Action = PendingAction.Get();
	Action->CancelPendingSelection();
	return true;
}

bool USelectionResolver::CancelSelection()
{
	if (!CanCancelPendingSelection())
	{
		return false;
	}

	ClearPendingSelectionInternal();
	return true;
}

void USelectionResolver::ClearPendingSelectionInternal()
{
	bHasPendingSelection = false;
	PendingRequest = FSelectionRequest{};
	PendingContinuation = nullptr;
	PendingAction = nullptr;
}

bool USelectionResolver::ValidateRequest(const FSelectionRequest& Request, FString& OutReason) const
{
	if (Request.Candidates.Num() == 0)
	{
		OutReason = TEXT("candidate set is empty");
		return false;
	}
	if (Request.MinCount < 0
		|| Request.MaxCount < Request.MinCount
		|| Request.MaxCount > Request.Candidates.Num())
	{
		OutReason = TEXT("selection count bounds are invalid");
		return false;
	}
	return true;
}

bool USelectionResolver::ValidateResult(const FSelectionResult& Result, FString& OutReason) const
{
	const int32 SelectedCount = Result.SelectedObjects.Num();
	if (SelectedCount < PendingRequest.MinCount || SelectedCount > PendingRequest.MaxCount)
	{
		OutReason = FString::Printf(
			TEXT("selected count %d outside [%d, %d]"),
			SelectedCount,
			PendingRequest.MinCount,
			PendingRequest.MaxCount
		);
		return false;
	}

	TSet<const UObject*> SeenSelectedObjects;
	for (const TObjectPtr<UObject>& Selected : Result.SelectedObjects)
	{
		UObject* SelectedObject = Selected.Get();
		if (!IsValid(SelectedObject))
		{
			OutReason = TEXT("selected object is invalid");
			return false;
		}

		if (SeenSelectedObjects.Contains(SelectedObject))
		{
			OutReason = TEXT("selected object appears more than once");
			return false;
		}
		SeenSelectedObjects.Add(SelectedObject);

		const bool bInCandidateSet = PendingRequest.Candidates.ContainsByPredicate(
			[SelectedObject](const FSelectionCandidate& Candidate)
			{
				return Candidate.RuntimeObject.Get() == SelectedObject;
			}
		);
		if (!bInCandidateSet)
		{
			OutReason = TEXT("selected object is not in the candidate set");
			return false;
		}
	}

	return true;
}
