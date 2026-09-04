#pragma once

#include "CoreMinimal.h"
#include "BattleAction.h"
#include "UpgradeCardAction.generated.h"

class UCardInstance;

UCLASS()
class SLAYTHESPIREDEMO_API UUpgradeCardAction : public UBattleAction
{
	GENERATED_BODY()

public:
	void Initialize(UCardInstance* InCard);
	virtual void Execute(UBattleActionQueue* Queue) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UCardInstance> Card = nullptr;
};
