#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"
#include "UObject/Object.h"
#include "BattleActionQueue.generated.h"

class UBattleAction;

DECLARE_MULTICAST_DELEGATE(FOnBattleActionQueueEmpty);
DECLARE_MULTICAST_DELEGATE_ThreeParams(
	FOnBattleActionQueueResolutionFaulted,
	const FString&,
	int32,
	UBattleAction*
);

UCLASS()
class SLAYTHESPIREDEMO_API UBattleActionQueue : public UObject
{
	GENERATED_BODY()

public:
	bool AddToBack(UBattleAction* Action);
	bool AddToFront(UBattleAction* Action);
	bool AddBatchToBackPreserveOrder(const TArray<UBattleAction*>& Actions);
	bool AddBatchToFrontPreserveOrder(const TArray<UBattleAction*>& Actions);
	bool StartProcessing();

	// QueueEmpty observers may inspect the completed boundary, but authoritative
	// macro progression must not synchronously start a new resolution from inside
	// the multicast broadcast. The one authoritative continuation registered here
	// runs only after every QueueEmpty listener has returned. If it enqueues and
	// starts another batch, the existing PumpQueue call continues that work without
	// nesting another QueueEmpty broadcast inside the previous one.
	bool DeferUntilAfterQueueEmptyBroadcast(TFunction<void()>&& Continuation);

	bool RequestResolutionFault(const FString& Reason);
	bool IsResolutionFaulted() const;
	bool IsBusy() const;
	int32 GetPendingCount() const;
	int32 GetExecutedCountInResolution() const;
	const FString& GetResolutionFaultReason() const;
	UBattleAction* GetLastExecutedAction() const;

#if WITH_DEV_AUTOMATION_TESTS
	void SetMaxActionsPerResolutionForTesting(int32 InMaxActions);
#endif

	FOnBattleActionQueueEmpty OnQueueEmpty;
	FOnBattleActionQueueResolutionFaulted OnResolutionFaulted;

private:
	static constexpr int32 DefaultMaxActionsPerResolution = 10000;

	bool ValidateBatchForInsertion(const TArray<UBattleAction*>& Actions, FString& OutReason) const;
	void PumpQueue();
	void HandleActionFinished(UBattleAction* FinishedAction);
	void EnterResolutionFaultAtSafePoint();

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBattleAction>> PendingActions;

	UPROPERTY(Transient)
	TObjectPtr<UBattleAction> CurrentAction = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UBattleAction> LastExecutedAction = nullptr;

	bool bIsPumping = false;
	bool bIsBroadcastingQueueEmpty = false;
	bool bIsExecutingPostQueueEmptyContinuation = false;
	bool bHasDeferredQueueEmptyContinuation = false;
	bool bResolutionFaultRequested = false;
	bool bResolutionFaulted = false;
	int32 ExecutedCountInResolution = 0;
	int32 MaxActionsPerResolution = DefaultMaxActionsPerResolution;
	FString ResolutionFaultReason;
	TFunction<void()> DeferredQueueEmptyContinuation;
};
