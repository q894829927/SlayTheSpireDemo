#pragma once

#include "CoreMinimal.h"
#include "BattleAction.h"
#include "DamageAction.generated.h"

class ACombatant;

UCLASS()
class SLAYTHESPIREDEMO_API UDamageAction : public UBattleAction
{
	GENERATED_BODY()

public:
	void Initialize(ACombatant* InSource, ACombatant* InTarget, int32 InBaseAmount);
	virtual void Execute() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<ACombatant> Source = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<ACombatant> Target = nullptr;

	int32 BaseAmount = 0;
};
