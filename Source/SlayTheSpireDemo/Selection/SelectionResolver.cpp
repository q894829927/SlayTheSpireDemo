#include "SelectionResolver.h"

#include "AuthoredContinuation.h"
#include "../Actions/BattleActionQueue.h"
#include "../Actions/BattleAction.h"

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

bool USelectionResolver::BeginSelection(
	const FSelectionRequest& Request,
	const UAuthoredContinuation* Continuation
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

	PendingRequest = Request;
	PendingContinuation = Continuation;
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

	// Cancelled is a legal resolution path: clear pending, no mutation, no fault.
	if (Result.Status == ESelectionStatus::Cancelled)
	{
		bHasPendingSelection = false;
		PendingRequest = FSelectionRequest{};
		PendingContinuation = nullptr;
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
		bHasPendingSelection = false;
		PendingRequest = FSelectionRequest{};
		PendingContinuation = nullptr;
		return false;
	}

	FString Reason;
	if (!ValidateResult(Result, Reason))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Selection] TryResolveSelection rejected invalid result: %s"), *Reason);
		bHasPendingSelection = false;
		PendingRequest = FSelectionRequest{};
		PendingContinuation = nullptr;
		return false;
	}

	UBattleActionQueue* Queue = QueueAccess.IsBound() ? QueueAccess.Execute(this) : nullptr;
	if (!IsValid(Queue))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Selection] TryResolveSelection rejected: no authoritative queue available."));
		bHasPendingSelection = false;
		PendingRequest = FSelectionRequest{};
		PendingContinuation = nullptr;
		return false;
	}

	const UAuthoredContinuation* Continuation = PendingContinuation.Get();
	if (!IsValid(Continuation))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Selection] TryResolveSelection rejected: continuation lost."));
		bHasPendingSelection = false;
		PendingRequest = FSelectionRequest{};
		PendingContinuation = nullptr;
		return false;
	}

	if (!Continuation->BuildNextActions(Result, Queue, OutActions))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Selection] Continuation declined to build dependent actions."));
		bHasPendingSelection = false;
		PendingRequest = FSelectionRequest{};
		PendingContinuation = nullptr;
		return false;
	}

	bHasPendingSelection = false;
	PendingRequest = FSelectionRequest{};
	PendingContinuation = nullptr;
	return true;
}

void USelectionResolver::CancelSelection()
{
	bHasPendingSelection = false;
	PendingRequest = FSelectionRequest{};
	PendingContinuation = nullptr;
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

	for (const TObjectPtr<UObject>& Selected : Result.SelectedObjects)
	{
		if (!IsValid(Selected.Get()))
		{
			OutReason = TEXT("selected object is invalid");
			return false;
		}

		const bool bInCandidateSet = PendingRequest.Candidates.ContainsByPredicate(
			[&Selected](const FSelectionCandidate& Candidate)
			{
				return Candidate.RuntimeObject.Get() == Selected.Get();
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
