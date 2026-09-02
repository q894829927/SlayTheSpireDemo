#pragma once

#include "CoreMinimal.h"
#include "BattleAction.h"
#include "GainEnergyAction.generated.h"

class ABattleManager;

UCLASS()
class SLAYTHESPIREDEMO_API UGainEnergyAction : public UBattleAction
{
	GENERATED_BODY()

public:
	void Initialize(ABattleManager* InBattle, int32 InAmount);
	virtual void Execute(UBattleActionQueue* Queue) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<ABattleManager> Battle = nullptr;

	int32 Amount = 0;
};
