#pragma once

#include "CoreMinimal.h"
#include "BattleAction.h"
#include "ReduceStatusAction.generated.h"

class UStatusContainer;
class UStatusInstance;

UCLASS()
class SLAYTHESPIREDEMO_API UReduceStatusAction : public UBattleAction
{
	GENERATED_BODY()

public:
	void Initialize(UStatusContainer* InContainer, UStatusInstance* InExpectedInstance, int32 InAmountToRemove);
	virtual void Execute(UBattleActionQueue* Queue) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UStatusContainer> Container = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UStatusInstance> ExpectedInstance = nullptr;

	int32 AmountToRemove = 0;
};
