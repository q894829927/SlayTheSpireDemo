#pragma once

#include "CoreMinimal.h"
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
	bool bResolutionFaultRequested = false;
	bool bResolutionFaulted = false;
	int32 ExecutedCountInResolution = 0;
	int32 MaxActionsPerResolution = DefaultMaxActionsPerResolution;
	FString ResolutionFaultReason;
};
