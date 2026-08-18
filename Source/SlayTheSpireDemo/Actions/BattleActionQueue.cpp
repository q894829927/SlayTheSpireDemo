#include "BattleActionQueue.h"

#include "BattleAction.h"

bool UBattleActionQueue::AddToBack(UBattleAction* Action)
{
	TArray<UBattleAction*> Batch;
	Batch.Add(Action);
	return AddBatchToBackPreserveOrder(Batch);
}

bool UBattleActionQueue::AddToFront(UBattleAction* Action)
{
	TArray<UBattleAction*> Batch;
	Batch.Add(Action);
	return AddBatchToFrontPreserveOrder(Batch);
}

bool UBattleActionQueue::AddBatchToBackPreserveOrder(const TArray<UBattleAction*>& Actions)
{
	FString FailureReason;
	if (!ValidateBatchForInsertion(Actions, FailureReason))
	{
		UE_LOG(LogTemp, Warning, TEXT("[ActionQueue] AddBatchToBack rejected: %s"), *FailureReason);
		return false;
	}

	for (UBattleAction* Action : Actions)
	{
		PendingActions.Add(Action);
	}

	if (Actions.Num() > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[ActionQueue] AddBatchToBack: Count=%d Pending=%d"), Actions.Num(), PendingActions.Num());
	}
	return true;
}

bool UBattleActionQueue::AddBatchToFrontPreserveOrder(const TArray<UBattleAction*>& Actions)
{
	FString FailureReason;
	if (!ValidateBatchForInsertion(Actions, FailureReason))
	{
		UE_LOG(LogTemp, Warning, TEXT("[ActionQueue] AddBatchToFront rejected: %s"), *FailureReason);
		return false;
	}

	for (int32 Index = Actions.Num() - 1; Index >= 0; --Index)
	{
		PendingActions.Insert(Actions[Index], 0);
	}

	if (Actions.Num() > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[ActionQueue] AddBatchToFront: Count=%d Pending=%d"), Actions.Num(), PendingActions.Num());
	}
	return true;
}

bool UBattleActionQueue::StartProcessing()
{
	if (bResolutionFaulted || bResolutionFaultRequested)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ActionQueue] StartProcessing rejected: resolution is faulted or fault-requested."));
		return false;
	}

	if (bIsPumping || IsValid(CurrentAction.Get()) || PendingActions.Num() == 0)
	{
		return false;
	}

	PumpQueue();
	return true;
}

bool UBattleActionQueue::RequestResolutionFault(const FString& Reason)
{
	if (bResolutionFaulted || bResolutionFaultRequested)
	{
		return false;
	}

	bResolutionFaultRequested = true;
	ResolutionFaultReason = Reason.IsEmpty() ? TEXT("Unspecified resolution fault.") : Reason;

	UE_LOG(LogTemp, Error, TEXT("[ActionQueue] Resolution fault requested: %s"), *ResolutionFaultReason);

	if (!bIsPumping && !IsValid(CurrentAction.Get()))
	{
		EnterResolutionFaultAtSafePoint();
	}

	return true;
}

bool UBattleActionQueue::IsResolutionFaulted() const
{
	return bResolutionFaulted;
}

bool UBattleActionQueue::IsBusy() const
{
	return bResolutionFaulted
		|| bResolutionFaultRequested
		|| bIsPumping
		|| IsValid(CurrentAction.Get())
		|| PendingActions.Num() > 0;
}

int32 UBattleActionQueue::GetPendingCount() const
{
	return PendingActions.Num();
}

int32 UBattleActionQueue::GetExecutedCountInResolution() const
{
	return ExecutedCountInResolution;
}

const FString& UBattleActionQueue::GetResolutionFaultReason() const
{
	return ResolutionFaultReason;
}

UBattleAction* UBattleActionQueue::GetLastExecutedAction() const
{
	return LastExecutedAction.Get();
}

#if WITH_DEV_AUTOMATION_TESTS
void UBattleActionQueue::SetMaxActionsPerResolutionForTesting(int32 InMaxActions)
{
	if (bIsPumping || IsValid(CurrentAction.Get()) || PendingActions.Num() > 0 || bResolutionFaulted || bResolutionFaultRequested)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ActionQueue] Test budget change rejected while Queue is active."));
		return;
	}

	MaxActionsPerResolution = FMath::Max(1, InMaxActions);
}
#endif

