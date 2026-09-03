#pragma once

#include "CoreMinimal.h"
#include "BattleAction.h"
#include "AdvanceRelicCounterAction.generated.h"

class URelicInstance;

UCLASS()
class SLAYTHESPIREDEMO_API UAdvanceRelicCounterAction : public UBattleAction
{
	GENERATED_BODY()

public:
	bool Initialize(
		URelicInstance* InRelic,
		int32 InRequiredCount,
		const TArray<UBattleAction*>& InRewardActions
	);

	virtual void Execute(UBattleActionQueue* Queue) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<URelicInstance> Relic = nullptr;

	int32 RequiredCount = 0;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBattleAction>> RewardActions;
};
