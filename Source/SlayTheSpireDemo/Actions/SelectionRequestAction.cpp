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
		Finish();
		return;
	}

	if (!Resolver->BeginSelection(Request, Continuation.Get()))
	{
		// Malformed request or an already-pending selection: fail soft, no hold.
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
	if (!bAwaitingSelection)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Action] SelectionRequestAction resolve ignored: not awaiting selection."));
		return;
	}

	UBattleActionQueue* Queue = GetOwningQueue();
	if (!IsValid(Queue) || !IsValid(Resolver.Get()))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Action] SelectionRequestAction resolve aborted: Queue or Resolver lost."));
		bAwaitingSelection = false;
		Finish();
		return;
	}

	TArray<UBattleAction*> ContinuationBatch;
	if (!Resolver->TryResolveSelection(Result, ContinuationBatch))
	{
		// Cancelled or invalid selection: no dependent work, no fault. Finish and
		// let the queue resume with whatever was already pending.
		bAwaitingSelection = false;
		Finish();
		return;
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
		return;
	}

	bAwaitingSelection = false;
	Finish();
}

void USelectionRequestAction::CancelPendingSelection()
{
	if (!bAwaitingSelection)
	{
		return;
	}

	if (IsValid(Resolver.Get()))
	{
		Resolver->CancelSelection();
	}

	bAwaitingSelection = false;
	Finish();
}

UBattleActionQueue* USelectionRequestAction::GetOwningQueue() const
{
	return OwningQueue.Get();
}
