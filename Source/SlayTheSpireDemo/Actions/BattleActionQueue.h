#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "BattleActionQueue.generated.h"

class UBattleAction;

DECLARE_MULTICAST_DELEGATE(FOnBattleActionQueueEmpty);

UCLASS()
class SLAYTHESPIREDEMO_API UBattleActionQueue : public UObject
{
	GENERATED_BODY()

public:
	void AddToBack(UBattleAction* Action);
	void AddToFront(UBattleAction* Action);
	void StartProcessing();

	bool IsBusy() const;
	int32 GetPendingCount() const;

	FOnBattleActionQueueEmpty OnQueueEmpty;

private:
	void PumpQueue();
	void HandleActionFinished(UBattleAction* FinishedAction);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBattleAction>> PendingActions;

	UPROPERTY(Transient)
	TObjectPtr<UBattleAction> CurrentAction = nullptr;

	bool bIsPumping = false;
};
