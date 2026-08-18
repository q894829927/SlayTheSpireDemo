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

	if (bIsPumping)
	{
		// A post-QueueEmpty macro continuation may enqueue the next authoritative
		// batch while the existing PumpQueue frame is intentionally still alive.
		// Report success without recursively entering PumpQueue; the outer frame
		// will observe PendingActions and continue the next resolution itself.
		if (bIsExecutingPostQueueEmptyContinuation &&
			!IsValid(CurrentAction.Get()) &&
			PendingActions.Num() > 0)
		{
			return true;
		}

		return false;
	}

	if (IsValid(CurrentAction.Get()) || PendingActions.Num() == 0)
	{
		return false;
	}

	PumpQueue();
	// PumpQueue has now genuinely returned to its caller. Only this post-return
	// check may publish healthy ResolutionIdle.
	BroadcastResolutionIdleIfSettled();
	return true;
}

bool UBattleActionQueue::DeferUntilAfterQueueEmptyBroadcast(TFunction<void()>&& Continuation)
{
	if (bResolutionFaulted || bResolutionFaultRequested)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ActionQueue] QueueEmpty continuation rejected: resolution is faulted or fault-requested."));
		return false;
	}

	if (!bIsBroadcastingQueueEmpty)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ActionQueue] QueueEmpty continuation rejected: no QueueEmpty broadcast is active."));
		return false;
	}

	if (!Continuation)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ActionQueue] QueueEmpty continuation rejected: callable is unbound."));
		return false;
	}

	if (bHasDeferredQueueEmptyContinuation)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ActionQueue] QueueEmpty continuation rejected: an authoritative continuation is already registered for this boundary."));
		return false;
	}

	DeferredQueueEmptyContinuation = MoveTemp(Continuation);
	bHasDeferredQueueEmptyContinuation = true;
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

	// Empty batches are always a legal no-op for a healthy Queue, including
	// while QueueEmpty observers are being notified. They cannot mutate pending
	// work, so the non-reentrant observer protection only applies to non-empty work.
	if (Actions.Num() == 0)
	{
		return true;
	}

	if (bIsBroadcastingQueueEmpty)
	{
		OutReason = TEXT("Actions cannot be inserted directly while QueueEmpty observers are being notified; defer authoritative macro progression first.");
		return false;
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

	while (true)
	{
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
				// An asynchronous Action still owns the Queue. Release the pump frame;
				// HandleActionFinished will resume when that Action eventually finishes.
				bIsPumping = false;
				return;
			}
		}

		if (bResolutionFaultRequested && !IsValid(CurrentAction.Get()))
		{
			EnterResolutionFaultAtSafePoint();
			return;
		}

		if (IsValid(CurrentAction.Get()))
		{
			bIsPumping = false;
			return;
		}

		if (PendingActions.Num() > 0)
		{
			continue;
		}

		// This resolution is genuinely empty. Keep bIsPumping true across the
		// complete multicast so no listener can recursively start another pump.
		UE_LOG(LogTemp, Log, TEXT("[ActionQueue] Empty. Resolution actions executed=%d."), ExecutedCountInResolution);
		ExecutedCountInResolution = 0;

		bIsBroadcastingQueueEmpty = true;
		OnQueueEmpty.Broadcast();
		bIsBroadcastingQueueEmpty = false;

		if (bResolutionFaultRequested)
		{
			EnterResolutionFaultAtSafePoint();
			return;
		}

		if (bHasDeferredQueueEmptyContinuation)
		{
			TFunction<void()> ContinuationToExecute = MoveTemp(DeferredQueueEmptyContinuation);
			DeferredQueueEmptyContinuation = TFunction<void()>();
			bHasDeferredQueueEmptyContinuation = false;

			bIsExecutingPostQueueEmptyContinuation = true;
			ContinuationToExecute();
			bIsExecutingPostQueueEmptyContinuation = false;
		}

		if (bResolutionFaultRequested)
		{
			EnterResolutionFaultAtSafePoint();
			return;
		}

		if (IsValid(CurrentAction.Get()))
		{
			bIsPumping = false;
			return;
		}

		if (PendingActions.Num() == 0)
		{
			// No signal is emitted from inside PumpQueue. The caller will perform the
			// settled check only after this function has actually returned.
			bIsPumping = false;
			return;
		}

		// The deferred authoritative continuation produced the next batch. Loop in
		// this same pump frame so its eventual QueueEmpty broadcast cannot be nested
		// inside the previous QueueEmpty multicast.
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
		// As with StartProcessing, healthy idle is published only after PumpQueue
		// has returned from the resumed asynchronous resolution.
		BroadcastResolutionIdleIfSettled();
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
	bIsBroadcastingQueueEmpty = false;
	bIsExecutingPostQueueEmptyContinuation = false;
	bHasDeferredQueueEmptyContinuation = false;
	DeferredQueueEmptyContinuation = TFunction<void()>();
	bIsPumping = false;

	UE_LOG(
		LogTemp,
		Error,
		TEXT("[ActionQueue] Resolution faulted. Reason=%s Executed=%d LastAction=%s"),
		*ResolutionFaultReason,
		ExecutedCountInResolution,
		*GetNameSafe(LastExecutedAction.Get())
	);

	// The Queue publishes only its own fault boundary. Its owning battle decides
	// how that boundary maps into battle state and player-facing read publication.
	// A faulted Queue is never reported as a healthy ResolutionIdle.
	OnResolutionFaulted.Broadcast(ResolutionFaultReason, ExecutedCountInResolution, LastExecutedAction.Get());
}

void UBattleActionQueue::BroadcastResolutionIdleIfSettled()
{
	const auto IsSettled = [this]()
	{
		return !bIsPumping
			&& !bIsBroadcastingQueueEmpty
			&& !bIsExecutingPostQueueEmptyContinuation
			&& !bHasDeferredQueueEmptyContinuation
			&& !bResolutionFaultRequested
			&& !bResolutionFaulted
			&& !IsValid(CurrentAction.Get())
			&& PendingActions.Num() == 0;
	};

	if (!IsSettled())
	{
		return;
	}

	OnResolutionIdle.Broadcast();
}
