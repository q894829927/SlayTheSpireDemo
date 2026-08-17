#include "BattleActionQueue.h"

#include "BattleAction.h"

void UBattleActionQueue::AddToBack(UBattleAction* Action)
{
	if (!IsValid(Action) || Action->IsFinished())
	{
		UE_LOG(LogTemp, Warning, TEXT("[ActionQueue] AddToBack rejected: invalid or already-finished action."));
		return;
	}

	PendingActions.Add(Action);
	UE_LOG(LogTemp, Log, TEXT("[ActionQueue] AddToBack: %s Pending=%d"), *GetNameSafe(Action), PendingActions.Num());
}

void UBattleActionQueue::AddToFront(UBattleAction* Action)
{
	if (!IsValid(Action) || Action->IsFinished())
	{
		UE_LOG(LogTemp, Warning, TEXT("[ActionQueue] AddToFront rejected: invalid or already-finished action."));
		return;
	}

	PendingActions.Insert(Action, 0);
	UE_LOG(LogTemp, Log, TEXT("[ActionQueue] AddToFront: %s Pending=%d"), *GetNameSafe(Action), PendingActions.Num());
}

void UBattleActionQueue::StartProcessing()
{
	if (bIsPumping || IsValid(CurrentAction.Get()) || PendingActions.Num() == 0)
	{
		return;
	}

	PumpQueue();
}

bool UBattleActionQueue::IsBusy() const
{
	return bIsPumping || IsValid(CurrentAction.Get()) || PendingActions.Num() > 0;
}

int32 UBattleActionQueue::GetPendingCount() const
{
	return PendingActions.Num();
}

void UBattleActionQueue::PumpQueue()
{
	if (bIsPumping)
	{
		return;
	}

	bIsPumping = true;

	while (!IsValid(CurrentAction.Get()) && PendingActions.Num() > 0)
	{
		CurrentAction = PendingActions[0];
		PendingActions.RemoveAt(0);

		UBattleAction* ActionToExecute = CurrentAction.Get();
		if (!IsValid(ActionToExecute))
		{
			CurrentAction = nullptr;
			continue;
		}

		ActionToExecute->OnFinished.AddUObject(this, &UBattleActionQueue::HandleActionFinished);

		UE_LOG(
			LogTemp,
			Log,
			TEXT("[ActionQueue] Execute: %s Remaining=%d"),
			*GetNameSafe(ActionToExecute),
			PendingActions.Num()
		);

		ActionToExecute->Execute();

		// A synchronous action calls Finish() during Execute(), which clears
		// CurrentAction through HandleActionFinished. The loop can then continue
		// without recursive ProcessNext-style calls. An asynchronous action keeps
		// CurrentAction valid until it finishes later.
		if (IsValid(CurrentAction.Get()))
		{
			break;
		}
	}

	const bool bQueueEmpty = !IsValid(CurrentAction.Get()) && PendingActions.Num() == 0;
	bIsPumping = false;

	if (bQueueEmpty)
	{
		UE_LOG(LogTemp, Log, TEXT("[ActionQueue] Empty."));
		OnQueueEmpty.Broadcast();
	}
}

void UBattleActionQueue::HandleActionFinished(UBattleAction* FinishedAction)
{
	if (CurrentAction.Get() != FinishedAction)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[ActionQueue] Ignored finish from non-current action: %s"),
			*GetNameSafe(FinishedAction)
		);
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[ActionQueue] Finished: %s"), *GetNameSafe(FinishedAction));
	CurrentAction = nullptr;

	if (!bIsPumping)
	{
		PumpQueue();
	}
}
