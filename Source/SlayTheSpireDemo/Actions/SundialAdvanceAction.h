#pragma once

#include "CoreMinimal.h"
#include "BattleAction.h"
#include "SundialAdvanceAction.generated.h"

class URelicInstance;

UCLASS()
class SLAYTHESPIREDEMO_API USundialAdvanceAction : public UBattleAction
{
	GENERATED_BODY()

public:
	void Initialize(URelicInstance* InRelic, int32 InRequiredShuffles, int32 InEnergyGain);
	virtual void Execute(UBattleActionQueue* Queue) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<URelicInstance> Relic = nullptr;

	int32 RequiredShuffles = 0;
	int32 EnergyGain = 0;
};