bool UBattleActionQueue::ValidateBatchForInsertion(const TArray<UBattleAction*>& Actions, FString& OutReason) const
{
	OutReason.Reset();

	if (bResolutionFaulted || bResolutionFaultRequested)
	{
		OutReason = TEXT("Queue is faulted or fault-requested.");
		return false;
	}

	if (Actions.Num() == 0)
	{
		return true;
	}

	TSet<UBattleAction*> Seen;
	Seen.Reserve(Actions.Num());

	for (UBattleAction* Action : Actions)
	{
		if (!IsValid(Action))
		{
			OutReason = TEXT("Batch contains an invalid Action.");
			return false;
		}

		if (Action->IsFinished())
		{
			OutReason = FString::Printf(TEXT("Batch contains already-finished Action %s."), *GetNameSafe(Action));
			return false;
		}

		if (Action->GetOuter() != this)
		{
			OutReason = FString::Printf(TEXT("Action %s does not use this Queue as Outer."), *GetNameSafe(Action));
			return false;
		}

		if (CurrentAction.Get() == Action)
		{
			OutReason = FString::Printf(TEXT("Action %s is already the CurrentAction."), *GetNameSafe(Action));
			return false;
		}

		if (Seen.Contains(Action))
		{
			OutReason = FString::Printf(TEXT("Batch contains duplicate Action %s."), *GetNameSafe(Action));
			return false;
		}
		Seen.Add(Action);

		const bool bAlreadyPending = PendingActions.ContainsByPredicate(
			[Action](const TObjectPtr<UBattleAction>& Pending)
			{
				return Pending.Get() == Action;
			}
		);
		if (bAlreadyPending)
		{
			OutReason = FString::Printf(TEXT("Action %s is already pending."), *GetNameSafe(Action));
			return false;
		}
	}

	return true;
}

void UBattleActionQueue::PumpQueue()
{
	if (bIsPumping || bResolutionFaulted)
	{
		return;
	}

	if (bResolutionFaultRequested && !IsValid(CurrentAction.Get()))
	{
		EnterResolutionFaultAtSafePoint();
		return;
	}

	bIsPumping = true;

	while (!IsValid(CurrentAction.Get()) && PendingActions.Num() > 0)
	{
		if (bResolutionFaultRequested)
		{
			EnterResolutionFaultAtSafePoint();
			return;
		}

		if (ExecutedCountInResolution >= MaxActionsPerResolution)
		{
			bResolutionFaultRequested = true;
			ResolutionFaultReason = FString::Printf(
				TEXT("Resolution action budget exceeded before the next dequeue. Executed=%d Max=%d."),
				ExecutedCountInResolution,
				MaxActionsPerResolution
			);
			EnterResolutionFaultAtSafePoint();
			return;
		}

		CurrentAction = PendingActions[0];
		PendingActions.RemoveAt(0);

		UBattleAction* ActionToExecute = CurrentAction.Get();
		if (!IsValid(ActionToExecute))
		{
			CurrentAction = nullptr;
			continue;
		}

		LastExecutedAction = ActionToExecute;
		++ExecutedCountInResolution;
		ActionToExecute->OnFinished.AddUObject(this, &UBattleActionQueue::HandleActionFinished);

		UE_LOG(
			LogTemp,
			Log,
			TEXT("[ActionQueue] Execute: %s Remaining=%d ResolutionCount=%d/%d"),
			*GetNameSafe(ActionToExecute),
			PendingActions.Num(),
			ExecutedCountInResolution,
			MaxActionsPerResolution
		);

		ActionToExecute->Execute(this);

		if (bResolutionFaultRequested && !IsValid(CurrentAction.Get()))
		{
			EnterResolutionFaultAtSafePoint();
			return;
		}

		if (IsValid(CurrentAction.Get()))
		{
			break;
		}
	}

	if (bResolutionFaultRequested && !IsValid(CurrentAction.Get()))
	{
		EnterResolutionFaultAtSafePoint();
		return;
	}

	const bool bQueueEmpty = !IsValid(CurrentAction.Get()) && PendingActions.Num() == 0;
	bIsPumping = false;

	if (bQueueEmpty)
	{
		UE_LOG(LogTemp, Log, TEXT("[ActionQueue] Empty. Resolution actions executed=%d."), ExecutedCountInResolution);
		ExecutedCountInResolution = 0;
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

void UBattleActionQueue::EnterResolutionFaultAtSafePoint()
{
	if (bResolutionFaulted)
	{
		return;
	}

	if (IsValid(CurrentAction.Get()))
	{
		UE_LOG(LogTemp, Error, TEXT("[ActionQueue] Refused to enter ResolutionFault while CurrentAction is still executing."));
		return;
	}

	bResolutionFaulted = true;
	bResolutionFaultRequested = false;
	PendingActions.Reset();
	bIsPumping = false;

	UE_LOG(
		LogTemp,
		Error,
		TEXT("[ActionQueue] Resolution faulted. Reason=%s Executed=%d LastAction=%s"),
		*ResolutionFaultReason,
		ExecutedCountInResolution,
		*GetNameSafe(LastExecutedAction.Get())
	);

	OnResolutionFaulted.Broadcast(ResolutionFaultReason, ExecutedCountInResolution, LastExecutedAction.Get());
}
